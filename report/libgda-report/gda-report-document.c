/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-document.h>
#include <bonobo/bonobo-i18n.h>

#define PARENT_TYPE GDA_TYPE_XML_DOCUMENT

#define OBJECT_REPORT         "report"
#define OBJECT_REPORT_ELEMENT "reportelement"

struct _GdaReportDocumentPrivate {
};

static void gda_report_document_class_init (GdaReportDocumentClass *klass);
static void gda_report_document_init       (GdaReportDocument *document,
					    GdaReportDocumentClass *klass);
static void gda_report_document_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaReportDocument class implementation
 */

static void
gda_report_document_class_init (GdaReportDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_report_document_finalize;
}

static void
gda_report_document_init (GdaReportDocument *document, GdaReportDocumentClass *klass)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	/* allocate private structure */
	document->priv = g_new0 (GdaReportDocumentPrivate, 1);
}

static void
gda_report_document_finalize (GObject *object)
{
	GdaReportDocument *document = (GdaReportDocument *) object;

	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	/* free memory */
	g_free (document->priv);
	document->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_report_document_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportDocumentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_document_class_init,
			NULL,
			NULL,
			sizeof (GdaReportDocument),
			0,
			(GInstanceInitFunc) gda_report_document_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaReportDocument", &info, 0);
	}
	return type;
}

/**
 * gda_report_document_new
 *
 * Create a new #GdaReportDocument object, which is a wrapper that lets
 * you easily manage the XML format used in the GDA report engine.
 *
 * Returns: the newly created object.
 */
GdaReportDocument *
gda_report_document_new (void)
{
	GdaReportDocument *document;

	document = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);
	gda_xml_document_construct (GDA_XML_DOCUMENT (document), OBJECT_REPORT);

	return document;
}

/**
 * gda_report_document_new_from_string
 */
GdaReportDocument *
gda_report_document_new_from_string (const gchar *xml)
{
	GdaReportDocument *document;

	g_return_val_if_fail (xml != NULL, NULL);

	/* parse the XML string */
	document = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);

	GDA_XML_DOCUMENT (document)->doc = xmlParseMemory (xml, strlen (xml));
	if (!GDA_XML_DOCUMENT (document)->doc) {
		gda_log_error (_("Could not parse XML document"));
		g_object_unref (G_OBJECT (document));
		return NULL;
	}

	GDA_XML_DOCUMENT (document)->root =
		xmlDocGetRootElement (GDA_XML_DOCUMENT (document)->doc);

	return document;
}

/**
 * gda_report_document_new_from_uri
 */
GdaReportDocument *
gda_report_document_new_from_uri (const gchar *uri)
{
	gchar *body;
	GdaReportDocument *document;

	g_return_val_if_fail (uri != NULL, NULL);

	/* get the file contents from the given URI */
	body = gda_file_load (uri);
	if (!body) {
		gda_log_error (_("Could not get file from %s"), uri);
		return NULL;
	}

	/* create the GdaReportDocument object */
	document = gda_report_document_new_from_string (body);
	g_free (body);

	return document;
}
