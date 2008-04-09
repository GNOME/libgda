/* 
 * Copyright (C) 2007 The GNOME Foundation.
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

#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif

#define HISTORY_ENV_NAME "GDA_SQL_HISTFILE"
#define HISTORY_FILE ".gdasql_history"
static gboolean history_init_done = FALSE;
const gchar *history_file = NULL;

/**
 * input_from_console
 *
 * returns: a new string read from the console, using readline or stdin.
 */
gchar *
input_from_console (const gchar *prompt)
{
#ifdef HAVE_READLINE_READLINE_H
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

#ifdef HAVE_READLINE_READLINE_H	
static char *
completion_generator_func (const char *text, int state)
{
	static char **compl = NULL;
	if (state == 0) {
		/* clear any previous completion */
		if (compl) {
			/* don't free the contents of @array, it is freed by readline */
			g_free (compl);
			compl = NULL;
		}

		/* compute list of possible completions. It's very simple at the moment */
		if (!(*text)) {
			/* no completion possible */
		}
		else {
			gchar *copy;

			copy = g_strdup (text);
			g_strchomp (copy);
			if (*copy) {
				const char *start;
				for (start = copy + (strlen (copy) - 1); start >= copy; start--)
					if (g_ascii_isspace (*start)) {
						start ++;
						break;
					}
				compl = g_new0 (char *, 2);
				compl[0] = malloc (sizeof (char) * (strlen (start) + 1));
				strcpy (compl[0], start);
			}
			g_free (copy);
		}

		if (compl)
			return compl[0];
		else
			return NULL;
	}
	else 
		return compl[state];
}
#endif

/**
 * init_input
 *
 * Initializes input
 */
void
init_input ()
{
#ifdef HAVE_READLINE_READLINE_H	
	rl_set_signals ();
	rl_readline_name = "gda-sql";
	rl_completion_entry_function = completion_generator_func;
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
#ifdef HAVE_READLINE_HISTORY_H
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
#ifdef HAVE_READLINE_HISTORY_H
	if (!history_init_done)
		init_history ();
	if (!txt || !(*txt))
		return;

	HIST_ENTRY *current;

	current = current_history ();
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
#ifdef HAVE_READLINE_HISTORY_H
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
