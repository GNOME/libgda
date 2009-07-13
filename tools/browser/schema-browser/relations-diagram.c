/*
 * Copyright (C) 2009 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "relations-diagram.h"
#include "../support.h"
#include "../cc-gray-bar.h"
#include "../canvas/browser-canvas-db-relations.h"
#include <gdk/gdkkeysyms.h>

struct _RelationsDiagramPrivate {
	BrowserConnection *bcnc;
	gchar *name; /* diagram's name */

	CcGrayBar *header;
	GtkWidget *canvas;
	GtkWidget *save_button;

	GtkWidget *window; /* to enter canvas's name */
	GtkWidget *name_entry;
	GtkWidget *real_save_button;
};

static void relations_diagram_class_init (RelationsDiagramClass *klass);
static void relations_diagram_init       (RelationsDiagram *diagram, RelationsDiagramClass *klass);
static void relations_diagram_dispose   (GObject *object);
static void relations_diagram_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void relations_diagram_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

static void meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, RelationsDiagram *diagram);

/* properties */
enum {
        PROP_0,
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint relations_diagram_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;


/*
 * RelationsDiagram class implementation
 */

static void
relations_diagram_class_init (RelationsDiagramClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Properties */
        object_class->set_property = relations_diagram_set_property;
        object_class->get_property = relations_diagram_get_property;

	object_class->dispose = relations_diagram_dispose;
}


static void
relations_diagram_init (RelationsDiagram *diagram, RelationsDiagramClass *klass)
{
	diagram->priv = g_new0 (RelationsDiagramPrivate, 1);
	diagram->priv->name = NULL;
	diagram->priv->window = NULL;
}

static void
relations_diagram_dispose (GObject *object)
{
	RelationsDiagram *diagram = (RelationsDiagram *) object;

	/* free memory */
	if (diagram->priv) {
		if (diagram->priv->bcnc) {
			g_signal_handlers_disconnect_by_func (diagram->priv->bcnc,
							      G_CALLBACK (meta_changed_cb), diagram);
			g_object_unref (diagram->priv->bcnc);
		}

		if (diagram->priv->window)
			gtk_widget_destroy (diagram->priv->window);

		g_free (diagram->priv->name);
		g_free (diagram->priv);
		diagram->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
relations_diagram_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (RelationsDiagramClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) relations_diagram_class_init,
			NULL,
			NULL,
			sizeof (RelationsDiagram),
			0,
			(GInstanceInitFunc) relations_diagram_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "RelationsDiagram", &info, 0);
	}
	return type;
}

static void
relations_diagram_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	RelationsDiagram *diagram;
	diagram = RELATIONS_DIAGRAM (object);
	switch (param_id) {
	}
}

static void
relations_diagram_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	RelationsDiagram *diagram;
	diagram = RELATIONS_DIAGRAM (object);
	switch (param_id) {
	}
}

static void
meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, RelationsDiagram *diagram)
{
	g_object_set (G_OBJECT (diagram->priv->canvas), "meta-struct", mstruct, NULL);
}


/*
 * POPUP
 */
static void
position_popup (RelationsDiagram *diagram)
{
        gint x, y;
        gint bwidth, bheight;
        GtkRequisition req;

        gtk_widget_size_request (diagram->priv->window, &req);

        gdk_window_get_origin (diagram->priv->save_button->window, &x, &y);

        x += diagram->priv->save_button->allocation.x;
        y += diagram->priv->save_button->allocation.y;
        bwidth = diagram->priv->save_button->allocation.width;
        bheight = diagram->priv->save_button->allocation.height;

        x += bwidth - req.width;
        y += bheight;

        if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;

        gtk_window_move (GTK_WINDOW (diagram->priv->window), x, y);
}



static gboolean
popup_grab_on_window (GdkWindow *window, guint32 activate_time)
{
        if ((gdk_pointer_grab (window, TRUE,
                               GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                               GDK_POINTER_MOTION_MASK,
                               NULL, NULL, activate_time) == 0)) {
                if (gdk_keyboard_grab (window, TRUE,
                                       activate_time) == 0)
                        return TRUE;
                else {
                        gdk_pointer_ungrab (activate_time);
                        return FALSE;
                }
        }
        return FALSE;
}


static gboolean
delete_popup (GtkWidget *widget, RelationsDiagram *diagram)
{
	gtk_widget_hide (diagram->priv->window);
        gtk_grab_remove (diagram->priv->window);
	return TRUE;
}

static gboolean
key_press_popup (GtkWidget *widget, GdkEventKey *event, RelationsDiagram *diagram)
{
	if (event->keyval != GDK_Escape)
                return FALSE;

        g_signal_stop_emission_by_name (widget, "key_press_event");
	gtk_widget_hide (diagram->priv->window);
        gtk_grab_remove (diagram->priv->window);
        return TRUE;
}

static gboolean
button_press_popup (GtkWidget *widget, GdkEventButton *event, RelationsDiagram *diagram)
{
	GtkWidget *child;

        child = gtk_get_event_widget ((GdkEvent *) event);

        /* We don't ask for button press events on the grab widget, so
         *  if an event is reported directly to the grab widget, it must
         *  be on a window outside the application (and thus we remove
         *  the popup window). Otherwise, we check if the widget is a child
         *  of the grab widget, and only remove the popup window if it
         *  is not.
         */
        if (child != widget) {
                while (child) {
                        if (child == widget)
                                return FALSE;
                        child = child->parent;
                }
        }
	gtk_widget_hide (diagram->priv->window);
        gtk_grab_remove (diagram->priv->window);
        return TRUE;
}


static void
real_save_clicked_cb (GtkWidget *button, RelationsDiagram *diagram)
{
	gchar *str;

	//str = browser_canvas_serialize_items (BROWSER_CANVAS (diagram->priv->canvas));
	str = g_strdup ("OOOOO");

	GError *lerror = NULL;
	BrowserFavorites *bfav;
	BrowserFavoritesAttributes fav;

	memset (&fav, 0, sizeof (BrowserFavoritesAttributes));
	fav.id = -1;
	fav.type = BROWSER_FAVORITES_DIAGRAMS;
	fav.name = gtk_editable_get_chars (GTK_EDITABLE (diagram->priv->name_entry), 0, -1);
	if (!*fav.name) {
		g_free (fav.name);
		fav.name = g_strdup ("Diagram #0");
	}
	fav.contents = str;
	
	gtk_widget_hide (diagram->priv->window);
	gtk_grab_remove (diagram->priv->window);
	
	bfav = browser_connection_get_favorites (diagram->priv->bcnc);
	if (! browser_favorites_add (bfav, 0, &fav, ORDER_KEY_SCHEMA, G_MAXINT, &lerror)) {
		browser_show_error ((GtkWindow*) gtk_widget_get_toplevel (button),
				    "<b>%s:</b>\n%s",
				    _("Could not save diagram"),
				    lerror && lerror->message ? lerror->message : _("No detail"));
		if (lerror)
			g_error_free (lerror);
	}
	g_free (fav.name);
	g_free (str);
}

static void
save_clicked_cb (GtkWidget *button, RelationsDiagram *diagram)
{
	gchar *str;

	if (!diagram->priv->window) {
		GtkWidget *window, *wid, *hbox;

		window = gtk_window_new (GTK_WINDOW_POPUP);
		gtk_widget_set_events (window, gtk_widget_get_events (window) | GDK_KEY_PRESS_MASK);
		gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
		gtk_container_set_border_width (GTK_CONTAINER (window), 5);
		g_signal_connect (G_OBJECT (window), "delete_event",
				  G_CALLBACK (delete_popup), diagram);
		g_signal_connect (G_OBJECT (window), "key_press_event",
				  G_CALLBACK (key_press_popup), diagram);
		g_signal_connect (G_OBJECT (window), "button_press_event",
				  G_CALLBACK (button_press_popup), diagram);
		diagram->priv->window = window;

		hbox = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (window), hbox);
		wid = gtk_label_new ("");
		str = g_strdup_printf ("%s:", _("Canvas's name"));
		gtk_label_set_markup (GTK_LABEL (wid), str);
		g_free (str);
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

		wid = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 5);
		diagram->priv->name_entry = wid;

		wid = gtk_button_new_with_label (_("Save"));
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
		g_signal_connect (wid, "clicked",
				  G_CALLBACK (real_save_clicked_cb), diagram);
		diagram->priv->real_save_button = wid;

		gtk_widget_show_all (hbox);
	}

	if (!popup_grab_on_window (button->window, gtk_get_current_event_time ()))
                return;
	position_popup (diagram);
	gtk_grab_add (diagram->priv->window);
        gtk_widget_show (diagram->priv->window);

	GdkScreen *screen;
	gint swidth, sheight;
	gint root_x, root_y;
	gint wwidth, wheight;
	gboolean do_move = FALSE;
	screen = gtk_window_get_screen (GTK_WINDOW (diagram->priv->window));
	if (screen) {
		swidth = gdk_screen_get_width (screen);
		sheight = gdk_screen_get_height (screen);
	}
	else {
		swidth = gdk_screen_width ();
		sheight = gdk_screen_height ();
	}
	gtk_window_get_position (GTK_WINDOW (diagram->priv->window), &root_x, &root_y);
	gtk_window_get_size (GTK_WINDOW (diagram->priv->window), &wwidth, &wheight);
	if (root_x + wwidth > swidth) {
		do_move = TRUE;
		root_x = swidth - wwidth;
	}
	else if (root_x < 0) {
		do_move = TRUE;
		root_x = 0;
	}
	if (root_y + wheight > sheight) {
		do_move = TRUE;
		root_y = sheight - wheight;
	}
	else if (root_y < 0) {
		do_move = TRUE;
		root_y = 0;
	}
	if (do_move)
		gtk_window_move (GTK_WINDOW (diagram->priv->window), root_x, root_y);

	str = gtk_editable_get_chars (GTK_EDITABLE (diagram->priv->name_entry), 0, -1);
	if (!*str)
		gtk_widget_grab_focus (diagram->priv->name_entry);
	else
		gtk_widget_grab_focus (diagram->priv->real_save_button);
	g_free (str);
        popup_grab_on_window (diagram->priv->window->window,
                              gtk_get_current_event_time ());
}


/**
 * relations_diagram_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
relations_diagram_new (BrowserConnection *bcnc)
{
	RelationsDiagram *diagram;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	diagram = RELATIONS_DIAGRAM (g_object_new (RELATIONS_DIAGRAM_TYPE, NULL));

	diagram->priv->bcnc = g_object_ref (bcnc);
	g_signal_connect (diagram->priv->bcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), diagram);

	/* header */
	GtkWidget *hbox, *wid;
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (diagram), hbox, FALSE, FALSE, 0);

        GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>\n%s", _("Relations diagram"), _("Unsaved diagram"));
	label = cc_gray_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	diagram->priv->header = CC_GRAY_BAR (label);

	wid = gtk_button_new ();
	label = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (wid), label);
	gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
	g_object_set (G_OBJECT (wid), "label", NULL, NULL);
	diagram->priv->save_button = wid;

	g_signal_connect (wid, "clicked",
			  G_CALLBACK (save_clicked_cb), diagram);

        gtk_widget_show_all (hbox);

	/* main contents */
	wid = browser_canvas_db_relations_new (NULL);
	diagram->priv->canvas = wid;
	gtk_box_pack_start (GTK_BOX (diagram), wid, TRUE, TRUE, 0);	
        gtk_widget_show_all (wid);
	
	GdaMetaStruct *mstruct;
	mstruct = browser_connection_get_meta_struct (diagram->priv->bcnc);
	if (mstruct)
		meta_changed_cb (diagram->priv->bcnc, mstruct, diagram);

	return (GtkWidget*) diagram;
}
