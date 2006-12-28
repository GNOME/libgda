/* GDA - Command line configuration utility
 * Copyright (C) 2002 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <popt.h>
#include <libgda/libgda.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <readline/readline.h>
#include <readline/history.h>

#define PRINT_ALL	0
#define PRINT_SECTIONS	1
#define PRINT_ENTRIES	2

#define LIBGDA_USER_CONFIG_FILE "/.libgda/config"  /* Appended to $HOME */

/*
 * Declarations
 */
typedef enum {
	CMD_OK,
	CMD_UNKNOWN,
	CMD_ERROR,
	CMD_QUIT
} CmdResult;

typedef CmdResult (*mode_function) (const gchar *args);
typedef struct {
	gchar *command;
	gchar *help;
	mode_function function;
} cmd;

/*
 * Static variables
 */
static const gchar *prompt_gda = "gda> ";
static const gchar *prompt_config = "config> ";
/* static const gchar *prompt_query = "query> "; */
/* static const gchar *prompt_query_cont = "-> "; */

enum {
	ACTION_NONE = 0,
	ACTION_FILE = 1,
	ACTION_LIST_PROVIDERS = 1 << 1,
	ACTION_LIST_DATASOURCES = 1 << 2
};

typedef struct {
	gboolean config_file_g_free;
	gint actions;
	gchar *config_file;
	gchar *user;
	gchar *password;
	gchar *dsn;
	gchar *name;
	gchar *provider;
} CmdArguments;

CmdArguments cmdArgs;

static cmd *commands;
gboolean changed_config = FALSE;
static GList *config_data;
static GList *current_section;

static gchar *
get_input_line (const gchar *prompt)
{
	gchar *line;
	char *line_read;

	line = NULL;
	line_read = readline (prompt);

	if (line_read){
		if (*line_read)
			add_history (line_read);
		line = g_strdup (line_read);
		free (line_read);
	}

	return line;
}

static CmdResult
do_command (const gchar *line)
{
	gchar *stripped;
	gint cmdlen;
	gint i;
	gchar *command;
	CmdResult result;

	stripped = g_strstrip (g_strdup (line));
	i = 0;
	while ((command = commands [i].help) != NULL){
		command = commands [i].command;
		cmdlen = strlen (command);
		if (!g_strncasecmp (stripped, command, cmdlen)){
			gchar *ptr;

			ptr = NULL;
			if (stripped [cmdlen] == '\0')
				ptr = &stripped [cmdlen];
			else if (g_ascii_isspace (stripped [cmdlen]))
				ptr = &stripped [cmdlen + 1];
			else
				continue;

			result = (commands [i].function) (ptr);
			g_free (stripped);
			return result;
		}
		i++;
	}
	
	g_free (stripped);
	return CMD_UNKNOWN;
}

static CmdResult
process_commands (const gchar *prompt)
{
	gchar *line;
	CmdResult result;

	while ((line = get_input_line (prompt)) != NULL){
		if (*line == '\0'){
			g_free (line);
			continue;
		}

		result = do_command (line);
		if (result == CMD_QUIT){
			g_free (line);
			break;
		}

		if (result == CMD_UNKNOWN)
			g_print ("Error: Unknown command\n");
		
		g_free (line);
	}

	return ((line != NULL) ? CMD_OK : CMD_QUIT);
}

static CmdResult
cmd_quit (const gchar *unused)
{
	return CMD_QUIT;
}

static void
pretty_print (const gchar *ptr, gint wrap)
{
	gint i;

	while (*ptr != '\0'){
		for (i = 0; i < 80 - wrap && *ptr; i++){
			g_print ("%c", *ptr++);
			if (*ptr && i > (80 - wrap - 8) && g_ascii_isspace (*ptr))
				break;
		}

		g_print ("\n");

		if (*ptr){
			g_print ("%*s", wrap, "");
			while (*ptr && g_ascii_isspace (*ptr))
				ptr++;
		}
	}
}

static CmdResult
help (const gchar *unused)
{
	gchar *command;
	gint idx;

	idx = 0;
	while ((command = commands [idx].command) != NULL){
		g_print ("%14s  ", command);
		pretty_print (commands [idx].help, 16);
		idx++;
	}

	g_print ("\n");
	return CMD_OK;
}

static gboolean
unquote (gchar *str)
{
	gint len;
	
	if (*str != '"')
		return FALSE;

	len = strlen (str);
	if (len == 1 || str [len - 1] != '"')
		return FALSE;

	str [len - 1] = '\0';
	g_memmove (str, str + 1, len - 1);
	return TRUE;
}

static gboolean
yes_or_no (const gchar *msg)
{
	gchar *prompt;
	gchar *resp;
	gint result;

	result = ~(FALSE + TRUE);
	prompt = g_strdup_printf ("Are you sure you want to %s? (yes/no) ", msg);
	while (result == ~(FALSE + TRUE)){
		resp = get_input_line (prompt);
		if (resp == NULL)
			continue;

		if (!g_strcasecmp (resp, "yes"))
			result = TRUE;
		else if (!g_strcasecmp (resp, "no"))
			result = FALSE;
		else
			g_print ("Error: enter yes or no, please.\n");

		g_free (resp);
	}
	g_free (prompt);
	return (gboolean) result;
}

/*
 * Mode CONFIG functions
*/
typedef struct {
	gchar *name;
	gchar *type;
	gchar *value;
} gda_config_entry;

typedef struct {
	gchar *path;
	GList *entries;
} gda_config_section;


static GList *
config_read_entries (xmlNodePtr cur)
{
	GList *list;
	gda_config_entry *entry;

	g_return_val_if_fail (cur != NULL, NULL);

	list = NULL;
	cur = cur->xmlChildrenNode;
	while (cur != NULL){
		if (!strcmp(cur->name, "entry")){
			entry = g_new0 (gda_config_entry, 1);
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

			list = g_list_append (list, entry);
		} else {
			g_warning ("'entry' expected, got '%s'. Ignoring...\n", 
				   cur->name);
		}
		cur = cur->next;
	}
	
	return list;
}

static GList *
parse_config_file (const gchar *buffer, int len)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	GList *list = NULL;
	gda_config_section *section;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (len != 0, NULL);

	xmlKeepBlanksDefault (0);
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

	if (strcmp ((gchar*)cur->name, "libgda-config") != 0){
		g_error ("root node != 'libgda-config' -> '%s'", cur->name);
		xmlFreeDoc (doc);
		return NULL;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!strcmp(cur->name, "section")){
			section = g_new0 (gda_config_section, 1);
			section->path = xmlGetProp(cur, (xmlChar*)"path");
			if (section->path != NULL){
				section->entries = config_read_entries (cur);
				list = g_list_append (list, section);
			} else {
				g_warning ("ignoring section without 'path'!");
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
free_config_data ()
{
	g_list_foreach (config_data, free_section, NULL);
	g_list_free (config_data);
	config_data = NULL;
}

static gboolean
config_load_file (const gchar *file)
{
	gchar *file_contents;
	gint len;

	if (g_file_get_contents (file, &file_contents, &len, NULL)){
		config_data = parse_config_file (file_contents, len);
		g_free (file_contents);
		return TRUE;
	}

	return FALSE;
}

static void
printEntry (gpointer data, gpointer user_data)
{
	gda_config_entry *entry = data;
	gint print_mode = GPOINTER_TO_INT (user_data);
	gda_config_section *section;

	if (print_mode != PRINT_ENTRIES){
		g_print ("\tname: %s\n", entry->name);
		g_print ("\ttype: %s\n", entry->type);
		g_print ("\tvalue: %s\n\n", entry->value);
	}
	else {
		section = current_section->data;
		g_print ("entry %d ", g_list_index (section->entries, data));
		g_print ("name: \"%s\" ", entry->name);
		g_print ("type: \"%s\" ", entry->type);
		g_print ("value: \"%s\"\n", entry->value);
	}
}

static void
printSection (gpointer data, gpointer user_data)
{
	gda_config_section *section = data;
	gint print_mode = GPOINTER_TO_INT (user_data);
	gint idx;
	
	idx = g_list_index (config_data, section);
	g_print ("Section %d: %s\n", idx, section->path);

	if (print_mode == PRINT_ALL || print_mode == PRINT_ENTRIES){
		GList *saved_current;
		saved_current = current_section;
		current_section = g_list_nth (config_data, idx);
		if (section->entries != NULL)
			g_list_foreach (section->entries,
					printEntry,
					user_data);
		else
			g_print ("No entries.\n");

		current_section = saved_current;
	}
}

static CmdResult
config_list (const gchar *line)
{
	g_print ("-Begin-------------------------\n");
	g_list_foreach (config_data, printSection, GINT_TO_POINTER (PRINT_ALL));
	g_print ("-End----------------------------\n");
	return CMD_OK;
}

static GList *
search_entry (GList *entries, const gchar *name)
{
	GList *ls;
	gda_config_entry *entry;

	entry = NULL;
	for (ls = entries; ls; ls = ls->next){
		entry = ls->data;
		if (!strcmp (entry->name, name))
			break;

		entry = NULL;
	}
	return ls;
}

static GList *
search_section (GList *sections, const gchar *path)
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
	return ls;
}

static CmdResult
config_sections (const gchar *line)
{
	g_list_foreach (config_data, 
			printSection,
			GINT_TO_POINTER (PRINT_SECTIONS));
	return CMD_OK;
}

static CmdResult
config_entries (const gchar *line)
{
	if (current_section == NULL){
		g_print ("You must select a section before this command.\n");
		return CMD_ERROR;
	}

	printSection (current_section->data, GINT_TO_POINTER (PRINT_ENTRIES));
	return CMD_OK;
}

static CmdResult
config_select_section (const gchar *line)
{
	gchar *section_name;
	GList *new_section;

	section_name = g_strstrip (g_strdup (line));
	if (*section_name == '\0'){
		gda_config_section *section;

		if (current_section == NULL){
			g_print ("Usage: section \"section_name\"|number\n");
			g_print ("No section selected.\n");
			g_free (section_name);
			return CMD_ERROR;
		}

		section = current_section->data;
		g_print ("Section '%s' is currently selected.\n",
			 section->path);
		g_free (section_name);
		return CMD_OK;

	}
		
	new_section = search_section (config_data, section_name);
	if (new_section == NULL){
		/* Try selecting by section number */
		gchar *endptr [1];
		glong value;
		gda_config_section *section;
		
		value = strtol (section_name, endptr, 10);
		new_section = g_list_nth (config_data, (gint) value);
		if (new_section == NULL || **endptr != '\0'){
			g_print ("Error: Section '%s' not found or incorrect"
				 " section number.\n", section_name);
			g_free (section_name);
			return CMD_ERROR;
		}

		section = new_section->data;
		current_section = new_section;
		g_print ("Selected section %d -> '%s'\n", (gint) value,
							  section->path);
	}
	else {
		current_section = new_section;
		g_print ("Selected section '%s'.\n", section_name);
	}

	g_free (section_name);
	
	return CMD_OK;
}

static CmdResult
config_add_section (const gchar *line)
{
	gda_config_section *section;
	gchar *section_name;
	GList *ls;

	section_name = g_strstrip (g_strdup (line));
	if (*section_name == '\0'){
		g_print ("Usage: add-section \"section_name\"\n");
		g_free (section_name);
		return CMD_ERROR;
	}

	if (unquote (section_name) == FALSE){
		g_print ("Error: section name must be quoted.\n");
		g_free (section_name);
		return CMD_ERROR;
	}

	if ((ls = search_section (config_data, section_name)) != NULL){
		g_print ("Warning: section '%s' already exists. "
			 "Section selected.\n", section_name);
		g_free (section_name);
		current_section = ls;
		return CMD_OK;
	}
	
	section = g_new0 (gda_config_section, 1);
	section->path = section_name; /* no need to free it */
	section->entries = NULL;
	config_data = g_list_append (config_data, section);
	current_section = g_list_last (config_data);
	g_print ("Added section '%s' and selected it.\n", section_name);
	changed_config = TRUE;

	return CMD_OK;
}

static CmdResult
config_remove_section (const gchar *line)
{
	gda_config_section *section;
	gchar *section_name;
	GList *delete;
	gchar *endptr [1];
	glong value;
	gboolean is_number;

	value = 0;
	is_number = FALSE;
	delete = NULL;
	section_name = g_strstrip (g_strdup (line));
	if (*section_name == '\0'){
		/* If no arguments, remove selected section */
		if (current_section == NULL){
			g_print ("Error: no selected section nor section name "
				"specified.\n");
			g_free (section_name);
			return CMD_ERROR;
		}
		
		if (yes_or_no ("remove current selected entry") != TRUE){
			g_free (section_name);
			return CMD_OK;
		}

		delete = g_list_nth (config_data, g_list_index (
				     config_data, current_section->data));
		g_assert (delete != NULL);
	}
	else {
		is_number = g_ascii_isdigit (*section_name);
		if (!is_number && unquote (section_name) == FALSE){
			g_print ("Error: section name must be quoted.\n");
			g_free (section_name);
			return CMD_ERROR;
		}

		if (!is_number){
			delete = search_section (config_data, section_name);
			if (delete == NULL){
				g_print ("Error: section '%s' not found.\n", 
					 section_name);
				g_free (section_name);
				return CMD_ERROR;
			}
		}
		else {
			value = strtol (line, endptr, 10);
			delete = g_list_nth (config_data, (gint) value);
			if (delete == NULL || **endptr != '\0'){
				g_print ("Error: section '%s' not found or "
					 "section number out of range.\n",
					section_name);
				g_free (section_name);
				return CMD_ERROR;
			}
		}
	}

	section = delete->data;
	if (is_number)
		g_print ("Removed section %d -> '%s'\n", (gint) value,
							 section->path);
	else 
		g_print ("Removed section '%s'.\n", section->path);

	free_section (section, NULL);
	config_data = g_list_remove (config_data, section);
	if (current_section == delete)
		current_section = NULL;

	g_free (section_name);
	changed_config = TRUE;
	return CMD_OK;
}

static gchar *
get_quoted_string (const gchar *prompt)
{
	gchar *str;

	while (1){
		str = get_input_line (prompt);
		if (str == NULL)
			return NULL;
		if (unquote (str) == FALSE){
			if (*str == '\0'){
				g_free (str);
				return NULL;
			}

			g_print ("Don't forget the quotes!\n");
			g_free (str);
			continue;
		}

		return str;
	}
}

static gda_config_entry *
get_entry (gda_config_entry *entry)
{
	gchar *name;
	gchar *type;
	gchar *value;
	gchar *prompt;

	g_print ("Enter requested data in quotes, ie \"myName\".\n"
		 "When adding a new entry, pressing ENTER will abort "
		 "the insertion.\n"
		 "When editing an existing entry, pressing ENTER will "
		 "let the value as before.\n\n"
		 );

	prompt = (entry != NULL) ? g_strdup_printf ("Name (\"%s\"): ", 
						    entry->name) :
				   g_strdup ("Name: ");
	name = get_quoted_string (prompt);
	if (entry == NULL && name == NULL){
		g_free (prompt);
		return NULL;
	}

	g_free (prompt);
	prompt = (entry != NULL) ? g_strdup_printf ("Type (\"%s\"): ",
						    entry->type) :
				   g_strdup ("Type: ");
	type = get_quoted_string (prompt);
	if (entry == NULL && type == NULL){
		g_free (prompt);
		g_free (name);
		return NULL;
	}

	g_free (prompt);
	prompt = (entry != NULL) ? g_strdup_printf ("Value (\"%s\"): ",
						    entry->value) :
				   g_strdup ("Value: ");
	value = get_quoted_string (prompt);
	if (entry == NULL && value == NULL){
		g_free (prompt);
		g_free (name);
		g_free (type);
		return NULL;
	}

	g_free (prompt);
	if (entry != NULL){
		if (name == NULL && type == NULL && value == NULL)
			return NULL;

		if (name == NULL)
			name = entry->name;
		else
			g_free (entry->name);

		if (type == NULL)
			type = entry->type;
		else
			g_free (entry->type);

		if (value == NULL)
			value = entry->value;
		else
			g_free (entry->value);
	}
		
	if (entry == NULL)
		entry = g_new0 (gda_config_entry, 1);

	entry->name = name;
	entry->type = type;
	entry->value = value;
	return entry;
}

static CmdResult
config_add_entry (const gchar *line)
{
	gda_config_section *section;
	gda_config_entry *entry;

	if (current_section == NULL){
		g_print ("Error: no selected section.\n");
		return CMD_ERROR;
	}

	entry = get_entry (NULL);
	if (entry == NULL){
		g_print ("Ok, i won't add any entry by now.\n");
		return CMD_OK;
	}

	section = current_section->data;
	section->entries = g_list_append (section->entries, entry);
	g_print ("Added new entry.\n");
	changed_config = TRUE;

	return CMD_OK;
}

static CmdResult
config_remove_entry (const gchar *line)
{
	gda_config_section *section;
	gda_config_entry *entry;
	GList *entries;
	gchar *entry_name;
	gboolean is_number;
	gchar *endptr [1];
	glong value;

	value = 0;
	if (current_section == NULL){
		g_print ("Error: no selected section.\n");
		return CMD_ERROR;
	}

	entry_name = g_strstrip (g_strdup (line));
	if (*entry_name == '\0'){
		g_print ("Usage: remove-entry \"entry_name\"|number\n");
		g_free (entry_name);
		return CMD_ERROR;
	}

	entries = NULL;
	section = current_section->data;
	is_number = g_ascii_isdigit (*entry_name);
	if (!is_number && unquote (entry_name) == FALSE){
		g_print ("Error: entry name must be quoted.\n");
		g_free (entry_name);
		return CMD_ERROR;
	}

	if (!is_number){
		entries = search_entry (section->entries, entry_name);
		if (entries == NULL){
			g_print ("Error: entry name=\"%s\" not found.\n", entry_name);
			g_free (entry_name);
			return CMD_ERROR;
		}
	}
	else {
		value = strtol (entry_name, endptr, 10);
		entries = g_list_nth (section->entries, (gint) value);
		if (entries == NULL || **endptr != '\0'){
			g_print ("Error: entry '%s' out of range or "
				"incorrect entry number.\n", entry_name);
			g_free (entry_name);
			return CMD_ERROR;
		}
	}

	entry = entries->data;
	if (!is_number)
		g_print ("Removed entry name=\"%s\".\n", entry->name);
	else
		g_print ("Removed entry %d -> name=\"%s\".\n", (gint) value, 
								entry->name);

	free_entry (entry, NULL);
	section->entries = g_list_remove (section->entries, entry);
	g_free (entry_name);
	changed_config = TRUE;
	return CMD_OK;
}

static CmdResult
config_edit_entry (const gchar *line)
{
	gda_config_entry *entry;
	gda_config_section *section;
	gchar *entry_name;
	gboolean is_number;
	GList *entries;
	gchar *endptr [1];
	glong value;

	if (current_section == NULL){
		g_print ("Error: no selected section.\n");
		return CMD_ERROR;
	}

	entry_name = g_strstrip (g_strdup (line));
	if (*entry_name == '\0'){
		g_print ("Usage: edit-entry \"entry_name\"|number\n");
		g_free (entry_name);
		return CMD_OK;
	}

	section = current_section->data;
	is_number = g_ascii_isdigit (*entry_name);
	if (!is_number && unquote (entry_name) == FALSE){
		g_print ("Error: entry name must be quoted.\n");
		g_free (entry_name);
		return CMD_ERROR;
	}

	if (!is_number){
		entries = search_entry (section->entries, entry_name);
		if (entries == NULL){
			g_print ("Error: entry '%s' out of range or "
				"incorrect entry number.\n", entry_name);
			g_free (entry_name);
			return CMD_ERROR;
		}
	} else {
		value = strtol (entry_name, endptr, 10);
		entries = g_list_nth (section->entries, (gint) value);
		if (entries == NULL || **endptr != '\0'){
			g_print ("Error: entry '%s' out of range or "
				"incorrect entry number.\n", entry_name);
			g_free (entry_name);
			return CMD_ERROR;
		}
	}

	g_free (entry_name);
	entry = get_entry (entries->data);
	g_print (((entry == NULL) ? "No changes made.\n" :
				    "Entry changed.\n"));
	if (entry != NULL)
		changed_config = TRUE;

	return CMD_OK;
}

static void
add_xml_entry (xmlNodePtr parent, gda_config_entry *entry)
{
	xmlNodePtr new_node;

	new_node = xmlNewTextChild (parent, NULL, "entry", NULL);
	xmlSetProp(new_node, (xmlChar*)"name", entry->name);
	xmlSetProp(new_node, (xmlChar*)"type", entry->type);
	xmlSetProp(new_node, (xmlChar*)"value", entry->value);
}

static xmlNodePtr
add_xml_section (xmlNodePtr parent, gda_config_section *section)
{
	xmlNodePtr new_node;

	new_node = xmlNewTextChild (parent, NULL, "section", NULL);
	xmlSetProp(new_node, (xmlChar*)"path", section->path);
	return new_node;
}

static CmdResult
write_config_file (const gchar *file)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr xml_section;
	GList *ls; /* List of sections */
	GList *le; /* List of entries */
	gda_config_section *section;
	gda_config_entry *entry;

	doc = xmlNewDoc ("1.0");
	g_return_val_if_fail (doc != NULL, CMD_ERROR);
	root = xmlNewDocNode (doc, NULL, "libgda-config", NULL);
	xmlDocSetRootElement (doc, root);
	for (ls = config_data; ls; ls = ls->next){
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

	if (xmlSaveFormatFile (file, doc, TRUE) == -1){
		g_print ("Error: could not save data to '%s'.\n", file);
		xmlFreeDoc (doc);
		return CMD_ERROR;
	}

	changed_config = FALSE;
	xmlFreeDoc (doc);
	g_print ("Saved configuration to '%s'.\n", file);

	return CMD_OK;
}

static CmdResult
config_write (const gchar *line)
{
	gchar *file;
	CmdResult result;

	file = g_strstrip (g_strdup (line));
	if (*file == '\0'){
		g_free (file);
		if (changed_config == FALSE){
			g_print ("No changes made.\n");
			return CMD_OK;
		}
		result = write_config_file (cmdArgs.config_file);
	}
	else {
		result = write_config_file (file);
		g_free (file);
	}

	return result;
}

/*
 * Mode functions
 */
static CmdResult
mode_query (const gchar *unused)
{
	g_print ("query mode not implemented yet, sorry.\n");
	return CMD_OK;
}

static CmdResult
mode_config (const gchar *unused)
{
	CmdResult result;
	cmd *old_commands;
	static cmd mode_config_commands [] = {
		{"help", "Display this help.", help},
		{"list", "Display the whole configuration file held in memory.",
		 config_list},
		{"sections", "Display the sections in the configuration file.",
		 config_sections},
		{"select", "Selects a section to work on. "
		           "You can use a section number or a quoted section "
			   "name.",
		 config_select_section},
		{"entries", "Display the entries in the selected section.",
		 config_entries},
		{"add-section", "Creates a new section with the specified "
				"path. The path must be quoted.",
		 config_add_section},
		{"remove-section", "Removes the specified section (number or "
				   "full path) or the selected section if no"
				   " argument specified. The path should be"
				   " double quoted.",
		 config_remove_section},
		{"add-entry", "Add a new entry to the selected section.",
		 config_add_entry},
		{"remove-entry", "Removes an entry in the selected section"
				 " by its name or number. "
				 "Entry name should be double quoted.",
		 config_remove_entry},
		{"edit-entry", "Edit an entry in the selected section "
				"by name or number. "
				"Entry name should be double quoted.",
		 config_edit_entry},
		{ "write", "Writes the in-memory configuration to file. "
			   "If no file name given, it will write to the "
			   "default config file or the one specified in "
			   "the command line.",
		 config_write},
		{"quit", "Back to gda mode.", cmd_quit},
		{ NULL, NULL, NULL }
	};

	if (!config_load_file (cmdArgs.config_file)){
		g_print ("File '%s' not found or not readable. You "
			 "are going to start a new configuration.\n",
			 cmdArgs.config_file);
	}

	current_section = NULL;
	old_commands = commands;
	commands = mode_config_commands;
	do {
		result = process_commands (prompt_config);
		if (!changed_config)
			break;
		if (yes_or_no ("lose the changes made to the configuration"))
			break;
	} while (1);

	free_config_data ();
	commands = old_commands;
	return result;
}

static CmdResult
mode_gda ()
{
	static cmd mode_gda_commands [] = {
		{"help", "Display this help.", help},
		{"config", "Enters libgda configuration management mode.",
		 mode_config},
		{"query", "Enters query mode.", mode_query},
		{"quit", "Finish the program.", cmd_quit},
		{ NULL, NULL, NULL }
	};

	commands = mode_gda_commands;
	process_commands (prompt_gda);
	
	return CMD_OK;
}

/*
 * Readline/history related stuff
 */
static char *
dupstr (const char *orig)
{
	char *dest;
	int len;

	len = strlen (orig) + 1;
	dest = malloc (len);
	if (dest == NULL){
		perror (g_get_prgname ());
		exit (1);
	}
	memcpy (dest, orig, len);
	return dest;
}

static char *
cmd_generator (const char *text, int state)
{
	static int list_index, len;
	char *name;

	if (!state){
		list_index = 0;
		len = strlen (text);
	}

	while ((name = commands [list_index].command) != NULL){
		list_index++;
		if (!g_strncasecmp (name, text, len))
			return dupstr (name);
	}

	return NULL;
}

static char **
completion_func (const gchar *text, int start, int end)
{
	return ((start == 0) ? rl_completion_matches (text, cmd_generator) :
				NULL);
}

static void
initialize_readline ()
{
	rl_readline_name = g_get_prgname ();
	rl_attempted_completion_function = completion_func;
}

/*
 */

static void 
options (int argc, const char **argv)
{
	poptContext context;
	
	static const struct poptOption options[] = {
		{
			"config-file", 
			'c', 
			POPT_ARG_STRING, 
			&cmdArgs.config_file, 
			0, 
			"File to load the configuration from.",
			"filename"
		},
		{
			"list-providers",
			'l',
			POPT_ARG_VAL | POPT_ARGFLAG_OR,
			&cmdArgs.actions,
			ACTION_LIST_PROVIDERS,
			"Lists installed providers"
		},
		{
			"list-datasources",
			'L',
			POPT_ARG_VAL | POPT_ARGFLAG_OR,
			&cmdArgs.actions,
			ACTION_LIST_DATASOURCES,
			"Lists configured datasources"
		},
		{
			"name",
			'n',
			POPT_ARG_STRING,
			&cmdArgs.name,
			0,
			"User-assigned name for this connection."
		},
		{
			"user",
			'u',
			POPT_ARG_STRING,
			&cmdArgs.user,
			0,
			"User name to pass to the provider."
		},
		{
			"password",
			'p',
			POPT_ARG_STRING,
			&cmdArgs.password,
			0,
			"Password for the given user to connect to the DB "
			"backend."
		},
		{
			"provider",
			'P',
			POPT_ARG_STRING,
			&cmdArgs.provider,
			0,
			"Provider name."
		},
		{
			"DSN",
			'd',
			POPT_ARG_STRING,
			&cmdArgs.dsn,
			0,
			"Semi-colon separated string with name=value options "
			"to pass to the GDA provider."
		},
		POPT_AUTOHELP
		{NULL, '\0', 0, NULL, 0}
	};

	context = poptGetContext (NULL, argc, argv, options, 0);
	if (!context)
		return;

	if (poptGetNextOpt (context) < -1 || poptGetArg (context) != NULL){
		g_print ("-Error: Incorrect argument in command line.\n");
		poptPrintHelp (context, stderr, 0);
		poptFreeContext (context);
		exit (1);
	}

	poptFreeContext (context);

	if (!cmdArgs.name && (cmdArgs.user || 
			      cmdArgs.dsn  || 
			      cmdArgs.password ||
			      cmdArgs.provider)){
		g_print ("-Error: You must use -n option along -u, -d, -p and/or -P.\n");
		exit (1);
	}

	if (cmdArgs.name != NULL) {
		if (cmdArgs.actions != ACTION_NONE) {
			g_print ("-Error: cannot use -n in this context.\n");
			exit (1);
		}
		cmdArgs.actions |= ACTION_FILE;
	}

	if (cmdArgs.config_file == NULL){
		cmdArgs.config_file = g_strdup_printf ("%s%s", g_get_home_dir (),
						LIBGDA_USER_CONFIG_FILE);
		cmdArgs.config_file_g_free = TRUE;
	}
}

static GList *
change_entry (GList *entries,
	      const gchar *name,
	      const gchar *type,
	      const gchar *value)
{
	gda_config_entry *entry;
	GList *le;

	le = search_entry (entries, name);
	if (le == NULL){
		entry = g_new0 (gda_config_entry, 1);
		entries = g_list_append (entries, entry);
		entry->name = g_strdup (name);
	}
	else
		entry = le->data;

	g_free (entry->type);
	g_free (entry->value);
	entry->type = g_strdup (type);
	entry->value = g_strdup (value);
	return entries;
}

static void
batch_options ()
{
	gda_config_section *section;
	CmdResult result;
	gchar *line;

	/* don't care about return value of this one */
	(void) config_load_file (cmdArgs.config_file);
	
	line = g_strdup_printf ("\"/apps/libgda/Datasources/%s\"", cmdArgs.name);
	result = config_add_section (line);
	if (result != CMD_OK){
		g_print ("Error adding section %s\n", line);
		g_free (line);
		free_config_data ();
		return;
	}

	section = current_section->data;
	if (cmdArgs.provider)
		section->entries = change_entry (section->entries,
						 "Provider",
						 "string",
						 cmdArgs.provider);
	
	if (cmdArgs.user)
		section->entries = change_entry (section->entries,
						 "User",
						 "string",
						 cmdArgs.user);
	
	if (cmdArgs.password)
		section->entries = change_entry (section->entries,
						 "Password",
						 "string",
						 cmdArgs.password);

	if (cmdArgs.dsn)
		section->entries = change_entry (section->entries,
						 "DSN",
						 "string",
						 cmdArgs.dsn);

	write_config_file (cmdArgs.config_file);
	free_config_data ();
	g_free (line);
}

static void
add_param_name_to_string (gpointer data, gpointer user_data)
{
	GdaParameter *param = GDA_PARAMETER (data);
	GString *str = user_data;
	gchar *tmp;

	g_object_get (G_OBJECT (param), "string_id", &tmp, NULL);
	g_string_append_c (str, ' ');
	g_string_append (str, tmp);
	g_free (tmp);
}

static void
display_provider (gpointer data, gpointer user_data)
{
	GdaProviderInfo *info = data;
	GString *str;
	const char *desc = "Description: ";
	const char *paramInDsn = "DSN parameters:";

	g_print ("Provider name: %s\n", info->id);
	g_print (desc);
	pretty_print (info->description, strlen (desc));
	g_print (paramInDsn);
	str = g_string_new (NULL);
	if (info->gda_params) 
		g_slist_foreach (info->gda_params->parameters, add_param_name_to_string, str);
	else
		g_string_append (str, "No DSN (or wrong) DSN spec");
	pretty_print (str->str, strlen (paramInDsn) + 1);
	g_string_free (str, TRUE);
	g_print ("Location: %s\n", info->location);
	g_print ("---\n");
}

static void
list_providers ()
{
	GList *providers;
	
	if (cmdArgs.config_file_g_free == FALSE)
		g_print ("Warning: this providers are listed from %s/%s\n", 
			 g_get_home_dir (), LIBGDA_USER_CONFIG_FILE);

	providers = gda_config_get_provider_list ();
	if (providers == NULL){
		g_print ("No providers available!\n");
		return;
	}
	
	g_print ("---Providers list---\n");
	g_list_foreach (providers, display_provider, NULL);
}

static void
display_datasource (gpointer data, gpointer user_data)
{
	GdaDataSourceInfo *info = data;

	g_print ("Datasource name: %s\n", info->name);
	if (info->provider)
		g_print ("Provider: %s\n", info->provider);
	if (info->description)
		g_print ("Description: %s\n", info->description);
	if (info->cnc_string)
		g_print ("DSN: %s\n", info->cnc_string);
	if (info->username)
		g_print ("User name: %s\n", info->username);
	if (info->password)
		g_print ("Password: %s\n", info->password);
	g_print ("---\n");
}

static void
list_datasources ()
{
	GList *datasources;

	if (cmdArgs.config_file_g_free == FALSE)
		g_print ("Warning: this datasources are listed from %s/%s\n", 
			 g_get_home_dir (), LIBGDA_USER_CONFIG_FILE);

	datasources = gda_config_get_data_source_list ();
	if (datasources == NULL){
		g_print ("No datasources configured!\n");
		return;
	}
	
	g_print("---Datasources list---\n");
	g_list_foreach (datasources, display_datasource, NULL);
	gda_config_free_data_source_list (datasources);
}

/*
 * main
 */
int
main (int argc, char *argv [])
{
	options (argc, (const char **) argv);
	if (cmdArgs.actions == ACTION_NONE) {
		g_print ("Using configuration file from %s\n", cmdArgs.config_file);
		initialize_readline ();
		mode_gda ();
	} else {
		if ((cmdArgs.actions & ACTION_LIST_PROVIDERS) != 0)
			list_providers ();

		if ((cmdArgs.actions & ACTION_LIST_DATASOURCES) != 0)
			list_datasources ();

		if ((cmdArgs.actions & ACTION_FILE) != 0)
			batch_options ();

	} 

	if (cmdArgs.config_file_g_free)
		g_free (cmdArgs.config_file);
	else
		free (cmdArgs.config_file);

	g_print ("\n");
	return 0;
}

