/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda-ui/libgda-ui.h>
#include "canvas/browser-canvas-db-relations.h"
#include "dnd.h"

static gboolean on_delete_event (GtkWidget *window, GdkEvent *event, gpointer unused_data);
static void auto_layout_cb (GtkWidget *button, BrowserCanvas *canvas);
static void label_drag_data_received (GtkWidget *label, GdkDragContext *context,
				      gint x, gint y, GtkSelectionData *data,
				      guint info, guint time);

static int
scroll_event_cb (G_GNUC_UNUSED GtkWidget *wid, GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	gboolean done = TRUE;

	g_print ("%d\n", event->type);
	switch (event->type) {
	case GDK_SCROLL:
		if (((GdkEventScroll *) event)->direction == GDK_SCROLL_UP)
			g_print ("UP\n");
		else if (((GdkEventScroll *) event)->direction == GDK_SCROLL_DOWN)
			g_print ("DOWN\n");
		done = TRUE;
		break;
	default:
		done = FALSE;
		break;
	}
	return done;
}

int
main (int argc, char *argv[])
{
	GdaConnection* connection;
	GError* error = NULL;

	gdaui_init ();
        gtk_init (&argc, &argv);

	/* open connection to the SalesTest data source */
	connection = gda_connection_open_from_dsn ("SalesTest", NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
	if (!connection) {
		fprintf (stderr, "%s\n", error->message);
		return -1;
	}

	/* mate store update */
	g_print ("Metastore update...\n");
	if (!gda_connection_update_meta_store (connection, NULL, &error))
		return -1;

	/* GdaMetaStruct */
	GdaMetaStruct *mstruct;
	mstruct = gda_meta_struct_new (gda_connection_get_meta_store (connection),
				       GDA_META_STRUCT_FEATURE_ALL);

	/* UI Part */
	GtkWidget *window, *grid, *canvas;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 600);
	g_signal_connect (window, "delete-event", G_CALLBACK (on_delete_event),
			  NULL);

	grid = gtk_grid_new ();
	gtk_container_set_border_width (GTK_CONTAINER (grid), 15);
	gtk_container_add (GTK_CONTAINER (window), grid);

	canvas = browser_canvas_db_relations_new (mstruct);
	g_object_unref (mstruct);

	gtk_grid_attach (GTK_GRID (grid), canvas, 0, 0, 1, 1);

	GtkWidget *bbox, *button;
	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach (GTK_GRID (grid), bbox, 0, 1, 1, 1);
	button = gtk_button_new_with_label ("Auto layout");
	g_signal_connect (button, "clicked",
			  G_CALLBACK (auto_layout_cb), canvas);
	gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);
	g_signal_connect (button, "scroll-event",
			  G_CALLBACK (scroll_event_cb), NULL);

	GtkWidget *wid;
	GtkTargetEntry dbo_table[] = {
		{ "text/plain", 0, 0 },
		{ "key-value", 0, 1 },
		{ "application/x-rootwindow-drop", 0, 2 }
	};
	wid = gtk_label_new ("\nDROP ZONE\n(hold SHIFT to drag and drop)\n");
	gtk_grid_attach (GTK_GRID (grid), wid, 0, 2, 1, 1);
	gtk_drag_dest_set (wid,
			   GTK_DEST_DEFAULT_ALL,
			   dbo_table, G_N_ELEMENTS (dbo_table),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect (wid, "drag_data_received",
			  G_CALLBACK (label_drag_data_received), NULL);

	gtk_window_set_default_size (GTK_WINDOW (window), 600, 450);
	gtk_widget_show_all (window);

	/* add some tables */
	GValue *tname;
	g_value_set_string ((tname = gda_value_new (G_TYPE_STRING)), "customers");
	browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (canvas), NULL, NULL, tname);
	g_value_set_string (tname, "orders");
	browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (canvas), NULL, NULL, tname);
	g_value_set_string (tname, "order_contents");
	browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (canvas), NULL, NULL, tname);
	g_value_set_string (tname, "products");
	browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (canvas), NULL, NULL, tname);
	g_value_set_string (tname, "locations");
	browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (canvas), NULL, NULL, tname);
	gda_value_free (tname);

	/* Pass control to the GTK+ main event loop. */
	gtk_main ();

	return 0;
}

static void
auto_layout_cb (G_GNUC_UNUSED GtkWidget *button, BrowserCanvas *canvas)
{
	browser_canvas_perform_auto_layout (BROWSER_CANVAS (canvas), TRUE, BROWSER_CANVAS_LAYOUT_RADIAL);
}

static void
label_drag_data_received (G_GNUC_UNUSED GtkWidget *label, GdkDragContext *context,
			  G_GNUC_UNUSED gint x, G_GNUC_UNUSED gint y, GtkSelectionData *data,
			  G_GNUC_UNUSED guint info, guint time)
{
	if ((gtk_selection_data_get_length (data) >= 0) && (gtk_selection_data_get_format (data) == 8)) {
		g_print ("Received \"%s\" in drop zone\n",
			 (gchar *) gtk_selection_data_get_data (data));
		gtk_drag_finish (context, TRUE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, FALSE, FALSE, time);
}

static gboolean
on_delete_event (G_GNUC_UNUSED GtkWidget *window, G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	exit (0);
}
