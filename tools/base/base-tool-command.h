/*
 * Copyright (C) 2012 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __BASE_TOOL_COMMAND__
#define __BASE_TOOL_COMMAND__

#include <stdio.h>
#include <glib.h>
#include <libgda.h>
#include "base-tool-decl.h"
#include "base-tool-output.h"
#include "base-tool-input.h"

/**
 * ToolCommandResultType:
 *
 * Defines the type of result of a command result
 */
typedef enum {
        BASE_TOOL_COMMAND_RESULT_EMPTY,
        BASE_TOOL_COMMAND_RESULT_DATA_MODEL,
        BASE_TOOL_COMMAND_RESULT_SET,
        BASE_TOOL_COMMAND_RESULT_TREE,
        BASE_TOOL_COMMAND_RESULT_TXT,
        BASE_TOOL_COMMAND_RESULT_TXT_STDOUT,
        BASE_TOOL_COMMAND_RESULT_MULTIPLE,
        BASE_TOOL_COMMAND_RESULT_HELP,
        BASE_TOOL_COMMAND_RESULT_EXIT
} ToolCommandResultType;

/**
 * ToolCommandResult:
 *
 * Returned value after a command has been executed
 */
struct _ToolCommandResult {
        ToolCommandResultType  type;
	GdaConnection         *cnc;
        gboolean               was_in_transaction_before_exec;
        union {
                GdaDataModel     *model;
                GdaSet           *set;
		GdaTree          *tree;
                GString          *txt;
                GSList           *multiple_results; /* for BASE_TOOL_COMMAND_RESULT_MULTIPLE */
		xmlNodePtr        xml_node; /* for BASE_TOOL_COMMAND_RESULT_HELP */
        } u;
};

ToolCommandResult *base_tool_command_result_new (GdaConnection *cnc, ToolCommandResultType type);
void               base_tool_command_result_free (ToolCommandResult *res);

/**
 * ToolCommandFunc:
 * @command: the #ToolCommand to execute
 * @argc: @argv's size
 * @argv: the arguments (not including the command's name itself)
 * @user_data: (nullable): a pointer to some use data, corresponds to the @user_data argument of base_tool_command_group_execute()
 * @error: (nullable): a place to store errors
 *
 * Function defining a command's execution code
 */
typedef ToolCommandResult *(*ToolCommandFunc) (ToolCommand *command, guint argc, const gchar **argv,
					       gpointer user_data, GError **error);

typedef gchar** (*ToolCommandToolInputCompletionFunc) (const gchar *text, gpointer user_data);

/**
 * ToolCommand: 
 *
 * Definition of a command. This can be subclassed to include more specific tools data.
 */
struct _ToolCommand {
        gchar                     *group;
        gchar                     *group_id; /* to be found in the help file */
        gchar                     *name; /* without the '\' or '.' */
        gchar                     *name_args; /* without the '\' or '.', but with the name included */
        gchar                     *description;
        ToolCommandFunc            command_func;
	ToolCommandToolInputCompletionFunc  completion_func;
	gpointer                   completion_func_data;
};

/**
 * ToolCommandGroup:
 *
 * A coherent ser of commands
 */
ToolCommandGroup  *base_tool_command_group_new     (void);
void               base_tool_command_group_free    (ToolCommandGroup *group);
void               base_tool_command_group_add     (ToolCommandGroup *group, ToolCommand *cmd);
void               base_tool_command_group_remove  (ToolCommandGroup *group, const gchar *name);
ToolCommand       *base_tool_command_group_find    (ToolCommandGroup *group, const gchar *name, GError **error);
GSList            *base_tool_command_get_all_commands (ToolCommandGroup *group);
GSList            *base_tool_command_get_commands  (ToolCommandGroup *group, const gchar *prefix);

gboolean           base_tool_command_is_internal   (const gchar *cmde);
ToolCommandResult *base_tool_command_group_execute (ToolCommandGroup *group, const gchar *cmde,
						    gpointer user_data, GError **error);

#endif
