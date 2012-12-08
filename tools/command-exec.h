/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_INTERNAL_COMMAND__
#define __GDA_INTERNAL_COMMAND__

#include <stdio.h>
#include <glib.h>
#include <libgda/libgda.h>
#include "gda-sql.h"
#include "cmdtool/tool.h"

/*
 * Command definition
 */
typedef ToolCommandResult *(*GdaInternalCommandFunc) (SqlConsole *, GdaConnection *cnc,
						      const gchar **, ToolOutputFormat, GError **, gpointer);

/* Available commands */
ToolCommandResult *gda_internal_command_history (ToolCommand *command, guint argc, const gchar **argv,
						 SqlConsole *console, GError **error);
ToolCommandResult *gda_internal_command_dict_sync (ToolCommand *command, guint argc, const gchar **argv,
						   SqlConsole *console, GError **error);
ToolCommandResult *gda_internal_command_list_tables (ToolCommand *command, guint argc, const gchar **argv,
						     SqlConsole *console, GError **error);
ToolCommandResult *gda_internal_command_list_views (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);
ToolCommandResult *gda_internal_command_list_schemas (ToolCommand *command, guint argc, const gchar **argv,
						      SqlConsole *console, GError **error);
ToolCommandResult *gda_internal_command_detail (ToolCommand *command, guint argc, const gchar **argv,
						SqlConsole *console, GError **error);

/* Misc */
GdaMetaStruct            *gda_internal_command_build_meta_struct (GdaConnection *cnc, const gchar **args, GError **error);

#endif
