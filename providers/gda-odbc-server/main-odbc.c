/* GNOME DB ODBC Provider
 * Copyright (C) 2000 Nick Gorham
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

#include "gda-odbc.h"

static GdaServer*             server_impl = NULL;
static GdaServerImplFunctions server_impl_functions =
{
  gda_odbc_connection_new,
  gda_odbc_connection_open,
  gda_odbc_connection_close,
  gda_odbc_connection_begin_transaction,
  gda_odbc_connection_commit_transaction,
  gda_odbc_connection_rollback_transaction,
  gda_odbc_connection_open_schema,
  gda_odbc_connection_modify_schema,
  gda_odbc_connection_start_logging,
  gda_odbc_connection_stop_logging,
  gda_odbc_connection_create_table,
  gda_odbc_connection_supports,
  gda_odbc_connection_get_gda_type,
  gda_odbc_connection_get_c_type,
  gda_odbc_connection_sql2xml,
  gda_odbc_connection_xml2sql,
  gda_odbc_connection_free,

  gda_odbc_command_new,
  gda_odbc_command_execute,
  gda_odbc_command_free,

  gda_odbc_recordset_new,
  gda_odbc_recordset_move_next,
  gda_odbc_recordset_move_prev,
  gda_odbc_recordset_close,
  gda_odbc_recordset_free,

  gda_odbc_error_make
};

gint
main (gint argc, gchar *argv[])
{
  CORBA_Environment ev;
  CORBA_ORB         orb;

  /* initialize CORBA stuff */
  gda_server_init("gda-odbc-srv", VERSION, argc, argv);

  /* register the server implementation */
  server_impl = gda_server_new("OAFIID:GNOME_GDA_Provider_ODBC_ConnectionFactory",
                               &server_impl_functions);
  if (server_impl)
    {
      gda_server_start(server_impl);
    }
  else gda_log_error(_("Could not register ODBC provider implementation"));
  return 0;
}

