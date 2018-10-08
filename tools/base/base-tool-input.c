/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include "base-tool-errors.h"
#include "base-tool-input.h"
#include "base-tool-command.h"
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#ifndef G_OS_WIN32
  #include <sys/ioctl.h>
#endif

#define TO_IMPLEMENT g_print ("Implementation missing: %s() in %s line %d\n", __FUNCTION__, __FILE__,__LINE__)

#ifdef HAVE_READLINE
  #include <readline/readline.h>
#endif
#ifdef HAVE_HISTORY
  #include <readline/history.h>
#endif

#include "base-tool-defines.h"

#ifdef HAVE_HISTORY
static gboolean history_init_done = FALSE;
gchar *history_file = NULL;
#endif

static void init_history ();

/**
 * base_tool_input_from_stream
 *
 * returns: a new string read from @stream
 */
gchar *
base_tool_input_from_stream  (FILE *stream)
{
	#define LINE_SIZE 65535
	gchar line [LINE_SIZE];
	gchar *result;
	
	result = fgets (line, LINE_SIZE, stream);
	if (!result)
		return NULL;
	else {
		gint len = strlen (line);
		if (line [len - 1] == '\n')
			line [len - 1] = 0;
		return g_strdup (line);
	}
}

static ToolInputTreatLineFunc line_cb_func = NULL;
static gpointer      line_cb_func_data = NULL;
static ToolInputComputePromptFunc line_prompt_func = NULL;
static GIOChannel *ioc = NULL;

static gboolean
chars_for_readline_cb (G_GNUC_UNUSED GIOChannel *ioc, G_GNUC_UNUSED GIOCondition condition,
		       G_GNUC_UNUSED gpointer data)
{
	gboolean res = TRUE;

#ifdef HAVE_READLINE
        rl_callback_read_char ();
#else
	gchar *line; 
	gsize tpos;
	GError *error = NULL;
	GIOStatus st;
	st = g_io_channel_read_line (ioc, &line, NULL, &tpos, &error);
	switch (st) {
	case G_IO_STATUS_NORMAL:
		line [tpos] = 0;
		gboolean need_quit;
		need_quit = line_cb_func (line, line_cb_func_data);
		if (!need_quit) {
			/* print prompt for next line */
			g_print ("%s", line_prompt_func ());
		}
		res = !need_quit;
		g_free (line);
		break;
	case G_IO_STATUS_ERROR:
		g_warning ("Error reading from STDIN: %s\n",
			   error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
		break;
	case G_IO_STATUS_EOF:
		return FALSE;
		break;
	default:
		break;
	}
#endif
        return res;
}

#ifdef HAVE_READLINE
static void
line_read_handler (char *line)
{
	gboolean need_quit;
	need_quit = line_cb_func (line, line_cb_func_data); /* we don't care about the return status */
        free (line);
	if (need_quit)
		base_tool_input_end ();
	else
		rl_set_prompt (line_prompt_func ());
}
#endif

/**
 * base_tool_input_init:
 * @context: a #GMainContext
 *
 * Initializes input
 */
void
base_tool_input_init (GMainContext *context, ToolInputTreatLineFunc treat_line_func,
		      ToolInputComputePromptFunc prompt_func, gpointer data)
{
	/* init readline related features */
	line_cb_func = treat_line_func;
	line_cb_func_data = data;
	line_prompt_func = prompt_func;

#ifdef HAVE_READLINE
	rl_catch_signals = 1;
	rl_set_signals ();
	rl_readline_name = "gda-sql";
	rl_callback_handler_install (prompt_func (),  line_read_handler);
#else
	g_print ("%s", line_prompt_func ());
#endif
	if (!ioc) {
		ioc = g_io_channel_unix_new (STDIN_FILENO);
		GSource *src;
		src = g_io_create_watch (ioc, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL);
		g_source_set_callback (src, (GSourceFunc) chars_for_readline_cb, NULL, NULL);
		g_source_attach (src, context);
		g_source_unref (src);
	}

	/* init history */
	init_history ();

	/* completion init */
#ifdef HAVE_READLINE
	rl_basic_word_break_characters = " \t\n\\'`@$><=;|&{(";
	rl_completer_word_break_characters = " \t\n\\'`@$><=;|&{(";
#endif	
}

/**
 * base_tool_input_end
 *
 * Releases any data related to the input and allocated during base_tool_input_init()
 */
void
base_tool_input_end (void)
{
#ifdef HAVE_READLINE
	rl_callback_handler_remove ();
#endif
	if (ioc) {
		g_io_channel_shutdown (ioc, TRUE, NULL);
		g_io_channel_unref (ioc);
		ioc = NULL;
	}
}

#ifdef HAVE_READLINE
static gsize
determine_max_prefix_len (gchar **array)
{
        gsize max;
        g_assert (array[0]);
        g_assert (array[1]);

        for (max = 1; ; max++) {
                gint i;
                gchar *ref;
                ref = array[0];
                for (i = 1; array[i]; i++) {
                        if (strncmp (ref, array[i], max))
                                break;
			if (!array[i][max]) {
				max ++;
				break;
			}
                }
                if (array[i])
                        break;
        }

        return max - 1;
}
#endif

static ToolCommandGroup *user_defined_completion_group = NULL;
static ToolInputCompletionFunc user_defined_completion_func = NULL;
static gpointer user_defined_completion_func_data = NULL;
static gchar *user_defined_chars_to_ignore = NULL;

#ifdef HAVE_READLINE
static char **
_base_tool_completion (const char *text, int start, int end)
{
	GArray *array = NULL;
        gchar *match, *prematch;
	gboolean treated = FALSE;
        match = g_strndup (rl_line_buffer + start, end - start);
	g_assert (!strcmp (text, match));
	prematch = g_strndup (rl_line_buffer, start);
	g_strstrip (prematch);
	g_strstrip (match);

        if (start == 0) {
                /* user needs to enter a command */
		if (!user_defined_completion_group)
			goto out;

		gchar *cmd_match;
		gchar start_char = 0;
		cmd_match = match;

		/* maybe ignore some start chars */
		if (user_defined_chars_to_ignore) {
			gchar *ptr;
			for (ptr = user_defined_chars_to_ignore; *ptr; ptr++) {
				if (*cmd_match == *ptr) {
					cmd_match ++;
					start_char = *ptr;
					break;
				}
			}
		}

		if (!user_defined_chars_to_ignore || (start_char && user_defined_chars_to_ignore)) {
			GSList *commands;
			commands = base_tool_command_get_commands (user_defined_completion_group, cmd_match);
			if (commands) {
				array = g_array_new (TRUE, FALSE, sizeof (gchar*));
				GSList *list;
				for (list = commands; list; list = list->next) {
					ToolCommand *tc = (ToolCommand*) list->data;
					const gchar *tmp;
					if (start_char)
						tmp = g_strdup_printf ("%c%s", start_char, tc->name);
					else
						tmp = g_strdup (tc->name);
					g_array_append_val (array, tmp);
				}
				g_slist_free (commands);
			}
			rl_completion_append_character = ' ';
			treated = TRUE;
		}
        }

	if (!treated && user_defined_completion_func) {
		gchar **vals;
		ToolCommand *tc = NULL;
		if (*prematch && user_defined_completion_group) {
			gchar *tmp = prematch;
			if (user_defined_chars_to_ignore) {
				gchar *ptr;
				for (ptr = user_defined_chars_to_ignore; *ptr; ptr++) {
					if (*tmp == *ptr) {
						tmp ++;
						break;
					}
				}
			}
			tc = base_tool_command_group_find (user_defined_completion_group, tmp, NULL);
		}
		if (tc && tc->completion_func)
			vals = tc->completion_func (text, tc->completion_func_data);
		else
			vals = user_defined_completion_func (text, rl_line_buffer, start, end,
							     user_defined_completion_func_data);

		if (vals) {
			guint i;
			for (i = 0; vals [i]; i++) {
				if (!array)
					array = g_array_new (TRUE, FALSE, sizeof (gchar*));
				gchar *tmp;
				tmp = vals[i];
				g_array_append_val (array, tmp);
			}
			g_free (vals); /* and not g_strfreev() */
			rl_completion_append_character = 0;
		}
	}

 out:
	g_free (match);
	g_free (prematch);
	if (array) {
                if (array->len > 1) {
                        /* determine max string in common to all possible completions */
                        gsize len;
                        len = determine_max_prefix_len ((gchar**) array->data);
                        if (len > 0) {
                                gchar *tmp;
                                tmp = g_strndup (g_array_index (array, gchar*, 0), len);
                                g_array_prepend_val (array, tmp);
                        }
			else {
				gchar *tmp;
                                tmp = g_strdup ("");
                                g_array_prepend_val (array, tmp);
			}
                }
                return (gchar**) g_array_free (array, FALSE);
        }
        else
                return NULL;
}
#endif /* HAVE_READLINE */

/**
 * base_tool_input_set_completion_func:
 * @group: (nullable): a #ToolCommandGroup, or %NULL
 * @func: (nullable): a #ToolInputCompletionFunc, or %NULL
 * @func_data: (nullable): a pointer to pass when calling @func
 * @start_chars_to_ignore: (nullable): a list of characters to ignore at the beginning of commands
 *
 * Defines the completion function.
 */
void
base_tool_input_set_completion_func (ToolCommandGroup *group, ToolInputCompletionFunc func, gpointer func_data, gchar *start_chars_to_ignore)
{
	user_defined_completion_group = group;
	user_defined_completion_func = func;
	user_defined_completion_func_data = func_data;
	g_free (user_defined_chars_to_ignore);
	user_defined_chars_to_ignore = start_chars_to_ignore ? g_strdup (start_chars_to_ignore) : NULL;

#ifdef HAVE_READLINE
	if (group || func)
		rl_attempted_completion_function = _base_tool_completion;
	else
		rl_attempted_completion_function = NULL;
#endif	
}

/*
 * base_tool_input_get_size
 *
 * Get the size of the input term, if possible, otherwise returns -1
 */
void
base_tool_input_get_size (gint *width, gint *height)
{
	int cols = -1, rows = -1;

#ifdef TIOCGWINSZ
	int tty = fileno (stdin);
	struct winsize window_size;
	if (ioctl (tty, TIOCGWINSZ, &window_size) == 0)	{
		cols = (int) window_size.ws_col;
		rows = (int) window_size.ws_row;
	}

	if (cols <= 1)
		cols = -1;
	if (rows <= 0)
		rows = -1;
#endif 

	if (width)
		*width = cols;
	if (height)
		*height = rows;

	/*g_print ("Screen: %dx%d\n", cols, rows);*/
}

#ifdef HAVE_HISTORY
static void
sanitize_env (gchar *str)
{
	gchar *ptr;
	for (ptr = str; *ptr; ptr++) {
                if (! g_ascii_isprint (*ptr) || (*ptr == G_DIR_SEPARATOR))
                        *ptr = '_';
        }
}
#endif

/**
 * init_history
 *
 * Loads the contents of the history file, if supported
 */
static void
init_history ()
{
#ifdef HAVE_HISTORY
	if (history_init_done)
		return;
	if (getenv (BASE_TOOL_HISTORY_ENV_NAME)) {
		const gchar *ename;
		ename = getenv (BASE_TOOL_HISTORY_ENV_NAME);
		if (!ename || !*ename || !strcmp (ename, "NO_HISTORY")) {
			history_init_done = TRUE;
			return;
		}
		history_file = g_strdup (ename);
		sanitize_env (history_file);
	}
	else {
		gchar *cache_dir;
#ifdef LIBGDA_ABI_NAME
		cache_dir = g_build_filename (g_get_user_cache_dir (), "libgda", NULL);
#else
		gchar *tmp;
		tmp = g_utf8_strdown (BASE_TOOL_NAME, -1);
		cache_dir = g_build_filename (g_get_user_cache_dir (), tmp, NULL);
		g_free (tmp);
#endif
		history_file = g_build_filename (cache_dir, BASE_TOOL_HISTORY_FILE, NULL);
		if (!g_file_test (cache_dir, G_FILE_TEST_EXISTS)) {
			if (g_mkdir_with_parents (cache_dir, 0700)) {
				g_free (history_file);
				history_file = NULL;
			}
		}
		g_free (cache_dir);
	}
	if (history_file) {
		using_history ();
		read_history (history_file);
		history_init_done = TRUE;
	}
#endif
}

/**
 * base_tool_input_add_to_history
 */
void
base_tool_input_add_to_history (const gchar *txt)
{
#ifdef HAVE_HISTORY
	if (!history_init_done)
		init_history ();
	if (!txt || !(*txt))
		return;

	HIST_ENTRY *current;

	current = history_get (history_length);
	if (current && current->line && !strcmp (current->line, txt))
		return;
	add_history (txt);
#endif
}

/**
 * base_tool_input_save_history
 */
gboolean
base_tool_input_save_history (const gchar *file, GError **error)
{
#ifdef HAVE_HISTORY
	int res;
	if (!history_init_done || !history_file)
		return FALSE;
	res = append_history (1, file ? file : history_file);
	if (res == ENOENT)
		res = write_history (file ? file : history_file);
	if (res != 0) {
		g_set_error (error, BASE_TOOL_ERROR, BASE_TOOL_STORED_DATA_ERROR,
			     _("Could not save history file to '%s': %s"), 
			     file ? file : history_file, strerror (errno));
		return FALSE;
	}
	/*if (res == 0)
	  history_truncate_file (history_file, 500);*/
#endif
	return TRUE;
}

#define MAX_HIST_FETCHED 200
/**
 * base_tool_input_get_history:
 *
 * Get the (at most 200) recent history commands
 *
 * Returns: (transfer full): a new string of the whole history
 */
gchar *
base_tool_input_get_history (void)
{
	GString *string;
	string = g_string_new ("");
#ifdef HAVE_HISTORY
	HIST_ENTRY **hist_array = history_list ();
	if (hist_array) {
		HIST_ENTRY *current;
		gint index;
		for (index = 0, current = *hist_array;
		     current && (index < MAX_HIST_FETCHED);
		     index++, current = hist_array [index]) {
			g_string_append (string, current->line);
			g_string_append_c (string, '\n');
		}
	}
#endif
	return g_string_free (string, FALSE);
}
