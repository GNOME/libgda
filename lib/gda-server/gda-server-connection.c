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

static void
free_error_list (GList *list)
{
  GList* node;

  g_return_if_fail(list != NULL);

  while ((node = g_list_first(list)))
    {
      Gda_ServerError* error = (Gda_ServerError *) node->data;
      list = g_list_remove(list, (gpointer) error);
      gda_server_error_free(error);
    }
}

/**
 * gda_server_connection_new
 */
Gda_ServerConnection *
gda_server_connection_new (Gda_ServerImpl *server_impl)
{
  Gda_ServerConnection* cnc;

  g_return_val_if_fail(server_impl != NULL, NULL);

  /* FIXME: could be possible to share connections */
  cnc = g_new0(Gda_ServerConnection, 1);
  cnc->server_impl = server_impl;
  cnc->users = 1;

  /* notify 'real' provider */
  cnc->server_impl->connections = g_list_append(cnc->server_impl->connections, (gpointer) cnc);
  if (cnc->server_impl->functions.connection_new != NULL)
    {
      cnc->server_impl->functions.connection_new(cnc);
    }

  return cnc;
}

/**
 * gda_server_connection_open
 */
gint
gda_server_connection_open (Gda_ServerConnection *cnc,
			    const gchar *dsn,
			    const gchar *user,
			    const gchar *password)
{
  gint rc;

  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(dsn != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_open != NULL, -1);

  rc = cnc->server_impl->functions.connection_open(cnc, dsn, user, password);
  if (rc != -1)
    {
      gda_server_connection_set_dsn(cnc, dsn);
      gda_server_connection_set_username(cnc, user);
      gda_server_connection_set_password(cnc, password);
      rc = 0;
    }
  return rc;
}

/**
 * gda_server_connection_close
 */
void
gda_server_connection_close (Gda_ServerConnection *cnc)
{
  g_return_if_fail(cnc != NULL);
  g_return_if_fail(cnc->server_impl != NULL);
  g_return_if_fail(cnc->server_impl->functions.connection_close != NULL);

  cnc->server_impl->functions.connection_close(cnc);
}

/**
 * gda_server_connection_begin_transaction
 */
gint
gda_server_connection_begin_transaction (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_begin_transaction != NULL, -1);

  return cnc->server_impl->functions.connection_begin_transaction(cnc);
}

/**
 * gda_server_connection_commit_transaction
 */
gint
gda_server_connection_commit_transaction (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_commit_transaction != NULL, -1);

  return cnc->server_impl->functions.connection_commit_transaction(cnc);
}

/**
 * gda_server_connection_rollback_transaction
 */
gint
gda_server_connection_rollback_transaction (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_rollback_transaction != NULL, -1);

  return cnc->server_impl->functions.connection_rollback_transaction(cnc);
}

/**
 * gda_server_connection_open_schema
 */
Gda_ServerRecordset *
gda_server_connection_open_schema (Gda_ServerConnection *cnc,
				   Gda_ServerError *error,
				   GDA_Connection_QType t,
				   GDA_Connection_Constraint *constraints,
				   gint length)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  g_return_val_if_fail(cnc->server_impl != NULL, NULL);
  g_return_val_if_fail(cnc->server_impl->functions.connection_open_schema != NULL, NULL);

  return cnc->server_impl->functions.connection_open_schema(cnc, error, t, constraints, length);
}

/**
 * gda_server_connection_start_logging
 */
gint
gda_server_connection_start_logging (Gda_ServerConnection *cnc, const gchar *filename)
{
  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_start_logging != NULL, -1);

  return cnc->server_impl->functions.connection_start_logging(cnc, filename);
}

/**
 * gda_server_connection_stop_logging
 */
gint
gda_server_connection_stop_logging (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_stop_logging != NULL, -1);

  return cnc->server_impl->functions.connection_stop_logging(cnc);
}

/**
 * gda_server_connection_create_table
 */
gchar *
gda_server_connection_create_table (Gda_ServerConnection *cnc, GDA_RowAttributes *columns)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  g_return_val_if_fail(cnc->server_impl != NULL, NULL);
  g_return_val_if_fail(cnc->server_impl->functions.connection_create_table != NULL, NULL);
  g_return_val_if_fail(columns != NULL, NULL);

  return cnc->server_impl->functions.connection_create_table(cnc, columns);
}

/**
 * gda_server_connection_supports
 */
gboolean
gda_server_connection_supports (Gda_ServerConnection *cnc, GDA_Connection_Feature feature)
{
  g_return_val_if_fail(cnc != NULL, FALSE);
  g_return_val_if_fail(cnc->server_impl != NULL, FALSE);
  g_return_val_if_fail(cnc->server_impl->functions.connection_supports != NULL, FALSE);

  return cnc->server_impl->functions.connection_supports(cnc, feature);
}

/**
 * gda_server_connection_get_gda_type
 */
GDA_ValueType
gda_server_connection_get_gda_type (Gda_ServerConnection *cnc, gulong sql_type)
{
  g_return_val_if_fail(cnc != NULL, GDA_TypeNull);
  g_return_val_if_fail(cnc->server_impl != NULL, GDA_TypeNull);
  g_return_val_if_fail(cnc->server_impl->functions.connection_get_gda_type != NULL, GDA_TypeNull);

  return cnc->server_impl->functions.connection_get_gda_type(cnc, sql_type);
}

/**
 * gda_server_connection_get_c_type
 */
gshort
gda_server_connection_get_c_type (Gda_ServerConnection *cnc, GDA_ValueType type)
{
  g_return_val_if_fail(cnc != NULL, -1);
  g_return_val_if_fail(cnc->server_impl != NULL, -1);
  g_return_val_if_fail(cnc->server_impl->functions.connection_get_c_type != NULL, -1);

  return cnc->server_impl->functions.connection_get_c_type(cnc, type);
}

/**
 * gda_server_connection_get_dsn
 */
gchar *
gda_server_connection_get_dsn (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  return cnc->dsn;
}

/**
 * gda_server_connection_set_dsn
 */
void
gda_server_connection_set_dsn (Gda_ServerConnection *cnc, const gchar *dsn)
{
  g_return_if_fail(cnc != NULL);

  if (cnc->dsn) g_free((gpointer) cnc->dsn);
  if (dsn) cnc->dsn = g_strdup(dsn);
  else cnc->dsn = NULL;
}

/**
 * gda_server_connection_get_username
 */
gchar *
gda_server_connection_get_username (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  return cnc->username;
}

/**
 * gda_server_connection_set_username
 */
void
gda_server_connection_set_username (Gda_ServerConnection *cnc, const gchar *username)
{
  g_return_if_fail(cnc != NULL);

  if (cnc->username) g_free((gpointer) cnc->username);
  if (username) cnc->username = g_strdup(username);
  else cnc->username = NULL;
}

/**
 * gda_server_connection_get_password
 */
gchar *
gda_server_connection_get_password (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  return cnc->password;
}

/**
 * gda_server_connection_set_password
 */
void
gda_server_connection_set_password (Gda_ServerConnection *cnc, const gchar *password)
{
  g_return_if_fail(cnc != NULL);

  if (cnc->password) g_free((gpointer) cnc->password);
  if (password) cnc->password = g_strdup(password);
  else cnc->password = NULL;
}

/**
 * gda_server_connection_add_error
 */
void
gda_server_connection_add_error (Gda_ServerConnection *cnc, Gda_ServerError *error)
{
  g_return_if_fail(cnc != NULL);
  g_return_if_fail(error != NULL);

  cnc->errors = g_list_append(cnc->errors, (gpointer) error);
}

/**
 * gda_server_connection_add_error_string
 * @cnc: connection
 * @msg: error message
 *
 * Adds a new error to the given connection from a given error string
 */
void
gda_server_connection_add_error_string (Gda_ServerConnection *cnc, const gchar *msg)
{
  Gda_ServerError* error;

  g_return_if_fail(cnc != NULL);
  g_return_if_fail(msg != NULL);

  error = gda_server_error_new();
  gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
  gda_server_error_set_description(error, msg);
  gda_server_error_set_native(error, msg);
}

/**
 * gda_server_connection_get_user_data
 */
gpointer
gda_server_connection_get_user_data (Gda_ServerConnection *cnc)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  return cnc->user_data;
}

/**
 * gda_server_connection_set_user_data
 */
void
gda_server_connection_set_user_data (Gda_ServerConnection *cnc, gpointer user_data)
{
  g_return_if_fail(cnc != NULL);
  cnc->user_data = user_data;
}

/**
 * gda_server_connection_free
 */
void
gda_server_connection_free (Gda_ServerConnection *cnc)
{
  g_return_if_fail(cnc != NULL);

  if ((cnc->server_impl != NULL) &&
      (cnc->server_impl->functions.connection_free != NULL))
    {
      cnc->server_impl->functions.connection_free(cnc);
    }

  cnc->users--;
  if (!cnc->users)
    {
      if (cnc->dsn) g_free((gpointer) cnc->dsn);
      if (cnc->username) g_free((gpointer) cnc->username);
      if (cnc->password) g_free((gpointer) cnc->password);
      //g_list_foreach(cnc->commands, (GFunc) gda_server_command_free, NULL);
      //free_error_list(cnc->errors);

      if (cnc->server_impl)
	{
	  cnc->server_impl->connections = g_list_remove(cnc->server_impl->connections,
							(gpointer) cnc);
	}
      g_free((gpointer) cnc);
    }
}










