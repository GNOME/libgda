/* check-server-operation.c
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libgda/libgda.h>

typedef struct {
    GdaServerOperation *op;
    GdaConnection *cnc;
    GdaServerProvider *provider;
} CheckOPObject;

static void
test_server_operation_provider (CheckOPObject *self,
                                gconstpointer user_data)
{
  GdaServerProvider *server = NULL;
  GdaConnection *cnc = NULL;

  g_object_get (self->op,"provider",&server,"connection",&cnc,NULL);

  g_assert_nonnull (server);
  g_assert_nonnull (cnc);

  g_assert_true (GDA_IS_SERVER_PROVIDER(server));
  g_assert_true (GDA_IS_CONNECTION(cnc));
}

static void
test_server_operation_start (CheckOPObject *self,
                             gconstpointer user_data)
{
  self->cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=op_test_db", NULL,
                                               GDA_CONNECTION_OPTIONS_NONE, NULL);
  g_assert_nonnull (self->cnc);

  self->provider = gda_connection_get_provider (self->cnc);

  self->op = gda_server_provider_create_operation (self->provider,
                                                   self->cnc,
                                                   GDA_SERVER_OPERATION_CREATE_TABLE,
                                                   NULL,
                                                   NULL); 
  g_assert_nonnull (self->op);
}

static void
test_server_operation_finish (CheckOPObject *self,
                              gconstpointer user_data)
{
  g_object_unref (self->op);
  gda_connection_close (self->cnc,NULL);
}

gint 
main(gint argc, gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-server-operation/provider",
              CheckOPObject,
              NULL,
              test_server_operation_start,
              test_server_operation_provider,
              test_server_operation_finish);

  return g_test_run();
}

