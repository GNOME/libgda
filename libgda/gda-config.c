/* GDA common library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-log.h>
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo-activation/bonobo-activation-server-info.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <gconf/gconf.h>
#include <string.h>

#define GDA_CONFIG_SECTION_DATASOURCES       "/apps/libgda/Datasources"
#define GDA_CONFIG_SECTION_LAST_CONNECTIONS  "/apps/libgda/LastConnections"

#define GDA_CONFIG_KEY_MAX_LAST_CONNECTIONS  "/apps/libgda/MaxLastConnections"

static GList *activation_property_to_list (Bonobo_ActivationProperty *prop);
static GdaParameter *activation_property_to_parameter (Bonobo_ActivationProperty *prop);
static gchar *activation_property_to_string (Bonobo_ActivationProperty *prop);

static GConfEngine *conf_engine = NULL;

/*
 * Private functions
 */

static GList *
activation_property_to_list (Bonobo_ActivationProperty *prop)
{
	GList *list = NULL;

	g_return_val_if_fail (prop != NULL, NULL);

	if (prop->v._d == Bonobo_ACTIVATION_P_STRING)
		list = g_list_append (list, g_strdup (prop->v._u.value_string));
	else if (prop->v._d == Bonobo_ACTIVATION_P_STRINGV) {
		gint j;
		Bonobo_StringList strlist = prop->v._u.value_stringv;

		for (j = 0; j < strlist._length; j++) {
			gchar *str = g_strdup (strlist._buffer[j]);
			list = g_list_append (list, str);
		}
	}

	return list;
}

static GdaParameter *
activation_property_to_parameter (Bonobo_ActivationProperty *prop)
{
	GdaParameter *param;
	gchar *str;

	g_return_val_if_fail (prop != NULL, NULL);

	switch (prop->v._d) {
	case Bonobo_ACTIVATION_P_STRING :
		param = gda_parameter_new ((const gchar *) prop->name, GDA_VALUE_TYPE_STRING);
		gda_value_set_string (gda_parameter_get_value (param),
				      prop->v._u.value_string);
		break;
	case Bonobo_ACTIVATION_P_NUMBER :
		param = gda_parameter_new ((const gchar *) prop->name, GDA_VALUE_TYPE_DOUBLE);
		gda_value_set_double (gda_parameter_get_value (param),
				      prop->v._u.value_number);
		break;
	case Bonobo_ACTIVATION_P_BOOLEAN :
		param = gda_parameter_new ((const gchar *) prop->name, GDA_VALUE_TYPE_BOOLEAN);
		gda_value_set_boolean (gda_parameter_get_value (param),
				       prop->v._u.value_boolean);
		break;
	case Bonobo_ACTIVATION_P_STRINGV :
		param = gda_parameter_new ((const gchar *) prop->name, GDA_VALUE_TYPE_STRING);
		str = activation_property_to_string (prop);
		if (str) {
			gda_value_set_string (gda_parameter_get_value (param), str);
			g_free (str);
		}
		break;
	default :
		param = NULL;
	}

	return param;
}

static gchar *
activation_property_to_string (Bonobo_ActivationProperty *prop)
{
	g_return_val_if_fail (prop != NULL, NULL);

	if (prop->v._d == Bonobo_ACTIVATION_P_STRING)
		return g_strdup (prop->v._u.value_string);
	else if (prop->v._d == Bonobo_ACTIVATION_P_STRINGV) {
		gint j;
		GString *str = NULL;
		Bonobo_StringList strlist = prop->v._u.value_stringv;

		for (j = 0; j < strlist._length; j++) {
			if (!str)
				str = g_string_new (strlist._buffer[j]);
			else {
				str = g_string_append (str, ";");
				str = g_string_append (str, strlist._buffer[j]);
			}
		}
		if (str) {
			gchar *ret = g_strdup (str->str);
			g_string_free (str, TRUE);
			return ret;
		}
	}

	return NULL;
}

static GConfEngine *
get_conf_engine (void)
{
        if (!conf_engine) {
                /* initialize GConf */
                if (!gconf_is_initialized ())
                        gconf_init (0, NULL, NULL);
                conf_engine = gconf_engine_get_default ();
        }
        return conf_engine;
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
        return gconf_engine_get_string (get_conf_engine (), path, NULL);
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
        return gconf_engine_get_int (get_conf_engine (), path, NULL);
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
        return gconf_engine_get_float (get_conf_engine (), path, NULL);
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
        return gconf_engine_get_bool (get_conf_engine (), path, NULL);
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
        gconf_engine_set_string (get_conf_engine (), path, new_value, NULL);
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
        gconf_engine_set_int (get_conf_engine (), path, new_value, NULL);
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
        gconf_engine_set_float (get_conf_engine (), path, new_value, NULL);
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
        gconf_engine_set_bool (get_conf_engine (), path, new_value, NULL);
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
        gconf_engine_remove_dir (get_conf_engine (), path, NULL);
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
        gconf_engine_unset (get_conf_engine (), path, NULL);
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
        return gconf_engine_dir_exists (get_conf_engine (), path, NULL);
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

        value = gconf_engine_get (get_conf_engine (), path, NULL);
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

        slist = gconf_engine_all_dirs (get_conf_engine (), path, NULL);
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

        slist = gconf_engine_all_entries (get_conf_engine (), path, NULL);
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

static GList *config_listeners = NULL;

static void
config_listener_func (GConfEngine *conf, guint id, GConfEntry *entry, gpointer user_data)
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

	listener_data->id = gconf_engine_notify_add (get_conf_engine (),
						     path,
						     (GConfNotifyFunc) config_listener_func,
						     listener_data->user_data,
						     &err);
	if (listener_data->id == 0) {
		g_free (listener_data);
		return 0;
	}

	config_listeners = g_list_prepend (config_listeners, listener_data);

	return listener_data->id;
}

/**
 * gda_config_remove_listener
 */
void
gda_config_remove_listener (guint id)
{
	GList *l;

	for (l = config_listeners; l != NULL; l = l->next) {
		config_listener_data_t *listener_data = l->data;

		if (!listener_data)
			continue;

		if (listener_data->id == id) {
			gconf_engine_notify_remove (get_conf_engine (), id);
			config_listeners = g_list_remove (config_listeners, listener_data);
			g_free (listener_data);
			break;
		}
	}
}

/**
 * gda_config_get_component_list
 * @query: condition for components to be retrieved.
 *
 * Return a list of all components currently installed in
 * the system that match the given query (see
 * BonoboActivation documentation). Each of the nodes
 * in the returned GList is a #GdaComponentInfo. To free
 * the returned list, call the #gda_config_free_component_list
 * function.
 *
 * Returns: a GList of #GdaComponentInfo structures.
 */
GList *
gda_config_get_component_list (const gchar *query)
{
	CORBA_Environment ev;
	Bonobo_ServerInfoList *server_list;
	gint i;
	GList *list = NULL;

	CORBA_exception_init (&ev);

	server_list = bonobo_activation_query (query, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		gda_log_error (_("Could not query CORBA components"));
		CORBA_exception_free (&ev);
		return NULL;
	}

	/* create the list to be returned from the CORBA sequence */
	for (i = 0; i < server_list->_length; i++) {
		GdaComponentInfo *comp_info;
		gint j;
		Bonobo_ServerInfo *bonobo_info = &server_list->_buffer[i];

		comp_info = g_new0 (GdaComponentInfo, 1);

		comp_info->id = g_strdup (bonobo_info->iid);
		comp_info->location = g_strdup (bonobo_info->location_info);
		comp_info->description = activation_property_to_string (
			(Bonobo_ActivationProperty *)
			bonobo_server_info_prop_find (bonobo_info, "description"));
		comp_info->repo_ids = activation_property_to_list (
			bonobo_server_info_prop_find (bonobo_info, "repo_ids"));
		comp_info->username = g_strdup (bonobo_info->username);
		comp_info->hostname = g_strdup (bonobo_info->hostname);
		comp_info->domain = g_strdup (bonobo_info->domain);

		if (!strcmp (bonobo_info->server_type, "exe"))
			comp_info->type = GDA_COMPONENT_TYPE_EXE;
		else if (!strcmp (bonobo_info->server_type, "shlib"))
			comp_info->type = GDA_COMPONENT_TYPE_SHLIB;
		else if (!strcmp (bonobo_info->server_type, "factory"))
			comp_info->type = GDA_COMPONENT_TYPE_FACTORY;
		else
			comp_info->type = GDA_COMPONENT_TYPE_INVALID;

		/* get all properties */
		comp_info->properties = gda_parameter_list_new ();
		for (j = 0; j < bonobo_info->props._length; j++) {
			GdaParameter *param;

			param = activation_property_to_parameter (
				&bonobo_info->props._buffer[j]);
			if (param != NULL) {
				gda_parameter_list_add_parameter (
					comp_info->properties, param);
			}
		}

		list = g_list_append (list, comp_info);
	}

	CORBA_free (server_list);

	return list;
}

/**
 * gda_config_free_component_list
 */
void
gda_config_free_component_list (GList *list)
{
	GList *l;

	for (l = g_list_first (list); l; l = l->next) {
		GdaComponentInfo *comp_info = (GdaComponentInfo *) l->data;

		if (comp_info != NULL) {
			g_free (comp_info->id);
			g_free (comp_info->location);
			g_free (comp_info->description);
			g_free (comp_info->username);
			g_free (comp_info->hostname);
			g_free (comp_info->domain);

			g_list_foreach (comp_info->repo_ids, (GFunc) g_free, NULL);
			g_list_free (comp_info->repo_ids);
			gda_parameter_list_free (comp_info->properties);

			g_free (comp_info);
		}
	}

	g_list_free (list);
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
	CORBA_Environment ev;
	Bonobo_ServerInfoList *server_list;
	gint i;
	GList *list = NULL;

	CORBA_exception_init (&ev);

	server_list = bonobo_activation_query (
		"repo_ids.has('IDL:GNOME/Database/Provider:1.0')", NULL, &ev);
	if (BONOBO_EX (&ev)) {
		gda_log_error (_("Could not query CORBA providers"));
		CORBA_exception_free (&ev);
		return NULL;
	}

	/* create the list to be returned from the CORBA sequence */
	for (i = 0; i < server_list->_length; i++) {
		GdaProviderInfo *provider_info;
		Bonobo_ServerInfo *bonobo_info = &server_list->_buffer[i];

		provider_info = g_new0 (GdaProviderInfo, 1);

		provider_info->id = g_strdup (bonobo_info->iid);
		provider_info->location = g_strdup (bonobo_info->location_info);
		provider_info->description = activation_property_to_string (
			(Bonobo_ActivationProperty *)
			bonobo_server_info_prop_find (bonobo_info, "description"));
		provider_info->repo_ids = activation_property_to_list (
			bonobo_server_info_prop_find (bonobo_info, "repo_ids"));
		provider_info->username = g_strdup (bonobo_info->username);
		provider_info->hostname = g_strdup (bonobo_info->hostname);
		provider_info->domain = g_strdup (bonobo_info->domain);
		provider_info->gda_params = activation_property_to_list (
			bonobo_server_info_prop_find (bonobo_info, "gda_params"));

		if (!strcmp (bonobo_info->server_type, "exe"))
			provider_info->type = GDA_COMPONENT_TYPE_EXE;
		else if (!strcmp (bonobo_info->server_type, "shlib"))
			provider_info->type = GDA_COMPONENT_TYPE_SHLIB;
		else if (!strcmp (bonobo_info->server_type, "factory"))
			provider_info->type = GDA_COMPONENT_TYPE_FACTORY;
		else
			provider_info->type = GDA_COMPONENT_TYPE_INVALID;

		list = g_list_append (list, provider_info);
	}

	CORBA_free (server_list);

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

		if (provider_info != NULL) {
			g_free (provider_info->id);
			g_free (provider_info->location);
			g_free (provider_info->description);
			g_free (provider_info->username);
			g_free (provider_info->hostname);
			g_free (provider_info->domain);

			g_list_foreach (provider_info->repo_ids, (GFunc) g_free, NULL);
			g_list_free (provider_info->repo_ids);
			g_list_foreach (provider_info->gda_params, (GFunc) g_free, NULL);
			g_list_free (provider_info->gda_params);

			g_free (provider_info);
		}
	}

	g_list_free (list);
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
		tmp = g_strdup_printf ("%s/%s/Provider", GDA_CONFIG_SECTION_DATASOURCES, l->data);
		info->provider = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the connection string */
		tmp = g_strdup_printf ("%s/%s/DSN", GDA_CONFIG_SECTION_DATASOURCES, l->data);
		info->cnc_string = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the description */
		tmp = g_strdup_printf ("%s/%s/Description", GDA_CONFIG_SECTION_DATASOURCES, l->data);
		info->description = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the user name */
		tmp = g_strdup_printf ("%s/%s/Username", GDA_CONFIG_SECTION_DATASOURCES, l->data);
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
