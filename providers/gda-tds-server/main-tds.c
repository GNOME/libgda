/* 
 * $Id$
 * 
 * GNOME DB tds Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
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

#include "gda-tds.h"

static Gda_ServerImpl*         server_impl = NULL;
static Gda_ServerImplFunctions server_impl_functions =
{
  gda_tds_connection_new,
  gda_tds_connection_open,
  gda_tds_connection_close,
  gda_tds_connection_begin_transaction,
  gda_tds_connection_commit_transaction,
  gda_tds_connection_rollback_transaction,
  gda_tds_connection_open_schema,
  gda_tds_connection_modify_schema,
  gda_tds_connection_start_logging,
  gda_tds_connection_stop_logging,
  gda_tds_connection_create_table,
  gda_tds_connection_supports,
  gda_tds_connection_get_gda_type,
  gda_tds_connection_get_c_type,
  gda_tds_connection_free,

  gda_tds_command_new,
  gda_tds_command_execute,
  gda_tds_command_free,

  gda_tds_recordset_new,
  gda_tds_recordset_move_next,
  gda_tds_recordset_move_prev,
  gda_tds_recordset_close,
  gda_tds_recordset_free,

  gda_tds_error_make
};

gint
main (gint argc, gchar *argv[])
{
  CORBA_Environment ev;
  CORBA_ORB         orb;

  /* initialize CORBA stuff */
  gda_server_init("gda-tds-srv", VERSION, argc, argv);

  /* register the server implementation */
  server_impl = gda_server_impl_new("OAFIID:gda-tds:87cd7ac2-c89d-436c-bf39-7146d2fb0f9e",
                                    &server_impl_functions);
  if (server_impl)
    {
      gda_server_impl_start(server_impl);
    }
  else gda_log_error(_("Could not register gda-tds provider implementation"));
  return 0;
}
