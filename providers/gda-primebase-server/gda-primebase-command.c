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
gda_primebase_command_new (Gda_ServerCommand *cmd)
{
  primebase_Command    *pcmd = NULL;

  g_return_val_if_fail(cmd != NULL, FALSE);

  pcmd = g_new0(primebase_Command, 1);
  if (!pcmd) {
    return FALSE;
  }

  gda_server_command_set_user_data(cmd, (gpointer) pcmd);
  return TRUE;
}

Gda_ServerRecordset *
gda_primebase_command_execute (Gda_ServerCommand *cmd,
                               Gda_ServerError *error,
                               const GDA_CmdParameterSeq *params,
                               gulong *affected,
                               gulong options)
{
  Gda_ServerRecordset  *recset = NULL;
  Gda_ServerConnection *cnc = NULL;
  primebase_Connection *pcnc = NULL;
  primebase_Command    *pcmd = NULL;
  primebase_Recordset  *prset = NULL;
  gchar                *sql_cmd = NULL;
  gchar                *ptr     = NULL;
  gint                 cmd_pos      = 0;
  gint                 cmd_len      = 0;
  gint                 cmd_frac_len = 0;
  
  g_return_val_if_fail(cmd != NULL, NULL);
  pcmd = (primebase_Command *) gda_server_command_get_user_data(cmd);
  g_return_val_if_fail(pcmd != NULL, NULL);
  cnc = gda_server_command_get_connection(cmd);
  g_return_val_if_fail(cnc != NULL, NULL);
  pcnc = (primebase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(pcnc != NULL, NULL);
  sql_cmd = gda_server_command_get_text(cmd);
  g_return_val_if_fail(sql_cmd != NULL, NULL);
  cmd_len = strlen(sql_cmd);
  ptr = sql_cmd;

  switch(gda_server_command_get_type(cmd)) {
    case GDA_COMMAND_TYPE_FILE:
      return NULL;
      break;
    case GDA_COMMAND_TYPE_STOREDPROCEDURE:
      return NULL;
      break;
    case GDA_COMMAND_TYPE_TEXT:
      return NULL;
      break;
    case GDA_COMMAND_TYPE_TABLE:
      return NULL;
      break;
    case GDA_COMMAND_TYPE_TABLEDIRECT:
      return NULL;
      break;
    case GDA_COMMAND_TYPE_XML:
      return NULL;
      break;
    default:
      return NULL;
		break;
  }
  
  return recset;
}

void
gda_primebase_command_free (Gda_ServerCommand *cmd)
{
  primebase_Command *pcmd = NULL;
  
  g_return_if_fail(cmd != NULL);

  pcmd = (primebase_Command *) gda_server_command_get_user_data(cmd);
  g_free((gpointer) pcmd);
  gda_server_command_set_user_data(cmd, (gpointer) NULL);
}
