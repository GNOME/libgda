/* gda-graph.c
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
#include <glib/gi18n-lib.h>
#include "gda-graph.h"
#include <libgda/gda-dict.h>
#include <libgda/gda-object-ref.h>
#include <libgda/gda-xml-storage.h>
#include "gda-graph-item.h"

/* 
 * Main static functions 
 */
static void gda_graph_class_init (GdaGraphClass *class);
static void gda_graph_init (GdaGraph *graph);
static void gda_graph_dispose (GObject *object);
static void gda_graph_finalize (GObject *object);

static void gda_graph_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec);
static void gda_graph_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec);


static void init_ref_object (GdaGraph *graph);
static void destroyed_item_cb (GdaGraphItem *item, GdaGraph *graph);

static void graph_item_moved_cb (GdaGraphItem *item, GdaGraph *graph);


/* XML storage interface */
static void        gda_graph_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gda_graph_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gda_graph_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_graph_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);


#ifdef GDA_DEBUG
static void        gda_graph_dump                (GdaGraph *graph, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	ITEM_ADDED,
	ITEM_DROPPED,
	ITEM_MOVED,
	LAST_SIGNAL
};

static gint gda_graph_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_REF_OBJECT,
	PROP_GRAPH_TYPE
};


struct _GdaGraphPrivate
{
	GdaGraphType   type;
	GdaObjectRef    *ref_object;
	GSList       *graph_items;
};

/* module error */
GQuark gda_graph_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_graph_error");
	return quark;
}


GType
gda_graph_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaGraphClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_graph_class_init,
			NULL,
			NULL,
			sizeof (GdaGraph),
			0,
			(GInstanceInitFunc) gda_graph_init
		};
		
		static const GInterfaceInfo xml_storage_info = {
                        (GInterfaceInitFunc) gda_graph_xml_storage_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaGraph", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
	}
	return type;
}

static void
gda_graph_xml_storage_init (GdaXmlStorageIface *iface)
{
        iface->get_xml_id = gda_graph_get_xml_id;
        iface->save_to_xml = gda_graph_save_to_xml;
        iface->load_from_xml = gda_graph_load_from_xml;
}


static void
gda_graph_class_init (GdaGraphClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	
	gda_graph_signals[ITEM_ADDED] =
		g_signal_new ("item_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaGraphClass, item_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_graph_signals[ITEM_DROPPED] =
		g_signal_new ("item_dropped",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaGraphClass, item_dropped),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_graph_signals[ITEM_MOVED] =
		g_signal_new ("item_moved",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaGraphClass, item_moved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	
	class->item_added = NULL;
	class->item_dropped = NULL;
	class->item_moved = NULL;

	object_class->dispose = gda_graph_dispose;
	object_class->finalize = gda_graph_finalize;

	/* Properties */
	object_class->set_property = gda_graph_set_property;
	object_class->get_property = gda_graph_get_property;

	g_object_class_install_property (object_class, PROP_REF_OBJECT,
					 g_param_spec_pointer ("ref_object", NULL, NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_GRAPH_TYPE,
					 g_param_spec_int ("graph_type", NULL, NULL, 
							   1, G_MAXINT, 1, G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_graph_dump;
#endif
}

static void
gda_graph_init (GdaGraph *graph)
{
	graph->priv = g_new0 (GdaGraphPrivate, 1);
	graph->priv->type = GDA_GRAPH_DB_RELATIONS;
	graph->priv->ref_object = NULL;
	graph->priv->graph_items = NULL;
}

static void ref_object_ref_lost_cb (GdaObjectRef *ref, GdaGraph *graph);
static void
init_ref_object (GdaGraph *graph)
{
	graph->priv->ref_object = GDA_OBJECT_REF (gda_object_ref_new_no_ref_count (gda_object_get_dict (GDA_OBJECT (graph))));
	g_signal_connect (G_OBJECT (graph->priv->ref_object), "ref_lost",
			  G_CALLBACK (ref_object_ref_lost_cb), graph);
}
static void
ref_object_ref_lost_cb (GdaObjectRef *ref, GdaGraph *graph)
{
	gda_object_destroy (GDA_OBJECT (graph));
}

/**
 * gda_graph_new
 * @dict: a #GdaDict object
 * @type: the graph type (one of #GdaGraphType)
 *
 * Creates a new #GdaGraph object. The graph type is used only to be able to sort out the
 * different types of graphs. It brings no special functionnality.
 *
 * Returns: the newly created object
 */
GObject *
gda_graph_new (GdaDict *dict, GdaGraphType type)
{
	GObject *obj = NULL;
	GdaGraph *graph;
	guint id;
	gchar *str;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_GRAPH, "dict", ASSERT_DICT (dict), NULL);
	graph = GDA_GRAPH (obj);

	g_object_get (G_OBJECT (ASSERT_DICT (dict)), "graph_serial", &id, NULL);
	str = g_strdup_printf ("GR%u", id);
	gda_object_set_id (GDA_OBJECT (obj), str);
	g_free (str);

	graph->priv->type = type;

	gda_dict_declare_graph (ASSERT_DICT (dict), graph);

	return obj;
}

static void
gda_graph_dispose (GObject *object)
{
	GdaGraph *graph;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPH (object));

	graph = GDA_GRAPH (object);
	if (graph->priv) {		
		if (graph->priv->ref_object) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (graph->priv->ref_object),
							      G_CALLBACK (ref_object_ref_lost_cb), graph);
			g_object_unref (G_OBJECT (graph->priv->ref_object));
                        graph->priv->ref_object = NULL;
                }
		
		while (graph->priv->graph_items)
			destroyed_item_cb (GDA_GRAPH_ITEM (graph->priv->graph_items->data), graph);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
destroyed_item_cb (GdaGraphItem *item, GdaGraph *graph)
{
	g_assert (g_slist_find (graph->priv->graph_items, item));
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
					      G_CALLBACK (destroyed_item_cb) , graph);
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
					      G_CALLBACK (graph_item_moved_cb) , graph);

	graph->priv->graph_items = g_slist_remove (graph->priv->graph_items, item);
#ifdef GDA_DEBUG_signal
	g_print (">> 'ITEM_DROPPED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (graph), gda_graph_signals [ITEM_DROPPED], 0, item);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'ITEM_DROPPED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
	g_object_unref (G_OBJECT (item));
}


static void
gda_graph_finalize (GObject   * object)
{
	GdaGraph *graph;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPH (object));

	graph = GDA_GRAPH (object);
	if (graph->priv) {
		g_free (graph->priv);
		graph->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_graph_set_property (GObject *object,
		       guint param_id,
		       const GValue *value,
		       GParamSpec *pspec)
{
	GdaGraph *graph;
	gpointer ptr;

	graph = GDA_GRAPH (object);
	if (graph->priv) {
		switch (param_id) {
		case PROP_REF_OBJECT:
			if (!graph->priv->ref_object)
				init_ref_object (graph);

			if (graph->priv->ref_object) {
				ptr = g_value_get_pointer (value);
				g_signal_handlers_block_by_func (G_OBJECT (graph->priv->ref_object),
								 G_CALLBACK (ref_object_ref_lost_cb), graph);
				gda_object_ref_set_ref_object (graph->priv->ref_object, ptr);
				g_signal_handlers_unblock_by_func (G_OBJECT (graph->priv->ref_object),
								   G_CALLBACK (ref_object_ref_lost_cb), graph);
			}
			break;
		case PROP_GRAPH_TYPE:
			graph->priv->type = g_value_get_int (value);
			break;
		}
	}
}

static void
gda_graph_get_property (GObject *object,
		       guint param_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	GdaGraph *graph;

	graph = GDA_GRAPH (object);
        if (graph->priv) {
                switch (param_id) {
                case PROP_REF_OBJECT:
			if (graph->priv->ref_object) 
				g_value_set_pointer (value, 
						     gda_object_ref_get_ref_object (graph->priv->ref_object));
			else
				g_value_set_pointer (value, NULL);
                        break;
		case PROP_GRAPH_TYPE:
			g_value_set_int (value, graph->priv->type);
			break;
                }
        }
}


/**
 * gda_graph_get_graph_type
 * @graph: a #GdaGraph object
 *
 * Get the graph type of @graph.
 *
 * Returns: the type
 */
GdaGraphType
gda_graph_get_graph_type (GdaGraph *graph)
{
	g_return_val_if_fail (graph && GDA_IS_GRAPH (graph), GDA_GRAPH_DB_RELATIONS);
	g_return_val_if_fail (graph->priv, GDA_GRAPH_DB_RELATIONS);

	return graph->priv->type;
}

/**
 * gda_graph_get_items
 * @graph: a #GdaGraph object
 *
 * Get a list of #GdaGraphItem objects which are items of @graph
 *
 * Returns: a new list of #GdaGraphItem objects
 */
GSList *
gda_graph_get_items (GdaGraph *graph)
{
	g_return_val_if_fail (graph && GDA_IS_GRAPH (graph), NULL);
	g_return_val_if_fail (graph->priv, NULL);

	if (graph->priv->graph_items)
		return g_slist_copy (graph->priv->graph_items);
	else
		return NULL;
}



/**
 * gda_graph_get_item_from_obj
 * @graph: a #GdaGraph object
 * @ref_obj: the #GdaObject the returned item references
 * @create_if_needed:
 *
 * Get a pointer to a #GdaGraphItem item from the object is represents.
 * If the searched #GdaGraphItem is not found and @create_if_needed is TRUE, then a new
 * #GdaGraphItem is created.
 *
 * Returns: the #GdaGraphItem object, or %NULL if not found
 */
GdaGraphItem *
gda_graph_get_item_from_obj (GdaGraph *graph, GdaObject *ref_obj, gboolean create_if_needed)
{
	GdaGraphItem *item = NULL;
	GSList *list;
	GdaObject *obj;

	g_return_val_if_fail (graph && GDA_IS_GRAPH (graph), NULL);
	g_return_val_if_fail (graph->priv, NULL);
	g_return_val_if_fail (ref_obj, NULL);

	list = graph->priv->graph_items;
	while (list && !item) {
		g_object_get (G_OBJECT (list->data), "ref_object", &obj, NULL);
		if (obj == ref_obj)
			item = GDA_GRAPH_ITEM (list->data);
		list = g_slist_next (list);
	}

	if (!item && create_if_needed) {
		item = GDA_GRAPH_ITEM (gda_graph_item_new (gda_object_get_dict (GDA_OBJECT (graph)), ref_obj));
		
		gda_graph_add_item (graph, item);
		g_object_unref (G_OBJECT (item));
	}

	return item;
}


/**
 * gda_graph_add_item
 * @graph: a #GdaGraph object
 * @item: a #GdaGraphItem object
 *
 * Adds @item to @graph.
 */
void
gda_graph_add_item (GdaGraph *graph, GdaGraphItem *item)
{
	g_return_if_fail (graph && GDA_IS_GRAPH (graph));
	g_return_if_fail (graph->priv);
	g_return_if_fail (item && GDA_IS_GRAPH_ITEM (item));

	g_object_ref (G_OBJECT (item));
	
	graph->priv->graph_items = g_slist_append (graph->priv->graph_items, item);
	gda_object_connect_destroy (item, G_CALLBACK (destroyed_item_cb), graph);
	g_signal_connect (G_OBJECT (item), "moved",
			  G_CALLBACK (graph_item_moved_cb), graph);
#ifdef GDA_DEBUG_signal
	g_print (">> 'ITEM_ADDED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (graph), gda_graph_signals [ITEM_ADDED], 0, item);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'ITEM_ADDED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
}

static void
graph_item_moved_cb (GdaGraphItem *item, GdaGraph *graph)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'ITEM_MOVED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (graph), gda_graph_signals [ITEM_MOVED], 0, item);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'ITEM_MOVED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
}

/**
 * gda_graph_del_item
 * @graph: a #GdaGraph object
 * @item: a #GdaGraphItem object
 *
 * Removes @item from @graph
 */
void
gda_graph_del_item (GdaGraph *graph, GdaGraphItem *item)
{
	g_return_if_fail (graph && GDA_IS_GRAPH (graph));
	g_return_if_fail (graph->priv);
	g_return_if_fail (item && GDA_IS_GRAPH_ITEM (item));

	destroyed_item_cb (item, graph);
}

#ifdef GDA_DEBUG
static void
gda_graph_dump (GdaGraph *graph, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (graph && GDA_IS_GRAPH (graph));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (graph->priv) {
		GSList *items;
		if (gda_object_get_name (GDA_OBJECT (graph)))
			g_print ("%s" D_COL_H1 "GdaGraph" D_COL_NOR "\"%s\" (%p) ",
				 str, gda_object_get_name (GDA_OBJECT (graph)), graph);
		else
			g_print ("%s" D_COL_H1 "GdaGraph" D_COL_NOR " (%p) ", str, graph);
		g_print ("\n");
		items = graph->priv->graph_items;
		while (items) {
			gda_object_dump (GDA_OBJECT (items->data), offset+5);
			items = g_slist_next (items);
		}
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, graph);
}
#endif

/* 
 * GdaXmlStorage interface implementation
 */
static gchar *
gda_graph_get_xml_id (GdaXmlStorage *iface)
{
        g_return_val_if_fail (GDA_IS_GRAPH (iface), NULL);
        g_return_val_if_fail (GDA_GRAPH (iface)->priv, NULL);
	
	return g_strdup (gda_object_get_id (GDA_OBJECT (iface)));
}

static xmlNodePtr
gda_graph_save_to_xml (GdaXmlStorage *iface, GError **error)
{
        xmlNodePtr node = NULL;
	GdaGraph *graph;
        gchar *str = NULL;
	GSList *list;

        g_return_val_if_fail (iface && GDA_IS_GRAPH (iface), NULL);
        g_return_val_if_fail (GDA_GRAPH (iface)->priv, NULL);

        graph = GDA_GRAPH (iface);

        node = xmlNewNode (NULL, "gda_graph");

        str = gda_graph_get_xml_id (iface);
        xmlSetProp (node, "id", str);
        g_free (str);
	xmlSetProp (node, "name", gda_object_get_name (GDA_OBJECT (graph)));
        xmlSetProp (node, "descr", gda_object_get_description (GDA_OBJECT (graph)));

	switch (graph->priv->type) {
	case GDA_GRAPH_DB_RELATIONS:
		str = "R";
		break;
	case GDA_GRAPH_QUERY_JOINS:
		str = "Q";
		break;
	case GDA_GRAPH_MODELLING:
		str = "M";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
        xmlSetProp (node, "type", str);
	
	if (graph->priv->ref_object) {
		GdaObject *base = gda_object_ref_get_ref_object (graph->priv->ref_object);
		if (base) {
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
			xmlSetProp (node, "object", str);
			g_free (str);
		}
	}

	/* graph items */
	list = graph->priv->graph_items;
	while (list) {
		xmlNodePtr sub = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
		if (sub)
                        xmlAddChild (node, sub);
                else {
                        xmlFreeNode (node);
                        return NULL;
                }
		list = g_slist_next (list);
	}

        return node;
}

static gboolean
gda_graph_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaGraph *graph;
        gchar *prop;
	xmlNodePtr children;
	gboolean id=FALSE;

        g_return_val_if_fail (iface && GDA_IS_GRAPH (iface), FALSE);
        g_return_val_if_fail (GDA_GRAPH (iface)->priv, FALSE);
        g_return_val_if_fail (node, FALSE);

	graph = GDA_GRAPH (iface);

	if (strcmp (node->name, "gda_graph")) {
                g_set_error (error,
                             GDA_GRAPH_ERROR,
                             GDA_GRAPH_XML_LOAD_ERROR,
                             _("XML Tag is not <gda_graph>"));
                return FALSE;
        }

	prop = xmlGetProp (node, "id");
        if (prop) {
                if (strlen (prop) <= 2) {
                        g_set_error (error,
                                     GDA_GRAPH_ERROR,
                                     GDA_GRAPH_XML_LOAD_ERROR,
                                     _("Wrong 'id' attribute in <gda_graph>"));
                        return FALSE;
                }
                gda_object_set_id (GDA_OBJECT (graph), prop);
		id = TRUE;
                g_free (prop);
        }

	prop = xmlGetProp (node, "name");
        if (prop) {
                gda_object_set_name (GDA_OBJECT (graph), prop);
                g_free (prop);
        }

        prop = xmlGetProp (node, "descr");
        if (prop) {
                gda_object_set_description (GDA_OBJECT (graph), prop);
                g_free (prop);
        }

	prop = xmlGetProp (node, "type");
        if (prop) {
		switch (*prop) {
		case 'R':
			graph->priv->type = GDA_GRAPH_DB_RELATIONS;
			break;
		case 'Q':
			graph->priv->type = GDA_GRAPH_QUERY_JOINS;
			break;
		case 'M':
			graph->priv->type = GDA_GRAPH_MODELLING;
			break;
		default:
			g_set_error (error,
                                     GDA_GRAPH_ERROR,
                                     GDA_GRAPH_XML_LOAD_ERROR,
                                     _("Wrong 'type' attribute in <gda_graph>"));
                        return FALSE;
			break;
		}
                g_free (prop);
        }

	prop = xmlGetProp (node, "object");
	if (prop) {
		if (!graph->priv->ref_object)
			init_ref_object (graph);

		g_signal_handlers_block_by_func (G_OBJECT (graph->priv->ref_object),
						 G_CALLBACK (ref_object_ref_lost_cb), graph);
		gda_object_ref_set_ref_name (graph->priv->ref_object, 0/* FIXME */, REFERENCE_BY_XML_ID, prop);
		g_signal_handlers_unblock_by_func (G_OBJECT (graph->priv->ref_object),
						   G_CALLBACK (ref_object_ref_lost_cb), graph);
		g_free (prop);
	}

	/* items nodes */
	children = node->children;
	while (children) {
		if (!strcmp (children->name, "gda_graph_item")) {
			GdaGraphItem *item;
			
			item = GDA_GRAPH_ITEM (gda_graph_item_new (gda_object_get_dict (GDA_OBJECT (graph)), NULL));
			if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (item), children, error)) {
				gda_graph_add_item (graph, item);
				g_object_unref (G_OBJECT (item));
			}
			else
				return FALSE;
                }

		children = children->next;
	}

	if (!id) {
		g_set_error (error,
			     GDA_GRAPH_ERROR,
			     GDA_GRAPH_XML_LOAD_ERROR,
			     _("Missing ID attribute in <gda_graph>"));
		return FALSE;
        }

        return TRUE;
}

