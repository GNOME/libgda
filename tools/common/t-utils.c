/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include "t-utils.h"
#include "t-app.h"
#include <glib/gi18n-lib.h>
#include <unistd.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <ctype.h>

/**
 * t_utils_fk_policy_to_string
 * @policy: a #GdaMetaForeignKeyPolicy
 *
 * Returns: the human readable version of @policy
 */
const gchar *
t_utils_fk_policy_to_string (GdaMetaForeignKeyPolicy policy)
{
	switch (policy) {
	default:
		g_assert_not_reached ();
	case GDA_META_FOREIGN_KEY_UNKNOWN:
		return _("Unknown");
	case GDA_META_FOREIGN_KEY_NONE:
		return _("not enforced");
	case GDA_META_FOREIGN_KEY_NO_ACTION:
		return _("stop with error");
	case GDA_META_FOREIGN_KEY_RESTRICT:
		return _("stop with error, not deferrable");
	case GDA_META_FOREIGN_KEY_CASCADE:
		return _("cascade changes");
	case GDA_META_FOREIGN_KEY_SET_NULL:
		return _("set to NULL");
	case GDA_META_FOREIGN_KEY_SET_DEFAULT:
		return _("set to default value");
	}
}

/**
 * t_utils_compute_prompt:
 *
 * Returns: (transfer full): the prompt
 */
gchar *
t_utils_compute_prompt (TContext *console, gboolean in_command, gboolean for_readline, ToolOutputFormat format)
{
	g_return_val_if_fail (T_IS_CONTEXT (console), NULL);

	const gchar *prefix = NULL;
	TConnection *tcnc;
	GString *string;
	gchar suffix = '>';

	string = g_string_new ("");

	if (format & BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM) {
		const gchar *color;
		color = base_tool_output_color_s (BASE_TOOL_COLOR_BOLD, format);
		if (color && *color) {
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_START_IGNORE);
#endif
			g_string_append (string, color);
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_END_IGNORE);
#endif
		}
	}
	tcnc = t_context_get_connection (console);
	if (tcnc) {
		prefix = t_connection_get_name (tcnc);
		if (t_connection_get_cnc (tcnc)) {
			GdaTransactionStatus *ts;
			ts = gda_connection_get_transaction_status (t_connection_get_cnc (tcnc));
			if (ts)
				suffix='[';
		}
	}
	else
		prefix = "gda";

	if (in_command) {
		gint i, len;
		len = strlen (prefix);
		for (i = 0; i < len; i++)
			g_string_append_c (string, ' ');
		g_string_append_c (string, suffix);
		g_string_append_c (string, ' ');		
	}
	else 
		g_string_append_printf (string, "%s%c ", prefix, suffix);

	if (format & BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM) {
		const gchar *color;
		color = base_tool_output_color_s (BASE_TOOL_COLOR_RESET, BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM);
		if (color && *color) {
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_START_IGNORE);
#endif
			g_string_append (string, color);
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_END_IGNORE);
#endif
		}
	}

	return g_string_free (string, FALSE);
}

/**
 * t_utils_split_text_into_single_commands:
 * @console: a #TContext
 * @commands: a string containing one or more internal or SQL commands
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Splits @commands into separate commands, either internal or SQL.
 * Also this function:
 *   - ignores comment lines (starting with '#')
 *   - ignores "would be commands" composed of only isspace() characters
 *
 * Returns: (transfer full): an array of strings, one for each actual command, or %NULL if @commands is %NULL or empty (""). Free using g_strfreev().
 */
gchar **
t_utils_split_text_into_single_commands (TContext *console, const gchar *commands, GError **error)
{
	g_return_val_if_fail (! console || T_IS_CONTEXT (console), NULL);

	if (!commands || !(*commands))
		return NULL;

	GArray *parts;
	parts = g_array_new (TRUE, FALSE, sizeof (gchar*));
	const gchar *start;
	for (start = commands; *start; ) {
	ignore_parts:
		/* ignore newlines */
		for (; *start == '\n'; start++);
		if (! *start)
			break;

		/* ignore comments */
		if (*start == '#') {
			for (; *start != '\n'; start++);
			if (! *start)
				break;
			goto ignore_parts;
		}

		/* ignore parts which are composed of spaces only */
		const gchar *hold = start;
		for (; *start && isspace (*start); start++);
		if (!*start) {
			if (parts->len == 0) {
				/* the whole @commands argument is empty => return NULL */
				g_array_free (parts, TRUE);
				parts = NULL;
			}
			break;
		}
		else if (start != hold)
			goto ignore_parts;

		/* real job here */
		if (base_tool_command_is_internal (start)) {
			/* determine end of internal command */
			const gchar *end;
			gboolean inquotes = FALSE;
			gchar *chunk;
			for (end = start; *end; end++) {
				if (*end == '\\') { /* ignore next char */
					end++;
					if (!*end)
						break;
					else
						continue;
				}

				if (*end == '"')
					inquotes = !inquotes;
				else if ((*end == '\n') && !inquotes)
					break;
			}
			if (inquotes) {
				/* error: quotes non closed */
				g_set_error (error, GDA_SQL_PARSER_ERROR, GDA_SQL_PARSER_SYNTAX_ERROR,
					     _("Syntax error"));
				gchar **res;
				res = (gchar **) g_array_free (parts, FALSE);
				g_strfreev (res);
				return NULL;
			}
			chunk = g_strndup (start, end - start);
			g_array_append_val (parts, chunk);

			if (*end)
				start = end + 1;
			else
				break;
		}
		else {
			/* parse statement */
			GdaStatement *stmt;
			const gchar *remain = NULL;
			GdaSqlParser *parser = NULL;
			if (console && t_context_get_connection (console))
				parser = t_connection_get_parser (t_context_get_connection (console));
			else
				parser = gda_sql_parser_new ();
			stmt = gda_sql_parser_parse_string (parser, start, &remain, error);
			if (!console)
				g_object_unref (parser);

			if (stmt) {
				g_object_unref (stmt);

				gchar *chunk;
				if (remain)
					chunk = g_strndup (start, remain - start);
				else
					chunk = g_strdup (start);
				g_array_append_val (parts, chunk);

				if (remain)
					start = remain;
				else
					break;
			}
			else {
				gchar **res;
				res = (gchar **) g_array_free (parts, FALSE);
				g_strfreev (res);
				return NULL;
			}
		}
	}

	if (parts)
		return (gchar **) g_array_free (parts, FALSE);
	else
		return NULL;
}

/**
 * t_utils_command_is_complete:
 * @console: a #TContext
 * @command: a string
 *
 * Determines if @command represents one or more (internal or SQL) complete commands, used by interactive
 * sessions.
 *
 * Returns: %TRUE if @command is determined to be complete
 */
gboolean
t_utils_command_is_complete (TContext *console, const gchar *command)
{
	if (!command || !(*command))
		return FALSE;

	gchar **parts;
	parts = t_utils_split_text_into_single_commands (console, command, NULL);
	if (!parts)
		return FALSE;

	guint i;
	gboolean retval = FALSE;
	for (i = 0; parts [i]; i++);
	if (i > 0) {
		const gchar *str;
		str = parts [i-1];

		if (*str) {
			if (base_tool_command_is_internal (str))
				retval = TRUE;
			else {
				guint l;
				for (l = strlen (str) - 1; (l > 0) && isspace (str[l]); l--); /* ignore spaces
											       * at the end */
				if (str[l] == ';')
					retval = TRUE; /* we consider the end of the command when there is a ';' */
			}
		}
	}
	g_strfreev (parts);
	return retval;
}

/**
 * t_utils_check_shell_argument:
 *
 * Check that the @arg string can safely be passed to a shell
 * to be executed, i.e. it does not contain dangerous things like "rm -rf *"
 */
// coverity[ +tainted_string_sanitize_content : arg-0 ]
gboolean
t_utils_check_shell_argument (const gchar *arg)
{
	const gchar *ptr;
	g_assert (arg);

	/* check for starting spaces */
	for (ptr = arg; *ptr == ' '; ptr++);
	if (!*ptr)
		return FALSE; /* only spaces is not allowed */

	/* check for the rest */
	for (; *ptr; ptr++) {
		if (! g_ascii_isalnum (*ptr) && (*ptr != G_DIR_SEPARATOR))
			return FALSE;
	}
	return TRUE;
}

/**
 * t_utils_term_compute_color_attribute:
 */
void
t_utils_term_compute_color_attribute ()
{
	ToolOutputFormat oformat;
	oformat = t_context_get_output_format (t_app_get_term_console ());
	oformat &= ~(BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM);

	FILE *ostream;
	ostream = t_context_get_output_stream (t_app_get_term_console (), NULL);
	if (!ostream || isatty (fileno (ostream))) {
		const gchar *term;
		oformat |= BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM;
		term = g_getenv ("TERM");
                if (term && !strcmp (term, "dumb"))
			oformat ^= BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM;
	}

	t_context_set_output_format (t_app_get_term_console (), oformat);
}

