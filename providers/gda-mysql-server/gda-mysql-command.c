/* GNOME DB MySQL Provider
 * Copyright (C) 1998 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
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

#include "gda-mysql.h"

/*
 * Private functions
 */
void
gda_mysql_init_recset_fields (Gda_ServerRecordset *recset, MYSQL_Recordset *mysql_recset) {
	gint max_fieldidx;
	gint fieldidx;
	
	g_return_if_fail(recset != NULL);
	g_return_if_fail(mysql_recset != NULL);
	g_return_if_fail(mysql_recset->mysql_res != NULL);
	
	max_fieldidx = mysql_num_fields(mysql_recset->mysql_res);
	for (fieldidx = 0; fieldidx < max_fieldidx; fieldidx++) {
		MYSQL_FIELD*     mysql_field;
		Gda_ServerField* field;
		
		mysql_field = mysql_fetch_field(mysql_recset->mysql_res);
		field = gda_server_field_new();
		gda_server_field_set_name(field, mysql_field->name);
		gda_server_field_set_sql_type(field, mysql_field->type);
		gda_server_field_set_scale(field, mysql_field->decimals);
		gda_server_field_set_actual_length(field, mysql_field->length);
		gda_server_field_set_defined_length(field, mysql_field->max_length);
		gda_server_field_set_user_data(field, (gpointer) mysql_field);
		
		gda_server_recordset_add_field(recset, field);
	}
}

/*
 * Public functions
 */
gboolean
gda_mysql_command_new (Gda_ServerCommand *cmd) {
	return TRUE;
}

Gda_ServerRecordset *
gda_mysql_command_execute (Gda_ServerCommand *cmd,
                           Gda_ServerError *error,
                           const GDA_CmdParameterSeq *params,
                           gulong *affected,
                           gulong options) {
	gint                  rc;
	gchar*                cmd_string;
	Gda_ServerConnection* cnc;
	Gda_ServerRecordset*  recset = NULL;
	MYSQL_Connection*     mysql_cnc;
	
	cnc = gda_server_command_get_connection(cmd);
	if (cnc) {
		mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
		if (mysql_cnc) {
			switch (gda_server_command_get_type(cmd)) {
				case GDA_COMMAND_TYPE_TABLE : {
					gchar* tmp = g_strdup_printf("SELECT * FROM %s", gda_server_command_get_text(cmd));
					gda_server_command_set_text(cmd, tmp);
					g_free((gpointer) tmp);
				}
				case GDA_COMMAND_TYPE_TEXT :
					cmd_string = gda_server_command_get_text(cmd);
					//cmd_string = splice_parameters(gda_server_command_get_text(cmd), params, cnc);
					if (cmd_string) {
						/* execute command */
						gda_log_message(_("executing command '%s'"), cmd_string);
						rc = mysql_real_query(mysql_cnc->mysql, cmd_string, strlen(cmd_string));
						gda_log_message(_("info '%s'"), mysql_info(mysql_cnc->mysql));
						if (rc == 0) {
							MYSQL_Recordset* mysql_recset;
							
							/* create the recordset to be returned */
							recset = gda_server_recordset_new(cnc);
							mysql_recset = (MYSQL_Recordset *) gda_server_recordset_get_user_data(recset);
							if (mysql_recset) {
								mysql_recset->mysql_res = mysql_store_result(mysql_cnc->mysql);
								gda_mysql_init_recset_fields(recset, mysql_recset);
							}
						}
						else gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);
					}
					break;
			}
		}
	}
	
	return recset;
}

void
gda_mysql_command_free (Gda_ServerCommand *cmd) {
}
