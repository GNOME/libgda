/* 
 * GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

struct _GdaReportDocbookDocumentPrivate {
	gchar *html_stylesheet;
	gchar *fo_stylesheet;
	gchar *java_home;
	gchar *fop_path;
};

/* properties */
enum
{
        PROP_0,
	PROP_HTML_STYLESHEET,

	PROP_FO_STYLESHEET,
	PROP_JAVA_HOME,
	PROP_FOP_PATH
};

static void gda_report_docbook_document_class_init (GdaReportDocbookDocumentClass *klass);
static void gda_report_docbook_document_init       (GdaReportDocbookDocument *doc, GdaReportDocbookDocumentClass *klass);
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

static GObjectClass *parent_class = NULL;

/*
 * GdaReportDocbookDocument class implementation
 */
static void
gda_report_docbook_document_class_init (GdaReportDocbookDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaReportDocumentClass *doc_class = GDA_REPORT_DOCUMENT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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
                                         g_param_spec_string ("fop_path", NULL, NULL, NULL, 
							      G_PARAM_WRITABLE | G_PARAM_READABLE));

	/* virtual methods */
	doc_class->run_as_html = gda_report_docbook_document_run_as_html;
	doc_class->run_as_pdf = gda_report_docbook_document_run_as_pdf;
}

static void
gda_report_docbook_document_init (GdaReportDocbookDocument *doc, GdaReportDocbookDocumentClass *klass)
{
	doc->priv = g_new0 (GdaReportDocbookDocumentPrivate, 1);
}

static void
gda_report_docbook_document_dispose (GObject *object)
{
	GdaReportDocbookDocument *doc = (GdaReportDocbookDocument *) object;

	g_return_if_fail (GDA_IS_REPORT_DOCBOOK_DOCUMENT (doc));

	/* free memory */
	if (doc->priv) {
		g_free (doc->priv->html_stylesheet);
		g_free (doc->priv->fo_stylesheet);
		g_free (doc->priv->java_home);
		g_free (doc->priv->fop_path);

		g_free (doc->priv);
		doc->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

GType
gda_report_docbook_document_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static GTypeInfo info = {
			sizeof (GdaReportDocbookDocumentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_docbook_document_class_init,
			NULL, NULL,
			sizeof (GdaReportDocbookDocument),
			0,
			(GInstanceInitFunc) gda_report_docbook_document_init
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_REPORT_DOCUMENT, "GdaReportDocbookDocument", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static void
gda_report_docbook_document_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
        GdaReportDocbookDocument *doc;

        doc = GDA_REPORT_DOCBOOK_DOCUMENT (object);
        if (doc->priv) {
                switch (param_id) {
		case PROP_HTML_STYLESHEET:
			if (doc->priv->html_stylesheet) {
				g_free (doc->priv->html_stylesheet);
				doc->priv->html_stylesheet = NULL;
			}
			if (g_value_get_string (value))
				doc->priv->html_stylesheet = g_strdup (g_value_get_string (value));
			break;
		case PROP_FO_STYLESHEET:
			if (doc->priv->fo_stylesheet) {
				g_free (doc->priv->fo_stylesheet);
				doc->priv->fo_stylesheet = NULL;
			}
			if (g_value_get_string (value))
				doc->priv->fo_stylesheet = g_strdup (g_value_get_string (value));
			break;
		case PROP_JAVA_HOME:
			if (doc->priv->java_home) {
				g_free (doc->priv->java_home);
				doc->priv->java_home = NULL;
			}
			if (g_value_get_string (value))
				doc->priv->java_home = g_strdup (g_value_get_string (value));
			break;
		case PROP_FOP_PATH:
			if (doc->priv->fop_path) {
				g_free (doc->priv->fop_path);
				doc->priv->fop_path = NULL;
			}
			if (g_value_get_string (value))
				doc->priv->fop_path = g_strdup (g_value_get_string (value));
			break;
		default:
			break;
                }
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
        if (doc->priv) {
		switch (param_id) {
		case PROP_HTML_STYLESHEET:
			g_value_set_string (value, doc->priv->html_stylesheet);
			break;
		case PROP_FO_STYLESHEET:
			g_value_set_string (value, doc->priv->fo_stylesheet);
			break;
		case PROP_JAVA_HOME:
			g_value_set_string (value, doc->priv->java_home);
			break;
		case PROP_FOP_PATH:
			g_value_set_string (value, doc->priv->fop_path);
			break;
		default:
			break;
		}
        }
}

/**
 * gda_report_docbook_document_new
 * @fo_file: a FO file name, or %NULL
 *
 * Creates a new #GdaReportDocbookDocument using @fo_file as a base
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
	static GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;
	static gchar *xsltproc = NULL;
	GdaReportDocbookDocument *fdoc;
	gchar **argv;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_REPORT_DOCBOOK_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);
	fdoc = GDA_REPORT_DOCBOOK_DOCUMENT (doc);
	g_return_val_if_fail (fdoc->priv, FALSE);

	g_static_mutex_lock (&init_mutex);
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
			g_set_error (error, 0, 0,
				     _("Could not find the '%s' program"), "xsltproc");
			g_static_mutex_unlock (&init_mutex);
			return FALSE;
		}
	}

	if (!fdoc->priv->html_stylesheet) {
		 fdoc->priv->html_stylesheet = gda_gbr_get_file_path (GDA_DATA_DIR, "xml", "docbook", 
								    "stylesheet", "html", "docbook.xsl", NULL);
		if (!g_file_test (fdoc->priv->html_stylesheet, G_FILE_TEST_EXISTS)) {
			g_free (fdoc->priv->html_stylesheet);
			fdoc->priv->html_stylesheet = NULL;
		}
		if (!fdoc->priv->html_stylesheet) {
			g_set_error (error, 0, 0,
				     _("Could not find the DocBook XSL stylesheet for HTML"));
			g_static_mutex_unlock (&init_mutex);
			return FALSE;
		}
	}
	g_static_mutex_unlock (&init_mutex);

	argv = g_new (gchar *, 9);
	argv[0] = g_strdup (xsltproc);
	argv[1] = g_strdup ("--output");
	argv[2] = g_strdup (filename);
	argv[3] = g_strdup ("--stringparam");
	argv[4] = g_strdup ("use.extensions");
	argv[5] = g_strdup ("0");
	argv[6] = g_strdup (fdoc->priv->html_stylesheet);
	argv[7] = NULL;
	argv[8] = NULL;

	retval = gda_report_document_run_converter_argv (doc, NULL, argv, 7, 
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
	g_return_val_if_fail (fdoc->priv, FALSE);

	if (!fdoc->priv->fop_path) {
		fdoc->priv->fop_path = g_find_program_in_path ("fop");
		if (!fdoc->priv->fop_path) {
			fdoc->priv->fop_path = gda_gbr_get_file_path (GDA_BIN_DIR, "fop", NULL);
			if (!g_file_test (fdoc->priv->fop_path, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (fdoc->priv->fop_path);
				fdoc->priv->fop_path = NULL;
			}
		}
		if (!fdoc->priv->fop_path && fdoc->priv->java_home) {
			fdoc->priv->fop_path = g_build_filename (fdoc->priv->java_home, "fop", NULL);
			if (!g_file_test (fdoc->priv->fop_path, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (fdoc->priv->fop_path);
				fdoc->priv->fop_path = NULL;
			}
		}
		if (!fdoc->priv->fop_path) {
			g_set_error (error, 0, 0,
				     _("Could not find the '%s' program"), "fop");
			return FALSE;
		}
	}

	if (!fdoc->priv->fo_stylesheet) {
		 fdoc->priv->fo_stylesheet = gda_gbr_get_file_path (GDA_DATA_DIR, "xml", "docbook", 
								    "stylesheet", "fo", "docbook.xsl", NULL);
		if (!g_file_test (fdoc->priv->fo_stylesheet, G_FILE_TEST_EXISTS)) {
			g_free (fdoc->priv->fo_stylesheet);
			fdoc->priv->fo_stylesheet = NULL;
		}
		if (!fdoc->priv->fo_stylesheet) {
			g_set_error (error, 0, 0,
				     _("Could not find the DocBook XSL stylesheet for Formatting Objects"));
			return FALSE;
		}
	}

	argv = g_new (gchar *, 8);
	argv[0] = g_strdup (fdoc->priv->fop_path);
	argv[1] = g_strdup ("-xml");
	argv[2] = NULL;
	argv[3] = g_strdup ("-xsl");
	argv[4] = g_strdup (fdoc->priv->fo_stylesheet);
	argv[5] = g_strdup ("-pdf");
	argv[6] = g_strdup (filename);
	argv[7] = NULL;

	gint i;
	for (i= 0; i< 7; i++)
		g_print ("==%d %s\n", i, argv[i]);

	retval = gda_report_document_run_converter_argv (doc, NULL, argv, 2, 
							 "fop", error);
	g_strfreev (argv);
	return retval;
}
