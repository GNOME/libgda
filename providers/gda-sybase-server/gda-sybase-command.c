/*
 * $Id$
 *
 * GNOME DB sybase Provider
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
// Revision 1.4  2001/03/08 17:56:15  holger
// Holger Thon <holger.thon@gnome-db.org>  Mar 8th, 2001
//
// 	* Added debian packaging proto entries for gda-primebase, gda-sybase,
// 	  gda-tds
//
// Revision 1.3  2000/10/29 17:55:29  holger
// - fixed AUTHORS file
// - no code changes to gda-sybase-command.c (timestamp failure)
//
// Revision 1.2  2000/10/06 19:24:45  menthos
// Added Swedish entry to configure.in and changed some C++-style comments causing problems to
// C-style.
//
// Revision 1.1.1.1  2000/08/10 09:32:37  rodrigo
// First version of libgda separated from GNOME-DB
//
// Revision 1.1  2000/08/04 10:20:37  rodrigo
// New Sybase provider
//
//

#include "gda-sybase.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_sybase_command_new (Gda_ServerCommand *cmd)
{
  Gda_ServerConnection *cnc = NULL;
  sybase_Command       *scmd = NULL;
  sybase_Connection    *scnc = NULL;

  g_return_val_if_fail(cmd != NULL, FALSE);
  cnc = gda_server_command_get_connection(cmd);
  g_return_val_if_fail(cnc != NULL, FALSE);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(scnc != NULL, FALSE);
  // FIXME: Reopen connection if this sanity check fails
  g_return_val_if_fail(scnc->cnc != NULL, FALSE);
  
  // Allocate command userdata
  scmd = g_new0(sybase_Command, 1);
  g_return_val_if_fail(scmd != NULL, FALSE);
  
  scmd->cmd = (CS_COMMAND *) NULL;

  if (SYB_CHK((CS_INT *) &scnc->ret, ct_cmd_alloc(scnc->cnc, &scmd->cmd),
	      NULL, NULL, cnc, NULL
	      ) != CS_SUCCEED)
    {
      gda_sybase_cleanup(scnc, scnc->ret, "Could not allocate "
			 "cmd structure\n");
      g_free((gpointer) scmd);
      return FALSE;
    }
  gda_server_command_set_user_data(cmd, (gpointer) scmd);

  return TRUE;
}

Gda_ServerRecordset *
gda_sybase_command_execute (Gda_ServerCommand *cmd,
                            Gda_ServerError *error,
                            const GDA_CmdParameterSeq *params,
                            gulong *affected,
                            gulong options)
{
  Gda_ServerConnection* cnc = NULL;
  Gda_ServerRecordset*  recset = NULL;
  sybase_Connection*    scnc = NULL;
  sybase_Command*       scmd = NULL;
  sybase_Recordset*     srecset = NULL;
  gchar*                sql_cmd = NULL;
  CS_INT                result_type;

  g_return_val_if_fail(cmd != NULL, NULL);
  cnc = gda_server_command_get_connection(cmd);
  g_return_val_if_fail(cnc != NULL, NULL);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(scnc != NULL, NULL);
  scmd = (sybase_Command *)    gda_server_command_get_user_data(cmd);
  
  g_return_val_if_fail(scmd->cmd != NULL, NULL);

  sql_cmd = gda_server_command_get_text(cmd);
  g_return_val_if_fail(sql_cmd != NULL, NULL);

  if (gda_sybase_connection_dead(cnc))
    {
//      gda_server_error_make(error, 0, cnc, "connection seems to be dead");
      gda_log_error(_("Connection seems to be dead"));
      sybase_chkerr(error, NULL, cnc, cmd, __PRETTY_FUNCTION__);
    }

#ifdef SYBASE_DEBUG
  gda_log_message(_("executing command '%s'"), sql_cmd);
#endif

  // Initialize cmd structure and send query
  if (SYB_CHK((CS_INT *) &scnc->ret,
              ct_command(scmd->cmd, CS_LANG_CMD, sql_cmd, CS_NULLTERM,
                         CS_UNUSED),
              error, NULL, cnc, cmd
 	          ) != CS_SUCCEED)
    {
      // Cancel command if setting cmd structure fails
      SYB_CHK((CS_INT *) &scnc->ret, ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
	      error, NULL, cnc, cmd
	      );
      gda_log_error(_("Filling command structure failed\n"));
      return NULL;
    }
  if (SYB_CHK((CS_INT *) &scnc->ret, ct_send(scmd->cmd),
	      error, NULL, cnc, cmd
	      ) != CS_SUCCEED
      )
    {
      // Cancel command if sending command fails
      SYB_CHK((CS_INT *) &scnc->ret, ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
	      error, NULL, cnc, cmd
	      );
      gda_log_error(_("sending cmd failed"));
      return NULL;
    }

  // Create a new recordset
  recset = gda_server_recordset_new(cnc);
  if (!recset) {
    gda_log_error(_("Allocating recordset failed"));
    SYB_CHK((CS_INT *) &scnc->ret, ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
	    error, NULL, cnc, cmd
	    );
    return NULL;
  }
  srecset = (sybase_Recordset *) gda_server_recordset_get_user_data(recset);
  
  if (!srecset) {
    gda_server_recordset_free(recset);
    gda_log_error(_("Getting recordset userdata failed"));
    SYB_CHK((CS_INT *) &scnc->ret, ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
	    error, NULL, cnc, cmd
	    );
    return NULL;
  }

  // initialize recordset
  srecset->cnc  = cnc;
  srecset->cmd  = cmd;
  srecset->scnc = scnc;
  srecset->scmd = scmd;
  
  /* Fetching results is a bit tricky,
     because sybase can have multiple result types outstanding on one query.
     If we do not fetch all results, the command structure having results
     blocks the connection (i.e. all commands using the same connection).
     If we want to cleanly perform the query, we'd need to either hack
     the ct_results loop into gda_sybase_recordset_move_next func
     or proceed the results within here, storing the result data redundant
     in srecset. In 2nd case, we'd have resultdata at least twice in memory.
     On the other hand, we could return a correct affected result, because+
     sybase just can return a CS_ROW_COUNT after all rows have been fetched.

     We have to keep our eyes on recset pointer we return, though.
     At the moment, we try to just fetch the row results and first cursor
     results, which is not clean, but saves memory:
     1. switch ct_results
     2. if row/cursor result: init recordset(ct_res_info/ct_bind)
        if succeed: return recordset
     3. gda will ct_fetch result using gda_sybase_recordset_move_next()
     If anything fails or is unknown, ct_cancel all results of current cmd
  */
  while (SYB_CHK((CS_INT *) &scnc->ret, ct_results(scmd->cmd, &result_type),
		 error, recset, cnc, cmd
		 ) == CS_SUCCEED
	 ) {
    srecset->ret = result_type;
    
    switch((int)result_type) {
    case CS_ROW_RESULT:
    case CS_CURSOR_RESULT:
      gda_sybase_init_recset_fields(error, recset, srecset,
				    result_type);
      if (recset) {
	if (affected) {
	  *affected = srecset->rows_cnt;
	}
	return recset;
      } else {
	SYB_CHK((CS_INT *) &scnc->ret,
		ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
		error, recset, cnc, cmd);
	return NULL;
      }
      break;
    case CS_CMD_DONE:
#ifdef SYBASE_DEBUG
      gda_log_message(_("Command execution finished, all results processed"));
#endif
      break;
    case CS_CMD_FAIL:
      gda_log_error(_("Command failed"));
      break;
    case CS_CMD_SUCCEED:
#ifdef SYBASE_DEBUG
      gda_log_message(_("Execution of resultless command succeeded"));
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
      if (srecset && !srecset->data) {
	ct_cancel(NULL, scmd->cmd, CS_CANCEL_CURRENT);
      } else {
	ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL);
      }
      gda_log_message(_("ct_results() returned %x"), result_type);
      break;
    }
  }
  
	
  // Check if we succeeded
  switch ((int)scnc->ret) {
  case CS_END_RESULTS:
#ifdef SYBASE_DEBUG
    gda_log_message(_("No further results, command completly succeeded."));
#endif
    break;
  case CS_FAIL:
    gda_log_error(_("An error occured during command execution."));
    SYB_CHK((CS_INT *) &scnc->ret,
	    ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
	    error, recset, cnc, cmd
	    );
    break;
  default:
    gda_log_error(_("Cmd execution returned unknown result, "
		    "may be an error"));
    SYB_CHK((CS_INT *) &scnc->ret,
	    ct_cancel(NULL, scmd->cmd, CS_CANCEL_ALL),
	    error, recset, cnc, cmd
	    );
    break;
  }
  
  // Did we initialize any data in sybase_Recordset?
  // If not, we can free the recordset
  if (srecset && !srecset->data) {
#ifdef SYBASE_DEBUG
    gda_log_message(_("No data returned, freeing recordset structure"));
#endif
    gda_server_recordset_free(recset);
    recset = NULL;
  }
  
  return recset;
}

void
gda_sybase_command_free (Gda_ServerCommand *cmd)
{
  Gda_ServerConnection *cnc = NULL;
  sybase_Connection    *scnc = NULL;
  sybase_Command       *scmd = NULL;
  
  g_return_if_fail(cmd != NULL);
  cnc  = gda_server_command_get_connection(cmd);
  if (cnc) {
    scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  }
  scmd = (sybase_Command *) gda_server_command_get_user_data(cmd);
  
  // Be careful: We have not checked any pointer so far because we want to
  //             free any cmd structure
  if (scmd && scnc) {
    if (scmd->cmd) {
      if (SYB_CHK((CS_INT *) &scnc->ret, ct_cmd_drop(scmd->cmd),
		  NULL, NULL, cnc, cmd
		  ) != CS_SUCCEED
	  ) {
	gda_log_error(_("Freeing command structure failed"));
      }
    }
    g_free((gpointer) scmd);
    gda_server_command_set_user_data(cmd, (gpointer) NULL);
  } else if (scmd && !scnc) {
    if (scmd->cmd) {
      if (SYB_CHK(NULL,
		  ct_cmd_drop(scmd->cmd),
		  NULL, NULL, cnc, cmd
		  ) != CS_SUCCEED
	  ) {
	gda_log_error(_("Freeing command structure failed"));
      }
    }
    g_free((gpointer) scmd);
    gda_server_command_set_user_data(cmd, (gpointer) NULL);
  } // else would be we have no command userdata and hence nothing to free
}


