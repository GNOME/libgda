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
#include "gda-xml-bin-item.h"

struct _GdaXmlBinItemPrivate
{
	GdaXmlItem *child;
};

static void gda_xml_bin_item_class_init (GdaXmlBinItemClass * klass);
static void gda_xml_bin_item_init (GdaXmlBinItem * bin);
static void gda_xml_bin_item_destroy (GtkObject * object);

/*
 * GdaXmlBinItem class implementation
 */
static void
gda_xml_bin_item_class_init (GdaXmlBinItemClass * klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	GdaXmlItemClass *item_class = GDA_XML_ITEM_CLASS (klass);

	object_class->destroy = gda_xml_bin_item_destroy;
	item_class->add = gda_xml_bin_item_add;
	item_class->to_dom = gda_xml_bin_item_to_dom;
	item_class->find_id = gda_xml_bin_item_find_id;
}

static void
gda_xml_bin_item_init (GdaXmlBinItem * bin)
{
	bin->priv = g_new (GdaXmlBinItemPrivate, 1);
	bin->priv->child = NULL;
}

static void
gda_xml_bin_item_destroy (GtkObject * object)
{
	GtkObjectClass *parent_class;
	GdaXmlBinItem *bin = (GdaXmlBinItem *) object;

	g_return_if_fail (GDA_IS_XML_BIN_ITEM (bin));

	/* free memory */
	gtk_object_unref (GTK_OBJECT (bin->priv->child));
	g_free (bin->priv);

	parent_class = gtk_type_class (GDA_TYPE_XML_ITEM);
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}

GtkType
gda_xml_bin_item_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaXmlBinItem",
			sizeof (GdaXmlBinItem),
			sizeof (GdaXmlBinItemClass),
			(GtkClassInitFunc) gda_xml_bin_item_class_init,
			(GtkObjectInitFunc) gda_xml_bin_item_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL
		};
		type = gtk_type_unique (gda_xml_item_get_type (), &info);
	}

	return type;
}

/**
 * gda_xml_bin_item_new
 */
GdaXmlItem *
gda_xml_bin_item_new (const gchar * tag)
{
	GdaXmlItem *bin;

	bin = GDA_XML_ITEM (gtk_type_new (GDA_TYPE_XML_BIN_ITEM));
	gda_xml_item_set_tag (bin, tag);

	return bin;
}

/**
 * gda_xml_bin_item_new_with_data
 */
GdaXmlItem *
gda_xml_bin_item_new_with_data (const gchar * tag, GdaXmlItem * child)
{
	GdaXmlItem *bin;

	bin = gda_xml_bin_item_new (tag);
	gda_xml_item_add (bin, child);

	return bin;
}

/**
 * gda_xml_bin_item_get_child
 */
GdaXmlItem *
gda_xml_bin_item_get_child (GdaXmlBinItem * bin)
{
	g_return_val_if_fail (GDA_IS_XML_BIN_ITEM (bin), NULL);
	return bin->priv->child;
}

/**
 * gda_xml_bin_item_set_child
 */
void
gda_xml_bin_item_set_child (GdaXmlBinItem * bin, GdaXmlItem * child)
{
	g_return_if_fail (GDA_IS_XML_BIN_ITEM (bin));
	g_return_if_fail (GDA_IS_XML_ITEM (child));

	if (bin->priv->child != NULL)
		gtk_object_unref (GTK_OBJECT (bin->priv->child));
	bin->priv->child = child;
	gtk_object_ref (GTK_OBJECT (child));
}

/**
 * gda_xml_bin_item_add
 */
void
gda_xml_bin_item_add (GdaXmlItem * item, GdaXmlItem * child)
{
	GdaXmlBinItem *bin = (GdaXmlBinItem *) item;

	g_return_if_fail (GDA_IS_XML_BIN_ITEM (bin));

	if (bin->priv->child != NULL)
		gtk_object_unref (GTK_OBJECT (bin->priv->child));
	bin->priv->child = child;

	gda_xml_item_set_parent (child, item);
}

/**
 * gda_xml_bin_item_to_dom
 */
xmlNodePtr
gda_xml_bin_item_to_dom (GdaXmlItem * item, xmlNodePtr parent_node)
{
	GdaXmlItemClass *item_class;
	xmlNodePtr node;
	GdaXmlBinItem *bin = (GdaXmlBinItem *) bin;

	g_return_val_if_fail (GDA_IS_XML_BIN_ITEM (bin), NULL);

	/* call the parent class 'to_dom' handler */
	item_class = gtk_type_class (GDA_TYPE_XML_ITEM);
	if (item_class && item_class->to_dom) {
		node = item_class->to_dom (item, parent_node);
		gda_xml_item_to_dom (bin->priv->child, node);
		return node;
	}

	return NULL;
}

/**
 * gda_xml_bin_item_find_id
 */
GdaXmlItem *
gda_xml_bin_item_find_id (GdaXmlItem * item, const gchar * id)
{
	GdaXmlItem *id_item = NULL;
	GdaXmlItemClass *item_class;
	GdaXmlBinItem *bin = (GdaXmlBinItem *) item;

	g_return_val_if_fail (GDA_IS_XML_BIN_ITEM (bin), NULL);

	item_class = gtk_type_class (GDA_TYPE_XML_ITEM);
	if (item_class && item_class->find_id)
		id_item = item_class->find_id (item, id);

	if (id_item != NULL)
		return id_item;

	return gda_xml_item_find_id (item, id);
}
