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

#if !defined(__gda_xml_item_h__)
#  define __gda_xml_item_h__

#include <tree.h>
#include <parser.h>
#include <gtk/gtkobject.h>
#include <gda-common-defs.h>

/*
 * GdaXmlItem: an abstract class to be implemented by specific
 * XML nodes classes
 */

G_BEGIN_DECLS

#define GDA_TYPE_XML_ITEM (gda_xml_item_get_type ())
#define GDA_XML_ITEM(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_ITEM, GdaXmlItem)
#define GDA_XML_ITEM_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_ITEM, GdaXmlItemClass)
#define GDA_IS_XML_ITEM(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_ITEM)
#define GDA_IS_XML_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_ITEM))

typedef struct _GdaXmlItem GdaXmlItem;
typedef struct _GdaXmlItemClass GdaXmlItemClass;
typedef struct _GdaXmlItemPrivate GdaXmlItemPrivate;

struct _GdaXmlItem {
	GtkObject object;
	GdaXmlItemPrivate *priv;
};

struct _GdaXmlItemClass {
	GtkObjectClass *object_class;

	/* virtual methods */
	void (*add) (GdaXmlItem * item, GdaXmlItem * child);
	  xmlNodePtr (*to_dom) (GdaXmlItem * item,
				xmlNodePtr parent_node);
	GdaXmlItem *(*find_id) (GdaXmlItem * item, const gchar * id);
};

GtkType gda_xml_item_get_type (void);
void gda_xml_item_free (GdaXmlItem * item);

void gda_xml_item_add (GdaXmlItem * item, GdaXmlItem * child);
xmlNodePtr gda_xml_item_to_dom (GdaXmlItem * item,
				xmlNodePtr parent_node);

const gchar *gda_xml_item_get_attribute (GdaXmlItem * item,
					 const gchar * attrib);
void gda_xml_item_set_attribute (GdaXmlItem * item,
				 const gchar * attrib,
				 const gchar * value);
const gchar *gda_xml_item_get_tag (GdaXmlItem * item);
void gda_xml_item_set_tag (GdaXmlItem * item, const gchar * tag);
GdaXmlItem *gda_xml_item_get_parent (GdaXmlItem * item);
void gda_xml_item_set_parent (GdaXmlItem * item, GdaXmlItem * parent);

GdaXmlItem *gda_xml_item_find_root (GdaXmlItem * item);
GdaXmlItem *gda_xml_item_find_id (GdaXmlItem * item,
				  const gchar * id);
GdaXmlItem *gda_xml_item_find_ref (GdaXmlItem * item,
				   const gchar * ref);

void gda_xml_item_add_id (GdaXmlItem * item, const gchar * id);
void gda_xml_item_add_ref (GdaXmlItem * item, const gchar * ref);

G_END_DECLS

#endif
