/* GdaXql: the xml query object
 * Copyright (C) 2000-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Gerhard Dieringer <gdieringer@compuserve.com>
 * 	Cleber Rodrigues <cleber@gnome-db.org>
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

#include "gda-xql-delete.h"

#include "gda-xql-dml.h"
#include "gda-xql-utils.h"
#include "gda-xql-atom.h"
#include "gda-xql-func.h"
#include "gda-xql-field.h"
#include "gda-xql-value.h"
#include "gda-xql-const.h"
#include "gda-xql-dual.h"
#include <string.h>

static void gda_xql_delete_init (GdaXqlDelete *xqldel);
static void gda_xql_delete_class_init (GdaXqlDeleteClass *klass);
static void gda_xql_delete_add (GdaXqlItem *parent, GdaXqlItem *child);

static GdaXqlDmlClass *parent_class = NULL;

GType
gda_xql_delete_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlDeleteClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_delete_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlDelete),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_delete_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_DML, "GdaXqlDelete", &info, 0);
	}

	return type;
}

static void 
gda_xql_delete_init (GdaXqlDelete* xqldel)
{

}

static void 
gda_xql_delete_class_init (GdaXqlDeleteClass *klass)
{
	GdaXqlItemClass *itemclass = GDA_XQL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	itemclass->add = gda_xql_delete_add;
}


GdaXqlItem * 
gda_xql_delete_new (void)
{
	
        GdaXqlItem *delete;

	delete = g_object_new (GDA_TYPE_XQL_DELETE, NULL);
        gda_xql_item_set_tag (delete, "delete");

        return delete;
}

static void 
gda_xql_delete_add (GdaXqlItem *parent, GdaXqlItem *child)
{
	GdaXqlDml *dml;
	gchar *childtag;

	g_return_if_fail (GDA_IS_XQL_ITEM (parent));
	g_return_if_fail (parent != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (child));
	g_return_if_fail (child != NULL);

	dml = GDA_XQL_DML (parent);
        childtag = gda_xql_item_get_tag (child);

        if (!strcmp (childtag, "target")) {

	    if (dml->priv->target != NULL)
		g_object_unref ( G_OBJECT (dml->priv->target));
            dml->priv->target = child;

        } else if (!strcmp (childtag, "where")) {

	    if (dml->priv->where != NULL)
		g_object_unref ( G_OBJECT (dml->priv->where));
            dml->priv->where = child;

        } else {
            g_warning ("Invalid objecttype `%s' in delete\n", childtag);
            return;
	}

        gda_xql_item_set_parent (child, parent);
}
