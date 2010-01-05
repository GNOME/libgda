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
scroll_event_cb (GtkWidget *wid, GdkEvent *event, gpointer data)
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
	GtkWidget *window, *table, *canvas;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 600);
	g_signal_connect (window, "delete-event", G_CALLBACK (on_delete_event),
			  NULL);

	table = gtk_table_new (3, 1, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 15);
	gtk_container_add (GTK_CONTAINER (window), table);

	canvas = browser_canvas_db_relations_new (mstruct);
	g_object_unref (mstruct);

	gtk_table_attach_defaults (GTK_TABLE (table), canvas,
				   0, 1, 0, 1);

	GtkWidget *bbox, *button;
	bbox = gtk_hbutton_box_new ();
	gtk_table_attach (GTK_TABLE (table), bbox, 0, 1, 1, 2, 0, 0, 0, 0);
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
	gtk_table_attach (GTK_TABLE (table), wid, 0, 1, 2, 3, 0, 0, 0, 0);
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
auto_layout_cb (GtkWidget *button, BrowserCanvas *canvas)
{
	browser_canvas_perform_auto_layout (BROWSER_CANVAS (canvas), TRUE, BROWSER_CANVAS_LAYOUT_RADIAL);
}

static void
label_drag_data_received (GtkWidget *label, GdkDragContext *context,
			  gint x, gint y, GtkSelectionData *data,
			  guint info, guint time)
{
#if GTK_CHECK_VERSION(2,18,0)
	if ((gtk_selection_data_get_length (data) >= 0) && (gtk_selection_data_get_format (data) == 8)) {
		g_print ("Received \"%s\" in drop zone\n",
			 (gchar *) gtk_selection_data_get_data (data));
		gtk_drag_finish (context, TRUE, FALSE, time);
		return;
	}
#else
	if ((data->length >= 0) && (data->format == 8)) {
		g_print ("Received \"%s\" in drop zone\n", (gchar *)data->data);
		gtk_drag_finish (context, TRUE, FALSE, time);
		return;
	}
#endif

	gtk_drag_finish (context, FALSE, FALSE, time);
}

static gboolean
on_delete_event (GtkWidget *window, GdkEvent *event, gpointer unused_data)
{
	exit (0);
}

/*
 * icons
 */
typedef enum {
        BROWSER_ICON_BOOKMARK,
        BROWSER_ICON_SCHEMA,
        BROWSER_ICON_TABLE,
        BROWSER_ICON_COLUMN,
        BROWSER_ICON_COLUMN_PK,
        BROWSER_ICON_COLUMN_FK,
        BROWSER_ICON_COLUMN_FK_NN,
        BROWSER_ICON_COLUMN_NN,
        BROWSER_ICON_REFERENCE,

        BROWSER_ICON_LAST
} BrowserIconType;

GdkPixbuf *
browser_get_pixbuf_icon (BrowserIconType type)
{
        static GdkPixbuf **array = NULL;
        static const gchar* names[] = {
                "gda-browser-bookmark.png",
                "gda-browser-schema.png",
                "gda-browser-table.png",
                "gda-browser-column.png",
                "gda-browser-column-pk.png",
                "gda-browser-column-fk.png",
                "gda-browser-column-fknn.png",
                "gda-browser-column-nn.png",
                "gda-browser-reference.png"
        };

        if (!array)
                array = g_new0 (GdkPixbuf *, BROWSER_ICON_LAST);
        if (!array [type]) {
                gchar *path;
                path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", names[type], NULL);
                array [type] = gdk_pixbuf_new_from_file (path, NULL);
                g_free (path);

                if (!array [type])
                        array [type] = (GdkPixbuf*) 0x01;
        }
        if (array [type] == (GdkPixbuf*) 0x01)
                return NULL;
        else
                return array [type];
}
