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
#include "gda-xql-column.h"

static void gda_xql_column_class_init (GdaXqlColumnClass * klass);
static void gda_xql_column_init (GdaXqlColumn * col);

/*
 * GdaXqlColumn class implementation
 */
static void
gda_xql_column_class_init (GdaXqlColumnClass * klass)
{
}

static void
gda_xql_column_init (GdaXqlColumn * col)
{
}

GtkType
gda_xql_column_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaXmlAtomItem",
			sizeof (GdaXmlAtomItem),
			sizeof (GdaXmlAtomItemClass),
			(GtkClassInitFunc) gda_xql_column_class_init,
			(GtkObjectInitFunc) gda_xql_column_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL
		};
		type = gtk_type_unique (gda_xml_atom_item_get_type (), &info);
	}

	return type;
}

/**
 * gda_xql_column_new
 */
GdaXmlItem *
gda_xql_column_new (void)
{
	GdaXqlColumn *col;

	col = GDA_XQL_COLUMN (gtk_type_new (GDA_TYPE_XQL_COLUMN));
	gda_xml_item_set_tag (GDA_XML_ITEM (col), "column");

	gda_xml_item_set_attribute (GDA_XML_ITEM (col), "num", "1");
	gda_xml_item_set_attribute (GDA_XML_ITEM (col), "order", "asc");

	return GDA_XML_ITEM (col);
}

/**
 * gda_xql_column_new_with_data
 */
GdaXmlItem *
gda_xql_column_new_with_data (gint num, gboolean asc)
{
	GdaXqlColumn *col;
	gchar *numstr;
	gchar *ascstr;

	numstr = g_strdup_printf ("%d", num);
	ascstr = (asc) ? "asc" : "desc";

	col = GDA_XQL_COLUMN (gda_xql_column_new ());
	gda_xml_item_set_attribute (GDA_XML_ITEM (col), "num", numstr);
	gda_xml_item_set_attribute (GDA_XML_ITEM (col), "order", ascstr);
	g_free (numstr);

	return GDA_XML_ITEM (col);
}
