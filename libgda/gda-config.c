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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <sys/stat.h>

#define LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S ".libgda"
#define LIBGDA_USER_CONFIG_FILE LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S \
				"config"

/* FIXME: many functions are not thread safe! */

typedef struct {
    gchar *name;
    gchar *type;
    gchar *value;
    /* gchar *mtime; */
    /* gchar *muser; */
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
			if (entry->name == NULL){
				g_warning ("NULL 'name' in an entry");
				entry->name = g_strdup ("");
			}

			entry->type =  xmlGetProp(cur, "type");
			if (entry->type == NULL){
				g_warning ("NULL 'type' in an entry");
				entry->type = g_strdup ("");
			}

			entry->value =  xmlGetProp(cur, "value");
			if (entry->value == NULL){
				g_warning ("NULL 'value' in an entry");
				entry->value = g_strdup ("");
			}

			/*
			entry->muser =  xmlGetProp(cur, "muser");
			if (entry->value == NULL){
				g_warning ("NULL 'muser' in an entry");
				entry->muser = g_strdup ("");
			}
			
			entry->mtime =  xmlGetProp(cur, "mtime");
			if (entry->value == NULL){
				g_warning ("NULL 'mtime' in an entry");
				entry->mtime = g_strdup ("");
			}
			*/
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
	gint sp_len;
	const gchar *section_path = GDA_CONFIG_SECTION_DATASOURCES;
	xmlFreeFunc old_free;
	xmlMallocFunc old_malloc;
	xmlReallocFunc old_realloc;
	xmlStrdupFunc old_strdup;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (len != 0, NULL);

	sp_len = strlen (section_path);

	xmlMemGet (&old_free,
		   &old_malloc,
		   &old_realloc,
		   &old_strdup);

	xmlMemSetup (g_free,
		     (xmlMallocFunc) g_malloc,
		     (xmlReallocFunc) g_realloc,
		     g_strdup);

	xmlDoValidityCheckingDefaultValue = FALSE;
	xmlKeepBlanksDefault(0);

	doc = xmlParseMemory (buffer, len);
	if (doc == NULL){
		g_warning ("File empty or not well-formed.");
		xmlMemSetup (old_free,
			     old_malloc,
			     old_realloc,
			     old_strdup);
		return NULL;
	}

	cur = xmlDocGetRootElement (doc);
	if (cur == NULL){
		g_warning ("Cannot get root element!");
		xmlFreeDoc (doc);
		xmlMemSetup (old_free,
			     old_malloc,
			     old_realloc,
			     old_strdup);
		return NULL;
	}

	if (strcmp (cur->name, "libgda-config") != 0){
		g_warning ("root node != 'libgda-config' -> '%s'", cur->name);
		xmlFreeDoc (doc);
		xmlMemSetup (old_free,
			     old_malloc,
			     old_realloc,
			     old_strdup);
		return NULL;
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (strcmp(cur->name, "section")) {
			g_warning ("'section' expected, got '%s'. Ignoring...", cur->name);
			continue;
		}

		section = g_new (gda_config_section, 1);
		section->path = xmlGetProp (cur, "path");
		if (section->path != NULL &&
		    !strncmp (section->path, section_path, sp_len)){
			section->entries = gda_config_read_entries (cur);
			if (section->entries == NULL) {
				g_free (section->path);
				g_free (section);
				continue;
			}
			
			list = g_list_append (list, section);
		} else {
			g_warning ("Ignoring section '%s'.", section->path);
			g_free (section->path);
			g_free (section);
		}
	}

	xmlFreeDoc (doc);
	xmlMemSetup (old_free,
		     old_malloc,
		     old_realloc,
		     old_strdup);
	return list;
}

static gda_config_client *
get_config_client ()
{
	/* FIXME: if we're updating or writing config_client,
	 *	  wait until the operation finishes
	 */
	if (config_client == NULL){
		gint len;
		gchar *full_file;
		gchar *user_config;

		config_client = g_new0 (gda_config_client, 1);
		xmlKeepBlanksDefault(0);
		if (g_file_get_contents (LIBGDA_GLOBAL_CONFIG_FILE, &full_file,
					 &len, NULL)){
			config_client->global = gda_config_parse_config_file (full_file, len);
			g_free (full_file);

		}

		user_config = g_strdup_printf ("%s%s", g_get_home_dir (),
						LIBGDA_USER_CONFIG_FILE);
		if (g_file_get_contents (user_config, &full_file, &len, NULL)){
			config_client->user = gda_config_parse_config_file (full_file, len);
			g_free (full_file);
		} else {
			if (!g_file_test (user_config, G_FILE_TEST_EXISTS)){
				gchar *dirpath;
				FILE *fp;

				dirpath = g_strdup_printf ("%s%s", g_get_home_dir (),
								LIBGDA_USER_CONFIG_DIR);
				if (!g_file_test (dirpath, G_FILE_TEST_IS_DIR)){
					if (mkdir (dirpath, 0700))
						g_warning ("Error creating directory %s", dirpath);
				}

				fp = fopen (user_config, "wt");
				if (fp == NULL)
					g_warning ("Unable to create the configuration file.");
				else
					fclose (fp);

				g_free (dirpath);
			} else
				g_warning ("Config file is not readable.");
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
	
	section = gda_config_search_section (cfg_client->user, section_path);
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
	xmlSetProp (new_node, "name", entry->name);
	xmlSetProp (new_node, "type", entry->type);
	xmlSetProp (new_node, "value", entry->value);
	/*
	xmlSetProp (new_node, "mtime", entry->mtime);
	xmlSetProp (new_node, "muser", entry->muser);
	*/
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
 * @path: path to the configuration entry.
 *
 * Gets the value of the specified configuration entry as a string. You
 * are then responsible to free the returned string.
 *
 * Returns: the value stored at the given entry.
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
 * @path: path to the configuration entry.
 *
 * Gets the value of the specified configuration entry as an integer.
 *
 * Returns: the value stored at the given entry.
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
 * @path: path to the configuration entry.
 *
 * Gets the value of the specified configuration entry as a float.
 *
 * Returns: the value stored at the given entry.
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
 * @path: path to the configuration entry.
 *
 * Gets the value of the specified configuration entry as a boolean.
 *
 * Returns: the value stored at the given entry.
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
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain a string.
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
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain an integer.
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
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain a float.
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
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain a boolean.
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
 * @path: path to the configuration section.
 *
 * Removes the given section from the configuration database.
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
 * @path: path to the configuration entry.
 *
 * Removes the given entry from the configuration database.
 * If the section is empty, also remove the section.
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
	if (section == NULL) {
		g_free (section_path);
		return;
	}

	for (le = section->entries; le; le = le->next){
		entry = le->data;
		if (!strcmp (entry->name, ptr_last_dash + 1))
			break;

		entry = NULL;
	}

	g_free (section_path);

	if (entry == NULL)
		return;

	section->entries = g_list_remove (section->entries, entry);
	free_entry (entry, NULL);
	if (section->entries == NULL) {
		cfg_client->user = g_list_remove (cfg_client->user, section);
		free_section (section, NULL);
	}

	write_config_file ();
	do_notify (path);
}

/**
 * gda_config_has_section
 * @path: path to the configuration section.
 *
 * Checks whether the given section exists in the configuration
 * system.
 *
 * Returns: %TRUE if the section exists, %FALSE otherwise.
 */
gboolean
gda_config_has_section (const gchar *path)
{
	gda_config_section *section;
	gda_config_client *cfg_client;

	g_return_val_if_fail (path != NULL, FALSE);

	cfg_client = get_config_client ();
	section = gda_config_search_section (cfg_client->user, path);
	if (section == NULL)
		section = gda_config_search_section (cfg_client->global, path);

	return (section != NULL) ? TRUE : FALSE;
}

/**
 * gda_config_has_key
 * @path: path to the configuration key.
 *
 * Checks whether the given key exists in the configuration system.
 *
 * Returns: %TRUE if the entry exists, %FALSE otherwise.
 */
gboolean
gda_config_has_key (const gchar *path)
{
	gda_config_entry *entry;
	gda_config_client *cfg_client;

	g_return_val_if_fail (path != NULL, FALSE);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, NULL);
	if (entry == NULL)
		entry = gda_config_search_entry (cfg_client->global,
						 path,
						 NULL);

	return (entry != NULL) ? TRUE : FALSE;
}

/**
 * gda_config_get_type
 * @path: path to the configuration key.
 *
 * Gets a string representing the type of the value of the given key.
 * The caller is responsible of freeing the returned value.
 *
 * Returns: %NULL if not found. Otherwise: "string", "float", "long", "bool".
 */
gchar *
gda_config_get_type (const gchar *path)
{
	gda_config_entry *entry;
	gda_config_client *cfg_client;

	g_return_val_if_fail (path != NULL, FALSE);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, NULL);
	if (entry == NULL)
		entry = gda_config_search_entry (cfg_client->global, 
						 path,
						 NULL);

	if (entry == NULL)
		return NULL;

	return g_strdup (entry->type);
}

/**
 * gda_config_list_sections
 * @path: path for root dir.
 *
 * Returns a GList containing the names of all the sections available
 * under the given root directory.
 *
 * To free the returned value, you can use #gda_config_free_list.
 *
 * Returns: a list containing all the section names.
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
 * @path: path for root dir.
 *
 * Returns a list of all keys that exist under the given path.
 *
 * To free the returned value, you can use #gda_config_free_list.
 *
 * Returns: a list containing all the key names.
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
 * @list: list to be freed.
 *
 * Frees all memory used by the given GList, which must be the return value
 * from either #gda_config_list_sections and #gda_config_list_keys.
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
 * Returns a list of all providers currently installed in
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
		if (strcmp (ext + 1, G_MODULE_SUFFIX))
			continue;

		path = g_build_path (G_DIR_SEPARATOR_S, LIBGDA_PLUGINDIR,
				     name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY);
		if (!handle) {
			g_warning (_("Error: %s"), g_module_error ());
			g_free (path);
			continue;
		}

		g_module_symbol (handle, "plugin_get_name",
				 (gpointer *) &plugin_get_name);
		g_module_symbol (handle, "plugin_get_description",
				 (gpointer *) &plugin_get_description);
		g_module_symbol (handle, "plugin_get_connection_params",
				 (gpointer *) &plugin_get_cnc_params);

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
 * Frees a list of #GdaProviderInfo structures.
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
 * @name: name of the provider to search for.
 *
 * Gets a #GdaProviderInfo structure from the provider list given its name.
 *
 * Returns: a #GdaProviderInfo structure, if found, or %NULL if not found.
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
 * gda_config_get_provider_model
 *
 * Fills and returns a new #GdaDataModel object using information from all 
 * providers which are currently installed in the system.
 *
 * Rows are separated in 3 columns: 
 * 'Id', 'Location' and 'Description'. 
 *
 * Returns: a new #GdaDataModel object. 
 */
GdaDataModel *
gda_config_get_provider_model (void)
{
	GList *prov_list;
	GList *l;
	GdaDataModel *model;

	model = gda_data_model_array_new (3);
	gda_data_model_set_column_title (model, 0, _("Id"));
	gda_data_model_set_column_title (model, 1, _("Location"));
	gda_data_model_set_column_title (model, 2, _("Description"));

	/* convert the returned GList into a GdaDataModelArray */
	prov_list = gda_config_get_provider_list ();
	for (l = prov_list; l != NULL; l = l->next) {
		GList *value_list = NULL;
		GdaProviderInfo *prov_info = (GdaProviderInfo *) l->data;

		g_assert (prov_info != NULL);

		value_list = g_list_append (value_list, gda_value_new_string (prov_info->id));
		value_list = g_list_append (value_list, gda_value_new_string (prov_info->location));
		value_list = g_list_append (value_list, gda_value_new_string (prov_info->description));

		gda_data_model_append_row (GDA_DATA_MODEL (model), value_list);
	}
	
	/* free memory */
	gda_config_free_provider_list (prov_list);
	
	return model;
}

/**
 * gda_provider_info_copy
 * @src: provider information to get a copy from.
 *
 * Creates a new #GdaProviderInfo structure from an existing one.

 * Returns: a newly allocated #GdaProviderInfo with contains a copy of 
 * information in @src.
 */
GdaProviderInfo*
gda_provider_info_copy (GdaProviderInfo *src)
{
	GdaProviderInfo *info;
	GList *list = NULL;
	GList *list_src = NULL;

	g_return_val_if_fail (src != NULL, NULL);

	info = g_new0 (GdaProviderInfo, 1);
	info->id = g_strdup (src->id);
	info->location = g_strdup (src->location);
	info->description = g_strdup (src->description);

	/* Deep copy: */
	list_src = src->gda_params;
	while (list_src) {
		list = g_list_append (list, g_strdup (list_src->data));
		list_src = g_list_next (list_src);
	}

	info->gda_params = list;

	return info;
}

/**
 * gda_config_free_provider_info
 * @provider_info: provider information to free.
 *
 * Deallocates all memory associated to the given #GdaProviderInfo.
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
 *
 * Returns a list of all data sources currently configured in the system.
 * Each of the nodes in the returned GList is a #GdaDataSourceInfo.
 * To free the returned list, call the #gda_config_free_data_source_list
 * function.
 *
 * Returns: a GList of #GdaDataSourceInfo structures.
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

		/* get the password */
		tmp = g_strdup_printf ("%s/%s/Password", GDA_CONFIG_SECTION_DATASOURCES, (char *) l->data);
		info->password = gda_config_get_string (tmp);
		g_free (tmp);

		list = g_list_append (list, info);
	}

	gda_config_free_list (sections);

	return list;
}

/**
 * gda_config_find_data_source
 * @name: name of the data source to search for.
 *
 * Gets a #GdaDataSourceInfo structure from the data source list given its 
 * name.
 *
 * Returns: a #GdaDataSourceInfo structure, if found, or %NULL if not found.
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
 * @src: data source information to get a copy from.
 *
 * Creates a new #GdaDataSourceInfo structure from an existing one.

 * Returns: a newly allocated #GdaDataSourceInfo with contains a copy of 
 * information in @src.
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
	info->password = g_strdup (src->password);

	return info;
}

/**
 * gda_config_free_data_source_info
 * @info: data source information to free.
 *
 * Deallocates all memory associated to the given #GdaDataSourceInfo.
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
	g_free (info->password);

	g_free (info);
}

/**
 * gda_config_free_data_source_list
 * @list: the list to be freed.
 *
 * Frees a list of #GdaDataSourceInfo structures.
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
 *
 * Fills and returns a new #GdaDataModel object using information from all 
 * data sources which are currently configured in the system.
 *
 * Rows are separated in 6 columns: 
 * 'Name', 'Provider', 'Connection string', 'Description', 'Username' and 
 * 'Password'.
 *
 * Returns: a new #GdaDataModel object. 
 */
GdaDataModel *
gda_config_get_data_source_model (void)
{
	GList *dsn_list;
	GList *l;
	GdaDataModel *model;

	model = gda_data_model_array_new (6);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("Provider"));
	gda_data_model_set_column_title (model, 2, _("Connection string"));
	gda_data_model_set_column_title (model, 3, _("Description"));
	gda_data_model_set_column_title (model, 4, _("Username"));
	gda_data_model_set_column_title (model, 5, _("Password"));
	
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
		value_list = g_list_append (value_list, gda_value_new_string ("******"));

		gda_data_model_append_row (GDA_DATA_MODEL (model), value_list);
	}
	
	/* free memory */
	gda_config_free_data_source_list (dsn_list);
	
	return model;
}

/**
 * gda_config_save_data_source
 * @name: name for the data source to be saved.
 * @provider: provider ID for the new data source.
 * @cnc_string: connection string for the new data source.
 * @description: description for the new data source.
 * @username: user name for the new data source.
 * @password: password to use when authenticating @username.
 *
 * Adds a new data source (or update an existing one) to the GDA
 * configuration, based on the parameters given.
 */
void
gda_config_save_data_source (const gchar *name,
			     const gchar *provider,
			     const gchar *cnc_string,
			     const gchar *description,
			     const gchar *username,
			     const gchar *password)
{
	GString *str;
	gint trunc_len;

	g_return_if_fail (name != NULL);
	g_return_if_fail (provider != NULL);

	str = g_string_new ("");
	g_string_printf (str, "%s/%s/", GDA_CONFIG_SECTION_DATASOURCES, name);
	trunc_len = strlen (str->str);
	
	/* set the provider */
	g_string_append (str, "Provider");
	gda_config_set_string (str->str, provider);
	g_string_truncate (str, trunc_len);

	/* set the connection string */
	if (cnc_string) {
		g_string_append (str, "DSN");
		gda_config_set_string (str->str, cnc_string);
		g_string_truncate (str, trunc_len);
	}

	/* set the description */
	if (description) {
		g_string_append (str, "Description");
		gda_config_set_string (str->str, description);
		g_string_truncate (str, trunc_len);
	}

	/* set the username */
	if (username) {
		g_string_append (str, "Username");
		gda_config_set_string (str->str, username);
		g_string_truncate (str, trunc_len);
	}

	/* set the password */
	if (password) {
		g_string_append (str, "Password");
		gda_config_set_string (str->str, password);
		g_string_truncate (str, trunc_len);
	}
	g_string_free (str, TRUE);
}

/**
 * gda_config_save_data_source_info
 * @dsn_info: a #GdaDataSourceInfo structure.
 *
 * Saves a data source in the libgda configuration given a
 * #GdaDataSourceInfo structure containing all the information
 * about the data source.
 */
void
gda_config_save_data_source_info (GdaDataSourceInfo *dsn_info)
{
	g_return_if_fail (dsn_info != NULL);

	gda_config_save_data_source (dsn_info->name,
				     dsn_info->provider,
				     dsn_info->cnc_string,
				     dsn_info->description,
				     dsn_info->username,
				     dsn_info->password);
}

/**
 * gda_config_remove_data_source
 * @name: name for the data source to be removed.
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
 * @id: the ID of the listener to remove.
 *
 * Removes a configuration listener previously installed with
 * #gda_config_add_listener, given its ID.
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

GType
gda_provider_info_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static ("GdaProviderInfo",
			(GBoxedCopyFunc) gda_provider_info_copy,
			(GBoxedFreeFunc) gda_config_free_provider_info);

  return our_type;
}

GType
gda_data_source_info_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static ("GdaDataSourceInfo",
			(GBoxedCopyFunc) gda_config_copy_data_source_info,
			(GBoxedFreeFunc) gda_config_free_data_source_info);

  return our_type;
}




