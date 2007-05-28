/* 
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GDA_INTERNAL_COMMAND__
#define __GDA_INTERNAL_COMMAND__

#include <stdio.h>
#include <glib.h>
#include <libgda/libgda.h>

/*
 * Command exec result
 */
typedef enum {
	GDA_INTERNAL_COMMAND_RESULT_EMPTY,
	GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL,
	GDA_INTERNAL_COMMAND_RESULT_PLIST,
	GDA_INTERNAL_COMMAND_RESULT_TXT,
	GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT,
} GdaInternalCommandResultType;

typedef struct {
	GdaInternalCommandResultType type;
	union {
		GdaDataModel     *model;
		GdaParameterList *plist;
		GString          *txt;
	} u;
} GdaInternalCommandResult;

/*
 * Command definition
 */
typedef GdaInternalCommandResult *(*GdaInternalCommandFunc) (GdaConnection *, GdaDict *,
							     const gchar **,
							     GError **, gpointer);
typedef gchar                   **(*GdaInternalCommandArgsFunc) (const gchar *);
typedef struct {
	gchar    *name;
	gboolean  optional;
} GdaInternalCommandArgument;

typedef struct {
	gchar                     *group;
	gchar                     *name; /* without the '\' */
	gchar                     *description;
	GSList                    *args; /* list of GdaInternalCommandArgument structures */
	GdaInternalCommandFunc     command_func;
	gpointer                   user_data;
	GdaInternalCommandArgsFunc arguments_delimiter_func;
} GdaInternalCommand;

typedef struct {
	GSList    *commands;

	/* internal usage */
	GSList    *name_ordered;
	GSList    *group_ordered;
} GdaInternalCommandsList;

/* Commands execution */
GdaInternalCommandResult *gda_internal_command_execute (GdaInternalCommandsList *commands_list,
							GdaConnection *cnc, GdaDict *dict, 
							const gchar *command_str, GError **error);
void                      gda_internal_command_exec_result_free (GdaInternalCommandResult *res);


/* Available commands */
GdaInternalCommandResult *gda_internal_command_help (GdaConnection *cnc, GdaDict *dict, 
						     const gchar **args,
						     GError **error,GdaInternalCommandsList *clist);
GdaInternalCommandResult *gda_internal_command_history (GdaConnection *cnc, GdaDict *dict, 
							const gchar **args,
							GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_dict_sync (GdaConnection *cnc, GdaDict *dict,
							  const gchar **args,
							  GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_dict_save (GdaConnection *cnc, GdaDict *dict, 
							  const gchar **args,
							  GError **error, gpointer data);
GdaInternalCommandResult *gda_internal_command_list_tables_views (GdaConnection *cnc, GdaDict *dict, 
								  const gchar **args,
								  GError **error, gpointer data);

#endif
