/* GNOME DB Interbase Provider
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

#include "gda-interbase.h"

static Gda_ServerImpl*         server_impl = NULL;
static Gda_ServerImplFunctions server_impl_functions =
{
  gda_interbase_connection_new,
  gda_interbase_connection_open,
  gda_interbase_connection_close,
  gda_interbase_connection_begin_transaction,
  gda_interbase_connection_commit_transaction,
  gda_interbase_connection_rollback_transaction,
  gda_interbase_connection_open_schema,
  gda_interbase_connection_start_logging,
  gda_interbase_connection_stop_logging,
  gda_interbase_connection_create_table,
  gda_interbase_connection_supports,
  gda_interbase_connection_get_gda_type,
  gda_interbase_connection_get_c_type,
  gda_interbase_connection_free,

  gda_interbase_command_new,
  gda_interbase_command_execute,
  gda_interbase_command_free,

  gda_interbase_recordset_new,
  gda_interbase_recordset_move_next,
  gda_interbase_recordset_move_prev,
  gda_interbase_recordset_close,
  gda_interbase_recordset_free,

  gda_interbase_error_make
};

gint
main (gint argc, gchar *argv[])
{
  CORBA_Environment ev;
  CORBA_ORB         orb;

  /* initialize CORBA stuff */
  CORBA_exception_init(&ev);
#if defined(USING_OAF)
  gda_server_init("gda-interbase-srv", VERSION, argc, argv);
#else
  orb = gnome_CORBA_init("gda-interbase-srv", VERSION, &argc, argv,
			 GNORBA_INIT_SERVER_FUNC, &ev);
#endif
  gda_server_impl_exception(&ev);

  /* register the server implementation */
  server_impl = gda_server_impl_new("OAFIID:gda-interbase:f3dee204-4e44-4505-8ca8-9703c4473c0f",
                                    &server_impl_functions);
  if (server_impl)
    {
      gda_server_impl_start(server_impl);
    }
  else gda_log_error(_("Could not register INTERBASE provider implementation"));
  return 0;
}

