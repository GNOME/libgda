/* check-ddl-creator.c
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
#include <libgda/gda-ddl-creator.h>
#include <libgda/gda-ddl-base.h>
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
    GdaDdlCreator *creator;
    gchar *xmlfile;
    GdaConnection *cnc;
    GFile *file;
} CheckDdlObject;

typedef struct {
    GdaDdlCreator *creator;
    GdaConnection *cnc;
    GdaServerProvider *provider;
} DdlCreatorCnc;

static void
test_ddl_creator_start (CheckDdlObject *self,
                     gconstpointer user_data)
{
  gda_init();
  self->xmlfile = NULL;
  self->creator = NULL;
  self->cnc = NULL;
  self->file = NULL;

  const gchar *topsrcdir = g_getenv ("GDA_TOP_SRC_DIR");

  g_print ("ENV: %s\n",topsrcdir);
  g_assert_nonnull (topsrcdir);

  self->xmlfile = g_build_filename(topsrcdir,
                                   "tests",
                                   "ddl",
                                   "ddl-db.xml",NULL);

  g_assert_nonnull (self->xmlfile);

  self->creator = gda_ddl_creator_new();
  
  self->cnc = gda_connection_new_from_string("SQLite",
                                             "DB_DIR=.;DB_NAME=ddl_test",
                                             NULL,
                                             GDA_CONNECTION_OPTIONS_NONE,
                                             NULL);

  g_assert_nonnull (self->cnc);

  gboolean openres = gda_connection_open(self->cnc,NULL);
  g_assert_true (openres);

  self->file = g_file_new_for_path (self->xmlfile); 
  g_print ("GFile is %s\n",g_file_get_path(self->file));
}

static void
test_ddl_creator_start_db (DdlCreatorCnc *self,
                           gconstpointer user_data)
{
  gda_init();

  self->cnc = NULL;
  self->creator = NULL;
  self->creator = gda_ddl_creator_new ();
 
  self->cnc = gda_connection_new_from_string ("SQLite",
                                              "DB_DIR=.;DB_NAME=ddl_types",
                                              NULL,
                                              GDA_CONNECTION_OPTIONS_NONE,
                                              NULL);
  g_assert_nonnull (self->cnc);

  gboolean open_res = gda_connection_open (self->cnc, NULL);

  g_assert_true (open_res);

  GdaDdlTable *table = gda_ddl_table_new ();
  gda_ddl_base_set_name (GDA_DDL_BASE(table),"dntypes");

  GdaDdlColumn *column = gda_ddl_column_new ();
  gda_ddl_column_set_name (column,"id");
  gda_ddl_column_set_type (column, G_TYPE_INT);
  gda_ddl_column_set_autoinc (column, TRUE);
  gda_ddl_column_set_pkey (column, TRUE);

  gda_ddl_table_append_column (table,column);

  g_clear_object (&column);

 
  column = gda_ddl_column_new ();
  gda_ddl_column_set_name (column,"name");
  gda_ddl_column_set_type (column, G_TYPE_STRING);
  gda_ddl_column_set_size (column, 50);

  gda_ddl_table_append_column (table,column);

  g_clear_object (&column);

  column = gda_ddl_column_new ();
  gda_ddl_column_set_name (column,"create_time");
  gda_ddl_column_set_type (column, GDA_TYPE_TIME);

  gda_ddl_table_append_column (table,column);

  g_clear_object (&column);

  column = gda_ddl_column_new ();
  gda_ddl_column_set_name (column,"mytimestamp");
  gda_ddl_column_set_type (column, G_TYPE_DATE_TIME);

  gda_ddl_table_append_column (table,column);

  g_clear_object (&column);

  gda_ddl_creator_append_table (self->creator, table);
  g_object_unref (table);

  open_res = gda_ddl_creator_perform_operation (self->creator,self->cnc,NULL);

  g_assert_true (open_res);

}

static void
test_ddl_creator_finish (CheckDdlObject *self,
                      gconstpointer user_data)
{
  gda_connection_close(self->cnc,NULL);
  g_free (self->xmlfile);
  g_object_unref (self->file);
  g_object_unref (self->creator);
  g_object_unref (self->cnc);
}

static void
test_ddl_creator_finish_db (DdlCreatorCnc *self,
                            gconstpointer user_data)
{
  GdaServerOperation *op = NULL;

  op = gda_server_provider_create_operation (self->provider,
                                             self->cnc,
                                             GDA_SERVER_OPERATION_DROP_TABLE,
                                             NULL,
                                             NULL);

  gboolean open_res;

  open_res = gda_server_operation_set_value_at (op,"dntypes",NULL,"/TABLE_DESC_P/TABLE_NAME");
  g_assert_true (open_res);

  /*open_res = gda_server_provider_perform_operation (self->provider,self->cnc,op,NULL);*/
  g_assert_true (open_res);

  g_assert_nonnull (op);

  g_object_unref (op);

  gda_connection_close(self->cnc,NULL);
  g_object_unref (self->creator);
  g_object_unref (self->cnc);
}

static void
test_ddl_creator_parse_xml_path (CheckDdlObject *self,
                                gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
}

static void
test_ddl_creator_parse_xml_file (CheckDdlObject *self,
                                gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file (self->creator,self->file,NULL);
  g_assert_true (res);
}

static void
test_ddl_creator_validate_xml (CheckDdlObject *self,
                              gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_validate_file_from_path (self->xmlfile,NULL);
  g_assert_true (res);
}

static void
test_ddl_creator_create_db (CheckDdlObject *self,
                           gconstpointer user_data)
{
  gboolean res = gda_ddl_creator_parse_file_from_path(self->creator,
                                                      self->xmlfile,
                                                      NULL);

  g_assert_true (res);
 
  res = gda_ddl_creator_write_to_path (self->creator,
                                       "ddl-test-out.xml",
                                             NULL);

  g_assert_true (res);

  GError *error = NULL;
  gboolean resop = gda_ddl_creator_perform_operation(self->creator,
                                                     self->cnc,
                                                     &error);  

  if (!resop)
    g_print ("myerr: %s\n",error && error->message ? error->message : "No default");

  g_assert_true (resop);
}

static void
test_ddl_creator_parse_cnc (DdlCreatorCnc *self,
                            gconstpointer user_data)
{
  gboolean open_res;

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
	gint i, ncols;
	g_print ("=== Data Model Dump ===\n");
	ncols = gda_data_model_get_n_columns (model);
	for (i = 0; i < ncols; i++) {
		GdaColumn *col;
		col = gda_data_model_describe_column (model, i);
		if (!col)
			g_print ("Missing column %d\n", i);
		g_print ("Column %d: ptr=>%p type=>%s dbms_type=%s\n", i, col, 
             g_type_name (gda_column_get_g_type (col)), gda_column_get_dbms_type (col));
		g_print ("2Column %d: type=>%s\n", i,  
             gda_g_type_to_string (gda_column_get_g_type (col)));
	}
	gda_data_model_dump (model, stdout);

  open_res = gda_ddl_creator_parse_cnc (self->creator,self->cnc,NULL);

  const GList *tables = gda_ddl_creator_get_tables (self->creator);
  g_assert_nonnull (tables);
  gint raw = 0;
  gint column_count = 0;

  for (GList *it = tables; it; it = it->next)
    {
      g_print("For table %s\n",gda_ddl_base_get_name (GDA_DDL_BASE(it->data)));
      const GList *columns = gda_ddl_table_get_columns (GDA_DDL_TABLE(it->data));
      g_assert_nonnull (columns);

      column_count = 0;

      for (GList *jt = columns;jt;jt=jt->next)
        {
          GValue *value = NULL;
          GdaDdlColumn *column = GDA_DDL_COLUMN (jt->data);
          GType column_type = gda_ddl_column_get_gtype (column);
          if (column_type == G_TYPE_NONE)
            g_print("Column type is NULL\n");
          g_print ("Column type from DDL: %s\n",g_type_name(column_type));
          
          if (!g_strcmp0 ("id",gda_ddl_column_get_name (column))) 
            {
              const GValue *value_int = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           G_TYPE_INT,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value_int);

              GType ggtype = G_VALUE_TYPE(value_int);

              g_print ("for ID type is %s\n",g_type_name (ggtype));
              
            }
          
          if (!g_strcmp0 ("name",gda_ddl_column_get_name (column))) 
            {
              const GValue *value_int = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           G_TYPE_STRING,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value_int);

              GType ggtype = G_VALUE_TYPE(value_int);

              g_print ("for NAME type is %s\n",g_type_name (ggtype));
              
            }

          if (!g_strcmp0 ("state",gda_ddl_column_get_name (column))) 
            {
              const GValue *value_int = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           G_TYPE_BOOLEAN,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value_int);

              GType ggtype = G_VALUE_TYPE(value_int);

              g_print ("for STATE type is %s\n",g_type_name (ggtype));
              
            }

          if (!g_strcmp0 ("create_time",gda_ddl_column_get_name (column))) 
            {
              const GValue *value_int = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           GDA_TYPE_TIME,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value_int);

              GType ggtype = G_VALUE_TYPE(value_int);

              g_print ("for created_time type is %s\n",g_type_name (ggtype));
              
            }

          if (!g_strcmp0 ("mytimestamp",gda_ddl_column_get_name (column))) 
            {
              const GValue *value_int = gda_data_model_get_typed_value_at (model,
                                                                           column_count++,
                                                                           raw,
                                                                           G_TYPE_DATE_TIME,
                                                                           FALSE,
                                                                           NULL);

              g_assert_nonnull (value_int);

              GType ggtype = G_VALUE_TYPE(value_int);
              GDateTime *dt = (GDateTime*)g_value_get_boxed (value_int);

              g_print ("for dt type is %s\n",g_type_name (ggtype));
              g_print ("YYYY-MM-DD: %d-%d-%d\n",g_date_time_get_year (dt),
                                                g_date_time_get_month (dt),
                                                g_date_time_get_day_of_month (dt));
               
            }
        }
      raw++;
    }




  g_assert_true (open_res);

  gda_value_free (value_name);
  gda_value_free (value_state);
  gda_value_free (value_ctime);
  gda_value_free (value_timest);
  g_object_unref (model);
}
/* static void */
/* test_ddl_creator_parse_cnc (CheckDdlObject *self, */
/*                            gconstpointer user_data) */
/* { */
/*   gboolean res = gda_ddl_creator_parse_cnc(self->creator,self->cnc,NULL); */

/*   g_assert_true (res); */
/* } */

gint
main (gint   argc,
      gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-ddl/creator-parse-file",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_parse_xml_file,
              test_ddl_creator_finish);

  g_test_add ("/test-ddl/creator-parse-path",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_parse_xml_path,
              test_ddl_creator_finish);

  g_test_add ("/test-ddl/creator-create-db",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_create_db,
              test_ddl_creator_finish);

  g_test_add ("/test-ddl/creator-validate-xml",
              CheckDdlObject,
              NULL,
              test_ddl_creator_start,
              test_ddl_creator_validate_xml,
              test_ddl_creator_finish);
  g_test_add ("/test-ddl/creator-parse-cnc",
              DdlCreatorCnc, 
              NULL,
              test_ddl_creator_start_db,
              test_ddl_creator_parse_cnc,
              test_ddl_creator_finish_db);
 
  return g_test_run();
}
