/* GNOME DB MySQL Provider
 * Copyright (C) 1998 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2001 Vivien Malerba
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
gda_mysql_init_recset_fields (GdaServerRecordset *recset, MYSQL_Recordset *mysql_recset)
{
	gint max_fieldidx;
	gint fieldidx;
	
	g_return_if_fail(recset != NULL);
	g_return_if_fail(mysql_recset != NULL);
	g_return_if_fail((mysql_recset->mysql_res != NULL) || 
			 (mysql_recset->btin_res != NULL));
	
	if (mysql_recset->mysql_res) {
	        max_fieldidx = mysql_num_fields(mysql_recset->mysql_res);
		for (fieldidx = 0; fieldidx < max_fieldidx; fieldidx++) {
		        MYSQL_FIELD*     mysql_field;
			GdaServerField* field;
		
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
	else { /* we use the builtin result */
	        max_fieldidx = GdaBuiltin_Result_get_nbfields(mysql_recset->btin_res);
		for (fieldidx = 0; fieldidx < max_fieldidx; fieldidx++) {
			GdaServerField* field;
		
			field = gda_server_field_new();
			gda_server_field_set_name(field, 
						  GdaBuiltin_Result_get_fname
						  (mysql_recset->btin_res, fieldidx));
			gda_server_field_set_sql_type(field, 
						      GdaBuiltin_Result_get_ftype
						      (mysql_recset->btin_res, fieldidx));
			gda_server_recordset_add_field(recset, field);
		}
	}
}

/*
 * Public functions
 */
gboolean
gda_mysql_command_new (GdaServerCommand *cmd)
{
	return TRUE;
}

GdaServerRecordset *
gda_mysql_command_execute (GdaServerCommand *cmd,
                           GdaServerError *error,
                           const GDA_CmdParameterSeq *params,
                           gulong *affected,
                           gulong options)
{
	gint                  rc;
	gchar*                cmd_string;
	GdaServerConnection* cnc;
	GdaServerRecordset*  recset = NULL;
	MYSQL_Connection*     mysql_cnc;
	
	cnc = gda_server_command_get_connection(cmd);
	if (cnc) {
		mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
		if (mysql_cnc) {
			gchar* tmp;

			switch (gda_server_command_get_type(cmd)) {
				case GDA_COMMAND_TYPE_TABLE :
					cmd_string = g_strdup_printf("SELECT * FROM %s", gda_server_command_get_text(cmd));
					break;
				case GDA_COMMAND_TYPE_XML :
					tmp = gda_mysql_connection_xml2sql(cnc, gda_server_command_get_text(cmd));
					if (tmp)
						cmd_string = g_strdup(tmp);
					else
						cmd_string = NULL;
					break;
				case GDA_COMMAND_TYPE_TEXT :
					cmd_string = g_strdup(gda_server_command_get_text(cmd));
					break;
				default :
					cmd_string = NULL;
					break;
			}
			
			/* execute the command */
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
				
				g_free((gpointer) cmd_string);
			}
		}
	}
	
	return recset;
}

GdaServerRecordset *
gda_mysql_command_build_recset_with_builtin (GdaServerConnection *cnc,
					     GdaBuiltin_Result *res,
					     gulong *affected)
{
  	GdaServerRecordset *recset;
	MYSQL_Recordset  *myset;

	recset = gda_server_recordset_new(cnc);
	myset = (MYSQL_Recordset *) 
		gda_server_recordset_get_user_data(recset);
	myset->mysql_res = NULL;
	myset->btin_res = res;
	gda_server_recordset_set_at_begin(recset, TRUE);
	gda_server_recordset_set_at_end(recset, FALSE);
	myset->pos = 0;
	*affected = GdaBuiltin_Result_get_nbtuples(res);
	gda_mysql_init_recset_fields(recset, myset);
	return recset;
}

void
gda_mysql_command_free (GdaServerCommand *cmd)
{
}
