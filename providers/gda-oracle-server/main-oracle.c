/* GNOME DB ORACLE Provider
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

#include "gda-oracle.h"

static GdaServer*             server_impl = NULL;
static GdaServerImplFunctions server_impl_functions =
{
  gda_oracle_connection_new,
  gda_oracle_connection_open,
  gda_oracle_connection_close,
  gda_oracle_connection_begin_transaction,
  gda_oracle_connection_commit_transaction,
  gda_oracle_connection_rollback_transaction,
  gda_oracle_connection_open_schema,
  gda_oracle_connection_modify_schema,
  gda_oracle_connection_start_logging,
  gda_oracle_connection_stop_logging,
  gda_oracle_connection_create_table,
  gda_oracle_connection_supports,
  gda_oracle_connection_get_gda_type,
  gda_oracle_connection_get_c_type,
  gda_oracle_connection_sql2xml,
  gda_oracle_connection_xml2sql,
  gda_oracle_connection_free,

  gda_oracle_command_new,
  gda_oracle_command_execute,
  gda_oracle_command_free,

  gda_oracle_recordset_new,
  gda_oracle_recordset_move_next,
  gda_oracle_recordset_move_prev,
  gda_oracle_recordset_close,
  gda_oracle_recordset_free,

  gda_oracle_error_make
};

gint
main (gint argc, gchar *argv[])
{
  CORBA_Environment ev;
  CORBA_ORB         orb;

  /* initialize CORBA stuff */
  gda_server_init("gda-oracle-srv", VERSION, argc, argv);

  /* register the server implementation */
  server_impl = gda_server_new("OAFIID:GNOME_GDA_Provider_Oracle_ConnectionFactory",
                                    &server_impl_functions);
  if (server_impl)
    {
      gda_server_start(server_impl);
    }
  else gda_log_error(_("Could not register ORACLE provider implementation"));
  return 0;
}

