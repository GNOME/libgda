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

#include "gda-xql-list.h"
#include "gda-xql-utils.h"

struct _GdaXqlListPrivate {
	GSList * list;
};

static void gda_xql_list_init (GdaXqlList *xqllist);
static void gda_xql_list_class_init (GdaXqlListClass *klass);
static void gda_xql_list_add (GdaXqlItem *parent, GdaXqlItem *child);
static xmlNode *gda_xql_list_to_dom (GdaXqlItem * parent, xmlNode * parNode);
static GdaXqlItem * gda_xql_list_find_id (GdaXqlItem * parent, gchar * id);

static GdaXqlItemClass *parent_class = NULL;

GType
gda_xql_list_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlListClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_list_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlList),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_list_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_ITEM, "GdaXqlList", &info, 0);
	}

	return type;
}

static void
gda_xql_list_finalize(GObject *object)
{
	GdaXqlList *xqllist = GDA_XQL_LIST (object);

	if(G_OBJECT_CLASS(parent_class)->finalize) \
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);

	if(xqllist->priv->list) {
	  gda_xql_list_unref_list (xqllist->priv->list);
	  xqllist->priv->list = NULL;
	}
	g_free (xqllist->priv);
}

static void 
gda_xql_list_init (GdaXqlList *xqllist)
{
	xqllist->priv = g_new0 (GdaXqlListPrivate, 1);
	xqllist->priv->list = NULL;
}

static void 
gda_xql_list_class_init (GdaXqlListClass *klass)
{
        GObjectClass *g_object_class;
	GdaXqlItemClass *gda_xql_item_class;

	g_object_class = G_OBJECT_CLASS (klass);
	gda_xql_item_class = GDA_XQL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_xql_item_class->add = gda_xql_list_add;
	gda_xql_item_class->to_dom = gda_xql_list_to_dom;
	gda_xql_item_class->find_id = gda_xql_list_find_id;
	g_object_class->finalize = gda_xql_list_finalize;
}

GdaXqlItem * 
gda_xql_list_new (gchar * tag)
{
	GdaXqlItem *list;

	list = g_object_new (GDA_TYPE_XQL_LIST, NULL);

        gda_xql_item_set_tag (list, tag);

	return list;
}

static void 
gda_xql_list_add (GdaXqlItem *parent, GdaXqlItem *child)
{
	GdaXqlList *xqllist;

	g_return_if_fail (GDA_IS_XQL_LIST (parent));
	g_return_if_fail (parent != NULL);
	g_return_if_fail (GDA_IS_XQL_LIST (child));
	g_return_if_fail (child != NULL);

	xqllist = GDA_XQL_LIST (parent);
	
        xqllist->priv->list = g_slist_append (xqllist->priv->list, child);

        gda_xql_item_set_parent (child, parent);
}

static xmlNode * 
gda_xql_list_to_dom (GdaXqlItem *xqlitem, xmlNode *parent)
{
	GdaXqlList *xqllist;
        xmlNode *node;

	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (parent != NULL, NULL);

	xqllist = GDA_XQL_LIST (xqlitem);

	node = parent_class->to_dom ? parent_class->to_dom (xqlitem, parent) : NULL;

        g_slist_foreach(xqllist->priv->list, (GFunc) gda_xql_item_to_dom, node);

        return node;
}

void 
gda_xql_list_unref_list (GSList * list)
{
        g_slist_foreach (list, (GFunc) g_object_unref, NULL);
        g_slist_free(list);
}

GdaXqlItem * 
gda_xql_list_new_setlist (void)
{
        return gda_xql_list_new ("setlist");
}

GdaXqlItem * 
gda_xql_list_new_sourcelist (void)
{
        return gda_xql_list_new ("sourcelist");
}

GdaXqlItem * 
gda_xql_list_new_targetlist (void)
{
        return gda_xql_list_new ("targetlist");
}

GdaXqlItem * 
gda_xql_list_new_order (void)
{
        return gda_xql_list_new ("order");
}

GdaXqlItem * 
gda_xql_list_new_dest (void)
{
        return gda_xql_list_new ("dest");
}

GdaXqlItem * 
gda_xql_list_new_arglist (void)
{
        return gda_xql_list_new ("arglist");
}

GdaXqlItem * 
gda_xql_list_new_valuelist (void)
{
        return gda_xql_list_new ("valuelist");
}

GdaXqlItem * 
gda_xql_list_new_joinlist (void)
{
        return gda_xql_list_new("joinlist");
}

GdaXqlItem * 
gda_xql_list_new_and (void)
{
        return gda_xql_list_new("and");
}

GdaXqlItem * 
gda_xql_list_new_or (void)
{
        return gda_xql_list_new("or");
}

GdaXqlItem * 
gda_xql_list_new_group (void)
{
        return gda_xql_list_new("group");
}

static GdaXqlItem * 
gda_xql_list_find_id (GdaXqlItem *parent, gchar *id)
{
        GdaXqlItem *item;
	GdaXqlList *xqllist;
        GSList     *list;

	g_return_val_if_fail (GDA_IS_XQL_ITEM (parent), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);

	xqllist = GDA_XQL_LIST (parent);

	item = parent_class->find_id ? parent_class->find_id (parent, id) : NULL;

        if (item != NULL)
            return item;

        for (list = xqllist->priv->list; list != NULL; list = list->next) {
            item = gda_xql_item_find_id (list->data, id);
            if (item != NULL)
                return item;
        } 
        return NULL;
}
