/*
 * Copyright (C) 2007 - 2010 Vivien Malerba <malerba@gnome-db.org>
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

/*
 * Command exec result
 */
typedef enum {
	GDA_INTERNAL_COMMAND_RESULT_EMPTY,
	GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL,
	GDA_INTERNAL_COMMAND_RESULT_SET,
	GDA_INTERNAL_COMMAND_RESULT_TXT,
	GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT,
	GDA_INTERNAL_COMMAND_RESULT_MULTIPLE,
	GDA_INTERNAL_COMMAND_RESULT_EXIT
} GdaInternalCommandResultType;

typedef struct {
	GdaInternalCommandResultType type;
	GdaConnection               *cnc;
	gboolean                     was_in_transaction_before_exec;
	union {
		GdaDataModel     *model;
		GdaSet           *set;
		GString          *txt;
		GSList           *multiple_results; /* for GDA_INTERNAL_COMMAND_RESULT_MULTIPLE */
	} u;
} GdaInternalCommandResult;

/*
 * Command definition
 */
typedef GdaInternalCommandResult *(*GdaInternalCommandFunc) (SqlConsole *, GdaConnection *cnc,
							     const gchar **, OutputFormat, GError **, gpointer);
typedef gchar                   **(*GdaInternalCommandArgsFunc) (const gchar *);
typedef struct {
	gchar    *name;
	gboolean  optional;
} GdaInternalCommandArgument;

typedef struct {
	gchar                     *group;
	gchar                     *group_id; /* to be found in the help file */
	gchar                     *name; /* without the '\' or '.' */
	gchar                     *description;
	GSList                    *args; /* list of GdaInternalCommandArgument structures */
	GdaInternalCommandFunc     command_func;
	gpointer                   user_data;
	GdaInternalCommandArgsFunc arguments_delimiter_func;
	gboolean                   unquote_args;
	gboolean                   limit_to_main;
} GdaInternalCommand;

typedef struct {
	GSList    *commands;

	/* internal usage */
	GSList    *name_ordered;
	GSList    *group_ordered;
} GdaInternalCommandsList;

gchar                    *gda_internal_command_arg_remove_quotes (gchar *str);

/* Commands execution */
gchar                   **default_gda_internal_commandargs_func (const gchar *string);
void                      gda_internal_command_exec_result_free (GdaInternalCommandResult *res);

/* Available commands */
GdaInternalCommandResult *gda_internal_command_help (SqlConsole *console, GdaConnection *cnc, const gchar **args,
						     OutputFormat format, GError **error, GdaInternalCommandsList *clist);
GdaInternalCommandResult *gda_internal_command_history (SqlConsole *console, GdaConnection *cnc, const gchar **args,
							OutputFormat format, GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_dict_sync (SqlConsole *console, GdaConnection *cnc, const gchar **args,
							  OutputFormat format, GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_list_tables (SqlConsole *console, GdaConnection *cnc, const gchar **args,
							    OutputFormat format, GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_list_views (SqlConsole *console, GdaConnection *cnc, const gchar **args,
							   OutputFormat format, GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_list_schemas (SqlConsole *console, GdaConnection *cnc, const gchar **args,
							     OutputFormat format, GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_detail (SqlConsole *console, GdaConnection *cnc, const gchar **args,
						       OutputFormat format, GError **error, gpointer data);

/* Misc */
GdaMetaStruct            *gda_internal_command_build_meta_struct (GdaConnection *cnc, const gchar **args, GError **error);

#endif
