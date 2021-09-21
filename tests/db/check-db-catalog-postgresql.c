/* check-db-catalog-postgresql.c
 *
 * Copyright 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 * Copyright 2018 Daniel Espinosa <esodan@gmail.com>
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
#include "../test-cnc-utils.h"

#define PROVIDER_DB_CREATE_PARAMS "POSTGRESQL_DBCREATE_PARAMS"
#define PROVIDER_CNC_PARAMS "POSTGRESQL_CNC_PARAMS"

typedef struct {
  GdaDbCatalog *catalog;
  GdaConnection *cnc;
  gboolean started_db;
  gboolean cont;
  gchar *dbname;
  GdaQuarkList *quark_list;
} CheckDbObject;



static void create_users_table (CheckDbObject *self) {
  GError *error = NULL;
  gboolean res = FALSE;
  GdaDbTable *table = NULL;
  GdaDbColumn *column_id;
  GdaDbColumn *column_name;
  GdaDbColumn *column_ctime;
  GdaDbColumn *column_ts;
  GdaDbColumn *column_state;

  if (self->cnc == NULL) {
    return;
  }

  g_assert_nonnull (self->catalog);

  table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE(table), "users");

  column_id = gda_db_column_new ();
  gda_db_column_set_name (column_id,"id");
  gda_db_column_set_type (column_id, G_TYPE_INT);
  gda_db_column_set_autoinc (column_id, TRUE);
  gda_db_column_set_pkey (column_id, TRUE);

  gda_db_table_append_column (table, column_id);

  column_name = gda_db_column_new ();
  gda_db_column_set_name (column_name, "name");
  gda_db_column_set_type (column_name, G_TYPE_STRING);
  gda_db_column_set_size (column_name, 50);

  gda_db_table_append_column (table, column_name);

  column_ctime = gda_db_column_new ();
  gda_db_column_set_name (column_ctime, "create_time");
  gda_db_column_set_type (column_ctime, GDA_TYPE_TIME);

  gda_db_table_append_column (table, column_ctime);

  column_state = gda_db_column_new ();
  gda_db_column_set_name (column_state, "state");
  gda_db_column_set_type (column_state, G_TYPE_BOOLEAN);

  gda_db_table_append_column (table,column_state);

  column_ts = gda_db_column_new ();
  gda_db_column_set_name (column_ts, "mytimestamp");
  gda_db_column_set_type (column_ts, G_TYPE_DATE_TIME);

  gda_db_table_append_column (table, column_ts);

  gda_db_catalog_append_table (self->catalog, table);
  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (table), self->cnc, NULL, &error);

  g_object_unref (table);
  g_object_unref (column_id);
  g_object_unref (column_name);
  g_object_unref (column_ctime);
  g_object_unref (column_ts);
  g_object_unref (column_state);

  if (!res) {
    g_warning ("Error Creating table: %s", error->message);
  }
  g_assert_true (res);
}

static void create_companies_table (CheckDbObject *self) {
  GError *error = NULL;
  gboolean res = FALSE;
  GdaDbTable *table = NULL;
  GdaDbColumn *column_id;
  GdaDbColumn *column_name;
  GdaDbColumn *column_ctime;
  GdaDbColumn *column_ts;
  GdaDbColumn *column_state;

  if (self->cnc == NULL) {
    return;
  }

  g_assert_nonnull (self->catalog);

  table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE(table), "companies");

  column_id = gda_db_column_new ();
  gda_db_column_set_name (column_id,"id");
  gda_db_column_set_type (column_id, G_TYPE_INT);
  gda_db_column_set_autoinc (column_id, TRUE);
  gda_db_column_set_pkey (column_id, TRUE);

  gda_db_table_append_column (table, column_id);

  column_name = gda_db_column_new ();
  gda_db_column_set_name (column_name, "name");
  gda_db_column_set_type (column_name, G_TYPE_STRING);
  gda_db_column_set_size (column_name, 50);

  gda_db_table_append_column (table, column_name);

  column_ctime = gda_db_column_new ();
  gda_db_column_set_name (column_ctime, "create_time");
  gda_db_column_set_type (column_ctime, GDA_TYPE_TIME);

  gda_db_table_append_column (table, column_ctime);

  column_state = gda_db_column_new ();
  gda_db_column_set_name (column_state, "state");
  gda_db_column_set_type (column_state, G_TYPE_BOOLEAN);

  gda_db_table_append_column (table,column_state);

  column_ts = gda_db_column_new ();
  gda_db_column_set_name (column_ts, "mytimestamp");
  gda_db_column_set_type (column_ts, G_TYPE_DATE_TIME);

  gda_db_table_append_column (table, column_ts);

  gda_db_catalog_append_table (self->catalog, table);
  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (table), self->cnc, NULL, &error);

  g_object_unref (table);
  g_object_unref (column_id);
  g_object_unref (column_name);
  g_object_unref (column_ctime);
  g_object_unref (column_ts);
  g_object_unref (column_state);

  if (!res) {
    g_warning ("Error Creating table: %s", error->message);
  }
  g_assert_true (res);
}


static void create_countries_table (CheckDbObject *self) {
  GError *error = NULL;
  gboolean res = FALSE;
  GdaDbTable *table = NULL;
  GdaDbColumn *column_id;
  GdaDbColumn *column_name;
  GdaDbColumn *column_ctime;
  GdaDbColumn *column_ts;
  GdaDbColumn *column_state;

  if (self->cnc == NULL) {
    return;
  }

  g_assert_nonnull (self->catalog);

  table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE(table), "countries");

  column_id = gda_db_column_new ();
  gda_db_column_set_name (column_id,"id");
  gda_db_column_set_type (column_id, G_TYPE_INT);
  gda_db_column_set_autoinc (column_id, TRUE);
  gda_db_column_set_pkey (column_id, TRUE);

  gda_db_table_append_column (table, column_id);

  column_name = gda_db_column_new ();
  gda_db_column_set_name (column_name, "name");
  gda_db_column_set_type (column_name, G_TYPE_STRING);
  gda_db_column_set_size (column_name, 50);

  gda_db_table_append_column (table, column_name);

  column_ctime = gda_db_column_new ();
  gda_db_column_set_name (column_ctime, "create_time");
  gda_db_column_set_type (column_ctime, GDA_TYPE_TIME);

  gda_db_table_append_column (table, column_ctime);

  column_state = gda_db_column_new ();
  gda_db_column_set_name (column_state, "state");
  gda_db_column_set_type (column_state, G_TYPE_BOOLEAN);

  gda_db_table_append_column (table,column_state);

  column_ts = gda_db_column_new ();
  gda_db_column_set_name (column_ts, "mytimestamp");
  gda_db_column_set_type (column_ts, G_TYPE_DATE_TIME);

  gda_db_table_append_column (table, column_ts);

  gda_db_catalog_append_table (self->catalog, table);
  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (table), self->cnc, NULL, &error);

  g_object_unref (table);
  g_object_unref (column_id);
  g_object_unref (column_name);
  g_object_unref (column_ctime);
  g_object_unref (column_ts);
  g_object_unref (column_state);

  if (!res) {
    g_warning ("Error Creating table: %s", error->message);
  }
  g_assert_true (res);
}

static void
test_db_catalog_start (CheckDbObject *self,
                     G_GNUC_UNUSED gconstpointer user_data)
{
  gda_init();
  self->catalog = NULL;
  self->cnc = NULL;
  self->started_db = FALSE;
  self->cont = FALSE;
  GError *error = NULL;

  CreateDBObject *crdbobj = test_create_database ("PostgreSQL");

  g_assert_nonnull (crdbobj);

  self->dbname = crdbobj->dbname;
  self->quark_list = crdbobj->quark_list;

  gchar **env = g_get_environ ();
  const gchar *cnc_string = g_environ_getenv (env, "POSTGRESQL_META_CNC");

  if (cnc_string == NULL) {
    g_print ("Environment variable POSTGRESQL_META_CNC was not set. No PostgreSQL provider test will be performed \n"
			        "You can set it, for example, to 'DB_NAME=$POSTGRES_DB;HOST=postgres;USERNAME=$POSTGRES_USER;PASSWORD=$POSTGRES_PASSWORD'\n");
		return;
  }

  g_message ("Connecting using: %s", cnc_string);

  GString *new_cnc_string = g_string_new (cnc_string);
  g_string_append_printf (new_cnc_string, ";DB_NAME=%s", self->dbname);

  self->cnc = gda_connection_new_from_string("PostgreSQL",
                                             new_cnc_string->str,
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             NULL);
  if (self->cnc == NULL) {
    GdaServerOperation *op;
    op = gda_server_operation_prepare_create_database ("PostgreSQL", "test", &error);
    if (op == NULL) {
      g_message ("Error creating database 'test': %s",
                 error && error->message ? error->message : "No error was set");
      g_clear_error (&error);
      return;
    } else {
      self->cnc = gda_connection_new_from_string("PostgreSQL",
                                             cnc_string,
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             &error);
      if (self->cnc == NULL) {
        g_message ("Error connecting to created database 'test': %s",
                 error && error->message ? error->message : "No error was set");
        g_clear_error (&error);
        return;
      }
    }
  }

  if (!gda_connection_open(self->cnc, &error)) {
    g_warning ("Error Opening connection: %s",
             error && error->message ? error->message : "No error was set");
    return;
  }

  self->catalog = gda_connection_create_db_catalog (self->cnc);

  g_assert_nonnull (self->catalog);

  /* Create DataBase structure */
  create_users_table (self);
  create_companies_table (self);
  create_countries_table (self);
  self->cont = TRUE;
}

static void
test_db_catalog_finish (CheckDbObject *self,
                      G_GNUC_UNUSED gconstpointer user_data)
{
  if (self->cnc != NULL) {
    gda_connection_close(self->cnc,NULL);
    g_object_unref (self->catalog);
    g_object_unref (self->cnc);
  }
}


static void
test_tables (CheckDbObject *self,
             G_GNUC_UNUSED gconstpointer user_data)
{
  if (!self->cont) {
    g_message ("Test skiped");
    return;
  }
  if (self->cnc == NULL) {
      return;
  }
  g_message ("Testing Tables...");
  GList *tables = gda_db_catalog_get_tables (self->catalog);
  g_assert (tables != NULL);
  g_assert (g_list_length (tables) != 0);
  g_assert (g_list_length (tables) == 3);
  GList *lt = NULL;
  for (lt = tables; lt; lt = lt->next) {
    GdaDbTable *table = (GdaDbTable *) lt->data;
    g_message ("Table found: %s", gda_db_base_get_full_name (GDA_DB_BASE (table)));
  }
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  const gchar *db_create_str;
  const gchar *cnc_params;

  db_create_str = g_getenv (PROVIDER_DB_CREATE_PARAMS);
  cnc_params = g_getenv (PROVIDER_CNC_PARAMS);

  if (!db_create_str || !cnc_params) {
      g_print ("Please set POSTGRESQL_DBCREATE_PARAMS and POSTGRESQL_CNC_PARAMS variable"
	      "with dbname, host, user and port (usually 5432)\n");
      g_print ("Test will not be performed\n");
      return EXIT_SUCCESS;
  }

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-db-postgresql/meta-tables",
              CheckDbObject,
              NULL,
              test_db_catalog_start,
              test_tables,
              test_db_catalog_finish);

  return g_test_run();
}
