/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2013 Haïkel Guémar <hguemar@fedoraproject.org>
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

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "browser-canvas.h"
#include "browser-canvas-priv.h"
#include "browser-canvas-item.h"
#include "browser-canvas-print.h"
#include <libgda/libgda.h>
#include <libgda-ui/libgda-ui.h>

#define DEFAULT_SCALE .8
#ifdef HAVE_GRAPHVIZ
#include <stddef.h>
#include <gvc.h>
#ifndef ND_coord_i
    #define ND_coord_i ND_coord
#endif
static GVC_t* gvc = NULL;
#endif
#include <cairo.h>
#include <cairo-svg.h>
#include <math.h>

static void browser_canvas_class_init (BrowserCanvasClass *klass);
static void browser_canvas_init       (BrowserCanvas *canvas);
static void browser_canvas_dispose    (GObject *object);
static void browser_canvas_finalize   (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

enum
{
	ITEM_SELECTED,
	LAST_SIGNAL
};

static gint canvas_signals[LAST_SIGNAL] = { 0 };

GType
browser_canvas_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvas),
			0,
			(GInstanceInitFunc) browser_canvas_init,
			0
		};		

		type = g_type_register_static (GTK_TYPE_SCROLLED_WINDOW, "BrowserCanvas", &info, 0);
	}
	return type;
}

static void
browser_canvas_class_init (BrowserCanvasClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	canvas_signals[ITEM_SELECTED] =
		g_signal_new ("item-selected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (BrowserCanvasClass, item_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      TYPE_BROWSER_CANVAS_ITEM);

	/* virtual functions */
	klass->clean_canvas_items = NULL;
	klass->build_context_menu = NULL;

	object_class->dispose = browser_canvas_dispose;
	object_class->finalize = browser_canvas_finalize;
}

static gboolean canvas_event_cb (BrowserCanvas *canvas, GdkEvent *event, GooCanvas *gcanvas);
static gboolean motion_notify_event_cb (BrowserCanvas *canvas, GdkEvent *event, GooCanvas *gcanvas);
static gboolean canvas_scroll_event_cb (GooCanvas *gcanvas, GdkEvent *event, BrowserCanvas *canvas);
static void drag_begin_cb (BrowserCanvas *canvas, GdkDragContext *drag_context, GooCanvas *gcanvas);
static void drag_data_get_cb (BrowserCanvas *canvas, GdkDragContext   *drag_context,
			      GtkSelectionData *data, guint info,
			      guint time, GooCanvas *gcanvas);
static void drag_data_received_cb (BrowserCanvas *canvas, GdkDragContext *context,
				   gint x, gint y, GtkSelectionData *data,
				   guint info, guint time, GooCanvas *gcanvas);
static gboolean idle_add_canvas_cb (BrowserCanvas *canvas);
static void
browser_canvas_init (BrowserCanvas *canvas)
{
	canvas->priv = g_new0 (BrowserCanvasPrivate, 1);

	canvas->priv->goocanvas = GOO_CANVAS (goo_canvas_new ());
	gtk_widget_show (GTK_WIDGET (canvas->priv->goocanvas));
	g_object_set_data (G_OBJECT (canvas->priv->goocanvas), "browsercanvas", canvas);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (canvas), 
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (canvas), GTK_SHADOW_NONE);
	g_idle_add ((GSourceFunc) idle_add_canvas_cb, canvas);
	canvas->priv->items = NULL;
	canvas->priv->current_selected_item = NULL;

	canvas->xmouse = 50.;
	canvas->ymouse = 50.;
	
	g_signal_connect (canvas, "event",
			  G_CALLBACK (canvas_event_cb), canvas->priv->goocanvas);
	g_signal_connect (canvas->priv->goocanvas, "scroll-event",
			  G_CALLBACK (canvas_scroll_event_cb), canvas);
	g_signal_connect (canvas, "motion-notify-event",
			  G_CALLBACK (motion_notify_event_cb), canvas->priv->goocanvas);
	g_signal_connect (canvas, "drag-begin",
			  G_CALLBACK (drag_begin_cb), canvas->priv->goocanvas);
	g_signal_connect (canvas, "drag-data-get",
			  G_CALLBACK (drag_data_get_cb), canvas->priv->goocanvas);
	g_signal_connect (canvas, "drag-data-received",
			  G_CALLBACK (drag_data_received_cb), canvas->priv->goocanvas);
	g_object_set (G_OBJECT (canvas->priv->goocanvas),
		      "automatic-bounds", TRUE,
		      "bounds-padding", 5., 
		      "bounds-from-origin", FALSE, 
		      "anchor", GOO_CANVAS_ANCHOR_CENTER, NULL);

	/* reseting the zoom */
	goo_canvas_set_scale (canvas->priv->goocanvas, DEFAULT_SCALE);
}

static gboolean
idle_add_canvas_cb (BrowserCanvas *canvas)
{
	gtk_container_add (GTK_CONTAINER (canvas), GTK_WIDGET (canvas->priv->goocanvas));
	return FALSE;
}

static void
drag_begin_cb (BrowserCanvas *canvas, G_GNUC_UNUSED GdkDragContext *drag_context,
	       G_GNUC_UNUSED GooCanvas *gcanvas)
{
	BrowserCanvasItem *citem;

	citem = g_object_get_data (G_OBJECT (canvas), "__drag_src_item");
	if (citem) {
		/*
		gtk_drag_source_set_icon_pixbuf (GTK_WIDGET (canvas),
						 browser_get_pixbuf_icon (BROWSER_ICON_TABLE));
		*/
	}
}

static void
drag_data_get_cb (BrowserCanvas *canvas, GdkDragContext *drag_context,
		  GtkSelectionData *data, guint info,
		  guint time, G_GNUC_UNUSED GooCanvas *gcanvas)
{
	BrowserCanvasItem *citem;

	citem = g_object_get_data (G_OBJECT (canvas), "__drag_src_item");
	if (citem) {
		BrowserCanvasItemClass *iclass = BROWSER_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (citem));
		if (iclass->drag_data_get)
			iclass->drag_data_get (citem, drag_context, data, info, time);
	}
}

static void
drag_data_received_cb (G_GNUC_UNUSED BrowserCanvas *canvas, GdkDragContext *context,
		       gint x, gint y, G_GNUC_UNUSED GtkSelectionData *data,
		       G_GNUC_UNUSED guint info, guint time, GooCanvas *gcanvas)
{
	GooCanvasItem *item;
	item = goo_canvas_get_item_at (gcanvas, x, y, TRUE);
	if (item) {
		/*g_print ("Dragged into %s\n", G_OBJECT_TYPE_NAME (item));*/
		gtk_drag_finish (context, TRUE, FALSE, time);
	}
	else {
		gtk_drag_finish (context, FALSE, FALSE, time);
	}
}

static gboolean
canvas_scroll_event_cb (G_GNUC_UNUSED GooCanvas *gcanvas, GdkEvent *event, BrowserCanvas *canvas)
{
	gboolean done = TRUE;
	GdkEventScroll *ev = (GdkEventScroll *) event;

	switch (event->type) {
	case GDK_SCROLL:
		if (ev->state & GDK_SHIFT_MASK) {
			if (ev->direction == GDK_SCROLL_UP)
				browser_canvas_scale_layout (canvas, 1.05);
			else
				browser_canvas_scale_layout (canvas, .95);
		}
		else {
			if (ev->direction == GDK_SCROLL_UP)
				browser_canvas_set_zoom_factor (canvas, browser_canvas_get_zoom_factor (canvas) + .03);
			else if (ev->direction == GDK_SCROLL_DOWN)
				browser_canvas_set_zoom_factor (canvas, browser_canvas_get_zoom_factor (canvas) - .03);
		}
		done = TRUE;
		break;
	default:
		done = FALSE;
		break;
	}
	return done;
}

static GdkCursor *hand_cursor = NULL;

static gboolean
motion_notify_event_cb (BrowserCanvas *canvas, GdkEvent *event, G_GNUC_UNUSED GooCanvas *gcanvas)
{
	gboolean done = TRUE;

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		if (((GdkEventMotion*) event)->state & GDK_BUTTON1_MASK) {
			if (canvas->priv->canvas_moving) {
				GtkAdjustment *ha, *va;
				gdouble x, y;
				gdouble upper, lower, page_size;
				ha = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (canvas));
				va = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (canvas));

				upper = gtk_adjustment_get_upper (ha);
				lower = gtk_adjustment_get_lower (ha);
				page_size = gtk_adjustment_get_page_size (ha);
				x = gtk_adjustment_get_value (ha);
				x = CLAMP (x + canvas->xmouse - ((GdkEventMotion*) event)->x,
					   lower, upper - page_size);
				gtk_adjustment_set_value (ha, x);

				upper = gtk_adjustment_get_upper (va);
				lower = gtk_adjustment_get_lower (va);
				page_size = gtk_adjustment_get_page_size (va);
				y = gtk_adjustment_get_value (va);
				y = CLAMP (y + canvas->ymouse - ((GdkEventMotion*) event)->y,
					   lower, upper - page_size);
				gtk_adjustment_set_value (va, y);
			}
			else {
				canvas->xmouse = ((GdkEventMotion*) event)->x;
				canvas->ymouse = ((GdkEventMotion*) event)->y;
				canvas->priv->canvas_moving = TRUE;
				if (! hand_cursor) {
					hand_cursor = gdk_cursor_new_for_display (
                        gtk_widget_get_display (GTK_WIDGET(canvas)),
                        GDK_HAND2);
				}
				gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (canvas)),
						       hand_cursor);
			}
		}
		done = TRUE;
		break;
	default:
		done = FALSE;
		break;
	}
	return done;
}

#ifdef HAVE_GRAPHVIZ
static void popup_layout_default_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
static void popup_layout_radial_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
#endif
static void popup_zoom_in_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
static void popup_zoom_out_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
static void popup_zoom_fit_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
static void popup_export_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
static void popup_print_cb (GtkMenuItem *mitem, BrowserCanvas *canvas);
static gboolean
canvas_event_cb (BrowserCanvas *canvas, GdkEvent *event, GooCanvas *gcanvas)
{
	gboolean done = TRUE;
	GooCanvasItem *item;
	BrowserCanvasClass *class = BROWSER_CANVAS_CLASS (G_OBJECT_GET_CLASS (canvas));
	gdouble x, y;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		x = ((GdkEventButton *) event)->x;
		y = ((GdkEventButton *) event)->y;
		goo_canvas_convert_from_pixels (gcanvas, &x, &y);
		item = goo_canvas_get_item_at (gcanvas, x, y, TRUE);

		if (!item) {
			if ((((GdkEventButton *) event)->button == 3) && (class->build_context_menu)) {
				GtkWidget *menu, *mitem;
				
				canvas->xmouse = x;
				canvas->ymouse = y;

				/* extra menu items, if any */
				menu = (class->build_context_menu) (canvas);
				
				/* default menu items */
				if (!menu)
					menu = gtk_menu_new ();
				else {
					mitem = gtk_separator_menu_item_new ();
					gtk_widget_show (mitem);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				}
				mitem = gtk_menu_item_new_with_label (_("Zoom in"));
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_zoom_in_cb), canvas);
				mitem = gtk_menu_item_new_with_label (_("Zoom out"));
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_zoom_out_cb), canvas);
				mitem = gtk_menu_item_new_with_label (_("Adjusted zoom"));
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_zoom_fit_cb), canvas);

				mitem = gtk_separator_menu_item_new ();
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

#ifdef HAVE_GRAPHVIZ
				mitem = gtk_menu_item_new_with_label (_("Linear layout"));
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_layout_default_cb), canvas);

				mitem = gtk_menu_item_new_with_label (_("Radial layout"));
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_layout_radial_cb), canvas);

				mitem = gtk_separator_menu_item_new ();
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
#endif
				
				mitem = gtk_menu_item_new_with_label (_("Save as"));
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_export_cb), canvas);

				mitem = gtk_menu_item_new_with_label (_("Print"));
				gtk_widget_show (mitem);
				g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_print_cb), canvas);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

				gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
			}
		}
		done = TRUE;
		break;
	case GDK_BUTTON_RELEASE:
		if (canvas->priv->canvas_moving) {
			canvas->priv->canvas_moving = FALSE;
			gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (canvas)), NULL);
		}
		break;
	case GDK_2BUTTON_PRESS:
		x = ((GdkEventButton *) event)->x;
		y = ((GdkEventButton *) event)->y;
		goo_canvas_convert_from_pixels (gcanvas, &x, &y);
		item = goo_canvas_get_item_at (gcanvas, x, y, TRUE);
		if (item) {
			GooCanvasItem *bitem;
			for (bitem = item; bitem; bitem = goo_canvas_item_get_parent (bitem)) {
				if (IS_BROWSER_CANVAS_ITEM (bitem)) {
					gboolean allow_select;
					g_object_get (G_OBJECT (bitem), "allow-select", &allow_select, NULL);
					if (allow_select) {
						browser_canvas_item_toggle_select (canvas, BROWSER_CANVAS_ITEM (bitem));
						break;
					}
				}
			}
		}
		else
			browser_canvas_fit_zoom_factor (canvas);
		done = TRUE;
		break;
	default:
		done = FALSE;
		break;
	}
	return done;	
}

#ifdef HAVE_GRAPHVIZ
static void
popup_layout_default_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	browser_canvas_perform_auto_layout (canvas, TRUE, BROWSER_CANVAS_LAYOUT_DEFAULT);
}

static void
popup_layout_radial_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	browser_canvas_perform_auto_layout (canvas, TRUE, BROWSER_CANVAS_LAYOUT_RADIAL);
}
#endif

static void
popup_zoom_in_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	browser_canvas_set_zoom_factor (canvas, browser_canvas_get_zoom_factor (canvas) + .05);
}

static void
popup_zoom_out_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	browser_canvas_set_zoom_factor (canvas, browser_canvas_get_zoom_factor (canvas) - .05);
}

static void
popup_zoom_fit_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	browser_canvas_fit_zoom_factor (canvas);
}

static void
popup_export_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	GtkWidget *dlg;
	gint result;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (canvas));
	GtkFileFilter *filter;

#define MARGIN 5.

	if (!gtk_widget_is_toplevel (toplevel))
		toplevel = NULL;

	dlg = gtk_file_chooser_dialog_new (_("Save diagram as"), (GtkWindow*) toplevel,
					   GTK_FILE_CHOOSER_ACTION_SAVE, 
					   _("_Cancel"), GTK_RESPONSE_CANCEL,
					   _("_Save"), GTK_RESPONSE_ACCEPT,
					   NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg),
					     gdaui_get_default_path ());
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("PNG Image"));
	gtk_file_filter_add_mime_type (filter, "image/png");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dlg), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("SVG file"));
	gtk_file_filter_add_mime_type (filter, "image/svg+xml");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dlg), filter);

	result = gtk_dialog_run (GTK_DIALOG (dlg));
	if (result == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		gchar *lcfilename;
		cairo_surface_t *surface = NULL;

		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg)));
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		if (filename) {
			GooCanvasBounds bounds;
			gdouble width, height;
			gchar *error = NULL;
			enum {
				OUT_UNKNOWN,
				OUT_PNG,
				OUT_SVG
			} otype = OUT_UNKNOWN;

			goo_canvas_item_get_bounds (goo_canvas_get_root_item (canvas->priv->goocanvas), &bounds);
			width = (bounds.x2 - bounds.x1) + 2. * MARGIN;
			height = (bounds.y2 - bounds.y1) + 2. * MARGIN;
			
			lcfilename = g_ascii_strdown (filename, -1);
			if (g_str_has_suffix (lcfilename, "png")) {
				otype = OUT_PNG;
				surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
			}
			if (g_str_has_suffix (lcfilename, "svg")) {
				cairo_status_t status;
				otype = OUT_SVG;
				surface = cairo_svg_surface_create (filename, width, height);
				status = cairo_surface_status (surface);
				if (status != CAIRO_STATUS_SUCCESS) {
					error = g_strdup_printf ("<b>%s</b>:\n%s",
								 _("Failed to create SVG file"), 
								 cairo_status_to_string (status));
					cairo_surface_destroy (surface);
					surface = NULL;
				}
			}
			if (otype == OUT_UNKNOWN)
				error = g_strdup_printf ("<b>%s</b>",
							 _("File format to save to is not recognized."));
			
			if (surface) {
				cairo_t *cr;
				cairo_status_t status;

				cr = cairo_create (surface);
				cairo_set_antialias (cr, CAIRO_ANTIALIAS_GRAY);
				cairo_set_line_width (cr, goo_canvas_get_default_line_width (canvas->priv->goocanvas));
				cairo_translate (cr, MARGIN - bounds.x1, MARGIN - bounds.y1);

				goo_canvas_render (canvas->priv->goocanvas, cr, NULL, 0.8);

				cairo_show_page (cr);

				switch (otype) {
				case OUT_PNG:
					status = cairo_surface_write_to_png (surface, filename);
					if (status != CAIRO_STATUS_SUCCESS)
						error = g_strdup_printf ("<b>%s</b>:\n%s",
									 _("Failed to create PNG file"), 
									 cairo_status_to_string (status));
					break;
				default:
					break;
				}

				cairo_surface_destroy (surface);
				cairo_destroy (cr);
			}

			if (error) {
				GtkWidget *errdlg;

				errdlg = gtk_message_dialog_new ((GtkWindow*) toplevel,
								 GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, 
								 GTK_BUTTONS_CLOSE, NULL);
				gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (errdlg), error);
				g_free (error);
				gtk_dialog_run (GTK_DIALOG (errdlg));
				gtk_widget_destroy (errdlg);
			}
				
			g_free (filename);
			g_free (lcfilename);
		}
	}
	gtk_widget_destroy (dlg);
}

static void
popup_print_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvas *canvas)
{
	browser_canvas_print (canvas);
}


static void
weak_ref_lost (BrowserCanvas *canvas, BrowserCanvasItem *old_item)
{
        canvas->priv->items = g_slist_remove (canvas->priv->items, old_item);
	if (canvas->priv->current_selected_item == old_item) {
		canvas->priv->current_selected_item = NULL;
		g_signal_emit (canvas, canvas_signals [ITEM_SELECTED], 0, NULL);
	}
}

static void
browser_canvas_dispose (GObject   * object)
{
	BrowserCanvas *canvas;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS (object));

	canvas = BROWSER_CANVAS (object);

	/* get rid of the GooCanvasItems */
	if (canvas->priv->items) {
		GSList *list;
		for (list = canvas->priv->items; list; list = list->next)
			g_object_weak_unref (G_OBJECT (list->data), (GWeakNotify) weak_ref_lost, canvas);
		g_slist_free (canvas->priv->items);
		canvas->priv->items = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}


/**
 * browser_canvas_declare_item
 * @canvas: a #BrowserCanvas widget
 * @item: a #BrowserCanvasItem object
 *
 * Declares @item to be listed by @canvas as one of its items.
 */
void
browser_canvas_declare_item (BrowserCanvas *canvas, BrowserCanvasItem *item)
{
        g_return_if_fail (IS_BROWSER_CANVAS (canvas));
        g_return_if_fail (canvas->priv);
        g_return_if_fail (IS_BROWSER_CANVAS_ITEM (item));

	/*g_print ("%s (canvas=>%p, item=>%p)\n", __FUNCTION__, canvas, item);*/
        if (g_slist_find (canvas->priv->items, item))
                return;

        canvas->priv->items = g_slist_prepend (canvas->priv->items, item);
	g_object_weak_ref (G_OBJECT (item), (GWeakNotify) weak_ref_lost, canvas);
}


static void
browser_canvas_finalize (GObject *object)
{
	BrowserCanvas *canvas;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS (object));
	canvas = BROWSER_CANVAS (object);

	if (canvas->priv) {
		g_free (canvas->priv);
		canvas->priv = NULL;
	}

	/* for the parent class */
	parent_class->finalize (object);
}

/**
 * browser_canvas_set_zoom_factor
 * @canvas: a #BrowserCanvas widget
 * @n: the zoom factor
 *
 * Sets the zooming factor of a canvas by specifying the number of pixels that correspond 
 * to one canvas unit. A zoom factor of 1.0 is the default value; greater than 1.0 makes a zoom in
 * and lower than 1.0 makes a zoom out.
 */
void
browser_canvas_set_zoom_factor (BrowserCanvas *canvas, gdouble n)
{
	g_return_if_fail (IS_BROWSER_CANVAS (canvas));
	g_return_if_fail (canvas->priv);

	if (n < 0.01)
		n = 0.01;
	else if (n > 1.)
		n = 1.;
	goo_canvas_set_scale (canvas->priv->goocanvas, n);
}

/**
 * browser_canvas_get_zoom_factor
 * @canvas: a #BrowserCanvas widget
 *
 * Get the current zooming factor of a canvas.
 *
 * Returns: the zooming factor.
 */
gdouble
browser_canvas_get_zoom_factor (BrowserCanvas *canvas)
{
	g_return_val_if_fail (IS_BROWSER_CANVAS (canvas), 1.);
	g_return_val_if_fail (canvas->priv, 1.);

	return goo_canvas_get_scale (canvas->priv->goocanvas);
}

/**
 * browser_canvas_fit_zoom_factor
 * @canvas: a #BrowserCanvas widget
 *
 * Compute and set the correct zoom factor so that all the items on @canvas can be displayed
 * at once.
 *
 * Returns: the new zooming factor.
 */
gdouble
browser_canvas_fit_zoom_factor (BrowserCanvas *canvas)
{
	gdouble zoom, xall, yall;
	GooCanvasBounds bounds;

	g_return_val_if_fail (IS_BROWSER_CANVAS (canvas), 1.);
	g_return_val_if_fail (canvas->priv, 1.);

	GtkAllocation alloc;
	gtk_widget_get_allocation (GTK_WIDGET (canvas), &alloc);
	xall = alloc.width;
	yall = alloc.height;

	goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (goo_canvas_get_root_item (canvas->priv->goocanvas)),
				    &bounds);
	bounds.y1 -= 6.; bounds.y2 += 6.;
	bounds.x1 -= 6.; bounds.x2 += 6.;
	zoom = yall / (bounds.y2 - bounds.y1);
	if (xall / (bounds.x2 - bounds.x1) < zoom)
		zoom = xall / (bounds.x2 - bounds.x1);

	/* set a limit to the zoom */
	if (zoom > DEFAULT_SCALE)
		zoom = DEFAULT_SCALE;
	
	browser_canvas_set_zoom_factor (canvas, zoom);

	return zoom;
}

/**
 * browser_canvas_center
 * @canvas: a #BrowserCanvas widget
 *
 * Centers the display on the layout
 */ 
void
browser_canvas_center (BrowserCanvas *canvas)
{
	/* remove top and left margins if we are running out of space */
	if (canvas->priv->goocanvas->hadjustment && canvas->priv->goocanvas->vadjustment) {
		gdouble hlow, hup, vlow, vup, hmargin, vmargin;
		gdouble left, top, right, bottom;
		GooCanvasBounds bounds;

		goo_canvas_get_bounds (canvas->priv->goocanvas, &left, &top, &right, &bottom);
		goo_canvas_item_get_bounds (goo_canvas_get_root_item (canvas->priv->goocanvas),
					    &bounds);

		g_object_get (G_OBJECT (GOO_CANVAS (canvas->priv->goocanvas)->hadjustment),
			      "lower", &hlow, "upper", &hup, NULL);
		g_object_get (G_OBJECT (GOO_CANVAS (canvas->priv->goocanvas)->vadjustment),
			      "lower", &vlow, "upper", &vup, NULL);

		/*
		g_print ("Canvas's bounds: %.2f,%.2f -> %.2f,%.2f\n", left, top, right, bottom);
		g_print ("Root's bounds: %.2f,%.2f -> %.2f,%.2f\n", bounds.x1, bounds.y1, bounds.x2, bounds.y2);
		g_print ("Xm: %.2f, Ym: %.2f\n", hup - hlow - (right - left), vup - vlow - (bottom - top));
		*/
		hmargin = hup - hlow - (bounds.x2 - bounds.x1);
		if (hmargin > 0) 
			left -= hmargin / 2. + (left - bounds.x1);
		vmargin = vup - vlow - (bounds.y2 - bounds.y1);
		if (vmargin > 0) 
			top -= vmargin / 2. + (top - bounds.y1);
		if ((hmargin > 0) || (vmargin > 0)) {
			goo_canvas_set_bounds (canvas->priv->goocanvas, left, top, right, bottom);
			/*g_print ("Canvas's new bounds: %.2f,%.2f -> %.2f,%.2f\n", left, top, right, bottom);*/
			goo_canvas_set_scale (canvas->priv->goocanvas, canvas->priv->goocanvas->scale);	
		}
	}
}

/**
 * browser_canvas_auto_layout_enabled
 * @canvas: a #BrowserCanvas widget
 *
 * Tells if @canvas has the possibility to automatically adjust its layout
 * using the GraphViz library.
 *
 * Returns: TRUE if @canvas can automatically adjust its layout
 */
gboolean
browser_canvas_auto_layout_enabled (BrowserCanvas *canvas)
{
	g_return_val_if_fail (IS_BROWSER_CANVAS (canvas), FALSE);
	g_return_val_if_fail (canvas->priv, FALSE);

#ifdef HAVE_GRAPHVIZ
	return TRUE;
#else
	return FALSE;
#endif
}

#ifdef HAVE_GRAPHVIZ
typedef struct {
	BrowserCanvas    *canvas;
	Agraph_t      *graph;
	GSList        *nodes_list; /* list of NodeLayout structures */
} GraphLayout;

typedef struct {
	BrowserCanvasItem *item; /* item to be moved */
	Agnode_t          *node;
	gdouble            start_x;
	gdouble            start_y;
	gdouble            end_x;
	gdouble            end_y;
	gdouble            width;
	gdouble            height;
	gdouble            dx;
	gdouble            dy;
	gboolean           stop;
	gdouble            cur_x;
	gdouble            cur_y;
} NodeLayout;

static gboolean canvas_animate_to (GraphLayout *gl);
#endif

/**
 * browser_canvas_auto_layout
 * @canvas: a #BrowserCanvas widget
 *
 * Re-organizes the layout of the @canvas' items using the GraphViz
 * layout engine.
 */
void
browser_canvas_perform_auto_layout (BrowserCanvas *canvas, gboolean animate, BrowserCanvasLayoutAlgorithm algorithm)
{
	g_return_if_fail (IS_BROWSER_CANVAS (canvas));
	g_return_if_fail (canvas->priv);

#define GV_SCALE 72.

#ifndef HAVE_GRAPHVIZ
	g_message ("GraphViz library support not compiled, cannot do graph layout...\n");
	return;
#else
	BrowserCanvasClass *class = BROWSER_CANVAS_CLASS (G_OBJECT_GET_CLASS (canvas));
	GSList *list, *layout_items;
	Agraph_t *graph;
	GHashTable *nodes_hash; /* key = BrowserCanvasItem, value = Agnode_t *node */
	GSList *nodes_list = NULL; /* list of NodeLayout structures */

	if (!gvc)
		gvc = gvContext ();

#ifdef GRAPHVIZ_NEW_API
	graph = agopen ("BrowserCanvasLayout", Agdirected, NULL);
        agset (graph, "shape", "box");
        agset (graph, "height", ".1");
        agset (graph, "width", ".1");
        agset (graph, "fixedsize", "true");
        agset (graph, "pack", "true");
	agset (graph, "packmode", "node");
#else
	graph = agopen ("BrowserCanvasLayout", AGRAPH);
        agnodeattr (graph, "shape", "box");
        agnodeattr (graph, "height", ".1");
        agnodeattr (graph, "width", ".1");
        agnodeattr (graph, "fixedsize", "true");
        agnodeattr (graph, "pack", "true");
	agnodeattr (graph, "packmode", "node");
#endif


	if (class->get_layout_items)
		layout_items = class->get_layout_items (canvas);
	else
		layout_items = canvas->priv->items;

	/* Graph nodes creation */
	nodes_hash = g_hash_table_new (NULL, NULL);
	for (list = layout_items; list; list = list->next) {
		BrowserCanvasItem *item = BROWSER_CANVAS_ITEM (list->data);
		Agnode_t *node;
		gchar *tmp;
		double val;
		GooCanvasBounds bounds;
		gboolean moving;
		NodeLayout *nl;

		g_object_get (G_OBJECT (item), "allow-move", &moving, NULL);
		if (!moving)
			continue;

		nl = g_new0 (NodeLayout, 1);
		nl->item = item;
		nodes_list = g_slist_prepend (nodes_list, nl);
		
		tmp = g_strdup_printf ("%p", item);
#ifdef GRAPHVIZ_NEW_API
		node = agnode (graph, tmp, 1);
#else
		node = agnode (graph, tmp);
#endif
		g_free (tmp);
		nl->node = node;
		g_hash_table_insert (nodes_hash, item, node);
		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (item), &bounds);
		nl->width = bounds.x2 - bounds.x1;
		nl->height = bounds.y2 - bounds.y1;
		val = (bounds.y2 - bounds.y1) / GV_SCALE;

		if (node) {
			tmp = g_strdup_printf ("%p", node);
			agset (node, "label", tmp);
			g_free (tmp);

			tmp = g_strdup_printf ("%.3f", val);
			agset (node, "height", tmp);
			g_free (tmp);

			val = (bounds.x2 - bounds.x1) / GV_SCALE;
			tmp = g_strdup_printf ("%.3f", val);
			agset (node, "width", tmp);
			g_free (tmp);
		}

		nl->start_x = bounds.x1;
		nl->start_y = bounds.y1;
		nl->cur_x = nl->start_x;
		nl->cur_y = nl->start_y;
		
		/*g_print ("Before: Node %p: HxW: %.3f %.3f\n", node, (bounds.y2 - bounds.y1) / GV_SCALE, 
		  (bounds.x2 - bounds.x1) / GV_SCALE);*/
	}
	/* Graph edges creation */
	for (list = layout_items; list; list = list->next) {
		BrowserCanvasItem *item = BROWSER_CANVAS_ITEM (list->data);
		BrowserCanvasItem *from, *to;
		gboolean moving;

		g_object_get (G_OBJECT (item), "allow-move", &moving, NULL);
		if (moving)
			continue;

		browser_canvas_item_get_edge_nodes (item, &from, &to);
		if (from && to) {
			Agnode_t *from_node, *to_node;
			from_node = (Agnode_t*) g_hash_table_lookup (nodes_hash, from);
			to_node = (Agnode_t*) g_hash_table_lookup (nodes_hash, to);
			if (from_node && to_node) {
#ifdef GRAPHVIZ_NEW_API
				agedge (graph, from_node, to_node, "", 0);
#else
				agedge (graph, from_node, to_node);
#endif
			}
		}
	}

	if (layout_items != canvas->priv->items)
		g_slist_free (layout_items);

	switch (algorithm) {
	default:
	case BROWSER_CANVAS_LAYOUT_DEFAULT:
		gvLayout (gvc, graph, "dot");
		break;
	case BROWSER_CANVAS_LAYOUT_RADIAL:
		gvLayout (gvc, graph, "circo");
		break;
	}
        /*gvRender (gvc, graph, "dot", NULL);*/
	/*gvRenderFilename (gvc, graph, "png", "out.png");*/
        /*gvRender (gvc, graph, "dot", stdout);*/

	for (list = nodes_list; list; list = list->next) {
		NodeLayout *nl = (NodeLayout*) list->data;
		nl->end_x = ND_coord_i (nl->node).x - (nl->width / 2.);
		nl->end_y = - ND_coord_i (nl->node).y - (nl->height / 2.);
		nl->dx = fabs (nl->end_x - nl->start_x);
		nl->dy = fabs (nl->end_y - nl->start_y);
		nl->stop = FALSE;
		/*g_print ("After: Node %p: HxW: %.3f %.3f XxY = %d, %d\n", nl->node, 
			 ND_height (nl->node), ND_width (nl->node),
			 ND_coord_i (nl->node).x, - ND_coord_i (nl->node).y);*/
		if (!animate)
			goo_canvas_item_translate (GOO_CANVAS_ITEM (nl->item), nl->end_x - nl->start_x,
						   nl->end_y - nl->start_y);
	}

	g_hash_table_destroy (nodes_hash);
	gvFreeLayout (gvc, graph);

	if (animate) {
		GraphLayout *gl;
		gl = g_new0 (GraphLayout, 1);
		gl->canvas = canvas;
		gl->graph = graph;
		gl->nodes_list = nodes_list;
		while (canvas_animate_to (gl));
	}
	else {
		agclose (graph);
		g_slist_foreach (nodes_list, (GFunc) g_free, NULL);
		g_slist_free (nodes_list);
	}

#endif
}

#ifdef HAVE_GRAPHVIZ
static gdouble
compute_animation_inc (float start, float stop, G_GNUC_UNUSED float current)
{
        gdouble inc;
#ifndef PI
#define PI 3.14159265
#endif
#define STEPS 30.

        if (stop == start)
                return 0.;

	inc = (stop - start) / STEPS;

        return inc;
}

static gboolean
canvas_animate_to (GraphLayout *gl) 
{
	gboolean stop = TRUE;
	GSList *list;

#define EPSILON 1.
	for (list = gl->nodes_list; list; list = list->next) {
		NodeLayout *nl = (NodeLayout*) list->data;
		if (!nl->stop) {
			gdouble dx, dy, ndx, ndy;

			dx = compute_animation_inc (nl->start_x, nl->end_x, nl->cur_x);
			dy = compute_animation_inc (nl->start_y, nl->end_y, nl->cur_y);
			ndx = fabs (nl->cur_x + dx - nl->end_x);
			ndy = fabs (nl->cur_y + dy - nl->end_y);
			nl->cur_x +=  dx;
			nl->cur_y +=  dy;
			browser_canvas_item_translate (nl->item, dx, dy);
			if (((ndx <= EPSILON) || (ndx >= nl->dx)) &&
			    ((ndy <= EPSILON) || (ndy >= nl->dy)))
				nl->stop = TRUE;
			else {
				stop = FALSE;
				nl->dx = ndx;
				nl->dy = ndy;
			}
		}
	}

	goo_canvas_request_update (GOO_CANVAS (gl->canvas->priv->goocanvas));
	goo_canvas_update (GOO_CANVAS (gl->canvas->priv->goocanvas));
	while (gtk_events_pending ())
		gtk_main_iteration ();

	if (stop) {
		agclose (gl->graph);
		g_slist_foreach (gl->nodes_list, (GFunc) g_free, NULL);
		g_slist_free (gl->nodes_list);
		g_free (gl);
	}
	return !stop;
}
#endif

/**
 * browser_canvas_scale_layout
 */
void
browser_canvas_scale_layout (BrowserCanvas *canvas, gdouble scale)
{
	GSList *list;
	GooCanvasBounds ref_bounds;
	gdouble refx, refy;

	g_return_if_fail (IS_BROWSER_CANVAS (canvas));
	if (!canvas->priv->items)
		return;

	goo_canvas_get_bounds (canvas->priv->goocanvas, &ref_bounds.x1, &ref_bounds.y1,
			       &ref_bounds.x2, &ref_bounds.y2);
	refx = (ref_bounds.x2 - ref_bounds.x1) / 2.;
	refy = (ref_bounds.y2 - ref_bounds.y1) / 2.;
	for (list = canvas->priv->items; list; list = list->next) {
		gboolean can_move;
		g_object_get ((GObject*) list->data, "allow-move", &can_move, NULL);
		if (can_move) {
			BrowserCanvasItem *item = BROWSER_CANVAS_ITEM (list->data);
			GooCanvasBounds bounds;
			gdouble tx, ty;
			
			goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (item), &bounds);
			tx = (scale - 1.) * (bounds.x1 - refx);
			ty = (scale - 1.) * (bounds.y1 - refy);
			browser_canvas_item_translate (item, tx, ty);
		}
	}
}

/**
 * browser_canvas_serialize_items
 */
gchar *
browser_canvas_serialize_items (BrowserCanvas *canvas)
{
	gchar *retval = NULL;
	GSList *list;
	xmlDocPtr doc;
	xmlNodePtr topnode;

	g_return_val_if_fail (IS_BROWSER_CANVAS (canvas), NULL);
	
	/* create XML doc and root node */
	doc = xmlNewDoc (BAD_CAST "1.0");
	topnode = xmlNewDocNode (doc, NULL, BAD_CAST "canvas", NULL);
        xmlDocSetRootElement (doc, topnode);
	
	/* actually serialize all the items which can be serialized */
	for (list = canvas->priv->items; list; list = list->next) {
		BrowserCanvasItem *item = BROWSER_CANVAS_ITEM (list->data);
		BrowserCanvasItemClass *iclass = (BrowserCanvasItemClass*) G_OBJECT_GET_CLASS (item);
		if (iclass->serialize) {
			xmlNodePtr node;
			node = iclass->serialize (item);
			if (node)
				xmlAddChild (topnode, node);
		}
	}

	/* create buffer from XML tree */
	xmlChar *xstr = NULL;
	xmlDocDumpMemory (doc, &xstr, NULL);
	if (xstr) {
		retval = g_strdup ((gchar *) xstr);
		xmlFree (xstr);
	}
	xmlFreeDoc (doc);
	
	return retval;
}

/**
 * browser_canvas_item_toggle_select
 */
void
browser_canvas_item_toggle_select (BrowserCanvas *canvas, BrowserCanvasItem *item)
{
	gboolean do_select = TRUE;
	g_return_if_fail (IS_BROWSER_CANVAS (canvas));
	g_return_if_fail (!item || IS_BROWSER_CANVAS_ITEM (item));

	if (canvas->priv->current_selected_item == item) {
		/* deselect item */
		do_select = FALSE;
	}

	if (canvas->priv->current_selected_item) {
		BrowserCanvasItemClass *iclass = BROWSER_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (canvas->priv->current_selected_item));
		if (iclass->set_selected)
			iclass->set_selected (canvas->priv->current_selected_item, FALSE);
		canvas->priv->current_selected_item = NULL;
	}


	if (do_select && item) {
		BrowserCanvasItemClass *iclass = BROWSER_CANVAS_ITEM_CLASS (G_OBJECT_GET_CLASS (item));
		if (iclass->set_selected)
			iclass->set_selected (item, TRUE);
		canvas->priv->current_selected_item = item;
	}
	g_signal_emit (canvas, canvas_signals [ITEM_SELECTED], 0, item);
}

void
browser_canvas_translate_item (G_GNUC_UNUSED BrowserCanvas *canvas, BrowserCanvasItem *item,
			       gdouble dx, gdouble dy)
{
	browser_canvas_item_translate (item, dx, dy);
}
