/* GNOME DB Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-common.h"
#include <gconf/gconf.h>

static GConfEngine* conf_engine = NULL;

static GConfEngine *
get_conf_engine (void)
{
  if (!conf_engine)
    {
      conf_engine = gconf_engine_get_default();
    }
  return conf_engine;
}

/**
 * gda_config_get_string
 * @path: path to the configuration entry
 */
gchar *
gda_config_get_string (const gchar *path)
{
  return gconf_engine_get_string(get_conf_engine(), path, NULL);
}

/**
 * gda_config_get_int
 * @path: path to the configuration entry
 */
gint
gda_config_get_int (const gchar *path)
{
  return gconf_engine_get_int(get_conf_engine(), path, NULL);
}

/**
 * gda_config_get_float
 * @path: path to the configuration entry
 */
gdouble
gda_config_get_float (const gchar *path)
{
  return gconf_engine_get_float(get_conf_engine(), path, NULL);
}

/**
 * gda_config_get_boolean
 * @path: path to the configuration entry
 */
gboolean
gda_config_get_boolean (const gchar *path)
{
  return gconf_engine_get_bool(get_conf_engine(), path, NULL);
}

/**
 * gda_config_set_string
 * @path: path to the configuration entry
 * @new_value: new value
 */
void
gda_config_set_string (const gchar *path, const gchar *new_value)
{
  gconf_engine_set_string(get_conf_engine(), path, new_value, NULL);
}

/**
 * gda_config_set_int
 * @path: path to the configuration entry
 * @new_value: new value
 */
void
gda_config_set_int (const gchar *path, gint new_value)
{
  gconf_engine_set_int(get_conf_engine(), path, new_value, NULL);
}

/**
 * gda_config_set_float
 * @path: path to the configuration entry
 * @new_value: new value
 */
void
gda_config_set_float (const gchar *path, gdouble new_value)
{
  gconf_engine_set_float(get_conf_engine(), path, new_value, NULL);
}

/**
 * gda_config_set_boolean
 * @path: path to the configuration entry
 * @new_value: new value
 */
void
gda_config_set_boolean (const gchar *path, gboolean new_value)
{
  gconf_engine_set_bool(get_conf_engine(), path, new_value, NULL);
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
  gconf_engine_unset(get_conf_engine(), path, NULL);
}

/**
 * gda_config_has_section
 * @path: path to the configuration section
 *
 * Checks whether the given section exists in the configuration
 * system
 */
gboolean
gda_config_has_section (const gchar *path)
{
  return gconf_engine_dir_exists(get_conf_engine, path, NULL);
}

/**
 * gda_config_commit
 *
 * Commits all changes made to the configuration system
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
 * Discards all changes made to the configuration system
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
 * To free the returned value, you can use @gda_config_free_list
 */
GList *
gda_config_list_sections (const gchar *path)
{
  GList*   ret = NULL;
  GSList*  slist;

  g_return_val_if_fail(path != NULL, NULL);
  
  slist = gconf_engine_all_dirs(get_conf_engine(), path, NULL);
  if (slist)
    {
      GSList* node;
      
      for (node = slist; node != NULL; node = g_slist_next(node))
        {
          ret = g_list_append(ret, g_strdup(node->data));
        }
      g_slist_free(slist);
    }
  return ret;
}

/**
 * gda_config_list_keys
 */
GList *
gda_config_list_keys (const gchar *path)
{
  GList*   ret = NULL;
  GSList*  slist;

  g_return_val_if_fail(path != NULL, NULL);
  
  slist = gconf_engine_all_entries(get_conf_engine(), path, NULL);
  if (slist)
    {
      GSList* node;
      
      for (node = slist; node != NULL; node = g_slist_next(node))
        {
          ret = g_list_append(ret, g_strdup(node->data));
        }
      g_slist_free(slist);
    }
  return ret;
}

/**
 * gda_config_free_list
 * @list: list to be freed
 *
 * Free all memory used by the given GList, which must be the return value
 * from either @gda_config_list_sections and @gda_config_list_keys
 */
void
gda_config_free_list (GList *list)
{
  while (list != NULL)
    {
      gchar* str = (gchar *) list->data;
      list = g_list_remove(list, (gpointer) str);
      g_free((gpointer) str);
    }
}

/**
 * gda_provider_new:
 *
 * Allocates memory for a new Gda_Provider object and initializes struct 
 * members.
 *
 * Returns: a pointer to a new Gda_Provider object.
 */
Gda_Provider *
gda_provider_new (void)
{
  Gda_Provider* retval;

  retval = g_new0(Gda_Provider, 1);
  return retval;
}

/**
 * gda_provider_copy:
 * @provider: the provider to be copies.
 *
 * Make a deep copy of all the data needed to describe a gGDA provider.
 *
 * Returns: a pointer to the newly allocated provider object
 */
Gda_Provider *
gda_provider_copy (Gda_Provider* provider)
{
  Gda_Provider* retval;

  retval = gda_provider_new();
  
  if (provider->name) retval->name = g_strdup(provider->name);
  if (provider->comment) retval->comment = g_strdup(provider->comment);
  if (provider->location) retval->location = g_strdup(provider->location);
  if (provider->repo_id) retval->repo_id = g_strdup(provider->repo_id);
  if (provider->type) retval->type = g_strdup(provider->type);
  
  return retval;
}

/**
 * gda_provider_free:
 * @provider: the provider to de-allocate.
 *
 * Frees the memory allocated with gda_provider_new() and the memory
 * allocated to struct members.
 */
void
gda_provider_free (Gda_Provider* provider)
{
  if (provider->name) g_free((gpointer) provider->name);
  if (provider->comment) g_free((gpointer) provider->comment);
  if (provider->location) g_free((gpointer) provider->location);
  if (provider->repo_id) g_free((gpointer) provider->repo_id);
  if (provider->type) g_free((gpointer) provider->type);
  if (provider->username) g_free((gpointer) provider->username);
  if (provider->hostname) g_free((gpointer) provider->hostname);
  if (provider->domain) g_free((gpointer) provider->domain);
  if (provider->dsn_params)
    {
      GList* node;
      
      while ((node = g_list_first(provider->dsn_params)))
        {
          gchar* str = (gchar *) node->data;
          provider->dsn_params = g_list_remove(provider->dsn_params, (gpointer) str);
          g_free((gpointer) str);
        }
    }

  g_free(provider);
}

/**
 * gda_provider_list:
 *
 * Searches the CORBA database for GDA providers and returns a Glist of 
 * Gda_Provider structs. 
 *
 * Returns: a GList of GDA providers structs
 */
GList *
gda_provider_list (void)
{
  GList*              retval = NULL;
  OAF_ServerInfoList* servlist;
  CORBA_Environment   ev;
  gint                i;
  Gda_Provider*         provider;

  CORBA_exception_init(&ev);
  servlist = oaf_query("repo_ids.has('IDL:GDA/ConnectionFactory:1.0')", NULL, &ev);
  if (servlist)
    {
      for (i = 0; i < servlist->_length; i++)
        {
          gchar* dsn_params;
          
          provider = gda_provider_new();
          provider->name = g_strdup(servlist->_buffer[i].iid);
          provider->location = g_strdup(servlist->_buffer[i].location_info);
          provider->comment = gda_corba_get_oaf_attribute(servlist->_buffer[i].props, "description");
          provider->repo_id = gda_corba_get_oaf_attribute(servlist->_buffer[i].props, "repo_ids");
          provider->type = g_strdup(servlist->_buffer[i].server_type);
          provider->username = g_strdup(servlist->_buffer[i].username);
          provider->hostname = g_strdup(servlist->_buffer[i].hostname);
          provider->domain = g_strdup(servlist->_buffer[i].domain);

          /* get list of available DSN params */
          provider->dsn_params = NULL;
          dsn_params = gda_corba_get_oaf_attribute(servlist->_buffer[i].props, "gda_params");
          if (dsn_params)
            {
              gint   cnt = 0;
              gchar* arr = g_strsplit(dsn_params, ";", 0);
              
              while (arr[cnt])
                {
                  provider->dsn_params = g_list_append(provider->dsn_params, g_strdup(arr[cnt]));
                  cnt++;
                }
              g_strfreev(arr);
            }

          retval = g_list_append(retval, (gpointer) provider);
        }

      /* FIXME: free servlist */
    }
  CORBA_exception_free(&ev);

  return retval;
}

/**
 * gda_provider_free_list
 * @list: list of #Gda_Provider structures
 *
 * Frees a list of #Gda_Provider structures previously returned by
 * a call to #gda_provider_list
 */
void
gda_provider_free_list (GList *list)
{
  GList* node;

  while ((node = g_list_first(list)))
    {
      Gda_Provider* provider = (Gda_Provider *) node->data;
      list = g_list_remove(list, (gpointer) provider);
      gda_provider_free(provider);
    }
}

/**
 * gda_provider_find_by_name
 * @name: provider name
 *
 * Returns a #Gda_Provider structure describing the given provider. This function
 * searches all the providers present on your system
 * and tries to find the specified provider.
 *
 * Returns: a pointer to the provider structure, or NULL on error
 */
Gda_Provider *
gda_provider_find_by_name (const gchar *name)
{
  GList*      list;
  GList*      node;
  Gda_Provider* provider = NULL;

  g_return_val_if_fail(name, NULL);

  list = gda_provider_list();
  node = g_list_first(list);
  while (node)
    {
      if (!strcmp(name, GDA_PROVIDER_NAME((Gda_Provider *) node->data)))
        {
          provider = gda_provider_copy((Gda_Provider *) node->data);
          break;
        }
      node = g_list_next(node);
    }
  gda_provider_free_list(list);
  return (provider);
}

/**
 * gda_list_datasources:
 *
 * Lists all datasources configured on the system.
 *
 * Returns a GList with the names of all data sources configured.
 */
GList *
gda_list_datasources (void)
{
  GList* res = 0;
  GList* dsns;
  GList* node;

  dsns = node = gda_dsn_list();
  while (node)
    {
      Gda_Dsn* dsn = (Gda_Dsn *) node->data;
      if (dsn)
        {
          res = g_list_append(res, g_strdup(GDA_DSN_GDA_NAME(dsn)));
        }
      node = g_list_next(node);
    }
  gda_dsn_free_list(dsns);

  return res;
}

/**
 * gda_list_datasources_for_provider:
 * @provider: the provider which should be used to look for datasources
 *
 * Returns: a GList of all datasources available to a specific @provider.
 */
GList *
gda_list_datasources_for_provider (gchar* provider)
{
  GList* res = 0;
  GList* dsns;
  GList* node;

  dsns = node = gda_dsn_list();
  while (node)
    {
      Gda_Dsn* dsn = (Gda_Dsn *) node->data;
      if (dsn && !strcmp(GDA_DSN_PROVIDER(dsn), provider))
        {
          res = g_list_append(res, g_strdup(GDA_DSN_GDA_NAME(dsn)));
        }
      node = g_list_next(node);
    }
  gda_dsn_free_list(dsns);

  return res;
}

static gchar *
get_config_string (const gchar *format, ...)
{
  gchar   buffer[2048];
  va_list args;

  g_return_val_if_fail(format, 0);

  va_start(args, format);
  vsprintf(buffer, format, args);
  va_end(args);

  return gda_config_get_string(buffer);
}

/**
 * gda_dsn_list
 *
 * Returns a list of all available data sources. The returned value is
 * a GList of #Gda_Dsn structures
 */
GList *
gda_dsn_list (void)
{
  GList* datasources = NULL;
  GList* ret = NULL;
  GList* node;

  datasources = gda_config_list_sections(GDA_CONFIG_SECTION_DATASOURCES);
  for (node = datasources; node != NULL; node = g_list_next(node))
    {
      Gda_Dsn* dsn;
      gchar*   name;
      
      name = (gchar *) node->data;
      if (name)
        {
          dsn = gda_dsn_new();
          gda_dsn_set_name(dsn, name);
          gda_dsn_set_provider(dsn, get_config_string("%s/%s/Provider", GDA_CONFIG_SECTION_DATASOURCES, name));
          gda_dsn_set_dsn(dsn, get_config_string("%s/%s/DSN", GDA_CONFIG_SECTION_DATASOURCES, name));
          gda_dsn_set_description(dsn, get_config_string("%s/%s/Description", GDA_CONFIG_SECTION_DATASOURCES, name));
          gda_dsn_set_username(dsn, get_config_string("%s/%s/Username", GDA_CONFIG_SECTION_DATASOURCES, name));
          
          /* add datasource to return list */
          ret = g_list_append(ret, (gpointer) dsn);
        }
    }
  gda_config_free_list(datasources);
  
  return ret;
}

/**
 * gda_dsn_free
 * @dsn: data source
 */
void
gda_dsn_free (Gda_Dsn *dsn)
{
  g_return_if_fail(dsn != NULL);

  if (dsn->gda_name) g_free((gpointer) dsn->gda_name);
  if (dsn->provider) g_free((gpointer) dsn->provider);
  if (dsn->dsn_str) g_free((gpointer) dsn->dsn_str);
  if (dsn->description) g_free((gpointer) dsn->description);
  if (dsn->username) g_free((gpointer) dsn->username);
  if (dsn->config) g_free((gpointer) dsn->config);
  g_free((gpointer) dsn);
}

/**
 * gda_dsn_find_by_name
 * @dsn_name: data source name
 */
Gda_Dsn *
gda_dsn_find_by_name (const gchar *dsn_name)
{
  GList*   list;
  gboolean found = FALSE;
  Gda_Dsn* rc = NULL;

  g_return_val_if_fail(dsn_name != NULL, NULL);

  list = gda_dsn_list();
  while (list)
    {
      Gda_Dsn* dsn = (Gda_Dsn *) list->data;
      if (dsn && !found)
        {
          if (!g_strcasecmp(GDA_DSN_GDA_NAME(dsn), dsn_name))
            {
              rc = dsn;
              found = TRUE;
            }
          else gda_dsn_free(dsn);
        }
      else gda_dsn_free(dsn);
      list = g_list_next(list);
    }
  g_list_free(g_list_first(list));
  return rc;
}

/**
 * gda_dsn_set_name
 * @dsn: data source
 * @name: new data source name
 */
void
gda_dsn_set_name (Gda_Dsn *dsn, const gchar *name)
{
  g_return_if_fail(dsn != NULL);
  g_return_if_fail(name != NULL);

  if (dsn->gda_name) g_free((gpointer) dsn->gda_name);
  dsn->gda_name = g_strdup(name);
}

/**
 * gda_dsn_set_provider
 * @dsn: data source
 * @provider: provider name
 */
void
gda_dsn_set_provider (Gda_Dsn *dsn, const gchar *provider)
{
  g_return_if_fail(dsn != NULL);
  g_return_if_fail(provider != NULL);

  /* FIXME: should check if the provider does exist */
  if (dsn->provider) g_free((gpointer) dsn->provider);
  dsn->provider = g_strdup(provider);
}

/**
 * gda_dsn_set_dsn
 * @dsn: data source
 * @dsn_str: DSN connection string
 */
void
gda_dsn_set_dsn (Gda_Dsn *dsn, const gchar *dsn_str)
{
  g_return_if_fail(dsn != NULL);
  g_return_if_fail(dsn_str != NULL);

  if (dsn->dsn_str) g_free((gpointer) dsn->dsn_str);
  dsn->dsn_str = g_strdup(dsn_str);
}

/**
 * gda_dsn_set_description
 * @dsn: data source
 * @description: DSN description
 */
void
gda_dsn_set_description (Gda_Dsn *dsn, const gchar *description)
{
  g_return_if_fail(dsn != NULL);
  g_return_if_fail(description != NULL);

  if (dsn->description) g_free((gpointer) dsn->description);
  dsn->description = g_strdup(description);
}

/**
 * gda_dsn_set_username
 * @dsn: data source
 * @username: user name
 */
void
gda_dsn_set_username (Gda_Dsn *dsn, const gchar *username)
{
  g_return_if_fail(dsn != NULL);
  g_return_if_fail(username != NULL);

  if (dsn->username) g_free((gpointer) dsn->username);
  dsn->username = g_strdup(username);
}

/**
 * gda_dsn_set_config
 * @dsn: data source
 * @config: configurator
 */
void
gda_dsn_set_config (Gda_Dsn *dsn, const gchar *config)
{
  g_return_if_fail(dsn != NULL);
  g_return_if_fail(config != NULL);

  if (dsn->config) g_free((gpointer) dsn->config);
  dsn->config = g_strdup(config);
}

/**
 * gda_dsn_set_global
 * @dsn: data source
 * @is_global: global flag
 */
void
gda_dsn_set_global (Gda_Dsn *dsn, gboolean is_global)
{
  g_return_if_fail(dsn != NULL);
  dsn->is_global = is_global;
}

/**
 * gda_dsn_save
 * @dsn: data source
 *
 * Saves the given data source into the GDA configuration
 */
gboolean
gda_dsn_save (Gda_Dsn *dsn)
{
  g_return_val_if_fail(dsn != NULL, FALSE);

  if (dsn->gda_name)
    {
      gchar* config_prefix;
      gchar* tmp;

      config_prefix = g_strdup(GDA_CONFIG_SECTION_DATASOURCES);

      tmp = g_strdup_printf("%s/%s", config_prefix, dsn->gda_name);
      gda_config_set_string(tmp, GDA_DSN_GDA_NAME(dsn));
      g_free((gpointer) tmp);

      tmp = g_strdup_printf("%s/%s/Provider", config_prefix, dsn->gda_name);
      gda_config_set_string(tmp, GDA_DSN_PROVIDER(dsn));
      g_free((gpointer) tmp);

      tmp = g_strdup_printf("%s/%s/DSN", config_prefix, dsn->gda_name);
      gda_config_set_string(tmp, GDA_DSN_DSN(dsn));
      g_free((gpointer) tmp);

      tmp = g_strdup_printf("%s/%s/Description", config_prefix, dsn->gda_name);
      gda_config_set_string(tmp, GDA_DSN_DESCRIPTION(dsn));
      g_free((gpointer) tmp);

      tmp = g_strdup_printf("%s/%s/Username", config_prefix, dsn->gda_name);
      gda_config_set_string(tmp, GDA_DSN_USERNAME(dsn));
      g_free((gpointer) tmp);

      tmp = g_strdup_printf("%s/%s/Configurator", config_prefix, dsn->gda_name);
      gda_config_set_string(tmp, GDA_DSN_CONFIG(dsn));
      g_free((gpointer) tmp);

      gda_config_commit();
      g_free((gpointer) config_prefix);
      return TRUE;
    }
  else g_warning("data source has no name");
  return FALSE;
}

/**
 * gda_dsn_remove
 * @dsn: data source
 */
gboolean
gda_dsn_remove (Gda_Dsn *dsn)
{
  gchar* tmp;

  g_return_val_if_fail(dsn != NULL, FALSE);

  tmp = g_strdup_printf("%s/%s", GDA_CONFIG_SECTION_DATASOURCES, GDA_DSN_GDA_NAME(dsn));
  gda_config_remove_section(tmp);
  gda_config_commit();
  g_free((gpointer) tmp);

  return TRUE;
}

/**
 * gda_dsn_free_list
 */
void
gda_dsn_free_list (GList *list)
{
  GList* node;

  g_return_if_fail(list);

  while ((node = g_list_first(list)))
    {
      Gda_Dsn* dsn = (Gda_Dsn *) node->data;
      list = g_list_remove(list, (gpointer) dsn);
      gda_dsn_free(dsn);
    }
}

