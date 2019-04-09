/* check-db-db-create.c
 *
 * Copyright 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
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
#include <libgda/gda-db-catalog.h>
#include <libgda/gda-db-base.h>
#include <sql-parser/gda-sql-parser.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
  GdaDbCatalog *catalog;
  GdaConnection *cnc;
  gchar *xmlfile;


}CheckCreatedb;

static void
test_db_db_create_start(CheckCreatedb *self,
                         G_GNUC_UNUSED gconstpointer user_data)
{
  gda_init ();
  self->catalog = NULL;
  self->cnc = NULL;
  GError *error = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "db",
                                   "check_db.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->cnc = gda_connection_new_from_string ("SQLite",
                                              "DB_DIR=.;DB_NAME=db_test_create",
                                              NULL,
                                              GDA_CONNECTION_OPTIONS_NONE,
                                              &error);
  if (self->cnc == NULL) {
    g_warning ("Error creating connection from string: %s",
               error && error->message ? error->message : "(No message was set)");

  }

  g_assert_nonnull (self->cnc);

  gboolean res = gda_connection_open (self->cnc, &error);
  if (!res) {
    g_warning ("Error openning connection: %s",
               error && error->message ? error->message : "(No message was set)");
  }
  g_assert_true (res);

  self->catalog = gda_connection_create_db_catalog (self->cnc);

  g_assert_nonnull (self->catalog);

  res = gda_db_catalog_validate_file_from_path (self->xmlfile, &error);
  if (!res) {
    g_warning ("Error validating xml file: %s",
               error && error->message ? error->message : "(No message was set)");
  }

  res = gda_db_catalog_parse_file_from_path (self->catalog,
                                              self->xmlfile,
                                              &error);
  if (!res) {
    g_warning ("Error parsing file: %s",
               error && error->message ? error->message : "(No message was set)");

  }
  
  res = gda_db_catalog_perform_operation (self->catalog,
                                           &error);
  if (!res) {
    g_warning ("Error performing operation: %s",
               error && error->message ? error->message : "(No message was set)");
  }
}

static void
test_db_db_create_finish (CheckCreatedb *self,
                           G_GNUC_UNUSED gconstpointer user_data)
{
/* Dropping all tables */
  GdaServerOperation *op = gda_connection_create_operation (self->cnc,
                                                            GDA_SERVER_OPERATION_DROP_TABLE,
                                                            NULL,
                                                            NULL);

  g_assert_true (op);

  gboolean res = gda_server_operation_set_value_at (op,"employee",NULL,"/TABLE_DESC_P/TABLE_NAME");
  g_assert_true (res);
  res = gda_connection_perform_operation (self->cnc,op,NULL);
  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,"job_site",NULL,"/TABLE_DESC_P/TABLE_NAME");
  g_assert_true (res);
  res = gda_connection_perform_operation (self->cnc,op,NULL);
  g_assert_true (res);

  g_object_unref (op);

  gda_connection_close (self->cnc, NULL);
  g_free (self->xmlfile);
  g_object_unref (self->catalog);
  g_object_unref (self->cnc);
}

static void
test_db_db_create_test1 (CheckCreatedb *self,
                          G_GNUC_UNUSED gconstpointer user_data)
{
  GValue *value_name = gda_value_new (G_TYPE_STRING);

  g_value_set_string (value_name,"John");

  GValue *value_id = gda_value_new (G_TYPE_INT);

  GError *error = NULL;

  gboolean res = gda_connection_insert_row_into_table (self->cnc,"job_site",&error,
                                                       "name",value_name,
                                                       NULL); 
  g_assert_true (res);

  g_value_set_string (value_name,"Tom");

  res = gda_connection_insert_row_into_table (self->cnc,"job_site",NULL,
                                              "name",value_name,
                                              NULL); 
  g_assert_true (res);

  g_value_set_int (value_id,2);

  res = gda_connection_insert_row_into_table (self->cnc,"employee",NULL,
                                              "job_site_id",value_id,
                                              NULL); 
  g_assert_true (res);

/* Try to insert wrong data 
 * There is no job_site record with id == 3
 */
  g_value_set_int (value_id,3);

  res = gda_connection_insert_row_into_table (self->cnc,"employee",NULL,
                                              "job_site_id",value_id,
                                              NULL); 
  g_assert_false (res);

  gda_value_free (value_id);
  gda_value_free (value_name);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-db/CheckCreatedb",
              CheckCreatedb,
              NULL,
              test_db_db_create_start,
              test_db_db_create_test1,
              test_db_db_create_finish);
 
  return g_test_run();
}

