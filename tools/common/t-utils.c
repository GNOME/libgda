/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
 * t_utils_command_is_complete:
 */
gboolean
t_utils_command_is_complete (const gchar *command)
{
	if (!command || !(*command))
		return FALSE;
	if ((*command == '\\') || (*command == '.')) {
		/* internal command */
		return TRUE;
	}
	else if (*command == '#') {
		/* comment, to be ignored */
		return TRUE;
	}
	else {
		gint i, len;
		len = strlen (command);
		for (i = len - 1; i > 0; i--) {
			if ((command [i] != ' ') && (command [i] != '\n') && (command [i] != '\r'))
				break;
		}
		if (command [i] == ';')
			return TRUE;
		else
			return FALSE;
	}	
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

