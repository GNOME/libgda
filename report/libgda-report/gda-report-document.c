/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Santi Camps <scamps@users.sourceforge.net>
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

#include <config.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-item-report.h>
#include <libgda-report/gda-report-document.h>


struct _GdaReportDocumentPrivate {
	xmlDocPtr       doc;
	GdaReportValid *valid;
};


static void gda_report_document_class_init (GdaReportDocumentClass *klass);
static void gda_report_document_init       (GdaReportDocument *document,
					    GdaReportDocumentClass *klass);
static void gda_report_document_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

GdaReportDocument *gda_report_document_construct (GdaReportValid *valid);


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
	g_return_if_fail (GDA_REPORT_IS_DOCUMENT (document));

	document->priv = g_new0 (GdaReportDocumentPrivate, 1);
	document->priv->doc 	= NULL;
	document->priv->valid 	= NULL;
}

static void
gda_report_document_finalize (GObject *object)
{
	GdaReportDocument *document = (GdaReportDocument *) object;

	g_return_if_fail (GDA_REPORT_IS_DOCUMENT (document));

	xmlFreeDoc (document->priv->doc);
	g_free (document->priv);
	document->priv = NULL;
	
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
		type = g_type_register_static (G_TYPE_OBJECT, "GdaReportDocument", &info, 0);
	}
	return type;
}


/*
 * gda_report_document_construct
 */
GdaReportDocument *
gda_report_document_construct (GdaReportValid *valid)
{
	GdaReportDocument *document;

	document = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);
	document->priv->valid = valid;
	
	return document;
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
gda_report_document_new (GdaReportValid *valid)
{
	GdaReportDocument *document;

	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);
	document = gda_report_document_construct(valid);
	document->priv->doc = xmlNewDoc ("1.0");
	document->priv->doc->intSubset = gda_report_valid_to_dom (valid);
	
	return document;
}


/**
 * gda_report_document_new_from_string
 */
GdaReportDocument *
gda_report_document_new_from_string (const gchar *xml, 
				     GdaReportValid *valid)
{
	GdaReportDocument *document;

	g_return_val_if_fail (xml != NULL, NULL);
	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);

	document = gda_report_document_construct (valid);
	document->priv->doc = xmlParseMemory (xml, strlen (xml));
	if (!document->priv->doc) {
		gda_log_error (_("Could not parse XML document"));
		g_object_unref (G_OBJECT (document));
		return NULL;
	}
	document->priv->doc->intSubset = gda_report_valid_to_dom (valid);
	
	if (!gda_report_valid_validate_document (
				document->priv->valid, 
				document->priv->doc))
	{
		gda_log_error (_("XML document is not valid"));
		g_object_unref (G_OBJECT (document));
		return NULL;
	}			
	
	return document;
}

/**
 * gda_report_document_new_from_uri
 */
GdaReportDocument *
gda_report_document_new_from_uri (const gchar *uri,
				  GdaReportValid *valid)
{
	gchar *body;
	GdaReportDocument *document;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);

	body = gda_file_load (uri);
	if (!body) 
	{
		gda_log_error (_("Could not get file from %s"), uri);
		return NULL;
	}

	document = gda_report_document_new_from_string (body, valid);
	g_free (body);

	return document;
}


/*
 * gda_report_document_to_dom
 */
xmlDocPtr 
gda_report_document_to_dom (GdaReportDocument *document) 
{
	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT (document), NULL);
	return document->priv->doc;
}


/*
 * gda_report_document_get_valid
 */
GdaReportValid*
gda_report_document_get_valid (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT (document), NULL);
	return document->priv->valid;
}


/*
 * gda_report_document_set_root_item
 */
gboolean
gda_report_document_set_root_item (GdaReportDocument *document,
				   GdaReportItem *item)
{
	xmlNodePtr node;
	
	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT (document), FALSE);
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);

	node = gda_report_item_to_dom(item);
	if (node != NULL)
	{
		xmlDocSetRootElement (document->priv->doc, node);
		return TRUE;
	}
	else
		return FALSE;
}


/*
 * gda_report_document_get_root_item
 */
GdaReportItem *
gda_report_document_get_root_item (GdaReportDocument *document)
{
	xmlNodePtr node;
	
	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT (document), NULL);
	node = xmlDocGetRootElement (document->priv->doc);

	return gda_report_item_report_new_from_dom (node);
}


/*
 * gda_report_document_save_file
 */
gboolean
gda_report_document_save_file (const char *filename,
			       GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT (document), FALSE);

	if (xmlSaveFile (filename, document->priv->doc))		
		return TRUE;
	else
		return FALSE;
}




