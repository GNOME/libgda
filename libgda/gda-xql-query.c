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

#include "gda-xql-query.h"
#include "gda-xql-utils.h"

static void gda_xql_query_init (GdaXqlQuery *xqlquery);
static void gda_xql_query_class_init (GdaXqlQueryClass *klass);
static xmlNode *gda_xql_query_to_dom (GdaXqlItem *xqlitem, xmlNode *parent);

static GdaXqlBinClass *parent_class = NULL;

GType
gda_xql_query_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlQueryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_query_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlQuery),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_query_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_BIN, "GdaXqlQuery", &info, (GTypeFlags)0);
	}

	return type;
}

static void 
gda_xql_query_init (GdaXqlQuery *xqlquery)
{

}

static void 
gda_xql_query_class_init (GdaXqlQueryClass *klass)
{
	GdaXqlItemClass *xqlitemclass = GDA_XQL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	xqlitemclass->to_dom = gda_xql_query_to_dom;
}

GdaXqlItem * 
gda_xql_query_new (void)
{
        GdaXqlItem *query;

        query = (GdaXqlItem *) g_object_new (GDA_TYPE_XQL_QUERY, NULL);
        gda_xql_item_set_tag (query, "query");

        return query;
}

GdaXqlItem * 
gda_xql_query_new_with_data (GdaXqlItem * xqlitem)
{
        GdaXqlItem *query;

        query = gda_xql_query_new();
        gda_xql_item_add (query, xqlitem);
        return query;
}

static xmlNode* 
gda_xql_query_to_dom (GdaXqlItem *xqlitem, xmlNode *parent)
{
  xmlNode *node;
  GdaXqlItemClass *klass = GDA_XQL_ITEM_CLASS (xqlitem);

  g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
  g_return_if_fail (parent != NULL);

  node =  klass->to_dom (xqlitem, parent);

  return node;
}
