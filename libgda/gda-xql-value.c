/* GdaXql: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
 * deGOBified by Cleber Rodrigues (cleber@gnome-db.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-xql-value.h"

static void gda_xql_value_init (GdaXqlValue *xqlval);
static void gda_xql_value_class_init (GdaXqlValueClass *klass);

static GdaXqlBinClass *parent_class = NULL;

GType
gda_xql_value_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlValueClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_value_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlValue),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_value_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_BIN, "GdaXqlValue", &info, 0);
	}

	return type;
}

static void 
gda_xql_value_init (GdaXqlValue *xqlval)
{

}

static void 
gda_xql_value_class_init (GdaXqlValueClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);
}

GdaXqlItem * 
gda_xql_value_new (void)
{
        GdaXqlItem *value;

        value = g_object_new (GDA_TYPE_XQL_VALUE, NULL);
        gda_xql_item_set_tag (value, "value");

        return value;
}

GdaXqlItem * 
gda_xql_value_new_with_data (gchar * id)
{
        GdaXqlItem *value;

        value = gda_xql_value_new ();
        if (id != NULL)
            gda_xql_item_set_attrib (value, "id", id);

        return value;
}
