/* check-db-catalog.c
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

enum {
  COLUMN_ID = 0,
  COLUMN_NAME,
  COLUMN_STATE,
  COLUMN_TIME,
  COLUMN_TIMESTAMP
};


typedef struct {
    GdaDbCatalog *catalog;
    gchar *xmlfile;
    GdaConnection *cnc;
    GFile *file;
} CheckDbObject;

typedef struct {
    GdaDbCatalog *catalog;
    GdaConnection *cnc;
    GdaDbColumn *column_id;
    GdaDbColumn *column_name;
    GdaDbColumn *column_ctime;
    GdaDbColumn *column_ts;
    GdaDbColumn *column_state;
    GdaDbTable *table;
    GdaDbView  *view;
} DbCatalogCnc;

typedef struct {
  GdaDbCatalog *catalog;
  GdaConnection *cnc;
  GdaDbColumn *column_a;
  GdaDbColumn *column_b;
  GdaDbTable *table;
} DbCheckCatallog;

static void
test_db_catalog_start (CheckDbObject *self,
                     G_GNUC_UNUSED gconstpointer user_data)
{
  gda_init();
  self->xmlfile = NULL;
  self->catalog = NULL;
  self->cnc = NULL;
  self->file = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "db",
                                   "db.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  gchar* dbname = g_strdup_printf ("DB_DIR=.;DB_NAME=db_test_%d", g_random_int ());

  self->cnc = gda_connection_new_from_string("SQLite",
                                             dbname,
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             NULL);
  g_free (dbname);

  g_assert_nonnull (self->cnc);

  gboolean openres = gda_connection_open(self->cnc,NULL);
  g_assert_true (openres);

  self->catalog = gda_connection_create_db_catalog (self->cnc);

  g_assert_nonnull (self->catalog);

  self->file = g_file_new_for_path (self->xmlfile);
  gchar *strpath = g_file_get_path (self->file);
  g_print ("GFile is %s\n", strpath);
  g_free (strpath);
}

static void
test_db_catalog_start_db (DbCatalogCnc *self,
                           G_GNUC_UNUSED gconstpointer user_data)
{
  gda_init();

  self->cnc = NULL;
  self->catalog = NULL;

  gchar* dbname = g_strdup_printf ("DB_DIR=.;DB_NAME=db_types_%d", g_random_int ());

  g_print ("Will use DB: %s\n", dbname);

  self->cnc = gda_connection_new_from_string ("SQLite",
                                              dbname,
                                              NULL,
                                              GDA_CONNECTION_OPTIONS_NONE,
                                              NULL);
  g_free (dbname);

  g_assert_nonnull (self->cnc);

  gboolean open_res = gda_connection_open (self->cnc, NULL);

  g_assert_true (open_res);

  self->catalog = gda_connection_create_db_catalog (self->cnc);

  g_assert_nonnull (self->catalog);

  self->table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE(self->table),"dntypes");

  self->column_id = gda_db_column_new ();
  gda_db_column_set_name (self->column_id,"id");
  gda_db_column_set_type (self->column_id, G_TYPE_INT);
  gda_db_column_set_autoinc (self->column_id, TRUE);
  gda_db_column_set_pkey (self->column_id, TRUE);

  gda_db_table_append_column (self->table,self->column_id);
  g_object_unref (self->column_id);

  self->column_name = gda_db_column_new ();
  gda_db_column_set_name (self->column_name,"name");
  gda_db_column_set_type (self->column_name, G_TYPE_STRING);
  gda_db_column_set_size (self->column_name, 50);

  gda_db_table_append_column (self->table,self->column_name);
  g_object_unref (self->column_name);

  self->column_ctime = gda_db_column_new ();
  gda_db_column_set_name (self->column_ctime,"create_time");
  gda_db_column_set_type (self->column_ctime, GDA_TYPE_TIME);

  gda_db_table_append_column (self->table,self->column_ctime);
  g_object_unref (self->column_ctime);

  self->column_state = gda_db_column_new ();
  gda_db_column_set_name (self->column_state,"state");
  gda_db_column_set_type (self->column_state, G_TYPE_BOOLEAN);

  gda_db_table_append_column (self->table,self->column_state);
  g_object_unref (self->column_state);

  self->column_ts = gda_db_column_new ();
  gda_db_column_set_name (self->column_ts,"mytimestamp");
  gda_db_column_set_type (self->column_ts, G_TYPE_DATE_TIME);

  gda_db_table_append_column (self->table,self->column_ts);
  g_object_unref (self->column_ts);

  gda_db_catalog_append_table (self->catalog, self->table);
  g_object_unref (self->table);

  open_res = gda_db_catalog_perform_operation (self->catalog,NULL);

  g_assert_true (open_res);

  self->view = gda_db_view_new ();

  gda_db_base_set_name (GDA_DB_BASE (self->view), "myview");
  gda_db_view_set_defstring (self->view, "SELECT name FROM dntypes");
  gda_db_view_set_istemp (self->view, FALSE);

  open_res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (self->view),
                                        self->cnc, NULL, NULL);

  g_assert_true (open_res);
}

static void
test_db_catalog_finish (CheckDbObject *self,
                      G_GNUC_UNUSED gconstpointer user_data)
{
  gda_connection_close(self->cnc,NULL);
  g_free (self->xmlfile);
  g_object_unref (self->file);
  g_object_unref (self->catalog);
  g_object_unref (self->cnc);
}

static void
test_db_catalog_finish_db (DbCatalogCnc *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  g_object_unref (self->cnc);
  g_object_unref (self->catalog);
  g_object_unref (self->column_id);
  g_object_unref (self->column_name);
  g_object_unref (self->column_ctime);
  g_object_unref (self->column_ts);
  g_object_unref (self->view);
}

static void
test_db_catalog_parse_xml_path (CheckDbObject *self,
                                G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = gda_db_catalog_parse_file_from_path(self->catalog,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
}

static void
test_db_catalog_parse_xml_file (CheckDbObject *self,
                                G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = gda_db_catalog_parse_file (self->catalog,self->file,NULL);
  g_assert_true (res);
}

static void
test_db_catalog_validate_xml (CheckDbObject *self,
                              G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = gda_db_catalog_validate_file_from_path (self->xmlfile,NULL);
  g_assert_true (res);
}

static void
test_db_catalog_create_db (CheckDbObject *self,
                           G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean res = gda_db_catalog_parse_file_from_path(self->catalog,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);

  res = gda_db_catalog_write_to_path (self->catalog,
                                       "db-test-out.xml",
                                             NULL);

  g_assert_true (res);

  GError *error = NULL;
  gboolean resop = gda_db_catalog_perform_operation(self->catalog,
                                                     &error);

  if (!resop)
    g_print ("myerr: %s\n",error && error->message ? error->message : "No default");

  g_assert_true (resop);
}

static void
test_db_catalog_parse_cnc (DbCatalogCnc *self,
                            G_GNUC_UNUSED gconstpointer user_data)
{
  gboolean open_res;
  const gchar* dntypes = "dntypes";

  GValue *value_name,*value_state,*value_ctime,*value_timest;

  value_name = gda_value_new (G_TYPE_STRING);
  g_assert_nonnull (value_name);
  g_value_set_string (value_name,"First");

  value_state = gda_value_new (G_TYPE_BOOLEAN);
  g_assert_nonnull (value_state);
  g_value_set_boolean (value_state,TRUE);

  value_ctime = gda_value_new_time_from_timet (time(NULL));
  g_assert_nonnull (value_ctime);

  value_timest = gda_value_new_date_time_from_timet (time(NULL));
  g_assert_nonnull (value_timest);

  open_res = gda_connection_insert_row_into_table (self->cnc,"dntypes",NULL,
                                                   "name",value_name,
                                                   "state",value_state,
                                                   "create_time",value_ctime,
                                                   "mytimestamp",value_timest,
                                                   NULL);

  GdaDataModel *model = NULL;
  model = gda_connection_execute_select_command (self->cnc,"SELECT * FROM dntypes",NULL);
  g_assert_nonnull (model);

  GdaDbCatalog *catalog = gda_connection_create_db_catalog (self->cnc);
  open_res = gda_db_catalog_parse_cnc (catalog,NULL);

  g_assert_true (open_res);

  GList *tables = gda_db_catalog_get_tables (catalog);
  g_assert_nonnull (tables);
  gint raw = 0;
  gint column_count = 0;

  for (GList *it = tables; it; it = it->next)
    {
      g_assert_cmpstr (dntypes,==,gda_db_base_get_name (GDA_DB_BASE(it->data)));
      GList *columns = gda_db_table_get_columns (GDA_DB_TABLE(it->data));
      g_assert_nonnull (columns);

      column_count = 0;

      for (GList *jt = columns;jt;jt=jt->next)
        {
          GdaDbColumn *column = GDA_DB_COLUMN (jt->data);
          GType column_type = gda_db_column_get_gtype (column);
          g_assert_true (column_type != G_TYPE_NONE);

          if (!g_strcmp0 ("id",gda_db_column_get_name (column)))
            {
              GError *error = NULL;
              const GValue *value = gda_data_model_get_typed_value_at (model,
                                                                       column_count++,
                                                                       raw,
                                                                       G_TYPE_INT,
                                                                       FALSE,
                                                                       &error);

              if (!value)
                g_print ("value_int error: %s\n",error && error->message ? error->message : "No default");

              g_assert_nonnull (value);

              GType ggtype = G_VALUE_TYPE(value);

              g_print ("for ID type is %s\n",g_type_name (ggtype));

            }

          if (!g_strcmp0 ("name",gda_db_column_get_name (column)))
            {
              const GValue *value = gda_data_model_get_typed_value_at (model,
                                                                       column_count++,
                                                                       raw,
                                                                       G_TYPE_STRING,
                                                                       FALSE,
                                                                       NULL);

              g_assert_nonnull (value);

              GType ggtype = G_VALUE_TYPE(value);

              g_print ("for NAME type is %s\n",g_type_name (ggtype));

            }

          if (!g_strcmp0 ("state",gda_db_column_get_name (column)))
            {
              const GValue *value = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           G_TYPE_BOOLEAN,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value);

              g_assert_true (TRUE && g_value_get_boolean (value));
            }

          if (!g_strcmp0 ("create_time",gda_db_column_get_name (column)))
            {
              const GValue *value = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           GDA_TYPE_TIME,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value);

              GType ggtype = G_VALUE_TYPE(value);

              g_print ("for created_time type is %s\n",g_type_name (ggtype));

            }

          if (!g_strcmp0 ("mytimestamp",gda_db_column_get_name (column)))
            {
              const GValue *value = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           G_TYPE_DATE_TIME,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value);

              GType ggtype = G_VALUE_TYPE(value);
              GDateTime *dt = (GDateTime*)g_value_get_boxed (value);

              g_print ("for dt type is %s\n",g_type_name (ggtype));
              g_print ("YYYY-MM-DD: %d-%d-%d\n",g_date_time_get_year (dt),
                                                g_date_time_get_month (dt),
                                                g_date_time_get_day_of_month (dt));

            }
        }
      raw++;
    }

  g_object_unref (catalog);
  g_assert_true (open_res);

  gda_value_free (value_name);
  gda_value_free (value_state);
  gda_value_free (value_ctime);
  gda_value_free (value_timest);
  g_object_unref (model);
}

static void
test_db_catalog_constraint_start (DbCheckCatallog *self,
                                  G_GNUC_UNUSED gconstpointer user_data)
{
  self->catalog = NULL;
  self->cnc = NULL;
  self->column_a = NULL;
  self->column_b = NULL;
  self->table = NULL;

  gda_init();

  self->cnc = gda_connection_new_from_string ("SQLite",
                                              "DB_DIR=.;DB_NAME=db_constraint",
                                              NULL,
                                              GDA_CONNECTION_OPTIONS_NONE,
                                              NULL);
  g_assert_nonnull (self->cnc);

  gboolean open_res = gda_connection_open (self->cnc, NULL);

  g_assert_true (open_res);

  self->catalog = gda_connection_create_db_catalog (self->cnc);

  g_assert_nonnull (self->catalog);

  self->table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE(self->table),"tconstraint");

  self->column_a = gda_db_column_new ();
  gda_db_column_set_name (self->column_a,"columna");
  gda_db_column_set_type (self->column_a, G_TYPE_INT);
  gda_db_column_set_autoinc (self->column_a, FALSE);
  gda_db_column_set_pkey (self->column_a, TRUE);

  gda_db_table_append_column (self->table,self->column_a);
  g_object_unref (self->column_a);

  self->column_b = gda_db_column_new ();
  gda_db_column_set_name (self->column_b, "columnb");
  gda_db_column_set_type (self->column_b, G_TYPE_INT);
  gda_db_column_set_autoinc (self->column_b, FALSE);
  gda_db_column_set_pkey (self->column_b, FALSE);

  gda_db_table_append_column (self->table, self->column_b);
  g_object_unref (self->column_b);

  gda_db_table_append_constraint (self->table, "CHECK (columna = columnb)");

  gda_db_catalog_append_table (self->catalog, self->table);
  g_object_unref (self->table);

  open_res = gda_db_catalog_perform_operation (self->catalog,NULL);

  g_assert_true (open_res);

  g_object_unref (self->catalog);
}

static void
test_db_catalog_constraint_run (DbCheckCatallog *self,
                                G_GNUC_UNUSED gconstpointer user_data)
{
  GValue *val_columna = NULL;
  GValue *val_columnb = NULL;
  gboolean res;
  GError *error = NULL;

  val_columna = gda_value_new (G_TYPE_INT);
  val_columnb = gda_value_new (G_TYPE_INT);

  g_assert_nonnull (val_columna);
  g_assert_nonnull (val_columnb);

  guint32 v = g_random_int ();

  g_value_set_int (val_columna, v);
  g_value_set_int (val_columnb, v);

  res = gda_connection_insert_row_into_table (self->cnc, "tconstraint", &error,
                                              "columna", val_columna,
                                              "columnb", val_columnb,
                                              NULL);
  if (error != NULL) {
    g_print ("Error: %s",
              error->message != NULL ? error->message : "No detail");
    g_clear_error (&error);
  }

  /* Two column must have the same values as we restricted. res is true. */
  g_assert_true (res);

  g_value_set_int (val_columna, 1);
  g_value_set_int (val_columnb, 2);

  res = gda_connection_insert_row_into_table (self->cnc, "tconstraint", NULL,
                                              "columna", val_columna,
                                              "columnb", val_columnb,
                                              NULL);

  /* Two column must have the same values as we restricted. res should be false, since
   * we are inserting different numbers.
   */
  g_assert_false (res);

  gda_value_free (val_columna);
  gda_value_free (val_columnb);
}

static void
test_db_catalog_constraint_finish (DbCheckCatallog *self,
                                   G_GNUC_UNUSED gconstpointer user_data)
{
  gda_connection_close (self->cnc, NULL);
}

static void
test_db_catalog_get_objects (DbCatalogCnc *self,
                             G_GNUC_UNUSED gconstpointer user_data)
{
  GdaDbTable *copy_table;
  GdaDbView *copy_view;
  GError *error = NULL;
  gboolean res;
  const gchar *name1;
  const gchar *name2;
  /* Creating a view to work with */

  /* view has been created */

  res = gda_db_catalog_parse_cnc (self->catalog, &error);

  g_assert_true (res);
  g_assert_null (error);

  copy_table = gda_db_catalog_get_table (self->catalog, NULL, NULL,
	  gda_db_base_get_name (GDA_DB_BASE (self->table)));

  g_assert_nonnull (copy_table);

  name1 = gda_db_base_get_name (GDA_DB_BASE (copy_table));
  name2 = gda_db_base_get_name (GDA_DB_BASE (self->table));

  g_assert_cmpstr (name1, ==, name2);

  const gchar *view_name = gda_db_base_get_name (GDA_DB_BASE (self->view));

  copy_view = gda_db_catalog_get_view (self->catalog, NULL, NULL, view_name);

  g_assert_nonnull (copy_view);

  name1 = gda_db_base_get_name (GDA_DB_BASE (copy_view));
  name2 = gda_db_base_get_name (GDA_DB_BASE (self->view));

  g_assert_cmpstr (name1, ==, name2);
}

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-db/catalog-parse-file",
              CheckDbObject,
              NULL,
              test_db_catalog_start,
              test_db_catalog_parse_xml_file,
              test_db_catalog_finish);

  g_test_add ("/test-db/catalog-parse-path",
              CheckDbObject,
              NULL,
              test_db_catalog_start,
              test_db_catalog_parse_xml_path,
              test_db_catalog_finish);

  g_test_add ("/test-db/catalog-create-db",
              CheckDbObject,
              NULL,
              test_db_catalog_start,
              test_db_catalog_create_db,
              test_db_catalog_finish);

  g_test_add ("/test-db/catalog-validate-xml",
              CheckDbObject,
              NULL,
              test_db_catalog_start,
              test_db_catalog_validate_xml,
              test_db_catalog_finish);
  g_test_add ("/test-db/catalog-parse-cnc",
              DbCatalogCnc,
              NULL,
              test_db_catalog_start_db,
              test_db_catalog_parse_cnc,
              test_db_catalog_finish_db);

  g_test_add ("/test-db/catalog-constraint",
              DbCheckCatallog,
              NULL,
              test_db_catalog_constraint_start,
              test_db_catalog_constraint_run,
              test_db_catalog_constraint_finish);

  g_test_add ("/test-db/catalog-get-objects",
              DbCatalogCnc,
              NULL,
              test_db_catalog_start_db,
              test_db_catalog_get_objects,
              test_db_catalog_finish_db);

  return g_test_run();
}
