/* check-server-operation-postgresql.c
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

/*
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
 * Employee table will be renamed to NewEmployee
 *
 */
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libgda/libgda.h>
/*#include <stdlib.h>*/
#include "test-cnc-utils.h"

#define PROVIDER_NAME "PostgreSQL"
#define PROVIDER_DB_CREATE_PARAMS "POSTGRESQL_DBCREATE_PARAMS"
#define PROVIDER_CNC_PARAMS "POSTGRESQL_CNC_PARAMS"

#define GDA_PGSQL_ERROR_HANDLE(e) (g_print("Error: %s: %s\n", G_STRLOC, e && e->message ? e->message : "No default"));

typedef struct
{
  GdaConnection *cnc;
  GdaServerProvider *provider;
  gchar *dbname;
  GdaQuarkList *quark_list;
} TestObjectFixture;

static void
test_server_operation_start (TestObjectFixture *fixture,
                             G_GNUC_UNUSED gconstpointer user_data)
{
  GError *error = NULL;

  fixture->cnc = NULL;

  CreateDBObject *crdbobj = test_create_database (PROVIDER_NAME);

  g_assert_nonnull (crdbobj);

  fixture->dbname = crdbobj->dbname;
  fixture->quark_list = crdbobj->quark_list;

  const gchar *db_string = g_getenv("POSTGRESQL_CNC_PARAMS");

  if (!db_string)
    {
      g_print ("Please set POSTGRESQL_CNC_PARAMS variable"
               "with dbname, host, user and port (usually 5432)\n");
      g_print ("Test will not be performed\n");
      return;
    }

  GString *new_cnc_string = g_string_new (db_string);
  g_string_append_printf (new_cnc_string, ";DB_NAME=%s", fixture->dbname);

  fixture->cnc = gda_connection_open_from_string (PROVIDER_NAME,
                                                  new_cnc_string->str,
                                                  NULL,
                                                  GDA_CONNECTION_OPTIONS_NONE,
                                                  &error);

  g_string_free (new_cnc_string, TRUE);

  if (!fixture->cnc)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (fixture->cnc);

}

static void
test_server_operation_finish (TestObjectFixture *fixture,
                              G_GNUC_UNUSED gconstpointer user_data)
{
  if (!GDA_IS_CONNECTION (fixture->cnc)) {
    return;
  }

  gboolean res = gda_connection_close (fixture->cnc, NULL);

  g_assert_true (res);

  gda_quark_list_free (fixture->quark_list);
  g_free (fixture->dbname);
}

static void
test_server_operation_operations (TestObjectFixture *fixture,
                                  G_GNUC_UNUSED gconstpointer user_data)
{
  GdaServerOperation *op = NULL;
  GError *error = NULL;

  if (!GDA_IS_CONNECTION (fixture->cnc)) {
    return;
  }

/* CREATE_TABLE operation */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_CREATE_TABLE,
                                             NULL,
                                             &error);
  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

/* We will create two tables: Employee and Project. One table will contain foreigh key that points
 * to the column from the second one.
 *
 */

  gboolean res = FALSE;
  /* Define table name */
  res = gda_server_operation_set_value_at (op,
                                           "Project",
                                           &error,
                                           "/TABLE_DEF_P/TABLE_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Table will not be temporary */
  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/TABLE_DEF_P/TABLE_TEMP");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Create table if table doesn't exist yet */
  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Define column id*/
  gint column_order = 0;
  res = gda_server_operation_set_value_at (op,
                                           "id",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* We need to pass DBMS specific string reprisentation of the column type */
  const gchar *id_dbms_type = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (fixture->cnc),
									 fixture->cnc,
									 G_TYPE_INT);
  res = gda_server_operation_set_value_at (op,
                                           id_dbms_type,
                                           &error,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Define column name */
  column_order++;
  res = gda_server_operation_set_value_at (op,
                                           "name",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* We need to pass DBMS specific string reprisentation of the column type */
  const gchar *name_dbms_type = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (fixture->cnc),
									   fixture->cnc,
									   G_TYPE_STRING);

  res = gda_server_operation_set_value_at (op,
                                           name_dbms_type,
                                           &error,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "50",
                                           &error,
                                           "/FIELDS_A/@COLUMN_SIZE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "Default_name",
                                           &error,
                                           "/FIELDS_A/@COLUMN_DEFAULT/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  g_clear_object (&op);

  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_CREATE_TABLE,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

/* We will create two tables: Employee and Project. One table will contain foreigh key that points
 *iiiiiii to the column from the second one.
 *
 */
  /* Define table name */
  res = gda_server_operation_set_value_at (op,
                                           "Employee",
                                           &error,
                                           "/TABLE_DEF_P/TABLE_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Table will not be temporary */
  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/TABLE_DEF_P/TABLE_TEMP");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Create table if table doesn't exist yet */
  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Define column id */
  column_order = 0;
  res = gda_server_operation_set_value_at (op,
                                           "id",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* We need to pass DBMS specific string reprisentation of the column type */
  const gchar *id2_dbms_type = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (fixture->cnc),
									  fixture->cnc,
									  G_TYPE_INT);
  res = gda_server_operation_set_value_at (op,
                                           id2_dbms_type,
                                           &error,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Define column name */
  column_order++;
  res = gda_server_operation_set_value_at (op,
                                           "name",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* We need to pass DBMS specific string reprisentation of the column type */
  const gchar *name2_dbms_type = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (fixture->cnc),
									   fixture->cnc,
									   G_TYPE_STRING);
  res = gda_server_operation_set_value_at (op,
                                           name2_dbms_type,
                                           &error,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "50",
                                           &error,
                                           "/FIELDS_A/@COLUMN_SIZE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "Default_name",
                                           &error,
                                           "/FIELDS_A/@COLUMN_DEFAULT/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Define column project_id */
  column_order++;
  res = gda_server_operation_set_value_at (op,
                                           "project_id",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NAME/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* We need to pass DBMS specific string reprisentation of the column type */
  const gchar *id3_dbms_type = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (fixture->cnc),
									  fixture->cnc,
									  G_TYPE_INT);
  res = gda_server_operation_set_value_at (op,
                                           id3_dbms_type,
                                           &error,
                                           "/FIELDS_A/@COLUMN_TYPE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_NNUL/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_AUTOINC/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_UNIQUE/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/FIELDS_A/@COLUMN_PKEY/%d",
                                           column_order);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "Project",
                                           &error,
                                           "/FKEY_S/%d/FKEY_REF_TABLE",
                                           0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "RESTRICT",
                                           &error,
                                           "/FKEY_S/%d/FKEY_ONDELETE",
                                           0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "RESTRICT",
                                           &error,
                                           "/FKEY_S/%d/FKEY_ONUPDATE",
                                           0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "project_id",
                                           &error,
                                           "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d",
                                           0,0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "id",
                                           &error,
                                           "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
                                           0,0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  /* END of CREATE_TABLE operation */

  g_clear_object (&op);

  /* Start of ADD_COLUMN operation */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc), fixture->cnc,
                                             GDA_SERVER_OPERATION_ADD_COLUMN,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

  res = gda_server_operation_set_value_at (op,
                                           "Project",
                                           &error,
                                           "/COLUMN_DEF_P/TABLE_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "cost",
                                           &error,
                                           "/COLUMN_DEF_P/COLUMN_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* We need to pass DBMS specific string reprisentation of the column type */
  const gchar *cost_dbms_type = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (fixture->cnc),
									   fixture->cnc,
									   GDA_TYPE_NUMERIC);
  res = gda_server_operation_set_value_at (op,
                                           cost_dbms_type,
                                           &error,
                                           "/COLUMN_DEF_P/COLUMN_TYPE");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "5",
                                           &error,
                                           "/COLUMN_DEF_P/COLUMN_SIZE");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "2",
                                           &error,
                                           "/COLUMN_DEF_P/COLUMN_SCALE");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/COLUMN_DEF_P/COLUMN_NNUL");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE  (error);

  g_assert_true (res);

  g_clear_object (&op);

  /* START RENAME_TABLE OPERATION */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc), fixture->cnc,
                                             GDA_SERVER_OPERATION_RENAME_TABLE,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (op);

  res = gda_server_operation_set_value_at (op,
                                           "Employee",
                                           &error,
                                           "/TABLE_DESC_P/TABLE_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "NewEmployee",
                                           &error,
                                           "/TABLE_DESC_P/TABLE_NEW_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* END RENAME_TABLE OPERATION */
  g_clear_object (&op);

  /* START CREATE_VIEW OPERATION */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_CREATE_VIEW,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

  res = gda_server_operation_set_value_at (op,
                                           "MyView",
                                           &error,
                                           "/VIEW_DEF_P/VIEW_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "FALSE",
                                           &error,
                                           "/VIEW_DEF_P/VIEW_TEMP");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/VIEW_DEF_P/VIEW_IFNOTEXISTS");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "SELECT name, project_id FROM NewEmployee",
                                           &error,
                                           "/VIEW_DEF_P/VIEW_DEF");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  /* END CREATE_VIEEW OPERATION */

  g_clear_object (&op);

  /* START DROP_VIEW OPERATION */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_DROP_VIEW,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

  res = gda_server_operation_set_value_at (op,
                                           "MyView",
                                           &error,
                                           "/VIEW_DESC_P/VIEW_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/VIEW_DESC_P/VIEW_IFNOTEXISTS");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  /* END DROP VIEW OPERATION */

  g_clear_object (&op);

  /* START CREATE_INDEX OPERATION */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_CREATE_INDEX,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

  res = gda_server_operation_set_value_at (op,
                                           "UNIQUE",
                                           &error,
                                           "/INDEX_DEF_P/INDEX_TYPE");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "MyIndex",
                                           &error,
                                           "/INDEX_DEF_P/INDEX_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "NewEmployee",
                                           &error,
                                           "/INDEX_DEF_P/INDEX_ON_TABLE");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/INDEX_DEF_P/INDEX_IFNOTEXISTS");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "name",
                                           &error,
                                           "/INDEX_FIELDS_S/%d/INDEX_FIELD",
                                           0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "BINARY",
                                           &error,
                                           "/INDEX_FIELD_S/%d/INDEX_COLLATE",
                                           0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "ASC",
                                           &error,
                                           "/INDEX_FIELD_S/%d/INDEX_SORT_ORDER",
                                           0);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc), fixture->cnc, op, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  /* END CREATE_INDEX OPERATION */
  g_clear_object (&op);

  /* START DROP_INDEX OPERATION */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_DROP_INDEX,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

  res = gda_server_operation_set_value_at (op,
                                           "MyIndex",
                                           &error,
                                           "/INDEX_DESC_P/INDEX_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/INDEX_DESC_P/INDEX_IFEXISTS");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  /* END DROP_INDEX OPERATION */

  g_clear_object (&op);

  /* START DROP_TABLE OPERATION */
  op = gda_server_provider_create_operation (gda_connection_get_provider (fixture->cnc),
                                             fixture->cnc,
                                             GDA_SERVER_OPERATION_DROP_TABLE,
                                             NULL,
                                             &error);

  if (!op)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_nonnull (op);

  res = gda_server_operation_set_value_at (op,
                                           "NewEmployee",
                                           &error,
                                           "/TABLE_DESC_P/TABLE_NAME");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_operation_set_value_at (op,
                                           "TRUE",
                                           &error,
                                           "/TABLE_DESC_P/TABLE_IFEXISTS");

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_server_provider_perform_operation (gda_connection_get_provider (fixture->cnc),
                                               fixture->cnc,
                                               op,
                                               &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
  /* END DROP_TABLE OPERATION */

  g_object_unref (op);
}

static void
test_server_operation_operations_db (TestObjectFixture *fixture,
                                     G_GNUC_UNUSED gconstpointer user_data)
{
  if (!GDA_IS_CONNECTION (fixture->cnc)) {
    return;
  }

  GError *error = NULL;
/* Define table Project */
  GdaDbTable *tproject = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE (tproject), "Project");
  gda_db_table_set_is_temp (tproject, FALSE);

  /* Defining column id */
  GdaDbColumn *pid = gda_db_column_new ();
  gda_db_column_set_name (pid, "id");
  gda_db_column_set_type (pid, G_TYPE_INT);
  gda_db_column_set_nnul (pid, TRUE);
  gda_db_column_set_autoinc (pid, TRUE);
  gda_db_column_set_unique (pid, TRUE);
  gda_db_column_set_pkey (pid, TRUE);

  gda_db_table_append_column (tproject, pid);

  g_object_unref (pid);

  /* Defining column name */
  GdaDbColumn *pname = gda_db_column_new ();
  gda_db_column_set_name (pname, "name");
  gda_db_column_set_type (pname, G_TYPE_STRING);
  gda_db_column_set_size (pname, 50);
  gda_db_column_set_nnul (pname, TRUE);
  gda_db_column_set_autoinc (pname, FALSE);
  gda_db_column_set_unique (pname, TRUE);
  gda_db_column_set_pkey (pname, FALSE);
  gda_db_column_set_default (pname, "Default_name");

  gda_db_table_append_column (tproject, pname);

  g_object_unref (pname);

/* Create table */
  gboolean res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (tproject), fixture->cnc, NULL, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

/* Define table Employee */
  GdaDbTable *temployee = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE (temployee), "Employee");
  gda_db_table_set_is_temp (temployee, FALSE);

  /* Defining column id */
  GdaDbColumn *eid = gda_db_column_new ();
  gda_db_column_set_name (eid, "id");
  gda_db_column_set_type (eid, G_TYPE_INT);
  gda_db_column_set_nnul (eid, TRUE);
  gda_db_column_set_autoinc (eid, TRUE);
  gda_db_column_set_unique (eid, TRUE);
  gda_db_column_set_pkey (eid, TRUE);

  gda_db_table_append_column (temployee, eid);

  g_object_unref (eid); /* We will reuse this object */

  GdaDbColumn *ename = gda_db_column_new ();
  gda_db_column_set_name (ename, "name");
  gda_db_column_set_type (ename, G_TYPE_STRING);
  gda_db_column_set_size (ename, 50);
  gda_db_column_set_nnul (ename, TRUE);
  gda_db_column_set_autoinc (ename, FALSE);
  gda_db_column_set_unique (ename, TRUE);
  gda_db_column_set_pkey (ename, FALSE);
  gda_db_column_set_default (ename, "Default_name");

  gda_db_table_append_column (temployee, ename);

  g_object_unref (ename); /* We will reuse this object */

  GdaDbColumn *project_id = gda_db_column_new();
  gda_db_column_set_name (project_id, "project_id");
  gda_db_column_set_type (project_id, G_TYPE_INT);
  gda_db_column_set_nnul (project_id, TRUE);
  gda_db_column_set_autoinc (project_id, FALSE);
  gda_db_column_set_unique (project_id, FALSE);
  gda_db_column_set_pkey (project_id, FALSE);

  gda_db_table_append_column (temployee, project_id);

  g_object_unref (project_id);

/* Creating Foreign key */
  GdaDbFkey *fkey = gda_db_fkey_new ();
  gda_db_fkey_set_ref_table (fkey, "Project");
  gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_RESTRICT);
  gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_RESTRICT);
  gda_db_fkey_set_field (fkey, "project_id", "id");

  gda_db_table_append_fkey (temployee, fkey);

  g_object_unref (fkey);

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (temployee), fixture->cnc, NULL, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  /* Start of ADD_COLUMN operation */
  GdaDbColumn *cost = gda_db_column_new ();
  gda_db_column_set_name (cost, "costa");
  gda_db_column_set_type (cost, GDA_TYPE_NUMERIC);
  gda_db_column_set_size (cost, 5);
  gda_db_column_set_scale (cost, 2);
  gda_db_column_set_nnul (cost, FALSE);

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (tproject), fixture->cnc, cost, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  g_object_unref (cost);
  g_object_unref (tproject);

/* RENAME_TABLE operation */
  GdaDbTable *new_table = gda_db_table_new ();
  gda_db_base_set_name (GDA_DB_BASE (new_table), "NewEmployee");

  res = gda_ddl_modifiable_rename (GDA_DDL_MODIFIABLE (temployee), fixture->cnc, new_table, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  GdaDbView *myview = gda_db_view_new ();
  gda_db_base_set_name (GDA_DB_BASE (myview), "MyView");
  gda_db_view_set_istemp (myview, FALSE);
  gda_db_view_set_defstring (myview, "SELECT name, project_id FROM NewEmployee");

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (myview), fixture->cnc, NULL, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  /* DROP_VIEW operation. We will reuse the  view */
  GdaDbViewRefAction action = GDA_DB_VIEW_RESTRICT;
  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (myview), fixture->cnc, &action, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  GdaDbIndex *index = gda_db_index_new ();

  gda_db_index_set_unique (index, TRUE);
  gda_db_base_set_name (GDA_DB_BASE (index), "MyIndex"); // GdaDBIndex is derived from GdaDbBase

  GdaDbIndexField *field = gda_db_index_field_new ();

  GdaDbColumn *fcol = gda_db_column_new ();

  gda_db_column_set_name (fcol, "name");

  gda_db_index_field_set_column (field, fcol);
  gda_db_index_field_set_sort_order (field, GDA_DB_INDEX_SORT_ORDER_ASC);

  gda_db_index_append_field (index, field);
  g_object_unref (fcol);
  g_object_unref (field);

  g_object_set (index, "table", new_table, NULL);

  res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (index), fixture->cnc, new_table, NULL);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (index), fixture->cnc, NULL, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);

  res = gda_ddl_modifiable_drop (GDA_DDL_MODIFIABLE (new_table), fixture->cnc, NULL, &error);

  if (!res)
    GDA_PGSQL_ERROR_HANDLE (error);

  g_assert_true (res);
}

gint
main(gint argc, gchar *argv[])
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

  g_test_add ("/test-server-operation-sqlite/old-so-module",
	      TestObjectFixture,
	      NULL,
	      test_server_operation_start,
	      test_server_operation_operations,
	      test_server_operation_finish);

  g_test_add ("/test-server-operation-sqlite/gda-db-module",
	      TestObjectFixture,
	      NULL,
	      test_server_operation_start,
	      test_server_operation_operations_db,
	      test_server_operation_finish);

  return g_test_run();
}

