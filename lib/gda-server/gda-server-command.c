/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gda-server-impl.h"

/**
 * gda_server_command_new
 */
Gda_ServerCommand *
gda_server_command_new (Gda_ServerConnection *cnc)
{
  Gda_ServerCommand* cmd;

  g_return_val_if_fail(cnc != NULL, NULL);

  cmd = g_new0(Gda_ServerCommand, 1);
  cmd->cnc = cnc;
  cmd->users = 1;

  if ((cmd->cnc->server_impl != NULL) &&
      (cmd->cnc->server_impl->functions.command_new != NULL))
    {
      cmd->cnc->server_impl->functions.command_new(cmd);
    }
  cmd->cnc->commands = g_list_append(cmd->cnc->commands, (gpointer) cmd);

  return cmd;
}

/**
 * gda_server_command_get_connection
 */
Gda_ServerConnection *
gda_server_command_get_connection (Gda_ServerCommand *cmd)
{
  g_return_val_if_fail(cmd != NULL, NULL);
  return cmd->cnc;
}

/**
 * gda_server_command_get_text
 */
gchar *
gda_server_command_get_text (Gda_ServerCommand *cmd)
{
  g_return_val_if_fail(cmd != NULL, NULL);
  return cmd->text;
}

/**
 * gda_server_command_set_text
 */
void
gda_server_command_set_text (Gda_ServerCommand *cmd, const gchar *text)
{
  g_return_if_fail(cmd != NULL);

  if (cmd->text) g_free((gpointer) cmd->text);
  if (text) cmd->text = g_strdup(text);
  else cmd->text = NULL;
}

/**
 * gda_server_command_get_type
 */
gulong
gda_server_command_get_type (Gda_ServerCommand *cmd)
{
  g_return_val_if_fail(cmd != NULL, 0);
  return cmd->type;
}

/**
 * gda_server_command_set_type
 */
void
gda_server_command_set_type (Gda_ServerCommand *cmd, gulong type)
{
  g_return_if_fail(cmd != NULL);
  cmd->type = type;
}

/**
 * gda_server_command_get_user_data
 */
gpointer
gda_server_command_get_user_data (Gda_ServerCommand *cmd)
{
  g_return_val_if_fail(cmd != NULL, NULL);
  return cmd->user_data;
}

/**
 * gda_server_command_set_user_data
 */
void
gda_server_command_set_user_data (Gda_ServerCommand *cmd, gpointer user_data)
{
  g_return_if_fail(cmd != NULL);
  cmd->user_data = user_data;
}

/**
 * gda_server_command_free
 */
void
gda_server_command_free (Gda_ServerCommand *cmd)
{
  g_return_if_fail(cmd != NULL);

  if ((cmd->cnc != NULL) &&
      (cmd->cnc->server_impl != NULL) &&
      (cmd->cnc->server_impl->functions.command_free != NULL))
    {
      cmd->cnc->server_impl->functions.command_free(cmd);
    }

  cmd->users--;
  if (!cmd->users)
    {
      cmd->cnc->commands = g_list_remove(cmd->cnc->commands, (gpointer) cmd);
      if (cmd->text) g_free((gpointer) cmd->text);
      g_free((gpointer) cmd);
    }
}

/**
 * gda_server_command_execute
 */
Gda_ServerRecordset *
gda_server_command_execute (Gda_ServerCommand *cmd,
			    Gda_ServerError *error,
			    const GDA_CmdParameterSeq *params,
			    gulong *affected,
			    gulong options)
{
  Gda_ServerRecordset* recset;

  g_return_val_if_fail(cmd != NULL, NULL);
  g_return_val_if_fail(cmd->cnc != NULL, NULL);
  g_return_val_if_fail(cmd->cnc->server_impl != NULL, NULL);
  g_return_val_if_fail(cmd->cnc->server_impl->functions.command_execute != NULL, NULL);

  recset = cmd->cnc->server_impl->functions.command_execute(cmd, error, params,
							    affected, options);
  return recset;
}

