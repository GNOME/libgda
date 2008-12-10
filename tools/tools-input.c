/* 
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
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

#include "tools-input.h"
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#ifndef G_OS_WIN32
#include <sys/ioctl.h>
#endif
#include <libgda/gda-debug-macros.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif
#ifdef HAVE_HISTORY
#include <readline/history.h>
#endif

#define HISTORY_ENV_NAME "GDA_SQL_HISTFILE"
#define HISTORY_FILE ".gdasql_history"
#ifdef HAVE_HISTORY
static gboolean history_init_done = FALSE;
const gchar *history_file = NULL;
#endif

/**
 * input_from_console
 *
 * returns: a new string read from the console, using readline or stdin.
 */
gchar *
input_from_console (const gchar *prompt)
{
#ifdef HAVE_READLINE
	char *read;

	if (isatty (fileno (stdin))) {
		read = readline (prompt);
		if (read) {
			gchar *str = g_strdup (read);
			free (read);
			add_to_history (str);
			return str;
		}
		else
			return NULL;
	}
	else
		return input_from_stream (stdin);
#else
	if (isatty (fileno (stdout)))
		g_print ("%s", prompt);
	return input_from_stream (stdin);
#endif
}

/**
 * input_from_stream
 *
 * returns: a new string read from @stream
 */
gchar *
input_from_stream  (FILE *stream)
{
	#define LINE_SIZE 1024
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

static TreatLineFunc line_cb_func = NULL;
static gpointer      line_cb_func_data = NULL;
static ComputePromptFunc line_prompt_func = NULL;
static GIOChannel *ioc = NULL;

static gboolean
chars_for_readline_cb (GIOChannel *ioc, GIOCondition condition, gpointer data)
{
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
		if (line_cb_func (line, line_cb_func_data) == TRUE) {
			/* print prompt for next line */
			g_print ("%s", line_prompt_func ());
		}
		g_free (line);
		break;
	case G_IO_STATUS_ERROR:
		g_warning ("Error reading from STDIN: %s\n",
			   error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
		break;
	case G_IO_STATUS_EOF:
		/* send the Quit command */
		line_cb_func (".q", line_cb_func_data);
		return FALSE;
		break;
	default:
		break;
	}
#endif
        return TRUE;
}

#ifdef HAVE_READLINE
static void
line_read_handler (char *line)
{
	line_cb_func (line, line_cb_func_data); /* we don't care about the return status */
        free (line);
	rl_set_prompt (line_prompt_func ());
}
#endif

/**
 * init_input
 *
 * Initializes input
 */
void
init_input (TreatLineFunc treat_line_func, ComputePromptFunc prompt_func, gpointer data)
{
	line_cb_func = treat_line_func;
	line_cb_func_data = data;
	line_prompt_func = prompt_func;

#ifdef HAVE_READLINE
	rl_set_signals ();
	rl_readline_name = "gda-sql";
	rl_callback_handler_install (prompt_func () ,  line_read_handler);
#else
	g_print ("%s", line_prompt_func ());
#endif
	if (!ioc) {
		ioc = g_io_channel_unix_new (STDIN_FILENO);
		g_io_add_watch (ioc, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL, (GIOFunc) chars_for_readline_cb, NULL);
	}
}

/**
 * end_input
 *
 * Releases any data related to the input and allocated during init_input()
 */
void
end_input (void)
{
#ifdef HAVE_READLINE
	rl_callback_handler_remove ();
#endif
	if (ioc) {
		g_io_channel_shutdown (ioc, TRUE, NULL);
		g_io_channel_unref (ioc);
	}
}

/**
 * set_completion_func
 *
 *
 */
void
set_completion_func (CompletionFunc func)
{
#ifdef HAVE_READLINE	
	rl_attempted_completion_function = func;
	rl_basic_word_break_characters = " \t\n\\'`@$><=;|&{(";
#endif	
}

/*
 * input_get_size
 *
 * Get the size of the input term, if possible, otherwise returns -1
 */
void
input_get_size (gint *width, gint *height)
{
	int tty = fileno (stdin);
	int cols = -1, rows = -1;

#ifdef TIOCGWINSZ
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

/**
 * init_history
 *
 * Loads the contents of the history file, if supported
 */
void
init_history ()
{
#ifdef HAVE_HISTORY
	rl_set_signals ();
	
	if (history_init_done)
		return;
	if (getenv (HISTORY_ENV_NAME)) {
		if (!strcmp (getenv (HISTORY_ENV_NAME), "NO_HISTORY")) {
			history_init_done = TRUE;
			return;
		}
		history_file = g_strdup (getenv (HISTORY_ENV_NAME));
	}
	else
		history_file = g_build_filename (g_get_home_dir (), HISTORY_FILE, NULL);
	using_history ();
	read_history (history_file);
	history_init_done = TRUE;
#endif
}

/**
 * add_to_history
 */
void
add_to_history (const gchar *txt)
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
 * save_history
 */
gboolean
save_history (const gchar *file, GError **error)
{
#ifdef HAVE_HISTORY
	int res;
	if (!history_init_done || !history_file)
		return FALSE;
	res = append_history (1, file ? file : history_file);
	if (res == ENOENT)
		res = write_history (file ? file : history_file);
	if (res != 0) {
		g_set_error (error, 0, 0,
			     _("Could not save history file to '%s': %s"), 
			     file ? file : history_file, strerror (errno));
		return FALSE;
	}
	/*if (res == 0)
	  history_truncate_file (history_file, 500);*/
#endif
	return TRUE;
}
