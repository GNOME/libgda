/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "dsn-config.h"
#include "provider-config.h"
#include "gdaui-dsn-assistant.h"
#include <libgda-ui/internal/utility.h>

GtkApplication *app;
GtkWindow *main_window;

#define DSN_PAGE "DSN"
#define PROV_PAGE "Prov"
#define STACK "Nb"
static GtkWidget *create_main_stack (GtkApplicationWindow *app_window);

static void
assistant_finished_cb (GdauiDsnAssistant *assistant, gboolean error, G_GNUC_UNUSED gpointer user_data)
{
	const GdaDsnInfo *dsn_info;

	if (!error) {
		dsn_info = gdaui_dsn_assistant_get_dsn (assistant);
		if (dsn_info) {
			if (! gda_config_define_dsn (dsn_info, NULL)) 
				_gdaui_utility_show_error (NULL, _("Could not declare new data source"));
		}
		else
			_gdaui_utility_show_error (NULL, _("No valid data source info was created"));
	}
}

static void
assistant_closed_cb (GdauiDsnAssistant *assistant, G_GNUC_UNUSED gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (assistant));
}

static void
file_new_cb (G_GNUC_UNUSED GSimpleAction *action,
             G_GNUC_UNUSED GVariant *parameter,
             G_GNUC_UNUSED gpointer user_data)
{
	GtkWidget *assistant;

	assistant = gdaui_dsn_assistant_new ();
	g_signal_connect (G_OBJECT (assistant), "finished",
			  G_CALLBACK (assistant_finished_cb), NULL);
	g_signal_connect (G_OBJECT (assistant), "close",
			  G_CALLBACK (assistant_closed_cb), NULL);
	gtk_widget_show (assistant);
}

static void
file_delete_cb (G_GNUC_UNUSED GSimpleAction *action,
                G_GNUC_UNUSED GVariant *parameter,
                gpointer user_data)
{
	GtkWidget *app_window = GTK_WIDGET (user_data);
	GtkWidget *stack, *dsn;

	dsn = g_object_get_data (G_OBJECT (app_window), DSN_PAGE);
	stack = g_object_get_data (G_OBJECT (app_window), STACK);

	const gchar *visible;
	visible = gtk_stack_get_visible_child_name (GTK_STACK (stack));
	if (visible && !strcmp (visible, DSN_PAGE))
		dsn_config_delete (dsn);	
}

static void
show_datasources_cb (G_GNUC_UNUSED GSimpleAction *unused_action,
                     G_GNUC_UNUSED GVariant *parameter,
                     gpointer user_data)
{
	GtkWidget *app_window;
	GtkWidget *stack;
	app_window = GTK_WIDGET (user_data);
	stack = g_object_get_data (G_OBJECT (app_window), STACK);
	gtk_stack_set_visible_child_name (GTK_STACK (stack), DSN_PAGE);

	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (app_window), "ShowDatasources");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
	action = g_action_map_lookup_action (G_ACTION_MAP (app_window), "ShowProviders");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static void
show_providers_cb (G_GNUC_UNUSED GSimpleAction *unused_action,
                   G_GNUC_UNUSED GVariant *parameter,
                   gpointer user_data)
{
	GtkWidget *app_window;
	GtkWidget *stack;
	app_window = GTK_WIDGET (user_data);
	stack = g_object_get_data (G_OBJECT (app_window), STACK);
	gtk_stack_set_visible_child_name (GTK_STACK (stack), PROV_PAGE);

	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (app_window), "ShowDatasources");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
	action = g_action_map_lookup_action (G_ACTION_MAP (app_window), "ShowProviders");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
window_closed_cb (G_GNUC_UNUSED GSimpleAction *action,
                  G_GNUC_UNUSED GVariant *parameter,
                  G_GNUC_UNUSED gpointer user_data)
{
	g_application_quit (G_APPLICATION (app));
}

static void
about_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, G_GNUC_UNUSED gpointer user_data)
{
	GdkPixbuf *icon;
	GtkWidget *dialog;
	const gchar *authors[] = {
		"Vivien Malerba <malerba@gnome-db.org> (current maintainer)",
		"Rodrigo Moya <rodrigo@gnome-db.org>",
		"Carlos Perello Marin <carlos@gnome-db.org>",
		"Gonzalo Paniagua Javier <gonzalo@gnome-db.org>",
		"Laurent Sansonetti <lrz@gnome.org>",
		"Daniel Espinosa <esodan@gmail.com>",
		NULL
	};
	const gchar *documenters[] = {
		"Rodrigo Moya <rodrigo@gnome-db.org>",
		NULL
	};
	const gchar *translator_credits =
		"Christian Rose <menthos@menthos.com> Swedish translations\n" \
		"Kjartan Maraas <kmaraas@online.no> Norwegian translation\n";


	dialog = gtk_about_dialog_new ();
	gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (dialog), _("Database sources control center"));
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), PACKAGE_VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (dialog), "© 1998 - 2014 GNOME Foundation");
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (dialog), _("Database access services for the GNOME Desktop"));
	gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (dialog), "GNU Lesser General Public License");
	gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (dialog), "http://www.gnome-db.org");
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (dialog), authors);
	gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (dialog), documenters);
	gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (dialog), translator_credits);
	icon = gdk_pixbuf_new_from_resource ("/images/gda-control-center.png", NULL);
	if (icon)
		gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (dialog), icon);
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (gtk_widget_destroy),
			  dialog);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
	gtk_widget_show (dialog);

}

static GActionEntry app_entries[] = {
	{ "quit", window_closed_cb, NULL, NULL, NULL },
	{ "about", about_cb, NULL, NULL, NULL }
};

static GActionEntry win_entries[] = {
	{ "DatasourceNew", file_new_cb, NULL, NULL, NULL },
	{ "DatasourceDelete", file_delete_cb, NULL, NULL, NULL },
	{ "ShowDatasources", show_datasources_cb, NULL, NULL, NULL },
	{ "ShowProviders", show_providers_cb, NULL, NULL, NULL },
};

static void
startup (GApplication *app)
{
	g_action_map_add_action_entries (G_ACTION_MAP (app),
					 app_entries, G_N_ELEMENTS (app_entries),
					 NULL);
	GtkBuilder *builder;
	builder = gtk_builder_new ();
	g_assert (gtk_builder_add_from_resource (builder, "/application/menus.ui", NULL));

	GMenuModel *appmenu;
	appmenu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));

	gtk_application_set_app_menu (GTK_APPLICATION (app), appmenu);
	g_object_unref (builder);
}


static void
activate (GApplication *app)
{
	static GtkWidget *window = NULL;
	if (window) {
		gtk_window_present (GTK_WINDOW (window));
		return;
	}

	GtkWidget *vbox;
	GtkWidget *nb;

	gdaui_init ();

	/* create the main window */
	window = gtk_application_window_new (GTK_APPLICATION (app));
	main_window = GTK_WINDOW (window);

	GtkWidget *header;
	header = gtk_header_bar_new ();
	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
	gtk_header_bar_set_title (GTK_HEADER_BAR (header), _("Datasource access control center"));
	gtk_header_bar_set_has_subtitle (GTK_HEADER_BAR (header), FALSE);

	gtk_window_set_titlebar (GTK_WINDOW (window), header);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size (GTK_WINDOW (window), 780, 600);

	GtkWidget *menu_button;
	menu_button = gtk_menu_button_new ();
	gtk_header_bar_pack_end (GTK_HEADER_BAR (header), menu_button);

	GtkWidget *menu_icon;
	menu_icon = gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (menu_button), menu_icon);
	
	/* icon */
	GdkPixbuf *icon;
	icon = gdk_pixbuf_new_from_resource ("/images/gda-control-center.png", NULL);
	if (icon) {
		gtk_window_set_icon (GTK_WINDOW (window), icon);
		g_object_unref (icon);
	}

	/* contents */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show (vbox);

	nb = create_main_stack (GTK_APPLICATION_WINDOW (window));
        gtk_container_set_border_width (GTK_CONTAINER (nb), 6);
	gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);
	gtk_widget_show (nb);

	gtk_widget_show_all (window);

	/* menus */
	g_action_map_add_action_entries (G_ACTION_MAP (window),
					 win_entries, G_N_ELEMENTS (win_entries),
					 window);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (window),
										  "DatasourceDelete")), FALSE);

	GtkBuilder *builder;
	GMenuModel *menubar;
	builder = gtk_builder_new ();
	g_assert (gtk_builder_add_from_resource (builder, "/application/menus.ui", NULL));

	menubar = G_MENU_MODEL (gtk_builder_get_object (builder, "menubar"));

	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), menubar);
	g_object_unref (builder);

	/* set actions start state */
	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (window), "ShowDatasources");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

int
main (int argc, char *argv[])
{
	gint status;

	app = gtk_application_new ("org.Libgda.Preferences", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

	g_action_map_add_action_entries (G_ACTION_MAP (app),
					 app_entries, G_N_ELEMENTS (app_entries),
					 app);

	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}

static void
dsn_selection_changed_cb (GdauiRawGrid *dbrawgrid, GtkApplicationWindow *main_window)
{
	GArray *selection;
	selection = gdaui_data_selector_get_selected_rows (GDAUI_DATA_SELECTOR (dbrawgrid));

	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (main_window), "DatasourceDelete");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), selection ? TRUE : FALSE);
	if (selection)
		g_array_free (selection, TRUE);
}

static GtkWidget *
create_main_stack (GtkApplicationWindow *app_window)
{
	GtkWidget *stack;
	GtkWidget *dsn;
	GtkWidget *provider;
	GdauiRawGrid *grid;

	stack = gtk_stack_new ();
	g_object_set_data (G_OBJECT (app_window), STACK, stack);
        gtk_widget_show (stack);
	gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);

	/* data source configuration page */
	dsn = dsn_config_new ();
	g_object_set_data (G_OBJECT (app_window), DSN_PAGE, dsn);
	gtk_stack_add_named (GTK_STACK (stack), dsn, DSN_PAGE);
	
	grid = g_object_get_data (G_OBJECT (dsn), "grid");
	g_signal_connect (G_OBJECT (grid), "selection-changed",
			  G_CALLBACK (dsn_selection_changed_cb), app_window);

	/* providers configuration page */
	provider = provider_config_new ();
	gtk_stack_add_named (GTK_STACK (stack), provider, PROV_PAGE);

	return stack;
}
