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

	read = readline (prompt);
	if (read) {
		gchar *str = g_strdup (read);
		free (read);
		add_to_history (str);
		return str;
	}
	else
		return NULL;
#else
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

/**
 * init_history
 *
 * Loads the contents of the history file, if supported
 */
void
init_history ()
{
#ifdef HAVE_READLINE_HISTORY_H
	
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
void
save_history ()
{
#ifdef HAVE_READLINE_HISTORY_H
	int res;
	if (!history_init_done || !history_file)
		return;
	res = append_history (1, history_file);
	if (res == ENOENT)
		res = write_history (history_file);
	/*if (res == 0)
	  history_truncate_file (history_file, 500);*/
#endif
}
