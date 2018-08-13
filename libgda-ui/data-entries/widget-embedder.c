/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#include "widget-embedder.h"
#include <gdaui-decl.h>

static void     widget_embedder_realize       (GtkWidget       *widget);
static void     widget_embedder_unrealize     (GtkWidget       *widget);
static void     widget_embedder_get_preferred_width  (GtkWidget *widget,
						      gint      *minimum,
						      gint      *natural);
static void     widget_embedder_get_preferred_height (GtkWidget *widget,
						      gint      *minimum,
						      gint      *natural);
static void     widget_embedder_size_allocate (GtkWidget       *widget,
                                               GtkAllocation   *allocation);
static gboolean widget_embedder_damage        (GtkWidget       *widget,
                                               GdkEventExpose  *event);
static gboolean widget_embedder_draw          (GtkWidget       *widget,
					       cairo_t         *cr);
static void     widget_embedder_add           (GtkContainer    *container,
                                               GtkWidget       *child);
static void     widget_embedder_remove        (GtkContainer    *container,
                                               GtkWidget       *widget);
static void     widget_embedder_forall        (GtkContainer    *container,
                                               gboolean         include_internals,
                                               GtkCallback      callback,
                                               gpointer         callback_data);
static GType    widget_embedder_child_type    (GtkContainer    *container);

G_DEFINE_TYPE (WidgetEmbedder, widget_embedder, GTK_TYPE_CONTAINER);

static void
to_child (G_GNUC_UNUSED WidgetEmbedder *bin,
          double         widget_x,
          double         widget_y,
          double        *x_out,
          double        *y_out)
{
	*x_out = widget_x;
	*y_out = widget_y;
}

static void
to_parent (G_GNUC_UNUSED WidgetEmbedder *bin,
           double         offscreen_x,
           double         offscreen_y,
           double        *x_out,
           double        *y_out)
{
	*x_out = offscreen_x;
	*y_out = offscreen_y;
}

static void
widget_embedder_class_init (WidgetEmbedderClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	widget_class->realize = widget_embedder_realize;
	widget_class->unrealize = widget_embedder_unrealize;
	widget_class->get_preferred_width = widget_embedder_get_preferred_width;
	widget_class->get_preferred_height = widget_embedder_get_preferred_height;
	widget_class->size_allocate = widget_embedder_size_allocate;
	widget_class->draw = widget_embedder_draw;

	g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
					 WIDGET_EMBEDDER_TYPE,
					 g_cclosure_new (G_CALLBACK (widget_embedder_damage),
							 NULL, NULL));

	container_class->add = widget_embedder_add;
	container_class->remove = widget_embedder_remove;
	container_class->forall = widget_embedder_forall;
	container_class->child_type = widget_embedder_child_type;
}

static void
widget_embedder_init (WidgetEmbedder *bin)
{
	gtk_widget_set_has_window (GTK_WIDGET (bin), TRUE);
	bin->valid = TRUE;
	bin->red = -1.;
	bin->green = -1.;
	bin->blue = -1.;
	bin->alpha = -1.;
}

GtkWidget *
widget_embedder_new (void)
{
	return g_object_new (WIDGET_EMBEDDER_TYPE, NULL);
}

static GdkWindow *
pick_offscreen_child (G_GNUC_UNUSED GdkWindow     *offscreen_window,
                      double         widget_x,
                      double         widget_y,
                      WidgetEmbedder *bin)
{
	GtkAllocation child_area;
	double x, y;

	if (bin->child && gtk_widget_get_visible (bin->child)) {
		to_child (bin, widget_x, widget_y, &x, &y);
		
		gtk_widget_get_allocation ((GtkWidget*) bin, &child_area);
		if (x >= 0 && x < child_area.width &&
		    y >= 0 && y < child_area.height)
			return bin->offscreen_window;
	}

	return NULL;
}

static void
offscreen_window_to_parent (G_GNUC_UNUSED GdkWindow     *offscreen_window,
                            double         offscreen_x,
                            double         offscreen_y,
                            double        *parent_x,
                            double        *parent_y,
                            WidgetEmbedder *bin)
{
	to_parent (bin, offscreen_x, offscreen_y, parent_x, parent_y);
}

static void
offscreen_window_from_parent (G_GNUC_UNUSED GdkWindow     *window,
                              double         parent_x,
                              double         parent_y,
                              double        *offscreen_x,
                              double        *offscreen_y,
                              WidgetEmbedder *bin)
{
	to_child (bin, parent_x, parent_y, offscreen_x, offscreen_y);
}

static void
widget_embedder_realize (GtkWidget *widget)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (widget);
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
			  G_CALLBACK (pick_offscreen_child), bin);

	attributes.window_type = GDK_WINDOW_OFFSCREEN;

	child_requisition.width = child_requisition.height = 0;
	if (bin->child && gtk_widget_get_visible (bin->child)) {
		GtkAllocation allocation;
		gtk_widget_get_allocation (bin->child, &allocation);
		attributes.width = allocation.width;
		attributes.height = allocation.height;
	}
	bin->offscreen_window = gdk_window_new (gdk_screen_get_root_window (gtk_widget_get_screen (widget)),
						&attributes, attributes_mask);
	gdk_window_set_user_data (bin->offscreen_window, widget);
	if (bin->child)
		gtk_widget_set_parent_window (bin->child, bin->offscreen_window);
	gdk_offscreen_window_set_embedder (bin->offscreen_window, win);
	g_signal_connect (bin->offscreen_window, "to-embedder",
			  G_CALLBACK (offscreen_window_to_parent), bin);
	g_signal_connect (bin->offscreen_window, "from-embedder",
			  G_CALLBACK (offscreen_window_from_parent), bin);

	gdk_window_show (bin->offscreen_window);
}

static void
widget_embedder_unrealize (GtkWidget *widget)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (widget);

	gdk_window_set_user_data (bin->offscreen_window, NULL);
	gdk_window_destroy (bin->offscreen_window);
	bin->offscreen_window = NULL;

	GTK_WIDGET_CLASS (widget_embedder_parent_class)->unrealize (widget);
}

static GType
widget_embedder_child_type (GtkContainer *container)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (container);

	if (bin->child)
		return G_TYPE_NONE;

	return GTK_TYPE_WIDGET;
}

static void
widget_embedder_add (GtkContainer *container,
                     GtkWidget    *widget)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (container);

	if (!bin->child) {
		gtk_widget_set_parent_window (widget, bin->offscreen_window);
		gtk_widget_set_parent (widget, GTK_WIDGET (bin));
		bin->child = widget;
	}
	else
		g_warning ("WidgetEmbedder cannot have more than one child\n");
}

static void
widget_embedder_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (container);
	gboolean was_visible;

	was_visible = gtk_widget_get_visible (widget);

	if (bin->child == widget) {
		gtk_widget_unparent (widget);
		
		bin->child = NULL;
		
		if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
			gtk_widget_queue_resize (GTK_WIDGET (container));
	}
}

static void
widget_embedder_forall (GtkContainer *container,
                        G_GNUC_UNUSED gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (container);

	g_return_if_fail (callback != NULL);

	if (bin->child)
		(*callback) (bin->child, callback_data);
}

static void
widget_embedder_size_request (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (widget);
	GtkRequisition child_requisition;

	child_requisition.width = 0;
	child_requisition.height = 0;

	if (bin->child && gtk_widget_get_visible (bin->child))
		gtk_widget_get_preferred_size (bin->child, &child_requisition, NULL);

	guint border_width;
	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	requisition->width = border_width * 2 + child_requisition.width;
	requisition->height = border_width * 2 + child_requisition.height;
}

static void
widget_embedder_get_preferred_width (GtkWidget *widget,
				     gint      *minimum,
				     gint      *natural)
{
	GtkRequisition requisition;
	widget_embedder_size_request (widget, &requisition);
	*minimum = *natural = requisition.width;
}

static void
widget_embedder_get_preferred_height (GtkWidget *widget,
				      gint      *minimum,
				      gint      *natural)
{
	GtkRequisition requisition;
	widget_embedder_size_request (widget, &requisition);
	*minimum = *natural = requisition.height;
}

static void
widget_embedder_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (widget);
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

	if (bin->child && gtk_widget_get_visible (bin->child)){
		GtkAllocation child_allocation;
		
		child_allocation.x = 0;
		child_allocation.y = 0;
		child_allocation.height = h;
		child_allocation.width = w;
		
		if (gtk_widget_get_realized (widget))
			gdk_window_move_resize (bin->offscreen_window,
						child_allocation.x,
						child_allocation.y,
						child_allocation.width,
						child_allocation.height);
		
		child_allocation.x = child_allocation.y = 0;
		gtk_widget_size_allocate (bin->child, &child_allocation);
	}
}

static gboolean
widget_embedder_damage (GtkWidget      *widget,
                        G_GNUC_UNUSED GdkEventExpose *event)
{
	gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

	return TRUE;
}

void
widget_embedder_set_ucolor (WidgetEmbedder *bin, gdouble red, gdouble green,
			    gdouble blue, gdouble alpha)
{
	bin->red = red;
	bin->green = green;
	bin->blue = blue;
	bin->alpha = alpha;
	gtk_widget_queue_draw (GTK_WIDGET (bin));
}


static gboolean
widget_embedder_draw (GtkWidget *widget, cairo_t *cr)
{
	WidgetEmbedder *bin = WIDGET_EMBEDDER (widget);
#define MARGIN 1.5
	GdkWindow *window;

	window = gtk_widget_get_window (widget);
	if (gtk_cairo_should_draw_window (cr, window)) {
		cairo_surface_t *surface;
		GtkAllocation child_area;
		
		if (bin->child && gtk_widget_get_visible (bin->child)) {
			surface = gdk_offscreen_window_get_surface (bin->offscreen_window);
			gtk_widget_get_allocation (bin->child, &child_area);
			cairo_set_source_surface (cr, surface, 0, 0);
			cairo_paint (cr);

			if (! bin->valid) {
				if ((bin->red >= 0.) && (bin->red <= 1.) &&
				    (bin->green >= 0.) && (bin->green <= 1.) &&
				    (bin->blue >= 0.) && (bin->blue <= 1.) &&
				    (bin->alpha >= 0.) && (bin->alpha <= 1.))
					cairo_set_source_rgba (cr, bin->red, bin->green,
							       bin->blue, bin->alpha);
				else
					cairo_set_source_rgba (cr, GDAUI_COLOR_UNKNOWN_MASK);
				cairo_rectangle (cr, child_area.x + MARGIN, child_area.y + MARGIN,
						 child_area.width - 2. * MARGIN,
						 child_area.height - 2. * MARGIN);
				cairo_fill (cr);
			}
		}
	}

	if (gtk_cairo_should_draw_window (cr, bin->offscreen_window)) {
		if (bin->child)
			gtk_container_propagate_draw (GTK_CONTAINER (widget),
						      bin->child,
						      cr);
	}

	return FALSE;
}

/**
 * widget_embedder_set_valid
 * @bin: a #WidgetEmbedder
 * @valid: set to %TRUE for a valid entry
 *
 * Changes the validity aspect of @bin
 */
void
widget_embedder_set_valid (WidgetEmbedder *bin, gboolean valid)
{
	bin->valid = valid;
	gtk_widget_queue_draw (GTK_WIDGET (bin));
}
