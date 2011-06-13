/*
 * Copyright (C) 2000 - 2001 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2001 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2007 Baris Cicek <bcicek@src.gnome.org>
 * Copyright (C) 2007 - 2008 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_TOOLS_INPUT__
#define __GDA_TOOLS_INPUT__

#include <stdio.h>
#include <glib.h>

typedef char       **(*CompletionFunc) (const char *, int, int);
typedef gboolean     (*TreatLineFunc) (const gchar *, gpointer);
typedef const  char *(*ComputePromptFunc) (void);

gchar   *input_from_console (const gchar *prompt);
gchar   *input_from_stream  (FILE *stream);

void     init_input (TreatLineFunc treat_line_func, ComputePromptFunc prompt_func, gpointer data);
void     input_get_size (gint *width, gint *height);
void     end_input (void);

void     init_history ();
void     add_to_history (const gchar *txt);
gboolean save_history (const gchar *file, GError **error);
void     set_completion_func (CompletionFunc func);

#endif
