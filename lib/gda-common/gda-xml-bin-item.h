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

#if !defined(__gda_xml_bin_item_h__)
# define __gda_xml_bin_h__

#include <gda-xml-item.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define GDA_TYPE_XML_BIN_ITEM            (gda_xml_bin_item_get_type ())
#define GDA_XML_BIN_ITEM(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_BIN_ITEM, GdaXmlBinItem)
#define GDA_XML_BIN_ITEM_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_BIN_ITEM, GdaXmlBinItemClass)
#define GDA_IS_XML_BIN_ITEM(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_BIN_ITEM)
#define GDA_IS_XML_BIN_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_BIN_ITEM))

	typedef struct _GdaXmlBinItem GdaXmlBinItem;
	typedef struct _GdaXmlBinItemClass GdaXmlBinItemClass;
	typedef struct _GdaXmlBinItemPrivate GdaXmlBinItemPrivate;

	struct _GdaXmlBinItem
	{
		GdaXmlItem item;
		GdaXmlBinItemPrivate *priv;
	};

	struct _GdaXmlBinItemClass
	{
		GdaXmlItemClass parent_class;
	};

	GtkType gda_xml_bin_item_get_type (void);
	GdaXmlItem *gda_xml_bin_item_new (const gchar * tag);
	GdaXmlItem *gda_xml_bin_item_new_with_data (const gchar * tag,
						    GdaXmlItem * child);

	GdaXmlItem *gda_xml_bin_item_get_child (GdaXmlBinItem * bin);
	void gda_xml_bin_item_set_child (GdaXmlBinItem * bin,
					 GdaXmlItem * child);
	void gda_xml_bin_item_add (GdaXmlItem * item, GdaXmlItem * child);
	xmlNodePtr gda_xml_bin_item_to_dom (GdaXmlItem * item,
					    xmlNodePtr parent_node);
	GdaXmlItem *gda_xml_bin_item_find_id (GdaXmlItem * item,
					      const gchar * id);

#if defined(__cplusplus)
}
#endif

#endif
