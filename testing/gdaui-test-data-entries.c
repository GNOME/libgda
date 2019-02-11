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
#include <libgda-ui/libgda-ui.h>
#include <libgda-ui/gdaui-plugin.h>
#include <gtk/gtk.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

typedef enum {
	TESTED_BASIC,
	TESTED_FORM,
	TESTED_GRID
} TestedType;

typedef struct {
	GtkWidget      *vbox;
	GType           type;
	GdauiPlugin    *plugin;
	gchar          *plugin_name;
	GtkWidget      *options;
	GdaDataHandler *dh;
	GtkWidget      *test_widget;
} TestWidgetData;

typedef struct {
	TestedType  test_type;
	GType      *tested_gtypes;
	gint        nb_tested_gtypes;
	GHashTable *models_hash; /* key = GType, value = a GdaDataModel to test */
	GHashTable *tests_hash; /* key = a GtkBox, value = a TestWidgetData */
} TestConfig;

extern GHashTable *gdaui_plugins_hash;
TestConfig mainconf;

static gboolean delete_event( G_GNUC_UNUSED GtkWidget *widget,
                              G_GNUC_UNUSED GdkEvent  *event,
                              G_GNUC_UNUSED gpointer   data )
{
	g_print ("Leaving test...\n");
	
	return FALSE;
}

static void destroy( G_GNUC_UNUSED GtkWidget *widget,
                     G_GNUC_UNUSED gpointer   data )
{
	gtk_main_quit ();
}

static GtkWidget *build_menu (GtkWidget *mainwin, GtkWidget *top_nb);
static void       fill_tested_models (void);
static GtkWidget *build_test_for_plugin_struct (GdauiPlugin *plugin);
static void       build_test_widget (TestWidgetData *tdata);

static gboolean 
gtype_equal (gconstpointer a, gconstpointer b)
{
	return (GType) a == (GType) b ? TRUE : FALSE;
}

GdaDataHandler* 
get_handler (GType for_type)
{
	static GHashTable *hash = NULL;
	if (!hash) {
		hash = g_hash_table_new_full (g_direct_hash, gtype_equal, 
					      NULL, (GDestroyNotify) g_object_unref);

		g_hash_table_insert (hash, (gpointer) G_TYPE_UINT64, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_INT64, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_UINT, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_INT, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_USHORT, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_SHORT, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_UCHAR, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_CHAR, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_BINARY, gda_handler_bin_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_STRING, gda_handler_string_new ());
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_BLOB, gda_handler_bin_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_BOOLEAN, gda_handler_boolean_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_DATE, gda_handler_time_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_FLOAT, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_DOUBLE, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_NUMERIC, gda_handler_numerical_new ());
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIME, gda_handler_time_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_DATE_TIME, gda_handler_time_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_ULONG, gda_handler_type_new ());
	}

	return g_hash_table_lookup (hash, (gpointer) for_type);
}

static void builtin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, GtkWidget *nb);
static void plugin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, GtkWidget *nb);

gchar *test_type;
static GOptionEntry entries[] = {
        { "test-type", 't', 0, G_OPTION_ARG_STRING, &test_type, "Test condition", "{basic,form,grid}"},
        { NULL, 0, 0, 0, NULL, NULL, NULL }
};

int 
main (int argc, char **argv)
{
	GtkWidget *mainwin, *vbox, *nb, *label, *menu, *top_nb;
	GOptionContext *context;
	GError *error = NULL;

#ifdef HAVE_LOCALE_H
	/* Initialize i18n support */
	setlocale (LC_ALL,"");
#endif

	/* command line parsing */
        context = g_option_context_new ("Gdaui entry widgets and cell renderers testing");
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_warning ("Can't parse arguments: %s", error->message);
                exit (1); 
        }
        g_option_context_free (context);
	
	/* Initialize the widget set */
	gtk_init (&argc, &argv);
	gdaui_init ();

	/* init main conf */
	GType tested_gtypes [] = {G_TYPE_STRING,
				  G_TYPE_INT64, G_TYPE_UINT64, GDA_TYPE_BINARY, G_TYPE_BOOLEAN, GDA_TYPE_BLOB,
				  G_TYPE_DATE, G_TYPE_DOUBLE,
				  GDA_TYPE_GEOMETRIC_POINT, G_TYPE_OBJECT, G_TYPE_INT, 
				  GDA_TYPE_NUMERIC, G_TYPE_FLOAT, GDA_TYPE_SHORT, GDA_TYPE_USHORT,
				  GDA_TYPE_TIME, G_TYPE_DATE_TIME, G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_UINT};
	mainconf.test_type = TESTED_BASIC;
	if (test_type) {
		if (*test_type == 'f')
			mainconf.test_type = TESTED_FORM;
		else if (*test_type == 'g')
			mainconf.test_type = TESTED_GRID;
	}
	mainconf.tested_gtypes = tested_gtypes;
	mainconf.nb_tested_gtypes = sizeof (tested_gtypes) / sizeof (GType);
	mainconf.models_hash = g_hash_table_new (NULL, NULL);
	fill_tested_models ();
	mainconf.tests_hash = g_hash_table_new_full (NULL, NULL, NULL, g_free);

	/* Create the main window */
	mainwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (mainwin), 0);
	g_signal_connect (G_OBJECT (mainwin), "delete-event",
			  G_CALLBACK (delete_event), NULL);
	g_signal_connect (G_OBJECT (mainwin), "destroy",
			  G_CALLBACK (destroy), NULL);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (mainwin), vbox);
	gtk_widget_show (vbox);

	/* top notebook */
	top_nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (top_nb), FALSE);
	gtk_widget_show (top_nb);
	menu = build_menu (mainwin, top_nb);
	gtk_widget_show_all (menu);
	gtk_box_pack_start (GTK_BOX (vbox), menu, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), top_nb, TRUE, TRUE, 0);

	/*
	 * Default handlers
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), vbox, NULL);
	gtk_widget_show (vbox);

	label = gtk_label_new("");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), "<b>Default data entries:</b>\n"
			      "This test displays GdauiDataEntry widgets and helpers to test "
			      "them in pages of a notebook. Each page presents the default "
			      "data entry for the corresponding data type.");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);
	gtk_widget_set_margin_end (label, 10);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 5);
	gtk_widget_show (label);

	nb = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (nb), GTK_POS_LEFT);
	gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);
	gtk_widget_show (nb);

	g_hash_table_foreach (gdaui_plugins_hash, (GHFunc) builtin_hash_foreach_func, nb);

	/*
	 * Plugins page 
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), vbox, NULL);
	gtk_widget_show (vbox);

	label = gtk_label_new("");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), "<b>Plugin data entries:</b>\n"
			      "This test displays GdauiDataEntry widgets and helpers to test "
			      "them in pages of a notebook. Each page tests a plugin for a given "
			      "data type");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);
	gtk_widget_set_margin_end (label, 10);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 5);
	gtk_widget_show (label);

	nb = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (nb), GTK_POS_LEFT);
	gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);
	gtk_widget_show (nb);

	g_hash_table_foreach (gdaui_plugins_hash, (GHFunc) plugin_hash_foreach_func, nb);

	/* Show the application window */
	gtk_widget_show (mainwin);
	
	gtk_main ();

	return 0;
}

/*
 * Menu
 */
static void on_default_handlers_activate (GtkMenuItem *item, GtkWidget *top_nb);
static void on_plugins_activate (GtkMenuItem *item, GtkWidget *top_nb);
static GtkWidget *
build_menu (GtkWidget *mainwin, GtkWidget *top_nb)
{
	GtkWidget *menubar1, *menuitem1, *menuitem1_menu, *mitem, *menuitem2, *menuitem2_menu;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();

	menubar1 = gtk_menu_bar_new ();
	gtk_widget_show (menubar1);

	menuitem1 = gtk_menu_item_new_with_mnemonic ("_File");
	gtk_widget_show (menuitem1);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem1);
	
	menuitem1_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem1), menuitem1_menu);
	
	mitem = gtk_menu_item_new_with_label ("_Quit");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem1_menu), mitem);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (destroy), NULL);

	menuitem2 = gtk_menu_item_new_with_mnemonic ("_Tested Widgets");
	gtk_widget_show (menuitem2);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem2);
	
	menuitem2_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem2), menuitem2_menu);
	
	mitem = gtk_menu_item_new_with_mnemonic ("Default individual data entry widgets");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_BASIC));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_default_handlers_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic ("Default data entry widgets in a form");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_FORM));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_default_handlers_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic ("Default data cell renderers in a grid");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_GRID));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_default_handlers_activate), top_nb);
	
	mitem = gtk_separator_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);

	mitem = gtk_menu_item_new_with_mnemonic ("Plugins: data entry widgets alone");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_BASIC));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_plugins_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic ("Plugins: data entry widgets in a form");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_FORM));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_plugins_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic ("Plugins: data cell renderers");
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_GRID));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_plugins_activate), top_nb);

	gtk_window_add_accel_group (GTK_WINDOW (mainwin), accel_group);

	return menubar1;
}

static void
reset_tests_widgets_hash_foreach_func (G_GNUC_UNUSED GtkWidget *vbox, TestWidgetData *tdata,
				       G_GNUC_UNUSED gpointer data)
{
	if (tdata->test_widget) {
		gtk_widget_destroy (tdata->test_widget);
		tdata->test_widget = NULL;
		build_test_widget (tdata);
	}
}

static void
reset_test_widgets ()
{
	g_hash_table_foreach (mainconf.tests_hash, (GHFunc) reset_tests_widgets_hash_foreach_func, NULL);
}

static void
on_default_handlers_activate (GtkMenuItem *item, GtkWidget *top_nb)
{
	TestedType newtype = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "test_type"));
	if (newtype != mainconf.test_type) {
		mainconf.test_type = newtype;
		reset_test_widgets ();
	}
	gtk_notebook_set_current_page (GTK_NOTEBOOK (top_nb), 0);
}

static void
on_plugins_activate (GtkMenuItem *item, GtkWidget *top_nb)
{
	TestedType newtype = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "test_type"));
	if (newtype != mainconf.test_type) {
		mainconf.test_type = newtype;
		reset_test_widgets ();
	}
	gtk_notebook_set_current_page (GTK_NOTEBOOK (top_nb), 1);
}

static void
builtin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, GtkWidget *nb)
{
	GtkWidget *label, *wid;

	if (!plugin->plugin_file) {
		wid = build_test_for_plugin_struct (plugin);
		gtk_widget_show (wid);
		label = gtk_label_new (plugin_name);
		gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, label);
	}
}

static void
plugin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, GtkWidget *nb)
{
	GtkWidget *label, *wid;

	if (plugin->plugin_file) {
		wid = build_test_for_plugin_struct (plugin);
		gtk_widget_show (wid);
		label = gtk_label_new (plugin_name);
		gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, label);
	}
}


static void create_plugin_nb (GtkWidget *vbox, GdauiPlugin *plugin);
static void options_form_holder_changed_cb (GdauiBasicForm *form, GdaHolder *param, gboolean user_modif,
					   GtkWidget *grid);
static GtkWidget *
build_test_for_plugin_struct (GdauiPlugin *plugin)
{
	GtkWidget *label, *grid;
	GString *string;
	gchar *str, *txt = "";

	g_assert (plugin);

	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);

	/* explain top label */
	if (plugin->plugin_file)
		txt = "Plugin ";
	str = g_strdup_printf ("<b>%sName:</b>", txt);
	label = gtk_label_new ("");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_widget_show (label);

	label = gtk_label_new (plugin->plugin_name);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
	gtk_widget_show (label);

	str = g_strdup_printf ("<b>%sDescription:</b>", txt);
	label = gtk_label_new ("");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	gtk_widget_show (label);

	label = gtk_label_new (plugin->plugin_descr);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);
	gtk_widget_show (label);

	/* options */
	string = g_string_new ("");
	g_string_append_printf (string, "<b>%sOptions:</b>", txt);
	label = gtk_label_new ("");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_markup (GTK_LABEL (label), string->str);
	g_string_free (string, TRUE);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
	gtk_widget_show (label);

	if (plugin->options_xml_spec) {
		GdaSet *plist;
		GError *error = NULL;

		plist = gda_set_new_from_spec_string (plugin->options_xml_spec, &error);
		if (!plist) {
			gchar *str;
			str = g_strdup_printf ("Cannot parse XML spec for %soptions: %s", txt,
					       error && error->message ? error->message : "No detail");
			label = gtk_label_new (str);
			gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);
			g_warning ("%s", str);
			g_error_free (error);
		}
		else {
			GtkWidget *form, *frame;
			frame = gtk_frame_new ("");
			form = gdaui_basic_form_new (plist);
			g_object_unref (plist);
			gtk_widget_set_margin_top (form, 10);
			gtk_widget_set_margin_bottom (form, 10);
			gtk_widget_set_margin_start (form, 10);
			gtk_widget_set_margin_end (form, 10);
			gtk_grid_attach (GTK_GRID (grid), frame, 1, 2, 1, 1);
			gtk_container_add (GTK_CONTAINER (frame), form);
			gtk_widget_show_all (frame);
			g_object_set_data (G_OBJECT (grid), "options", form);
			g_signal_connect (G_OBJECT (form), "holder-changed",
					  G_CALLBACK (options_form_holder_changed_cb), grid);
		}
	}
	else {
		label = gtk_label_new ("None");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);
		gtk_widget_show (label);
	}

	/* notebook */
	create_plugin_nb (grid, plugin);

	gtk_widget_show (grid);

	return grid;
}
static void plugin_nb_page_changed_cb (GtkNotebook *nb, GtkWidget *page, gint pageno, GtkWidget *grid);
static void vbox_destroyed_cb (GtkWidget *widget, gpointer data);
static void
create_plugin_nb (GtkWidget *grid, GdauiPlugin *plugin)
{
	GtkWidget *wid, *nb, *label;
	gsize i;
	GType type;
	GdaDataHandler *dh;

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), "<b>Handled GTypes:</b>");
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
	gtk_widget_show (label);

	nb = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (nb), GTK_POS_LEFT);
	gtk_grid_attach (GTK_GRID (grid), nb, 1, 3, 1, 1);
	gtk_widget_show (nb);
	g_signal_connect (G_OBJECT (nb), "switch-page",
			  G_CALLBACK (plugin_nb_page_changed_cb), grid);

	g_object_set_data (G_OBJECT (grid), "nb", nb);
	g_object_set_data (G_OBJECT (grid), "plugin", plugin);

	if (plugin->nb_g_types > 0) {
		for (i = 0; i < plugin->nb_g_types; i++) {
			type = plugin->valid_g_types[i];
			dh = get_handler (type);
			if (dh) {
				TestWidgetData *tdata;

				wid = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				tdata = g_new0 (TestWidgetData, 1);
				tdata->vbox = wid;
				tdata->type = type;
				tdata->dh = dh;
				tdata->plugin = plugin;
				tdata->options = g_object_get_data (G_OBJECT (grid), "options");
				g_hash_table_insert (mainconf.tests_hash, wid, tdata);

				gtk_widget_show (wid);
				label = gtk_label_new (gda_g_type_to_string (type));
				gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, label);
				g_signal_connect (G_OBJECT (wid), "destroy",
						  G_CALLBACK (vbox_destroyed_cb), NULL);
			}
		}
	}
	else {
		gint j;
		for (j = 0; j < mainconf.nb_tested_gtypes; j++) {
			type = mainconf.tested_gtypes [j];
			dh = get_handler (type);
			if (dh) {
				TestWidgetData *tdata;

				wid = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				tdata = g_new0 (TestWidgetData, 1);
				tdata->vbox = wid;
				tdata->type = type;
				tdata->dh = dh;
				tdata->plugin = plugin;
				tdata->options = g_object_get_data (G_OBJECT (grid), "options");
				g_hash_table_insert (mainconf.tests_hash, wid, tdata);

				gtk_widget_show (wid);
				label = gtk_label_new (gda_g_type_to_string (type));
				gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, label);
				g_signal_connect (G_OBJECT (wid), "destroy",
						  G_CALLBACK (vbox_destroyed_cb), NULL);
			}
		}
	}
}

static void
vbox_destroyed_cb (GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
	g_hash_table_remove (mainconf.tests_hash, widget);
}

static GtkWidget *build_basic_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name);
static GtkWidget *build_form_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name);
static GtkWidget *build_grid_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name);

static void
plugin_nb_page_changed_cb (GtkNotebook *nb, G_GNUC_UNUSED GtkWidget *page, gint pageno,
			   G_GNUC_UNUSED GtkWidget *grid)
{
	GtkWidget *vbox;
	TestWidgetData *tdata;

	vbox = gtk_notebook_get_nth_page (nb, pageno);
	tdata = g_hash_table_lookup (mainconf.tests_hash, vbox);
	build_test_widget (tdata);
}

static void
build_test_widget (TestWidgetData *tdata)
{
	if (!tdata->test_widget) {
		gchar *options = NULL;
		gchar *plugin_name;
		GtkWidget *wid;

		if (tdata->options) {
			GdaSet *plist;
			GSList *list;
			GString *string = NULL;
			
			plist = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (tdata->options));
			g_assert (plist);
			for (list = gda_set_get_holders (plist); list; list = list->next) {
				GdaHolder *param = GDA_HOLDER (list->data);
				const GValue *value;
				value = gda_holder_get_value (param);
				if (value && !gda_value_is_null (value)) {
					gchar *str;
					if (!string)
						string = g_string_new ("");
					else
						g_string_append_c (string, ';');
					g_string_append (string, gda_holder_get_id (param));
					g_string_append_c (string, '=');
					str = gda_value_stringify (value);
					g_string_append (string, str);
					g_free (str);
				}
			}
			
			if (string) {
				options = string->str;
				g_string_free (string, FALSE);
			}
		}
		
		if (options) {
			plugin_name = g_strdup_printf ("%s:%s", tdata->plugin->plugin_name, options);
			g_free (options);
		}
		else
			plugin_name = g_strdup (tdata->plugin->plugin_name);
		g_free (tdata->plugin_name);
		tdata->plugin_name = plugin_name;
		
		switch (mainconf.test_type) {
		case TESTED_BASIC:
			wid = build_basic_test_for_gtype (tdata->dh, tdata->type, plugin_name);
			break;
		case TESTED_FORM:
			wid = build_form_test_for_gtype (tdata->dh, tdata->type, plugin_name);
			break;
		case TESTED_GRID:
			wid = build_grid_test_for_gtype (tdata->dh, tdata->type, plugin_name);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		
		gtk_box_pack_start (GTK_BOX (tdata->vbox), wid, TRUE, TRUE, 0);
		tdata->test_widget = wid;
		gtk_widget_show (wid);
	}
}

static void
options_form_holder_changed_cb (G_GNUC_UNUSED GdauiBasicForm *form, G_GNUC_UNUSED GdaHolder *param,
				G_GNUC_UNUSED gboolean user_modif, GtkWidget *grid)
{
	GtkWidget *nb;
	GdauiPlugin *plugin;
	
	nb = g_object_get_data (G_OBJECT (grid), "nb");
	plugin = g_object_get_data (G_OBJECT (grid), "plugin");
	g_assert (plugin);
	
	if (nb)
		gtk_widget_destroy (nb);
	g_object_set_data (G_OBJECT (grid), "nb", NULL);
	create_plugin_nb (grid, plugin);
}


/*
 * Basic test (individual GdauiDataEntry widget being tested)
 */
void entry_contents_or_attrs_changed (GtkWidget *entry, gpointer data);
void null_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void default_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void actions_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void editable_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void orig_clicked_cb (GtkButton *button, GtkWidget *entry);
void default_clicked_cb (GtkButton *button, GtkWidget *entry);
static GtkWidget *
build_basic_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name)
{
	GtkWidget *wid, *label, *grid, *button, *bbox, *entry, *vbox;
	GString *string;
	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	grid = gtk_grid_new ();
	gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
	gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

	/* explain top label */
	string = g_string_new ("");
	if (dh)
		g_string_append_printf (string, "<b>Data handler description:</b> %s\n",
					gda_data_handler_get_descr (dh));
	else
		g_string_append_printf (string, "<b>Data handler description:</b> %s\n",
					"No GdaDataHandler available for this type");
	label = gtk_label_new ("");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_markup (GTK_LABEL (label), string->str);
	g_string_free (string, TRUE);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);
	gtk_widget_show (label);

	/* widget being tested */
	label = gtk_label_new ("Data entry widget: ");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	gtk_widget_show (label);

	GtkWidget *frame;
	frame = gtk_frame_new ("");
	gtk_grid_attach (GTK_GRID (grid), frame, 1, 1, 2, 1);
	wid = GTK_WIDGET (gdaui_new_data_entry (type, plugin_name));
	gtk_widget_set_margin_top (wid, 10);
	gtk_widget_set_margin_bottom (wid, 10);
	gtk_widget_set_margin_start (wid, 10);
	gtk_widget_set_margin_end (wid, 10);
	g_object_set (G_OBJECT (wid), "handler", dh, NULL);
	gtk_container_add (GTK_CONTAINER (frame), wid);
	gtk_widget_show_all (frame);

	/* Other widgets */
	label = gtk_label_new ("Current flags: ");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
	gtk_widget_show (label);

	label = gtk_label_new ("--");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 2, 1);
	g_object_set_data (G_OBJECT (wid), "flags", label);
	gtk_widget_show (label);

	label = gtk_label_new ("Current value: ");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
	gtk_widget_show (label);
	
	label = gtk_label_new ("");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 3, 1, 1);
	g_object_set_data (G_OBJECT (wid), "value", label);
	g_signal_connect (G_OBJECT (wid), "contents-modified",
			  G_CALLBACK (entry_contents_or_attrs_changed), NULL);
	g_signal_connect (G_OBJECT (wid), "status-changed",
			  G_CALLBACK (entry_contents_or_attrs_changed), NULL);
	entry_contents_or_attrs_changed (wid, GTK_LABEL (label));
	gtk_widget_show (label);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
	gtk_grid_attach (GTK_GRID (grid), bbox, 0, 4, 2, 1);
		
	button = gtk_toggle_button_new_with_label ("NULL ok");
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (null_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wid)) & 
				      GDA_VALUE_ATTR_CAN_BE_NULL);

	button = gtk_toggle_button_new_with_label ("DEFAULT ok");
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (default_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wid)) & 
				      GDA_VALUE_ATTR_CAN_BE_DEFAULT);

	button = gtk_toggle_button_new_with_label ("Editable?");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (editable_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);

	entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "Grab focus here!");
	gtk_widget_set_can_focus (entry, TRUE);
	gtk_container_add (GTK_CONTAINER (bbox), entry);

	gtk_widget_show_all (bbox);

	/* to set the original value */
	entry = GTK_WIDGET (gdaui_new_data_entry (type, plugin_name));
	g_object_set (G_OBJECT (entry), "handler", dh, NULL);

	gtk_grid_attach (GTK_GRID (grid), entry, 1, 5, 1, 1);
	gtk_widget_show (entry);
	button = gtk_button_new_with_label ("Set as original");
	g_object_set_data (G_OBJECT (button), "entry", entry);
	gtk_grid_attach (GTK_GRID (grid), button, 0, 5, 1, 1);
	gtk_widget_show (button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (orig_clicked_cb), wid);

	/* to set the default value */
	entry = GTK_WIDGET (gdaui_new_data_entry (type, plugin_name));
	g_object_set (G_OBJECT (entry), "handler", dh, NULL);

	gtk_grid_attach (GTK_GRID (grid), entry, 1, 6, 1, 1);
	gtk_widget_show (entry);
	button = gtk_button_new_with_label ("Set as default");
	g_object_set_data (G_OBJECT (button), "entry", entry);
	gtk_grid_attach (GTK_GRID (grid), button, 0, 6, 1, 1);
	gtk_widget_show (button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (default_clicked_cb), wid);

	gtk_widget_show (grid);
	gtk_widget_show (vbox);

	return vbox;
}

void 
null_toggled_cb (GtkToggleButton *button, GtkWidget *entry)
{
	guint action = 0;
	if (gtk_toggle_button_get_active (button))
		action = GDA_VALUE_ATTR_CAN_BE_NULL;

	gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
				      action, GDA_VALUE_ATTR_CAN_BE_NULL);
	gtk_toggle_button_set_active (button,
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (entry)) & 
				      GDA_VALUE_ATTR_CAN_BE_NULL);
}

void
default_toggled_cb (GtkToggleButton *button, GtkWidget *entry)
{
	guint action = 0;
	if (gtk_toggle_button_get_active (button))
		action = GDA_VALUE_ATTR_CAN_BE_DEFAULT;

	gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
				      action, GDA_VALUE_ATTR_CAN_BE_DEFAULT);
	gtk_toggle_button_set_active (button,
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (entry)) & 
				      GDA_VALUE_ATTR_CAN_BE_DEFAULT);
}

void
editable_toggled_cb (GtkToggleButton *button, GtkWidget *entry)
{
	gboolean editable;

	editable = gtk_toggle_button_get_active (button);
	gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (entry), editable);
}

void 
entry_contents_or_attrs_changed (GtkWidget *entry, G_GNUC_UNUSED gpointer data)
{
	guint attrs;
	GtkLabel *label;
	GString *str = NULL;
	GValue *value;
	GdaDataHandler *dh;

	/* flags */
	label = g_object_get_data (G_OBJECT (entry), "flags");
	attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (entry));
	if (attrs & GDA_VALUE_ATTR_IS_NULL) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "IS_NULL");
	}
	if (attrs & GDA_VALUE_ATTR_CAN_BE_NULL) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "CAN_BE_NULL");
	}
	if (attrs & GDA_VALUE_ATTR_IS_DEFAULT) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "IS_DEFAULT");
	}
	if (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "CAN_BE_DEFAULT");
	}
	if (attrs & GDA_VALUE_ATTR_IS_UNCHANGED) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "IS_UNCHANGED");
	}
	if (attrs & GDA_VALUE_ATTR_DATA_NON_VALID) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "NON_VALID");
	}
	if (attrs & GDA_VALUE_ATTR_READ_ONLY) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "READ_ONLY");
	}
	if (attrs & GDA_VALUE_ATTR_HAS_VALUE_ORIG) {
		if (!str) str = g_string_new (""); else	g_string_append (str, ", ");
		g_string_append (str, "HAS_VALUE_ORIG");
	}

	gtk_label_set_text (label, str ? str->str : "");
	g_string_free (str, TRUE);

	/* value */
	label = g_object_get_data (G_OBJECT (entry), "value");
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (entry));
	if (dh) {
		gchar *strval;
		value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (entry));
		if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			const GdaBinary *bin;
			bin = gda_value_get_binary (value);
			strval = g_strdup_printf ("Binary data, size=%ld", gda_binary_get_size (bin));
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			blob = g_value_get_boxed (value);
			strval = g_strdup_printf ("Blob data, size=%ld", gda_binary_get_size (gda_blob_get_binary (blob)));
		}
		else
			strval = gda_data_handler_get_sql_from_value (dh, value);
		gda_value_free (value);
		gtk_label_set_text (label, strval);
		g_free (strval);
	}
}

void
orig_clicked_cb (GtkButton *button, GtkWidget *entry)
{
	GtkWidget *wid;
	GValue *value;

	wid = g_object_get_data (G_OBJECT (button), "entry");
	value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (wid));
	g_print ("Setting to original Value %p ", value);
	if (value) {
		gchar *str = gda_value_stringify (value);
		if (G_VALUE_TYPE (value) == GDA_TYPE_NULL)
			g_print ("GDA_TYPE_NULL");
		else
			g_print ("%s", gda_g_type_to_string (G_VALUE_TYPE (value)));
		g_print (": %s\n", str);
		g_free (str);
	}
	
	gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), value);
	gda_value_free (value);
}

void
default_clicked_cb (GtkButton *button, GtkWidget *entry)
{
	GtkWidget *wid;
	GValue *value;

	wid = g_object_get_data (G_OBJECT (button), "entry");
	value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (wid));
	gdaui_data_entry_set_default_value (GDAUI_DATA_ENTRY (entry), value);
	gda_value_free (value);
}

static GtkWidget *
build_form_test_for_gtype (G_GNUC_UNUSED GdaDataHandler *dh, GType type, const gchar *plugin_name)
{
	GdaDataModel *model;
	GtkWidget *wid;

	model = g_hash_table_lookup (mainconf.models_hash, GUINT_TO_POINTER (type));
	if (model) {
		GdaSet *plist;
		GdaHolder *param;

		wid = gdaui_form_new (model);
		plist = GDA_SET (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (wid)));
		param = gda_set_get_holders (plist)->data;

		g_object_set ((GObject*) param, "plugin", plugin_name, NULL);
	}
	else {
		gchar *str;

		str = g_strdup_printf ("No data model defined in this test for type '%s'",
				       gda_g_type_to_string (type));
		wid = gtk_label_new (str);
		g_free (str);
	}

	return wid;
}

static GtkWidget *
build_grid_test_for_gtype (G_GNUC_UNUSED GdaDataHandler *dh, GType type, const gchar *plugin_name)
{
	GdaDataModel *model;
	GtkWidget *wid;

	model = g_hash_table_lookup (mainconf.models_hash, GUINT_TO_POINTER (type));
	if (model) {
		GdaSet *plist;
		GdaHolder *param;

		wid = gdaui_grid_new (model);
		plist = GDA_SET (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (wid)));
		param = gda_set_get_holders (plist)->data;

		g_object_set ((GObject*) param, "plugin", plugin_name, NULL);
	}
	else {
		gchar *str;

		str = g_strdup_printf ("No data model defined in this test for type '%s'",
				       gda_g_type_to_string (type));
		wid = gtk_label_new (str);
		g_free (str);
	}

	return wid;
}

/*
 * Data models contents
 */
static void
fill_tested_models (void)
{
	gint row;
        GValue *value;
        GdaDataModel *model;

	/* G_TYPE_STRING */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_STRING);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_STRING), model);
	gda_data_model_set_column_title (model, 0, "String value");

        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "A string");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "22:54:35");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "11-23-2003");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "23-11-2003");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "23.11.2003");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "192.168.2.1/32");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "iVBORw0KGgoAAAANSUhEUgAAABUAAAAVCAYAAACpF6WWAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH1gcbEgkA8+uVSwAAAKtJREFUOMvFk0sOwyAMRD0op8NXYsmV7Ou5iwoEiDZ2E6UjIcC/vIwSiIhRQMyM0yIRsaazsxcAj5Lu5AVIRESqOgVVtcfWnEcHEVHOeQqO9zV3iXS3h0kBeHvMNbSRmBldFYD366++AeirqZRCpZR+XmPb75SZbyMdjbQ2tBGODxmJaq3xobd4ukuMu8fHqf8x0pU4mk+/DvxW93Go14pdXYo2ePKI/NN/1QtrPq4I2jvJSwAAAABJRU5ErkJggg==");
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_INT64 */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_INT64);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_INT64), model);
	gda_data_model_set_column_title (model, 0, "INT64 value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_int64 ((value = gda_value_new (G_TYPE_INT64)), G_MININT64);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_int64 ((value = gda_value_new (G_TYPE_INT64)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_int64 ((value = gda_value_new (G_TYPE_INT64)), G_MAXINT64);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_UINT64 */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_UINT64);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_UINT64), model);
	gda_data_model_set_column_title (model, 0, "UINT64 value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_uint64 ((value = gda_value_new (G_TYPE_UINT64)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_uint64 ((value = gda_value_new (G_TYPE_UINT64)), G_MAXUINT64);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_INT */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_INT);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_INT), model);
	gda_data_model_set_column_title (model, 0, "INT value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_int ((value = gda_value_new (G_TYPE_INT)), G_MININT);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_int ((value = gda_value_new (G_TYPE_INT)), G_MAXINT);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_UINT */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_UINT);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_UINT), model);
	gda_data_model_set_column_title (model, 0, "UINT value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_uint ((value = gda_value_new (G_TYPE_UINT)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_uint ((value = gda_value_new (G_TYPE_UINT)), G_MAXUINT);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* GDA_TYPE_SHORT */
        model = gda_data_model_array_new_with_g_types (1, GDA_TYPE_SHORT);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (GDA_TYPE_SHORT), model);
	gda_data_model_set_column_title (model, 0, "SHORT value");

	row = gda_data_model_append_row (model, NULL);
        gda_value_set_short ((value = gda_value_new (GDA_TYPE_SHORT)), G_MINSHORT);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        gda_value_set_short ((value = gda_value_new (GDA_TYPE_SHORT)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        gda_value_set_short ((value = gda_value_new (GDA_TYPE_SHORT)), G_MAXSHORT);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* GDA_TYPE_USHORT */
        model = gda_data_model_array_new_with_g_types (1, GDA_TYPE_USHORT);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (GDA_TYPE_USHORT), model);
	gda_data_model_set_column_title (model, 0, "USHORT value");

	row = gda_data_model_append_row (model, NULL);
        gda_value_set_ushort ((value = gda_value_new (GDA_TYPE_USHORT)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        gda_value_set_ushort ((value = gda_value_new (GDA_TYPE_USHORT)), G_MAXUSHORT);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_LONG */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_LONG);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_LONG), model);
	gda_data_model_set_column_title (model, 0, "LONG value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_long ((value = gda_value_new (G_TYPE_LONG)), G_MINLONG);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_long ((value = gda_value_new (G_TYPE_LONG)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_long ((value = gda_value_new (G_TYPE_LONG)), G_MAXLONG);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_ULONG */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_ULONG);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_ULONG), model);
	gda_data_model_set_column_title (model, 0, "Type (not gulong)");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_ulong ((value = gda_value_new (G_TYPE_ULONG)), G_TYPE_INVALID);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_ulong ((value = gda_value_new (G_TYPE_ULONG)), GDA_TYPE_TIME);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_FLOAT */
	model = gda_data_model_array_new_with_g_types (1, G_TYPE_FLOAT);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_FLOAT), model);
	gda_data_model_set_column_title (model, 0, "Float value");

        row = gda_data_model_append_row (model, NULL);
        g_value_set_float ((value = gda_value_new (G_TYPE_FLOAT)), 0.12345);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_float ((value = gda_value_new (G_TYPE_FLOAT)), 1234.5678);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_DOUBLE */
	model = gda_data_model_array_new_with_g_types (1, G_TYPE_DOUBLE);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_DOUBLE), model);
	gda_data_model_set_column_title (model, 0, "Double value");

        row = gda_data_model_append_row (model, NULL);
        g_value_set_double ((value = gda_value_new (G_TYPE_DOUBLE)), 0.12345);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
        row = gda_data_model_append_row (model, NULL);
        g_value_set_double ((value = gda_value_new (G_TYPE_DOUBLE)), 1234.5678);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_CHAR */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_CHAR);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_CHAR), model);
	gda_data_model_set_column_title (model, 0, "CHAR value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_schar ((value = gda_value_new (G_TYPE_CHAR)), (gint8) -128);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_schar ((value = gda_value_new (G_TYPE_CHAR)), (gint8) 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_schar ((value = gda_value_new (G_TYPE_CHAR)), (gint8) 127);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

	/* G_TYPE_UCHAR */
        model = gda_data_model_array_new_with_g_types (1, G_TYPE_UCHAR);
	g_hash_table_insert (mainconf.models_hash, GUINT_TO_POINTER (G_TYPE_UCHAR), model);
	gda_data_model_set_column_title (model, 0, "UCHAR value");

	row = gda_data_model_append_row (model, NULL);
        g_value_set_uchar ((value = gda_value_new (G_TYPE_UCHAR)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_uchar ((value = gda_value_new (G_TYPE_UCHAR)), 255);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);

}
