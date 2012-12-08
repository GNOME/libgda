/*
 * Copyright (C) 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __TOOL_COMMAND__
#define __TOOL_COMMAND__

#include <stdio.h>
#include <glib.h>
#include <libgda.h>
#include "tool-decl.h"
#include "tool-output.h"
#include "tool-input.h"

/**
 * ToolCommandResultType:
 *
 * Defines the type of result of a command result
 */
typedef enum {
        TOOL_COMMAND_RESULT_EMPTY,
        TOOL_COMMAND_RESULT_DATA_MODEL,
        TOOL_COMMAND_RESULT_SET,
        TOOL_COMMAND_RESULT_TREE,
        TOOL_COMMAND_RESULT_TXT,
        TOOL_COMMAND_RESULT_TXT_STDOUT,
        TOOL_COMMAND_RESULT_MULTIPLE,
        TOOL_COMMAND_RESULT_EXIT
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
                GSList           *multiple_results; /* for TOOL_COMMAND_RESULT_MULTIPLE */
        } u;
};

ToolCommandResult *tool_command_result_new (GdaConnection *cnc, ToolCommandResultType type);
void               tool_command_result_free (ToolCommandResult *res);

/**
 * ToolCommandFunc:
 * @command: the #ToolCommand to execute
 * @argc: @argv's size
 * @argv: the arguments (not including the command's name itself)
 * @user_data: (allow-none): a pointer to some use data, corresponds to the @user_data argument of tool_command_group_execute()
 * @error: (allow-none): a place to store errors
 *
 * Function defining a command's execution code
 */
typedef ToolCommandResult *(*ToolCommandFunc) (ToolCommand *command, guint argc, const gchar **argv,
					       gpointer user_data, GError **error);

typedef gchar** (*ToolCommandCompletionFunc) (const gchar *text);

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
	ToolCommandCompletionFunc  completion_func;
};

/* error reporting */
extern GQuark tool_command_error_quark (void);
#define TOOL_COMMAND_ERROR tool_command_error_quark ()

typedef enum {
        TOOL_COMMAND_COMMAND_NOT_FOUND_ERROR,
	TOOL_COMMAND_SYNTAX_ERROR
} CoolCommandError;


/**
 * ToolCommandGroup:
 *
 * A coherent ser of commands
 */
ToolCommandGroup  *tool_command_group_new     (void);
void               tool_command_group_free    (ToolCommandGroup *group);
void               tool_command_group_add     (ToolCommandGroup *group, ToolCommand *cmd);
void               tool_command_group_remove  (ToolCommandGroup *group, const gchar *name);
ToolCommand       *tool_command_group_find    (ToolCommandGroup *group, const gchar *name, GError **error);
GSList            *tool_command_get_all_commands (ToolCommandGroup *group);
GSList            *tool_command_get_commands  (ToolCommandGroup *group, const gchar *prefix);

ToolCommandResult *tool_command_group_execute (ToolCommandGroup *group, const gchar *cmde,
					       ToolOutputFormat format,
					       gpointer user_data, GError **error);

#endif
