/* GNOME DB Interbase Provider
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

#include "gda-interbase.h"

/*
 * Private functions
 */
static void
init_recordset_fields (Gda_ServerRecordset *recset,
                       INTERBASE_Recordset *ib_recset,
                       INTERBASE_Connection *ib_cnc,
                       Gda_ServerConnection *cnc) {
	gint num_cols, i;
	
	g_return_if_fail(recset != NULL);
	g_return_if_fail(ib_recset != NULL);
	g_return_if_fail(ib_recset->sqlda != NULL);
	g_return_if_fail(ib_cnc != NULL);
	
	/* reallocate sqlda area with correct size if necessary */
	num_cols = ib_recset->sqlda->sqld;
	if (ib_recset->sqlda->sqln < ib_recset->sqlda->sqld) {
		ib_recset->sqlda = (XSQLDA *) g_malloc0(XSQLDA_LENGTH(num_cols));
		ib_recset->sqlda->sqln = num_cols;
		ib_recset->sqlda->version = 1;
		
		/* re-describe statement */
		if (isc_dsql_describe(ib_cnc->status, &ib_recset->ib_cmd, 1, ib_recset->sqlda)) {
			gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
			return;
		}
		num_cols = ib_recset->sqlda->sqld;
	}
	
	/* get fields description */
	for (i = 0; i < num_cols; i++) {
		Gda_ServerField* field = gda_server_field_new();
		
		gda_server_field_set_name(field, ib_recset->sqlda->sqlvar[i].sqlname);
		gda_server_field_set_sql_type(field, ib_recset->sqlda->sqlvar[i].sqltype);
		gda_server_field_set_defined_length(field, ib_recset->sqlda->sqlvar[i].sqllen);
		gda_server_field_set_actual_length(field, ib_recset->sqlda->sqlvar[i].sqllen);
		gda_server_field_set_scale(field, ib_recset->sqlda->sqlvar[i].sqlscale);
		
		gda_server_recordset_add_field(recset, field);
		gda_log_message("Sending description for field %s", ib_recset->sqlda->sqlvar[i].sqlname);
	}
}

/*
 * Public functions
 */
gboolean
gda_interbase_command_new (Gda_ServerCommand *cmd) {
	isc_stmt_handle       stmt = NULL;
	INTERBASE_Connection* ib_cnc;
	
	g_return_val_if_fail(cmd != NULL, FALSE);
	g_return_val_if_fail(cmd->cnc != NULL, FALSE);
	
	ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cmd->cnc);
	if (ib_cnc) {
		if (isc_dsql_allocate_statement(ib_cnc->status, &ib_cnc->db, &stmt) == 0) {
			gda_server_command_set_user_data(cmd, (gpointer) stmt);
			return TRUE;
		}
		
		gda_server_error_make(gda_server_error_new(), 0, cmd->cnc, __PRETTY_FUNCTION__);
	}
	return FALSE;
}

Gda_ServerRecordset *
gda_interbase_command_execute (Gda_ServerCommand *cmd,
                               Gda_ServerError *error,
                               const GDA_CmdParameterSeq *params,
                               gulong *affected,
                               gulong options) {
	Gda_ServerRecordset*  recset;
	INTERBASE_Connection* ib_cnc;
	
	g_return_val_if_fail(cmd != NULL, NULL);
	g_return_val_if_fail(cmd->cnc != NULL, NULL);
	
	ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cmd->cnc);
	if (ib_cnc) {
		switch (gda_server_command_get_type(cmd)) {
			case GDA_COMMAND_TYPE_TABLE : {
				/* it's a TABLE command, command's text is the name of a table */
				gchar* tmp = g_strdup_printf("SELECT * FROM %s", gda_server_command_get_text(cmd));
				gda_server_command_set_text(cmd, tmp);
				g_free((gpointer) tmp);
			}
			case GDA_COMMAND_TYPE_TEXT : {
				isc_stmt_handle stmt = (isc_stmt_handle) gda_server_command_get_user_data(cmd);
				if (stmt) {
					INTERBASE_Recordset* ib_recset;
					
					/* initialize stuff */
					if (affected) *affected = 0;
					if (!error) error = gda_server_error_new();
					recset = gda_server_recordset_new(cmd->cnc);
					ib_recset = (INTERBASE_Recordset *) gda_server_recordset_get_user_data(recset);
					if (ib_recset) {
						ib_recset->ib_cmd = stmt;
						
						/* prepare statement */
						if (isc_dsql_prepare(ib_cnc->status, &ib_cnc->trans, &stmt, 0,
						                     gda_server_command_get_text(cmd), 1,
						                     ib_recset->sqlda) == 0) {
							if (ib_recset->sqlda->sqld) {
								/* it's a SELECT statement, so prepare field buffers */
								init_recordset_fields(recset, ib_recset, ib_cnc, cmd->cnc);
							}
							
							/* execute command */
							if (isc_dsql_execute(ib_cnc->status, &ib_cnc->trans, &stmt, 1, NULL) == 0)
								return recset;
						}
						gda_server_error_make(error, NULL, cmd->cnc, __PRETTY_FUNCTION__);
					}
					
					/* free memory on error */
					gda_server_recordset_free(recset);
				}
				break;
			}
		}
	}
	return NULL;
}

void
gda_interbase_command_free (Gda_ServerCommand *cmd) {
	INTERBASE_Connection* ib_cnc;
	
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(cmd->cnc != NULL);
	
	ib_cnc = (INTERBASE_Connection *) gda_server_connection_get_user_data(cmd->cnc);
	if (ib_cnc) {
		isc_stmt_handle stmt = (isc_stmt_handle) gda_server_command_get_user_data(cmd);
		if (stmt) {
			if (isc_dsql_free_statement(ib_cnc->status, &stmt, DSQL_close))
				gda_server_error_make(gda_server_error_new(), 0, cmd->cnc, __PRETTY_FUNCTION__);
		}
	}
}
