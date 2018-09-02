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

void init_data (CheckProviderMeta *data, gconstpointer user_data);
void finish_data (CheckProviderMeta *data, gconstpointer user_data);
void test_iface (CheckProviderMeta *data, gconstpointer user_data);

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  gda_init ();


  g_test_init (&argc,&argv,NULL);

  g_test_add ("/gda/provider-meta/iface",
              CheckProviderMeta,
              NULL,
              init_data,
              test_iface,
              finish_data);

  return g_test_run();
}

void
init_data (CheckProviderMeta *data, gconstpointer user_data) {
  GString *strc, *cstr;
  GdaConnection *cnn;
  GError *error = NULL;
  GFile *dir, *dbf;

  dir = g_file_new_for_path (BUILD_DIR);
  g_assert (g_file_query_exists (dir, NULL));
  cstr = g_string_new ("");
  g_string_printf (cstr, "%s/iter.db", g_file_get_uri (dir));
  dbf = g_file_new_for_uri (cstr->str);
  if (g_file_query_exists (dbf, NULL)) {
    g_file_delete (dbf, NULL, &error);
    if (error) {
      g_print ("Error deleting DB file: %s", error->message != NULL ? error->message : "No detail");
      g_assert_not_reached ();
    }
  }
  g_object_unref (dbf);

  strc = g_string_new ("");
  g_string_printf (strc,"DB_DIR=%s;DB_NAME=iter", g_file_get_path (dir));

  cnn = gda_connection_open_from_string ("SQLite",
                                         strc->str,
                                         NULL,
                                         GDA_CONNECTION_OPTIONS_NONE,
                                         &error);

  g_string_free (strc, TRUE);

  if (error) {
    g_print ("Error creating/opening database: %s", error->message != NULL ? error->message : "No detail");
    g_assert_not_reached ();
  }
  g_print ("Initializing DB\n");
  gint rows = 0;
  gda_connection_execute_non_select_command (cnn, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)", &error);
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
  data->cnn = cnn;
}


void
finish_data (CheckProviderMeta *data, gconstpointer user_data) {
  g_object_unref (data->cnn);
}


void test_iface (CheckProviderMeta *data, gconstpointer user_data) {
  GdaConnection *cnn = data->cnn;
  GdaServerProvider *prov = gda_connection_get_provider (cnn);

  g_assert (prov != NULL);
  g_assert (GDA_IS_PROVIDER_META (prov));
}
