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
#include "gda-report-document.h"
#include "gda-report-document-private.h"
#include <libgda/binreloc/gda-binreloc.h>


/* module error */
GQuark gda_report_document_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_report_document_error");
        return quark;
}

typedef struct {
	GdaReportEngine *engine;
	xmlDocPtr        doc;
} GdaReportDocumentPrivate;

/* properties */
enum
{
        PROP_0,
	PROP_ENGINE,
	PROP_TEMPLATE,
};

static void gda_report_document_dispose   (GObject *object);
static void gda_report_document_set_property (GObject *object,
						  guint param_id,
						  const GValue *value,
						  GParamSpec *pspec);
static void gda_report_document_get_property (GObject *object,
						  guint param_id,
						  GValue *value,
						  GParamSpec *pspec);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdaReportDocument, gda_report_document, G_TYPE_OBJECT)

/*
 * GdaReportDocument class implementation
 */
static void
gda_report_document_class_init (GdaReportDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* report methods */
	object_class->dispose = gda_report_document_dispose;

	/* Properties */
        object_class->set_property = gda_report_document_set_property;
        object_class->get_property = gda_report_document_get_property;

	g_object_class_install_property (object_class, PROP_TEMPLATE,
                                         g_param_spec_string ("template", NULL, NULL, NULL, G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_ENGINE,
                                         g_param_spec_object ("engine", NULL, NULL, GDA_TYPE_REPORT_ENGINE, 
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
}

static void
gda_report_document_init (GdaReportDocument *doc) {}

static void
gda_report_document_dispose (GObject *object)
{
	GdaReportDocument *doc = (GdaReportDocument *) object;

	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (doc));
	GdaReportDocumentPrivate *priv = gda_report_document_get_instance_private (doc);

	/* free memory */
	if (priv->doc) {
		xmlFreeDoc (priv->doc);
		priv->doc = NULL;
	}

	if (priv->engine) {
		g_object_unref (priv->engine);
		priv->engine = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_report_document_parent_class)->dispose (object);
}

static void
gda_report_document_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdaReportDocument *doc;

	doc = GDA_REPORT_DOCUMENT (object);
	GdaReportDocumentPrivate *priv = gda_report_document_get_instance_private (doc);

	switch (param_id) {
		case PROP_ENGINE:
			if (priv->engine)
				g_object_unref (priv->engine);
			priv->engine = g_value_get_object (value);
			if (priv->engine)
				g_object_ref (priv->engine);
			break;
		case PROP_TEMPLATE: 
			if (priv->doc)
				xmlFreeDoc (priv->doc);
			priv->doc = xmlParseFile (g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_report_document_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	GdaReportDocument *doc;

	doc = GDA_REPORT_DOCUMENT (object);
	GdaReportDocumentPrivate *priv = gda_report_document_get_instance_private (doc);
	switch (param_id) {
		case PROP_ENGINE:
			if (!priv->engine)
				priv->engine = gda_report_engine_new (NULL);
			g_value_set_object (value, priv->engine);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_report_document_set_template
 * @doc: a #GdaReportDocument object
 * @file: a RML file name, or %NULL
 *
 * Instructs @doc to use the @file RML file as a template
 */
void
gda_report_document_set_template (GdaReportDocument *doc, const gchar *file)
{
	g_object_set (G_OBJECT (doc), "template", file, NULL);
}


typedef struct _SpawnedData {
	gint     child_pid;
	gint     standard_out;
	gint     standard_error;
	GString *stdout_buf;
	GString *stderr_buf;
} SpawnedData;

gboolean 
_gda_report_document_run_converter_path (GdaReportDocument *doc, const gchar *filename, 
					const gchar *full_converter_path, const gchar *converter_name, 
					GError **error)
{
	gchar **argv;
	gboolean retval;

	argv = g_new (gchar *, 3);
	argv[0] = g_strdup (full_converter_path);
	argv[1] = NULL;
	argv[2] = NULL;

	retval = _gda_report_document_run_converter_argv (doc, filename, argv, 1, 
							 converter_name, error);
	g_strfreev (argv);
	return retval;
}


gboolean
_gda_report_document_run_converter_argv (GdaReportDocument *doc, const gchar *filename, 
					gchar **argv, gint argv_index_fname, 
					const gchar *converter_name, GError **error)
{
#define DO_UNLINK(x) g_unlink(x)
	/*
#undef DO_UNLINK
#define DO_UNLINK(x)
	*/

	/* execute engine */
	xmlNodePtr node, res;
	xmlDocPtr res_doc;
	GdaReportDocumentPrivate *priv = gda_report_document_get_instance_private (doc);

	if (!priv->doc || !(node = xmlDocGetRootElement (priv->doc))) {
		g_set_error (error, GDA_REPORT_DOCUMENT_ERROR, GDA_REPORT_DOCUMENT_GENERAL_ERROR, "%s",
			     _("Document not specified"));
		return FALSE;
	}
	g_object_set (G_OBJECT (priv->engine), "spec", xmlCopyNode (node, 1), NULL);
	res = gda_report_engine_run_as_node (priv->engine, error);
	if (!res) 
		return FALSE;
	node = xmlDocGetRootElement (priv->doc);
	xmlUnlinkNode (node);
	res_doc = xmlCopyDoc (priv->doc, 1);
	xmlDocSetRootElement (priv->doc, node);
	xmlDocSetRootElement (res_doc, res);

	/* save document to a temp. XML file */
	gint fd;
	gchar *tmp_filename;
	fd  = g_file_open_tmp (NULL, &tmp_filename, error);
	if (fd < 0) {
		xmlFreeDoc (res_doc);
		return FALSE;
	}
	/*g_print ("TMP filename: %s\n", tmp_filename);*/
	
	FILE *file;
	file = fdopen (fd, "w");
	if (xmlDocDump (file, res_doc) < 0) {
		g_set_error (error, GDA_REPORT_DOCUMENT_ERROR, GDA_REPORT_DOCUMENT_GENERAL_ERROR, "%s",
			     _("Cannot create temporary file"));
		DO_UNLINK (tmp_filename);
		g_free (tmp_filename);
		xmlFreeDoc (res_doc);
		fclose (file);
		return FALSE;
	}
	fclose (file);
	xmlFreeDoc (res_doc);
	
	/* launch converter */
	gchar *out, *err;
	gint exit_status;

	argv [argv_index_fname] = tmp_filename;
	if (! g_spawn_sync (NULL, argv, NULL, 
			    0, NULL, NULL,
			    &out, &err, &exit_status, error)) {
		argv [argv_index_fname] = NULL;
		DO_UNLINK (tmp_filename);
		return FALSE;
	}
	argv [argv_index_fname] = NULL;
	DO_UNLINK (tmp_filename);

	if (exit_status != 0) {
		g_set_error (error, GDA_REPORT_DOCUMENT_ERROR, GDA_REPORT_DOCUMENT_GENERAL_ERROR,
			     _("Execution of the %s program failed: %s"), converter_name, err);
		g_free (out);
		g_free (err);
		return FALSE;
	}
	g_free (err);

	/* write out to file */
	if (filename) {
		if (!g_file_set_contents (filename, out, -1, error)) {
			g_free (out);
			return FALSE;
		}
	}
	g_free (out);
	return TRUE;	
}

/**
 * gda_report_document_run_as_html
 * @doc: a #GdaReportDocument object
 * @filename: the name of a filename to save to
 * @error: a place to store errors, or %NULL
 *
 * Creates the report document and saves it as an HTML file into @filename
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_report_document_run_as_html (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);

	if (GDA_REPORT_DOCUMENT_CLASS (doc)->run_as_html)
		return GDA_REPORT_DOCUMENT_CLASS (doc)->run_as_html (doc, filename, error);

	g_set_error (error, GDA_REPORT_DOCUMENT_ERROR, GDA_REPORT_DOCUMENT_GENERAL_ERROR,
		     _("This report document does not handle %s output"), "HTML");
	return FALSE;
}

/**
 * gda_report_document_run_as_pdf
 * @doc: a #GdaReportDocument object
 * @filename: the name of a filename to save to
 * @error: a place to store errors, or %NULL
 *
 * Creates the report document and saves it as a PDF file into @filename
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_report_document_run_as_pdf (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);

	if (GDA_REPORT_DOCUMENT_CLASS (doc)->run_as_pdf)
		return GDA_REPORT_DOCUMENT_CLASS (doc)->run_as_pdf (doc, filename, error);
	else {
		g_set_error (error, GDA_REPORT_DOCUMENT_ERROR, GDA_REPORT_DOCUMENT_GENERAL_ERROR,
			     _("This report document does not handle %s output"), "PDF");
		return FALSE;
	}
}
