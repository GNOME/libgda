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
#include "gda-xml-atom-item.h"

static void gda_xml_atom_item_class_init (GdaXmlAtomItemClass *klass);
static void gda_xml_atom_item_init       (GdaXmlAtomItem *atom_item, GdaXmlAtomItemClass *klass);

/*
 * GdaXmlAtomItem class implementation
 */
static void
gda_xml_atom_item_class_init (GdaXmlAtomItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
}

static void
gda_xml_atom_item_init (GdaXmlAtomItem *atom_item, GdaXmlAtomItemClass *klass)
{
}

GType
gda_xml_atom_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlAtomItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xml_atom_item_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlAtomItem),
			0,
			(GInstanceInitFunc) gda_xml_atom_item_init
		};
		type = g_type_register_static (GDA_TYPE_XML_ITEM,
					       "GdaXmlAtomItem",
					       &info, 0);
	}

	return type;
}

/**
 * gda_xml_atom_item_new
 */
GdaXmlItem *
gda_xml_atom_item_new (const gchar * tag)
{
	GdaXmlAtomItem *atom_item;

	atom_item = GDA_XML_ATOM_ITEM (g_object_new (GDA_TYPE_XML_ATOM_ITEM, NULL));
	gda_xml_item_set_tag (GDA_XML_ITEM (atom_item), tag);

	return GDA_XML_ITEM (atom_item);
}
