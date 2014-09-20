/*
 * Copyright (C) 2000 - 2001 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2001 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2007 Baris Cicek <bcicek@src.gnome.org>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __BASE_TOOL_INPUT__
#define __BASE_TOOL_INPUT__

#include <stdio.h>
#include <glib.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif
#include "base-tool-decl.h"

typedef char       **(*ToolInputCompletionFunc) (const char *match, const char *line, int start, int end, gpointer func_data);
typedef gboolean     (*ToolInputTreatLineFunc) (const gchar *, gpointer);
typedef const  char *(*ToolInputComputePromptFunc) (void);

gchar   *base_tool_input_from_stream  (FILE *stream);

void     base_tool_input_init (GMainContext *context, ToolInputTreatLineFunc treat_line_func, ToolInputComputePromptFunc prompt_func, gpointer data);
void     base_tool_input_get_size (gint *width, gint *height);
void     base_tool_input_end (void);

void     base_tool_input_add_to_history (const gchar *txt);
gboolean base_tool_input_save_history (const gchar *file, GError **error);
void     base_tool_input_set_completion_func (ToolCommandGroup *group, ToolInputCompletionFunc func, gpointer func_data, gchar *start_chars_to_ignore);
gchar   *base_tool_input_get_history (void);

#endif
