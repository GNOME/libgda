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
#include "gda-common-private.h"
#include "gda-xml-item.h"
#include "gda-util.h"

struct _GdaXmlItemPrivate {
	gchar *tag;
	GHashTable *attrlist;
	GHashTable *idlist;
	GHashTable *reflist;
	GdaXmlItem *parent;
};

static void gda_xml_item_class_init (GdaXmlItemClass *klass);
static void gda_xml_item_init       (GdaXmlItem *item, GdaXmlItemClass *klass);
static void gda_xml_item_finalize   (GObject * object);

static GdaXmlItem *gda_xml_item_class_find_id (GdaXmlItem * item,
					       const gchar * id);
static xmlNodePtr gda_xml_item_class_to_dom (GdaXmlItem * item,
					     xmlNodePtr parent_node);

/**
 * Private functions
 */
static void
attr_to_dom (gchar * key, gchar * value, xmlNodePtr node)
{
	gda_xml_util_new_attr (key, value, node);
}

static void
destroy_attrlist (GHashTable * hashtab)
{
	g_hash_table_foreach_remove (hashtab,
				     (GHRFunc) gda_util_destroy_hash_pair,
				     g_free);
	g_hash_table_destroy (hashtab);
}


static void
destroy_idlist (GHashTable * hashtab)
{
	g_hash_table_foreach_remove (hashtab,
				     (GHRFunc) gda_util_destroy_hash_pair,
				     NULL);
	g_hash_table_destroy (hashtab);
}


static void
destroy_reflist (GHashTable * hashtab)
{
	g_hash_table_foreach_remove (hashtab,
				     (GHRFunc) gda_util_destroy_hash_pair,
				     g_object_unref);
	g_hash_table_destroy (hashtab);
}


/*
 * GdaXmlItem class implementation
 */
static void
gda_xml_item_class_init (GdaXmlItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_xml_item_finalize;
	klass->add = NULL;
	klass->to_dom = gda_xml_item_class_to_dom;
	klass->find_id = gda_xml_item_class_find_id;
}

static void
gda_xml_item_init (GdaXmlItem *item, GdaXmlItemClass *klass)
{
	item->priv = g_new (GdaXmlItemPrivate, 1);
	item->priv->attrlist = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gda_xml_item_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaXmlItem *item = (GdaXmlItem *) object;

	g_return_if_fail (GDA_IS_XML_ITEM (item));

	/* free memory */
	g_free (item->priv->tag);
	destroy_attrlist (item->priv->attrlist);
	destroy_idlist (item->priv->idlist);
	destroy_reflist (item->priv->reflist);
	gda_xml_item_free (item->priv->parent);
	g_free (item->priv);
	item->priv = NULL;

	parent_class = G_OBJECT_CLASS (g_type_peek_class_parent (G_TYPE_OBJECT));
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

GType
gda_xml_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xml_item_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlItem),
			0,
			(GInstanceInitFunc) gda_xml_item_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaXmlItem", &info, 0);
	}

	return type;
}

/**
 * gda_xml_item_free
 */
void
gda_xml_item_free (GdaXmlItem * item)
{
	g_return_if_fail (GDA_IS_XML_ITEM (item));
	g_object_unref (G_OBJECT (item));
}

/**
 * gda_xml_item_add
 */
void
gda_xml_item_add (GdaXmlItem *item, GdaXmlItem *child)
{
	GdaXmlItemClass *item_class;

	item_class = GDA_XML_ITEM_CLASS (g_type_class_peek (GDA_TYPE_XML_ITEM));
	if (item_class && item_class->add)
		item_class->add (item, child);
}

static xmlNodePtr
gda_xml_item_class_to_dom (GdaXmlItem * item, xmlNodePtr parent_node)
{
	xmlNodePtr node;

	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);

	node = gda_xml_util_new_node (item->priv->tag, parent_node);
	g_hash_table_foreach (item->priv->attrlist, (GHFunc) attr_to_dom,
			      node);

	return node;
}

/**
 * gda_xml_item_to_dom
 */
xmlNodePtr
gda_xml_item_to_dom (GdaXmlItem * item, xmlNodePtr parent_node)
{
	GdaXmlItemClass *item_class;

	item_class = G_OBJECT_CLASS (g_type_class_peek (GDA_TYPE_XML_ITEM));
	if (item_class && item_class->to_dom)
		return item_class->to_dom (item, parent_node);

	return NULL;
}

/**
 * gda_xml_item_get_attribute
 */
const gchar *
gda_xml_item_get_attribute (GdaXmlItem * item, const gchar * attrib)
{
	gchar *value;

	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);
	g_return_val_if_fail (attrib != NULL, NULL);

	value = (gchar *) g_hash_table_lookup (item->priv->attrlist, attrib);
	if (value == NULL)
		value = "";
	return (const gchar *) value;
}

/**
 * gda_xml_item_set_attribute
 */
void
gda_xml_item_set_attribute (GdaXmlItem * item,
			    const gchar * attrib, const gchar * value)
{
	gchar *oldval, *oldkey;

	g_return_if_fail (GDA_IS_XML_ITEM (item));

	if (g_hash_table_lookup_extended (item->priv->attrlist,
					  attrib,
					  (gpointer) & oldkey,
					  (gpointer) & oldval)) {
		g_hash_table_remove (item->priv->attrlist, attrib);
		g_free (oldval);
		g_free (oldkey);
	}
	g_hash_table_insert (item->priv->attrlist,
			     g_strdup (attrib), g_strdup (value));
}

/**
 * gda_xml_item_get_tag
 */
const gchar *
gda_xml_item_get_tag (GdaXmlItem * item)
{
	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);
	return (const gchar *) item->priv->tag;
}

/**
 * gda_xml_item_set_tag
 */
void
gda_xml_item_set_tag (GdaXmlItem * item, const gchar * tag)
{
	g_return_if_fail (GDA_IS_XML_ITEM (item));

	g_free (item->priv->tag);
	item->priv->tag = g_strdup (tag);
}

/**
 * gda_xml_item_get_parent
 */
GdaXmlItem *
gda_xml_item_get_parent (GdaXmlItem * item)
{
	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);
	return item->priv->parent;
}

/**
 * gda_xml_item_set_parent
 */
void
gda_xml_item_set_parent (GdaXmlItem * item, GdaXmlItem * parent)
{
	g_return_if_fail (GDA_IS_XML_ITEM (item));

	if (GDA_IS_XML_ITEM (item->priv->parent))
		gda_xml_item_free (item->priv->parent);
	item->priv->parent = parent;
	g_object_ref (G_OBJECT (parent));
}

/**
 * gda_xml_item_find_root
 */
GdaXmlItem *
gda_xml_item_find_root (GdaXmlItem * item)
{
	GdaXmlItem *parent;

	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);

	parent = item->priv->parent;
	if (parent == NULL)
		return item;
	return gda_xml_item_find_root (parent);
}

static GdaXmlItem *
gda_xml_item_class_find_id (GdaXmlItem * item, const gchar * id)
{
	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);

	if (item->priv->idlist == NULL)
		return NULL;
	return g_hash_table_lookup (item->priv->idlist, id);
}

/**
 * gda_xml_item_find_id
 */
GdaXmlItem *
gda_xml_item_find_id (GdaXmlItem * item, const gchar * id)
{
	GdaXmlItemClass *item_class;

	item_class = GDA_XML_ITEM_CLASS (g_type_class_peek (GDA_TYPE_XML_ITEM));
	if (item_class && item_class->find_id)
		return item_class->find_id (item, id);

	return NULL;
}

/**
 * gda_xml_item_find_ref
 */
GdaXmlItem *
gda_xml_item_find_ref (GdaXmlItem * item, const gchar * ref)
{
	g_return_val_if_fail (GDA_IS_XML_ITEM (item), NULL);

	if (item->priv->reflist == NULL)
		return NULL;
	return g_hash_table_lookup (item->priv->reflist, ref);
}

/**
 * gda_xml_item_add_id
 */
void
gda_xml_item_add_id (GdaXmlItem * item, const gchar * id)
{
	GdaXmlItem *root;

	g_return_if_fail (GDA_IS_XML_ITEM (item));
	g_return_if_fail (id != NULL);

	root = gda_xml_item_find_root (item);
	if (root->priv->idlist == NULL)
		root->priv->idlist = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (item->priv->idlist, g_strdup (id), item);
}

/**
 * gda_xml_item_add_ref
 */
void
gda_xml_item_add_ref (GdaXmlItem * item, const gchar * ref)
{
	GdaXmlItem *root, *ref_node;

	root = gda_xml_item_find_root (item);
	ref_node = gda_xml_item_find_id (root, ref);
	if (ref_node == NULL) {
		gda_log_message (_("Item with id %s not found"), ref);
		return;
	}

	if (item->priv->reflist == NULL)
		item->priv->reflist =
			g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (item->priv->reflist, g_strdup (ref), ref_node);
	g_object_ref (G_OBJECT (ref_node));
}
