/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda-ui/libgda-ui.h>
#include "../tests/data-model-errors.h"

static void destroy (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
	gtk_main_quit ();
}

static gboolean change_unknow_color (GdauiBasicForm *form)
{
	static gdouble sign = 1.;
	static gdouble red = .3;
	static gdouble green = .1;
	static gdouble blue = .1;
	static gdouble alpha = .5;

	red += .075 * sign;
	if (red >= 1.) {
		sign = -1.;
		red = 1.;
	}
	else if (red <= .3) {
		sign = 1.;
		red = .3;
	}
	gdaui_basic_form_set_unknown_color (form, red, green, blue, alpha);
	return TRUE; /* keep timer */
}

int
main (int argc, char *argv[])
{
	gtk_init (&argc, &argv);
	gdaui_init ();

	/* create data model */
	GdaDataModel *model; 
        model = gda_data_model_errors_new ();
	gda_data_model_dump (model, NULL);
	
	/* create UI */
	GtkWidget *window, *vbox, *button, *form, *grid;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_default_size (GTK_WINDOW(window), 400, 200);
	g_signal_connect_swapped (window, "destroy",
				  G_CALLBACK (destroy),
				  window);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	/* main form to list customers */
	form = gdaui_form_new (model);
	gtk_box_pack_start (GTK_BOX (vbox), form, FALSE, FALSE, 0);

	GtkWidget *raw;
	g_object_get (form, "raw-form", &raw, NULL);
	g_timeout_add (80, (GSourceFunc) change_unknow_color, raw);
	g_object_unref (raw);

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
