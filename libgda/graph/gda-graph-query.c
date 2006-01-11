/* gda-graph-query.c
 *
 * Copyright (C) 2004 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include "gda-graph-query.h"
#include "gda-graph-item.h"
#include <libgda/gda-dict.h>
#include <libgda/gda-query.h>
#include <libgda/gda-query-target.h>

/* 
 * Main static functions 
 */
static void gda_graph_query_class_init (GdaGraphQueryClass *class);
static void gda_graph_query_init (GdaGraphQuery *graph);
static void gda_graph_query_dispose (GObject *object);
static void gda_graph_query_finalize (GObject *object);

static void gda_graph_query_initialize (GdaGraphQuery *graph);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


struct _GdaGraphQueryPrivate
{
	GdaQuery   *query;
};


GType
gda_graph_query_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaGraphQueryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_graph_query_class_init,
			NULL,
			NULL,
			sizeof (GdaGraphQuery),
			0,
			(GInstanceInitFunc) gda_graph_query_init
		};

		type = g_type_register_static (GDA_TYPE_GRAPH, "GdaGraphQuery", &info, 0);
	}
	return type;
}



static void
gda_graph_query_class_init (GdaGraphQueryClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	
	object_class->dispose = gda_graph_query_dispose;
	object_class->finalize = gda_graph_query_finalize;
}

static void
gda_graph_query_init (GdaGraphQuery *graph)
{
	graph->priv = g_new0 (GdaGraphQueryPrivate, 1);
	graph->priv->query = NULL;
}

/**
 * gda_graph_query_new
 * @query: a #GdaQuery object
 *
 * Creates a new #GdaGraphQuery object. This graph object is a specialized #GdaGraph object
 * in the way that it always make sure it "contains" #GdaGraphItem objects for each target
 * in @query.
 *
 * However, when created, the new #GdaGraphItem object will not contain any graph item; 
 * but can be brought in sync with the gda_graph_query_sync_targets() method.
 *
 * Returns: the newly created object
 */
GObject *
gda_graph_query_new (GdaQuery *query)
{
	GObject *obj = NULL;
	GdaGraphQuery *graph;
	guint id;
	GdaDict *dict;
	gchar *str;

	g_return_val_if_fail (query && GDA_IS_QUERY (query), NULL);
	dict = gda_object_get_dict (GDA_OBJECT (query));

	obj = g_object_new (GDA_TYPE_GRAPH_QUERY, "dict", dict, NULL);
	graph = GDA_GRAPH_QUERY (obj);

	g_object_get (G_OBJECT (dict), "graph_serial", &id, NULL);
	str = g_strdup_printf ("GR%u", id);
	gda_object_set_id (GDA_OBJECT (obj), str);
	g_free (str);

	gda_dict_declare_graph (dict, GDA_GRAPH (graph));
	g_object_set (obj, "graph_type", GDA_GRAPH_QUERY_JOINS, "ref_object", query, NULL);

	/* REM: we don't need to catch @query's nullification because #GdaGraph already does it. */
	graph->priv->query = query;

	gda_graph_query_initialize (graph);

	return obj;
}

static void target_added_cb (GdaQuery *query, GdaQueryTarget *target, GdaGraphQuery *graph);
static void target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaGraphQuery *graph);

static void
gda_graph_query_dispose (GObject *object)
{
	GdaGraphQuery *graph;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPH_QUERY (object));

	graph = GDA_GRAPH_QUERY (object);
	if (graph->priv) {
		if (graph->priv->query) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (graph->priv->query),
							      G_CALLBACK (target_added_cb), graph);
			g_signal_handlers_disconnect_by_func (G_OBJECT (graph->priv->query),
							      G_CALLBACK (target_removed_cb), graph);
			graph->priv->query = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}


static void
gda_graph_query_finalize (GObject   * object)
{
	GdaGraphQuery *graph;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPH_QUERY (object));

	graph = GDA_GRAPH_QUERY (object);
	if (graph->priv) {
		g_free (graph->priv);
		graph->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
gda_graph_query_initialize (GdaGraphQuery *graph)
{
	/* catch the creation and deletion of any new target */
	g_signal_connect (G_OBJECT (graph->priv->query), "target_added",
			  G_CALLBACK (target_added_cb), graph);
	g_signal_connect (G_OBJECT (graph->priv->query), "target_removed",
			  G_CALLBACK (target_removed_cb), graph);
}

static void
target_added_cb (GdaQuery *query, GdaQueryTarget *target, GdaGraphQuery *graph)
{
	GdaGraphItem *gitem;

	/* There is a possibility that the graph item has already been created somewhere else,
	   in which case there is nothing to be done */
	gitem = gda_graph_get_item_from_obj (GDA_GRAPH (graph), GDA_OBJECT (target), FALSE);
	if (gitem)
		return;

	/* FIXME: find a better  positionning layout for the target items */
	gitem = GDA_GRAPH_ITEM (gda_graph_item_new (gda_object_get_dict (GDA_OBJECT (graph->priv->query)), GDA_OBJECT (target)));
	gda_graph_item_set_position (gitem, 50., 50.);
	gda_graph_add_item (GDA_GRAPH (graph), gitem);
	g_object_unref (G_OBJECT (gitem));
}

static void
target_removed_cb (GdaQuery *query, GdaQueryTarget *target, GdaGraphQuery *graph)
{
	GdaGraphItem *gitem;

	gitem = gda_graph_get_item_from_obj (GDA_GRAPH (graph), GDA_OBJECT (target), FALSE);
	if (gitem)
		gda_graph_del_item (GDA_GRAPH (graph), gitem);
}

/**
 * gda_graph_query_sync_targets
 * @graph: a #GdaGraphQuery object
 *
 * Synchronises the graph items with the targets of the query @graph represents
 */
void
gda_graph_query_sync_targets (GdaGraphQuery *graph)
{
	GSList *targets, *list;

	g_return_if_fail (graph && GDA_IS_GRAPH_QUERY (graph));
	g_return_if_fail (graph->priv);

	/* for each target in graph->priv->query, we create a #GdaGraphItem, if necessary */
	targets = gda_query_get_targets (graph->priv->query);
	list = targets;
	while (list) {
		target_added_cb (graph->priv->query, GDA_QUERY_TARGET (list->data), graph);
		list = g_slist_next (list);
	}
	g_slist_free (targets);
}
