/*
 * Copyright (C) 2012 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "base-tool-help.h"
#include "base-tool-command.h"
#include "base-tool-input.h"
#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>

static xmlDocPtr helpdoc = NULL;

/*
 * Same as append_to_string but cuts strings which are too long
 */
static void
append_raw_to_string (GString *string, gchar *str, gint width, gint offset)
{
	g_assert (string);
	if (!str)
		return;
	if ((width <= 0) && (offset <= 0)) {
		g_string_append (string, str);
		return;
	}
	if (offset < 0)
		offset = 0;

	/* start on a fresh line */
	if ((string->str) && (string->len > 0) &&
	    (string->str[string->len - 1] != '\n'))
		g_string_append_c (string, '\n');

	gchar *ptr;
	gboolean startofline = TRUE;
	for (ptr = str; *ptr; ) {
		gchar *next, *pnext;
		if (*ptr == '\n') {
			g_string_append_c (string, '\n');
			ptr++;
			startofline = TRUE;
			continue;
		}
		guint clen = 0;
		for (next = ptr, pnext = ptr;
		     *next && (*next != '\n');
		     pnext = next + 1, next = g_utf8_next_char (next), clen++) {
			if (startofline) {
				if (offset > 0) {
					gint i;
					for (i = 0; i < offset; i++)
						g_string_append_c (string, ' ');
				}
				startofline = FALSE;
			}

			if ((width > 0) && ((gint) clen >= width - offset)) {
				g_string_append_c (string, '\n');
				startofline = TRUE;
				for (; *next && (*next != '\n'); next++);
				if (*next == '\n')
					next++;
				break;
			}
			else {
				for (; pnext <= next; pnext++)
					g_string_append_c (string, *pnext);
			}
		}
		ptr = next;
	}
}

/*
 * parses @str, and appends to @string lines which are @width large, if @offset is >0, then
 * leave that amount of spaces. If @width <= 0, then only adds @offset spaces at the beginning of each
 * new line.
 */
static void
append_to_string (GString *string, gchar *str, gint width, gint offset)
{
	g_assert (string);
	if (!str)
		return;
	if ((width <= 0) && (offset <= 0)) {
		g_string_append (string, str);
		return;
	}
	if (offset < 0)
		offset = 0;

	/* replace @WORD@ by <WORD> */
	gchar *ptr;
	gboolean in = FALSE;
	for (ptr = str; *ptr; ptr++) {
		if (*ptr == '@') {
			if (in) {
				*ptr = '>';
				in = FALSE;
			}
			else {
				*ptr = '<';
				in = TRUE;
			}
		}
	}

	/* actual work */
	gboolean firstword = TRUE;
	gint clen = 0;
	if ((string->str) && (string->len > 0) &&
	    (string->str[string->len - 1] != '\n')) {
		for (ptr = string->str + (string->len -1); (ptr >= string->str) && (*ptr != '\n'); ptr --)
			clen++;
	}
	if (!strcmp (str, "> ")) {
		if (offset > 0) {
			gint i;
			for (i = 0; i < offset; i++)
				g_string_append_c (string, ' ');
		}
		g_string_append (string, str);
		return;
	}

	for (ptr = str; *ptr; ) {
		/* skip spaces */
		if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == '\r')) {
			ptr++;
			continue;
		}

		/* find end of next word */
		gchar *next;
		gint wlen;
		for (wlen = 0, next = ptr;
		     *next && (*next != ' ') && (*next != '\t') && (*next != '\n') && (*next != '\r');
		     wlen ++, next = g_utf8_next_char (next));

		if (wlen >= width - offset) {
			const gchar *n2;
			for (n2 = ptr; n2 < next; n2++) {
				g_string_append_c (string, *n2);
			}
			ptr = next;
			firstword = FALSE;
			g_string_append_c (string, '\n');
			clen = 0;
			continue;
		}
		else if ((width > 0) && ((wlen + clen) >= width)) {
			/* cut here */
			g_string_append_c (string, '\n');
			clen = 0;
			continue;
		}
		else {
			/* copy word */
			if (clen == 0) {
				if (offset > 0) {
					gint i;
					for (i = 0; i < offset; i++) {
						g_string_append_c (string, ' ');
						clen++;
					}
				}
			}
			else if (!firstword) {
				g_string_append_c (string, ' ');
				clen++;
			}
			const gchar *n2;
			for (n2 = ptr; n2 < next; n2++) {
				g_string_append_c (string, *n2);
				clen++;
			}
			ptr = next;
			firstword = FALSE;
		}
	}
}

/*
 * @node must be a <command> tag
 */
static gchar *
help_xml_doc_to_string_single_command (xmlNodePtr node, gint width, gboolean color_term)
{
	g_assert (!strcmp ((gchar*) node->name, "command"));

	/* create output string */
	GString *string;
	string = g_string_new ("");
	for (node = node->children; node; node = node->next) {
		xmlChar *data = NULL;
		if (!strcmp ((gchar*) node->name, "shortdescription")) {
			data = xmlNodeGetContent (node);
			if (data) {
				append_to_string (string, (gchar*) data, width, 0);
				g_string_append (string, "\n");
			}
		}
		else if (!strcmp ((gchar*) node->name, "usage") || !strcmp ((gchar*) node->name, "example")) {
			if (!strcmp ((gchar*) node->name, "usage"))
				append_to_string (string, _("Usage"), width, 0);
			else
				append_to_string (string, _("Example"), width, 0);
			g_string_append (string, ":\n");
			xmlNodePtr snode;
			for (snode = node->children; snode; snode = snode->next) {
				if (!strcmp ((gchar*) snode->name, "synopsis")) {
					data = xmlNodeGetContent (snode);
					if (data) {
						append_to_string (string, "> ", width, 3);
						gchar *tmp;
						tmp = g_strdup_printf ("%s%s%s",
								       base_tool_output_color_s (BASE_TOOL_COLOR_BOLD, color_term),
								       data,
								       base_tool_output_color_s (BASE_TOOL_COLOR_RESET, color_term));
						append_to_string (string, tmp, width, 3);
						g_free (tmp);
						g_string_append_c (string, '\n');
					}
				}
				else if (!strcmp ((gchar*) snode->name, "comment")) {
					data = xmlNodeGetContent (snode);
					if (data) {
						append_to_string (string, (gchar*) data, width, 6);
						g_string_append_c (string, '\n');
					}
				}
				else if (!strcmp ((gchar*) snode->name, "raw")) {
					data = xmlNodeGetContent (snode);
					if (data) {
						append_raw_to_string (string, (gchar*) data, width, 6);
						g_string_append (string, "\n\n");
					}
				}
				if (data)
					xmlFree (data);
				data = NULL;
			}
		}
		if (data)
			xmlFree (data);
	}

	return g_string_free (string, FALSE);
}

static gchar *
help_xml_doc_to_string_command (xmlNodePtr node, gint width, gboolean color_term)
{
	if (!strcmp ((gchar*) node->name, "command"))
		return help_xml_doc_to_string_single_command (node, width, color_term);
	else {
		GString *string;
		string = g_string_new ("");
		for (node = node->children; node; node = node->next) {
			if (!strcmp ((gchar*) node->name, "command")) {
				gchar *tmp;
				tmp = help_xml_doc_to_string_single_command (node, width, color_term);
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else if (!strcmp ((gchar*) node->name, "section")) {
				xmlChar *section_name;
				section_name = xmlGetProp (node, BAD_CAST "name");
				if (width > 0) {
					gint i, nb, remain;
					nb = (width - g_utf8_strlen ((gchar*) section_name, -1) - 2) / 2;
					remain = width - (2 * nb + 2 + g_utf8_strlen ((gchar*) section_name, -1));
					for (i = 0; i < nb; i++)
						g_string_append_c (string, '=');
					g_string_append_c (string, ' ');
					append_to_string (string, (gchar*) section_name, width, 0);
					g_string_append_c (string, ' ');
					for (i = 0; i < nb + remain; i++)
						g_string_append_c (string, '=');
					g_string_append_c (string, '\n');
				}
				else {
					g_string_append (string, "=== ");
					append_to_string (string, (gchar*) section_name, width, 0);
					g_string_append (string, " ===\n");
				}

				xmlNodePtr snode;
				for (snode = node->children; snode; snode = snode->next) {
					if (!strcmp ((gchar*) snode->name, "command")) {
						gchar *tmp;
						tmp = help_xml_doc_to_string_single_command (snode, width, color_term);
						g_string_append (string, tmp);
						g_free (tmp);
					}
					if (snode->next)
						g_string_append_c (string, '\n');
				}
			}
		}
		return g_string_free (string, FALSE);
	}
}

/**
 * base_tool_help_to_string:
 *
 * Returns: a new string
 */
gchar *
base_tool_help_to_string (ToolCommandResult *help, gboolean limit_width, gboolean color_term)
{
	return help_xml_doc_to_string_command (help->u.xml_node, limit_width, color_term);
}

/**
 * load_help_doc:
 */
static xmlDocPtr
load_help_doc (void)
{
	xmlDocPtr helpdoc = NULL;
	const gchar * const *langs = g_get_language_names ();
	gchar *dirname, *helpfile;
	gint i;
	dirname = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "gda-sql", "help", NULL);
	for (i = 0; langs[i]; i++) {
		helpfile = g_build_filename (dirname, langs[i], "gda-sql-help.xml", NULL);
		if (g_file_test (helpfile, G_FILE_TEST_EXISTS))
			helpdoc = xmlParseFile (helpfile);
		g_free (helpfile);
		if (helpdoc)
			break;
	}

	if (!helpdoc) {
		/* default to the "C" one */
		helpfile = g_build_filename (dirname, "C", "gda-sql-help.xml", NULL);
		if (g_file_test (helpfile, G_FILE_TEST_EXISTS))
			helpdoc = xmlParseFile (helpfile);
		g_free (helpfile);
	}
	g_free (dirname);
	return helpdoc;
}

/**
 * tools_help_get_command_help:
 *
 * Note: the XML node in the help part will be:
 * - either a <command> tag
 * - either a <commands> tag including <command> tags
 *
 * Returns: (transfer full): a new ToolCommandResult of type BASE_TOOL_COMMAND_RESULT_HELP
 */
ToolCommandResult *
base_tool_help_get_command_help (ToolCommandGroup *group, const gchar *command_name, GError **error)
{
	xmlNodePtr retnode = NULL;

	if (!helpdoc)
		helpdoc = load_help_doc ();

	if (command_name) {
		ToolCommand *command = NULL;
		if ((*command_name == '.') || (*command_name == '\\'))
			command_name = command_name + 1;
		command = base_tool_command_group_find (group, command_name, error);
		if (command) {
			gboolean done = FALSE;
			if (helpdoc) {
				xmlNodePtr node;
				node = xmlDocGetRootElement (helpdoc);
				if (node) {
					for (node = node->children; node; node = node->next) {
						if (strcmp ((gchar*) node->name, "command"))
							continue;
						xmlChar *prop;
						prop = xmlGetProp (node, BAD_CAST "name");
						if (prop && !strcmp ((gchar*) prop, command_name)) {
							retnode = xmlCopyNode (node, 1);
							done = TRUE;
							break;
						}
					}
				}
			}
			if (!done) {
				/* build XML tree */
				xmlNodePtr top, node;
				top = xmlNewNode (NULL, BAD_CAST "command");
				xmlSetProp (top, BAD_CAST "name", BAD_CAST command->name);
				node = xmlNewChild (top, NULL, BAD_CAST "shortdescription", BAD_CAST command->description);
				node = xmlNewChild (top, NULL, BAD_CAST "usage", NULL);
				gchar *tmp;
				tmp = g_strdup_printf (".%s", command->name_args);
				node = xmlNewChild (node, NULL, BAD_CAST "synopsis", BAD_CAST tmp);
				g_free (tmp);
				retnode = top;
			}
		}
	}
	else {
		GSList *all_commands, *list;
		xmlNodePtr commands, section;
		const gchar *current_section = NULL;
		commands = xmlNewNode (NULL, BAD_CAST "commands");
		retnode = commands;
		all_commands = base_tool_command_get_all_commands (group);
		for (list = all_commands; list; list = list->next) {
			ToolCommand *command = (ToolCommand*) list->data;
			if (!current_section || strcmp (current_section, command->group)) {
				section = xmlNewChild (commands, NULL, BAD_CAST "section", NULL);
				xmlSetProp (section, BAD_CAST "name", BAD_CAST command->group);
				current_section = command->group;
			}

			xmlNodePtr top, node;
			top = xmlNewChild (section, NULL, BAD_CAST "command", NULL);
			xmlSetProp (top, BAD_CAST "name", BAD_CAST command->name);
			node = xmlNewChild (top, NULL, BAD_CAST "shortdescription", BAD_CAST command->description);
			node = xmlNewChild (top, NULL, BAD_CAST "usage", NULL);
			gchar *tmp;
			tmp = g_strdup_printf (".%s", command->name_args);
			node = xmlNewChild (node, NULL, BAD_CAST "synopsis", BAD_CAST tmp);
			g_free (tmp);
		}
	}

	if (retnode) {
		ToolCommandResult *res = NULL;
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_HELP;
		res->u.xml_node = retnode;
		return res;
	}
	else
		return NULL;
}
