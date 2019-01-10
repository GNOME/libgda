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

typedef struct {
  GdaDbCatalog *catalog;
  GdaConnection *cnc;
  gboolean started_db;
  gboolean cont;
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
  res = gda_db_table_create (table, self->cnc, TRUE, &error);

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
  res = gda_db_table_create (table, self->cnc, TRUE, &error);

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
  res = gda_db_table_create (table, self->cnc, TRUE, &error);

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
                     gconstpointer user_data)
{
  gda_init();
  self->catalog = NULL;
  self->cnc = NULL;
  self->started_db = FALSE;
  self->cont = FALSE;
  GError *error = NULL;

  gchar **env = g_get_environ ();
  const gchar *cnc_string = g_environ_getenv (env, "POSTGRESQL_CNC_PARAMS");

  g_message ("Connecting using: %s", cnc_string);

  if (cnc_string == NULL) {
    g_message ("Skipping. Error creating connection");
    return;
  }

  self->cnc = gda_connection_new_from_string("PostgreSQL",
                                             cnc_string,
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             &error);
  if (self->cnc == NULL) {
    g_message ("Skipping. Error creating connection: %s",
               error && error->message ? error->message : "No error was set");
    return;
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
                      gconstpointer user_data)
{
  if (self->cnc != NULL) {
    gda_connection_close(self->cnc,NULL);
    g_object_unref (self->catalog);
    g_object_unref (self->cnc);
  }
}


static void
test_tables (CheckDbObject *self,
             gconstpointer user_data)
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

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-db-postgresql/meta-tables",
              CheckDbObject,
              NULL,
              test_db_catalog_start,
              test_tables,
              test_db_catalog_finish);

  return g_test_run();
}
