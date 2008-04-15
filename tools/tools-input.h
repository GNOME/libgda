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

#ifndef __GDA_TOOLS_INPUT__
#define __GDA_TOOLS_INPUT__

#include <stdio.h>
#include <glib.h>

typedef char **(*CompletionFunc) (const char *, int, int);

gchar   *input_from_console (const gchar *prompt);
gchar   *input_from_stream  (FILE *stream);

void     init_input ();
void     input_get_size (gint *width, gint *height);

void     init_history ();
void     add_to_history (const gchar *txt);
gboolean save_history (const gchar *file, GError **error);
void     set_completion_func (CompletionFunc func);

#endif
