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
#include "gda-xql-const.h"

static void gda_xql_const_class_init (GdaXqlConstClass *klass);
static void gda_xql_const_init       (GdaXqlConst *xc);

/*
 * GdaXqlConst class implementation
 */
static void
gda_xql_const_class_init (GdaXqlConstClass *klass)
{
}

static void
gda_xql_const_init (GdaXqlConst *xc)
{
}

GtkType
gda_xql_const_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaXqlConst",
			sizeof (GdaXqlConstItem),
			sizeof (GdaXqlConstClass),
			(GtkClassInitFunc) gda_xql_const_item_class_init,
			(GtkObjectInitFunc) gda_xql_const_item_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL
		};
		type = gtk_type_unique (gda_xml_atom_item_get_type (), &info);
	}

	return type;
}

/**
 * gda_xql_const_new
 */
GdaXmlItem *
gda_xql_const_new (void)
{
	GdaXqlConst *xc;

	xc = GDA_XQL_CONST (gtk_type_new (GDA_TYPE_XQL_CONST));
	gda_xml_item_set_tag (GDA_XML_ITEM (xc), "const");

	return GDA_XML_ITEM (xc);
}

/**
 * gda_xql_const_new_with_data
 */
GdaXmlItem *
gda_xql_const_new_with_data (const gchar *value,
			     const gchar *alias,
			     const gchar *type,
			     const gchar *null)
{
	GdaXqlConst *xc;

	xc = GDA_XQL_CONST (gda_xql_const_new ());

	if (value != NULL)
		gda_xml_item_set_attrib (GDA_XML_ITEM (xc), "value", value);
	if (alias != NULL)
		gda_xml_item_set_attrib (GDA_XML_ITEM (xc), "alias", alias);
	if (type != NULL)
		gda_xml_item_set_attrib (GDA_XML_ITEM (xc), "type", type);
	if (null != NULL)
		gda_xml_item_set_attrib (GDA_XML_ITEM (xc), "null", null);

	return GDA_XML_ITEM (xc);
}
