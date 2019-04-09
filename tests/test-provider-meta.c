/* test-provider-meta.c
 *
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
#include <libgda/gda-provider-meta.h>

typedef struct {
  GdaConnection *cnn;
} CheckProviderMeta;

void test_iface (void);

gint
main (G_GNUC_UNUSED gint   argc, G_GNUC_UNUSED gchar *argv[])
{
  gchar *duri, *strc, *cstr, *db, *fpath;
  const gchar *pdir;
  GdaConnection *cnn;
  GError *error = NULL;
  GFile *dir, *dbf;
  gint num = -1;

  setlocale (LC_ALL,"");

  gda_init ();
  g_assert_not_reached ();


  gchar **penv = g_get_environ ();
  pdir = g_environ_getenv (penv, "GDA_TOP_BUILD_DIR");
  GRand *rand = g_rand_new ();
  dir = g_file_new_for_path (pdir);
  if (!g_file_query_exists (dir, NULL)) {
    g_assert_not_reached ();
  }

  duri = g_file_get_uri (dir);
  num = g_rand_int (rand);
  db = g_strdup_printf ("test%d", num);
  strc = g_strdup_printf ("%s/tests/test%d.db", duri, num);
  g_free (duri);
  dbf = g_file_new_for_uri (strc);
  g_free (strc);
  while (g_file_query_exists (dbf, NULL)) {
    g_free (db);
    num = g_rand_int (rand);
    db = g_strdup_printf ("test%d", num);
    strc = g_strdup_printf ("%s/tests/meta-store/test%d.db", duri, num);
    g_object_unref (dbf);
    dbf = g_file_new_for_uri (db);
  }
  g_free (duri);
  fpath = g_strdup_printf ("%s/tests", pdir);
  cstr = g_strdup_printf ("DB_NAME=%s;DB_DIR=%s", db, fpath);

  g_message ("Initializing Connection");
  cnn = gda_connection_open_from_string ("SQLite",
                                         cstr,
                                         NULL,
                                         GDA_CONNECTION_OPTIONS_NONE,
                                         &error);

  g_free (cstr);
  g_free (fpath);
  g_free (db);
  g_object_unref (dbf);

  if (error) {
    g_print ("Error creating/opening database: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  /* Initializing DB */
  gint rows = 0;
  gda_connection_execute_non_select_command (cnn, "CREATE TABLE countries (id INTEGER PRIMARY KEY, name TEXT)", &error);
  if (error) {
    g_print ("Error creating table users: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  gda_connection_execute_non_select_command (cnn, "CREATE TABLE cities (id INTEGER PRIMARY KEY, name TEXT, city INTEGER REFERENCES cities(id))", &error);
  if (error) {
    g_print ("Error creating table users: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  gda_connection_execute_non_select_command (cnn, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, city INTEGER REFERENCES cities(id)", &error);
  if (error) {
    g_print ("Error creating table users: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  rows = gda_connection_execute_non_select_command (cnn, "INSERT INTO users (name) VALUES (\'user1\');", &error);
  g_assert (rows == 1);
  if (error) {
    g_print ("Error inserting data into table users: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  rows = gda_connection_execute_non_select_command (cnn, "INSERT INTO users (name) VALUES (\'user2\');", &error);
  g_assert (rows == 1);
  if (error) {
    g_print ("Error inserting data into table users: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  rows = gda_connection_execute_non_select_command (cnn, "INSERT INTO users (name) VALUES (\'user3\');", &error);
  g_assert (rows == 1);
  if (error) {
    g_print ("Error inserting data into table users: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  /* Testing */

  GdaProviderMeta *prov = GDA_PROVIDER_META (gda_connection_get_provider (cnn));
  GdaDataModel *model = NULL;

  g_assert (prov != NULL);
  g_assert (GDA_IS_PROVIDER_META (prov));
  model = gda_provider_meta_tables (prov, &error);
  if (model == NULL) {
    g_message ("Error: %s", error->message);
    g_assert_not_reached ();
  }
  g_assert (gda_data_model_get_n_rows (model) == 3);
  g_message ("Tables:\n%s", gda_data_model_dump_as_string (model));
  g_object_unref (cnn);
}

