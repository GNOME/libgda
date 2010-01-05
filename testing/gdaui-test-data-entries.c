#include <glib/gi18n-lib.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda-ui/gdaui-plugin.h>
#include <gtk/gtk.h>

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
	GHashTable *tests_hash; /* key = a GtkVBox, value = a TestWidgetData */
} TestConfig;

extern GHashTable *gdaui_plugins_hash;
TestConfig mainconf;

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
	g_print ("Leaving test...\n");
	
	return FALSE;
}

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
	gtk_main_quit ();
}

static GtkWidget *build_menu (GtkWidget *mainwin, GtkWidget *top_nb);
static void       fill_tested_models (void);
static GtkWidget *build_test_for_plugin_struct (GdauiPlugin *plugin);
static void       build_test_widget (TestWidgetData *tdata);

static guint
gtype_hash (gconstpointer key)
{
	return (guint) key;
}

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
		hash = g_hash_table_new_full (gtype_hash, gtype_equal, 
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
		g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIMESTAMP, gda_handler_time_new ());
		g_hash_table_insert (hash, (gpointer) G_TYPE_ULONG, gda_handler_type_new ());
	}

	return g_hash_table_lookup (hash, (gpointer) for_type);
}

static void builtin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, GtkWidget *nb);
static void plugin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, GtkWidget *nb);

gchar *test_type;
static GOptionEntry entries[] = {
        { "test-type", 't', 0, G_OPTION_ARG_STRING, &test_type, "Test condition", "{basic,form,grid}"},
        { NULL }
};

int 
main (int argc, char **argv)
{
	GtkWidget *mainwin, *vbox, *nb, *label, *menu, *top_nb;
	GOptionContext *context;
	GError *error = NULL;

	/* Initialize i18n support */
	gtk_set_locale ();

	/* command line parsing */
        context = g_option_context_new ("Gdaui entry widgets and cell renderers testing");
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_warning ("Can't parse arguments: %s", error->message);
                exit (1); 
        }
        g_option_context_free (context);
	
	/* Initialize the widget set */
	gdaui_init ();
	gtk_init (&argc, &argv);

	/* init main conf */
	GType tested_gtypes [] = {G_TYPE_INT64, G_TYPE_UINT64, GDA_TYPE_BINARY, G_TYPE_BOOLEAN, GDA_TYPE_BLOB,
				  G_TYPE_DATE, G_TYPE_DOUBLE,
				  GDA_TYPE_GEOMETRIC_POINT, G_TYPE_OBJECT, G_TYPE_INT, GDA_TYPE_LIST, 
				  GDA_TYPE_NUMERIC, G_TYPE_FLOAT, GDA_TYPE_SHORT, GDA_TYPE_USHORT, G_TYPE_STRING, 
				  GDA_TYPE_TIME, GDA_TYPE_TIMESTAMP, G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_UINT};
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

	vbox = gtk_vbox_new (FALSE, 0);
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
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), vbox, NULL);
	gtk_widget_show (vbox);

	label = gtk_label_new("");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), "<b>Default data entries:</b>\n"
			      "This test displays GdauiDataEntry widgets and helpers to test "
			      "them in pages of a notebook. Each page presents the default "
			      "data entry for the corresponding data type.");
	gtk_misc_set_alignment (GTK_MISC (label), 0., 0.5);
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
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), vbox, NULL);
	gtk_widget_show (vbox);

	label = gtk_label_new("");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), "<b>Plugin data entries:</b>\n"
			      "This test displays GdauiDataEntry widgets and helpers to test "
			      "them in pages of a notebook. Each page tests a plugin for a given "
			      "data type");
	gtk_misc_set_alignment (GTK_MISC (label), 0., 0.5);
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

	menuitem1 = gtk_menu_item_new_with_mnemonic (_("_File"));
	gtk_widget_show (menuitem1);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem1);
	
	menuitem1_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem1), menuitem1_menu);
	
	mitem = gtk_image_menu_item_new_from_stock ("gtk-quit", accel_group);
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem1_menu), mitem);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (destroy), NULL);

	menuitem2 = gtk_menu_item_new_with_mnemonic (_("_Tested Widgets"));
	gtk_widget_show (menuitem2);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem2);
	
	menuitem2_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem2), menuitem2_menu);
	
	mitem = gtk_menu_item_new_with_mnemonic (_("Default individual data entry widgets"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_BASIC));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_default_handlers_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic (_("Default data entry widgets in a form"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_FORM));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_default_handlers_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic (_("Default data cell renderers in a grid"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_GRID));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_default_handlers_activate), top_nb);
	
	mitem = gtk_separator_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);

	mitem = gtk_menu_item_new_with_mnemonic (_("Plugins individual data entry widgets"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_BASIC));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_plugins_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic (_("Plugins data entry widgets in a form"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_FORM));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_plugins_activate), top_nb);

	mitem = gtk_menu_item_new_with_mnemonic (_("Plugins data cell renderers in a grid"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menuitem2_menu), mitem);
	g_object_set_data (G_OBJECT (mitem), "test_type", GINT_TO_POINTER (TESTED_GRID));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (on_plugins_activate), top_nb);

	gtk_window_add_accel_group (GTK_WINDOW (mainwin), accel_group);

	return menubar1;
}

static void
reset_tests_widgets_hash_foreach_func (GtkWidget *vbox, TestWidgetData *tdata, gpointer data)
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
					   GtkWidget *table);
static GtkWidget *
build_test_for_plugin_struct (GdauiPlugin *plugin)
{
	GtkWidget *label, *table;
	GString *string;
	gchar *str, *txt = "";

	g_assert (plugin);

	table = gtk_table_new (4, 2, FALSE);	

	/* explain top label */
	if (plugin->plugin_file)
		txt = "Plugin ";
	str = g_strdup_printf ("<b>%sName:</b>", txt);
	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);

	label = gtk_label_new (plugin->plugin_name);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (label);

	str = g_strdup_printf ("<b>%sDescription:</b>", txt);
	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_widget_show (label);

	label = gtk_label_new (plugin->plugin_descr);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (label);

	/* options */
	string = g_string_new ("");
	g_string_append_printf (string, "<b>%sOptions:</b>", txt);
	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_label_set_markup (GTK_LABEL (label), string->str);
	g_string_free (string, TRUE);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
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
			gtk_table_attach (GTK_TABLE (table), label, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
			g_warning ("%s", str);
			g_error_free (error);
		}
		else {
			GtkWidget *form;

			form = gdaui_basic_form_new (plist);
			g_object_unref (plist);
			gtk_table_attach (GTK_TABLE (table), form, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
			gtk_widget_show (form);
			g_object_set_data (G_OBJECT (table), "options", form);
			g_signal_connect (G_OBJECT (form), "holder-changed",
					  G_CALLBACK (options_form_holder_changed_cb), table);
		}
	}
	else {
		label = gtk_label_new ("None");
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
		gtk_table_attach (GTK_TABLE (table), label, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
		gtk_widget_show (label);
	}

	/* notebook */
	create_plugin_nb (table, plugin);

	gtk_widget_show (table);

	return table;
}
static void plugin_nb_page_changed_cb (GtkNotebook *nb, GtkNotebookPage *page, gint pageno, GtkWidget *table);
static void vbox_destroyed_cb (GtkWidget *widget, gpointer data);
static void
create_plugin_nb (GtkWidget *table, GdauiPlugin *plugin)
{
	GtkWidget *wid, *nb, *label;
	gint i;
	GType type;
	GdaDataHandler *dh;

	nb = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (nb), GTK_POS_LEFT);
	gtk_table_attach_defaults (GTK_TABLE (table), nb, 0, 2, 3, 4);
	gtk_widget_show (nb);
	g_signal_connect (G_OBJECT (nb), "switch-page",
			  G_CALLBACK (plugin_nb_page_changed_cb), table);

	g_object_set_data (G_OBJECT (table), "nb", nb);
	g_object_set_data (G_OBJECT (table), "plugin", plugin);

	if (plugin->nb_g_types > 0)
		for (i = 0; i < plugin->nb_g_types; i++) {
			type = plugin->valid_g_types[i];
			dh = get_handler (type);
			if (dh) {
				TestWidgetData *tdata;

				wid = gtk_vbox_new (FALSE, 0);
				tdata = g_new0 (TestWidgetData, 1);
				tdata->vbox = wid;
				tdata->type = type;
				tdata->dh = dh;
				tdata->plugin = plugin;
				tdata->options = g_object_get_data (G_OBJECT (table), "options");
				g_hash_table_insert (mainconf.tests_hash, wid, tdata);

				gtk_widget_show (wid);
				label = gtk_label_new (gda_g_type_to_string (type));
				gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, label);
				g_signal_connect (G_OBJECT (wid), "destroy",
						  G_CALLBACK (vbox_destroyed_cb), NULL);
			}
		}
	else
		for (i = 0; i < mainconf.nb_tested_gtypes; i++) {
			type = mainconf.tested_gtypes [i];
			dh = get_handler (type);
			if (dh) {
				TestWidgetData *tdata;

				wid = gtk_vbox_new (FALSE, 0);
				tdata = g_new0 (TestWidgetData, 1);
				tdata->vbox = wid;
				tdata->type = type;
				tdata->dh = dh;
				tdata->plugin = plugin;
				tdata->options = g_object_get_data (G_OBJECT (table), "options");
				g_hash_table_insert (mainconf.tests_hash, wid, tdata);

				gtk_widget_show (wid);
				label = gtk_label_new (gda_g_type_to_string (type));
				gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, label);
				g_signal_connect (G_OBJECT (wid), "destroy",
						  G_CALLBACK (vbox_destroyed_cb), NULL);
			}
		}
}

static void
vbox_destroyed_cb (GtkWidget *widget, gpointer data)
{
	g_hash_table_remove (mainconf.tests_hash, widget);
}

static GtkWidget *build_basic_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name);
static GtkWidget *build_form_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name);
static GtkWidget *build_grid_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name);

static void
plugin_nb_page_changed_cb (GtkNotebook *nb, GtkNotebookPage *page, gint pageno, GtkWidget *table)
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
			for (list = plist->holders; list; list = list->next) {
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
options_form_holder_changed_cb (GdauiBasicForm *form, GdaHolder *param, gboolean user_modif, GtkWidget *table)
{
	GtkWidget *nb;
	GdauiPlugin *plugin;
	
	nb = g_object_get_data (G_OBJECT (table), "nb");
	plugin = g_object_get_data (G_OBJECT (table), "plugin");
	g_assert (plugin);
	
	if (nb)
		gtk_widget_destroy (nb);
	g_object_set_data (G_OBJECT (table), "nb", NULL);
	create_plugin_nb (table, plugin);
}


/*
 * Basic test (individual GdauiDataEntry widget being tested)
 */
void entry_contents_modified (GtkWidget *entry, gpointer data);
void null_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void default_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void actions_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void editable_toggled_cb (GtkToggleButton *button, GtkWidget *entry);
void orig_clicked_cb (GtkButton *button, GtkWidget *entry);
void default_clicked_cb (GtkButton *button, GtkWidget *entry);
static GtkWidget *
build_basic_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name)
{
	GtkWidget *wid, *label, *table, *button, *bbox, *entry, *vbox;
	GString *string;
	gboolean expand;
	
	vbox = gtk_vbox_new (FALSE, 0);

	table = gtk_table_new (7, 3, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);

	/* explain top label */
	string = g_string_new ("");
	if (dh)
		g_string_append_printf (string, "<b>Data handler description:</b> %s\n",
					gda_data_handler_get_descr (dh));
	else
		g_string_append_printf (string, "<b>Data handler description:</b> %s\n",
					_("No GdaDataHandler available for this type"));
	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), string->str);
	g_string_free (string, TRUE);
	gtk_table_attach (GTK_TABLE (table), label, 0, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (label);

	/* widget being tested */
	wid = GTK_WIDGET (gdaui_new_data_entry (type, plugin_name));
	g_object_set (G_OBJECT (wid), "handler", dh, NULL);

	expand = gdaui_data_entry_expand_in_layout (GDAUI_DATA_ENTRY (wid));
	if (expand)
		gtk_table_attach_defaults (GTK_TABLE (table), wid, 0, 3, 1, 2);
	else
		gtk_table_attach (GTK_TABLE (table), wid, 0, 3, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (wid);

	/* Other widgets */
	label = gtk_label_new (_("Current flags: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);	

	label = gtk_label_new (_("--"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 1, 3, 2, 3, 0, 0, 0, 0);
	g_object_set_data (G_OBJECT (wid), "flags", label);
	gtk_widget_show (label);

	label = gtk_label_new (_("Current value: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);
	
	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 1, 3, 3, 4, 0, 0, 0, 0);
	g_object_set_data (G_OBJECT (wid), "value", label);
	g_signal_connect (G_OBJECT (wid), "contents-modified",
			  G_CALLBACK (entry_contents_modified), NULL);
	gtk_widget_show (label);

	entry_contents_modified (wid, GTK_LABEL (label));

	bbox = gtk_hbutton_box_new ();
	gtk_table_attach (GTK_TABLE (table), bbox, 0, 3, 4, 5, 0, 0, 0, 0);
		
	button = gtk_toggle_button_new_with_label (_("NULL ok"));
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (null_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wid)) & 
				      GDA_VALUE_ATTR_CAN_BE_NULL);

	button = gtk_toggle_button_new_with_label (_("DEFAULT ok"));
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (default_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wid)) & 
				      GDA_VALUE_ATTR_CAN_BE_DEFAULT);

	button = gtk_toggle_button_new_with_label (_("Actions?"));
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (actions_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wid)) & 
				      GDA_VALUE_ATTR_ACTIONS_SHOWN);

	button = gtk_toggle_button_new_with_label (_("Editable?"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (editable_toggled_cb), wid);
	gtk_container_add (GTK_CONTAINER (bbox), button);
			      
	gtk_widget_show_all (bbox);

	/* to set the original value */
	entry = GTK_WIDGET (gdaui_new_data_entry (type, plugin_name));
	g_object_set (G_OBJECT (entry), "handler", dh, NULL);

	gtk_table_attach (GTK_TABLE (table), entry, 0, 2, 5, 6, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (entry);
	button = gtk_button_new_with_label (_("Set as original"));
	g_object_set_data (G_OBJECT (button), "entry", entry);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 5, 6, 0, 0, 0, 0);
	gtk_widget_show (button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (orig_clicked_cb), wid);

	/* to set the default value */
	entry = GTK_WIDGET (gdaui_new_data_entry (type, plugin_name));
	g_object_set (G_OBJECT (entry), "handler", dh, NULL);

	gtk_table_attach (GTK_TABLE (table), entry, 0, 2, 6, 7, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show (entry);
	button = gtk_button_new_with_label (_("Set as default"));
	g_object_set_data (G_OBJECT (button), "entry", entry);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 6, 7, 0, 0, 0, 0);
	gtk_widget_show (button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (default_clicked_cb), wid);

	gtk_widget_show (table);

	if (!expand) {
		label = gtk_label_new ("");
		gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
		gtk_widget_show (label);
	}

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
actions_toggled_cb (GtkToggleButton *button, GtkWidget *entry)
{
	guint action = 0;
	if (gtk_toggle_button_get_active (button))
		action = GDA_VALUE_ATTR_ACTIONS_SHOWN;

	gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (entry),
				      action, GDA_VALUE_ATTR_ACTIONS_SHOWN);
	gtk_toggle_button_set_active (button,
				      gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (entry)) & 
				      GDA_VALUE_ATTR_ACTIONS_SHOWN);
}

void
editable_toggled_cb (GtkToggleButton *button, GtkWidget *entry)
{
	gboolean editable;

	editable = gtk_toggle_button_get_active (button);
	gdaui_data_entry_set_editable (GDAUI_DATA_ENTRY (entry), editable);
}

void 
entry_contents_modified (GtkWidget *entry, gpointer data)
{
	guint attrs;
	GtkLabel *label;
	GString *str = g_string_new ("");
	GValue *value;
	GdaDataHandler *dh;

	/* flags */
	label = g_object_get_data (G_OBJECT (entry), "flags");
	attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (entry));
	if (attrs & GDA_VALUE_ATTR_IS_NULL)
		g_string_append (str, "N ");
	if (attrs & GDA_VALUE_ATTR_IS_DEFAULT)
		g_string_append (str, "D ");
	if (!(attrs & GDA_VALUE_ATTR_IS_UNCHANGED))
		g_string_append (str, "M ");
	if (attrs & GDA_VALUE_ATTR_DATA_NON_VALID)
		g_string_append (str, "U ");

	gtk_label_set_text (label, str->str);
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
			strval = g_strdup_printf ("Binary data, size=%ld", bin->binary_length);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			const GdaBlob *blob;
			blob = gda_value_get_blob (value);
			strval = g_strdup_printf ("Blob data, size=%ld", ((GdaBinary*) blob)->binary_length);
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
	
	gdaui_data_entry_set_original_value (GDAUI_DATA_ENTRY (entry), value);
	gda_value_free (value);
	entry_contents_modified (entry, NULL);
}

void
default_clicked_cb (GtkButton *button, GtkWidget *entry)
{
	GtkWidget *wid;
	GValue *value;

	wid = g_object_get_data (G_OBJECT (button), "entry");
	value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (wid));
	gdaui_data_entry_set_value_default (GDAUI_DATA_ENTRY (entry), value);
	gda_value_free (value);
}

static GtkWidget *
build_form_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name)
{
	GdaDataModel *model;
	GtkWidget *wid;

	model = g_hash_table_lookup (mainconf.models_hash, GUINT_TO_POINTER (type));
	if (model) {
		GdaSet *plist;
		GdaHolder *param;
		GValue *value;

		wid = gdaui_form_new (model);
		plist = GDA_SET (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (wid)));
		gdaui_data_proxy_column_show_actions (GDAUI_DATA_PROXY (wid), -1, TRUE);
		param = plist->holders->data;

		value = gda_value_new_from_string (plugin_name, G_TYPE_STRING);
		gda_holder_set_attribute_static (param, GDAUI_ATTRIBUTE_PLUGIN, value);
		gda_value_free (value);
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
build_grid_test_for_gtype (GdaDataHandler *dh, GType type, const gchar *plugin_name)
{
	GdaDataModel *model;
	GtkWidget *wid;

	model = g_hash_table_lookup (mainconf.models_hash, GUINT_TO_POINTER (type));
	if (model) {
		GdaSet *plist;
		GdaHolder *param;
		GValue *value;
		
		wid = gdaui_grid_new (model);
		plist = GDA_SET (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (wid)));
		gdaui_data_proxy_column_show_actions (GDAUI_DATA_PROXY (wid), -1, TRUE);
		param = plist->holders->data;

		value = gda_value_new_from_string (plugin_name, G_TYPE_STRING);
		gda_holder_set_attribute_static (param, GDAUI_ATTRIBUTE_PLUGIN, value);
		gda_value_free (value);
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
        g_value_set_char ((value = gda_value_new (G_TYPE_CHAR)), -128);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_char ((value = gda_value_new (G_TYPE_CHAR)), 0);
        gda_data_model_set_value_at (model, 0, row, value, NULL); gda_value_free (value);
	row = gda_data_model_append_row (model, NULL);
        g_value_set_char ((value = gda_value_new (G_TYPE_CHAR)), 127);
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
