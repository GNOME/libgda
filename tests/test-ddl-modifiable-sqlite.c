/* check-ddl-modifiable.c
 *
 * Copyright (C) 2020 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#include "gda-db-column.h"
#include "gda-db-index.h"
#include "gda-db-table.h"
#define PROVIDER_NAME "SQLite"
#define DB_TEST_BASE "sqlite_ddl_modifiable"

#include <glib/gi18n.h>
#include <locale.h>
#include "libgda/libgda.h"

typedef struct
{
  GdaConnection *cnc;
  GdaServerProvider *provider;
  GdaDbTable *tproject;
  GdaDbColumn *pid;
  GdaDbColumn *pname;
  GdaDbIndex *index;
} TestObjectFixture;


static void
test_ddl_modifiable_start (TestObjectFixture *fixture,
                           G_GNUC_UNUSED gconstpointer user_data)
{
  fixture->cnc = NULL;
  fixture->provider = NULL;
  fixture->tproject = NULL;
  fixture->pid = NULL;
  fixture->pname = NULL;

  gchar *dbname = g_strdup_printf("DB_DIR=.;DB_NAME=%s_%d", DB_TEST_BASE, g_random_int ());

  g_print ("Will use DB: %s\n", dbname);

  fixture->cnc = gda_connection_open_from_string (PROVIDER_NAME,
                                                  dbname,
                                                  NULL,
                                                  GDA_CONNECTION_OPTIONS_NONE,
                                                  NULL);

  g_assert_nonnull (fixture->cnc);

  g_free (dbname);

  fixture->provider = gda_connection_get_provider (fixture->cnc);

  g_assert_nonnull (fixture->provider);

/* Define table Project */
  fixture->tproject = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE (fixture->tproject), "Project");
  gda_db_table_set_is_temp (fixture->tproject, FALSE);

  /* Defining column id */
  fixture->pid = gda_db_column_new ();
  gda_db_column_set_name (fixture->pid, "id");
  gda_db_column_set_type (fixture->pid, G_TYPE_INT);
  gda_db_column_set_nnul (fixture->pid, TRUE);
  gda_db_column_set_autoinc (fixture->pid, TRUE);
  gda_db_column_set_unique (fixture->pid, TRUE);
  gda_db_column_set_pkey (fixture->pid, TRUE);

  gda_db_table_append_column (fixture->tproject, fixture->pid);

  g_object_unref (fixture->pid);

  /* Defining column name */
  fixture->pname = gda_db_column_new ();
  gda_db_column_set_name (fixture->pname, "name");
  gda_db_column_set_type (fixture->pname, G_TYPE_STRING);
  gda_db_column_set_size (fixture->pname, 50);
  gda_db_column_set_nnul (fixture->pname, TRUE);
  gda_db_column_set_autoinc (fixture->pname, FALSE);
  gda_db_column_set_unique (fixture->pname, TRUE);
  gda_db_column_set_pkey (fixture->pname, FALSE);
  gda_db_column_set_default (fixture->pname, "Default_name");

  gda_db_table_append_column (fixture->tproject, fixture->pname);

  /* We don't need to dereference fixture->pname column yet. We will reuse it */

  fixture->index = gda_db_index_new ();

  gda_db_base_set_name (GDA_DB_BASE (fixture->index), "simpleindex");

  GdaDbIndexField *ifield = gda_db_index_field_new ();
  gda_db_index_field_set_column (ifield, fixture->pname);

  gda_db_index_append_field (fixture->index, ifield);
  g_object_unref (ifield);
}

static void
test_ddl_modifiable_finish (TestObjectFixture *fixture,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  g_object_unref (fixture->pname);
  g_object_unref (fixture->tproject);
  g_object_unref (fixture->index);

  gboolean res = gda_connection_close (fixture->cnc, NULL);

  g_assert_true (res);
}

static void
test_ddl_modifiable_table (TestObjectFixture *fixture,
                           G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res;

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, NULL, NULL);

  g_assert_true (res);

/* CREATE TABLE second time call  */
  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, NULL, NULL);

  g_assert_true (res);

/* RENAME TABLE */
  GdaDbTable *new_table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE (new_table), "NewProject");

  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, new_table, NULL);

  g_assert_true (res);

/* Calling the same operation twice */
  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, new_table, NULL);

  g_assert_false (res);

  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (new_table), fixture->cnc, NULL, NULL);

  g_assert_true (res);

  g_object_unref (new_table);
}

static void
test_ddl_modifiable_column (TestObjectFixture *fixture,
                            G_GNUC_UNUSED gconstpointer user_data)
{
/* CREATE TABLE */
  gboolean res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, NULL, NULL);

  g_assert_true (res);

  GdaDbColumn *last_name  = gda_db_column_new();
  gda_db_column_set_name (last_name, "LastName");
  gda_db_column_set_type (last_name, G_TYPE_STRING);

/* table property wasn't set. The operation should fail */
  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (last_name), fixture->cnc, NULL, NULL);

  g_assert_false (res);

  g_object_set (last_name, "table", fixture->tproject, NULL);

  GError *error = NULL;

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (last_name), fixture->cnc, NULL, &error);

  if (error != NULL) {
    g_print ("Error: %s", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_true (res);

  g_object_unref (last_name);

  last_name  = gda_db_column_new();
  gda_db_column_set_name (last_name, "NewLastName");
  gda_db_column_set_type (last_name, G_TYPE_STRING);

  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (fixture->pname), fixture->cnc, last_name, &error);

  if (error != NULL) {
    g_print ("Error: %s", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_true (res);

/* Calling the same operation twice. It should fail */
  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (fixture->pname), fixture->cnc, last_name, &error);

  if (error != NULL) {
    g_print ("Error: %s", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_false (res);

  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (last_name), fixture->cnc, NULL, &error);

  if (error != NULL) {
    g_print ("Error: %s", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_false (res);

  g_object_unref (last_name);
}

static void
test_ddl_modifiable_index (TestObjectFixture *fixture,
                           G_GNUC_UNUSED gconstpointer user_data)
{
  /* CREATE TABLE */
  gboolean res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, NULL, NULL);

  g_assert_true (res);

  GdaDbIndex *newindex = gda_db_index_new ();
  gda_db_base_set_name (GDA_DB_BASE (newindex), "newindex");
  GError *error = NULL;

  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (fixture->index), fixture->cnc, newindex, &error);

  if (error != NULL) {
    g_print ("Error: %s\n", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_false (res); // RENAME INDEX operation is not supported by SQLite3

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->index), fixture->cnc, newindex, &error);

  if (error != NULL) {
    g_print ("Error: %s\n", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_false (res); // CREATE INDEX without table should fail.

  g_object_set (fixture->index, "table", fixture->tproject, NULL);

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->index), fixture->cnc, newindex, &error);

  if (error != NULL) {
    g_print ("Error: %s\n", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_true (res); // CREATE INDEX with table should succeed.

  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (fixture->index), fixture->cnc, NULL, &error);

  if (error != NULL) {
    g_print ("Error: %s\n", error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  g_assert_true (res); // DROP INDEX

  g_object_unref (newindex);
}

static void
test_ddl_modifiable_view (TestObjectFixture *fixture,
                          G_GNUC_UNUSED gconstpointer user_data)
{
  /* CREATE TABLE */
  gboolean res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (fixture->tproject), fixture->cnc, NULL, NULL);

  g_assert_true (res);

  GdaDbView *myview = gda_db_view_new ();

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (myview), fixture->cnc, NULL, NULL);
  g_assert_false (res);

  gda_db_base_set_name (GDA_DB_BASE (myview), "MyView");

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (myview), fixture->cnc, NULL, NULL);
  g_assert_false (res);

  gda_db_view_set_defstring (myview, "SELECT id, name FROM Project");

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (myview), fixture->cnc, NULL, NULL);
  g_assert_true (res);

  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (myview), fixture->cnc, "NewView", NULL);

  g_assert_false (res); // RENAME VIEW is not implemented for Sqlite3

  /* DROP_VIEW operation. */
  GdaDbViewRefAction action = GDA_DB_VIEW_RESTRICT;

  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (myview), fixture->cnc, &action, NULL);

  g_assert_true (res);

  g_object_unref (myview);
}

gint
main(gint argc, gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-ddl-modifiable-sqlite/table",
              TestObjectFixture,
              NULL,
              test_ddl_modifiable_start,
              test_ddl_modifiable_table,
              test_ddl_modifiable_finish);

  g_test_add ("/test-ddl-modifiable-sqlite/column",
              TestObjectFixture,
              NULL,
              test_ddl_modifiable_start,
              test_ddl_modifiable_column,
              test_ddl_modifiable_finish);

  g_test_add ("/test-ddl-modifiable-sqlite/index",
              TestObjectFixture,
              NULL,
              test_ddl_modifiable_start,
              test_ddl_modifiable_index,
              test_ddl_modifiable_finish);

  g_test_add ("/test-ddl-modifiable-sqlite/view",
              TestObjectFixture,
              NULL,
              test_ddl_modifiable_start,
              test_ddl_modifiable_view,
              test_ddl_modifiable_finish);
  return g_test_run();
}



