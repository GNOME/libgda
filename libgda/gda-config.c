/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <config.h>
#include <glib/gdir.h>
#include <gmodule.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <gconf/gconf-client.h>
#include <string.h>

static GConfClient *conf_client = NULL;

/*
 * Private functions
 */

static GConfClient *
get_conf_client (void)
{
        if (!conf_client) {
                /* initialize GConf */
                if (!gconf_is_initialized ())
                        gconf_init (0, NULL, NULL);
                conf_client = gconf_client_get_default ();
        }
        return conf_client;
}

/**
 * gda_config_get_string
 * @path: path to the configuration entry
 *
 * Gets the value of the specified configuration entry as a string. You
 * are then responsible to free the returned string
 *
 * Returns: the value stored at the given entry
 */
gchar *
gda_config_get_string (const gchar *path)
{
        return gconf_client_get_string (get_conf_client (), path, NULL);
}

/**
 * gda_config_get_int
 * @path: path to the configuration entry
 *
 * Gets the value of the specified configuration entry as an integer
 *
 * Returns: the value stored at the given entry
 */
gint
gda_config_get_int (const gchar *path)
{
        return gconf_client_get_int (get_conf_client (), path, NULL);
}

/**
 * gda_config_get_float
 * @path: path to the configuration entry
 *
 * Gets the value of the specified configuration entry as a float
 *
 * Returns: the value stored at the given entry
 */
gdouble
gda_config_get_float (const gchar *path)
{
        return gconf_client_get_float (get_conf_client (), path, NULL);
}

/**
 * gda_config_get_boolean
 * @path: path to the configuration entry
 *
 * Gets the value of the specified configuration entry as a boolean
 *
 * Returns: the value stored at the given entry
 */
gboolean
gda_config_get_boolean (const gchar *path)
{
        return gconf_client_get_bool (get_conf_client (), path, NULL);
}

/**
 * gda_config_set_string
 * @path: path to the configuration entry
 * @new_value: new value
 *
 * Sets the given configuration entry to contain a string
 */
void
gda_config_set_string (const gchar *path, const gchar *new_value)
{
        gconf_client_set_string (get_conf_client (), path, new_value, NULL);
}

/**
 * gda_config_set_int
 * @path: path to the configuration entry
 * @new_value: new value
 *
 * Sets the given configuration entry to contain an integer
 */
void
gda_config_set_int (const gchar *path, gint new_value)
{
        gconf_client_set_int (get_conf_client (), path, new_value, NULL);
}

/**
 * gda_config_set_float
 * @path: path to the configuration entry
 * @new_value: new value
 *
 * Sets the given configuration entry to contain a float
 */
void
gda_config_set_float (const gchar * path, gdouble new_value)
{
        gconf_client_set_float (get_conf_client (), path, new_value, NULL);
}

/**
 * gda_config_set_boolean
 * @path: path to the configuration entry
 * @new_value: new value
 *
 * Sets the given configuration entry to contain a boolean
 */
void
gda_config_set_boolean (const gchar *path, gboolean new_value)
{
        g_return_if_fail (path != NULL);
        gconf_client_set_bool (get_conf_client (), path, new_value, NULL);
}

/**
 * gda_config_remove_section
 * @path: path to the configuration section
 *
 * Remove the given section from the configuration database
 */
void
gda_config_remove_section (const gchar *path)
{
        g_return_if_fail (path != NULL);
	/* FIXME: see bug #73323 */
        //gconf_client_recursive_unset (get_conf_client (), path, 0, NULL);
}

/**
 * gda_config_remove_key
 * @path: path to the configuration entry
 *
 * Remove the given entry from the configuration database
 */
void
gda_config_remove_key (const gchar *path)
{
        gconf_client_unset (get_conf_client (), path, NULL);
}

/**
 * gda_config_has_section
 * @path: path to the configuration section
 *
 * Checks whether the given section exists in the configuration
 * system
 *
 * Returns: TRUE if the section exists, FALSE otherwise
 */
gboolean
gda_config_has_section (const gchar *path)
{
        return gconf_client_dir_exists (get_conf_client (), path, NULL);
}

/**
 * gda_config_has_key
 * @path: path to the configuration key
 *
 * Check whether the given key exists in the configuration system
 *
 * Returns: TRUE if the entry exists, FALSE otherwise
 */
gboolean
gda_config_has_key (const gchar *path)
{
        GConfValue *value;

        g_return_val_if_fail (path != NULL, FALSE);

        value = gconf_client_get (get_conf_client (), path, NULL);
        if (value) {
                gconf_value_free (value);
                return TRUE;
        }
        return FALSE;
}

/**
 * gda_config_list_sections
 * @path: path for root dir
 *
 * Return a GList containing the names of all the sections available
 * under the given root directory.
 *
 * To free the returned value, you can use #gda_config_free_list
 *
 * Returns: a list containing all the section names
 */
GList *
gda_config_list_sections (const gchar *path)
{
        GList *ret = NULL;
        GSList *slist;

        g_return_val_if_fail (path != NULL, NULL);

        slist = gconf_client_all_dirs (get_conf_client (), path, NULL);
        if (slist) {
                GSList *node;

                for (node = slist; node != NULL; node = g_slist_next (node)) {
                        gchar *section_name;

			section_name = strrchr ((const char *) node->data, '/');
                        if (section_name) {
                                ret = g_list_append (ret, g_strdup (section_name + 1));
                        }
                }
                g_slist_free (slist);
        }
        return ret;
}

/**
 * gda_config_list_keys
 * @path: path for root dir
 *
 * Returns a list of all keys that exist under the given path.
 *
 * To free the returned value, you can use #gda_config_free_list
 *
 * Returns: a list containing all the key names
 */
GList *
gda_config_list_keys (const gchar * path)
{
        GList *ret = NULL;
        GSList *slist;

        g_return_val_if_fail (path != NULL, NULL);

        slist = gconf_client_all_entries (get_conf_client (), path, NULL);
        if (slist) {
                GSList *node;

                for (node = slist; node != NULL; node = g_slist_next (node)) {
                        GConfEntry *entry = (GConfEntry *) node->data;
                        if (entry) {
                                gchar *entry_name;

                                entry_name = strrchr (
					(const char *) gconf_entry_get_key (entry),
					'/');
                                if (entry_name) {
                                        ret = g_list_append (
						ret,
						g_strdup (entry_name + 1));
                                }
                                gconf_entry_free (entry);
                        }
                }
                g_slist_free (slist);
        }
        return ret;
}

/**
 * gda_config_free_list
 * @list: list to be freed
 *
 * Free all memory used by the given GList, which must be the return value
 * from either #gda_config_list_sections and #gda_config_list_keys
 */
void
gda_config_free_list (GList * list)
{
        while (list != NULL) {
                gchar *str = (gchar *) list->data;
                list = g_list_remove (list, (gpointer) str);
                g_free ((gpointer) str);
        }
}

typedef struct {
	guint id;
	GdaConfigListenerFunc func;
	gpointer user_data;
} config_listener_data_t;

static void
config_listener_func (GConfClient *conf, guint id, GConfEntry *entry, gpointer user_data)
{
	config_listener_data_t *listener_data = (config_listener_data_t *) user_data;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (listener_data != NULL);
	g_return_if_fail (listener_data->func != NULL);

	listener_data->func (entry->key, listener_data->user_data);
}

/**
 * gda_config_add_listener
 * @path: configuration path to listen to.
 * @func: callback function.
 * @user_data: data to be passed to the callback function.
 *
 * Installs a configuration listener, which is a callback function
 * which will be called every time a change occurs on a given
 * configuration entry.
 *
 * Returns: the ID of the listener, which you will need for
 * calling #gda_config_remove_listener. If an error occurs,
 * 0 is returned.
 */
guint
gda_config_add_listener (const gchar *path, GdaConfigListenerFunc func, gpointer user_data)
{
	config_listener_data_t *listener_data;
	GError *err;

	g_return_val_if_fail (path != NULL, 0);
	g_return_val_if_fail (func != NULL, 0);

	listener_data = g_new0 (config_listener_data_t, 1);
	listener_data->func = func;
	listener_data->user_data = user_data;

	gconf_client_add_dir (get_conf_client (), path, GCONF_CLIENT_PRELOAD_NONE, NULL);
	listener_data->id = gconf_client_notify_add (
		get_conf_client (),
		path,
		(GConfClientNotifyFunc) config_listener_func,
		listener_data,
		(GFreeFunc) g_free,
		&err);
	if (listener_data->id == 0) {
		g_free (listener_data);
		return 0;
	}

	return listener_data->id;
}

/**
 * gda_config_remove_listener
 */
void
gda_config_remove_listener (guint id)
{
	/* FIXME: remove directory from GConf list of watched dirs */
	gconf_client_notify_remove (get_conf_client (), id);
}

/**
 * gda_config_get_provider_list
 *
 * Return a list of all providers currently installed in
 * the system. Each of the nodes in the returned GList
 * is a #GdaProviderInfo. To free the returned list,
 * call the #gda_config_free_provider_list function.
 *
 * Returns: a GList of #GdaProviderInfo structures.
 */
GList *
gda_config_get_provider_list (void)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *name;
	GList *list = NULL;

	/* read the plugin directory */
	dir = g_dir_open (LIBGDA_PLUGINDIR, 0, &err);
	if (err) {
		gda_log_error (err->message);
		g_error_free (err);
		return NULL;
	}

	while ((name = g_dir_read_name (dir))) {
		GdaProviderInfo *info;
		GModule *handle;
		gchar *path;
		const gchar * (* plugin_get_name) (void);
		const gchar * (* plugin_get_description) (void);
		GList * (* plugin_get_cnc_params) (void);

		path = g_build_path (G_DIR_SEPARATOR_S, LIBGDA_PLUGINDIR, name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY);
		if (!handle) {
			g_free (path);
			continue;
		}

		g_module_symbol (handle, "plugin_get_name",
				 (gpointer) &plugin_get_name);
		g_module_symbol (handle, "plugin_get_description",
				 (gpointer) &plugin_get_description);
		g_module_symbol (handle, "plugin_get_connection_params",
				 (gpointer) &plugin_get_cnc_params);

		info = g_new0 (GdaProviderInfo, 1);
		info->location = path;

		if (plugin_get_name != NULL)
			info->id = g_strdup (plugin_get_name ());
		else
			info->id = g_strdup (name);

		if (plugin_get_description != NULL)
			info->description = g_strdup (plugin_get_description ());
		else
			info->description = NULL;

		if (plugin_get_cnc_params != NULL)
			info->gda_params = plugin_get_cnc_params ();
		else
			info->gda_params = NULL;

		list = g_list_append (list, info);

		g_module_close (handle);
	}

	/* free memory */
	g_dir_close (dir);

	return list;
}

/**
 * gda_config_free_provider_list
 * @list: the list to be freed.
 *
 * Free a list of #GdaProviderInfo structures.
 */
void
gda_config_free_provider_list (GList *list)
{
	GList *l;

	for (l = g_list_first (list); l; l = l->next) {
		GdaProviderInfo *provider_info = (GdaProviderInfo *) l->data;

		if (provider_info != NULL)
			gda_config_free_provider_info (provider_info);
	}

	g_list_free (list);
}

/**
 * gda_config_get_provider_by_name
 */
GdaProviderInfo *
gda_config_get_provider_by_name (const gchar *name)
{
	GList *prov_list;
	GList *l;

	prov_list = gda_config_get_provider_list ();

	for (l = prov_list; l != NULL; l = l->next) {
		GdaProviderInfo *provider_info = (GdaProviderInfo *) l->data;

		if (provider_info && !strcmp (provider_info->id, name)) {
			l->data = NULL;
			gda_config_free_provider_list (prov_list);

			return provider_info;
		}
	}

	gda_config_free_provider_list (prov_list);

	return NULL;
}

/**
 * gda_config_free_provider_info
 */
void
gda_config_free_provider_info (GdaProviderInfo *provider_info)
{
	g_return_if_fail (provider_info != NULL);

	g_free (provider_info->id);
	g_free (provider_info->location);
	g_free (provider_info->description);
	g_list_foreach (provider_info->gda_params, (GFunc) g_free, NULL);
	g_list_free (provider_info->gda_params);

	g_free (provider_info);
}

/**
 * gda_config_get_data_source_list
 */
GList *
gda_config_get_data_source_list (void)
{
	GList *list = NULL;
	GList *sections;
	GList *l;

	sections = gda_config_list_sections (GDA_CONFIG_SECTION_DATASOURCES);
	for (l = sections; l != NULL; l = l->next) {
		gchar *tmp;
		GdaDataSourceInfo *info;

		info = g_new0 (GdaDataSourceInfo, 1);
		info->name = g_strdup ((const gchar *) l->data);

		/* get the provider */
		tmp = g_strdup_printf ("%s/%s/Provider", GDA_CONFIG_SECTION_DATASOURCES, (char *) l->data);
		info->provider = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the connection string */
		tmp = g_strdup_printf ("%s/%s/DSN", GDA_CONFIG_SECTION_DATASOURCES, (char *) l->data);
		info->cnc_string = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the description */
		tmp = g_strdup_printf ("%s/%s/Description", GDA_CONFIG_SECTION_DATASOURCES, (char *) l->data);
		info->description = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the user name */
		tmp = g_strdup_printf ("%s/%s/Username", GDA_CONFIG_SECTION_DATASOURCES, (char *) l->data);
		info->username = gda_config_get_string (tmp);
		g_free (tmp);

		list = g_list_append (list, info);
	}

	gda_config_free_list (sections);

	return list;
}

/**
 * gda_config_find_data_source
 */
GdaDataSourceInfo *
gda_config_find_data_source (const gchar *name)
{
	GList *dsnlist;
	GList *l;
	GdaDataSourceInfo *info = NULL;

	g_return_val_if_fail (name != NULL, NULL);

	dsnlist = gda_config_get_data_source_list ();
	for (l = dsnlist; l != NULL; l = l->next) {
		GdaDataSourceInfo *tmp_info = (GdaDataSourceInfo *) l->data;

		if (tmp_info && !g_strcasecmp (tmp_info->name, name)) {
			info = gda_config_copy_data_source_info (tmp_info);
			break;
		}
	}

	gda_config_free_data_source_list (dsnlist);

	return info;
}

/**
 * gda_config_copy_data_source_info
 */
GdaDataSourceInfo *
gda_config_copy_data_source_info (GdaDataSourceInfo *src)
{
	GdaDataSourceInfo *info;

	g_return_val_if_fail (src != NULL, NULL);

	info = g_new0 (GdaDataSourceInfo, 1);
	info->name = g_strdup (src->name);
	info->provider = g_strdup (src->provider);
	info->cnc_string = g_strdup (src->cnc_string);
	info->description = g_strdup (src->description);
	info->username = g_strdup (src->username);

	return info;
}

/**
 * gda_config_free_data_source_info
 */
void
gda_config_free_data_source_info (GdaDataSourceInfo *info)
{
	g_return_if_fail (info != NULL);

	g_free (info->name);
	g_free (info->provider);
	g_free (info->cnc_string);
	g_free (info->description);
	g_free (info->username);

	g_free (info);
}

/**
 * gda_config_free_data_source_list
 */
void
gda_config_free_data_source_list (GList *list)
{
	GList *l;

	while ((l = g_list_first (list))) {
		GdaDataSourceInfo *info = (GdaDataSourceInfo *) l->data;

		list = g_list_remove (list, info);
		gda_config_free_data_source_info (info);
	}
}

/**
 * gda_config_get_data_source_model
 */
GdaDataModel *
gda_config_get_data_source_model (void)
{
	GList *dsn_list;
	GList *l;
	GdaDataModel *model;

	model = gda_data_model_array_new (5);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("Provider"));
	gda_data_model_set_column_title (model, 2, _("Connection string"));
	gda_data_model_set_column_title (model, 3, _("Description"));
	gda_data_model_set_column_title (model, 4, _("Username"));
	
	/* convert the returned GList into a GdaDataModelArray */
	dsn_list = gda_config_get_data_source_list ();
	for (l = dsn_list; l != NULL; l = l->next) {
		GList *value_list = NULL;
		GdaDataSourceInfo *dsn_info = (GdaDataSourceInfo *) l->data;

		g_assert (dsn_info != NULL);

		value_list = g_list_append (value_list, gda_value_new_string (dsn_info->name));
		value_list = g_list_append (value_list, gda_value_new_string (dsn_info->provider));
		value_list = g_list_append (value_list, gda_value_new_string (dsn_info->cnc_string));
		value_list = g_list_append (value_list, gda_value_new_string (dsn_info->description));
		value_list = g_list_append (value_list, gda_value_new_string (dsn_info->username));

		gda_data_model_array_append_row (GDA_DATA_MODEL_ARRAY (model), value_list);
	}
	
	/* free memory */
	gda_config_free_data_source_list (dsn_list);
	
	return model;
}

/**
 * gda_config_save_data_source
 * @name: Name for the data source to be saved.
 * @provider: Provider ID for the new data source.
 * @cnc_string: Connection string for the new data source.
 * @description: Description for the new data source.
 * @username: User name for the new data source.
 *
 * Adds a new data source (or update an existing one) to the GDA
 * configuration, based on the parameters given.
 */
void
gda_config_save_data_source (const gchar *name,
			     const gchar *provider,
			     const gchar *cnc_string,
			     const gchar *description,
			     const gchar *username)
{
	gchar *tmp;

	g_return_if_fail (name != NULL);
	g_return_if_fail (provider != NULL);

	/* set the provider */
	tmp = g_strdup_printf ("%s/%s/Provider", GDA_CONFIG_SECTION_DATASOURCES, name);
	gda_config_set_string (tmp, provider);
	g_free (tmp);

	/* set the connection string */
	if (cnc_string) {
		tmp = g_strdup_printf ("%s/%s/DSN", GDA_CONFIG_SECTION_DATASOURCES, name);
		gda_config_set_string (tmp, cnc_string);
		g_free (tmp);
	}

	/* set the description */
	if (description) {
		tmp = g_strdup_printf ("%s/%s/Description", GDA_CONFIG_SECTION_DATASOURCES, name);
		gda_config_set_string (tmp, description);
		g_free (tmp);
	}

	/* set the username */
	if (cnc_string) {
		tmp = g_strdup_printf ("%s/%s/Username", GDA_CONFIG_SECTION_DATASOURCES, name);
		gda_config_set_string (tmp, username);
		g_free (tmp);
	}
}

/**
 * gda_config_remove_data_source
 * @name: Name for the data source to be removed.
 *
 * Removes the given data source from the GDA configuration.
 */
void
gda_config_remove_data_source (const gchar *name)
{
	gchar *dir;

	g_return_if_fail (name != NULL);

	dir = g_strdup_printf ("%s/%s", GDA_CONFIG_SECTION_DATASOURCES, name);
	gda_config_remove_section (dir);
	g_free (dir);
}
