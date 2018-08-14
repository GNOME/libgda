/*
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include "widget-overlay.h"
#include <stdarg.h>
#include <glib/gi18n-lib.h>

#define SCALE_MIN .6
#define SCALE_MAX 1.
#define SCALE_MAX_SCALE 1.5
#define SCALE_STEP ((SCALE_MAX - SCALE_MIN) / 20.)

static void     widget_overlay_realize       (GtkWidget       *widget);
static void     widget_overlay_unrealize     (GtkWidget       *widget);
static void     widget_overlay_get_preferred_width  (GtkWidget *widget,
						     gint      *minimum,
						     gint      *natural);
static void     widget_overlay_get_preferred_height (GtkWidget *widget,
						     gint      *minimum,
						     gint      *natural);
static void     widget_overlay_size_allocate (GtkWidget       *widget,
					      GtkAllocation   *allocation);
static gboolean widget_overlay_damage        (GtkWidget       *widget,
					      GdkEventExpose  *event);
static gboolean widget_overlay_draw          (GtkWidget       *widget,
					      cairo_t         *cr);
static void     widget_overlay_add           (GtkContainer    *container,
					      GtkWidget       *child);
static void     widget_overlay_remove        (GtkContainer    *container,
					      GtkWidget       *widget);
static void     widget_overlay_forall        (GtkContainer    *container,
					      gboolean         include_internals,
					      GtkCallback      callback,
					      gpointer         callback_data);
static void     widget_overlay_show (GtkWidget *widget);
static void     widget_overlay_dispose       (GObject *obj);
static void     widget_overlay_finalize      (GObject *obj);

static gboolean widget_overlay_event         (GtkWidget *widget, GdkEvent *event);

static void widget_overlay_set_property (GObject *object,
                                         guint param_id,
                                         const GValue *value,
                                         GParamSpec *pspec);
static void widget_overlay_get_property (GObject *object,
                                         guint param_id,
                                         GValue *value,
                                         GParamSpec *pspec);

static GdkWindow * pick_offscreen_child (GdkWindow *offscreen_window,
					 double widget_x, double widget_y,
					 WidgetOverlay *ovl);


/* properties */
enum
{
        PROP_0,
	PROP_ADD_SCALE
};

G_DEFINE_TYPE (WidgetOverlay, widget_overlay, GTK_TYPE_CONTAINER);

typedef struct  {
	WidgetOverlay *ovl;
	GtkWidget *child;
	GdkWindow *offscreen_window;
	WidgetOverlayAlign halign;
	WidgetOverlayAlign valign;
	gint x, y;
	gdouble alpha;
	gboolean ignore_events;
	gdouble scale;

	gboolean is_tooltip;
} ChildData;

struct _WidgetOverlayPrivate
{
	GList *children;
	ChildData *scale_child;
	GtkRange *scale_range;

	guint tooltip_ms;
	guint idle_timer;
};

static GObjectClass *parent_class = NULL;

/*
static gint
get_child_nb (WidgetOverlay *ovl, ChildData *cd)
{
	return g_list_index (ovl->priv->children, cd);
}
*/

static void
to_child (WidgetOverlay *ovl, GdkWindow *offscreen_window, double widget_x, double widget_y,
          double *x_out, double *y_out)
{
	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *lcd = (ChildData*) list->data;
		if (lcd->offscreen_window == offscreen_window) {
			*x_out = (widget_x - lcd->x) / lcd->scale;
			*y_out = (widget_y - lcd->y) / lcd->scale;
			return;
		}
	}

	*x_out = -1;
	*y_out = -1;
}

static void
to_parent (WidgetOverlay *ovl, GdkWindow *offscreen_window, double offscreen_x, double offscreen_y,
           double *x_out, double *y_out)
{
	GList *list;

	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *lcd = (ChildData*) list->data;
		if (lcd->offscreen_window == offscreen_window) {
			*x_out = lcd->x * lcd->scale + offscreen_x;
			*y_out = lcd->y * lcd->scale + offscreen_y;
			return;
		}
	}

	*x_out = offscreen_x;
	*y_out = offscreen_y;
}

static void
widget_overlay_class_init (WidgetOverlayClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	widget_class->realize = widget_overlay_realize;
	widget_class->unrealize = widget_overlay_unrealize;
	widget_class->get_preferred_width = widget_overlay_get_preferred_width;
	widget_class->get_preferred_height = widget_overlay_get_preferred_height;
	widget_class->size_allocate = widget_overlay_size_allocate;
	widget_class->draw = widget_overlay_draw;
	widget_class->event = widget_overlay_event;
	widget_class->show = widget_overlay_show;

	g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
					 WIDGET_OVERLAY_TYPE,
					 g_cclosure_new (G_CALLBACK (widget_overlay_damage),
							 NULL, NULL));

	container_class->add = widget_overlay_add;
	container_class->remove = widget_overlay_remove;
	container_class->forall = widget_overlay_forall;
	G_OBJECT_CLASS (klass)->dispose = widget_overlay_dispose;
	G_OBJECT_CLASS (klass)->finalize = widget_overlay_finalize;

	/* Properties */
        G_OBJECT_CLASS (klass)->set_property = widget_overlay_set_property;
        G_OBJECT_CLASS (klass)->get_property = widget_overlay_get_property;
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ADD_SCALE,
                                         g_param_spec_boolean ("add-scale", NULL, NULL,
                                                               FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
change_widget_scale (WidgetOverlay *ovl, ChildData *cd, gdouble new_scale)
{
	widget_overlay_set_child_props (ovl, cd->child, WIDGET_OVERLAY_CHILD_SCALE, new_scale, -1);
	if (ovl->priv->scale_child)
		gtk_range_set_value (ovl->priv->scale_range, new_scale);
}

static ChildData *
get_first_child (WidgetOverlay *ovl)
{
	GList *list;

	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *lcd;
		lcd = (ChildData*) list->data;
		if (lcd != ovl->priv->scale_child)
			return lcd;
	}
	return NULL;
}

static void
scale_value_changed_cb (GtkRange *range, WidgetOverlay *ovl)
{
	ChildData *cd;
	cd = get_first_child (ovl);
	if (!cd)
		return;
	change_widget_scale (ovl, cd, gtk_range_get_value (range));
}

static void
scale_button_clicked_cb (GtkButton *button, WidgetOverlay *ovl)
{
	g_object_set (G_OBJECT (ovl), "add-scale", FALSE, NULL);
}

static void
widget_overlay_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (object);
	if (ovl->priv) {
                switch (param_id) {
                case PROP_ADD_SCALE: {
			gboolean need_scale;
			need_scale = g_value_get_boolean (value);
			if (!need_scale && !ovl->priv->scale_child)
				return;

			if (need_scale && ovl->priv->scale_child) {
				widget_overlay_set_child_props (ovl, ovl->priv->scale_child->child,
								WIDGET_OVERLAY_CHILD_ALPHA, .6,
								-1);
			}
			else if (need_scale) {
				GtkWidget *box, *wid, *button, *image;
				box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				wid = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
								SCALE_MIN, SCALE_MAX, SCALE_STEP);
				ovl->priv->scale_range = GTK_RANGE (wid);
				g_object_set (G_OBJECT (wid), "draw-value", FALSE, NULL);
				gtk_box_pack_start (GTK_BOX (box), wid, TRUE, TRUE, 0);

				button = gtk_button_new ();
				image = gtk_image_new_from_icon_name ("window-close-symbolic",
								  GTK_ICON_SIZE_MENU);
				gtk_container_add (GTK_CONTAINER (button), image);
				gtk_container_add (GTK_CONTAINER (box), button);
				gtk_widget_set_name (button, "browser-tab-close-button");
				g_signal_connect (button, "clicked",
						  G_CALLBACK (scale_button_clicked_cb), ovl);

				gtk_container_add (GTK_CONTAINER (ovl), box);
				gtk_widget_show_all (box);

				GList *list;
				for (list = ovl->priv->children; list; list = list->next) {
					ChildData *cd = (ChildData*) list->data;
					if (cd->child == box) {
						ovl->priv->scale_child = cd;
						break;
					}
				}
				g_assert (ovl->priv->scale_child);

				ChildData *cd;
				cd = get_first_child (ovl);
				if (cd)
					gtk_range_set_value (ovl->priv->scale_range, cd->scale);
				gtk_range_set_inverted (ovl->priv->scale_range, TRUE);

				g_signal_connect (wid, "value-changed",
						  G_CALLBACK (scale_value_changed_cb), ovl);

				widget_overlay_set_child_props (ovl, box,
								WIDGET_OVERLAY_CHILD_VALIGN,
								WIDGET_OVERLAY_ALIGN_FILL,
								WIDGET_OVERLAY_CHILD_HALIGN,
								WIDGET_OVERLAY_ALIGN_END,
								WIDGET_OVERLAY_CHILD_SCALE, 1.,
								WIDGET_OVERLAY_CHILD_ALPHA, .6,
								-1);
			}
			else {
				widget_overlay_set_child_props (ovl, ovl->priv->scale_child->child,
								WIDGET_OVERLAY_CHILD_ALPHA, 0.,
								-1);
			}
			break;
			}
		}
	}
}

static void
widget_overlay_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (object);
	if (ovl->priv) {
                switch (param_id) {
                case PROP_ADD_SCALE: {
			gboolean has_scale = FALSE;
			if (ovl->priv->scale_child &&
			    (ovl->priv->scale_child->alpha > 0.))
				has_scale = TRUE;
			g_value_set_boolean (value, has_scale);
			break;
		}
		}
	}
}

static gboolean
idle_timer_cb (WidgetOverlay *ovl)
{
	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd;
		cd = (ChildData*) list->data;
		if (cd->is_tooltip) {
			gtk_widget_show (cd->child);
			gtk_widget_queue_draw (GTK_WIDGET (ovl));
			/*
			widget_overlay_set_child_props (ovl, cd->child,
							WIDGET_OVERLAY_CHILD_ALPHA, 1.,
							WIDGET_OVERLAY_CHILD_HAS_EVENTS, TRUE,
							-1);
			*/
		}
	}
	ovl->priv->idle_timer = 0;
	return FALSE; /* remove timer */
}

static gboolean
widget_overlay_event (GtkWidget *widget, GdkEvent *event)
{
	GdkEventScroll *ev = (GdkEventScroll *) event;
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);
#ifdef GDA_DEBUG_NO
	ChildData *cdevent = NULL;

	/* find child */
	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd;
		cd = (ChildData*) list->data;
		if (cd->offscreen_window == ((GdkEventAny*)event)->window) {
			cdevent = cd;
			break;
		}
	}
	g_print (" CH%d/%d", g_list_index (ovl->priv->children, cdevent), event->type);
#endif

	/* tooltip widgets handling */
	gboolean tooltip_event = FALSE;
	if ((event->type == GDK_BUTTON_PRESS) ||
	    (event->type == GDK_2BUTTON_PRESS) ||
	    (event->type == GDK_3BUTTON_PRESS) ||
	    (event->type == GDK_BUTTON_RELEASE) ||
	    (event->type == GDK_MOTION_NOTIFY) ||
	    (event->type == GDK_KEY_PRESS) ||
	    (event->type == GDK_KEY_RELEASE) ||
	    (event->type == GDK_ENTER_NOTIFY) ||
	    (event->type == GDK_LEAVE_NOTIFY) ||
	    (event->type == GDK_SCROLL))
		tooltip_event = TRUE;

	if (tooltip_event) {
		/*g_print (".");*/
		gboolean need_tooltip = FALSE;
		GList *list;
		for (list = ovl->priv->children; list; list = list->next) {
			ChildData *cd;
			cd = (ChildData*) list->data;
			if (cd->is_tooltip) {
				need_tooltip = TRUE;
				break;
			}
		}
		
		if (ovl->priv->idle_timer) {
			g_source_remove (ovl->priv->idle_timer);
			ovl->priv->idle_timer = 0;
		}

		if ((event->type != GDK_ENTER_NOTIFY) &&
		    (event->type != GDK_LEAVE_NOTIFY)) {
			gboolean inc_timeout = FALSE;
			for (list = ovl->priv->children; list; list = list->next) {
				ChildData *cd;
				cd = (ChildData*) list->data;
				if (cd->is_tooltip && gtk_widget_get_visible (cd->child)) {
					inc_timeout = TRUE;
					gtk_widget_hide (cd->child);
				}
			}
			if (inc_timeout) {
				GtkSettings *settings;
				guint t;
				settings = gtk_widget_get_settings (widget);
				g_object_get (settings, "gtk-tooltip-timeout",
					      &t, NULL);
				if ((ovl->priv->tooltip_ms * 2) < (t * 32))
					ovl->priv->tooltip_ms = ovl->priv->tooltip_ms * 2;
			}
		}
					
		if ((event->type == GDK_LEAVE_NOTIFY) &&
		    ((GdkEventAny*)event)->window == gtk_widget_get_window (GTK_WIDGET (ovl)))
			need_tooltip = FALSE;

		if (need_tooltip) {
			if (ovl->priv->tooltip_ms == 0) {
				GtkSettings *settings;
				settings = gtk_widget_get_settings (widget);
				g_object_get (settings, "gtk-tooltip-timeout",
					      &(ovl->priv->tooltip_ms), NULL);
			}
			ovl->priv->idle_timer = g_timeout_add (ovl->priv->tooltip_ms,
							       (GSourceFunc) idle_timer_cb, ovl);
		}
	}

	if ((event->type != GDK_SCROLL) || !(ev->state & GDK_SHIFT_MASK))
		return FALSE;

	ChildData *cd = NULL;
	cd = get_first_child (ovl);
	if (!cd)
		return FALSE;

	gdouble scale;
	scale = cd->scale;
	if (ev->direction == GDK_SCROLL_UP)
		scale += SCALE_STEP;
	else
		scale -= SCALE_STEP;
	change_widget_scale (ovl, cd, scale);

	return TRUE;
}

static void
widget_overlay_init (WidgetOverlay *ovl)
{
	gtk_widget_set_has_window (GTK_WIDGET (ovl), TRUE);
	ovl->priv = g_new0 (WidgetOverlayPrivate, 1);
	ovl->priv->children = NULL;
	ovl->priv->scale_child = NULL;
	ovl->priv->tooltip_ms = 0;
}

GtkWidget *
widget_overlay_new (void)
{
	return g_object_new (WIDGET_OVERLAY_TYPE, NULL);
}

static void
widget_overlay_dispose (GObject *obj)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (obj);
	if (ovl->priv) {
		if (ovl->priv->idle_timer) {
			g_source_remove (ovl->priv->idle_timer);
			ovl->priv->idle_timer = 0;
		}
	}
	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
widget_overlay_finalize (GObject *obj)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (obj);
	if (ovl->priv->children) {
		GList *list;
		for (list = ovl->priv->children; list; list = list->next) {
			ChildData *cd = (ChildData*) list->data;
			g_free (cd);
		}
		g_list_free (ovl->priv->children);
	}
	g_free (ovl->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
widget_overlay_show (GtkWidget *widget)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);
	GList *list;

	((GtkWidgetClass *)parent_class)->show (widget);
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd;
		cd = (ChildData*) list->data;
		if (cd->is_tooltip)
			gtk_widget_hide (cd->child);
	}
}

static GdkWindow *
pick_offscreen_child (G_GNUC_UNUSED GdkWindow *offscreen_window,
                      double widget_x, double widget_y,
                      WidgetOverlay *ovl)
{
	GList *list;
	GdkWindow *retval = NULL;
	gboolean ign_non_tooltip = FALSE;

	//return ((ChildData*)ovl->priv->children)->offscreen_window;

	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		if (cd->is_tooltip && gtk_widget_get_visible (cd->child)) {
			ign_non_tooltip = TRUE;
			break;
		}
	}

	for (list = g_list_last (ovl->priv->children); list; list = list->prev) {
		ChildData *cd = (ChildData*) list->data;
		if (!cd->ignore_events && gtk_widget_get_visible (cd->child) && (cd->alpha > 0.) &&
		    (!ign_non_tooltip || (ign_non_tooltip && cd->is_tooltip))) {
			GtkAllocation alloc;
			double x, y;
			to_child (ovl, cd->offscreen_window, widget_x, widget_y, &x, &y);
			gtk_widget_get_allocation ((GtkWidget*) cd->child, &alloc);
			if (x >= 0 && x < alloc.width &&
			    y >= 0 && y < alloc.height) {
				retval = cd->offscreen_window;
				break;
			}
		}
	}

	/*g_print ("%s() => %p\n", __FUNCTION__, retval);*/
	return retval;
}

static void
offscreen_window_to_parent (GdkWindow *offscreen_window,
                            double offscreen_x, double offscreen_y,
                            double *parent_x, double *parent_y,
                            WidgetOverlay *ovl)
{
	to_parent (ovl, offscreen_window, offscreen_x, offscreen_y, parent_x, parent_y);
}

static void
offscreen_window_from_parent (GdkWindow *offscreen_window,
                              double parent_x, double parent_y,
                              double *offscreen_x, double *offscreen_y,
                              WidgetOverlay *ovl)
{
	to_child (ovl, offscreen_window, parent_x, parent_y, offscreen_x, offscreen_y);
}

static void
widget_overlay_realize (GtkWidget *widget)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);
	GdkWindowAttr attributes;
	gint attributes_mask;
	gint border_width;
	GtkRequisition child_requisition;
	GtkAllocation allocation;

	gtk_widget_set_realized (widget, TRUE);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	gtk_widget_get_allocation (widget, &allocation);
	attributes.x = allocation.x + border_width;
	attributes.y = allocation.y + border_width;
	attributes.width = allocation.width - 2 * border_width;
	attributes.height = allocation.height - 2 * border_width;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.event_mask = gtk_widget_get_events (widget)
		| GDK_EXPOSURE_MASK
		| GDK_POINTER_MOTION_MASK
		| GDK_BUTTON_PRESS_MASK
		| GDK_BUTTON_RELEASE_MASK
		| GDK_SCROLL_MASK
		| GDK_ENTER_NOTIFY_MASK
		| GDK_LEAVE_NOTIFY_MASK;

	attributes.visual = gtk_widget_get_visual (widget);
	attributes.wclass = GDK_INPUT_OUTPUT;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	GdkWindow *win;
	win = gdk_window_new (gtk_widget_get_parent_window (widget),
			      &attributes, attributes_mask);
	gtk_widget_set_window (widget, win);
	gdk_window_set_user_data (win, widget);
	g_signal_connect (win, "pick-embedded-child",
			  G_CALLBACK (pick_offscreen_child), ovl);

	/* offscreen windows */
	attributes.window_type = GDK_WINDOW_OFFSCREEN;

	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		child_requisition.width = child_requisition.height = 0;
		if (gtk_widget_get_visible (cd->child)) {
			GtkAllocation allocation;
			gtk_widget_get_allocation (cd->child, &allocation);
			attributes.width = allocation.width;
			attributes.height = allocation.height;
		}

		cd->offscreen_window = gdk_window_new (NULL, &attributes, attributes_mask);
		gdk_window_set_user_data (cd->offscreen_window, widget);
		gtk_widget_set_parent_window (cd->child, cd->offscreen_window);
		gdk_offscreen_window_set_embedder (cd->offscreen_window, win);
		g_signal_connect (cd->offscreen_window, "to-embedder",
				  G_CALLBACK (offscreen_window_to_parent), ovl);
		g_signal_connect (cd->offscreen_window, "from-embedder",
				  G_CALLBACK (offscreen_window_from_parent), ovl);

		gdk_window_show (cd->offscreen_window);
	}
}

static void
widget_overlay_unrealize (GtkWidget *widget)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);

	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		gdk_window_set_user_data (cd->offscreen_window, NULL);
		gdk_window_destroy (cd->offscreen_window);
		cd->offscreen_window = NULL;
	}

	GTK_WIDGET_CLASS (widget_overlay_parent_class)->unrealize (widget);
}

static void
widget_overlay_add (GtkContainer *container, GtkWidget *widget)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (container);
	ChildData *cd;

	cd = g_new0 (ChildData, 1);
	gtk_widget_set_parent (widget, GTK_WIDGET (ovl));
	cd->ovl = ovl;
	cd->child = widget;
	cd->halign = WIDGET_OVERLAY_ALIGN_CENTER;
	cd->valign = WIDGET_OVERLAY_ALIGN_END;
	cd->alpha = 1.;
	cd->scale = 1.;
	cd->ignore_events = FALSE;
	cd->is_tooltip = FALSE;
	
	ovl->priv->children = g_list_append (ovl->priv->children, cd);

	if (ovl->priv->scale_child) {
		ChildData *fcd;
		fcd = get_first_child (ovl);
		if (cd == fcd)
			gtk_range_set_value (ovl->priv->scale_range, cd->scale);

		ovl->priv->children = g_list_remove (ovl->priv->children, ovl->priv->scale_child);
		ovl->priv->children = g_list_append (ovl->priv->children, ovl->priv->scale_child);
	}
}

static void
widget_overlay_remove (GtkContainer *container, GtkWidget *widget)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (container);
	gboolean was_visible;

	was_visible = gtk_widget_get_visible (widget);
	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		if (cd->child == widget) {
			gtk_widget_unparent (widget);
			ovl->priv->children = g_list_remove (ovl->priv->children, cd);
			g_free (cd);
			if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
				gtk_widget_queue_resize (GTK_WIDGET (container));
			break;
		}
	}
}

static void
widget_overlay_forall (GtkContainer *container,
		       G_GNUC_UNUSED gboolean include_internals,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (container);

	g_return_if_fail (callback != NULL);
	GList *list, *copy;
	copy = g_list_copy (ovl->priv->children);
	for (list = copy; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		(*callback) (cd->child, callback_data);
	}
	g_list_free (copy);
}

static void
widget_overlay_size_request (GtkWidget *widget, GtkRequisition *req_min, GtkRequisition *req_nat)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);
	gint border_width;
	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	req_min->width = 1;
	req_min->height = 1;
	req_nat->width = 1;
	req_nat->height = 1;

	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		if (gtk_widget_get_visible (cd->child) && !cd->is_tooltip) {
			GtkRequisition child_req_min, child_req_nat;
			gtk_widget_get_preferred_size (cd->child, &child_req_min, &child_req_nat);
			if (cd->halign == WIDGET_OVERLAY_ALIGN_FILL) {
				req_min->width = MAX (border_width * 2 + child_req_min.width,
						      req_min->width);
				req_nat->width = MAX (border_width * 2 + child_req_nat.width,
						      req_nat->width);
			}
			else {
				req_min->width = MAX (border_width * 2 + child_req_min.width * cd->scale,
						      req_min->width);
				req_nat->width = MAX (border_width * 2 + child_req_nat.width * cd->scale,
						      req_nat->width);
			}
			
			if (cd->valign == WIDGET_OVERLAY_ALIGN_FILL) {
				req_min->height = MAX (border_width * 2 + child_req_min.height,
						       req_min->height);
				
				req_nat->height = MAX (border_width * 2 + child_req_nat.height,
						       req_nat->height);
			}
			else {
				req_min->height = MAX (border_width * 2 + child_req_min.height * cd->scale,
						       req_min->height);
				
				req_nat->height = MAX (border_width * 2 + child_req_nat.height * cd->scale,
						       req_nat->height);
			}
		}
	}
}

static void
widget_overlay_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural)
{
	GtkRequisition req_min, req_nat;
	widget_overlay_size_request (widget, &req_min, &req_nat);
	*minimum = req_min.width;
	*natural = req_nat.width;
}

static void
widget_overlay_get_preferred_height (GtkWidget *widget, gint *minimum, gint *natural)
{
	GtkRequisition req_min, req_nat;
	widget_overlay_size_request (widget, &req_min, &req_nat);
	*minimum = req_min.height;
	*natural = req_nat.height;
}

static void
widget_overlay_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);
	gint border_width;
	gint w, h;

	gtk_widget_set_allocation (widget, allocation);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	w = allocation->width - border_width * 2;
	h = allocation->height - border_width * 2;

	if (gtk_widget_get_realized (widget)) {
		GdkWindow *win;
		win = gtk_widget_get_window (widget);
		gdk_window_move_resize (win,
					allocation->x + border_width,
					allocation->y + border_width,
					w, h);
	}

	GList *list;
	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		if (gtk_widget_get_visible (cd->child)){
			GtkRequisition child_requisition;
			GtkAllocation child_allocation;
			
			gtk_widget_get_preferred_size (cd->child, &child_requisition, NULL);
			child_allocation.x = 0;
			child_allocation.y = 0;
			child_allocation.height = child_requisition.height;
			child_allocation.width = child_requisition.width;
			
			if (cd->halign == WIDGET_OVERLAY_ALIGN_FILL)
				child_allocation.width = w / cd->scale;
			if ((cd->valign == WIDGET_OVERLAY_ALIGN_FILL) ||
			    (cd == ovl->priv->scale_child))
				child_allocation.height = h / cd->scale;

			if (cd->is_tooltip) {
				cd->scale = 1.;
				if ((allocation->width > 0) && (allocation->height > 0)) {
					if (child_allocation.width > allocation->width)
						cd->scale = (gdouble) allocation->width /
							(gdouble) child_allocation.width;
					if (child_allocation.height > allocation->height) {
						if (cd->scale > ((gdouble) allocation->height /
								 (gdouble) child_allocation.height))
							cd->scale = (gdouble) allocation->height /
								(gdouble) child_allocation.height;
					}
				}
			}

			if (gtk_widget_get_realized (widget))
				gdk_window_move_resize (cd->offscreen_window,
							child_allocation.x,
							child_allocation.y,
							child_allocation.width,
							child_allocation.height);
			
			child_allocation.x = child_allocation.y = 0;

			gtk_widget_size_allocate (cd->child, &child_allocation);
		}
	}
}

static gboolean
widget_overlay_damage (GtkWidget      *widget,
		       G_GNUC_UNUSED GdkEventExpose *event)
{
	gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

	return TRUE;
}

static gboolean
widget_overlay_draw (GtkWidget *widget, cairo_t *cr)
{
	WidgetOverlay *ovl = WIDGET_OVERLAY (widget);
	GdkWindow *window;

	GtkAllocation area;
	gtk_widget_get_allocation (widget, &area);

	window = gtk_widget_get_window (widget);
	if (gtk_cairo_should_draw_window (cr, window)) {
		GList *list;
		for (list = ovl->priv->children; list; list = list->next) {
			ChildData *cd = (ChildData*) list->data;
			if (gtk_widget_get_visible (cd->child)) {
				cairo_surface_t *surface;
				GtkAllocation child_area;
				double x, y;
				x = y = 0.0;
				gtk_widget_get_allocation (cd->child, &child_area);
				child_area.width *= cd->scale;
				child_area.height *= cd->scale;
				switch (cd->halign) {
				case WIDGET_OVERLAY_ALIGN_FILL:
				case WIDGET_OVERLAY_ALIGN_START:
					x = 0.0;
					break;
				case WIDGET_OVERLAY_ALIGN_END:
					x = area.width - child_area.width;
					break;
				case WIDGET_OVERLAY_ALIGN_CENTER:
					x = (area.width - child_area.width) / 2.;
					break;
				}
				switch (cd->valign) {
				case WIDGET_OVERLAY_ALIGN_FILL:
				case WIDGET_OVERLAY_ALIGN_START:
					y = 0.0;
					break;
				case WIDGET_OVERLAY_ALIGN_END:
					y = area.height - child_area.height;
					break;
				case WIDGET_OVERLAY_ALIGN_CENTER:
					y = (area.height - child_area.height) / 2.;
					break;
				}
				
				surface = gdk_offscreen_window_get_surface (cd->offscreen_window);
				if (cd->scale == 1.) {
					cairo_set_source_surface (cr, surface, x, y);
					cairo_paint_with_alpha (cr, cd->alpha);
				}
				else {
					cairo_save (cr);
					cairo_scale (cr, cd->scale, cd->scale);
					cairo_set_source_surface (cr, surface, x/cd->scale, y/cd->scale);
					cairo_paint_with_alpha (cr, cd->alpha);
					cairo_restore (cr);
				}

				cd->x = x;
				cd->y = y;
			}

			if (list->next &&
			    ((ChildData*) list->next->data == ovl->priv->scale_child) &&
			    (ovl->priv->scale_child->alpha > 0.)) {
				cairo_set_source_rgba (cr, 0., 0., 0., .3);
				cairo_rectangle (cr, 0, 0,
						 area.width, area.height);
				cairo_fill (cr);
			}
		}
	}
	else {
		GList *list;
		for (list = ovl->priv->children; list; list = list->next) {
			ChildData *cd = (ChildData*) list->data;
			if (gtk_cairo_should_draw_window (cr, cd->offscreen_window))
				gtk_container_propagate_draw (GTK_CONTAINER (widget), cd->child, cr);
		}
	}

	return TRUE;
}

/**
 * widget_overlay_set_child_props:
 *
 * @...: (WidgetOverlayChildProperty, value) terminated by -1
 */
void
widget_overlay_set_child_props (WidgetOverlay *ovl, GtkWidget *child, ...)
{
	GList *list;
	g_return_if_fail (IS_WIDGET_OVERLAY (ovl));

	for (list = ovl->priv->children; list; list = list->next) {
		ChildData *cd = (ChildData*) list->data;
		if (cd->child == child) {
			va_list args;
			WidgetOverlayChildProperty prop;
			GtkAllocation area;
			gtk_widget_get_allocation (GTK_WIDGET (ovl), &area);
			if (gtk_widget_get_has_window (GTK_WIDGET (ovl)))
				area.x = area.y = 0;

			va_start (args, child);
			for (prop = va_arg (args, WidgetOverlayChildProperty); (gint) prop != -1;
			     prop = va_arg (args, WidgetOverlayChildProperty)) {
				switch (prop) {
				case WIDGET_OVERLAY_CHILD_VALIGN:
					cd->valign = va_arg (args, WidgetOverlayAlign);
					break;
				case WIDGET_OVERLAY_CHILD_HALIGN:
					cd->halign = va_arg (args, WidgetOverlayAlign);
					break;
				case WIDGET_OVERLAY_CHILD_ALPHA:
					cd->alpha = va_arg (args, gdouble);
					if (cd->alpha > 1.)
						cd->alpha = 1;
					else if (cd->alpha < 0.)
						cd->alpha = 0.;

					if (cd != ovl->priv->scale_child) {
						gtk_widget_get_allocation (cd->child, &area);
						area.x = cd->x;
						area.y = cd->y;
					}
					break;
				case WIDGET_OVERLAY_CHILD_HAS_EVENTS:
					cd->ignore_events = ! (va_arg (args, gboolean));
					break;
				case WIDGET_OVERLAY_CHILD_SCALE: {
					GtkAllocation alloc;
					gdouble current, max;
					current = cd->scale;
					gtk_widget_get_allocation (cd->child, &alloc);

					cd->scale = va_arg (args, gdouble);
					if (cd->scale < SCALE_MIN)
						cd->scale = SCALE_MIN;
					max = SCALE_MAX;
					if (cd == ovl->priv->scale_child)
						max = SCALE_MAX_SCALE;
					if (cd->scale > max)
						cd->scale = max;

					if (current >= cd->scale) {
						area.x = cd->x;
						area.y = cd->y;
						area.width = alloc.width;
						area.height = alloc.height;
					}
					if ((cd->halign == WIDGET_OVERLAY_ALIGN_FILL) ||
					    (cd->valign == WIDGET_OVERLAY_ALIGN_FILL)) {
						/*gtk_widget_queue_resize (GTK_WIDGET (ovl));*/
						gtk_widget_queue_resize (cd->child);
					}

					if (ovl->priv->scale_range && (cd == get_first_child (ovl)))
						gtk_range_set_value (ovl->priv->scale_range, cd->scale);
					break;
				}
				case WIDGET_OVERLAY_CHILD_TOOLTIP:
					cd->is_tooltip = va_arg (args, gboolean);
					break;
				default:
					g_assert_not_reached ();
					break;
				}
			}
			va_end (args);
			/*g_print ("Queue draw %dx%d %dx%d\n", area.x, area.y, area.width, area.height);*/
			gtk_widget_queue_draw_area (GTK_WIDGET (ovl), area.x, area.y, area.width, area.height);

			return;
		}
	}
}
