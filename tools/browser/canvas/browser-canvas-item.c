/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "browser-canvas-item.h"
#include "browser-canvas.h"
#include "../dnd.h"

static void browser_canvas_item_class_init (BrowserCanvasItemClass * class);
static void browser_canvas_item_init       (BrowserCanvasItem * item);
static void browser_canvas_item_dispose    (GObject *object);


static void browser_canvas_item_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void browser_canvas_item_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);
struct _BrowserCanvasItemPrivate
{
	gboolean            moving;
	double              xstart;
	double              ystart;
	gboolean            allow_move;
	gboolean            allow_select;

	gchar              *tooltip_text;
};

enum
{
	MOVED,
	MOVING,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_ALLOW_MOVE,
	PROP_ALLOW_SELECT,
	PROP_TOOLTIP_TEXT
};

static gint browser_canvas_item_signals[LAST_SIGNAL] = { 0, 0 };

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *base_parent_class = NULL;

GType
browser_canvas_item_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_item_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvasItem),
			0,
			(GInstanceInitFunc) browser_canvas_item_init,
			0
		};
		type = g_type_register_static (GOO_TYPE_CANVAS_GROUP, "BrowserCanvasItem", &info, 0);
	}

	return type;
}


static void
browser_canvas_item_class_init (BrowserCanvasItemClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	base_parent_class = g_type_class_peek_parent (class);

	browser_canvas_item_signals[MOVED] =
		g_signal_new ("moved",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (BrowserCanvasItemClass, moved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	browser_canvas_item_signals[MOVING] =
		g_signal_new ("moving",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (BrowserCanvasItemClass, moving),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
			      0);

	class->moved = NULL;
	class->moving = NULL;
	object_class->dispose = browser_canvas_item_dispose;

	/* virtual funstionc */
	class->extra_event = NULL;

	/* Properties */
	object_class->set_property = browser_canvas_item_set_property;
	object_class->get_property = browser_canvas_item_get_property;

	g_object_class_install_property
                (object_class, PROP_ALLOW_MOVE,
                 g_param_spec_boolean ("allow-move", NULL, NULL, FALSE, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
                (object_class, PROP_ALLOW_SELECT,
                 g_param_spec_boolean ("allow-select", NULL, NULL, FALSE, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(object_class, PROP_TOOLTIP_TEXT,
		 g_param_spec_string ("tip-text", NULL, NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static gboolean leave_notify_event (BrowserCanvasItem *citem, GooCanvasItem *target_item,
				    GdkEventCrossing *event, gpointer data);
static gboolean button_press_event (BrowserCanvasItem *citem, GooCanvasItem *target_item,
				    GdkEventButton *event, gpointer data);
static gboolean button_release_event (BrowserCanvasItem *citem, GooCanvasItem *target_item,
				      GdkEventButton *event, gpointer data);
static gboolean motion_notify_event (BrowserCanvasItem *citem, GooCanvasItem *target_item,
				     GdkEventMotion *event, gpointer data);


static void
browser_canvas_item_init (BrowserCanvasItem * item)
{
	item->priv = g_new0 (BrowserCanvasItemPrivate, 1);
	item->priv->moving = FALSE;
	item->priv->xstart = 0;
	item->priv->ystart = 0;
	item->priv->allow_move = FALSE;
	item->priv->tooltip_text = NULL;
	
	g_signal_connect (G_OBJECT (item), "leave-notify-event",
			  G_CALLBACK (leave_notify_event), NULL);
	g_signal_connect (G_OBJECT (item), "motion-notify-event",
			  G_CALLBACK (motion_notify_event), NULL);
	g_signal_connect (G_OBJECT (item), "button-press-event",
			  G_CALLBACK (button_press_event), NULL);
	g_signal_connect (G_OBJECT (item), "button-release-event",
			  G_CALLBACK (button_release_event), NULL);
	
}

static void
browser_canvas_item_dispose (GObject *object)
{
	BrowserCanvasItem *citem;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_ITEM (object));
	
	citem = BROWSER_CANVAS_ITEM (object);
	if (citem->priv) {
		if (citem->priv->tooltip_text) 
			g_free (citem->priv->tooltip_text);

		g_free (citem->priv);
		citem->priv = NULL;
	}

	/* for the parent class */
	base_parent_class->dispose (object);
}

static void 
browser_canvas_item_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	BrowserCanvasItem *citem = NULL;
	const gchar *str = NULL;

	citem = BROWSER_CANVAS_ITEM (object);

	switch (param_id) {
	case PROP_ALLOW_MOVE:
		citem->priv->allow_move = g_value_get_boolean (value);
		break;
	case PROP_ALLOW_SELECT:
		citem->priv->allow_select = g_value_get_boolean (value);
		break;
	case PROP_TOOLTIP_TEXT:
		str = g_value_get_string (value);
		if (citem->priv->tooltip_text) {
			g_free (citem->priv->tooltip_text);
			citem->priv->tooltip_text = NULL;
		}
		if (str)
			citem->priv->tooltip_text = g_strdup (str);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
browser_canvas_item_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
        BrowserCanvasItem *citem = NULL;
        
        citem = BROWSER_CANVAS_ITEM (object);
        switch (param_id) {
        case PROP_ALLOW_MOVE:
                g_value_set_boolean (value, citem->priv->allow_move);
                break;
        case PROP_ALLOW_SELECT:
                g_value_set_boolean (value, citem->priv->allow_select);
                break;
        case PROP_TOOLTIP_TEXT:
                g_value_set_string (value, citem->priv->tooltip_text);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
                break;
        }
        
}

/**
 * browser_canvas_item_get_canvas
 * @item: a #BrowserCanvasItem object
 *
 * Get the #BrowserCanvas on which @item is drawn
 *
 * Returns: the #BrowserCanvas widget
 */
BrowserCanvas *
browser_canvas_item_get_canvas (BrowserCanvasItem *item)
{
	g_return_val_if_fail (IS_BROWSER_CANVAS_ITEM (item), NULL);
	return (BrowserCanvas *) g_object_get_data (G_OBJECT (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (item))),
						     "browsercanvas");
}

/**
 * browser_canvas_item_get_edge_nodes
 * @item: a #BrowserCanvasItem object
 * @from: (nullable): a place to store the FROM part of the edge, or %NULL
 * @to: (nullable): a place to store the TO part of the edge, or %NULL
 *
 * If the @item canvas item represents a "link" between two other canvas items (an edge), then
 * set @from and @to to those items.
 */
void 
browser_canvas_item_get_edge_nodes (BrowserCanvasItem *item, 
				    BrowserCanvasItem **from, BrowserCanvasItem **to)
{
	BrowserCanvasItemClass *class;

	g_return_if_fail (IS_BROWSER_CANVAS_ITEM (item));

	class = BROWSER_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (item));
	if (class->get_edge_nodes)
		(class->get_edge_nodes) (item, from, to);
	else {
		if (from)
			*from = NULL;
		if (to)
			*to = NULL;
	}
}

static gboolean
leave_notify_event (BrowserCanvasItem *citem, G_GNUC_UNUSED GooCanvasItem *target_item,
		    G_GNUC_UNUSED GdkEventCrossing *event, G_GNUC_UNUSED gpointer data)
{
	gtk_widget_set_has_tooltip (GTK_WIDGET (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (citem))),
				    FALSE);
	return FALSE;
}

static gboolean
button_press_event (BrowserCanvasItem *citem, G_GNUC_UNUSED GooCanvasItem *target_item,
		    GdkEventButton *event, G_GNUC_UNUSED gpointer data)
{
	gboolean done = FALSE;

	switch (event->button) {
	case 1:
		if (event->state & GDK_SHIFT_MASK) {
			GdkDragContext *context;
			GtkTargetList *target_list;
			BrowserCanvas *canvas;
			BrowserCanvasItemClass *iclass = BROWSER_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (citem));
			
			if (iclass->drag_data_get) {
				canvas =  browser_canvas_item_get_canvas (citem);
				target_list = gtk_target_list_new (NULL, 0);
				gtk_target_list_add_table (target_list, dbo_table, G_N_ELEMENTS (dbo_table));
				context = gtk_drag_begin_with_coordinates (GTK_WIDGET (canvas),
									   target_list,
									   GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_DEFAULT,
									   event->button, (GdkEvent*) event,
									   -1, -1);
				gtk_drag_set_icon_default (context);
				gtk_target_list_unref (target_list);
				g_object_set_data (G_OBJECT (canvas), "__drag_src_item", citem);
			}
			done = TRUE;
		}
		else {
			if (citem->priv->allow_select && (event->state & GDK_CONTROL_MASK)) {
				browser_canvas_item_toggle_select (browser_canvas_item_get_canvas (citem), citem);
				done = TRUE;
			}
			if (citem->priv->allow_move) {
				/* movement management */
				goo_canvas_item_raise (GOO_CANVAS_ITEM (citem), NULL);
				citem->priv->xstart = event->x;
				citem->priv->ystart = event->y;
				citem->priv->moving = TRUE;
				done = TRUE;
				goo_canvas_pointer_grab (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (citem)),
							 GOO_CANVAS_ITEM (citem),
							 GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
							 NULL, event->time);
			}
		}
		break;
	default:
		break;
	}

	return done;
}

static gboolean
button_release_event (BrowserCanvasItem *citem, G_GNUC_UNUSED GooCanvasItem *target_item,
		      GdkEventButton *event, G_GNUC_UNUSED gpointer data)
{
	if (citem->priv->allow_move) {
		citem->priv->moving = FALSE;
		goo_canvas_pointer_ungrab (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (citem)),
					   GOO_CANVAS_ITEM (citem), event->time);
#ifdef debug_signal
		g_print (">> 'MOVED' from %s::item_event()\n", __FILE__);
#endif
		g_signal_emit (G_OBJECT (citem), browser_canvas_item_signals[MOVED], 0);
#ifdef debug_signal
		g_print ("<< 'MOVED' from %s::item_event()\n", __FILE__);
#endif
	}
	
	return FALSE;
}

static gboolean
motion_notify_event (BrowserCanvasItem *citem, G_GNUC_UNUSED GooCanvasItem *target_item,
		     GdkEventMotion *event, G_GNUC_UNUSED gpointer data)
{
	gboolean retval = FALSE;

	if (citem->priv->moving && (event->state & GDK_BUTTON1_MASK)) {
		g_assert (IS_BROWSER_CANVAS_ITEM (citem));
		goo_canvas_item_translate (GOO_CANVAS_ITEM (citem), 
					   (gdouble) event->x - citem->priv->xstart, 
					   (gdouble) event->y - citem->priv->ystart);
		g_signal_emit (G_OBJECT (citem), browser_canvas_item_signals[MOVING], 0);
		retval = TRUE;
	}
	else {
		if (citem->priv->tooltip_text)
			gtk_widget_set_tooltip_text (GTK_WIDGET (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (citem))),
						     citem->priv->tooltip_text);
	}
	
	return retval;
}

/**
 * browser_canvas_item_translate
 */
void
browser_canvas_item_translate (BrowserCanvasItem *item, gdouble dx, gdouble dy)
{
	g_return_if_fail (IS_BROWSER_CANVAS_ITEM (item));
	goo_canvas_item_translate (GOO_CANVAS_ITEM (item), dx, dy);
	g_signal_emit (G_OBJECT (item), browser_canvas_item_signals [MOVED], 0);
}
