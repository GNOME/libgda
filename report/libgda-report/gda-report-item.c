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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/valid.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-item.h>


static void gda_report_item_class_init (GdaReportItemClass *klass);
static void gda_report_item_init       (GdaReportItem *valid,
					   GdaReportItemClass *klass);
static void gda_report_item_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;


/*
 * GdaReportItem class implementation
 */
static void
gda_report_item_class_init (GdaReportItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_finalize;
}


static void
gda_report_item_init (GdaReportItem *item, 
		         GdaReportItemClass *klass)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM (item));

	item->priv = g_new0 (GdaReportItemPrivate, 1);
	item->priv->node = NULL;
	item->priv->valid = NULL;
}


static void
gda_report_item_finalize (GObject *object)
{
	GdaReportItem *item = (GdaReportItem *) object;

	g_return_if_fail (GDA_REPORT_IS_ITEM (object));

	xmlFreeNode (item->priv->node);
	g_free (item->priv);
	
	parent_class->finalize (object);
}


GType
gda_report_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_item_class_init,
			NULL,
			NULL,
			sizeof (GdaReportItem),
			0,
			(GInstanceInitFunc) gda_report_item_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaReportItem", &info, 0);
	}
	return type;
}


/**
 * gda_report_item_new
 * @valid: a #GdaReportValid object
 * @name: name of the item to be created
 *
 * Creates a new item with the given name, and using the given #GdaReportValid object
 *
 * Return: the new #GdaReportItem or NULL if there is some problem
 **/
GdaReportItem *
gda_report_item_new (GdaReportValid *valid,
		     const gchar *name)
{
	GdaReportItem *item;

	g_return_val_if_fail (GDA_IS_REPORT_VALID (valid), NULL);
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM, NULL);
	item->priv->valid = valid;
	item->priv->node = xmlNewNode (NULL, name);	
	return item;
}


/**
 * gda_report_item_new_child
 * @parent: the parent #GdaReportItem object
 * @name: name of the item to be created
 *
 * Creates a new item with the given name, as a child of the parent object
 *
 * Return: the new #GdaReportItem or NULL if there is some problem
 **/
GdaReportItem *
gda_report_item_new_child (GdaReportItem *parent,
		           const gchar *name)
{
	GdaReportItem *item;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM (parent), NULL);

	item = g_object_new (GDA_REPORT_TYPE_ITEM, NULL);
	item->priv->valid = parent->priv->valid;
	item->priv->node = xmlNewNode (NULL, name);
	if (xmlAddChild (parent->priv->node, item->priv->node) == NULL)
	{
		gda_log_error (_("Error setting parent of new node"));
		g_object_unref (G_OBJECT(item));
		return NULL;
	}
	
	return item;
}


/**
 * gda_report_item_new_from_dom 
 * @node: a xmlNodePtr, assumed to be a valid gda-report element
 *
 * Creates a new item from a given xml node
 *
 * Return: the new #GdaReportItem or NULL if there is some problem
 **/
GdaReportItem *
gda_report_item_new_from_dom (xmlNodePtr node)
{
	GdaReportItem *item;

	g_return_val_if_fail (node != NULL, NULL);

	item = g_object_new (GDA_REPORT_TYPE_ITEM, NULL);
	item->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	item->priv->node = node;

	return item;	
}


/**
 * gda_report_item_to_dom
 * @item: a #GdaReportItem
 *
 * Returns: the xml representation of the item, or NULL if there is some problem
 **/
xmlNodePtr 
gda_report_item_to_dom (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM (item), NULL);
	return item->priv->node;
}


/**
 * gda_report_item_set_parent
 * @parent: a #GdaReportItem
 * @child: an already created #GdaReportItem
 *
 * Sets child item as a child of parent item
 *
 * Returns: TRUE if all is ok, FALSE otherwise
 **/
gboolean
gda_report_item_set_parent (GdaReportItem *parent,
			    GdaReportItem *child)
{
	if (xmlAddChild (parent->priv->node, child->priv->node) == NULL)
	{
		gda_log_error (_("Error setting parent -> child relation"));
		return FALSE;
	}	
	return TRUE;
}


/**
 * gda_report_item_set_attribute
 * @item: a #GdaReportItem object
 * @name: name of the attribute to be set
 * @value: value to be set
 *
 * Validates the attribute and the value and, if all is right,
 * sets the given value to the attribute of given item
 *
 * Returns: TRUE if all is ok, FALSE otherwise
 **/
gboolean 
gda_report_item_set_attribute (GdaReportItem *item,
			       const gchar *name,
			       const gchar *value)
{
	xmlAttrPtr attribute;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM (item), FALSE);
	
	if (gda_report_valid_validate_attribute (
				item->priv->valid, 
				item->priv->node->name, 
				name, 
				value)) 
	{
		
		attribute = xmlSetProp (item->priv->node, name, value);
		if (attribute == NULL) 
		{
			gda_log_error (_("Error setting value %s to attribute %s of item %s"), 
					value, 
					name, 
					item->priv->node->name); 
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}


/**
 * gda_report_item_get_attribute
 * @item: a #GdaReportItem object
 * @name: an attribute name
 *
 * Returns: the value of given attribute in given item.  If attribute is not set, 
 * but a default value is defined in the DTD, this defaults value is returned.
 * If there is some problem, or attribute is not defined and there is no default value,
 * NULL is returned
 **/
gchar * 
gda_report_item_get_attribute (GdaReportItem *item, 
			       const gchar *name)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM (item), NULL);
	return xmlGetProp(item->priv->node, name);
}



/**
 * gda_report_item_get_inherit_attribute
 * @item: a #GdaReportItem object
 * @name: an attribute name
 *
 * Searches for the attribute in all ancestors of the item
 *
 * Returns: the value of the attribute in the first ancestor where found, 
 * or NULL if not found or there is some problem
 **/
gchar *
gda_report_item_get_inherit_attribute (GdaReportItem *item,
			 	       const gchar *name)
{
	xmlNodePtr node;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM (item), NULL);
	node = item->priv->node->parent;
	
	while (node != NULL)
	{
		if (xmlHasProp(node, name))
			return xmlGetProp(node, name);
		
		node = node->parent;
	}	
	return NULL;
}

