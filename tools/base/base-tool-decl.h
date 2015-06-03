/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __BASE_TOOL_DECL__
#define __BASE_TOOL_DECL__

typedef enum {
        BASE_TOOL_OUTPUT_FORMAT_DEFAULT = 1 << 0,
        BASE_TOOL_OUTPUT_FORMAT_HTML    = 1 << 1,
        BASE_TOOL_OUTPUT_FORMAT_XML     = 1 << 2,
        BASE_TOOL_OUTPUT_FORMAT_CSV     = 1 << 3,

        BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM = 1 << 8
} ToolOutputFormat;

typedef struct _ToolCommand ToolCommand;
typedef struct _ToolCommandResult ToolCommandResult;
typedef struct _ToolCommandGroup ToolCommandGroup;

#define TO_IMPLEMENT g_print ("Implementation missing: %s() in %s line %d\n", __FUNCTION__, __FILE__,__LINE__)

#endif
