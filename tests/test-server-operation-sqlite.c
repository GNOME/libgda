/* check-server-operation.c
 *
 * Copyright (C) 2018-2019 Pavlo Solntsev <p.sun.fun@gmail.com>
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

/* To initiate this test a variable SQLITE_SO_DDL shoule be set with value idential to the
 * connection string:
 * SQLITE_SO_DDL="DB_NAME=mydbname;DB_DIR=/some/place/with/database"
 * Two tables will be created:
 *
 * Project
 * =======================
 * id | name
 *
 * Employee
 * =======================
 * id | name | project_id
 *
 * An additional column "cost" will be added via ADD_COLUMN operation to the table Project
 *
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libgda/libgda.h>

#define PROVIDER_NAME "SQLite"

typedef struct {
    gchar *prefix;
    GdaServerOperation *op;
} QuarkSet;

typedef struct
{
  GdaConnection *cnc;
  const gchar *cnc_string;
  GdaServerProvider *provider;
  GdaServerOperation *op;
  gboolean skiptest;
} TestObjectFixture;


static void
set_val_quark_foreach_func (gchar *name, gchar *value, QuarkSet *setval)
{
	gda_server_operation_set_value_at (setval->op, value, NULL, "/%s/%s", setval->prefix, name);
}

static void
test_server_operation_start (TestObjectFixture *fixture,
                             gconstpointer user_data)
{
  fixture->cnc = NULL;
  fixture->provider = NULL;
  fixture->op = NULL;

  fixture->cnc_string = g_getenv("SQLITE_SO_DDL");

  if (!fixture->cnc_string)
    {
      g_print ("Please set SQLITE_CREATE_DB variable"
               "with DB_DIR and DB_NAME");
      g_print ("Test will not be performed\n");
      fixture->skiptest = TRUE;
      return;
    }
}


static void
test_server_operation_operations_start (TestObjectFixture *fixture,
                                        gconstpointer user_data)
{
  fixture->cnc = NULL;
  fixture->provider = NULL;
  fixture->op = NULL;

  fixture->cnc_string = g_getenv("SQLITE_SO_DDL");

  if (!fixture->cnc_string)
    {
      g_print ("Please set SQLITE_CREATE_DB variable"
               "with DB_DIR and DB_NAME");
      g_print ("Test will not be performed\n");
      fixture->skiptest = TRUE;
      return;
    }

  fixture->cnc = gda_connection_open_from_string (PROVIDER_NAME,
                                                  fixture->cnc_string,
                                                  NULL,
                                                  GDA_CONNECTION_OPTIONS_NONE,
                                                  NULL);
  g_assert_nonnull (fixture->cnc);

  fixture->provider = gda_connection_get_provider (fixture->cnc);

  g_assert_nonnull (fixture->provider);
}

static void
test_server_operation_finish (TestObjectFixture *fixture,
                             gconstpointer user_data)
{
  gboolean res = gda_connection_close (fixture->cnc, NULL);

  g_assert_true (res);

  g_object_unref (fixture->cnc);
  g_object_unref (fixture->op);
}

static void
test_server_operation_operations_finish (TestObjectFixture *fixture,
                                         gconstpointer user_data)
{
  gboolean res = gda_connection_close (fixture->cnc, NULL);

  g_assert_true (res);

  g_object_unref (fixture->op);
  g_object_unref (fixture->cnc);
}
static void
test_server_operation_create_db (TestObjectFixture *fixture,
                                 gconstpointer user_data)
{
  fixture->cnc = gda_connection_open_from_string (PROVIDER_NAME, fixture->cnc_string, NULL,
                                                  GDA_CONNECTION_OPTIONS_NONE, NULL);
  g_assert_nonnull (fixture->cnc);

  fixture->provider = gda_connection_get_provider (fixture->cnc);

  g_assert_nonnull (fixture->provider);

  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_CREATE_DB,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

	GdaQuarkList *db_quark_list = NULL;
  db_quark_list = gda_quark_list_new_from_string (fixture->cnc_string);

  QuarkSet setval;

  setval.prefix = g_strdup ("DB_DEF_P");
  setval.op = fixture->op;

  gda_quark_list_foreach (db_quark_list, (GHFunc) set_val_quark_foreach_func, &setval);

  g_free (setval.prefix);

  gboolean res = gda_server_provider_perform_operation (fixture->provider,
                                                        fixture->cnc,
                                                        fixture->op,
                                                        NULL);

  g_assert_true (res);
}

static void
test_server_operation_operations (TestObjectFixture *fixture,
                                  gconstpointer user_data)
{
  if (fixture->skiptest)
    return;

/* CREATE_TABLE operation */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_CREATE_TABLE,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

/* We will create two tables: Employee and Project. One table will contain foreigh key that points
 * to the column from the second one.
 *
 */

  gboolean res = FALSE;
  /* Define table name */
  res = gda_server_operation_set_value_at (fixture->op,
                                           "Project",
                                           NULL,
                                           "/TABLE_DEF_P/TABLE_NAME");

  g_assert_true (res);

  /* Table will not be temporary */
  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/TABLE_DEF_P/TABLE_TEMP");

  g_assert_true (res);

  /* Create table if table doesn't exist yet */
  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
  g_assert_true (res);

  /* Define column id*/
  gint column_order = 0;
  res = gda_server_operation_set_value_at (fixture->op,
                                           "id",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "gint",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  g_assert_true (res);

  /* Define column name */
  column_order++;
  res = gda_server_operation_set_value_at (fixture->op,
                                           "name",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "gchararray",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "50",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_SIZE/%d",
                                           column_order);

  g_assert_true (res);
  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "Default_name",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_DEFAULT/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);

  g_clear_object (&fixture->op);

  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_CREATE_TABLE,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

/* We will create two tables: Employee and Project. One table will contain foreigh key that points
 * to the column from the second one.
 *
 */

  res = FALSE;
  /* Define table name */
  res = gda_server_operation_set_value_at (fixture->op,
                                           "Employee",
                                           NULL,
                                           "/TABLE_DEF_P/TABLE_NAME");

  g_assert_true (res);

  /* Table will not be temporary */
  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/TABLE_DEF_P/TABLE_TEMP");

  g_assert_true (res);

  /* Create table if table doesn't exist yet */
  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
  g_assert_true (res);

  /* Define column id */
  column_order = 0;
  res = gda_server_operation_set_value_at (fixture->op,
                                           "id",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "gint",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  g_assert_true (res);

  /* Define column name */
  column_order++;
  res = gda_server_operation_set_value_at (fixture->op,
                                           "name",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "gchararray",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "50",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_SIZE/%d",
                                           column_order);

  g_assert_true (res);
  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "Default_name",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_DEFAULT/%d",
                                           column_order);

  g_assert_true (res);

  /* Define column project_id */
  column_order++;
  res = gda_server_operation_set_value_at (fixture->op,
                                           "project_id",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "gint",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "Project",
                                           NULL,
                                           "/FKEY_S/%d/FKEY_REF_TABLE",
                                           0);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "RESTRICT",
                                           NULL,
                                           "/FKEY_S/%d/FKEY_ONDELETE",
                                           0);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "RESTRICT",
                                           NULL,
                                           "/FKEY_S/%d/FKEY_ONUPDATE",
                                           0);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "project_id",
                                           NULL,
                                           "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d",
                                           0,0);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "id",
                                           NULL,
                                           "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
                                           0,0);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END of CREATE_TABLE operation */

  g_clear_object (&fixture->op);

  /* Start of ADD_COLUMN operation */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_ADD_COLUMN,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "Project",
                                           NULL,
                                           "/COLUMN_DEF_P/TABLE_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "cost",
                                           NULL,
                                           "/COLUMN_DEF_P/COLUMN_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "gfloat",
                                           NULL,
                                           "/COLUMN_DEF_P/COLUMN_TYPE");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "5",
                                           NULL,
                                           "/COLUMN_DEF_P/COLUMN_SIZE");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "2",
                                           NULL,
                                           "/COLUMN_DEF_P/COLUMN_SCALE");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/COLUMN_DEF_P/COLUMN_NNUL");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END OF ADD_COLUMN operation */

  g_clear_object (&fixture->op);

  /* START RENAME_TABLE OPERATION */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_RENAME_TABLE,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "Employee",
                                           NULL,
                                           "/TABLE_DESC_P/TABLE_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "NewEmployee",
                                           NULL,
                                           "/TABLE_DESC_P/TABLE_NEW_NAME");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);

  /* END RENAME_TABLE OPERATION */
  g_clear_object (&fixture->op);

  /* START CREATE_VIEW OPERATION */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_CREATE_VIEW,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "MyView",
                                           NULL,
                                           "/VIEW_DEF_P/VIEW_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "FALSE",
                                           NULL,
                                           "/VIEW_DEF_P/VIEW_TEMP");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/VIEW_DEF_P/VIEW_IFNOTEXISTS");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "SELECT name, project_id FROM Employee",
                                           NULL,
                                           "/VIEW_DEF_P/VIEW_DEF");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END CREATE_VIEEW OPERATION */

  g_clear_object (&fixture->op);

  /* START DROP_VIEEW OPERATION */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_DROP_VIEW,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "MyView",
                                           NULL,
                                           "/VIEW_DESC_P/VIEW_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/VIEW_DEF_P/VIEW_IFNOTEXISTS");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END CREATE_VIEEW OPERATION */

  g_clear_object (&fixture->op);

  /* START CREATE_INDEX OPERATION */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_CREATE_INDEX,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "UNIQUE",
                                           NULL,
                                           "/INDEX_DEF_P/INDEX_TYPE");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "MyIndex",
                                           NULL,
                                           "/INDEX_DEF_P/INDEX_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "NewEmployee",
                                           NULL,
                                           "/INDEX_DEF_P/INDEX_ON_TABLE");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/INDEX_DEF_P/INDEX_IFNOTEXISTS");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "name",
                                           NULL,
                                           "/INDEX_FIELDS_S/%d/INDEX_FIELD",
                                           0);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "NOCASE",
                                           NULL,
                                           "/INDEX_FIELD_S/%d/INDEX_COLLATE",
                                           0);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "ASC",
                                           NULL,
                                           "/INDEX_FIELD_S/%d/INDEX_SORT_ORDER",
                                           0);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END CREATE_INDEX OPERATION */
  g_clear_object (&fixture->op);

  /* START DROP_INDEX OPERATION */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_DROP_INDEX,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "MyIndex",
                                           NULL,
                                           "/INDEX_DESC_P/INDEX_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/INDEX_DESC_P/INDEX_IFEXISTS");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END DROP_INDEX OPERATION */

  g_clear_object(&fixture->op);

  /* START DROP_TABLE OPERATION */
  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_DROP_TABLE,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "NewEmployee",
                                           NULL,
                                           "/TABLE_DESC_P/TABLE_NAME");

  g_assert_true (res);

  res = gda_server_operation_set_value_at (fixture->op,
                                           "TRUE",
                                           NULL,
                                           "/TABLE_DESC_P/TABLE_IFEXISTS");

  g_assert_true (res);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
  /* END DROP_TABLE OPERATION */
}

void
test_server_operation_drop_db (TestObjectFixture *fixture,
                               gconstpointer user_data)
{
  fixture->cnc = gda_connection_open_from_string (PROVIDER_NAME, fixture->cnc_string, NULL,
                                                  GDA_CONNECTION_OPTIONS_NONE, NULL);
  g_assert_nonnull (fixture->cnc);

  fixture->provider = gda_connection_get_provider (fixture->cnc);

  g_assert_nonnull (fixture->provider);

  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_CREATE_DB,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

	GdaQuarkList *db_quark_list = NULL;
  db_quark_list = gda_quark_list_new_from_string (fixture->cnc_string);

  QuarkSet setval;

  setval.prefix = g_strdup ("DB_DEF_P");
  setval.op = fixture->op;

  gda_quark_list_foreach (db_quark_list, (GHFunc) set_val_quark_foreach_func, &setval);

  g_free (setval.prefix);

  gboolean res = gda_server_provider_perform_operation (fixture->provider,
                                                        fixture->cnc,
                                                        fixture->op,
                                                        NULL);

  g_assert_true (res);

  /* We need to clean everything after CREATE_DB operation above */
  gda_connection_close (fixture->cnc, NULL);
  g_clear_object (&fixture->cnc);
  g_clear_object (&fixture->op);
  /* End of  clean up block */

  fixture->cnc = gda_connection_open_from_string (PROVIDER_NAME, fixture->cnc_string, NULL,
                                                  GDA_CONNECTION_OPTIONS_NONE, NULL);
  g_assert_nonnull (fixture->cnc);

  fixture->op = gda_server_provider_create_operation (fixture->provider,
                                                      fixture->cnc,
                                                      GDA_SERVER_OPERATION_DROP_DB,
                                                      NULL,
                                                      NULL);

  g_assert_nonnull (fixture->op);

	db_quark_list = NULL;
  db_quark_list = gda_quark_list_new_from_string (fixture->cnc_string);

  setval.prefix = g_strdup ("DB_DESC_P"); // Prefix for op object
  setval.op = fixture->op;

  gda_quark_list_foreach (db_quark_list, (GHFunc) set_val_quark_foreach_func, &setval);

  g_free (setval.prefix);

  res = gda_server_provider_perform_operation (fixture->provider,
                                               fixture->cnc,
                                               fixture->op,
                                               NULL);

  g_assert_true (res);
}

gint
main(gint argc, gchar *argv[])
{
  setlocale (LC_ALL,"");

  g_test_init (&argc,&argv,NULL);

  g_test_add ("/test-server-operation-sqlite/create-db",
              TestObjectFixture,
              NULL,
              test_server_operation_start,
              test_server_operation_create_db,
              test_server_operation_finish);

  g_test_add ("/test-server-operation-sqlite/drop-db",
              TestObjectFixture,
              NULL,
              test_server_operation_start,
              test_server_operation_drop_db,
              test_server_operation_finish);

  g_test_add ("/test-server-operation-sqlite/create-table",
              TestObjectFixture,
              NULL,
              test_server_operation_operations_start,
              test_server_operation_operations,
              test_server_operation_operations_finish);

  return g_test_run();
}

