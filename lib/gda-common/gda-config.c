/* GDA Common Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include "config.h"
#include <gconf/gconf.h>
#include <bonobo-activation/bonobo-activation.h>
#include "gda-config.h"
#include "gda-corba.h"

static GConfEngine *conf_engine = NULL;

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
gda_config_get_string (const gchar * path)
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
gda_config_get_int (const gchar * path)
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
gda_config_get_float (const gchar * path)
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
gda_config_get_boolean (const gchar * path)
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
gda_config_set_string (const gchar * path, const gchar * new_value)
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
gda_config_set_int (const gchar * path, gint new_value)
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
gda_config_set_boolean (const gchar * path, gboolean new_value)
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
gda_config_remove_section (const gchar * path)
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
gda_config_remove_key (const gchar * path)
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
gda_config_has_section (const gchar * path)
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
gda_config_has_key (const gchar * path)
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
 * gda_config_commit
 *
 * Commits all changes made to the configuration system. This means that
 * all buffered data (that is, modified data not yet written to the
 * configuration database), if any, is actually written to disk
 */
void
gda_config_commit (void)
{
	/* FIXME: is that correct? */
	//gconf_suggest_sync(get_conf_engine(), NULL);
}

/**
 * gda_config_rollback
 *
 * Discards all changes made to the configuration system.
 */
void
gda_config_rollback (void)
{
	/* FIXME: how to do it? */
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
gda_config_list_sections (const gchar * path)
{
	GList *ret = NULL;
	GSList *slist;

	g_return_val_if_fail (path != NULL, NULL);

	slist = gconf_engine_all_dirs (get_conf_engine (), path, NULL);
	if (slist) {
		GSList *node;

		for (node = slist; node != NULL; node = g_slist_next (node)) {
			gchar *section_name =
				strrchr ((const char *) node->data, '/');
			if (section_name) {
				ret = g_list_append (ret,
						     g_strdup (section_name +
							       1));
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

				entry_name =
					strrchr ((const char *)
						 gconf_entry_get_key (entry),
						 '/');
				if (entry_name) {
					ret = g_list_append (ret,
							     g_strdup
							     (entry_name +
							      1));
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

/**
 * gda_provider_new:
 *
 * Allocates memory for a new #GdaProvider object and initializes struct 
 * members.
 *
 * Returns: a pointer to a new #GdaProvider object.
 */
GdaProvider *
gda_provider_new (void)
{
	GdaProvider *retval;

	retval = g_new0 (GdaProvider, 1);
	return retval;
}

/**
 * gda_provider_copy:
 * @provider: the provider to be copied.
 *
 * Make a deep copy of all the data needed to describe a GDA provider.
 *
 * Returns: a pointer to the newly allocated provider object
 */
GdaProvider *
gda_provider_copy (GdaProvider * provider)
{
	GdaProvider *retval;

	retval = gda_provider_new ();

	if (provider->name)
		retval->name = g_strdup (provider->name);
	if (provider->comment)
		retval->comment = g_strdup (provider->comment);
	if (provider->location)
		retval->location = g_strdup (provider->location);
	if (provider->repo_id)
		retval->repo_id = g_strdup (provider->repo_id);
	if (provider->type)
		retval->type = g_strdup (provider->type);
	if (provider->username)
		retval->username = g_strdup (provider->username);
	if (provider->hostname)
		retval->hostname = g_strdup (provider->hostname);
	if (provider->domain)
		retval->domain = g_strdup (provider->domain);

	if (provider->dsn_params) {
		GList *node;

		retval->dsn_params = NULL;
		for (node = g_list_first (provider->dsn_params); node;
		     node = g_list_next (node)) {
			retval->dsn_params =
				g_list_append (retval->dsn_params,
					       g_strdup ((gchar *) node->
							 data));
		}
	}

	return retval;
}

/**
 * gda_provider_free:
 * @provider: the provider to de-allocate.
 *
 * Frees the memory allocated with #gda_provider_new and the memory
 * allocated to struct members.
 */
void
gda_provider_free (GdaProvider * provider)
{
	if (provider->name)
		g_free ((gpointer) provider->name);
	if (provider->comment)
		g_free ((gpointer) provider->comment);
	if (provider->location)
		g_free ((gpointer) provider->location);
	if (provider->repo_id)
		g_free ((gpointer) provider->repo_id);
	if (provider->type)
		g_free ((gpointer) provider->type);
	if (provider->username)
		g_free ((gpointer) provider->username);
	if (provider->hostname)
		g_free ((gpointer) provider->hostname);
	if (provider->domain)
		g_free ((gpointer) provider->domain);
	if (provider->dsn_params) {
		GList *node;

		while ((node = g_list_first (provider->dsn_params))) {
			gchar *str = (gchar *) node->data;
			provider->dsn_params =
				g_list_remove (provider->dsn_params,
					       (gpointer) str);
			g_free ((gpointer) str);
		}
	}

	g_free (provider);
}

/**
 * gda_provider_list:
 *
 * Searches the CORBA database for GDA providers and returns a Glist of 
 * #GdaProvider structs.
 *
 * Returns: a GList of GDA providers structs
 */
GList *
gda_provider_list (void)
{
	GList *retval = NULL;
	OAF_ServerInfoList *servlist;
	CORBA_Environment ev;
	gint i;
	GdaProvider *provider;

	CORBA_exception_init (&ev);
	servlist =
		oaf_query ("repo_ids.has('IDL:GDA/Connection:1.0')", NULL,
			   &ev);
	if (servlist) {
		for (i = 0; i < servlist->_length; i++) {
			gchar *dsn_params;

			provider = gda_provider_new ();
			provider->name = g_strdup (servlist->_buffer[i].iid);
			provider->location =
				g_strdup (servlist->_buffer[i].location_info);
			provider->comment =
				gda_corba_get_oaf_attribute (servlist->
							     _buffer[i].props,
							     "description");
			provider->repo_id =
				gda_corba_get_oaf_attribute (servlist->
							     _buffer[i].props,
							     "repo_ids");
			provider->type =
				g_strdup (servlist->_buffer[i].server_type);
			provider->username =
				g_strdup (servlist->_buffer[i].username);
			provider->hostname =
				g_strdup (servlist->_buffer[i].hostname);
			provider->domain =
				g_strdup (servlist->_buffer[i].domain);

			/* get list of available DSN params */
			provider->dsn_params = NULL;
			dsn_params =
				gda_corba_get_oaf_attribute (servlist->
							     _buffer[i].props,
							     "gda_params");
			if (dsn_params) {
				gint cnt = 0;
				gchar **arr = g_strsplit (dsn_params, ";", 0);

				if (arr) {
					while (arr[cnt] != NULL) {
						provider->dsn_params =
							g_list_append
							(provider->dsn_params,
							 g_strdup (arr[cnt]));
						cnt++;
					}
					g_strfreev (arr);
				}
				g_free ((gpointer) dsn_params);
			}

			retval = g_list_append (retval, (gpointer) provider);
		}

		CORBA_free (servlist);
	}
	CORBA_exception_free (&ev);

	return retval;
}

/**
 * gda_provider_free_list
 * @list: list of #GdaProvider structures
 *
 * Frees a list of #GdaProvider structures previously returned by
 * a call to #gda_provider_list
 */
void
gda_provider_free_list (GList * list)
{
	GList *node;

	while ((node = g_list_first (list))) {
		GdaProvider *provider = (GdaProvider *) node->data;
		list = g_list_remove (list, (gpointer) provider);
		gda_provider_free (provider);
	}
}

/**
 * gda_provider_find_by_name
 * @name: provider name
 *
 * Returns a #GdaProvider structure describing the given provider. This function
 * searches all the providers present on your system
 * and tries to find the specified provider.
 *
 * Returns: a pointer to the provider structure, or NULL if not found
 */
GdaProvider *
gda_provider_find_by_name (const gchar * name)
{
	GList *list;
	GList *node;
	GdaProvider *provider = NULL;

	g_return_val_if_fail (name, NULL);

	list = gda_provider_list ();
	node = g_list_first (list);
	while (node) {
		if (!strcmp
		    (name, GDA_PROVIDER_NAME ((GdaProvider *) node->data))) {
			provider =
				gda_provider_copy ((GdaProvider *) node->
						   data);
			break;
		}
		node = g_list_next (node);
	}
	gda_provider_free_list (list);
	return provider;
}

/**
 * gda_list_datasources:
 *
 * Lists all datasources configured on the system. You can then call
 * #gda_config_free_list to free the returned list
 *
 * Returns: a GList with the names of all data sources configured.
 */
GList *
gda_list_datasources (void)
{
	GList *res = 0;
	GList *dsns;
	GList *node;

	dsns = node = gda_dsn_list ();
	while (node) {
		GdaDsn *dsn = (GdaDsn *) node->data;
		if (dsn) {
			res = g_list_append (res,
					     g_strdup (GDA_DSN_GDA_NAME
						       (dsn)));
		}
		node = g_list_next (node);
	}
	gda_dsn_free_list (dsns);

	return res;
}

/**
 * gda_list_datasources_for_provider:
 * @provider: the provider which should be used to look for datasources
 *
 * Returns a list of the names of all data sources set up for the
 * given provider.
 *
 * Returns: a GList of all datasources available to a specific @provider.
 */
GList *
gda_list_datasources_for_provider (gchar * provider)
{
	GList *res = 0;
	GList *dsns;
	GList *node;

	dsns = node = gda_dsn_list ();
	while (node) {
		GdaDsn *dsn = (GdaDsn *) node->data;
		if (dsn && !strcmp (GDA_DSN_PROVIDER (dsn), provider)) {
			res = g_list_append (res,
					     g_strdup (GDA_DSN_GDA_NAME
						       (dsn)));
		}
		node = g_list_next (node);
	}
	gda_dsn_free_list (dsns);

	return res;
}

static gchar *
get_config_string (const gchar * format, ...)
{
	gchar buffer[2048];
	va_list args;

	g_return_val_if_fail (format, 0);

	va_start (args, format);
	vsprintf (buffer, format, args);
	va_end (args);

	return gda_config_get_string (buffer);
}

/**
 * gda_dsn_list
 *
 * Returns a list of all available data sources. The returned value is
 * a GList of #GdaDsn structures
 *
 * Returns: a list of #GdaDsn structures
 */
GList *
gda_dsn_list (void)
{
	GList *datasources = NULL;
	GList *ret = NULL;
	GList *node;

	datasources =
		gda_config_list_sections (GDA_CONFIG_SECTION_DATASOURCES);
	for (node = datasources; node != NULL; node = g_list_next (node)) {
		GdaDsn *dsn;
		gchar *name;

		name = (gchar *) node->data;
		if (name) {
			dsn = gda_dsn_new ();
			gda_dsn_set_name (dsn, name);
			gda_dsn_set_provider (dsn,
					      get_config_string
					      ("%s/%s/Provider",
					       GDA_CONFIG_SECTION_DATASOURCES,
					       name));
			gda_dsn_set_dsn (dsn,
					 get_config_string ("%s/%s/DSN",
							    GDA_CONFIG_SECTION_DATASOURCES,
							    name));
			gda_dsn_set_description (dsn,
						 get_config_string
						 ("%s/%s/Description",
						  GDA_CONFIG_SECTION_DATASOURCES,
						  name));
			gda_dsn_set_username (dsn,
					      get_config_string
					      ("%s/%s/Username",
					       GDA_CONFIG_SECTION_DATASOURCES,
					       name));

			/* add datasource to return list */
			ret = g_list_append (ret, (gpointer) dsn);
		}
	}
	gda_config_free_list (datasources);

	return ret;
}

/**
 * gda_dsn_free
 * @dsn: data source
 *
 * Release all memory occupied by the given #GdaDsn structure
 */
void
gda_dsn_free (GdaDsn * dsn)
{
	g_return_if_fail (dsn != NULL);

	if (dsn->gda_name)
		g_free ((gpointer) dsn->gda_name);
	if (dsn->provider)
		g_free ((gpointer) dsn->provider);
	if (dsn->dsn_str)
		g_free ((gpointer) dsn->dsn_str);
	if (dsn->description)
		g_free ((gpointer) dsn->description);
	if (dsn->username)
		g_free ((gpointer) dsn->username);
	if (dsn->config)
		g_free ((gpointer) dsn->config);
	g_free ((gpointer) dsn);
}

/**
 * gda_dsn_copy
 * @dsn: #GdaDsn to copy from
 *
 * Make an exact copy of the given #GdaDsn structure.
 *
 * Returns: the newly created #GdaDsn structure
 */
GdaDsn *
gda_dsn_copy (GdaDsn * dsn)
{
	GdaDsn *ret;

	g_return_val_if_fail (dsn != NULL, NULL);

	ret = gda_dsn_new ();
	ret->gda_name = g_strdup (dsn->gda_name);
	ret->provider = g_strdup (dsn->provider);
	ret->dsn_str = g_strdup (dsn->dsn_str);
	ret->description = g_strdup (dsn->description);
	ret->username = g_strdup (dsn->username);
	ret->config = g_strdup (dsn->config);

	return ret;
}

/**
 * gda_dsn_find_by_name
 * @dsn_name: data source name
 *
 * Search in the database for the given data source, and return detailed
 * information about it
 *
 * Returns: a #GdaDsn structure containing all information about the found
 * data source
 */
GdaDsn *
gda_dsn_find_by_name (const gchar * dsn_name)
{
	GList *list;
	gboolean found = FALSE;
	GdaDsn *rc = NULL;

	g_return_val_if_fail (dsn_name != NULL, NULL);

	list = gda_dsn_list ();
	while (list) {
		GdaDsn *dsn = (GdaDsn *) list->data;
		if (dsn && !found) {
			if (!g_strcasecmp (GDA_DSN_GDA_NAME (dsn), dsn_name)) {
				rc = dsn;
				found = TRUE;
			}
			else
				gda_dsn_free (dsn);
		}
		else
			gda_dsn_free (dsn);
		list = g_list_next (list);
	}
	g_list_free (g_list_first (list));
	return rc;
}

/**
 * gda_dsn_set_name
 * @dsn: data source
 * @name: new data source name
 *
 * Set the name for the given #GdaDsn structure
 */
void
gda_dsn_set_name (GdaDsn * dsn, const gchar * name)
{
	g_return_if_fail (dsn != NULL);

	if (dsn->gda_name)
		g_free ((gpointer) dsn->gda_name);
	dsn->gda_name = g_strdup (name);
}

/**
 * gda_dsn_set_provider
 * @dsn: data source
 * @provider: provider name
 *
 * Set the provider for the given #GdaDsn structure
 */
void
gda_dsn_set_provider (GdaDsn * dsn, const gchar * provider)
{
	g_return_if_fail (dsn != NULL);

	/* FIXME: should check if the provider does exist */
	if (dsn->provider)
		g_free ((gpointer) dsn->provider);
	dsn->provider = g_strdup (provider);
}

/**
 * gda_dsn_set_dsn
 * @dsn: data source
 * @dsn_str: DSN connection string
 *
 * Set the connection string for the given #GdaDsn structure. This string
 * is DBMS-independent, so you must take care when filling it in. The best
 * way to guess about the allowed parameters in this string for each
 * provider is to obtain the #GdaProvider structure for the provider
 * you want to use, and query the list of available parameters by calling
 * the #GDA_PROVIDER_DSN_PARAMS macro, which returns a GList on which
 * each node is a string containing the name of one parameter.
 *
 * For example, a connection string to connect to a PostgreSQL database
 * might look like this:
 *
 *	"DATABASE=my_database;HOST=localhost
 *
 * to connect to the database "my_database" at the local machine. On the
 * other hand, a string for connecting a gda-default's database, might
 * look like:
 *
 *	"DIRECTORY=/home/me/database"
 */
void
gda_dsn_set_dsn (GdaDsn * dsn, const gchar * dsn_str)
{
	g_return_if_fail (dsn != NULL);

	if (dsn->dsn_str)
		g_free ((gpointer) dsn->dsn_str);
	dsn->dsn_str = g_strdup (dsn_str);
}

/**
 * gda_dsn_set_description
 * @dsn: data source
 * @description: DSN description for the given #GdaDsn structure
 *
 * Set the description string for the given #GdaDsn structure.
 */
void
gda_dsn_set_description (GdaDsn * dsn, const gchar * description)
{
	g_return_if_fail (dsn != NULL);

	if (dsn->description)
		g_free ((gpointer) dsn->description);
	dsn->description = g_strdup (description);
}

/**
 * gda_dsn_set_username
 * @dsn: data source
 * @username: user name
 *
 * Set the username for the given #GdaDsn structure. When using this, the gda-client
 * library would use this user name when you try to connect to this data source
 * without specifying a user name in the call to #gda_connection_open
 */
void
gda_dsn_set_username (GdaDsn * dsn, const gchar * username)
{
	g_return_if_fail (dsn != NULL);

	if (dsn->username)
		g_free ((gpointer) dsn->username);
	dsn->username = g_strdup (username);
}

/**
 * gda_dsn_set_config
 * @dsn: data source
 * @config: configurator
 */
void
gda_dsn_set_config (GdaDsn * dsn, const gchar * config)
{
	g_return_if_fail (dsn != NULL);

	if (dsn->config)
		g_free ((gpointer) dsn->config);
	dsn->config = g_strdup (config);
}

/**
 * gda_dsn_set_global
 * @dsn: data source
 * @is_global: global flag
 */
void
gda_dsn_set_global (GdaDsn * dsn, gboolean is_global)
{
	g_return_if_fail (dsn != NULL);
	dsn->is_global = is_global;
}

/**
 * gda_dsn_save
 * @dsn: data source
 *
 * Saves the given data source into the GDA configuration
 *
 * Returns: TRUE if it was successful, FALSE if there was an error
 */
gboolean
gda_dsn_save (GdaDsn * dsn)
{
	g_return_val_if_fail (dsn != NULL, FALSE);

	if (dsn->gda_name) {
		gchar *config_prefix;
		gchar *tmp;

		config_prefix = g_strdup (GDA_CONFIG_SECTION_DATASOURCES);

		tmp = g_strdup_printf ("%s/%s", config_prefix, dsn->gda_name);
		gda_config_set_string (tmp, GDA_DSN_GDA_NAME (dsn));
		g_free ((gpointer) tmp);

		tmp = g_strdup_printf ("%s/%s/Provider", config_prefix,
				       dsn->gda_name);
		if (GDA_DSN_PROVIDER (dsn))
			gda_config_set_string (tmp, GDA_DSN_PROVIDER (dsn));
		else
			gda_config_set_string (tmp, "");
		g_free ((gpointer) tmp);

		tmp = g_strdup_printf ("%s/%s/DSN", config_prefix,
				       dsn->gda_name);
		if (GDA_DSN_DSN (dsn))
			gda_config_set_string (tmp, GDA_DSN_DSN (dsn));
		else
			gda_config_set_string (tmp, "");
		g_free ((gpointer) tmp);

		tmp = g_strdup_printf ("%s/%s/Description", config_prefix,
				       dsn->gda_name);
		if (GDA_DSN_DESCRIPTION (dsn))
			gda_config_set_string (tmp,
					       GDA_DSN_DESCRIPTION (dsn));
		else
			gda_config_set_string (tmp, "");
		g_free ((gpointer) tmp);

		tmp = g_strdup_printf ("%s/%s/Username", config_prefix,
				       dsn->gda_name);
		if (GDA_DSN_USERNAME (dsn))
			gda_config_set_string (tmp, GDA_DSN_USERNAME (dsn));
		else
			gda_config_set_string (tmp, "");
		g_free ((gpointer) tmp);

		tmp = g_strdup_printf ("%s/%s/Configurator", config_prefix,
				       dsn->gda_name);
		if (GDA_DSN_CONFIG (dsn))
			gda_config_set_string (tmp, GDA_DSN_CONFIG (dsn));
		else
			gda_config_set_string (tmp, "");
		g_free ((gpointer) tmp);

		gda_config_commit ();
		g_free ((gpointer) config_prefix);
		return TRUE;
	}
	else
		g_warning ("data source has no name");
	return FALSE;
}

/**
 * gda_dsn_remove
 * @dsn: data source
 *
 * Remove the given data source (that is, the one described in the given
 * #GdaDsn structure) from the configuration database
 *
 * Returns: TRUE if it was successful, FALSE if there was an error
 */
gboolean
gda_dsn_remove (GdaDsn * dsn)
{
	gchar *tmp;

	g_return_val_if_fail (dsn != NULL, FALSE);

	tmp = g_strdup_printf ("%s/%s", GDA_CONFIG_SECTION_DATASOURCES,
			       GDA_DSN_GDA_NAME (dsn));
	gda_config_remove_section (tmp);
	gda_config_commit ();
	g_free ((gpointer) tmp);

	return TRUE;
}

/**
 * gda_dsn_free_list
 * @list: the list to be freed
 *
 * Free the given list, which should be a list of #GdaDsn structures,
 * as returned by #gda_dsn_list
 */
void
gda_dsn_free_list (GList * list)
{
	GList *node;

	g_return_if_fail (list);

	while ((node = g_list_first (list))) {
		GdaDsn *dsn = (GdaDsn *) node->data;
		list = g_list_remove (list, (gpointer) dsn);
		gda_dsn_free (dsn);
	}
}

/**
 * gda_config_save_last_connection
 * @gda_name: GDA connection name
 * @username: user name used for the connection
 *
 * Adds an entry to the 'Last Connections' configuration section. This section
 * is read by top level clients, such as the GnomeDbLogin widget in
 * GNOME-DB to show the user a list of the last successful connections.
 */
void
gda_config_save_last_connection (const gchar * gda_name,
				 const gchar * username)
{
	gboolean added = FALSE;
	gint cnt;
	GList *l;
	GdaDsn *dsn;
	static GList *last_connections = NULL;

	g_return_if_fail (gda_name != NULL);

	for (cnt = 1;
	     cnt <= gda_config_get_int (GDA_CONFIG_KEY_MAX_LAST_CONNECTIONS);
	     cnt++) {
		gchar *str, *data;

		str = g_strdup_printf ("%s/Connection%d",
				       GDA_CONFIG_SECTION_LAST_CONNECTIONS,
				       cnt);
		data = gda_config_get_string (str);
		g_free (str);
		if (data != NULL) {
			str = g_strdup (data);
			if (!strcmp (str, gda_name) && !added) {
				last_connections =
					g_list_prepend (last_connections,
							str);
				added = TRUE;
			}
			else if (!added)
				last_connections =
					g_list_append (last_connections, str);
		}
	}

	/* fix the length of the saved last connections */
	if (!added) {
		last_connections =
			g_list_prepend (last_connections,
					g_strdup (gda_name));
	}
	if (g_list_length (last_connections) >
	    gda_config_get_int (GDA_CONFIG_KEY_MAX_LAST_CONNECTIONS)) {
		gchar *str;

		l = g_list_last (last_connections);

		str = (gchar *) l->data;
		last_connections = g_list_remove (last_connections, str);
		g_free (str);
	}

	/* now, save the last connections to the configuration */
	for (cnt = 1, l = g_list_first (last_connections);
	     cnt <= gda_config_get_int (GDA_CONFIG_KEY_MAX_LAST_CONNECTIONS)
	     && l != NULL; cnt++, l = g_list_next (l)) {
		gchar *str =
			g_strdup_printf ("%s/Connection%d",
					 GDA_CONFIG_SECTION_LAST_CONNECTIONS,
					 cnt);

		gda_config_set_string (str, (gchar *) l->data);
	}

	dsn = gda_dsn_find_by_name (gda_name);
	if (dsn != NULL) {
		gda_dsn_set_username (dsn, username);
		gda_dsn_save (dsn);
	}
}
