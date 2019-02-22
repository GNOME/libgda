/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <libgda-ui/gdaui-data-entry.h>
#include <libgda-ui/gdaui-plugin.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "data-entries/gdaui-entry-boolean.h"
#include "data-entries/gdaui-entry-bin.h"
#include "data-entries/gdaui-entry-string.h"
#include "data-entries/gdaui-entry-number.h"
#include "data-entries/gdaui-entry-time.h"
#include "data-entries/gdaui-entry-date.h"
#include "data-entries/gdaui-entry-timestamp.h"
#include "data-entries/gdaui-entry-none.h"
#include "data-entries/gdaui-data-cell-renderer-textual.h"
#include "data-entries/gdaui-data-cell-renderer-boolean.h"
#include "data-entries/gdaui-data-cell-renderer-bin.h"
#include "gdaui-resources.h"

/* plugins list */

typedef GSList           *(*GdauiPluginInit)     (GError **);
static GHashTable *init_plugins_hash (void);
GHashTable *gdaui_plugins_hash = NULL; /* key = plugin name, value = GdauiPlugin structure pointer */

static void
catch_css_parsing_errors (GtkCssProvider *provider,
               GtkCssSection  *section,
               GError         *error,
               gpointer        user_data)
{
  g_return_if_fail (error != NULL);
  g_warning (_("Error parsing CSS: %s"), error->message);
}

extern void _gdaui_register_resource (void);
/**
 * gdaui_init:
 *
 * Initialization of the libgda-ui library, must be called before any usage of the library.
 *
 * <itemizedlist>
 * <listitem><para>Note 1: gtk_init() is not called by this function and should also
 *   be called (see Note 2 about the importance of the calling order)</para></listitem>
 * <listitem><para>Note 2: you should always call gtk_init() before gdaui_init().</para></listitem>
 * <listitem><para>Note 3: this funtion also calls gda_init() so it should not be called
 *    again</para></listitem>
 * </itemizedlist>
 *
 * Since: 4.2
 */
void
gdaui_init (void)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		gda_log_error (_("Attempt to initialize an already initialized library"));
		return;
	}

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	_gdaui_register_resource ();
	gda_init ();
	if (! gdaui_plugins_hash)
		gdaui_plugins_hash = init_plugins_hash ();

	/* initialize CSS */
  GtkCssProvider *css_provider;
  css_provider = gtk_css_provider_new ();
  g_object_connect (css_provider, "signal::parsing-error", catch_css_parsing_errors, NULL, NULL);
  gtk_css_provider_load_from_resource (css_provider, "/gdaui/gdaui.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
								   GTK_STYLE_PROVIDER (css_provider),
								   GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
  g_object_unref (css_provider);
	initialized = TRUE;
}

/**
 * gdaui_new_data_entry:
 * @type: a #GType
 * @plugin_name: (nullable): the name of an entry plugin, or %NULL
 *
 * Creates a new #GdauiDataEntry widget, taking into account the requested entry name
 * if @plugin_name is not %NULL (if no entry of that name is found, then the default data
 * entry widget will be created).
 *
 * The @plugin_name format is interpreted as two parts: &lt;plugin name&gt;:&lt;plugin options&gt;, and
 * if the plugins has no option, then the ":&lt;plugin options&gt;" part may be omitted.
 * 
 * Returns: (transfer full): a new #GdauiDataEntry widget, _NEVER_ %NULL
 */
GdauiDataEntry *
gdaui_new_data_entry (GType type, const gchar *plugin_name)
{
	GdaDataHandler *dh;
	GdauiDataEntry *entry = NULL;
	gchar *spec_options = NULL;

	if (!gdaui_plugins_hash)
		gdaui_plugins_hash = init_plugins_hash ();

	if (type == GDA_TYPE_NULL)
		return (GdauiDataEntry *) gdaui_entry_none_new (GDA_TYPE_NULL);

	dh = gda_data_handler_get_default (type);

	if (plugin_name && *plugin_name) {
		GdauiPlugin *plugin_struct;
		gchar *plugin = g_strdup (plugin_name);
		gchar *ptr, *options = NULL;

		for (ptr = plugin; *ptr && (*ptr != ':'); ptr++);
		*ptr = 0;
		ptr++;
		if (ptr < plugin + strlen (plugin_name)) {
			options = ptr;
			spec_options = g_strdup (options);
		}
		
		plugin_struct = g_hash_table_lookup (gdaui_plugins_hash, plugin);
		if (plugin_struct && plugin_struct->entry_create_func) { 
			gboolean allok = TRUE;
			if (plugin_struct->nb_g_types > 0) {
				guint i;
				for (i = 0; i < plugin_struct->nb_g_types; i++) {
					if (plugin_struct->valid_g_types[i] == type)
						break;
				}
				if (i == plugin_struct->nb_g_types)
					allok = FALSE;
			}
			if (allok)
				entry = (plugin_struct->entry_create_func) (dh, type, options);
		}
		g_free (plugin);
	}

	if (!entry) {
		if (type == G_TYPE_STRING)
			entry = (GdauiDataEntry *) gdaui_entry_string_new (dh, type, spec_options);
		else if ((type == G_TYPE_INT64) ||
			 (type == G_TYPE_UINT64) ||
			 (type == G_TYPE_DOUBLE) ||
			 (type == G_TYPE_INT) ||
			 (type == G_TYPE_LONG) ||
			 (type == GDA_TYPE_NUMERIC) ||
			 (type == G_TYPE_FLOAT) ||
			 (type == GDA_TYPE_SHORT) ||
			 (type == GDA_TYPE_USHORT) ||
			 (type == G_TYPE_CHAR) ||
			 (type == G_TYPE_UCHAR) ||
			 (type == G_TYPE_ULONG) ||
			 (type == G_TYPE_UINT))
			entry = (GdauiDataEntry *) gdaui_entry_number_new (dh, type, spec_options);
		else if (type == G_TYPE_BOOLEAN)
			entry = (GdauiDataEntry *) gdaui_entry_boolean_new (dh, G_TYPE_BOOLEAN);
		else if ((type == GDA_TYPE_BLOB) ||
			 (type == GDA_TYPE_BINARY))
			entry = (GdauiDataEntry *) gdaui_entry_bin_new (dh, type);
		else if	((type == GDA_TYPE_GEOMETRIC_POINT) ||
			 (type == G_TYPE_OBJECT))
			entry = (GdauiDataEntry *) gdaui_entry_none_new (type);
		else if	(type == GDA_TYPE_TIME)
			entry = (GdauiDataEntry *) gdaui_entry_time_new (dh);
		else if (type == G_TYPE_DATE_TIME)
			entry = (GdauiDataEntry *) gdaui_entry_timestamp_new (dh);
		else if (type == G_TYPE_DATE)
			entry = (GdauiDataEntry *) gdaui_entry_date_new (dh);
		else
			entry = (GdauiDataEntry *) gdaui_entry_none_new (type);
	}

	g_free (spec_options);
	return entry;
}

/*
 * _gdaui_new_cell_renderer
 * @type: a #GType
 * @plugin_name: (nullable): the name of an entry plugin, or %NULL
 *
 * Creates a new #GtkCellRenderer object which is suitable to use in
 * a #GtkTreeView widget, taking into account the requested entry name
 * if @plugin_name is not %NULL (if no entry of that name is found, then the default data
 * entry widget will be created).
 *
 * @plugin_name format is interpreted as two parts: &lt;plugin name&gt;:&lt;plugin options&gt;, and
 * if the plugins has no option, then the ":&lt;plugin options&gt;" part may be omitted.
 * 
 * 
 * Returns: a new #GtkCellRenderer object, _NEVER_ %NULL
 */
GtkCellRenderer *
_gdaui_new_cell_renderer (GType type, const gchar *plugin_name)
{
	GdaDataHandler *dh;
	GtkCellRenderer *cell = NULL;

	if (!gdaui_plugins_hash) 
		gdaui_plugins_hash = init_plugins_hash ();

	dh = gda_data_handler_get_default (type);
	
	if (plugin_name && *plugin_name) {
		GdauiPlugin *plugin_struct;
		gchar *plugin = g_strdup (plugin_name);
		gchar *ptr, *options = NULL;

		for (ptr = plugin; *ptr && (*ptr != ':'); ptr++);
		*ptr = 0;
		ptr++;
		if (ptr < plugin + strlen (plugin_name))
			options = ptr;
		
		plugin_struct = g_hash_table_lookup (gdaui_plugins_hash, plugin);
		if (plugin_struct && plugin_struct->cell_create_func) 
			cell = (plugin_struct->cell_create_func) (dh, type, options);
		g_free (plugin);
	}

	if (!cell) {
		if (type == GDA_TYPE_NULL)
			cell = gdaui_data_cell_renderer_textual_new (NULL, GDA_TYPE_NULL, NULL);
		else if (type == G_TYPE_BOOLEAN)
			cell = gdaui_data_cell_renderer_boolean_new (dh, G_TYPE_BOOLEAN);
		else if ((type == GDA_TYPE_BLOB) ||
			 (type == GDA_TYPE_BINARY))
			cell = gdaui_data_cell_renderer_bin_new (dh, type);
		else
			cell = gdaui_data_cell_renderer_textual_new (dh, type, NULL);
	}

	return cell;
}

/**
 * gdaui_plugin_declare:
 * @plugin: a pointer to a structure filled to describe the new plugin. All the contained information is copied.
 *
 * Adds a new plugin which will be used by the forms and grids. The new plugin, as
 * described by @plugin can declare a custom widget to be used for forms, grids, or both.
 *
 * If a plugin is already declared with the same name as the requested name, then
 * a warning is issued and the operation fails.
 */
void
gdaui_plugin_declare (const GdauiPlugin *plugin)
{
	GdauiPlugin *np;

	g_return_if_fail (plugin);
	g_return_if_fail (plugin->plugin_name);
	if (!gdaui_plugins_hash)
		gdaui_plugins_hash = init_plugins_hash ();
	if (g_hash_table_lookup (gdaui_plugins_hash, plugin->plugin_name)) {
		g_warning ("Plugin '%s' already declared", plugin->plugin_name);
		return;
	}
	if (((plugin->nb_g_types < 1) && plugin->valid_g_types) ||
	    ((plugin->nb_g_types > 0) && !plugin->valid_g_types)) {
		g_warning ("Invalid description of plugin accepted types");
		return;
	}
	g_return_if_fail (plugin->entry_create_func || plugin->cell_create_func);
	
	np = g_new0 (GdauiPlugin, 1);
	np->plugin_name = g_strdup (plugin->plugin_name);
	if (plugin->plugin_descr)
		np->plugin_descr = g_strdup (plugin->plugin_descr);
	np->plugin_file = g_strdup (plugin->plugin_file);

	np->nb_g_types = plugin->nb_g_types;
	if (plugin->valid_g_types) {
		np->valid_g_types = g_new0 (GType, np->nb_g_types);
		memcpy (np->valid_g_types, plugin->valid_g_types, sizeof (GType) * np->nb_g_types);
	}

	if (plugin->options_xml_spec)
		np->options_xml_spec = g_strdup (plugin->options_xml_spec);
	np->entry_create_func = plugin->entry_create_func;
	np->cell_create_func = plugin->cell_create_func;

	g_hash_table_insert (gdaui_plugins_hash, np->plugin_name, np);
}

static GdauiDataEntry *entry_none_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_boolean_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_bin_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_string_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_number_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_time_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_timestamp_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GdauiDataEntry *entry_date_create_func (GdaDataHandler *handler, GType type, const gchar *options);

static GtkCellRenderer *cell_textual_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer *cell_boolean_create_func (GdaDataHandler *handler, GType type, const gchar *options);
static GtkCellRenderer *cell_bin_create_func (GdaDataHandler *handler, GType type, const gchar *options);

static xmlChar *get_spec_with_isocodes (const gchar *file);

static GHashTable *
init_plugins_hash (void)
{
	GHashTable *hash;
	GdauiPlugin *plugin;

	hash = g_hash_table_new (g_str_hash, g_str_equal); /* key strings are not handled in the hash table */

	/* default data entry widgets: they are not plugins but will be stored in GdauiPlugin structures */
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "none";
	plugin->plugin_descr = "Nothing displayed";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 0;
	plugin->valid_g_types = NULL;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_none_create_func;
	plugin->cell_create_func = NULL;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "boolean";
	plugin->plugin_descr = "Boolean entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_BOOLEAN;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_boolean_create_func;
	plugin->cell_create_func = cell_boolean_create_func;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "binary";
	plugin->plugin_descr = "Binary data entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 2;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = GDA_TYPE_BLOB;
	plugin->valid_g_types [1] = GDA_TYPE_BINARY;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_bin_create_func;
	plugin->cell_create_func = cell_bin_create_func;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);
       
	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "string";
	plugin->plugin_descr = "String entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_STRING;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_string_create_func;
	plugin->cell_create_func = cell_textual_create_func;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);
	GBytes *bytes;
	bytes = g_resources_lookup_data ("/gdaui/data-entries/gdaui-entry-string.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (bytes) {
		const char *data;
		data = (const char *) g_bytes_get_data (bytes, NULL);
		plugin->options_xml_spec = g_strdup (data);
		g_bytes_unref (bytes);
	}
	else
		g_message ("Could not load data entry specifications for the '%s' plugin, please report error to "
                           "http://gitlab.gnome.org/GNOME/libgda/issues", plugin->plugin_name);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "integer";
	plugin->plugin_descr = "Numeric integer entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 10;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_INT64;
	plugin->valid_g_types [1] = G_TYPE_UINT64;
	plugin->valid_g_types [3] = G_TYPE_INT;
	plugin->valid_g_types [4] = GDA_TYPE_SHORT;
	plugin->valid_g_types [5] = GDA_TYPE_USHORT;
	plugin->valid_g_types [6] = G_TYPE_CHAR;
	plugin->valid_g_types [7] = G_TYPE_UCHAR;
	plugin->valid_g_types [8] = G_TYPE_ULONG;
	plugin->valid_g_types [9] = G_TYPE_UINT;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_number_create_func;
	plugin->cell_create_func = cell_textual_create_func;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);

	bytes = g_resources_lookup_data ("/gdaui/data-entries/gdaui-entry-integer.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (bytes) {
		const char *data;
		data = (const char *) g_bytes_get_data (bytes, NULL);
		plugin->options_xml_spec = g_strdup (data);
		g_bytes_unref (bytes);
	}
	else
		g_message ("Could not load data entry specifications for the '%s' plugin, please report error to "
                           "http://gitlab.gnome.org/GNOME/libgda/issues", plugin->plugin_name);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "number";
	plugin->plugin_descr = "Numeric entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 3;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = GDA_TYPE_NUMERIC;
	plugin->valid_g_types [1] = G_TYPE_FLOAT;
	plugin->valid_g_types [2] = G_TYPE_DOUBLE;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_number_create_func;
	plugin->cell_create_func = cell_textual_create_func;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);

	bytes = g_resources_lookup_data ("/gdaui/data-entries/gdaui-entry-number.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (bytes) {
		const char *data;
		data = (const char *) g_bytes_get_data (bytes, NULL);
		xmlChar *xml_spec;
		xml_spec = get_spec_with_isocodes (data);
		plugin->options_xml_spec = g_strdup ((gchar *) xml_spec);
		g_bytes_unref (bytes);
		xmlFree (xml_spec);
	}
	else
		g_message ("Could not load data entry specifications for the '%s' plugin, please report error to "
                           "http://gitlab.gnome.org/GNOME/libgda/issues", plugin->plugin_name);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "time";
	plugin->plugin_descr = "Time (HH:MM:SS) entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = GDA_TYPE_TIME;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_time_create_func;
	plugin->cell_create_func = NULL;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "timestamp";
	plugin->plugin_descr = "Timestamp (Date + HH:MM:SS) entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_DATE_TIME;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_timestamp_create_func;
	plugin->cell_create_func = NULL;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);

	plugin = g_new0 (GdauiPlugin, 1);
	plugin->plugin_name = "date";
	plugin->plugin_descr = "Date entry";
	plugin->plugin_file = NULL;
	plugin->nb_g_types = 1;
	plugin->valid_g_types = g_new (GType, plugin->nb_g_types);
	plugin->valid_g_types [0] = G_TYPE_DATE;
	plugin->options_xml_spec = NULL;
	plugin->entry_create_func = entry_date_create_func;
	plugin->cell_create_func = NULL;
	g_hash_table_insert (hash, plugin->plugin_name, plugin);
	
	/* plugins */
	GDir *dir;
	GError *err = NULL;
	gchar *plugins_dir;
	gboolean show_status = FALSE;
	
	/* read the plugin directory */
	plugins_dir = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "plugins", NULL);
	if (g_getenv ("GDAUI_SHOW_PLUGINS_LOADING"))
		show_status = TRUE;
	if (show_status)
		g_print ("Trying to load plugins in %s...\n", plugins_dir);
	dir = g_dir_open (plugins_dir, 0, NULL);
	if (!dir) {
		g_free (plugins_dir);
		plugins_dir = g_strdup (PLUGINSDIR);
		if (show_status)
			g_print ("Trying to load plugins in %s...\n", plugins_dir);
		dir = g_dir_open (plugins_dir, 0, NULL);
	}
	if (!dir && show_status)
		g_warning (_("Could not open plugins directory, no plugin loaded."));
	else {
		const gchar *name;

		while ((name = g_dir_read_name (dir))) {
			gchar *ext;
			GModule *handle;
			gchar *path;
			GdauiPluginInit plugin_init;
			GSList *plugins;

			ext = g_strrstr (name, ".");
			if (!ext)
				continue;
			if (strcmp (ext + 1, G_MODULE_SUFFIX))
				continue;

			path = g_build_path (G_DIR_SEPARATOR_S, plugins_dir, name, NULL);
			handle = g_module_open (path, G_MODULE_BIND_LAZY);
			if (!handle) {
				g_warning (_("Error: %s"), g_module_error ());
				g_free (path);
				continue;
			}

			g_module_symbol (handle, "plugin_init", (gpointer*) &plugin_init);
			if (plugin_init) {
				if (show_status)
					g_print (_("Loading file %s...\n"), path);
				plugins = plugin_init (&err);
				if (err) {
					if (show_status)
						g_message (_("Plugins load warning: %s"),
							   err->message ? err->message : _("No detail"));
					if (err)
						g_error_free (err);
					err = NULL;
				}

				GSList *list;
				for (list = plugins; list; list = list->next) {
					GdauiPlugin *plugin;
					
					plugin = (GdauiPlugin *)(list->data);
					g_hash_table_insert (hash, plugin->plugin_name, plugin);
					if (show_status) {
						g_print ("  - loaded %s (%s):", plugin->plugin_name,
							 plugin->plugin_descr);
						if (plugin->entry_create_func)
							g_print (" Entry");
						if (plugin->cell_create_func)
							g_print (" Cell");
						g_print ("\n");
					}
					plugin->plugin_file = g_strdup (path);
				}
				g_slist_free (plugins);
			}
			g_free (path);
		}
		g_dir_close (dir);
	}
	g_free (plugins_dir);

	return hash;
}

static GdauiDataEntry *
entry_none_create_func (G_GNUC_UNUSED GdaDataHandler *handler, GType type, G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_none_new (type);
}

static GdauiDataEntry *
entry_boolean_create_func (GdaDataHandler *handler, G_GNUC_UNUSED GType type,
			   G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_boolean_new (handler, G_TYPE_BOOLEAN);
}

static GdauiDataEntry *
entry_bin_create_func (GdaDataHandler *handler, GType type, G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_bin_new (handler, type);
}

static GdauiDataEntry *
entry_string_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_string_new (handler, type, options);
}

static GdauiDataEntry *
entry_number_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_number_new (handler, type, options);
}

static GdauiDataEntry *
entry_time_create_func (GdaDataHandler *handler, G_GNUC_UNUSED GType type, G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_time_new (handler);
}

static GdauiDataEntry *
entry_timestamp_create_func (GdaDataHandler *handler, G_GNUC_UNUSED GType type,
			     G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_timestamp_new (handler);
}

static GdauiDataEntry *
entry_date_create_func (GdaDataHandler *handler, G_GNUC_UNUSED GType type, G_GNUC_UNUSED const gchar *options)
{
	return (GdauiDataEntry *) gdaui_entry_date_new (handler);
}

static GtkCellRenderer *
cell_textual_create_func (GdaDataHandler *handler, GType type, const gchar *options)
{
	return gdaui_data_cell_renderer_textual_new (handler, type, options);
}

static GtkCellRenderer *
cell_boolean_create_func (GdaDataHandler *handler, G_GNUC_UNUSED GType type,
			  G_GNUC_UNUSED const gchar *options)
{
	return gdaui_data_cell_renderer_boolean_new (handler, G_TYPE_BOOLEAN);
}

static GtkCellRenderer *
cell_bin_create_func (GdaDataHandler *handler, GType type, G_GNUC_UNUSED const gchar *options)
{
	return gdaui_data_cell_renderer_bin_new (handler, type);
}

static xmlNodePtr
find_child_node_from_name (xmlNodePtr parent, const gchar *name, const gchar *attr_name, const gchar *attr_value)
{
	xmlNodePtr node;

	if (!parent)
		return NULL;

	for (node = parent->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, name)) {
			if (attr_name) {
				xmlChar *prop;
				prop = xmlGetProp (node, BAD_CAST attr_name);
				if (prop) {
					if (attr_value && !strcmp ((gchar*) prop, attr_value)) {
						xmlFree (prop);
						break;
					}
					xmlFree (prop);
				}
			}
			else
				break;
		}
	}
	if (!node) 
		g_warning ("Failed to find the <%s> tag", name);

	return node;
}

static xmlChar *
get_spec_with_isocodes (const gchar *data_spec)
{
	xmlDocPtr spec, isocodes = NULL;
	xmlChar *retval = NULL;
	gchar *isofile = NULL;
	GError *err = NULL;
	gchar  *buf = NULL;
	int   buf_len;
	
	/*
	 * Load iso codes
	 */
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"

	bindtextdomain ("iso_4217", ISO_CODES_LOCALESDIR);
	bind_textdomain_codeset ("iso_4217", "UTF-8");

	isofile = g_build_filename (ISO_CODES_PREFIX, "share", "xml", "iso-codes", "iso_4217.xml", NULL);
	if (g_file_get_contents (isofile, &buf, NULL, &err)) {
		isocodes = xmlParseDoc (BAD_CAST buf);
		g_free (buf);
		buf = NULL;
	} 

	/* 
	 * Load spec string 
	 */
	spec = xmlParseDoc (BAD_CAST data_spec);
	if (!spec) {
		g_warning ("Can't parse XML data, please report error to "
                           "http://gitlab.gnome.org/GNOME/libgda/issues");
		goto cleanup;
	}

	if (isocodes) {
		/*
		 * Merge isocodes into spec
		 */
		xmlNodePtr node, spec_node;
		
		node = find_child_node_from_name (xmlDocGetRootElement (spec), "sources", NULL, NULL);
		node = find_child_node_from_name (node, "gda_array", "name", "currencies");
		spec_node = find_child_node_from_name (node, "gda_array_data", NULL, NULL);
		xmlUnlinkNode (spec_node);
		xmlFreeNode (spec_node);
		spec_node = xmlNewChild (node, NULL, BAD_CAST "gda_array_data", NULL);
		
		node = xmlDocGetRootElement (isocodes);
		for (node = node->children; node; node = node->next) {
			if (!strcmp ((gchar*) node->name, "iso_4217_entry")) {
				xmlChar *code, *name;
				code = xmlGetProp (node, BAD_CAST "letter_code");
				name = xmlGetProp (node, BAD_CAST "currency_name");
				if (code && name) {
					xmlNodePtr row;
					row = xmlNewChild (spec_node, NULL, BAD_CAST "gda_array_row", NULL);
					xmlNewChild (row, NULL, BAD_CAST "gda_value", code);
					xmlNewChild (row, NULL, BAD_CAST "gda_value", code);
					xmlNewChild (row, NULL, BAD_CAST "gda_value",
						     BAD_CAST dgettext ("iso_4217", (gchar*) name));
				}
				if (code)
					xmlFree (code);
				if (name)
					xmlFree (name);
			}
		}
	}
	else {
		/*
		 * No ISO CODES found => no predefined source
		 */
		xmlNodePtr node;
		node = find_child_node_from_name (xmlDocGetRootElement (spec), "sources", NULL, NULL);
		node = find_child_node_from_name (node, "gda_array", "name", "currencies");
		xmlUnlinkNode (node);
		xmlFreeNode (node);

		node = find_child_node_from_name (xmlDocGetRootElement (spec), "parameters", NULL, NULL);
		node = find_child_node_from_name (node, "parameter", "id", "CURRENCY");
		xmlSetProp (node, BAD_CAST "source", NULL);
	}

	xmlDocDumpMemory (spec, (xmlChar **) &retval, &buf_len);

 cleanup:
	if (spec)
		xmlFreeDoc (spec);
	if (isocodes)
		xmlFreeDoc (isocodes);
	g_free (isofile);
	g_free (buf);

	return retval;
}

static gchar *gdaui_path = NULL;

/**
 * gdaui_get_default_path:
 *
 * Get the default path used when saving a file, or when showing a #GtkFileChooser file chooser.
 * When the application starts, the default path will be the same as the onde returned by
 * g_get_current_dir().
 *
 * Returns: (transfer none): the default path, or %NULL
 *
 * Since: 4.2.9
 */
const gchar *
gdaui_get_default_path (void)
{
	if (! gdaui_path)
		gdaui_path = g_get_current_dir ();
	return gdaui_path;
}

/**
 * gdaui_set_default_path:
 * @path: (nullable): a path, or %NULL to unset
 *
 * Define the default path used when saving a file, or when showing a #GtkFileChooser file chooser.
 *
 * Since: 4.2.9
 */
void
gdaui_set_default_path (const gchar *path)
{
	g_free (gdaui_path);
	gdaui_path = NULL;
	if (path)
		gdaui_path = g_strdup (path);
}

static guint
nocase_hash (gconstpointer key)
{
	gchar *cmp;
	cmp = g_utf8_casefold ((const gchar*) key, -1);
	guint retval;
	retval = g_str_hash (cmp);
	g_free (cmp);
	return retval;
}

static gboolean
nocase_equal (gconstpointer a, gconstpointer b)
{
	gchar *ca, *cb;
	ca = g_utf8_casefold ((const gchar*) a, -1);
	cb = g_utf8_casefold ((const gchar*) b, -1);

	gboolean retval;
	retval = g_str_equal (ca, cb);
	g_free (ca);
	g_free (cb);
	return retval;
}

/**
 * gdaui_get_icon_for_db_engine:
 * @engine: a database engine (case non sensitive), like "PostgreSQL", "MySQL", etc.
 *
 * Get an icon for the database engine.
 *
 * Returns: (transfer none): a #GdkPixbuf, or %NULL if none found
 *
 * Since: 6.0
 */
GdkPixbuf *
gdaui_get_icon_for_db_engine (const gchar *engine)
{
	g_return_val_if_fail (engine && *engine, NULL);
	static GHashTable *icons_hash = NULL;
	GdkPixbuf *pix;

	if (! icons_hash) {
		icons_hash = g_hash_table_new_full (nocase_hash, nocase_equal, g_free, g_object_unref);
		/* add icons from resources */
		gchar **dbs;
#define RESOURCES_PREFIX "/gdaui/db-engines/data"
		dbs = g_resources_enumerate_children (RESOURCES_PREFIX, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
		if (dbs) {
			guint i;
			for (i = 0; dbs[i]; i++) {
				/* dbs[i] is for example "Firebird.png", we need to grab the key from that */
				gchar *path;
				path = g_strdup_printf ("%s/%s", RESOURCES_PREFIX, dbs[i]);
				pix = gdk_pixbuf_new_from_resource (path, NULL);
				g_free (path);
				if (pix) {
					gchar *tmp;
					gint len;
					len = strlen (dbs[i]) - 4;
					g_assert (len > 0);
					tmp = dbs[i] + len;
					g_assert (! strcmp (tmp, ".png"));
					if (tmp > dbs[i]) {
						gchar *key, *tmp;
						key = g_strdup (dbs[i]);
						for (tmp = key + len;
						     (tmp > key + 4) || ((len <= 4) && (tmp == key + len));
						     tmp --) {
							*tmp = 0;
							if (g_hash_table_lookup (icons_hash, key)) {
								g_hash_table_remove (icons_hash, key);
								break;
							}
							/*g_print ("Inserted '%s'\n", key);*/
							g_hash_table_insert (icons_hash, g_strdup (key), g_object_ref (pix));
						}
						g_free (key);
					}
					g_object_unref (pix);
				}
				else
					g_warning ("Could not load GdkPixbuf for resource %s", dbs[i]);
			}
			g_strfreev (dbs);
		}
		else
			g_warning ("Could not locate any DB engine resource");
	}

	gchar *ekey;
	ekey = g_utf8_casefold (engine, -1);
	pix = g_hash_table_lookup (icons_hash, ekey);
	g_free (ekey);
	return pix;
}
