/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <engine/gda-report-engine.h>
#include <gda-report-document.h>
#include <gda-report-document-private.h>
#include "gda-report-docbook-document.h"
#include <libgda/binreloc/gda-binreloc.h>


/* module error */
GQuark gda_report_docbook_document_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_report_docbook_document_error");
        return quark;
}

typedef struct {
	gchar *html_stylesheet;
	gchar *fo_stylesheet;
	gchar *java_home;
	gchar *fop_path;
} GdaReportDocbookDocumentPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaReportDocbookDocument, gda_report_docbook_document, GDA_TYPE_REPORT_DOCUMENT)

/* properties */
enum
{
	PROP_0,
	PROP_HTML_STYLESHEET,

	PROP_FO_STYLESHEET,
	PROP_JAVA_HOME,
	PROP_FOP_PATH
};

static void gda_report_docbook_document_dispose   (GObject *object);
static void gda_report_docbook_document_set_property (GObject *object,
						      guint param_id,
						      const GValue *value,
						      GParamSpec *pspec);
static void gda_report_docbook_document_get_property (GObject *object,
						      guint param_id,
						      GValue *value,
						      GParamSpec *pspec);

static gboolean gda_report_docbook_document_run_as_html (GdaReportDocument *doc, const gchar *filename, GError **error);
static gboolean gda_report_docbook_document_run_as_pdf (GdaReportDocument *doc, const gchar *filename, GError **error);

/*
 * GdaReportDocbookDocument class implementation
 */
static void
gda_report_docbook_document_class_init (GdaReportDocbookDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaReportDocumentClass *doc_class = GDA_REPORT_DOCUMENT_CLASS (klass);

	/* report methods */
	object_class->dispose = gda_report_docbook_document_dispose;

	/* Properties */
        object_class->set_property = gda_report_docbook_document_set_property;
        object_class->get_property = gda_report_docbook_document_get_property;

	g_object_class_install_property (object_class, PROP_HTML_STYLESHEET,
                                         g_param_spec_string ("html-stylesheet", NULL, NULL, NULL, 
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_FO_STYLESHEET,
                                         g_param_spec_string ("fo-stylesheet", NULL, NULL, NULL, 
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_JAVA_HOME,
                                         g_param_spec_string ("java-home", NULL, NULL, NULL, 
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_FOP_PATH,
                                         g_param_spec_string ("fop-path", NULL, NULL, NULL, 
							      G_PARAM_WRITABLE | G_PARAM_READABLE));

	/* virtual methods */
	doc_class->run_as_html = gda_report_docbook_document_run_as_html;
	doc_class->run_as_pdf = gda_report_docbook_document_run_as_pdf;
}

static void
gda_report_docbook_document_init (GdaReportDocbookDocument *doc) {}

static void
gda_report_docbook_document_dispose (GObject *object)
{
	GdaReportDocbookDocument *doc = (GdaReportDocbookDocument *) object;

	g_return_if_fail (GDA_IS_REPORT_DOCBOOK_DOCUMENT (doc));
	GdaReportDocbookDocumentPrivate *priv = gda_report_docbook_document_get_instance_private (doc);

	/* free memory */
	if (priv->html_stylesheet) {
		g_free (priv->html_stylesheet);
		priv->html_stylesheet = NULL;
	}
	if (priv->fo_stylesheet) {
		g_free (priv->fo_stylesheet);
		priv->fo_stylesheet = NULL;
	}
	if (priv->java_home) {
		g_free (priv->java_home);
		priv->java_home = NULL;
	}
	if (priv->fop_path) {
		g_free (priv->fop_path);
		priv->fop_path = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_report_docbook_document_parent_class)->dispose (object);
}

static void
gda_report_docbook_document_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdaReportDocbookDocument *doc;

	doc = GDA_REPORT_DOCBOOK_DOCUMENT (object);
	GdaReportDocbookDocumentPrivate *priv = gda_report_docbook_document_get_instance_private (doc);
	switch (param_id) {
		case PROP_HTML_STYLESHEET:
			if (priv->html_stylesheet) {
				g_free (priv->html_stylesheet);
				priv->html_stylesheet = NULL;
			}
			if (g_value_get_string (value))
				priv->html_stylesheet = g_strdup (g_value_get_string (value));
			break;
		case PROP_FO_STYLESHEET:
			if (priv->fo_stylesheet) {
				g_free (priv->fo_stylesheet);
				priv->fo_stylesheet = NULL;
			}
			if (g_value_get_string (value))
				priv->fo_stylesheet = g_strdup (g_value_get_string (value));
			break;
		case PROP_JAVA_HOME:
			if (priv->java_home) {
				g_free (priv->java_home);
				priv->java_home = NULL;
			}
			if (g_value_get_string (value))
				priv->java_home = g_strdup (g_value_get_string (value));
			break;
		case PROP_FOP_PATH:
			if (priv->fop_path) {
				g_free (priv->fop_path);
				priv->fop_path = NULL;
			}
			if (g_value_get_string (value))
				priv->fop_path = g_strdup (g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_report_docbook_document_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	GdaReportDocbookDocument *doc;

	doc = GDA_REPORT_DOCBOOK_DOCUMENT (object);
	GdaReportDocbookDocumentPrivate *priv = gda_report_docbook_document_get_instance_private (doc);
	switch (param_id) {
		case PROP_HTML_STYLESHEET:
			g_value_set_string (value, priv->html_stylesheet);
			break;
		case PROP_FO_STYLESHEET:
			g_value_set_string (value, priv->fo_stylesheet);
			break;
		case PROP_JAVA_HOME:
			g_value_set_string (value, priv->java_home);
			break;
		case PROP_FOP_PATH:
			g_value_set_string (value, priv->fop_path);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_report_docbook_document_new
 * @engine: a #GdaReportEngine, or %NULL
 *
 * Creates a new #GdaReportDocbookDocument using @engine if specified
 *
 * Returns: a new #GdaReportDocbookDocument object
 */
GdaReportDocument *
gda_report_docbook_document_new (GdaReportEngine *engine)
{
	if (engine)
		return (GdaReportDocument *) g_object_new (GDA_TYPE_REPORT_DOCBOOK_DOCUMENT, "engine", engine, NULL);
	else
		return (GdaReportDocument *) g_object_new (GDA_TYPE_REPORT_DOCBOOK_DOCUMENT, NULL);
}


/* virtual methods */
static gboolean
gda_report_docbook_document_run_as_html (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	static GMutex init_mutex;
	static gchar *xsltproc = NULL;
	GdaReportDocbookDocument *fdoc;
	gchar **argv;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_REPORT_DOCBOOK_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);
	fdoc = GDA_REPORT_DOCBOOK_DOCUMENT (doc);
	GdaReportDocbookDocumentPrivate *priv = gda_report_docbook_document_get_instance_private (fdoc);

	g_mutex_lock (&init_mutex);
	if (!xsltproc) {
		xsltproc = g_find_program_in_path ("xsltproc");
		if (!xsltproc) {
			xsltproc = gda_gbr_get_file_path (GDA_BIN_DIR, "xsltproc", NULL);
			if (!g_file_test (xsltproc, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (xsltproc);
				xsltproc = NULL;
			}
		}
		if (!xsltproc) {
			g_set_error (error, GDA_REPORT_DOCBOOK_DOCUMENT_ERROR, GDA_REPORT_DOCBOOK_DOCUMENT_GENERAL_ERROR,
				     _("Could not find the '%s' program"), "xsltproc");
			g_mutex_unlock (&init_mutex);
			return FALSE;
		}
	}

	if (!priv->html_stylesheet) {
		 priv->html_stylesheet = gda_gbr_get_file_path (GDA_DATA_DIR, "xml", "docbook",
								    "stylesheet", "html", "docbook.xsl", NULL);
		if (!g_file_test (priv->html_stylesheet, G_FILE_TEST_EXISTS)) {
			g_free (priv->html_stylesheet);
			priv->html_stylesheet = NULL;
		}
		if (!priv->html_stylesheet) {
			g_set_error (error, GDA_REPORT_DOCBOOK_DOCUMENT_ERROR, GDA_REPORT_DOCBOOK_DOCUMENT_GENERAL_ERROR, "%s",
				     _("Could not find the DocBook XSL stylesheet for HTML"));
			g_mutex_unlock (&init_mutex);
			return FALSE;
		}
	}
	g_mutex_unlock (&init_mutex);

	argv = g_new (gchar *, 9);
	argv[0] = g_strdup (xsltproc);
	argv[1] = g_strdup ("--output");
	argv[2] = g_strdup (filename);
	argv[3] = g_strdup ("--stringparam");
	argv[4] = g_strdup ("use.extensions");
	argv[5] = g_strdup ("0");
	argv[6] = g_strdup (priv->html_stylesheet);
	argv[7] = NULL;
	argv[8] = NULL;

	retval = _gda_report_document_run_converter_argv (doc, NULL, argv, 7, 
							 "xsltproc", error);
	g_strfreev (argv);
	return retval;
}

static gboolean
gda_report_docbook_document_run_as_pdf (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	GdaReportDocbookDocument *fdoc;
	gchar **argv;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_REPORT_DOCBOOK_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);
	fdoc = GDA_REPORT_DOCBOOK_DOCUMENT (doc);
	GdaReportDocbookDocumentPrivate *priv = gda_report_docbook_document_get_instance_private (fdoc);

	if (!priv->fop_path) {
		priv->fop_path = g_find_program_in_path ("fop");
		if (!priv->fop_path) {
			priv->fop_path = gda_gbr_get_file_path (GDA_BIN_DIR, "fop", NULL);
			if (!g_file_test (priv->fop_path, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (priv->fop_path);
				priv->fop_path = NULL;
			}
		}
		if (!priv->fop_path && priv->java_home) {
			priv->fop_path = g_build_filename (priv->java_home, "fop", NULL);
			if (!g_file_test (priv->fop_path, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (priv->fop_path);
				priv->fop_path = NULL;
			}
		}
		if (!priv->fop_path) {
			g_set_error (error, GDA_REPORT_DOCBOOK_DOCUMENT_ERROR, GDA_REPORT_DOCBOOK_DOCUMENT_GENERAL_ERROR,
				     _("Could not find the '%s' program"), "fop");
			return FALSE;
		}
	}

	if (!priv->fo_stylesheet) {
		 priv->fo_stylesheet = gda_gbr_get_file_path (GDA_DATA_DIR, "xml", "docbook",
								    "stylesheet", "fo", "docbook.xsl", NULL);
		if (!g_file_test (priv->fo_stylesheet, G_FILE_TEST_EXISTS)) {
			g_free (priv->fo_stylesheet);
			priv->fo_stylesheet = NULL;
		}
		if (!priv->fo_stylesheet) {
			g_set_error (error, GDA_REPORT_DOCBOOK_DOCUMENT_ERROR, GDA_REPORT_DOCBOOK_DOCUMENT_GENERAL_ERROR, "%s",
				     _("Could not find the DocBook XSL stylesheet for Formatting Objects"));
			return FALSE;
		}
	}

	argv = g_new (gchar *, 8);
	argv[0] = g_strdup (priv->fop_path);
	argv[1] = g_strdup ("-xml");
	argv[2] = NULL;
	argv[3] = g_strdup ("-xsl");
	argv[4] = g_strdup (priv->fo_stylesheet);
	argv[5] = g_strdup ("-pdf");
	argv[6] = g_strdup (filename);
	argv[7] = NULL;

	gint i;
	for (i= 0; i< 7; i++)
		g_print ("==%d %s\n", i, argv[i]);

	retval = _gda_report_document_run_converter_argv (doc, NULL, argv, 2, 
							 "fop", error);
	g_strfreev (argv);
	return retval;
}
