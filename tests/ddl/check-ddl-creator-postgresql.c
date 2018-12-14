/* check-ddl-creator-postgresql.c
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
  GdaDdlCreator *creator;
  gchar *xmlfile;
  GdaConnection *cnc;
  gboolean started_db;
} CheckDdlObject;



static void create_users_table (CheckDdlObject *self) {
  GError *error = NULL;
  gboolean res = FALSE;
  GdaDdlTable *table = NULL;
  GdaDdlColumn *column_id;
  GdaDdlColumn *column_name;
  GdaDdlColumn *column_ctime;
  GdaDdlColumn *column_ts;
  GdaDdlColumn *column_state;

  if (self->cnc == NULL) {
    return;
  }

  g_assert_nonnull (self->creator);

  table = gda_ddl_table_new ();
  gda_ddl_base_set_name (GDA_DDL_BASE(table), "users");

  column_id = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_id,"id");
  gda_ddl_column_set_type (column_id, G_TYPE_INT);
  gda_ddl_column_set_autoinc (column_id, TRUE);
  gda_ddl_column_set_pkey (column_id, TRUE);

  gda_ddl_table_append_column (table, column_id);

  column_name = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_name, "name");
  gda_ddl_column_set_type (column_name, G_TYPE_STRING);
  gda_ddl_column_set_size (column_name, 50);

  gda_ddl_table_append_column (table, column_name);

  column_ctime = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_ctime, "create_time");
  gda_ddl_column_set_type (column_ctime, GDA_TYPE_TIME);

  gda_ddl_table_append_column (table, column_ctime);

  column_state = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_state, "state");
  gda_ddl_column_set_type (column_state, G_TYPE_BOOLEAN);

  gda_ddl_table_append_column (table,column_state);

  column_ts = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_ts, "mytimestamp");
  gda_ddl_column_set_type (column_ts, G_TYPE_DATE_TIME);

  gda_ddl_table_append_column (table, column_ts);

  gda_ddl_creator_append_table (self->creator, table);
  res = gda_ddl_table_create (table, self->cnc, TRUE, &error);
  if (!res) {
    g_warning ("Error Creating table: %s", error->message);
  }
  g_assert_true (res);
}

static void create_companies_table (CheckDdlObject *self) {
  GError *error = NULL;
  gboolean res = FALSE;
  GdaDdlTable *table = NULL;
  GdaDdlColumn *column_id;
  GdaDdlColumn *column_name;
  GdaDdlColumn *column_ctime;
  GdaDdlColumn *column_ts;
  GdaDdlColumn *column_state;

  if (self->cnc == NULL) {
    return;
  }

  g_assert_nonnull (self->creator);

  table = gda_ddl_table_new ();
  gda_ddl_base_set_name (GDA_DDL_BASE(table), "companies");

  column_id = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_id,"id");
  gda_ddl_column_set_type (column_id, G_TYPE_INT);
  gda_ddl_column_set_autoinc (column_id, TRUE);
  gda_ddl_column_set_pkey (column_id, TRUE);

  gda_ddl_table_append_column (table, column_id);

  column_name = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_name, "name");
  gda_ddl_column_set_type (column_name, G_TYPE_STRING);
  gda_ddl_column_set_size (column_name, 50);

  gda_ddl_table_append_column (table, column_name);

  column_ctime = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_ctime, "create_time");
  gda_ddl_column_set_type (column_ctime, GDA_TYPE_TIME);

  gda_ddl_table_append_column (table, column_ctime);

  column_state = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_state, "state");
  gda_ddl_column_set_type (column_state, G_TYPE_BOOLEAN);

  gda_ddl_table_append_column (table,column_state);

  column_ts = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_ts, "mytimestamp");
  gda_ddl_column_set_type (column_ts, G_TYPE_DATE_TIME);

  gda_ddl_table_append_column (table, column_ts);

  gda_ddl_creator_append_table (self->creator, table);
  res = gda_ddl_table_create (table, self->cnc, TRUE, &error);
  if (!res) {
    g_warning ("Error Creating table: %s", error->message);
  }
  g_assert_true (res);
}


static void create_countries_table (CheckDdlObject *self) {
  GError *error = NULL;
  gboolean res = FALSE;
  GdaDdlTable *table = NULL;
  GdaDdlColumn *column_id;
  GdaDdlColumn *column_name;
  GdaDdlColumn *column_ctime;
  GdaDdlColumn *column_ts;
  GdaDdlColumn *column_state;

  if (self->cnc == NULL) {
    return;
  }

  g_assert_nonnull (self->creator);

  table = gda_ddl_table_new ();
  gda_ddl_base_set_name (GDA_DDL_BASE(table), "countries");

  column_id = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_id,"id");
  gda_ddl_column_set_type (column_id, G_TYPE_INT);
  gda_ddl_column_set_autoinc (column_id, TRUE);
  gda_ddl_column_set_pkey (column_id, TRUE);

  gda_ddl_table_append_column (table, column_id);

  column_name = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_name, "name");
  gda_ddl_column_set_type (column_name, G_TYPE_STRING);
  gda_ddl_column_set_size (column_name, 50);

  gda_ddl_table_append_column (table, column_name);

  column_ctime = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_ctime, "create_time");
  gda_ddl_column_set_type (column_ctime, GDA_TYPE_TIME);

  gda_ddl_table_append_column (table, column_ctime);

  column_state = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_state, "state");
  gda_ddl_column_set_type (column_state, G_TYPE_BOOLEAN);

  gda_ddl_table_append_column (table,column_state);

  column_ts = gda_ddl_column_new ();
  gda_ddl_column_set_name (column_ts, "mytimestamp");
  gda_ddl_column_set_type (column_ts, G_TYPE_DATE_TIME);

  gda_ddl_table_append_column (table, column_ts);

  gda_ddl_creator_append_table (self->creator, table);
  res = gda_ddl_table_create (table, self->cnc, TRUE, &error);
  if (!res) {
    g_warning ("Error Creating table: %s", error->message);
  }
  g_assert_true (res);
}

static void
test_ddl_creator_start (CheckDdlObject *self,
                     gconstpointer user_data)
{
  gda_init();
  self->xmlfile = NULL;
  self->creator = NULL;
  self->cnc = NULL;
  self->started_db = FALSE;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "ddl",
                                   "ddl-db.xml",NULL);

  g_assert_nonnull (self->xmlfile);

#ifdef CI_ENVIRONMENT
  const gchar *cnc_string = "DB_NAME=test;HOST=postgres;USERNAME=test;PASSWORD=test1";
#else
  const gchar *cnc_string = "DB_NAME=test;HOST=localhost;USERNAME=test;PASSWORD=test1";
#endif

  self->cnc = gda_connection_new_from_string("Postgresql",
                                             cnc_string,
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             NULL);
  if (self->cnc == NULL) {
    g_print ("Postgres test not run, please setup a database 'test', owned by 'test' role with password 'test1' at localhost\n");
    g_print ("Test Skip.\n");
    return;
  }

  g_assert_nonnull (self->cnc);

  gboolean openres = gda_connection_open(self->cnc, NULL);
  g_assert_true (openres);

  self->creator = gda_connection_create_ddl_creator (self->cnc);

  g_assert_nonnull (self->creator);

  /* Create DataBase structure */
  create_users_table (self);
  create_companies_table (self);
  create_countries_table (self);
}

static void
test_ddl_creator_finish (CheckDdlObject *self,
                      gconstpointer user_data)
{
  gda_connection_close(self->cnc,NULL);
  g_free (self->xmlfile);
  g_object_unref (self->creator);
  g_object_unref (self->cnc);
}


static void
test_tables (CheckDdlObject *self,
             gconstpointer user_data)
{
  g_message ("Testing Tables...");
  if (self->cnc == NULL) {
      return;
  }
  GList *tables = gda_ddl_creator_get_tables (self->creator);
  g_assert (tables != NULL);
  g_assert (g_list_length (tables) != 0);
  g_assert (g_list_length (tables) == 3);
  GList *lt = NULL;
  for (lt = tables; lt; lt = lt->next) {
    GdaDdlTable *table = (GdaDdlTable *) lt->data;
    g_message ("Table found: %s", gda_ddl_base_get_full_name (GDA_DDL_BASE (table)));
  }
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-ddl-postgresql/meta-tables",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_tables,
              test_ddl_creator_finish);

  return g_test_run();
}
