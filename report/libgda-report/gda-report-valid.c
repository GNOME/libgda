/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
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
#include <libxml/valid.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-valid.h>

#define GDA_REPORT_DTD_URL		LIBGDA_REPORT_DTDDIR"/gda-report.dtd"
#define GDA_REPORT_EXTERNAL_ID		"report"


struct _GdaReportValidPrivate {
	xmlDtdPtr       dtd;
	xmlValidCtxtPtr context;
};


static void gda_report_valid_class_init (GdaReportValidClass *klass);
static void gda_report_valid_init       (GdaReportValid *valid,
					 GdaReportValidClass *klass);
static void gda_report_valid_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;


/*
 * GdaReportValid class implementation
 */
static void
gda_report_valid_class_init (GdaReportValidClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_valid_finalize;
}


static void
gda_report_valid_init (GdaReportValid *valid, 
		       GdaReportValidClass *klass)
{
	g_return_if_fail (GDA_IS_REPORT_VALID (valid));

	/* allocate private structure */
	valid->priv = g_new0 (GdaReportValidPrivate, 1);
	valid->priv->dtd = NULL;
	valid->priv->context = NULL;
}


static void
gda_report_valid_finalize (GObject *object)
{
	GdaReportValid *valid = (GdaReportValid *) object;

	g_return_if_fail (GDA_IS_REPORT_VALID (object));

	/* free memory */
	xmlFreeDtd (valid->priv->dtd);
	g_free (valid->priv->context);
	g_free (valid->priv);
	
	parent_class->finalize (object);
}


GType
gda_report_valid_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportValidClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_valid_class_init,
			NULL,
			NULL,
			sizeof (GdaReportValid),
			0,
			(GInstanceInitFunc) gda_report_valid_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaReportValid", &info, 0);
	}
	return type;
}

/**
 * gda_report_valid_load 
 * 
 * Loads the gda-report DTD into a #GdaReportValid structure, needed 
 * to create new report items or documents.
 *
 * Returns: The new #GdaReportValid object
 */
GdaReportValid *
gda_report_valid_load (void)
{
	GdaReportValid *valid;

	valid = g_object_new (GDA_TYPE_REPORT_VALID, NULL);
	valid->priv->dtd = xmlParseDTD (GDA_REPORT_EXTERNAL_ID, GDA_REPORT_DTD_URL);
	if (valid->priv->dtd == NULL) 
	{
		gda_log_error (_("could not get DTD from %s"), GDA_REPORT_DTD_URL);
		return NULL;
	}
	valid->priv->context = g_new0 (xmlValidCtxt, 1);
	valid->priv->context->userData = (void *) stderr;
	valid->priv->context->error = (xmlValidityErrorFunc) fprintf;
	valid->priv->context->warning = (xmlValidityWarningFunc) fprintf;

	return valid;
}


/*
 * gda_report_valid_new_from_dom 
 *
 * @dtd: a xmlDtdPtr object that should be the gda-report DTD
 *
 * Returns: The #GdaReportValid object created from the DTD
 */
GdaReportValid *
gda_report_valid_new_from_dom (xmlDtdPtr dtd)
{
	GdaReportValid *valid;
	
	g_return_val_if_fail (dtd != NULL, NULL);
	
	valid = g_object_new (GDA_TYPE_REPORT_VALID, NULL);
	valid->priv->dtd = dtd;
	valid->priv->context = g_new0 (xmlValidCtxt, 1);
	valid->priv->context->userData = (void *) stderr;
	valid->priv->context->error = (xmlValidityErrorFunc) fprintf;
	valid->priv->context->warning = (xmlValidityWarningFunc) fprintf;

	return valid;
}


/*
 * gda_report_valid_to_dom
 * 
 * @valid: a #GdaReportValid object
 *
 * Returns: The gda-report DTD in a xmlDtdPtr object
 */
xmlDtdPtr 
gda_report_valid_to_dom (GdaReportValid *valid)
{
	g_return_val_if_fail (GDA_IS_REPORT_VALID (valid), NULL);
	return valid->priv->dtd;	
}


/*
 * gda_report_valid_validate_document
 *
 * @valid: a #GdaReportValid object
 * @document: a xmlDocPtr object 
 *
 * Test if the document is a valid gda-report xml document
 *
 * Returns: TRUE if the document is valid, FALSE otherwise
 */
gboolean 
gda_report_valid_validate_document (GdaReportValid *valid, 
				    xmlDocPtr document)
{
	g_return_val_if_fail (GDA_IS_REPORT_VALID (valid), FALSE);
	g_return_val_if_fail (document != NULL, FALSE);
	
	document->intSubset = valid->priv->dtd;
	if (xmlValidateOneElement (valid->priv->context, document, xmlDocGetRootElement(document)))
		return TRUE;
	else
		return FALSE;
}


/*
 * gda_report_valid_validate_element
 *
 * @valid: a #GdaReportValid object
 * @element: a xmlNodePtr element
 *
 * Test if the element is a valid element of a gda-report xml document
 *
 * Returns: TRUE if the element is valid, FALSE otherwise
 */
gboolean 
gda_report_valid_validate_element (GdaReportValid *valid, 
				   xmlNodePtr element)
{
	xmlDocPtr  doc;	
	
	g_return_val_if_fail (GDA_IS_REPORT_VALID (valid), FALSE);
	g_return_val_if_fail (element != NULL, FALSE);
	
	doc = xmlNewDoc ("1.0");
	doc->intSubset = valid->priv->dtd;
	
	if (!xmlValidateOneElement(valid->priv->context, doc, element))
	{
		gda_log_error (_("Error validating element %s"), element->name);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	xmlFreeDoc (doc);
	return TRUE;
}


/*
 * gda_report_valid_validate_attribute
 *
 * @valid: a #GdaReportValid object
 * @element_name: name of the element witch attribute is to be set
 * @attribute_name: name of the attribute to be set
 * @value: value to be set to the attribute
 *
 * Test if the given value is a valid value for the given attribute, and also
 * if the given attribute is a valid attribute for the given element.  All this
 * using the gda-report DTD definition
 *
 * Returns: TRUE if valid, FALSE otherwise
 */
gboolean 
gda_report_valid_validate_attribute (GdaReportValid *valid, 
				     const gchar *element_name, 
				     const gchar *attribute_name,
				     const gchar *value)
{
	xmlAttributePtr   attr_decl  = NULL;
	xmlEnumerationPtr value_list = NULL;
	
	g_return_val_if_fail (GDA_IS_REPORT_VALID (valid), FALSE);
	
	attr_decl = xmlGetDtdAttrDesc(valid->priv->dtd, element_name, attribute_name);
	if (attr_decl == NULL) 
	{
		gda_log_error (_("No declaration for attribute %s of element %s\n"), attribute_name, element_name);
		return FALSE;
	}
	
	/* There is a list of valid values */ 
	if (attr_decl->tree != NULL)
	{
		value_list = attr_decl->tree;
		while (value_list != NULL) 
		{
		    if (g_ascii_strcasecmp(value_list->name, value) == 0) break;
		    value_list = value_list->next;			
		}
		if (value_list == NULL) {
			gda_log_error (_("Value \"%s\" for attribute %s of %s is not among the enumerated set\n"), value, attribute_name, element_name);
			return FALSE;
		}
	}
	return TRUE;
}
