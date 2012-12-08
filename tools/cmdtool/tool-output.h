/*
 * Copyright (C) 2000 - 2001 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2001 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2007 Baris Cicek <bcicek@src.gnome.org>
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __TOOL_OUTPUT__
#define __TOOL_OUTPUT__

#include <stdio.h>
#include <glib.h>

#include "tool-decl.h"
#include "tool-command.h"

gchar *tool_output_result_to_string (ToolCommandResult *res, ToolOutputFormat format,
				     FILE *stream, GdaSet *options);
void   tool_output_output_string (FILE *output_stream, const gchar *str);

/*
 * color output handling
 */
typedef enum {
	TOOL_COLOR_NORMAL,
	TOOL_COLOR_RESET,
	TOOL_COLOR_BOLD,
	TOOL_COLOR_RED
} ToolColor;

void         tool_output_color_print (ToolColor color, ToolOutputFormat format, const char *fmt, ...);
gchar       *tool_output_color_string (ToolColor color, ToolOutputFormat format, const char *fmt, ...);
void         tool_output_color_append_string (ToolColor color, ToolOutputFormat format, GString *string, const char *fmt, ...);
const gchar *tool_output_color_s (ToolColor color, ToolOutputFormat format);

#endif
