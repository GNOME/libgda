/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Vincent Untz <vuntz@gnome.org>
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
#include "gda-report-rml-document.h"
#include <libgda/binreloc/gda-binreloc.h>

/* module error */
GQuark gda_report_rml_document_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_report_rml_document_error");
        return quark;
}


typedef struct {
	int foo;
} GdaReportRmlDocumentPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaReportRmlDocument, gda_report_rml_document, GDA_TYPE_REPORT_DOCUMENT)

/* properties */
enum
{
        PROP_0,
};

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

/*
 * GdaReportRmlDocument class implementation
 */
static void
gda_report_rml_document_class_init (GdaReportRmlDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaReportDocumentClass *doc_class = GDA_REPORT_DOCUMENT_CLASS (klass);

	/* Properties */
        object_class->set_property = gda_report_rml_document_set_property;
        object_class->get_property = gda_report_rml_document_get_property;

	/* virtual methods */
	doc_class->run_as_html = gda_report_rml_document_run_as_html;
	doc_class->run_as_pdf = gda_report_rml_document_run_as_pdf;
}

static void
gda_report_rml_document_init (GdaReportRmlDocument *doc) {}

static void
gda_report_rml_document_set_property (GObject *object,
				guint param_id,
				G_GNUC_UNUSED const GValue *value,
				GParamSpec *pspec)
{
	switch (param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_report_rml_document_get_property (GObject *object,
				guint param_id,
				G_GNUC_UNUSED GValue *value,
				GParamSpec *pspec)
{
	switch (param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
}

/**
 * gda_report_rml_document_new
 * @engine: (allow-none): a #GdaReportEngine, or %NULL
 *
 * Creates a new #GdaReportRmlDocument using @engine if specified
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
	static GMutex init_mutex;
	static gchar *converter = NULL;

	g_return_val_if_fail (GDA_IS_REPORT_RML_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);

	g_mutex_lock (&init_mutex);
	if (!converter) {
		converter = g_find_program_in_path ("trml2html.py");
		if (!converter) {
			converter = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "gda_trml2html", "trml2html.py", NULL);
			if (!g_file_test (converter, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (converter);
				converter = NULL;
			}
		}
		if (!converter) {
			g_set_error (error, GDA_REPORT_RML_DOCUMENT_ERROR, GDA_REPORT_RML_DOCUMENT_GENERAL_ERROR,
				     _("Could not find the '%s' program"), "trml2html.py");
			g_mutex_unlock (&init_mutex);
			return FALSE;
		}
	}
	g_mutex_unlock (&init_mutex);

	return _gda_report_document_run_converter_path (doc, filename, converter, "trml2html", error);
}

static gboolean
gda_report_rml_document_run_as_pdf (GdaReportDocument *doc, const gchar *filename, GError **error)
{
	static GMutex init_mutex;
	static gchar *converter = NULL;

	g_return_val_if_fail (GDA_IS_REPORT_RML_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (filename && *filename, FALSE);

	g_mutex_lock (&init_mutex);
	if (!converter) {
		converter = g_find_program_in_path ("trml2pdf.py");
		if (!converter) {
			converter = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "gda_trml2pdf", "trml2pdf.py", NULL);
			if (!g_file_test (converter, G_FILE_TEST_IS_EXECUTABLE)) {
				g_free (converter);
				converter = NULL;
			}
		}
		if (!converter) {
			g_set_error (error, GDA_REPORT_RML_DOCUMENT_ERROR, GDA_REPORT_RML_DOCUMENT_GENERAL_ERROR,
				     _("Could not find the '%s' program"), "trml2pdf.py");
			g_mutex_unlock (&init_mutex);
			return FALSE;
		}
	}
	g_mutex_unlock (&init_mutex);

	return _gda_report_document_run_converter_path (doc, filename, converter, "trml2pdf", error);
}
