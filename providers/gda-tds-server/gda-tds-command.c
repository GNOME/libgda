/*
 * $Id$
 *
 * GNOME DB tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
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

// $Log$
// Revision 1.4.4.1  2001/09/21 10:33:35  rodrigo
// 2001-09-21  Dmitry G. Mastrukov <dmitry@taurussoft.org>
//
// 	* s/GdaServerError/GdaError
//
// Revision 1.4  2001/07/18 23:05:43  vivien
// Ran indent -br -i8 on the files
//
// Revision 1.3  2001/04/07 08:49:34  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * objects renaming (Gda_* to Gda*) to conform to the GNOME
//      naming standards
//
// Revision 1.2  2000/11/21 19:57:13  holger
// 2000-11-21 Holger Thon <holger@gidayu.max.uni-duisburg.de>
//
//      - Fixed missing parameter in gda-sybase-server
//      - Removed ct_diag/cs_diag stuff completly from tds server
//
// Revision 1.1  2000/11/04 23:42:17  holger
// 2000-11-05  Holger Thon <gnome-dms@users.sourceforge.net>
//
//   * Added gda-tds-server
//

#include "gda-tds.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_tds_command_new (GdaServerCommand * cmd)
{
	GdaServerConnection *cnc = NULL;
	tds_Command *tcmd = NULL;
	tds_Connection *tcnc = NULL;

	g_return_val_if_fail (cmd != NULL, FALSE);
	cnc = gda_server_command_get_connection (cmd);
	g_return_val_if_fail (cnc != NULL, FALSE);
	tcnc = (tds_Connection *) gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (tcnc != NULL, FALSE);
	// FIXME: Reopen connection if this sanity check fails
	g_return_val_if_fail (tcnc->cnc != NULL, FALSE);

	// Allocate command userdata
	tcmd = g_new0 (tds_Command, 1);
	g_return_val_if_fail (tcmd != NULL, FALSE);

	tcmd->cmd = (CS_COMMAND *) NULL;

	if ((tcnc->ret = ct_cmd_alloc (tcnc->cnc, &tcmd->cmd)
	    ) != CS_SUCCEED) {
		gda_tds_cleanup (tcnc, tcnc->ret, "Could not allocate "
				 "cmd structure\n");
		g_free ((gpointer) tcmd);
		return FALSE;
	}
	gda_server_command_set_user_data (cmd, (gpointer) tcmd);

	return TRUE;
}

GdaServerRecordset *
gda_tds_command_execute (GdaServerCommand * cmd,
			 GdaError * error,
			 const GDA_CmdParameterSeq * params,
			 gulong * affected, gulong options)
{
	GdaServerConnection *cnc = NULL;
	GdaServerRecordset *recset = NULL;
	tds_Connection *tcnc = NULL;
	tds_Command *tcmd = NULL;
	tds_Recordset *trecset = NULL;
	gchar *sql_cmd = NULL;
	CS_INT result_type;

	g_return_val_if_fail (cmd != NULL, NULL);
	cnc = gda_server_command_get_connection (cmd);
	g_return_val_if_fail (cnc != NULL, NULL);
	tcnc = (tds_Connection *) gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (tcnc != NULL, NULL);
	tcmd = (tds_Command *) gda_server_command_get_user_data (cmd);

	g_return_val_if_fail (tcmd->cmd != NULL, NULL);

	sql_cmd = gda_server_command_get_text (cmd);
	g_return_val_if_fail (sql_cmd != NULL, NULL);

	if (gda_tds_connection_dead (cnc)) {
//      gda_server_error_make(error, 0, cnc, "connection seems to be dead");
		gda_log_error (_("Connection seems to be dead"));
	}

#ifdef TDS_DEBUG
	gda_log_message (_("executing command '%s'"), sql_cmd);
#endif

	// Initialize cmd structure and send query
	if ((tcnc->ret =
	     ct_command (tcmd->cmd, CS_LANG_CMD, sql_cmd, CS_NULLTERM,
			 CS_UNUSED)
	    ) != CS_SUCCEED) {
		// Cancel command if setting cmd structure fails
		tcnc->ret = ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
		gda_log_error (_("Filling command structure failed\n"));
		return NULL;
	}
	if ((tcnc->ret = ct_send (tcmd->cmd)
	    ) != CS_SUCCEED) {
		// Cancel command if sending command fails
		tcnc->ret = ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
		gda_log_error (_("sending cmd failed"));
		return NULL;
	}

	// Create a new recordset
	recset = gda_server_recordset_new (cnc);
	if (!recset) {
		gda_log_error (_("Allocating recordset failed"));
		tcnc->ret = ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
		return NULL;
	}
	trecset =
		(tds_Recordset *) gda_server_recordset_get_user_data (recset);

	if (!trecset) {
		gda_server_recordset_free (recset);
		gda_log_error (_("Getting recordset userdata failed"));
		tcnc->ret = ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
		return NULL;
	}

	// initialize recordset
	trecset->cnc = cnc;
	trecset->cmd = cmd;
	trecset->tcnc = tcnc;
	trecset->tcmd = tcmd;

	/* Fetching results is a bit tricky,
	   because tds can have multiple result types outstanding on one query.
	   If we do not fetch all results, the command structure having results
	   blocks the connection (i.e. all commands using the same connection).
	   If we want to cleanly perform the query, we'd need to either hack
	   the ct_results loop into gda_tds_recordset_move_next func
	   or proceed the results within here, storing the result data redundant
	   in trecset. In 2nd case, we'd have resultdata at least twice in memory.
	   On the other hand, we could return a correct affected result, because+
	   tds just can return a CS_ROW_COUNT after all rows have been fetched.

	   We have to keep our eyes on recset pointer we return, though.
	   At the moment, we try to just fetch the row results and first cursor
	   results, which is not clean, but saves memory:
	   1. switch ct_results
	   2. if row/cursor result: init recordset(ct_res_info/ct_bind)
	   if succeed: return recordset
	   3. gda will ct_fetch result using gda_tds_recordset_move_next()
	   If anything fails or is unknown, ct_cancel all results of current cmd
	 */
	while ((tcnc->ret = ct_results (tcmd->cmd, &result_type)
	       ) == CS_SUCCEED) {
		trecset->ret = result_type;

		switch ((int) result_type) {
		case CS_ROW_RESULT:
		case CS_CURSOR_RESULT:
			gda_tds_init_recset_fields (error, recset, trecset,
						    result_type);
			if (recset) {
				if (affected) {
					*affected = trecset->rows_cnt;
				}
				return recset;
			}
			else {
				// cancel command and return NULL
				tcnc->ret =
					ct_cancel (NULL, tcmd->cmd,
						   CS_CANCEL_ALL);
				return NULL;
			}
			break;
		case CS_CMD_DONE:
#ifdef TDS_DEBUG
			gda_log_message (_
					 ("Command execution finished, all results processed"));
#endif
			break;
		case CS_CMD_FAIL:
			gda_log_error (_("Command failed"));
			break;
		case CS_CMD_SUCCEED:
#ifdef TDS_DEBUG
			gda_log_message (_
					 ("Execution of resultless command succeeded"));
#endif
			break;
		case CS_COMPUTE_RESULT:
		case CS_STATUS_RESULT:
		case CS_COMPUTEFMT_RESULT:
		case CS_MSG_RESULT:
		case CS_ROWFMT_RESULT:
		case CS_DESCRIBE_RESULT:
		default:
			// We cancel any other cmd
			if (trecset && !trecset->data) {
				ct_cancel (NULL, tcmd->cmd,
					   CS_CANCEL_CURRENT);
			}
			else {
				ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
			}
			gda_log_message (_("ct_results() returned %x"),
					 result_type);
			break;
		}
	}


	// Check if we succeeded
	switch ((int) tcnc->ret) {
	case CS_END_RESULTS:
#ifdef TDS_DEBUG
		gda_log_message (_
				 ("No further results, command completly succeeded."));
#endif
		break;
	case CS_FAIL:
		gda_log_error (_
			       ("An error occured during command execution."));
		tcnc->ret = ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
		break;
	default:
		gda_log_error (_("Cmd execution returned unknown result, "
				 "may be an error"));
		tcnc->ret = ct_cancel (NULL, tcmd->cmd, CS_CANCEL_ALL);
		break;
	}

	// Did we initialize any data in tds_Recordset?
	// If not, we can free the recordset
	if (trecset && !trecset->data) {
#ifdef TDS_DEBUG
		gda_log_message (_
				 ("No data returned, freeing recordset structure"));
#endif
		gda_server_recordset_free (recset);
		recset = NULL;
	}

	return recset;
}

void
gda_tds_command_free (GdaServerCommand * cmd)
{
	GdaServerConnection *cnc = NULL;
	tds_Connection *tcnc = NULL;
	tds_Command *tcmd = NULL;

	g_return_if_fail (cmd != NULL);
	cnc = gda_server_command_get_connection (cmd);
	if (cnc) {
		tcnc = (tds_Connection *)
			gda_server_connection_get_user_data (cnc);
	}
	tcmd = (tds_Command *) gda_server_command_get_user_data (cmd);

	// Be careful: We have not checked any pointer so far because we want to
	//             free any cmd structure
	if (tcmd && tcnc) {
		if (tcmd->cmd) {
			if ((tcnc->ret = ct_cmd_drop (tcmd->cmd)
			    ) != CS_SUCCEED) {
				gda_log_error (_
					       ("Freeing command structure failed"));
			}
		}
		g_free ((gpointer) tcmd);
		gda_server_command_set_user_data (cmd, (gpointer) NULL);
	}
	else if (tcmd && !tcnc) {
		if (tcmd->cmd) {
			if (ct_cmd_drop (tcmd->cmd) != CS_SUCCEED) {
				gda_log_error (_
					       ("Freeing command structure failed"));
			}
		}
		g_free ((gpointer) tcmd);
		gda_server_command_set_user_data (cmd, (gpointer) NULL);
	}			// else would be we have no command userdata and hence nothing to free
}
