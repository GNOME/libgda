/* GNOME DB ORACLE Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
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

#include "gda-oracle.h"

#define ORA_NAME_BUFFER_SIZE 30

/*
 * Private functions
 */
static GdaServerRecordset *
init_recordset_fields (GdaServerRecordset *recset,
                       ORACLE_Recordset *ora_recset,
                       ORACLE_Command *ora_cmd,
                       GdaError *error) {
	g_return_val_if_fail(recset != NULL, recset);
	g_return_val_if_fail(ora_recset != NULL, recset);
	g_return_val_if_fail(ora_cmd != NULL, recset);
	g_return_val_if_fail(ora_cmd->hstmt == ora_recset->hstmt, recset);
	
	/* traverse all fields in resultset */
	if (ora_cmd->stmt_type == OCI_STMT_SELECT) {
		gint      counter = 1;
		OCIParam* param;
		sb4       param_status;
		
		param_status = OCIParamGet(ora_recset->hstmt,
		                           OCI_HTYPE_STMT,
		                           ora_recset->ora_cnc->herr,
		                           (dvoid **) &param,
		                           (ub4) counter);
		while (param_status == OCI_SUCCESS) {
			glong            col_name_len;
			gchar            name_buffer[ORA_NAME_BUFFER_SIZE + 1];
			gchar*           pgchar_dummy;
			GdaServerField* field;
			ORACLE_Field*    ora_field;
			
			field = gda_server_field_new();
			/* associate private structure with field */
			ora_field = g_new0(ORACLE_Field, 1);
			gda_server_field_set_user_data(field, (gpointer) ora_field);
			
			/* fill in field information */
			/* attention - names are not terminated by '\0' */
			OCIAttrGet((dvoid *) param,
			           OCI_DTYPE_PARAM,
			           &pgchar_dummy,
			           (ub4) &col_name_len,
			           OCI_ATTR_NAME,
			           ora_recset->ora_cnc->herr);
			memcpy(name_buffer, pgchar_dummy, col_name_len);
			name_buffer[col_name_len] = '\0';
			gda_server_field_set_name(field, name_buffer);
			
			OCIAttrGet((dvoid *) param,
			           OCI_DTYPE_PARAM,
			           &(field->sql_type),
			           0,
			           OCI_ATTR_DATA_TYPE,
			           ora_recset->ora_cnc->herr);
			           
			/* wanna get numeric as string (TO BE FIXED) */
			if (SQLT_NUM == field->sql_type ||
			    SQLT_VNU == field->sql_type) {
				field->sql_type = SQLT_AVC;
			}
			OCIAttrGet((dvoid *) param,
			           OCI_DTYPE_PARAM,
			           &(field->nullable),
			           0,
			           OCI_ATTR_IS_NULL,
			           ora_recset->ora_cnc->herr);
			OCIAttrGet((dvoid *) param,
			           OCI_DTYPE_PARAM,
			           &(field->precision),
			           0,
			           OCI_ATTR_PRECISION,
			           ora_recset->ora_cnc->herr);
			OCIAttrGet((dvoid *) param,
			           OCI_DTYPE_PARAM,
			           &(field->num_scale),
			           0,
			           OCI_ATTR_SCALE,
			           ora_recset->ora_cnc->herr);
			OCIAttrGet((dvoid *) param,
			           OCI_DTYPE_PARAM,
			           &(field->defined_length),
			           0,
			           OCI_ATTR_DATA_SIZE,
			           ora_recset->ora_cnc->herr);
			gda_server_field_set_actual_length(field, 0);
			
			/* bind output columns to 'ora_field->real_value' */
			ora_field->real_value = g_malloc0(field->defined_length);
			ora_field->hdef = NULL;
			ora_field->indicator = 0;
			if (OCIDefineByPos(ora_recset->hstmt,
			                   &ora_field->hdef,
			                   ora_recset->ora_cnc->herr,
			                   counter,
			                   ora_field->real_value,
			                   field->defined_length,
			                   field->sql_type,
			                   &ora_field->indicator,
			                   0,
			                   0,
			                   OCI_DEFAULT) != OCI_SUCCESS) {
				gda_server_error_make(error,
				                      0,
				                      gda_server_recordset_get_connection(recset),
				                      __PRETTY_FUNCTION__);
				return recset;
			}
			
			/* add field to GdaServerRecordset structure */
			gda_server_recordset_add_field(recset, field);
			
			/* retrieve next field */
			counter++;
			param_status = OCIParamGet(ora_recset->hstmt,
			                           OCI_HTYPE_STMT,
			                           ora_recset->ora_cnc->herr,
			                           (dvoid **) &param,
			                           (ub4) counter);
		}
		
		/* if param_status != OCI_NO_DATA, there was an error */
		if (param_status != OCI_NO_DATA) {
			gda_server_error_make(error,
			                      0,
			                      gda_server_recordset_get_connection(recset),
			                      __PRETTY_FUNCTION__);
		}
	}
	return recset;
}

/*
 * Public functions
 */
gboolean
gda_oracle_command_new (GdaServerCommand *cmd) {
	ORACLE_Command*       ora_cmd;
	GdaServerConnection* cnc;
	ORACLE_Connection*    ora_cnc;
	
	g_return_val_if_fail(cmd != NULL, FALSE);
	
	ora_cmd = g_new0(ORACLE_Command, 1);
	gda_server_command_set_user_data(cmd, (gpointer) ora_cmd);
	
	cnc = gda_server_command_get_connection(cmd);
	if (cnc) {
		ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
		ora_cmd->ora_cnc = ora_cnc;
		
		/* initialize Oracle command */
		if (OCI_SUCCESS != OCIHandleAlloc((CONST dvoid *) ora_cmd->ora_cnc->henv,
		                                  (dvoid **) &ora_cmd->hstmt,
		                                  OCI_HTYPE_STMT,
		                                  (size_t) 0,
		                                  (dvoid **) NULL)) {
			GdaError* error = gda_error_new();
			gda_server_error_make(error,
			                      0,
			                      gda_server_command_get_connection(cmd),
			                      __PRETTY_FUNCTION__);
			gda_error_set_description(error, _("Could not allocate statement handle"));
			return FALSE;
		}
	}
	else gda_log_message(_("command object being created without associated connection"));
	return TRUE;
}

GdaServerRecordset *
gda_oracle_command_execute (GdaServerCommand *cmd,
                            GdaError *error,
                            const GDA_CmdParameterSeq *params,
                            gulong *affected,
                            gulong options) {
	GdaServerRecordset* recset = NULL;
	
	/* create recordset to be returned */
	recset = gda_server_recordset_new(gda_server_command_get_connection(cmd));
	if (recset) {
		ORACLE_Command*   ora_cmd;
		ORACLE_Recordset* ora_recset;
		gchar*            cmd_text;
		
		/* get each object's private data */
		ora_cmd = (ORACLE_Command *) gda_server_command_get_user_data(cmd);
		ora_recset = (ORACLE_Recordset *) gda_server_recordset_get_user_data(recset);
		if (ora_cmd && ora_recset) {
			switch (gda_server_command_get_cmd_type(cmd)) {
				case GDA_COMMAND_TYPE_TABLE : {
					gchar* tmp = g_strdup_printf("SELECT * FROM %s", gda_server_command_get_text(cmd));
					gda_server_command_set_text(cmd, tmp);
					g_free((gpointer) tmp);
				}
				case GDA_COMMAND_TYPE_TEXT :
					cmd_text = gda_server_command_get_text(cmd);
					gda_log_message(_("Executing command '%s'"), cmd_text);
					
					if (OCI_SUCCESS == OCIStmtPrepare(ora_cmd->hstmt,
					                                  ora_cmd->ora_cnc->herr,
					                                  (CONST text *) cmd_text,
					                                  strlen(cmd_text),
					                                  OCI_NTV_SYNTAX,
					                                  OCI_DEFAULT)) {
						ora_cmd->stmt_type = 0;
						OCIAttrGet((dvoid *) ora_cmd->hstmt,
						           OCI_HTYPE_STMT,
						           (dvoid *) &ora_cmd->stmt_type,
						           NULL,
						           OCI_ATTR_STMT_TYPE,
						           ora_cmd->ora_cnc->herr);
					
						/* now, really execute the command */
						if (OCI_SUCCESS == OCIStmtExecute(ora_cmd->ora_cnc->hservice,
						                                  ora_cmd->hstmt,
						                                  ora_cmd->ora_cnc->herr,
						                                  (ub4)((OCI_STMT_SELECT == ora_cmd->stmt_type) ? 0 : 1),
						                                  (ub4) 0,
						                                  (CONST OCISnapshot *) NULL,
						                                  (OCISnapshot *) NULL,
						                                  OCI_DEFAULT)) {
							ora_recset->ora_cnc = ora_cmd->ora_cnc;
							if (OCI_STMT_SELECT == ora_cmd->stmt_type) {
								ora_recset->hstmt = ora_cmd->hstmt;
								return init_recordset_fields(recset, ora_recset, ora_cmd, error);
							}
							else return recset;
						}
					}
					gda_server_error_make(error,
					                      0,
					                      gda_server_command_get_connection(cmd),
					                      __PRETTY_FUNCTION__);
					break;
			}
		}
		
		/* free memory on error */
		gda_server_recordset_free(recset);
		recset = NULL;
	}
	
	return recset;
}

void
gda_oracle_command_free (GdaServerCommand *cmd)
{
  ORACLE_Command* ora_cmd;

  g_return_if_fail(cmd != NULL);

  ora_cmd = (ORACLE_Command *) gda_server_command_get_user_data(cmd);
  if (ora_cmd)
    {
      /* free Stmt handle allocated in gda_oracle_command_new() */
      if (ora_cmd->hstmt != NULL)
	{
	  OCIHandleFree((dvoid *) ora_cmd->hstmt, OCI_HTYPE_STMT);
	}
      g_free((gpointer) ora_cmd);
    }
}
