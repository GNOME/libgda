/*
 * Copyright (C) 2020 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "../test-cnc-utils.h"
#include "common.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <libgda/libgda.h>
#include <locale.h>

#define START_FUNCTION g_print("Starting: %s\n", __func__);
#define END_FUNCTION g_print("Ending: %s\n", __func__);

typedef struct {
	gboolean dorun;
	const gchar *cnc_string;
	const gchar *dbcreate_params;
	gchar *dbname;
	GdaQuarkList *quark_list;
	GdaMetaStore *store;
	/* Members */
} FixtureObject;

const gchar *prov_id = "PostgreSQL";

static GdaServerOperation *
populate_so (const FixtureObject *fobj, GdaServerOperation *so)
{
	START_FUNCTION
	const gchar *value = NULL;
	gboolean res;
	value = gda_quark_list_find (fobj->quark_list, "HOST");
	res = gda_server_operation_set_value_at (so, value, NULL, "/SERVER_CNX_P/HOST");
	g_assert_true (res);

	value = gda_quark_list_find (fobj->quark_list, "ADM_LOGIN");
	res = gda_server_operation_set_value_at (so, value, NULL, "/SERVER_CNX_P/ADM_LOGIN");
	g_assert_true (res);

	value = gda_quark_list_find (fobj->quark_list, "ADM_PASSWORD");
	res = gda_server_operation_set_value_at (so, value, NULL, "/SERVER_CNX_P/ADM_PASSWORD");
	g_assert_true (res);

	END_FUNCTION
	return so;
}

static void
check_meta_store_postgresql_new_start (FixtureObject *self,
                                       G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION

	self->dorun = TRUE;

	self->cnc_string = g_getenv ("POSTGRESQL_META_CNC");
	if (!self->cnc_string) {
		g_print ("PostgreSQL test not run, please set the POSTGRESQL_META_CNC environment "
			 "variable \n"
			 "For example "
			 "'DB_NAME=$POSTGRES_DB;HOST=postgres;USERNAME=$POSTGRES_USER;PASSWORD=$"
			 "POSTGRES_PASSWORD'\n");
		self->dorun = FALSE;
		return;
	}

	self->dbcreate_params = g_getenv ("POSTGRESQL_DBCREATE_PARAMS");

	if (!self->dbcreate_params) {
		g_print ("PostgreSQL test not run, please set the POSTGRESQL_DBCREATE_PARAMS "
			 "environment variable \n");
		self->dorun = FALSE;
		return;
	}

	self->quark_list = gda_quark_list_new_from_string (self->dbcreate_params);

	self->dbname = test_random_string ("pg", 7);

	g_print ("Will use %s DB name\n", self->dbname);

	GError *error = NULL;
	gboolean res;

	GdaServerOperation *op =
	    gda_server_operation_prepare_create_database (prov_id, self->dbname, &error);

	g_assert_nonnull (op);

	op = populate_so (self, op);

	res = gda_server_operation_perform_create_database (op, prov_id, &error);

	if (!res) {
		g_print ("Error: %s\n", error && error->message ? error->message : "No Defaults");
	}

	g_assert_true (res);

	g_object_unref (op);

	GString *new_cnc_string = g_string_new (self->cnc_string);
	g_string_append_printf (new_cnc_string, ";DB_NAME=%s", self->dbname);

	g_print ("Connection string: %s\n", new_cnc_string->str);

	gchar *str = g_strdup_printf ("PostgreSQL://%s", new_cnc_string->str);
	g_string_free (new_cnc_string, TRUE);
	self->store = gda_meta_store_new (str);

	g_print ("STORE: %p, version: %d\n", self->store,
		 self->store ? gda_meta_store_get_version (self->store) : 0);

	common_declare_meta_store (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_new_finish (FixtureObject *self,
                                        G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	/*g_object_unref (self->cnc);*/

	if (self->store != NULL) {
      if (G_IS_OBJECT (self->store)) {
          g_object_unref (self->store);
      }
      self->store = NULL;
  }

	GError *error = NULL;

	GdaServerOperation *op =
	    gda_server_operation_prepare_drop_database (prov_id, self->dbname, &error);

	g_assert_nonnull (op);

	g_print ("Removing %s database\n", self->dbname);

	op = populate_so (self, op);

	g_object_unref (op);

	gda_quark_list_free (self->quark_list);
	g_free (self->dbname);
	END_FUNCTION
}

static void
check_meta_store_postgresql_iscatalog_name (FixtureObject *self,
                                            G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_information_schema_catalog_name (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_schemata (FixtureObject *self,
                                      G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_schemata_1 (self->store);
	test_schemata_2 (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_datatypes (FixtureObject *self,
                                       G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_builtin_data_types (self->store);
	/*test_domains (self->store);*/
	END_FUNCTION
}

static void
check_meta_store_postgresql_tables (FixtureObject *self,
                                    G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_tables (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_views (FixtureObject *self,
                                   G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_views (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_ruitines (FixtureObject *self,
                                      G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_routines (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_triggers (FixtureObject *self,
                                      G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_triggers (self->store);
	/*test_columns (self->store);*/
	END_FUNCTION
}

static void
check_meta_store_postgresql_constraints (FixtureObject *self,
                                         G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_table_constraints (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_refconstraints (FixtureObject *self,
                                            G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_referential_constraints (self->store);
	/*test_key_column_usage (self->store);*/
	END_FUNCTION
}

static void
check_meta_store_postgresql_domain_constr (FixtureObject *self,
                                           G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_domain_constraints (self->store);
	END_FUNCTION
}

static void
check_meta_store_postgresql_params (FixtureObject *self,
                                    G_GNUC_UNUSED gconstpointer user_data)
{
	START_FUNCTION
	if (!self->dorun)
		return;

	test_parameters (self->store);
	END_FUNCTION
}

gint
main (gint argc, gchar *argv[])
{
	START_FUNCTION

	setlocale (LC_ALL, "");

  const gchar* cnc_string = g_getenv ("POSTGRESQL_META_CNC");
	if (cnc_string == NULL) {
		g_print ("PostgreSQL test not run, please set the POSTGRESQL_META_CNC environment "
			 "variable \n"
			 "For example "
			 "'DB_NAME=$POSTGRES_DB;HOST=postgres;USERNAME=$POSTGRES_USER;PASSWORD=$"
			 "POSTGRES_PASSWORD'\n");
		return 0;
	}

	gda_init ();

	g_test_init (&argc, &argv, NULL);

	g_test_add ("/check_meta_store_postgresql_new/FixtureObject", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start,
		    check_meta_store_postgresql_iscatalog_name,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/schemata", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_schemata,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/datatypes", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_datatypes,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/tables", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_tables,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/views", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_views,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/ruitines", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_ruitines,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/triggers", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_triggers,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/constraints", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_constraints,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/refconstraints", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start,
		    check_meta_store_postgresql_refconstraints,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/domain_constr", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start,
		    check_meta_store_postgresql_domain_constr,
		    check_meta_store_postgresql_new_finish);

	g_test_add ("/check_meta_store_postgresql_new/params", FixtureObject, NULL,
		    check_meta_store_postgresql_new_start, check_meta_store_postgresql_params,
		    check_meta_store_postgresql_new_finish);

	END_FUNCTION
	return g_test_run ();
}

