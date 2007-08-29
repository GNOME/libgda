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
#include "gda-report-rml-document.h"
#include <libgda/binreloc/gda-binreloc.h>

struct _GdaReportRmlDocumentPrivate {
};

/* properties */
enum
{
        PROP_0,
};

static void gda_report_rml_document_class_init (GdaReportRmlDocumentClass *klass);
static void gda_report_rml_document_init       (GdaReportRmlDocument *doc, GdaReportRmlDocumentClass *klass);
static void gda_report_rml_document_dispose   (GObject *object);
static void gda_report_rml_document_set_property (GObject *object,
						  guint param_id,
						  const GValue *value,
						  GParamSpec *pspec);
static void gda_report_rml_document_get_property (GObject *object,
						  guint param_id,
						  GValue *value,
						  GParamSpec *pspec);

static gboolean gda_report_rml_document_run_as_html (GdaReportDocument *doc, const gchar *filename, GError **error);
static gboolean gda_report_rml_document_run_as_pdf (GdaReportDocument *doc, const gchar *filename, GError **error);

static GObjectClass *parent_class = NULL;

/*
 * GdaReportRmlDocument class implementation
 */
static void
gda_report_rml_document_class_init (GdaReportRmlDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaReportDocumentClass *doc_class = GDA_REPORT_DOCUMENT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* report methods */
	object_class->dispose = gda_report_rml_document_dispose;

	/* Properties */
        object_class->set_property = gda_report_rml_document_set_property;
        object_class->get_property = gda_report_rml_document_get_property;

	/* virtual methods */
	doc_class->run_as_html = gda_report_rml_document_run_as_html;
	doc_class->run_as_pdf = gda_report_rml_document_run_as_pdf;
}

static void
gda_report_rml_document_init (GdaReportRmlDocument *doc, GdaReportRmlDocumentClass *klass)
{
	doc->priv = g_new0 (GdaReportRmlDocumentPrivate, 1);
}

static void
gda_report_rml_document_dispose (GObject *object)
{
	GdaReportRmlDocument *doc = (GdaReportRmlDocument *) object;

	g_return_if_fail (GDA_IS_REPORT_RML_DOCUMENT (doc));

	/* free memory */
	if (doc->priv) {
		g_free (doc->priv);
		doc->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

GType
gda_report_rml_document_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaReportRmlDocumentClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_report_rml_document_class_init,
				NULL, NULL,
				sizeof (GdaReportRmlDocument),
				0,
				(GInstanceInitFunc) gda_report_rml_document_init
			};
			
			type = g_type_register_static (GDA_TYPE_REPORT_DOCUMENT, "GdaReportRmlDocument", &info, 0);
		}
	}

	return type;
}

static void
gda_report_rml_document_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
        GdaReportRmlDocument *doc;

        doc = GDA_REPORT_RML_DOCUMENT (object);
        if (doc->priv) {
                switch (param_id) {
		default:
			break;
                }
        }
}

static void
gda_report_rml_document_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
        GdaReportRmlDocument *doc;

        doc = GDA_REPORT_RML_DOCUMENT (object);
        if (doc->priv) {
		switch (param_id) {
		default:
			break;
		}
        }
}

/**
 * gda_report_rml_document_new
 * @rml_file: a RML file name, or %NULL
 *
 * Creates a new #GdaReportRmlDocument using @rml_file as a base
 *
 * Returns: a new #GdaReportRmlDocument object
 */
GdaReportDocument *
gda_report_rml_document_new (GdaReportEngine *engine)
{
	if (engine)
		return (GdaReportDocument *) g_object_new (GDA_TYPE_REPORT_RML_DOCUMENT, "engine", engine, NULL);
	else
		return (GdaReportDocument *) g_object_new (GDA_TYPE_REPORT_RML_DOCUMENT, NULL);
}


/* virtual methods */
static gboolean
gda_report_rml_document_run_as_html (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	static gchar *converter = NULL;

	g_return_val_if_fail (GDA_IS_REPORT_RML_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);

	if (!converter) {
		converter = g_find_program_in_path ("trml2html.py");
		if (!converter) {
			converter = gda_gbr_get_file_path (GDA_BIN_DIR, "gda_trml2html", "trml2html.py", NULL);
			if (!g_file_test (converter, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (converter);
				converter = NULL;
			}
		}
		if (!converter) {
			g_set_error (error, 0, 0,
				     _("Could not find the 'trml2html.py' program"));
			return FALSE;
		}
	}

	return gda_report_document_run_converter_path (doc, filename, converter, "trml2html", error);
}

static gboolean
gda_report_rml_document_run_as_pdf (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	static gchar *converter = NULL;

	g_return_val_if_fail (GDA_IS_REPORT_RML_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);

	if (!converter) {
		converter = g_find_program_in_path ("trml2pdf.py");
		if (!converter) {
			converter = gda_gbr_get_file_path (GDA_BIN_DIR, "gda_trml2pdf", "trml2pdf.py", NULL);
			if (!g_file_test (converter, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (converter);
				converter = NULL;
			}
		}
		if (!converter) {
			g_set_error (error, 0, 0,
				     _("Could not find the 'trml2pdf.py' program"));
			return FALSE;
		}
	}

	return gda_report_document_run_converter_path (doc, filename, converter, "trml2pdf", error);
}
