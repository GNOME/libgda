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

#ifndef __BASE_TOOL_OUTPUT__
#define __BASE_TOOL_OUTPUT__

#include <stdio.h>
#include <glib.h>

#include "base-tool-decl.h"
#include "base-tool-command.h"

gchar *base_tool_output_result_to_string (ToolCommandResult *res, ToolOutputFormat format,
					  FILE *stream, GdaSet *options);
gchar *base_tool_output_data_model_to_string (GdaDataModel *model, ToolOutputFormat format, FILE *stream, GdaSet *options);
void   base_tool_output_output_string (FILE *output_stream, const gchar *str);

/*
 * color output handling
 */
typedef enum {
	BASE_TOOL_COLOR_NORMAL,
	BASE_TOOL_COLOR_RESET,
	BASE_TOOL_COLOR_BOLD,
	BASE_TOOL_COLOR_RED
} ToolOutputColor;

void         base_tool_output_color_print (ToolOutputColor color, gboolean color_term, const char *fmt, ...);
gchar       *base_tool_output_color_string (ToolOutputColor color, gboolean color_term, const char *fmt, ...);
void         base_tool_output_color_append_string (ToolOutputColor color, gboolean color_term, GString *string, const char *fmt, ...);
const gchar *base_tool_output_color_s (ToolOutputColor color, gboolean color_term);

#endif
