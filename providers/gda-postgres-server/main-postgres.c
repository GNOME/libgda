/* GNOME DB MySQL Provider
 * Copyright (C) 2000 Vivien Malerba
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

#include "gda-postgres.h"

static GdaServer*             server_impl = NULL;
static GdaServerImplFunctions server_impl_functions =
{
  gda_postgres_connection_new,
  gda_postgres_connection_open,
  gda_postgres_connection_close,
  gda_postgres_connection_begin_transaction,
  gda_postgres_connection_commit_transaction,
  gda_postgres_connection_rollback_transaction,
  gda_postgres_connection_open_schema,
  gda_postgres_connection_modify_schema,
  gda_postgres_connection_start_logging,
  gda_postgres_connection_stop_logging,
  gda_postgres_connection_create_table,
  gda_postgres_connection_supports,
  gda_postgres_connection_get_gda_type,
  gda_postgres_connection_get_c_type,
  gda_postgres_connection_sql2xml,
  gda_postgres_connection_xml2sql,
  gda_postgres_connection_free,

  gda_postgres_command_new,
  gda_postgres_command_execute,
  gda_postgres_command_free,

  gda_postgres_recordset_new,
  gda_postgres_recordset_move_next,
  gda_postgres_recordset_move_prev,
  gda_postgres_recordset_close,
  gda_postgres_recordset_free,

  gda_postgres_error_make
};

int
main(int argc, char* argv[])
{
  CORBA_Environment ev;
  CORBA_ORB         orb;

  /* initialize CORBA stuff */
  gda_server_init("gda-postgres-srv", VERSION, argc, argv);

  /* register the server implementation */
  server_impl = gda_server_new("OAFIID:GNOME_GDA_Provider_Postgres_ConnectionFactory",
                                    &server_impl_functions);
  if (server_impl)
    {
      gda_server_start(server_impl);
    }
  else gda_log_error(_("Could not register PostgreSQL provider implementation"));
  return 0;
}



