/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
 *	Gerhard Dieringer <gdieringer@compuserve.com>
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

#include "config.h"
#include "gda-xml-list-item.h"

struct _GdaXmlListItemPrivate {
	GSList *list;
};

static void gda_xml_list_item_class_init (GdaXmlListItemClass *klass);
static void gda_xml_list_item_init       (GdaXmlListItem *list_item, GdaXmlListItemClass *klass);
static void gda_xml_list_item_finalize   (GObject * object);

/*
 * Private functions
 */
static void
unref_list (GSList * list)
{
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);
}

/*
 * GdaXmlListItem class implementation
 */
static void
gda_xml_list_item_class_init (GdaXmlListItemClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaXmlItemClass *item_class = GDA_XML_ITEM_CLASS (klass);

	object_class->finalize = gda_xml_list_item_finalize;
	item_class->add = gda_xml_list_item_add;
	item_class->to_dom = gda_xml_list_item_to_dom;
	item_class->find_id = gda_xml_list_item_find_id;
}

static void
gda_xml_list_item_init (GdaXmlListItem *list_item, GdaXmlListItemClass *klass)
{
	list_item->priv = g_new (GdaXmlListItemPrivate, 1);
	list_item->priv->list = NULL;
}

static void
gda_xml_list_item_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaXmlListItem *list_item = (GdaXmlListItem *) object;

	g_return_if_fail (GDA_IS_XML_LIST_ITEM (list_item));

	/* free memory */
	unref_list (list_item->priv->list);
	g_free (list_item->priv);

	parent_class = G_OBJECT_CLASS (g_type_peek_class_parent (GDA_TYPE_XML_LIST_ITEM));
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

GType
gda_xml_list_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlListItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xml_list_item_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlListItem),
			0,
			(GInstanceInitFunc) gda_xml_list_item_init
		};
		type = g_type_register_static (GDA_TYPE_XML_ITEM,
					       "GdaXmlListItem",
					       &info, 0);
	}
	return type;
}

/**
 * gda_xml_list_item_new
 */
GdaXmlItem *
gda_xml_list_item_new (const gchar * tag)
{
	GdaXmlListItem *list_item;

	list_item = GDA_XML_LIST_ITEM (g_object_new (GDA_TYPE_XML_LIST_ITEM, NULL));
	gda_xml_item_set_tag (GDA_XML_ITEM (list_item), tag);

	return GDA_XML_ITEM (list_item);
}

/**
 * gda_xml_list_item_add
 */
void
gda_xml_list_item_add (GdaXmlItem * item, GdaXmlItem * child)
{
	GdaXmlListItem *list_item = (GdaXmlListItem *) item;

	g_return_if_fail (GDA_IS_XML_LIST_ITEM (list_item));
	g_return_if_fail (GDA_IS_XML_ITEM (child));

	list_item->priv->list = g_slist_append (list_item->priv->list, child);
	gda_xml_item_set_parent (child, item);
}

/**
 * gda_xml_list_item_to_dom
 */
xmlNodePtr
gda_xml_list_item_to_dom (GdaXmlItem * item, xmlNodePtr parent_node)
{
	xmlNodePtr node = NULL;
	GdaXmlItemClass *item_class;
	GdaXmlListItem *list_item = (GdaXmlListItem *) item;

	g_return_val_if_fail (GDA_IS_XML_LIST_ITEM (list_item), NULL);

	item_class = GDA_XML_ITEM_CLASS (g_type_peek_class_parent (GDA_TYPE_XML_LIST_ITEM));
	if (item_class && item_class->to_dom) {
		node = item_class->to_dom (item, parent_node);
		g_slist_foreach (list_item->priv->list,
				 (GFunc) gda_xml_item_to_dom, node);
	}

	return node;
}

/**
 * gda_xml_list_item_find_id
 */
GdaXmlItem *
gda_xml_list_item_find_id (GdaXmlItem * item, const gchar * id)
{
	GdaXmlItem *id_item;
	GdaXmlItemClass *item_class;
	GSList *l;
	GdaXmlListItem *list_item = (GdaXmlListItem *) item;

	g_return_val_if_fail (GDA_IS_XML_LIST_ITEM (list_item), NULL);

	item_class = GDA_XML_ITEM_CLASS (g_type_peek_class_parent (GDA_TYPE_XML_LIST_ITEM));
	if (item_class && item_class->find_id) {
		id_item = item_class->find_id (item, id);
		if (id_item != NULL)
			return id_item;
	}

	for (l = list_item->priv->list; l != NULL; l = l->next) {
		id_item = gda_xml_item_find_id (GDA_XML_ITEM (l->data), id);
		if (id_item != NULL)
			return id_item;
	}

	return NULL;
}
