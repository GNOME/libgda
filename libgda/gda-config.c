/* GDA common library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *      Vivien Malerba <maletba@gnome-db.org>
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
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-parameter-list.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <sys/stat.h>
#ifdef HAVE_FAM
#include <fam.h>
#include <glib/giochannel.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

#define GDA_CONFIG_SECTION_DATASOURCES "/apps/libgda/Datasources"
#define LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S ".libgda"
#define LIBGDA_USER_CONFIG_FILE LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S "config"

/* FIXME: many functions are not thread safe! */

typedef struct {
	gchar *name;
	gchar *type;
	gchar *value;
} GdaConfigEntry;

typedef struct {
	gchar    *path;
	GList    *entries; /* list of GdaConfigEntry */
	gboolean  is_global;
} GdaConfigSection;

typedef struct {
	GList *global; /* list of GdaConfigSection */
	GList *user;   /* list of GdaConfigSection */
} GdaConfigClient;

static GdaConfigClient *config_client = NULL;
static gboolean         can_modify_global_conf; /* TRUE if the current user can modify the system wide config file */
static gboolean         lock_write_notify = FALSE;
static void             do_notify (const gchar *path);

/* if @dsn_list_only_in_mem is TRUE, then the DSN list is not written to any file, but
 * is just stored in memory, in @dsn_list_contents */
static gboolean  dsn_list_only_in_mem = FALSE;

/*
 * FAM delcarations and static variables
 */
#ifdef HAVE_FAM
static FAMConnection *fam_connection = NULL;
static gint           fam_watch_id = 0;
static gboolean       lock_fam = FALSE;
static FAMRequest    *fam_conf_user = NULL;
static FAMRequest    *fam_conf_global = NULL;
static time_t         last_mtime = 0;
static time_t         last_ctime = 0;
static off_t          last_size = 0;

static gboolean       fam_callback (GIOChannel *source, GIOCondition condition, gpointer data);
static void           fam_lock_notify ();
static void           fam_unlock_notify ();
#endif

/*
 * Private functions
 */
#ifdef GDA_DEBUG_NO
static void
dump_config_client ()
{
	if (config_client) {
		GList *list;

		list = config_client->global;
		while (list) {
			g_print ("GLOBAL section: %p %s\n", list->data, ((GdaConfigSection*)(list->data))->path);
			list = list->next;
		}
		list = config_client->user;
		while (list) {
			g_print ("USER   section: %p %s\n", list->data, ((GdaConfigSection*)(list->data))->path);
			list = list->next;
		}
	}
}
#endif

static GList *
gda_config_read_entries (xmlNodePtr cur)
{
	GList *list;
	GdaConfigEntry *entry;

	g_return_val_if_fail (cur != NULL, NULL);

	list = NULL;
	cur = cur->xmlChildrenNode;
	while (cur != NULL){
		if (!strcmp((gchar*)cur->name, "entry")){
			entry = g_new0 (GdaConfigEntry, 1);
			entry->name = (gchar*)xmlGetProp(cur, (xmlChar*)"name");
			if (entry->name == NULL){
				g_warning ("NULL 'name' in an entry");
				entry->name = g_strdup ("");
			}

			entry->type = (gchar*)xmlGetProp(cur, (xmlChar*)"type");
			if (entry->type == NULL){
				g_warning ("NULL 'type' in an entry");
				entry->type = g_strdup ("");
			}

			entry->value = (gchar*)xmlGetProp(cur, (xmlChar*)"value");
			if (entry->value == NULL){
				g_warning ("NULL 'value' in an entry");
				entry->value = g_strdup ("");
			}

			list = g_list_append (list, entry);
		} else {
			g_warning ("'entry' expected, got '%s'. Ignoring...", (gchar*)cur->name);
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
	GdaConfigSection *section;
	gint sp_len;
	const gchar *section_path = GDA_CONFIG_SECTION_DATASOURCES;
	xmlFreeFunc old_free;
	xmlMallocFunc old_malloc;
	xmlReallocFunc old_realloc;
	xmlStrdupFunc old_strdup;

	if (!buffer || (len == 0))
		return NULL;

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

	if (strcmp ((gchar*)cur->name, "libgda-config") != 0){
		g_warning ("root node != 'libgda-config' -> '%s'", (gchar*)cur->name);
		xmlFreeDoc (doc);
		xmlMemSetup (old_free,
			     old_malloc,
			     old_realloc,
			     old_strdup);
		return NULL;
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (xmlNodeIsText (cur)) 
			continue;

		if (strcmp ((gchar*)cur->name, "section")) {
			if (strcmp ((gchar*)cur->name, "comment"))
				g_warning ("'section' expected, got '%s'. Ignoring...", (gchar*)cur->name);
			continue;
		}

		section = g_new0 (GdaConfigSection, 1);
		section->path = (gchar*)xmlGetProp (cur, (xmlChar*)"path");
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

static GdaConfigClient *
get_config_client ()
{
	/* FIXME: if we're updating or writing config_client,
	 *	  wait until the operation finishes
	 */
	if (! config_client) {
		guint len = 0;
		gchar *full_file = NULL;
		gchar *user_config = NULL;
		gboolean has_user_config = FALSE;
		gchar *memonly;
		
		
		config_client = g_new0 (GdaConfigClient, 1);
		xmlKeepBlanksDefault(0);

		/* check if DSN list should only be stored in memory */
		memonly = getenv ("GDA_DSN_LIST_IN_MEMORY");
		if (memonly) {
			gsize length;
			gchar *init_contents;
			dsn_list_only_in_mem = TRUE;

			g_print ("** DSN list will remain in memory and will be lost at the end of the session **\n");
			if (*memonly && g_file_get_contents (memonly, &init_contents, &length, NULL)) {
				config_client->user = gda_config_parse_config_file (init_contents, length);
				g_free (init_contents);
			}
			return config_client;
		}

		has_user_config = g_get_home_dir () ? TRUE : FALSE;
		if (has_user_config)
			user_config = g_strdup_printf ("%s%s", g_get_home_dir (), LIBGDA_USER_CONFIG_FILE);

		{
			/* compute system wide rights */
			FILE *file;

			file = fopen (LIBGDA_GLOBAL_CONFIG_FILE, "a");
			if (file) {
				can_modify_global_conf = TRUE;
#ifdef GDA_DEBUG_NO
				g_print ("Can write system wide config file\n");
#endif
				fclose (file);
			}
			else {
				can_modify_global_conf = FALSE;
#ifdef GDA_DEBUG_NO
				g_print ("Cannot write system wide config file\n");
#endif			
			}
		}

#ifdef HAVE_FAM
		if (!fam_connection) {
			/* FAM init */
			GIOChannel *ioc;
			int res;
	
#ifdef GDA_DEBUG_NO
			g_print ("Using FAM to monitor configuration files changes.\n");
#endif
			fam_connection = g_malloc0 (sizeof (FAMConnection));
			if (FAMOpen2 (fam_connection, "libgnomedb user") != 0) {
				g_print ("FAMOpen failed, FAMErrno=%d\n", FAMErrno);
				g_free (fam_connection);
				fam_connection = NULL;
			}
			else {
				ioc = g_io_channel_unix_new (FAMCONNECTION_GETFD(fam_connection));
				fam_watch_id = g_io_add_watch (ioc,
							       G_IO_IN | G_IO_HUP | G_IO_ERR,
							       fam_callback, NULL);
				
				fam_conf_global = g_new0 (FAMRequest, 1);
				res = FAMMonitorFile (fam_connection, LIBGDA_GLOBAL_CONFIG_FILE, fam_conf_global, 
						      GINT_TO_POINTER (TRUE));
#ifdef GDA_DEBUG_NO
				g_print ("Monitoring changes on file %s: %s\n", 
					 LIBGDA_GLOBAL_CONFIG_FILE, res ? "ERROR" : "Ok");
#endif
				
				if (has_user_config) {
					fam_conf_user = g_new0 (FAMRequest, 1);
					res = FAMMonitorFile (fam_connection, user_config, fam_conf_user, 
							      GINT_TO_POINTER (FALSE));
#ifdef GDA_DEBUG_NO
					g_print ("Monitoring changes on file %s: %s\n", 
						 user_config, res ? "ERROR" : "Ok");
#endif
				}
			}
			
		}
#endif

		/*
		 * load system wide config
		 */
		if (g_file_get_contents (LIBGDA_GLOBAL_CONFIG_FILE, &full_file,
					 &len, NULL)){
			GList *list;
			config_client->global = gda_config_parse_config_file (full_file, len);
			g_free (full_file);
			
			/* mark GdaConfigSection as global */
			list = config_client->global;
			while (list) {
				((GdaConfigSection *)(list->data))->is_global = TRUE;
				list = g_list_next (list);
			}
		}

		if (! has_user_config) 
			/* this can occur on win98, and maybe other win32 platforms */
			return config_client;

		/*
		 * load user specific config
		 */
		if (g_file_get_contents (user_config, &full_file, &len, NULL)) {
			if (len != 0) 
				config_client->user = gda_config_parse_config_file (full_file, len);
			g_free (full_file);
		} 
		else {
			if (!g_file_test (user_config, G_FILE_TEST_EXISTS)){
				gchar *dirpath;
				FILE *fp;

				dirpath = g_strdup_printf ("%s%s", g_get_home_dir (), LIBGDA_USER_CONFIG_DIR);
				if (!g_file_test (dirpath, G_FILE_TEST_IS_DIR)){
#ifdef G_OS_WIN32
					if (mkdir (dirpath))
#else
					if (mkdir (dirpath, 0700))
#endif
						g_warning ("Error creating directory %s", dirpath);
				}

				fp = fopen (user_config, "wt");
				if (fp == NULL)
					g_warning ("Unable to create the configuration file.");
				else {
					gchar *str;
#define DB_FILE "sales_test.db"
#define DEFAULT_CONFIG \
"<?xml version=\"1.0\"?>\n" \
"<libgda-config>\n" \
"    <section path=\"/apps/libgda/Datasources/SalesTest\">\n" \
"        <entry name=\"DSN\" type=\"string\" value=\"DB_DIR=%s;DB_NAME=sales_test\"/>\n" \
"        <entry name=\"Description\" type=\"string\" value=\"Test database for a sales department\"/>\n" \
"        <entry name=\"Password\" type=\"string\" value=\"\"/>\n" \
"        <entry name=\"Provider\" type=\"string\" value=\"SQLite\"/>\n" \
"        <entry name=\"Username\" type=\"string\" value=\"\"/>\n" \
"    </section>\n" \
"</libgda-config>\n"
					str = g_build_filename (LIBGDA_GLOBAL_CONFIG_DIR, DB_FILE, NULL);
					if (g_file_get_contents (str, &full_file, &len, NULL)) {
						gboolean allok = TRUE;
						FILE *db;
						gchar *dbfile;
						
						/* copy the Sales test database */
						dbfile = g_build_filename (g_get_home_dir (), 
									   LIBGDA_USER_CONFIG_DIR, DB_FILE, NULL);
						db = fopen (dbfile, "wt");
						if (db) {
							allok = fwrite (full_file, sizeof (gchar), len, db) == len ? TRUE : FALSE;
							fclose (db);
						}
						else
							allok = FALSE;
						g_free (dbfile);
						g_free (full_file);
						
						if (allok) {
							/* write down a default data source entry */
							full_file = g_strdup_printf (DEFAULT_CONFIG, dirpath);
							len = strlen (full_file);
							fwrite (full_file, sizeof (gchar), len, fp);
							fclose (fp);
							config_client->user = gda_config_parse_config_file (full_file, len);
							g_free (full_file);
						}
					}
					g_free (str);
				}

				g_free (dirpath);
			} 
			else
				g_warning ("Config file is not readable.");
		}
		g_free (user_config);
#ifdef GDA_DEBUG_NO
		dump_config_client ();
#endif
	}

	return config_client;
}

static void gda_config_client_reset ();

#ifdef HAVE_FAM
static gboolean
fam_callback (GIOChannel *source, GIOCondition condition, gpointer data)
{
        gboolean res = TRUE;

	while (fam_connection && FAMPending (fam_connection)) {
                FAMEvent ev;
		gboolean is_global;
		
		if (FAMNextEvent (fam_connection, &ev) != 1) {
                        FAMClose (fam_connection);
                        g_free (fam_connection);
                        g_source_remove (fam_watch_id);
                        fam_watch_id = 0;
                        fam_connection = NULL;
                        return FALSE;
                }

		if (lock_fam)
			continue;

		is_global = GPOINTER_TO_INT (ev.userdata);
		switch (ev.code) {
		case FAMChanged: {
			struct stat stat;
			if (lstat (ev.filename, &stat))
				break;
			if ((stat.st_mtime != last_mtime) ||
			    (stat.st_ctime != last_ctime) ||
			    (stat.st_size != last_size)) {
				last_mtime = stat.st_mtime;
				last_ctime = stat.st_ctime;
				last_size = stat.st_size;
			}
			else
				break;
		}
		case FAMDeleted:
		case FAMCreated:
#ifdef GDA_DEBUG_NO
			g_print ("Reloading config files (%s config has changed)\n", is_global ? "global" : "user");
#endif
			gda_config_client_reset ();
			g_free (config_client);
			config_client = NULL;
			/* config_client will be re-created next time a gda_config* call is made */
			do_notify (NULL);
			break;
		case FAMAcknowledge:
		case FAMStartExecuting:
		case FAMStopExecuting:
		case FAMExists:
		case FAMEndExist:
		case FAMMoved:
			/* Not supported */
			break;
                }
	}
	
        return res;
}

static void
fam_lock_notify ()
{
	lock_fam = TRUE;
}

static void
fam_unlock_notify ()
{
	lock_fam = FALSE;
}

#endif

static GdaConfigSection *
gda_config_search_section (GList *sections, const gchar *path)
{
	GList *ls;
	GdaConfigSection *section;

	section = NULL;
	for (ls = sections; ls; ls = ls->next){
		section = ls->data;
		if (!strcmp (section->path, path))
			break;

		section = NULL;
	}

	return section;
}

static GdaConfigEntry *
gda_config_search_entry (GList *sections, const gchar *path, const gchar *type)
{
	gint last_dash;
	gchar *ptr_last_dash;
	gchar *section_path;
	GList *le;
	GdaConfigSection *section;
	GdaConfigEntry *entry;

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
			else {
				if (!strcmp (entry->name, ptr_last_dash + 1))
					break;
			}

			entry = NULL;
		}
	}

	g_free (section_path);
	return entry;
}

static GdaConfigSection *
gda_config_add_section (const gchar *section_path)
{
	GdaConfigClient *cfg_client;
	GdaConfigSection *section;

	section = g_new0 (GdaConfigSection, 1);
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
	GdaConfigEntry *entry;
	GdaConfigSection *section;
	GdaConfigClient *cfg_client;

	cfg_client = get_config_client ();
	entry = g_new0 (GdaConfigEntry, 1);
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
	GdaConfigEntry *entry = data;

	g_free (entry->name);
	g_free (entry->type);
	g_free (entry->value);
	g_free (entry);
}

static void
free_section (GdaConfigSection *section)
{
	g_list_foreach (section->entries, free_entry, NULL);
	g_list_free (section->entries);
	g_free (section->path);
	g_free (section);
}

static void
gda_config_client_reset ()
{
	GList *list;

	if (config_client == NULL)
		return;

	if (config_client->global) {
		for (list = config_client->global; list; list = g_list_next (list)) 
			free_section ((GdaConfigSection *)(list->data));
		g_list_free (config_client->global);
		config_client->global = NULL;
	}

	if (config_client->user) {
		for (list = config_client->user; list; list = g_list_next (list)) 
			free_section ((GdaConfigSection *)(list->data));
		g_list_free (config_client->user);
		config_client->user = NULL;
	}
}

static void
add_xml_entry (xmlNodePtr parent, GdaConfigEntry *entry)
{
	xmlNodePtr new_node;

	new_node = xmlNewTextChild (parent, NULL, (xmlChar*)"entry", NULL);
	xmlSetProp (new_node, (xmlChar*)"name", (xmlChar*)entry->name);
	xmlSetProp (new_node, (xmlChar*)"type", (xmlChar*)entry->type);
	xmlSetProp (new_node, (xmlChar*)"value", (xmlChar*)entry->value);
}

static xmlNodePtr
add_xml_section (xmlNodePtr parent, GdaConfigSection *section)
{
	xmlNodePtr new_node;

	new_node = xmlNewTextChild (parent, NULL, (xmlChar*)"section", NULL);
	xmlSetProp (new_node, (xmlChar*)"path", (xmlChar*)(section->path ? section->path : ""));
	return new_node;
}

static void
write_config_file ()
{
	GdaConfigClient *cfg_client;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr xml_section;
	GList *ls; /* List of sections */
	GList *le; /* List of entries */
	GdaConfigSection *section;
	GdaConfigEntry *entry;
	gchar *user_config;

	if (lock_write_notify)
		return;

	if (dsn_list_only_in_mem)
		/* nothing to do */
		return;

	/* user specific data sources */
	cfg_client = get_config_client ();
	doc = xmlNewDoc ((xmlChar*)"1.0");
	g_return_if_fail (doc != NULL);
	root = xmlNewDocNode (doc, NULL, (xmlChar*)"libgda-config", NULL);
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

	user_config = g_strdup_printf ("%s%s", g_get_home_dir (), LIBGDA_USER_CONFIG_FILE);
#ifdef HAVE_FAM
	fam_lock_notify ();
#endif
	if (xmlSaveFormatFile (user_config, doc, TRUE) == -1)
		g_warning ("Error saving config data to '%s'", user_config);
#ifdef HAVE_FAM
	fam_unlock_notify ();
#endif
	g_free (user_config);
	xmlFreeDoc (doc);

	/* system wide data sources */
	if (can_modify_global_conf) {
		doc = xmlNewDoc ((xmlChar*)"1.0");
		g_return_if_fail (doc != NULL);
		root = xmlNewDocNode (doc, NULL, (xmlChar*)"libgda-config", NULL);
		xmlDocSetRootElement (doc, root);
		for (ls = cfg_client->global; ls; ls = ls->next){
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
		
#ifdef HAVE_FAM
	fam_lock_notify ();
#endif
		if (xmlSaveFormatFile (LIBGDA_GLOBAL_CONFIG_FILE, doc, TRUE) == -1)
			g_warning ("Error saving config data to '%s'", user_config);
#ifdef HAVE_FAM
	fam_unlock_notify ();
#endif
		xmlFreeDoc (doc);
	}
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
	GdaConfigClient *cfg_client;
	GdaConfigEntry *entry;

	g_return_val_if_fail (path != NULL, NULL);

	cfg_client = get_config_client ();

	entry = gda_config_search_entry (cfg_client->user, path, "string");
	if (!entry)
		entry = gda_config_search_entry (cfg_client->global, path, "string");

        return (entry && entry->value) ? g_strdup (entry->value) : NULL;
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
	GdaConfigClient *cfg_client;
	GdaConfigEntry *entry;

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
	GdaConfigClient *cfg_client;
	GdaConfigEntry *entry;

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
	GdaConfigClient *cfg_client;
	GdaConfigEntry *entry;

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
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_set_string (const gchar *path, const gchar *new_value)
{
	GdaConfigEntry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	GdaConfigClient *cfg_client;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (new_value != NULL, FALSE);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "string");
	if (!entry) {
		entry = gda_config_search_entry (cfg_client->global, path, "string");
		if (entry && !can_modify_global_conf)
			return FALSE;
	}
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return FALSE;
		}
		last_dash = ptr_last_dash - path;
		section_path = g_strdup (path);
		section_path [last_dash] = '\0';
		gda_config_add_entry (section_path, ptr_last_dash + 1, 
						    "string", new_value);
		g_free (section_path);
	} 
	else {
		g_free (entry->value);
		g_free (entry->type);
		entry->value = g_strdup (new_value);
		entry->type = g_strdup ("string");
	}

	write_config_file ();
	do_notify (path);

	return TRUE;
}

/**
 * gda_config_set_int
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain an integer.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_set_int (const gchar *path, gint new_value)
{
	GdaConfigEntry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gchar *newstr;
	GdaConfigClient *cfg_client;

	g_return_val_if_fail (path !=NULL, FALSE);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "long");
	if (!entry) {
		entry = gda_config_search_entry (cfg_client->global, path, "long");
		if (entry && !can_modify_global_conf)
			return FALSE;
	}
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return FALSE;
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
		g_free (entry->type);
		entry->value = g_strdup_printf ("%d", new_value);
		entry->type = g_strdup ("long");
	}

	write_config_file ();
	do_notify (path);

	return TRUE;
}

/**
 * gda_config_set_float
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain a float.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_set_float (const gchar * path, gdouble new_value)
{
	GdaConfigEntry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gchar *newstr;
	GdaConfigClient *cfg_client;

	g_return_val_if_fail (path !=NULL, FALSE);

	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "float");
	if (!entry) {
		entry = gda_config_search_entry (cfg_client->global, path, "float");
		if (entry && !can_modify_global_conf)
			return FALSE;
	}
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return FALSE;
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
		g_free (entry->type);
		entry->value = g_strdup_printf ("%f", new_value);
		entry->type = g_strdup ("float");
	}

	write_config_file ();
	do_notify (path);

	return TRUE;
}

/**
 * gda_config_set_boolean
 * @path: path to the configuration entry.
 * @new_value: new value.
 *
 * Sets the given configuration entry to contain a boolean.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_set_boolean (const gchar *path, gboolean new_value)
{
	GdaConfigEntry *entry;
	gchar *ptr_last_dash;
	gchar *section_path;
	gint last_dash;
	gchar *newstr;
	GdaConfigClient *cfg_client;

	g_return_val_if_fail (path !=NULL, FALSE);

	new_value = new_value != 0 ? TRUE : FALSE;
	cfg_client = get_config_client ();
	entry = gda_config_search_entry (cfg_client->user, path, "bool");
	if (!entry) {
		entry = gda_config_search_entry (cfg_client->global, path, "bool");
		if (entry && !can_modify_global_conf)
			return FALSE;
	}
	if (entry == NULL){
		ptr_last_dash = strrchr (path, '/');
		if (ptr_last_dash == NULL){
			g_warning ("%s does not containt any '/'!?", path);
			return FALSE;
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
		g_free (entry->type);
		entry->value = g_strdup_printf ("%d", new_value);
		entry->type = g_strdup ("bool");
	}

	write_config_file ();
	do_notify (path);

	return TRUE;
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
	GdaConfigSection *section;
	GdaConfigClient *cfg_client;

	g_return_if_fail (path != NULL);

	cfg_client = get_config_client ();
	section = gda_config_search_section (cfg_client->user, path);
	if (!section)
		section = gda_config_search_section (cfg_client->global, path);
	if (section == NULL) {
		g_warning ("Section %s not found!", path);
		return;
	}

	cfg_client->user = g_list_remove (cfg_client->user, section);
	cfg_client->global = g_list_remove (cfg_client->global, section);
	free_section (section);
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
	GdaConfigSection *section;
	GdaConfigEntry *entry;
	GdaConfigClient *cfg_client;

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
	if (!section)
		section = gda_config_search_section (cfg_client->global, section_path);
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
		free_section (section);
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
	GdaConfigSection *section;
	GdaConfigClient *cfg_client;

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
	GdaConfigEntry *entry;
	GdaConfigClient *cfg_client;

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
	GdaConfigEntry *entry;
	GdaConfigClient *cfg_client;

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

static GList *
gda_config_list_sections_raw (const gchar *path)
{
	GdaConfigClient *cfg_client;
	GList *list;
        GList *ret = NULL;
	GdaConfigSection *section;
	gint len;

        g_return_val_if_fail (path != NULL, NULL);

	len = strlen (path);
	cfg_client = get_config_client ();
	for (list = cfg_client->user; list; list = list->next){
		section = (GdaConfigSection*) list->data;
		if (section && 
		    (len < strlen (section->path)) && 
		    !strncmp (path, section->path, len) && 
		    ((section->path[len] == 0) || (section->path[len] == '/')))
			ret = g_list_append (ret, section);
	}
		
	for (list = cfg_client->global; list; list = list->next){
		section = (GdaConfigSection*) list->data;
		if (section && 
		    (len < strlen (section->path)) && 
		    !strncmp (path, section->path, len) &&
		    ((section->path[len] == 0) || (section->path[len] == '/'))){
			if (!g_list_find_custom (ret, section->path + len + 1, (GCompareFunc) strcmp))
				ret = g_list_append (ret, section);
		}
	}

        return ret;
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
	gint len;
	GList *ret = NULL, *list, *tmp;

	len = strlen (path);
	list = gda_config_list_sections_raw (path);
	tmp = list;
	while (tmp) {
		ret = g_list_append (ret, g_strdup (((GdaConfigSection*) (tmp->data))->path + len + 1));
		tmp = g_list_next (tmp);
	}
	g_list_free (list);
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
	GdaConfigSection *section;
	GdaConfigEntry *entry;
	GdaConfigClient *cfg_client;
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

static GList *
load_providers_from_dir (const gchar *dirname, gboolean recurs)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *name;
	GList *list = NULL;
	
	/* read the plugin directory */
#ifdef GDA_DEBUG_NO
	g_print ("Loading providers in %s\n", dirname);
#endif
	dir = g_dir_open (dirname, 0, &err);
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
		void (*plugin_init) (const gchar *);
		const gchar * (* plugin_get_name) (void);
		const gchar * (* plugin_get_description) (void);
		gchar * (* plugin_get_dsn_spec) (void);
			
		if (recurs) {
			gchar *cname;
			cname = g_build_filename (dirname, name, NULL);
			if (g_file_test (cname, G_FILE_TEST_IS_DIR)) {
				GList *nlist;
				nlist = load_providers_from_dir (cname, TRUE);
				if (nlist)
					list = g_list_concat (list, nlist);
			}
			g_free (cname);
		}

		ext = g_strrstr (name, ".");
		if (!ext)
			continue;
		if (strcmp (ext + 1, G_MODULE_SUFFIX))
			continue;
			
		path = g_build_path (G_DIR_SEPARATOR_S, dirname,
				     name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY);
		if (!handle) {
			g_warning (_("Error: %s"), g_module_error ());
			g_free (path);
			continue;
		}
			
		if (g_module_symbol (handle, "plugin_init",
				     (gpointer *) &plugin_init))
			plugin_init (dirname);

		g_module_symbol (handle, "plugin_get_name",
				 (gpointer *) &plugin_get_name);
		g_module_symbol (handle, "plugin_get_description",
				 (gpointer *) &plugin_get_description);
		g_module_symbol (handle, "plugin_get_dsn_spec",
				 (gpointer *) &plugin_get_dsn_spec);
			
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
			
		info->dsn_spec = NULL;
		info->gda_params = NULL;

		if (plugin_get_dsn_spec) {
			GError *error = NULL;
				
			info->dsn_spec = plugin_get_dsn_spec ();
			if (info->dsn_spec) {
				info->gda_params = gda_parameter_list_new_from_spec_string (NULL, info->dsn_spec, &error);
				if (!info->gda_params) {
					g_warning ("Invalid format for provider '%s' DSN spec : %s",
						   info->id,
						   error ? error->message : "Unknown error");
					if (error)
						g_error_free (error);
				}
			}
			else {
				/* there may be traces of the provider installed but some parts are missing,
				   forget about that provider... */
				gda_provider_info_free (info);
				info = NULL;
			}
		}
		else 
			g_warning ("Provider '%s' does not provide a DSN spec", info->id);
			
		if (info) {
			list = g_list_append (list, info);
#ifdef GDA_DEBUG_NO
			g_print ("Loaded '%s' provider\n", info->id);
#endif
		}
			
		g_module_close (handle);
	}
		
	/* free memory */
	g_dir_close (dir);

	return list;
}

/**
 * gda_config_get_provider_list
 *
 * Returns a list of all providers currently installed in
 * the system. Each of the nodes in the returned GList
 * is a #GdaProviderInfo.
 *
 * Returns: a GList of #GdaProviderInfo structures, don't free or modify it!
 */
GList *
gda_config_get_provider_list (void)
{
	static GList *prov_list = NULL;

	if (!prov_list) {
		const gchar *from_dir;

		from_dir = getenv ("GDA_PROVIDERS_ROOT_DIR");
		if (from_dir)
			prov_list = load_providers_from_dir (from_dir, TRUE);
		else
			prov_list = load_providers_from_dir (LIBGDA_PLUGINDIR, FALSE);
	}

	return prov_list;
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
			gda_provider_info_free (provider_info);
	}

	g_list_free (list);
}

/**
 * gda_config_get_provider_by_name
 * @name: name of the provider to search for.
 *
 * Gets a #GdaProviderInfo structure from the provider list given its name,
 * don't modify or free it.
 *
 * Returns: a #GdaProviderInfo structure, if found, or %NULL if not found.
 */
GdaProviderInfo *
gda_config_get_provider_by_name (const gchar *name)
{
	GList *prov_list;
	GList *l;
	const gchar *tmpname;

	if (name)
		tmpname = name;
	else
		tmpname= "SQLite";

	prov_list = gda_config_get_provider_list ();

	for (l = prov_list; l; l = l->next) {
		GdaProviderInfo *provider_info = (GdaProviderInfo *) l->data;

		if (provider_info && !strcmp (provider_info->id, tmpname))
			return provider_info;
	}

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
		GValue *value;

		g_assert (prov_info != NULL);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, prov_info->id);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, prov_info->location);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, prov_info->description);
		value_list = g_list_append (value_list, value);

		gda_data_model_append_values (GDA_DATA_MODEL (model), value_list, NULL);
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}
	
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

	g_return_val_if_fail (src != NULL, NULL);

	info = g_new0 (GdaProviderInfo, 1);
	info->id = g_strdup (src->id);
	info->location = g_strdup (src->location);
	info->description = g_strdup (src->description);
	if (src->gda_params) {
		info->gda_params = src->gda_params;
		g_object_ref (info->gda_params);
	}

	return info;
}

/**
 * gda_provider_info_free
 * @provider_info: provider information to free.
 *
 * Deallocates all memory associated to the given #GdaProviderInfo.
 */
void
gda_provider_info_free (GdaProviderInfo *provider_info)
{
	g_return_if_fail (provider_info != NULL);

	g_free (provider_info->id);
	g_free (provider_info->location);
	g_free (provider_info->description);
	if (provider_info->gda_params)
		g_object_unref (provider_info->gda_params);
	if (provider_info->dsn_spec)
		g_free (provider_info->dsn_spec);

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
	gint len;

        len = strlen (GDA_CONFIG_SECTION_DATASOURCES);
	sections = gda_config_list_sections_raw (GDA_CONFIG_SECTION_DATASOURCES);
	for (l = sections; l != NULL; l = l->next) {
		gchar *tmp, *sect_name;
		GdaDataSourceInfo *info;

		sect_name = ((GdaConfigSection*) (l->data))->path + len + 1;
		info = g_new0 (GdaDataSourceInfo, 1);
		info->name = g_strdup ((const gchar *) sect_name);

		/* get the provider */
		tmp = g_strdup_printf ("%s/%s/Provider", GDA_CONFIG_SECTION_DATASOURCES, (char *) sect_name);
		info->provider = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the connection string */
		tmp = g_strdup_printf ("%s/%s/DSN", GDA_CONFIG_SECTION_DATASOURCES, (char *) sect_name);
		info->cnc_string = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the description */
		tmp = g_strdup_printf ("%s/%s/Description", GDA_CONFIG_SECTION_DATASOURCES, (char *) sect_name);
		info->description = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the user name */
		tmp = g_strdup_printf ("%s/%s/Username", GDA_CONFIG_SECTION_DATASOURCES, (char *) sect_name);
		info->username = gda_config_get_string (tmp);
		g_free (tmp);

		/* get the password */
		tmp = g_strdup_printf ("%s/%s/Password", GDA_CONFIG_SECTION_DATASOURCES, (char *) sect_name);
		info->password = gda_config_get_string (tmp);
		g_free (tmp);

		/* system wide data source ? */
		info->is_global = ((GdaConfigSection*) (l->data))->is_global;
				
		list = g_list_append (list, info);
	}

	g_list_free (sections);

	return list;
}

/**
 * gda_config_find_data_source
 * @name: name of the data source to search for.
 *
 * Gets a #GdaDataSourceInfo structure from the data source list given its 
 * name. After usage, the returned structure's memory must be freed using
 * gda_data_source_info_free().
 *
 * Returns: a new #GdaDataSourceInfo structure, if found, or %NULL if not found.
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

		if (tmp_info && !g_ascii_strcasecmp (tmp_info->name, name)) {
			info = gda_data_source_info_copy (tmp_info);
			break;
		}
	}

	gda_config_free_data_source_list (dsnlist);

	return info;
}

/**
 * gda_data_source_info_copy
 * @src: data source information to get a copy from.
 *
 * Creates a new #GdaDataSourceInfo structure from an existing one.

 * Returns: a newly allocated #GdaDataSourceInfo with contains a copy of 
 * information in @src.
 */
GdaDataSourceInfo *
gda_data_source_info_copy (GdaDataSourceInfo *src)
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
	info->is_global = src->is_global;

	return info;
}

#define str_is_equal(x,y) (((x) && (y) && !strcmp ((x),(y))) || (!(x) && (!y)))

/**
 * gda_data_source_info_equal
 * @info1: a data source information
 * @info2: a data source information
 * 
 * Tells if @info1 and @info2 are equal
 *
 * Returns: TRUE if @info1 and @info2 are equal
 */
gboolean
gda_data_source_info_equal (GdaDataSourceInfo *info1, GdaDataSourceInfo *info2)
{
	if (!info1 && !info2)
		return TRUE;
	if (!info1 || !info2)
		return FALSE;

	if (! str_is_equal (info1->name, info2->name))
		return FALSE;
	if (! str_is_equal (info1->provider, info2->provider))
		return FALSE;
	if (! str_is_equal (info1->cnc_string, info2->cnc_string))
		return FALSE;
	if (! str_is_equal (info1->description, info2->description))
		return FALSE;
	if (! str_is_equal (info1->username, info2->username))
		return FALSE;
	if (! str_is_equal (info1->password, info2->password))
		return FALSE;
	return info1->is_global == info2->is_global;
}

/**
 * gda_data_source_info_free
 * @info: data source information to free.
 *
 * Deallocates all memory associated to the given #GdaDataSourceInfo.
 */
void
gda_data_source_info_free (GdaDataSourceInfo *info)
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
		gda_data_source_info_free (info);
	}
}

/**
 * gda_config_get_data_source_model
 *
 * Fills and returns a new #GdaDataModel object using information from all 
 * data sources which are currently configured in the system.
 *
 * Rows are separated in 6 columns: 
 * 'Name', 'Provider', 'Connection string', 'Description', 'Username' and 'Global'.
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
	gda_data_model_set_column_title (model, 5, _("Global"));
	
	/* convert the returned GList into a GdaDataModelArray */
	dsn_list = gda_config_get_data_source_list ();
	for (l = dsn_list; l != NULL; l = l->next) {
		GList *value_list = NULL;
		GdaDataSourceInfo *dsn_info = (GdaDataSourceInfo *) l->data;
		GValue *value;

		g_assert (dsn_info != NULL);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, dsn_info->name);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, dsn_info->provider);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, dsn_info->cnc_string);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, dsn_info->description);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_set_string (value, dsn_info->username);
		value_list = g_list_append (value_list, value);

		value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
		g_value_set_boolean (value, dsn_info->is_global);
		value_list = g_list_append (value_list, value);
		
		gda_data_model_append_values (GDA_DATA_MODEL (model), value_list, NULL);
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}
	
	/* free memory */
	gda_config_free_data_source_list (dsn_list);
	
	return model;
}

/**
 * gda_config_can_modify_global_config
 *
 * Tells if the calling program can modify the global configured
 * data sources.
 * 
 * Returns: TRUE if modifications are possible
 */
gboolean
gda_config_can_modify_global_config (void)
{
	get_config_client ();
	return can_modify_global_conf;
}

/**
 * gda_config_save_data_source
 * @name: name for the data source to be saved.
 * @provider: provider ID for the new data source.
 * @cnc_string: connection string for the new data source.
 * @description: description for the new data source.
 * @username: user name for the new data source.
 * @password: password to use when authenticating @username.
 * @is_global: TRUE if the data source is system-wide
 *
 * Adds a new data source (or update an existing one) to the GDA
 * configuration, based on the parameters given.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_save_data_source (const gchar *name,
			     const gchar *provider,
			     const gchar *cnc_string,
			     const gchar *description,
			     const gchar *username,
			     const gchar *password,
			     gboolean is_global)
{
	GString *str;
	gint trunc_len;
	GdaConfigSection *section;
	GdaConfigClient *cfg_client;

	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (provider != NULL, FALSE);

	if (is_global && !can_modify_global_conf)
		return FALSE;

	/* delay write and nofity */
	lock_write_notify = TRUE;

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

	/* set the GdaSection in the right list (user of global) */
	cfg_client = get_config_client ();
	g_string_truncate (str, trunc_len - 1); /* we don't want the last '/' */
	section = gda_config_search_section (cfg_client->user, str->str);
	if (!section)
		section = gda_config_search_section (cfg_client->global, str->str);
	g_assert (section);
	section->is_global = is_global;

	if (section->is_global && (!g_list_find (cfg_client->global, section))) {
		cfg_client->global = g_list_append (cfg_client->global, section);
		cfg_client->user = g_list_remove (cfg_client->user, section);
	}

	if (!section->is_global && (!g_list_find (cfg_client->user, section))) {
		cfg_client->user = g_list_append (cfg_client->user, section);
		cfg_client->global = g_list_remove (cfg_client->global, section);
	}

	g_string_free (str, TRUE);

	/* actual write and nofity */
	lock_write_notify = FALSE;
	write_config_file ();
	do_notify (NULL);	

	return TRUE;
}

/**
 * gda_config_save_data_source_info
 * @dsn_info: a #GdaDataSourceInfo structure.
 *
 * Saves a data source in the libgda configuration given a
 * #GdaDataSourceInfo structure containing all the information
 * about the data source.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_save_data_source_info (GdaDataSourceInfo *dsn_info)
{
	g_return_val_if_fail (dsn_info != NULL, FALSE);

	return gda_config_save_data_source (dsn_info->name,
					    dsn_info->provider,
					    dsn_info->cnc_string,
					    dsn_info->description,
					    dsn_info->username,
					    dsn_info->password,
					    dsn_info->is_global);
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
static guint next_id = 1;

static void
config_listener_func (gpointer data, gpointer user_data)
{
	GList *l;
	config_listener_data_t *listener = data;
	gchar *path = user_data;
	gint len = -1;

	g_return_if_fail (listener != NULL);

	if (path)
		len = strlen (path);
	for (l = listeners; l; l = l->next){
		listener = l->data;
		if (!path ||
		    (path && !strncmp (listener->path, path, len))) {
			listener->func (path, listener->user_data);
		}
	}
}

static void
do_notify (const gchar *path)
{
	if (lock_write_notify)
		return;

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

	listener = g_new0 (config_listener_data_t, 1);
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
							 (GBoxedFreeFunc) gda_provider_info_free);

	return our_type;
}

GType
gda_data_source_info_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static ("GdaDataSourceInfo",
							 (GBoxedCopyFunc) gda_data_source_info_copy,
							 (GBoxedFreeFunc) gda_data_source_info_free);

	return our_type;
}
