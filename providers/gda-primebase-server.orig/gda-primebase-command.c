/* GNOME DB primebase Provider
 * Copyright (C) 2000 Holger Thon
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

#include "gda-primebase.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_primebase_command_new (GdaServerCommand * cmd)
{
	primebase_Command *pcmd = NULL;

	g_return_val_if_fail (cmd != NULL, FALSE);

	pcmd = g_new0 (primebase_Command, 1);
	if (pcmd) {
		gda_server_command_set_user_data (cmd, (gpointer) pcmd);
		return TRUE;
	}
	return FALSE;
}

GdaServerRecordset *
gda_primebase_command_execute (GdaServerCommand * cmd,
			       GdaServerError * error,
			       const GDA_CmdParameterSeq * params,
			       gulong * affected, gulong options)
{
	GdaServerRecordset *recset = NULL;
	GdaServerConnection *cnc = NULL;
	primebase_Connection *pcnc = NULL;
	primebase_Command *pcmd = NULL;
	primebase_Recordset *prset = NULL;
	gchar *sql_cmd = NULL;
	gchar *ptr = NULL;
	gint cmd_pos = 0;
	gint cmd_len = 0;
	gint cmd_frac_len = 0;

	g_return_val_if_fail (cmd != NULL, NULL);
	pcmd = (primebase_Command *) gda_server_command_get_user_data (cmd);
	g_return_val_if_fail (pcmd != NULL, NULL);
	cnc = gda_server_command_get_connection (cmd);
	g_return_val_if_fail (cnc != NULL, NULL);
	pcnc = (primebase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (pcnc != NULL, NULL);
	sql_cmd = gda_server_command_get_text (cmd);
	g_return_val_if_fail (sql_cmd != NULL, NULL);
	cmd_len = strlen (sql_cmd);
	ptr = sql_cmd;

	// Send command with packages of maximum size to the server
	do {
		cmd_frac_len = (cmd_len - cmd_pos > MAX_DALSIZE) ? MAX_DALSIZE
			: (cmd_len - cmd_pos);

		pcnc->state = CLSend (pcnc->sid, ptr, cmd_frac_len);

		ptr += cmd_frac_len;
		cmd_pos += cmd_frac_len;
	} while ((cmd_pos < cmd_len) || (pcnc->state != A_OK));

	// Break on error
	if (pcnc->state != A_OK) {
		pcnc->state = CLBreak (pcnc->sid, 0);
		return NULL;
	}

	// Start execution of previously sent DAL cmd
	pcnc->state = CLExec (pcnc->sid);

	if (pcnc->state != A_OK) {
		pcnc->state = CLBreak (pcnc->sid, 0);
		return NULL;
	}

	// CLExec() is asynchronous, 
	// but if it returns A_OK we assume that the server executes the query

	// Wait for execution to complete
	// FIXME: This is ugly... ;-)
	while ((pcnc->state = CLState (pcnc->sid)) == A_EXEC);

	switch (pcnc->state) {
	case A_VALUE:
	case A_NULL:		// Data or null data is ready for fetch
		recset = gda_server_recordset_new (cnc);
		if (!recset) {
			gda_log_error (_("Allocating recordset failed"));
			pcnc->state = CLBreak (pcnc->sid, 0);
			return NULL;
		}
		prset = (primebase_Recordset *)
			gda_server_recordset_get_user_data (recset);
		if (!prset) {
			gda_server_recordset_free (recset);
			gda_log_error (_
				       ("Getting recordset userdata failed"));
			pcnc->state = CLBreak (pcnc->sid, 0);
			return NULL;
		}
//              gda_primebase_init_recset_fields(error, recset, prset);
		return recset;
		break;
	case A_READY:
		break;
	case A_ERROR:
	default:
		gda_log_message (_("An error occured."));
		pcnc->state = CLBreak (pcnc->sid, 0);
	}

	return recset;
}

void
gda_primebase_command_free (GdaServerCommand * cmd)
{
	primebase_Command *pcmd = NULL;

	g_return_if_fail (cmd != NULL);

	pcmd = (primebase_Command *) gda_server_command_get_user_data (cmd);
	g_free ((gpointer) pcmd);
	gda_server_command_set_user_data (cmd, (gpointer) NULL);
}
