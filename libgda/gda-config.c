/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifndef LIBGDA_USER_CONFIG_FILE
#define LIBGDA_USER_CONFIG_FILE "/.libgda/config"  /* Appended to $HOME */
#endif

//FIXME: many functions are not thread safe!

typedef struct {
    gchar *name;
    gchar *type;
    gchar *value;
    gchar *mtime;
    gchar *muser;
} gda_config_entry;

typedef struct {
    gchar *path;
    GList *entries;
} gda_config_section;

typedef struct {
	GList *global;
	GList *user;
} gda_config_client;

static gda_config_client *config_client;
static void do_notify (const gchar *path);

/*
 * Private functions
 */
static GList *
gda_config_read_entries (xmlNodePtr cur)
{
	GList *list;
	gda_config_entry *entry;

	g_return_val_if_fail (cur != NULL, NULL);

	list = NULL;
	cur = cur->xmlChildrenNode;
	while (cur != NULL){
		if (!strcmp(cur->name, "entry")){
			entry = g_new (gda_config_entry, 1);
			entry->name = xmlGetProp(cur, "name");
			if (entry->name == NULL)
				g_warning ("NULL 'name' in an entry");

			entry->type =  xmlGetProp(cur, "type");
			if (entry->type == NULL)
				g_warning ("NULL 'type' in an entry");

			entry->value =  xmlGetProp(cur, "value");
			if (entry->value == NULL)
				g_warning ("NULL 'value' in an entry");

			entry->muser =  xmlGetProp(cur, "muser");
			if (entry->value == NULL)
				g_warning ("NULL 'muser' in an entry");

			entry->mtime =  xmlGetProp(cur, "mtime");
			if (entry->value == NULL)
				g_warning ("NULL 'mtime' in an entry");

			list = g_list_append (list, entry);
		} else {
			g_warning ("'entry' expected, got '%s'. Ignoring...", 
				   cur->name);
		}
		cur = cur->next;
	}
	
	return list;
}

static GList *
gda_config_parse_config_file (gchar *buffer, gint len)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	GList *list = NULL;
	gda_config_section *section;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (len != 0, NULL);

	doc = xmlParseMemory (buffer, len);
	if (doc == NULL){
		g_warning ("File empty or not well-formed.");
		return NULL;
	}

	cur = xmlDocGetRootElement (doc);
	if (cur == NULL){
		g_warning ("Cannot get root element!");
		xmlFreeDoc (doc);
		return NULL;
	}

	if (strcmp (cur->name, "libgda-config") != 0){
		g_warning ("root node != 'libgda-config' -> '%s'", cur->name);
		xmlFreeDoc (doc);
		return NULL;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!strcmp(cur->name, "section")){
			section = g_new (gda_config_section, 1);
			section->path = xmlGetProp (cur, "path");
			if (section->path != NULL){
				section->entries = gda_config_read_entries (cur);
				list = g_list_append (list, section);
			} else {
				g_warning ("section without 'path'!");
				g_free (section);
			}
		} else {
			g_warning ("'section' expected, got '%s'. Ignoring...",
				   cur->name);
		}

		cur = cur->next;
	}

	xmlFreeDoc (doc);
	return list;
}

static gda_config_client *
get_config_client ()
{
	//FIXME: if we're updating or writing config_client,
	//	 wait until the operation finishes
	if (config_client == NULL){
		gint len;
		gchar *full_file;
		gchar *user_config;

		config_client = g_new0 (gda_config_client, 1);
		xmlKeepBlanksDefault(0);
		if (g_file_get_contents (LIBGDA_GLOBAL_CONFIG_FILE, &full_file,
					 &len, NULL)){
			config_client->global = 
				gda_config_parse_config_file (full_file, len);
		}

		user_config = g_strdup_printf ("%s%s", g_get_home_dir (),
						LIBGDA_USER_CONFIG_FILE);
		if (g_file_get_contents (user_config, &full_file, &len, NULL)){
			config_client->user =
				gda_config_parse_config_file (full_file, len);
		}
		g_free (user_config);
	}

	return config_client;
}

static gda_config_section *
gda_config_search_section (GList *sections, const gchar *path)
{
	GList *ls;
	gda_config_section *section;

	section = NULL;
	for (ls = sections; ls; ls = ls->next){
		section = ls->data;
		if (!strcmp (section->path, path))
			break;

		section = NULL;
	}

	return section;
}

static gda_config_entry *
gda_config_search_entry (GList *sections, const gchar *path, const gchar *type)
{
	gint last_dash;
	gchar *ptr_last_dash;
	gchar *section_path;
	GList *le;
	gda_config_section *section;
	gda_config_entry *entry;

	if (sections == NULL)
		return NULL;
	
	ptr_last_dash = strrchr (path, '/');
	if (ptr_last_dash == NULL)
		return NULL;

	last_dash = ptr_last_dash - path;
	section_path = g_strdup (path);
	section_path [last_dash] = '\0';
	entry = NULL;
	section = gda_config_search_section (sections, section_path);
	if (section){
		for (le = section->entries; le; le = le->next){
			entry = le->data;
			if (type != NULL &&
			    !strcmp (entry->type, type) && 
			    !strcmp (entry->name, ptr_last_dash + 1))
				break;
			else if (!strcmp (entry->name, ptr_last_dash + 1))
				break;

			entry = NULL;
		}
	}

	g_free (section_path);
	return entry;
}

static gda_config_section *
gda_config_add_section (const gchar *section_path)
{
	gda_config_client *cfg_client;
	gda_config_section *section;

	section = g_new (gda_config_section, 1);
	section->path = g_strdup (section_path);
	section->entries = NULL;

	cfg_client = get_config_client ();
	cfg_client->user = g_list_append (cfg_client->user, section);

	return section;
}

static void
gda_config_add_entry (const gchar *section_path, 
		      const gchar *name,
		      const gchar *type,
		      const gchar *value)
{
	gda_config_entry *entry;
	gda_config_section *section;
	gda_config_client *cfg_client;

	cfg_client = get_config_client ();
	entry = g_new (gda_config_entry, 1);
	entry->name = g_strdup (name);
	entry->type = g_strdup (type);
	entry->value = g_strdup (value);
	
	section = gda_config_search_section (cfg_client->global, section_path);
	if (section == NULL)
		section = gda_config_add_section (section_path);

	section->entries = g_list_append (section->entries, entry);
}

static void
free_entry (gpointer data, gpointer user_data)
{
	gda_config_entry *entry = data;

	g_free (entry->name);
	g_free (entry->type);
	g_free (entry->value);
	g_free (entry);
}

static void
free_section (gpointer data, gpointer user_data)
{
	gda_config_section *section = data;

	g_list_foreach (section->entries, free_entry, NULL);
	g_list_free (section->entries);
	g_free (section->path);
	g_free (section);
}

static void
add_xml_entry (xmlNodePtr parent, gda_config_entry *entry)
{
	xmlNodePtr new_node;

	new_node = xmlNewTextChild (parent, NULL, "entry", NULL);
	xmlSetProp (new_node, "name", entry->name ? entry->name : "");
	xmlSetProp (new_node, "type", entry->type ? entry->type : "");
	xmlSetProp (new_node, "value", entry->value ? entry->value : "");
	xmlSetProp (new_node, "mtime", entry->name ? entry->name : "");
	xmlSetProp (new_node, "muser", entry->type ? entry->type : "");
}

static xmlNodePtr
add_xml_section (xmlNodePtr parent, gda_config_section *section)
{
	xmlNodePtr new_node;

	new_node = xmlNewTextChild (parent, NULL, "section", NULL);
	xmlSetProp (new_node, "path", section->path ? section->path : "");
	return new_node;
}

static void
write_config_file ()
{
	gda_config_client *cfg_client;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr xml_section;
	GList *ls; /* List of sections */
	GList *le; /* List of entries */
	gda_config_section *section;
	gda_config_entry *entry;
	gchar *user_config;

	cfg_client = get_config_client ();
	g_return_if_fail (cfg_client->user != NULL);

	doc = xmlNewDoc ("1.0");
	g_return_if_fail (doc != NULL);
	root = xmlNewDocNode (doc, NULL, "libgda-config", NULL);
	xmlDocSetRootElement (doc, root);
	for (ls = cfg_client->user; ls; ls = ls->next){
		section = ls->data;
		if (section == NULL)
			continue;

		xml_section = add_xml_section (root, section);
		for (le = section->entries; le; le = le->next){
			entry = le->data;
			if (entry == NULL)
				continue;

			add_xml_entry (xml_section, le->data);
		}
	}

	user_config = g_strdup_printf ("%s%s", g_get_home_dir (),
					LIBGDA_USER_CONFIG_FILE);
	if (xmlSaveFormatFile (user_config, doc, TRUE) == -1){
		g_warning ("Error saving config data to %s", user_config);
	}

	g_free (user_config);
	xmlFreeDoc (doc);
}

/*
 * Public functions
 */

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
	gda_config_client *cfg_client;
	gda_config_entry *entry;

	g_return_val_if_fail (path != NULL, NULL);

	cfg_client = get_config_client ();

	entry = gda_config_search_entry (cfg_client->user, path, "string");
	if (entry == NULL)
		entry = gda_config_search_entry (cfg_client->global, path, "string");

        return (entry != NULL && entry->value != NULL) ? 
			g_strdup (entry->value) : NULL;
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
	gda_config_client *cfg_client;
	gda_config_entry *entry;

	g_return_val_if_fail (path != NULL, 0);

	cfg_client = get_config_client ();

	entry = gda_config_search_entry (cfg_client->user, path, "long");
	if (entry == NULL)
		entry = gda_config_search_entry (cfg_client->global, path, "long");

        return (entry != NULL && entry->value != NULL) ? 
			atoi (entry->value) : 0;
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
	gda_config_client *cfg_client;
	gda_config_entry *entry;

	g_return_val_if_fail (path != NULL, 0.0);

	cfg_client = get_config_client ();

	entry = gda_config_search_entry (cfg_client->user, path, "float");
	if (entry == NULL)
		entry = gda_config_search_entry (cfg_client->global, path, "float");

        return (entry != NULL && entry->value != NULL) ? 
			g_strtod (entry->value, NULL) : 0.0;
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
	gda_config_client *cfg_client;
	gda_config_entry *entry;

	g_return_val_if_fail (path != NULL, FALSE);

	cfg_client = get_config_client ();

	entry = gda_config_search_entry (cfg_client->user, path, "bool");
	if (entry == NULL)
		entry = gda_config_search_entry (cfg_client->global, path, "bool");

        return (entry != NULL && entry->value != NULL) ? 
			!strcmp (entry->value, "true") : FALSE;
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
	gda_config_entry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gda_config_client *cfg_client;

	g_return_if_fail (path != NULL);
	g_return_if_fail (new_value != NULL);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "string");
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return;
		}
		last_dash = ptr_last_dash - path;
		section_path = g_strdup (path);
		section_path [last_dash] = '\0';
		gda_config_add_entry (section_path, ptr_last_dash + 1, 
						    "string", new_value);
		g_free (section_path);
	} else {
		g_free (entry->value);
		entry->value = g_strdup (new_value);
	}

	write_config_file ();
	do_notify (path);
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
	gda_config_entry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gchar *newstr;
	gda_config_client *cfg_client;

	g_return_if_fail (path !=NULL);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "long");
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return;
		}
		last_dash = ptr_last_dash - path;
		section_path = g_strdup (path);
		section_path [last_dash] = '\0';
		newstr = g_strdup_printf ("%d", new_value);
		gda_config_add_entry (section_path, ptr_last_dash + 1, 
						    "long", newstr);
		g_free (newstr);
		g_free (section_path);
	} else {
		g_free (entry->value);
		entry->value = g_strdup_printf ("%d", new_value);
	}

	write_config_file ();
	do_notify (path);
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
	gda_config_entry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gchar *newstr;
	gda_config_client *cfg_client;

	g_return_if_fail (path !=NULL);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "float");
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return;
		}
		last_dash = ptr_last_dash - path;
		section_path = g_strdup (path);
		section_path [last_dash] = '\0';
		newstr = g_strdup_printf ("%f", new_value);
		gda_config_add_entry (section_path, ptr_last_dash + 1, 
						    "float", newstr);
		g_free (newstr);
		g_free (section_path);
	} else {
		g_free (entry->value);
		entry->value = g_strdup_printf ("%f", new_value);
	}

	write_config_file ();
	do_notify (path);
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
	gda_config_entry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gchar *newstr;
	gda_config_client *cfg_client;

	g_return_if_fail (path !=NULL);

	new_value = new_value != 0 ? TRUE : FALSE;
	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "bool");
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return;
		}
		last_dash = ptr_last_dash - path;
		section_path = g_strdup (path);
		section_path [last_dash] = '\0';
		newstr = new_value == TRUE ? "true" : "false";
		gda_config_add_entry (section_path, ptr_last_dash + 1, 
						    "bool", newstr);
		g_free (section_path);
	} else {
		g_free (entry->value);
		entry->value = g_strdup_printf ("%d", new_value);
	}

	write_config_file ();
	do_notify (path);
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
	gda_config_section *section;
	gda_config_client *cfg_client;

	g_return_if_fail (path != NULL);

	cfg_client = get_config_client ();
	section = gda_config_search_section (cfg_client->user, path);
	if (section == NULL){
		g_warning ("Section %s not found!", path);
		return;
	}

	cfg_client->user = g_list_remove (cfg_client->user, section);
	free_section (section, NULL);
	write_config_file ();
	do_notify (path);
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
	gint last_dash;
	gchar *ptr_last_dash;
	gchar *section_path;
	GList *le;
	gda_config_section *section;
	gda_config_entry *entry;
	gda_config_client *cfg_client;

	g_return_if_fail (path != NULL);

	ptr_last_dash = strrchr (path, '/');
	if (ptr_last_dash == NULL)
		return;

	last_dash = ptr_last_dash - path;
	section_path = g_strdup (path);
	section_path [last_dash] = '\0';
	entry = NULL;
	cfg_client = get_config_client ();
	section = gda_config_search_section (cfg_client->user, section_path);
	if (section){
		for (le = section->entries; le; le = le->next){
			entry = le->data;
			if (!strcmp (entry->name, ptr_last_dash + 1))
				break;

			entry = NULL;
		}
	}

	g_free (section_path);
	if (entry != NULL){
		section->entries = g_list_remove (section->entries, entry);
		free_entry (entry, NULL);
		write_config_file ();
		do_notify (path);
	}
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
	gda_config_section *section;
	gda_config_client *cfg_client;

	g_return_val_if_fail (path != NULL, FALSE);

	cfg_client = get_config_client ();
	section = gda_config_search_section (cfg_client->user, path);
	return (section != NULL) ? TRUE : FALSE;
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
	gda_config_entry *entry;
	gda_config_client *cfg_client;

	g_return_val_if_fail (path != NULL, FALSE);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, NULL);
	return (entry != NULL) ? TRUE : FALSE;
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
	gda_config_client *cfg_client;
	GList *list;
        GList *ret = NULL;
	gda_config_section *section;
	gint len;

        g_return_val_if_fail (path != NULL, NULL);

	len = strlen (path);
	cfg_client = get_config_client ();
	if (cfg_client->user){
		for (list = cfg_client->user; list; list = list->next){
			section = list->data;
			if (section && len < strlen (section->path) && 
			    !strncmp (path, section->path, len))
				ret = g_list_append (ret, g_strdup (section->path + len + 1));
		}
	}
		
	if (cfg_client->global){
		for (list = cfg_client->global; list; list = list->next){
			section = list->data;
			if (section && len < strlen (section->path) && 
			    !strncmp (path, section->path, len)){
				if (!g_list_find_custom (ret, 
							 section->path + len + 1,
							 (GCompareFunc) strcmp))
					ret = g_list_append (ret, 
							g_strdup (section->path + len + 1));
			}
		}
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
	GList *ls;
	GList *le;
        GList *ret = NULL;
	gda_config_section *section;
	gda_config_entry *entry;
	gda_config_client *cfg_client;
	gint len;

        g_return_val_if_fail (path != NULL, NULL);

	len = strlen (path);
	cfg_client = get_config_client ();
	if (cfg_client->user){
		for (ls = cfg_client->user; ls; ls = ls->next){
			section = ls->data;
			if (!strcmp (path, section->path))
				for (le = section->entries; le; le = le->next){
					entry = le->data;
					if (entry && entry->name)
						ret = g_list_append (ret,
						      g_strdup (entry->name));
				}
		}
	}
		
	if (cfg_client->global){
		for (ls = cfg_client->global; ls; ls = ls->next){
			section = ls->data;
			if (!strcmp (path, section->path))
				for (le = section->entries; le; le = le->next){
					entry = le->data;
					if (entry && entry->name)
						if (!g_list_find_custom (ret, 
							 entry->name,
							 (GCompareFunc) strcmp))
							ret = g_list_append (ret,
							      g_strdup (entry->name));
				}
		}
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
gda_config_free_list (GList *list)
{
	g_list_foreach (list, (GFunc) g_free, NULL);
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
		gchar *ext;
		const gchar * (* plugin_get_name) (void);
		const gchar * (* plugin_get_description) (void);
		GList * (* plugin_get_cnc_params) (void);

		ext = g_strrstr (name, ".");
		if (!ext)
			continue;
		if (strcmp (ext, ".so"))
			continue;

		path = g_build_path (G_DIR_SEPARATOR_S, LIBGDA_PLUGINDIR,
				     name, NULL);
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

		gda_data_model_append_row (GDA_DATA_MODEL (model), value_list);
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

typedef struct {
	guint id;
	GdaConfigListenerFunc func;
	gpointer user_data;
	gchar *path;
} config_listener_data_t;

static GList *listeners;
static guint next_id;

static void
config_listener_func (gpointer data, gpointer user_data)
{
	GList *l;
	config_listener_data_t *listener = data;
	gchar *path = user_data;
	gint len;

	g_return_if_fail (listener != NULL);
	g_return_if_fail (path != NULL);

	len = strlen (path);
	for (l = listeners; l; l = l->next){
		listener = l->data;
		if (!strncmp (listener->path, path, len)){
			listener->func (path, listener->user_data);
		}
	}

}

static void
do_notify (const gchar *path)
{
	g_list_foreach (listeners, config_listener_func, (gpointer) path);
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
gda_config_add_listener (const gchar *path, 
			 GdaConfigListenerFunc func, 
			 gpointer user_data)
{
	config_listener_data_t *listener;

	g_return_val_if_fail (path != NULL, 0);
	g_return_val_if_fail (func != NULL, 0);

	listener = g_new (config_listener_data_t, 1);
	listener->id = next_id++;
	listener->func = func;
	listener->user_data = user_data;
	listener->path = g_strdup (path);
	listeners = g_list_append (listeners, listener);

	return listener->id;
}

/**
 * gda_config_remove_listener
 */
void
gda_config_remove_listener (guint id)
{
	GList *l;
	config_listener_data_t *listener;

	for (l = listeners; l; l = l->next){
		listener = l->data;
		if (listener->id == id){
			listeners = g_list_remove (listeners, listener);
			g_free (listener->path);
			g_free (listener);
			break;
		}
	}
}

