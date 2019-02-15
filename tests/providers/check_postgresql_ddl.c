/* Test for PostgreSQL provider
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com> 
 * 
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */ 
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libgda/libgda.h>

typedef struct {
    GdaConnection *cnc;
    GdaServerProvider *provider;
    const gchar *cnc_string;
    gboolean cont;
    
}PsqObject;

typedef struct {
    gchar *username;
    gchar *password;
    gboolean encrypt_pass;
    gboolean can_create_db;
    gboolean can_create_user;
} GdaUserInfo;

static void 
db_create_quark_foreach_func (gchar *name, gchar *value, GdaServerOperation *op)
{
	gda_server_operation_set_value_at (op, value, NULL, "/SERVER_CNX_P/%s", name);
}

static void
postgresql_create_db_no_cnc(void)
{
  const gchar *db_string = g_getenv("POSTGRESQL_DBCREATE_PARAMS");

  if (!db_string)
    {
      g_print ("Please set POSTGRESQL_DBCREATE_PARAMS variable"
               "with host,user and port (usually 5432)");
      g_print ("Test will not be performed\n");
      return;
    }

  GError *error = NULL;

  GdaServerProvider *provider = NULL;

  provider = gda_config_get_provider ("PostgreSQL",&error);

  if (!provider)
    {
      g_print ("%s: Can't get provider\n", G_STRLOC);
      g_print ("Error: %s\n",error && error->message ? error->message : "No error set");
    }

  g_assert_nonnull (provider);

  GdaServerOperation *op = gda_server_provider_create_operation (provider,
                                                                 NULL,
                                                                 GDA_SERVER_OPERATION_CREATE_DB,
                                                                 NULL,
                                                                 &error);

  if (!op)
    {
      g_print ("%s: Can't create operation\n", G_STRLOC);
      g_print ("Error: %s\n",error && error->message ? error->message : "No error set");
    }
 
  g_assert_nonnull (op); /* We need to terminate the test if op is NULL */ 

/* We need only host name, port and user for DB creation
 */
  
	GdaQuarkList *db_quark_list = NULL;
  db_quark_list = gda_quark_list_new_from_string (db_string);
  gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);

	gboolean res = gda_server_operation_set_value_at (op, "testddl", NULL, "/DB_DEF_P/DB_NAME");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (provider, NULL, op, &error); 
 
  if (!res)
    {
      g_print ("%s: Can't perform operation\n", G_STRLOC);
      g_print ("Error: %s\n",error && error->message ? error->message : "No error set");
    }
 
  g_object_unref (op);
  g_clear_error (&error);

  g_assert_true (res); /* We need to terminate the test if op is NULL */ 
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add_func ("/postgresql/create_db_no_cn", postgresql_create_db_no_cnc);

  return g_test_run();
}

