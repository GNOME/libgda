/*
 * Copyright (C) 2012 Vivien Malerba <malerba@gnome-db.org>
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

#include "tool-help.h"
#include "tool-command.h"
#include "tool-input.h"
#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>

static xmlDocPtr helpdoc = NULL;

/**
 * tools_help_set_xml_file:
 *
 * Returns: %TRUE if file has been loaded
 */
gboolean
tool_help_set_xml_file (const gchar *file)
{
	if (helpdoc) {
		xmlFreeDoc (helpdoc);
		helpdoc = NULL;
	}
	if (file) {
		helpdoc = xmlParseFile (file);
		return helpdoc ? TRUE : FALSE;
	}
	else
		return TRUE;
}

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

static gchar *
help_xml_doc_to_string_command (xmlDocPtr helpdoc, const gchar *command_name, gint width, ToolOutputFormat format)
{
	xmlNodePtr node;
	node = xmlDocGetRootElement (helpdoc);
	if (!node || !command_name || !*command_name)
		return NULL;
	for (node = node->children; node; node = node->next) {
		if (strcmp ((gchar*) node->name, "command"))
			continue;
		xmlChar *prop;
		prop = xmlGetProp (node, BAD_CAST "name");
		if (prop && !strcmp ((gchar*) prop, command_name))
			break;
	}
	if (!node)
		return NULL;

	/* create output string */
	GString *string;
	string = g_string_new ("");
	for (node = node->children; node; node = node->next) {
		xmlChar *data = NULL;
		if (!strcmp ((gchar*) node->name, "shortdescription")) {
			data = xmlNodeGetContent (node);
			if (data) {
				append_to_string (string, (gchar*) data, width, 0);
				g_string_append (string, "\n\n");
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
								       tool_output_color_s (TOOL_COLOR_BOLD, format),
								       data,
								       tool_output_color_s (TOOL_COLOR_RESET, format));
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
help_xml_doc_to_string_section (xmlDocPtr helpdoc, const gchar *section_name, gint width)
{
	xmlNodePtr node;
	node = xmlDocGetRootElement (helpdoc);
	if (!node || !section_name || !*section_name)
		return NULL;
	for (node = node->children; node; node = node->next) {
		if (strcmp ((gchar*) node->name, "section"))
			continue;
		xmlChar *prop;
		prop = xmlGetProp (node, BAD_CAST "name");
		if (prop && strstr ((gchar*) prop, section_name))
			break;
	}
	if (!node)
		return NULL;

	/* create output string */
	GString *string;
	string = g_string_new ("");
	for (node = node->children; node; node = node->next) {
		xmlChar *data = NULL;
		if (!strcmp ((gchar*) node->name, "shortdescription")) {
			data = xmlNodeGetContent (node);
			if (data) {
				append_to_string (string, (gchar*) data, width, 0);
				g_string_append (string, "\n\n");
			}
		}
		else if (!strcmp ((gchar*) node->name, "usage") || !strcmp ((gchar*) node->name, "example")) {
			if (!strcmp ((gchar*) node->name, "example")) {
				append_to_string (string, _("Example"), width, 0);
				g_string_append (string, ":\n");
			}
			xmlNodePtr snode;
			for (snode = node->children; snode; snode = snode->next) {
				if (!strcmp ((gchar*) snode->name, "synopsis")) {
					data = xmlNodeGetContent (snode);
					if (data) {
						append_to_string (string, "> ", width, 3);
						append_to_string (string, (gchar*) data, width, 3);
						g_string_append_c (string, '\n');
					}
				}
				else if (!strcmp ((gchar*) snode->name, "comment")) {
					data = xmlNodeGetContent (snode);
					if (data) {
						append_to_string (string, (gchar*) data, width, 3);
						g_string_append_c (string, '\n');
					}
				}
				else if (!strcmp ((gchar*) snode->name, "raw")) {
					data = xmlNodeGetContent (snode);
					if (data) {
						append_raw_to_string (string, (gchar*) data, width, 3);
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
 */
ToolCommandResult *
tool_help_get_command_help (ToolCommandGroup *group, const gchar *command_name, ToolOutputFormat format)
{
	ToolCommandResult *res;
	GSList *list;
	gchar *current_group = NULL;
	GString *string = g_string_new ("");
#define NAMESIZE 18

	/* get term size */
	gint width = -1;
	if (format | TOOL_OUTPUT_FORMAT_DEFAULT)
		input_get_size (&width, NULL);

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_TXT;

	if (command_name) {
		ToolCommand *command = NULL;
		GError *lerror = NULL;
		if ((*command_name == '.') || (*command_name == '\\'))
			command_name = command_name + 1;
		command = tool_command_group_find (group, command_name, &lerror);
		if (command) {
			if (!helpdoc)
				helpdoc = load_help_doc ();

			gboolean done = FALSE;
			if (helpdoc) {
				gchar *tmp;
				tmp = help_xml_doc_to_string_command (helpdoc, command_name, width, format);
				if (tmp) {
					g_string_append (string, tmp);
					g_free (tmp);
					done = TRUE;
				}
			}
			if (!done) {
				append_to_string (string, command->description, width, 0);
				g_string_append_printf (string, "\n\n%s:\n   ", _("Usage"));
				tool_output_color_append_string (TOOL_COLOR_BOLD, format, string, ".");
				tool_output_color_append_string (TOOL_COLOR_BOLD, format, string, command->name_args);
			}
		}
		else {
			g_string_append_printf (string, "%s", lerror->message);
			g_clear_error (&lerror);
		}
	}
	else {
		GSList *all_commands;
		all_commands = tool_command_get_all_commands (group);
		for (list = all_commands; list; list = list->next) {
			ToolCommand *command = (ToolCommand*) list->data;
			gint clen;

#ifdef HAVE_LDAP
			if (g_str_has_prefix (command->name, "ldap") && cnc && !GDA_IS_LDAP_CONNECTION (cnc))
				continue;
#endif

			if (!current_group || strcmp (current_group, command->group)) {
				current_group = command->group;
				if (list != all_commands)
					g_string_append_c (string, '\n');
				if (width > 0) {
					gint i, nb, remain;
					nb = (width - g_utf8_strlen (current_group, -1) - 2) / 2;
					remain = width - (2 * nb + 2 + g_utf8_strlen (current_group, -1));
					for (i = 0; i < nb; i++)
						g_string_append_c (string, '=');
					g_string_append_c (string, ' ');
					append_to_string (string, current_group, width, 0);
					g_string_append_c (string, ' ');
					for (i = 0; i < nb + remain; i++)
						g_string_append_c (string, '=');
					g_string_append_c (string, '\n');
				}
				else {
					g_string_append (string, "=== ");
					append_to_string (string, current_group, width, 0);
					g_string_append (string, " ===\n");
				}

				if (!helpdoc)
					helpdoc = load_help_doc ();

				if (helpdoc && command->group_id) {
					gchar *tmp;
					tmp = help_xml_doc_to_string_section (helpdoc, command->group_id, width);
					if (tmp) {
						g_string_append (string, tmp);
						g_free (tmp);
						g_string_append_c (string, '\n');
					}
				}
			}

			g_string_append (string, "   ");
			tool_output_color_append_string (TOOL_COLOR_BOLD, format, string, ".");
			tool_output_color_append_string (TOOL_COLOR_BOLD, format, string, command->name);
			clen = g_utf8_strlen (command->name, -1);
			if (clen >= NAMESIZE)
				g_string_append_c (string, '\n');
			else {
				gint i, size;
				size = NAMESIZE - clen - 1;
				for (i = 0; i < size; i++)
					g_string_append_c (string, ' ');
			}
			append_to_string (string, command->description, width, NAMESIZE + 3);
			g_string_append_c (string, '\n');
		}
	}
	res->u.txt = string;
	return res;
}
