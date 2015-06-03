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

#ifndef __BASE_TOOL_HELP__
#define __BASE_TOOL_HELP__

#include <stdio.h>
#include <glib.h>
#include "base-tool-command.h"

ToolCommandResult *base_tool_help_get_command_help (ToolCommandGroup *group, const gchar *command_name, GError **error);
gchar             *base_tool_help_to_string (ToolCommandResult *help, gboolean limit_width, gboolean color_term);

#endif
