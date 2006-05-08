/* gda-graph-item.c
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
#include "gda-graph-item.h"
#include <libgda/gda-dict.h>
#include <libgda/gda-object-ref.h>
#include <libgda/gda-xml-storage.h>

/* 
 * Main static functions 
 */
static void gda_graph_item_class_init (GdaGraphItemClass *class);
static void gda_graph_item_init (GdaGraphItem *graph);
static void gda_graph_item_dispose (GObject *object);
static void gda_graph_item_finalize (GObject *object);

static void gda_graph_item_set_property (GObject *object,
					guint param_id,
					const GValue *value,
					GParamSpec *pspec);
static void gda_graph_item_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec);

/* XML storage interface */
static void        gda_graph_item_xml_storage_init (GdaXmlStorageIface *iface);
static xmlNodePtr  gda_graph_item_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_graph_item_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);


#ifdef GDA_DEBUG
static void        gda_graph_item_dump                (GdaGraphItem *graph, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	MOVED,
	LAST_SIGNAL
};

static gint gda_graph_item_signals[LAST_SIGNAL] = { 0 };

/* properties */
enum
{
	PROP_0,
	PROP_REF_OBJECT
};


struct _GdaGraphItemPrivate
{
	GdaObjectRef    *ref_object;
	gdouble          x;
	gdouble          y;
};

/* module error */
GQuark gda_graph_item_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_graph_item_error");
	return quark;
}


GType
gda_graph_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaGraphItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_graph_item_class_init,
			NULL,
			NULL,
			sizeof (GdaGraphItem),
			0,
			(GInstanceInitFunc) gda_graph_item_init
		};
		
		static const GInterfaceInfo xml_storage_info = {
                        (GInterfaceInitFunc) gda_graph_item_xml_storage_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaGraphItem", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
	}
	return type;
}

static void
gda_graph_item_xml_storage_init (GdaXmlStorageIface *iface)
{
        iface->get_xml_id = NULL;
        iface->save_to_xml = gda_graph_item_save_to_xml;
        iface->load_from_xml = gda_graph_item_load_from_xml;
}


static void
gda_graph_item_class_init (GdaGraphItemClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	
	gda_graph_item_signals[MOVED] =
		g_signal_new ("moved",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaGraphItemClass, moved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	object_class->dispose = gda_graph_item_dispose;
	object_class->finalize = gda_graph_item_finalize;

	/* Properties */
	object_class->set_property = gda_graph_item_set_property;
	object_class->get_property = gda_graph_item_get_property;

	g_object_class_install_property (object_class, PROP_REF_OBJECT,
					 g_param_spec_pointer ("ref_object", NULL, NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_graph_item_dump;
#endif
}

static void
gda_graph_item_init (GdaGraphItem *graph)
{
	graph->priv = g_new0 (GdaGraphItemPrivate, 1);
	graph->priv->ref_object = NULL;
	graph->priv->x = 0.;
	graph->priv->y = 0.;
}

static void ref_lost_cb (GdaObjectRef *ref, GdaGraphItem *graph);

/**
 * gda_graph_item_new
 * @dict: a #GdaDict object
 * @ref_obj: a #GdaObject object which the new item will reference, or %NULL.
 *
 * Creates a new #GdaGraphItem object
 *
 * Returns: the newly created object
 */
GObject   *
gda_graph_item_new (GdaDict *dict, GdaObject *ref_obj)
{
	GObject *obj = NULL;
	GdaGraphItem *item;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);
	if (ref_obj)
		g_return_val_if_fail (GDA_IS_OBJECT (ref_obj), NULL);

	obj = g_object_new (GDA_TYPE_GRAPH_ITEM, "dict", ASSERT_DICT (dict), NULL);
	item = GDA_GRAPH_ITEM (obj);

	item->priv->ref_object = GDA_OBJECT_REF (gda_object_ref_new (ASSERT_DICT (dict)));
	if (ref_obj) 
		gda_object_ref_set_ref_object (item->priv->ref_object, ref_obj);

	g_signal_connect (G_OBJECT (item->priv->ref_object), "ref-lost",
			  G_CALLBACK (ref_lost_cb), item);

	return obj;
}

static void
ref_lost_cb (GdaObjectRef *ref, GdaGraphItem *graph)
{
	gda_object_destroy (GDA_OBJECT (graph));
}

static void
gda_graph_item_dispose (GObject *object)
{
	GdaGraphItem *graph;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPH_ITEM (object));

	graph = GDA_GRAPH_ITEM (object);
	if (graph->priv) {		
		if (graph->priv->ref_object) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (graph->priv->ref_object),
							      G_CALLBACK (ref_lost_cb), graph);
			g_object_unref (G_OBJECT (graph->priv->ref_object));
                        graph->priv->ref_object = NULL;
                }
	}

	/* parent class */
	parent_class->dispose (object);
}


static void
gda_graph_item_finalize (GObject   * object)
{
	GdaGraphItem *graph;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPH_ITEM (object));

	graph = GDA_GRAPH_ITEM (object);
	if (graph->priv) {
		g_free (graph->priv);
		graph->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_graph_item_set_property (GObject *object,
			   guint param_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	GdaGraphItem *graph;
	gpointer ptr;

	graph = GDA_GRAPH_ITEM (object);
	if (graph->priv) {
		switch (param_id) {
		case PROP_REF_OBJECT:
			g_assert (graph->priv->ref_object);
			if (graph->priv->ref_object) {
				ptr = g_value_get_pointer (value);
				gda_object_ref_set_ref_object (graph->priv->ref_object, ptr);
			}
			break;
		}
	}
}

static void
gda_graph_item_get_property (GObject *object,
			   guint param_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	GdaGraphItem *graph;

	graph = GDA_GRAPH_ITEM (object);
        if (graph->priv) {
                switch (param_id) {
                case PROP_REF_OBJECT:
			g_assert (graph->priv->ref_object);
			g_value_set_pointer (value, 
					     gda_object_ref_get_ref_object (graph->priv->ref_object));
                        break;
                }
        }
}

/**
 * gda_graph_item_set_position
 * @item: a #GdaGraphItemItem object
 * @x:
 * @y:
 *
 * Sets the position to be remembered for @item.
 */
void
gda_graph_item_set_position (GdaGraphItem *item, gdouble x, gdouble y)
{
	g_return_if_fail (item && GDA_IS_GRAPH_ITEM (item));
	g_return_if_fail (item->priv);
	
	if ((item->priv->x == x) && (item->priv->y == y))
		return;

	item->priv->x = x;
	item->priv->y = y;
	
#ifdef GDA_DEBUG_signal
	g_print (">> 'MOVED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (item), gda_graph_item_signals[MOVED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'MOVED' from %s::%s()\n", __FILE__, __FUNCTION__);
#endif
}

/**
 * gda_graph_item_get_position
 * @item: a #GdaGraphItemItem object
 * @x: a place where to store the X part of the position, or %NULL
 * @y: a place where to store the Y part of the position, or %NULL
 *
 * Get @item's position.
 */
void
gda_graph_item_get_position (GdaGraphItem *item, gdouble *x, gdouble *y)
{
	g_return_if_fail (item && GDA_IS_GRAPH_ITEM (item));
	g_return_if_fail (item->priv);
	
	if (x)
		*x = item->priv->x;
	if (y)
		*y = item->priv->y;
}


/**
 * gda_graph_item_get_ref_object
 * @item: a #GdaGraphItem object
 *
 * Get the referenced #GdaObject object, if it exists.
 *
 * Returns: the referenced object, or %NULL
 */
GdaObject *
gda_graph_item_get_ref_object (GdaGraphItem *item)
{
	g_return_val_if_fail (item && GDA_IS_GRAPH_ITEM (item), NULL);
	g_return_val_if_fail (item->priv, NULL);
	
	return gda_object_ref_get_ref_object (item->priv->ref_object);
}

#ifdef GDA_DEBUG
static void
gda_graph_item_dump (GdaGraphItem *item, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (item && GDA_IS_GRAPH_ITEM (item));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (item->priv) {
		GdaObject *base = gda_object_ref_get_ref_object (item->priv->ref_object);
		g_print ("%s" D_COL_H1 "GdaGraphItem" D_COL_NOR " for %p at (%.3f, %.3f) ", str, base,
			 item->priv->x, item->priv->y);
		
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, item);
}
#endif

/* 
 * GdaXmlStorage interface implementation
 */
static xmlNodePtr
gda_graph_item_save_to_xml (GdaXmlStorage *iface, GError **error)
{
        xmlNodePtr node = NULL;
	GdaGraphItem *item;
        gchar *str;
	GdaObject *base;

        g_return_val_if_fail (iface && GDA_IS_GRAPH_ITEM (iface), NULL);
        g_return_val_if_fail (GDA_GRAPH_ITEM (iface)->priv, NULL);

        item = GDA_GRAPH_ITEM (iface);

        node = xmlNewNode (NULL, "gda_graph_item");

	g_assert (item->priv->ref_object);
	base = gda_object_ref_get_ref_object (item->priv->ref_object);
	if (base) {
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
		xmlSetProp (node, "object", str);
		g_free (str);
	}
	
	str = g_strdup_printf ("%d.%03d", (int) item->priv->x, (int) ((item->priv->x - (int) item->priv->x) * 1000.));
	xmlSetProp (node, "xpos", str);
	g_free (str);

	str = g_strdup_printf ("%d.%03d", (int) item->priv->y, (int) ((item->priv->y - (int) item->priv->y) * 1000.));
	xmlSetProp (node, "ypos", str);
	g_free (str);

        return node;
}

/*
 * converts a 123.456 kind of string into its dgouble equivalent
 * without taking care of the locale settings
 */ 
static gdouble
parse_float (const gchar *str)
{
	gdouble retval = 0.;

	while (*str && g_ascii_isdigit (*str)) {
		retval = retval * 10 + (*str - '0');
		str++;
	}
	if (*str && *str == '.') {
		gdouble tmp = 0., divider = 1.;

		str++;
		while (*str && g_ascii_isdigit (*str)) {
			tmp = tmp * 10. + (*str - '0');
			divider *= 10.;
			str++;
		}

		retval += tmp / divider;
	}

	return retval;
}

static gboolean
gda_graph_item_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaGraphItem *item;
        gchar *prop;

        g_return_val_if_fail (iface && GDA_IS_GRAPH_ITEM (iface), FALSE);
        g_return_val_if_fail (GDA_GRAPH_ITEM (iface)->priv, FALSE);
        g_return_val_if_fail (node, FALSE);

	item = GDA_GRAPH_ITEM (iface);

	if (strcmp (node->name, "gda_graph_item")) {
                g_set_error (error,
                             GDA_GRAPH_ITEM_ERROR,
                             GDA_GRAPH_ITEM_XML_LOAD_ERROR,
                             _("XML Tag is not <gda_graph_item>"));
                return FALSE;
        }

	prop = xmlGetProp (node, "object");
	if (prop) {
		g_assert (item->priv->ref_object);
		gda_object_ref_set_ref_name (item->priv->ref_object, 0/* FIXME */, REFERENCE_BY_XML_ID, prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "xpos");
	if (prop) {
		item->priv->x = parse_float (prop);
		g_free (prop);
	}

	prop = xmlGetProp (node, "ypos");
	if (prop) {
		item->priv->y = parse_float (prop);
		g_free (prop);
	}

        return TRUE;
}
