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

#if !defined(__gda_xml_atom_item_h__)
#  define __gda_xml_atom_item_h__

#include <gda-common-defs.h>
#include <gda-xml-item.h>

G_BEGIN_DECLS

#define GDA_TYPE_XML_ATOM_ITEM            (gda_xml_atom_item_get_type ())
#define GDA_XML_ATOM_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_ATOM_ITEM, GdaXmlAtomItem))
#define GDA_XML_ATOM_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_ATOM_ITEM, GdaXmlAtomItemClass))
#define GDA_IS_XML_ATOM_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_XML_ATOM_ITEM))
#define GDA_IS_XML_ATOM_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_ATOM_ITEM))

typedef struct _GdaXmlAtomItem GdaXmlAtomItem;
typedef struct _GdaXmlAtomItemClass GdaXmlAtomItemClass;

struct _GdaXmlAtomItem {
	GdaXmlItem item;
};

struct _GdaXmlAtomItemClass {
	GdaXmlItemClass parent_class;
};

GType       gda_xml_atom_item_get_type (void);
GdaXmlItem *gda_xml_atom_item_new (const gchar * tag);

G_END_DECLS

#endif
