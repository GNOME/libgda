#include <libgda-ui/libgda-ui.h>
#include "../tests/data-model-errors.h"

static void destroy (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	gtk_init (&argc, &argv);
	gdaui_init ();

	/* create data model */
	GdaDataModel *model; 
        model = data_model_errors_new ();
	gda_data_model_dump (model, NULL);
	
	/* create UI */
	GtkWidget *window, *vbox, *button, *form, *grid;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_default_size (GTK_WINDOW(window), 400, 200);
	g_signal_connect_swapped (window, "destroy",
				  G_CALLBACK (destroy),
				  window);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	/* main form to list customers */
	form = gdaui_form_new (model);
	gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

        g_object_set (G_OBJECT (form),
		      "info-flags",
		      GDAUI_DATA_PROXY_INFO_CURRENT_ROW |
		      GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS |
		      GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS,
		      NULL
		      );

	/* main grid to list customers */
	grid = gdaui_grid_new (model);
	g_object_unref (model);
	gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

        g_object_set (G_OBJECT (grid),
		      "info-flags",
		      GDAUI_DATA_PROXY_INFO_CURRENT_ROW |
		      GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS,
		      NULL
		      );

	/* button to quit */
	button = gtk_button_new_with_label ("Quit");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	g_signal_connect_swapped (button, "clicked",
				  G_CALLBACK (gtk_widget_destroy),
				  window);

	gtk_widget_show_all (window);
	gtk_main ();

        return 0;
}
