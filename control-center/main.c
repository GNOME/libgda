/* 
 * Copyright (C) 2000 - 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "dsn-config.h"
#include "provider-config.h"
#include "gdaui-dsn-assistant.h"

GtkWindow *main_window;
GtkActionGroup *actions;

#define DSN_PAGE      "DSN_Page"
#define PROVIDER_PAGE "Provider_Page"
static GtkWidget *create_main_notebook (void);

static void
show_error (GtkWindow *parent, const gchar *format, ...)
{
        va_list args;
        gchar sz[2048];
        GtkWidget *dialog;

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof sz, format, args);
        va_end (args);

        /* create the error message dialog */
	gchar *str;
	str = g_strconcat ("<span weight=\"bold\">",
                           _("Error:"),
                           "</span>\n",
                           sz,
                           NULL);

	dialog = gtk_message_dialog_new_with_markup (parent,
                                                     GTK_DIALOG_DESTROY_WITH_PARENT |
                                                     GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                     GTK_BUTTONS_CLOSE, "%s", str);
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                      gtk_button_new_from_stock (GTK_STOCK_OK),
                                      GTK_RESPONSE_OK);
        gtk_widget_show_all (dialog);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
assistant_finished_cb (GdauiDsnAssistant *assistant, gboolean error, G_GNUC_UNUSED gpointer user_data)
{
	const GdaDsnInfo *dsn_info;

	if (!error) {
		dsn_info = gdaui_dsn_assistant_get_dsn (assistant);
		if (dsn_info) {
			if (! gda_config_define_dsn (dsn_info, NULL)) 
				show_error (NULL, _("Could not declare new data source"));
		}
		else
			show_error (NULL, _("No valid data source info was created"));
	}
}

static void
assistant_closed_cb (GdauiDsnAssistant *assistant, G_GNUC_UNUSED gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (assistant));
}

static void
file_new_cb (G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED gpointer user_data)
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
file_properties_cb (G_GNUC_UNUSED GtkAction *action, gpointer user_data)
{
	GtkWidget *nb = GTK_WIDGET (user_data);
	GtkWidget *dsn, *provider, *current_widget;
	gint current;

	dsn = g_object_get_data (G_OBJECT (nb), DSN_PAGE);
	provider = g_object_get_data (G_OBJECT (nb), PROVIDER_PAGE);

	current = gtk_notebook_get_current_page (GTK_NOTEBOOK (nb));
	if (current == -1)
		return;

	current_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), current);
	if (current_widget == dsn)
		dsn_config_edit_properties (dsn);
}

static void
file_delete_cb (G_GNUC_UNUSED GtkAction *action, gpointer user_data)
{
	GtkWidget *nb = GTK_WIDGET (user_data);
	GtkWidget *dsn, *provider, *current_widget;
	gint current;

	dsn = g_object_get_data (G_OBJECT (nb), DSN_PAGE);
	provider = g_object_get_data (G_OBJECT (nb), PROVIDER_PAGE);

	current = gtk_notebook_get_current_page (GTK_NOTEBOOK (nb));
	if (current == -1)
		return;

	current_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), current);
	if (current_widget == dsn)
		dsn_config_delete (dsn);
}

static void
window_closed_cb (G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED gpointer user_data)
{
	gtk_main_quit ();
}

static void
about_cb (G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED gpointer user_data)
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

	gchar *path;
	path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-control-center.png", NULL);
	icon = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	dialog = gtk_about_dialog_new ();
	gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (dialog), _("Database access control center"));
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), PACKAGE_VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (dialog), "(C) 1998 - 2011 GNOME Foundation");
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (dialog), _("Database access services for the GNOME Desktop"));
	gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (dialog), "GNU Lesser General Public License");
	gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (dialog), "http://www.gnome-db.org");
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (dialog), authors);
	gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (dialog), documenters);
	gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (dialog), translator_credits);
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (dialog), icon);
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (gtk_widget_destroy),
			  dialog);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
	gtk_widget_show (dialog);

}

static GtkActionEntry ui_actions[] = {
	{ "Database", NULL, N_("_Data source"), NULL, N_("Database"), NULL },
	{ "DatabaseClose", GTK_STOCK_CLOSE, N_("_Close"), NULL, N_("Close this window"), G_CALLBACK (window_closed_cb) },
	{ "DatabaseNew", GTK_STOCK_NEW, N_("_New datasource"), NULL, N_("Create new data source"), G_CALLBACK (file_new_cb) },
	{ "DatabaseDelete", GTK_STOCK_DELETE, N_("_Delete datasource"), NULL, N_("Delete selected data source"), G_CALLBACK (file_delete_cb) },
	{ "DatabaseProperties", GTK_STOCK_PROPERTIES, N_("_Properties"), NULL, N_("Edit properties for selected data source"), G_CALLBACK (file_properties_cb) },
	{ "About", NULL, N_("_About"), NULL, N_("About"), NULL },
	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL, N_("About GNOME-DB"), G_CALLBACK (about_cb) }
};

static const gchar *ui_actions_info =
        "<ui>"
	"  <menubar name='MenuBar'>"
	"    <menu name='Database' action='Database'>"
	"      <menuitem name='DatabaseNew' action= 'DatabaseNew'/>"
	"      <menuitem name='DatabaseProperties' action= 'DatabaseProperties'/>"
	"      <menuitem name='DatabaseDelete' action= 'DatabaseDelete'/>"
	"      <separator/>"
	"      <menuitem name='DatabaseClose' action= 'DatabaseClose'/>"
	"    </menu>"
	"    <menu name='About' action='About'>"
	"      <menuitem name='HelpAbout' action= 'HelpAbout'/>"
	"    </menu>"
	"  </menubar>"
        "  <toolbar  name='ToolBar'>"
        "    <toolitem action='DatabaseNew'/>"
        "    <toolitem action='DatabaseProperties'/>"
        "    <toolitem action='DatabaseDelete'/>"
        "  </toolbar>"
        "</ui>";

static void
prepare_menu (GtkBox *vbox, GtkWidget *nb)
{
        GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkUIManager *ui;

        actions = gtk_action_group_new ("Actions");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
        gtk_action_group_add_actions (actions, ui_actions, G_N_ELEMENTS (ui_actions), nb);

        ui = gtk_ui_manager_new ();
        gtk_ui_manager_insert_action_group (ui, actions, 0);
        gtk_ui_manager_add_ui_from_string (ui, ui_actions_info, -1, NULL);

        menubar = gtk_ui_manager_get_widget (ui, "/MenuBar");
        gtk_box_pack_start (vbox, menubar, FALSE, FALSE, 0);
	gtk_widget_show (menubar);

	toolbar = gtk_ui_manager_get_widget (ui, "/ToolBar");
        gtk_box_pack_start (vbox, toolbar, FALSE, FALSE, 0);
	gtk_widget_show (toolbar);

	GtkAction *action;
	action = gtk_action_group_get_action (actions, "DatabaseProperties");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
	action = gtk_action_group_get_action (actions, "DatabaseDelete");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
}


static GtkWidget *
create_main_window (void)
{
	GtkWidget *window, *vbox;
	GtkWidget *nb;
	GdkPixbuf *icon;

	/* create the main window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	main_window = GTK_WINDOW (window);
	gtk_window_set_title (GTK_WINDOW (window), _("Database access control center"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size (GTK_WINDOW (window), 550, 450);
	g_signal_connect (G_OBJECT (window), "destroy",
			  G_CALLBACK (window_closed_cb), NULL);

	/* icon */
	gchar *path;
	path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-control-center.png", NULL);
	icon = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);
	if (icon) {
		gtk_window_set_icon (GTK_WINDOW (window), icon);
		g_object_unref (icon);
	}

	/* menu and contents */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show (vbox);

	nb = create_main_notebook ();	
	prepare_menu (GTK_BOX (vbox), nb);

        gtk_container_set_border_width (GTK_CONTAINER (nb), 6);
	gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);
	gtk_widget_show (nb);

	gtk_widget_show (window);
	return window;
}

static void
activate (GtkApplication *app)
{
	static GtkWidget *window = NULL;

	if (! window) {
		gdaui_init ();
		window = create_main_window ();
		gtk_window_set_application (GTK_WINDOW (window), app);
		gtk_widget_show (window);
	}
}

int
main (int argc, char *argv[])
{
	GtkApplication *app;
	gint status;
	
	app = gtk_application_new ("org.GnomeDb.GdaBrowser", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	
	status = g_application_run (G_APPLICATION (app), argc, argv);
	
	g_object_unref (app);

	return status;
}

static void
dsn_selection_changed_cb (GdauiRawGrid *dbrawgrid, gboolean row_selected, G_GNUC_UNUSED gpointer data)
{
	GtkAction *action;
	GArray *selection;

	selection = gdaui_data_selector_get_selected_rows (GDAUI_DATA_SELECTOR (dbrawgrid));

	action = gtk_action_group_get_action (actions, "DatabaseProperties");
	g_object_set (G_OBJECT (action), "sensitive", selection ? TRUE : FALSE, NULL);

	action = gtk_action_group_get_action (actions, "DatabaseDelete");
	g_object_set (G_OBJECT (action), "sensitive", selection ? TRUE : FALSE, NULL);
	if (selection)
		g_array_free (selection, TRUE);
}

static void
main_nb_page_switched_cb (G_GNUC_UNUSED GtkNotebook *notebook, G_GNUC_UNUSED GtkWidget *page, guint page_num, G_GNUC_UNUSED gpointer data)
{
	gboolean show;
	GtkAction *action;

	if (!actions)
		return;

	show = page_num == 0 ? TRUE : FALSE;
	action = gtk_action_group_get_action (actions, "DatabaseProperties");
	g_object_set (G_OBJECT (action), "visible", show, NULL);
	action = gtk_action_group_get_action (actions, "DatabaseDelete");
	g_object_set (G_OBJECT (action), "visible", show, NULL);
}

static GtkWidget *
create_main_notebook (void)
{
	GtkWidget *nb;
	GtkWidget *dsn;
	GtkWidget *provider;
	GdauiRawGrid *grid;

	nb = gtk_notebook_new ();
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), TRUE);
        gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
        gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));
        gtk_widget_show (nb);
	g_signal_connect (G_OBJECT (nb), "switch-page",
			  G_CALLBACK (main_nb_page_switched_cb), NULL);

	/* data source configuration page */
	dsn = dsn_config_new ();
	g_object_set_data (G_OBJECT (nb), DSN_PAGE, dsn);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), dsn,
				  gtk_label_new (_("Data Sources")));
	
	grid = g_object_get_data (G_OBJECT (dsn), "grid");
	g_signal_connect (G_OBJECT (grid), "selection-changed",
			  G_CALLBACK (dsn_selection_changed_cb), NULL);

	/* providers configuration page */
	provider = provider_config_new ();
	g_object_set_data (G_OBJECT (nb), PROVIDER_PAGE, provider);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), provider,
				  gtk_label_new (_("Providers")));

	return nb;
}
