/* GDA Default Provider
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-default.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_default_command_new (GdaServerCommand *cmd)
{
	return TRUE;
}

GdaServerRecordset *
gda_default_command_execute (GdaServerCommand *cmd,
							 GdaServerError *error,
							 const GDA_CmdParameterSeq *params,
							 gulong *affected,
							 gulong options)
{
	gchar *cmd_string = NULL;
	GdaServerConnection *cnc;
	GdaServerRecordset *recset = NULL;
	DEFAULT_Connection *default_cnc;
	DEFAULT_Recordset *default_recset;

	g_return_val_if_fail(cmd != NULL, NULL);

	cnc = gda_server_command_get_connection(cmd);
	if (cnc) {
		default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
		if (default_cnc) {
			gchar *errmsg = NULL;

			/* create recordset to be returned */
			recset = gda_server_recordset_new(cnc);
			default_recset = (DEFAULT_Recordset *) gda_server_recordset_get_user_data(recset);
			if (!default_recset) {
				gda_server_recordset_free(recset);
				return NULL;
			}

			switch (gda_server_command_get_type(cmd)) {
			case GDA_COMMAND_TYPE_TEXT :
				cmd_string = g_strdup(gda_server_command_get_text(cmd));
				break;
			case GDA_COMMAND_TYPE_TABLE :
				cmd_string = g_strdup_printf("SELECT * FROM %s",
											 gda_server_command_get_text(cmd));
				break;
			default :
				cmd_string = NULL;
			}

			/* execute the command */
			if (sqlite_get_table(default_cnc->sqlite,
								 cmd_string,
								 &default_recset->data,
								 &default_recset->number_of_rows,
								 &default_recset->number_of_cols,
								 &errmsg) == SQLITE_OK) {
				default_recset->position = -1;
				gda_server_recordset_set_at_begin(recset, TRUE);
				gda_server_recordset_set_at_end(recset, FALSE);
				if (affected)
					*affected = default_recset->number_of_rows;

				/* add all information about the fields */
				if (default_recset->data) {
					gint n;

					for (n = 0; n < default_recset->number_of_cols; n++) {
						GdaServerField *field = gda_server_field_new();
						gda_server_field_set_name(field, default_recset->data[n]);
						gda_server_field_set_scale(field, 0);
						gda_server_field_set_actual_length(field, strlen(default_recset->data[n]));
						gda_server_field_set_defined_length(field, strlen(default_recset->data[n]));
						gda_server_recordset_add_field(recset, field);
					}
				}
			}
			else {
				gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
				gda_server_error_set_description(error, errmsg);
				gda_server_recordset_free(recset);
				recset = NULL;
			}

			/* free memory */
			if (cmd_string) g_free((gpointer) cmd_string);
			if (errmsg) free(errmsg);
		}
	}

	return recset;
}

void
gda_default_command_free (GdaServerCommand *cmd)
{
}
