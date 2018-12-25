/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2012-2017 Daniel Espinosa <esodan@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include "prov-test-common.h"
#include "prov-test-util.h"
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include "../test-cnc-utils.h"
#include "../test-errors.h"


#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

GdaProviderInfo *pinfo;
GdaConnection   *cnc;
gboolean         params_provided;
gboolean         fork_tests = TRUE;

/*
 *
 * SETUP
 *
 */

int
prov_test_common_setup (void)
{
	int number_failed = 0;
	GError *error = NULL;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	cnc = test_cnc_setup_connection (pinfo->id, "testcheckdb", &error);
	if (!cnc) {
		if (error) {
			if (error->domain != TEST_ERROR) {
				gchar *str = g_strdup_printf ("Could not setup connection: %s", 
							      error->message ? error->message : "No detail");
				g_warning ("%s", str);
				g_free (str);
				number_failed++;
			}
			else
				g_print ("==> %s\n", error->message ? error->message : "No detail");
			g_error_free (error);
		}
	}
	else {
		gchar *file;
		file = g_build_filename (CHECK_SQL_FILES, "tests", "providers", "prov_dbstruct.xml", NULL);
		if (!test_cnc_setup_db_structure (cnc, file, &error)) {
			gchar *str = g_strdup_printf ("Could not setup database structure: %s", 
						      error && error->message ? error->message : "No detail");
			g_warning ("%s", str);
			g_free (str);
			if (error)
				g_error_free (error);
			number_failed++;
		}
		g_free (file);
	}
	return number_failed;
}

GdaConnection *
prov_test_common_create_extra_connection (void)
{
	GdaConnection *cnc;
	GError *lerror = NULL;
	cnc = test_cnc_open_connection (pinfo->id, "testcheckdb", &lerror);
	if (!cnc) {
		g_print ("Error setting up connection: %s\n", lerror && lerror->message ? lerror->message : "No detail");
		g_clear_error (&lerror);
	}
	return cnc;
}


/*
 *
 * CLEAN
 *
 */
int
prov_test_common_clean (void)
{
	int number_failed = 0;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	if (!test_cnc_clean_connection (cnc, NULL))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CHECK_META
 *
 */

int
prov_test_common_check_meta (void)
{
	int number_failed = 0;
	GSList *tables = NULL, *list;
	gboolean dump_ok = TRUE;
	GdaMetaStore *store;
	gchar **dump1 = NULL;
	GError *gerror = NULL;
	gint ntables, i;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	/* update meta store */
#ifdef CHECK_EXTRA_INFO
	g_print ("Updating the complete meta store...\n");
#endif
	if (! gda_connection_update_meta_store (cnc, NULL, &gerror)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't update meta store (1): %s\n",
			   gerror && gerror->message ? gerror->message : "???");
#endif
		g_error_free (gerror);
		number_failed++;
		goto theend;
	}

	/* dump all tables */
	store = gda_connection_get_meta_store (cnc);
	tables = gda_meta_store_schema_get_all_tables (store);
	ntables = g_slist_length (tables);
	dump1 = g_new0 (gchar *, ntables + 1);

	for (i = 0, list = tables; list; i++, list = list->next) {
		GdaDataModel *model;
		gchar *tmp;
		
		tmp = g_strdup_printf ("SELECT * FROM %s", (gchar*) list->data);
		model = gda_meta_store_extract (store, tmp, &gerror, NULL);
		g_free (tmp);
		if (!model) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't execute SELECT statement: %s\n",
				   gerror && gerror->message ? gerror->message : "???");
#endif
			g_error_free (gerror);
			dump_ok = FALSE;
			break;
		}

		dump1 [i] = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							     NULL, 0, NULL, 0, NULL);
		g_object_unref (model);
		if (!dump1 [i]) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't export data model\n");
#endif
			dump_ok = FALSE;
			break;
		}
	}

	if (!dump_ok) {
		number_failed++;
		goto theend;
	}

	/* update meta store */
#ifdef CHECK_EXTRA_INFO
	g_print ("Updating the complete meta store...\n");
#endif
	if (! gda_connection_update_meta_store (cnc, NULL, &gerror)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't update meta store (2): %s\n",
			   gerror && gerror->message ? gerror->message : "???");
#endif
		g_error_free (gerror);
		number_failed++;
		goto theend;
	}
	/* Update meta store by context */
	GdaMetaContext *ctx;
	ctx = gda_meta_context_new ();
	gchar *meta_tables[] = {"_tables", "_attributes", "_information_schema_catalog_name",
						"_schemata", "_builtin_data_types", "_udt", "_udt_columns",
						"_enums", "_element_types", "_domains", "_views", "_collations",
						"_character_sets", "_routines", "_triggers", "_columns",
						"_table_constraints", "_referential_constraints",
						"_key_column_usage", "__declared_fk", "_check_column_usage",
						"_view_column_usage", "_domain_constraints", "_parameters",
						"_routine_columns", "_table_indexes", "_index_column_usage",
						NULL};
	for (i = 0; meta_tables[i]; i++) {
		gda_meta_context_set_table (ctx, meta_tables[i]);
#ifdef CHECK_EXTRA_INFO
		g_print ("Updating the meta store for table '%s'\n", meta_tables[i]);
#endif
		if (! gda_connection_update_meta_store (cnc, ctx, &gerror)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't update meta store (on table %s): %s\n",
				   meta_tables[i], gerror && gerror->message ? gerror->message : "???");
#endif
			g_error_free (gerror);
			number_failed++;
			goto theend;
		}
	}
	gda_meta_context_free (ctx);
	
	for (i = 0, list = tables; list; i++, list = list->next) {
		GdaDataModel *model;
		gchar *tmp;
		GError *gerror = NULL;
		
		tmp = g_strdup_printf ("SELECT * FROM %s", (gchar*) list->data);
		model = gda_meta_store_extract (store, tmp, &gerror, NULL);
		g_free (tmp);
		if (!model) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't execute SELECT statement: %s\n",
				   gerror && gerror->message ? gerror->message : "???");
#endif
			g_error_free (gerror);
			number_failed++;
			continue;
		}

		tmp = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
						       NULL, 0, NULL, 0, NULL);
		g_object_unref (model);
		if (!tmp) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't export data model\n");
#endif
			number_failed++;
			continue;
		}
		if (strcmp (tmp, dump1[i])) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Meta data has changed after update for table %s\n", (gchar*) list->data);
			g_print ("===\n%s\n===\n%s\n===\n", tmp, dump1[i]);
#endif
			number_failed++;
			g_free (tmp);
			continue;
		}
#ifdef CHECK_EXTRA_INFO
		else
			g_print ("Meta for table '%s' Ok\n", (gchar*) list->data);
#endif
		g_free (tmp);
	}

 theend:
	/* remove tmp files */
	if (dump1)
		g_strfreev (dump1);
	g_slist_free (tables);

	return number_failed;
}

/*
 *
 * CHECK_META_IDENTIFIERS
 *
 */

int
prov_test_common_check_meta_identifiers (gboolean case_sensitive, gboolean update_all)
{
	GdaMetaStore *store;
	GError *error = NULL;
	gchar *table_name = "CapitalTest";
	gchar *field_name = "AName";
	GdaServerProvider* provider = gda_connection_get_provider (cnc);
	GdaServerOperation* operation;
	GdaDataModel* data;
	GValue *value, *value2;
	const GValue *cvalue;
	GdaMetaContext mcontext = {"_tables", 1, NULL, NULL};
	GdaConnectionOptions options;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s(%s, %s) =============\n", __FUNCTION__,
		 case_sensitive ? "case sensitive" : "case NOT sensitive",
		 update_all ? "complete meta data update" : "partial meta data update");
#endif

	g_object_get (G_OBJECT (cnc), "options", &options, NULL);
	if (case_sensitive)
		g_object_set (G_OBJECT (cnc), "options",
			      options | GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE, NULL);

	

	/* DROP table */
        operation = gda_server_provider_create_operation (provider, cnc,
							  GDA_SERVER_OPERATION_DROP_TABLE, NULL, &error);
        g_assert (operation);
        gda_server_operation_set_value_at_path (operation, table_name, "/TABLE_DESC_P/TABLE_NAME", NULL);
        gda_server_provider_perform_operation (provider, cnc, operation, NULL);
	g_object_unref (operation);

	/* CREATE table */
	operation = gda_server_provider_create_operation (provider, cnc,
							  GDA_SERVER_OPERATION_CREATE_TABLE, NULL, &error);
        g_assert (operation);
        gda_server_operation_set_value_at (operation, table_name, NULL, "/TABLE_DEF_P/TABLE_NAME");
        gda_server_operation_set_value_at (operation, "TRUE", NULL, "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
        gda_server_operation_set_value_at (operation, "id", NULL, "/FIELDS_A/@COLUMN_NAME/0");
        gda_server_operation_set_value_at (operation, "int", NULL, "/FIELDS_A/@COLUMN_TYPE/0");
        gda_server_operation_set_value_at (operation, "TRUE", NULL, "/FIELDS_A/@COLUMN_PKEY/0");
        if (! gda_server_provider_perform_operation (provider, cnc, operation, &error)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("perform_operation(CREATE_TABLE) failed: %s\n", error && error->message ? 
			   error->message : "???");
#endif
                g_clear_error (&error);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
                return 1;
        }


	/* update meta store */
#ifdef CHECK_EXTRA_INFO
	g_print ("Updating the complete meta store...\n");
#endif
	if (!update_all) {
		g_value_take_string ((value = gda_value_new (G_TYPE_STRING)),
				     gda_meta_store_sql_identifier_quote (table_name, cnc));
		mcontext.column_names = g_new (gchar *, 1);
		mcontext.column_names [0] = "table_name";
		mcontext.column_values = g_new (GValue *, 1);
		mcontext.column_values [0] = value;
	}
	store = gda_meta_store_new (NULL); /* create an in memory meta store */
	g_object_set (G_OBJECT (cnc), "meta-store", store, NULL);
	g_object_unref (store);
	if (! gda_connection_update_meta_store (cnc, update_all ? NULL : &mcontext, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't FULL update meta store: %s\n",
			   error && error->message ? error->message : "???");
#endif
		g_clear_error (&error);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}

	/* check table */
#ifdef CHECK_EXTRA_INFO
	g_print ("Checking fetched meta data...\n");
#endif
	g_value_take_string ((value = gda_value_new (G_TYPE_STRING)),
			     gda_meta_store_sql_identifier_quote (table_name, cnc));
	data = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_TABLES, &error, 1,
						   "name", value);
	g_assert (data);
	if (gda_data_model_get_n_rows (data) != 1) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): wrong number of rows : %d\n",
			   gda_data_model_get_n_rows (data));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	cvalue = gda_data_model_get_value_at (data, 0, 0, &error);
	g_assert (cvalue);
	if (gda_value_compare (value, cvalue)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): expected %s and got %s\n",
			   gda_value_stringify (cvalue), gda_value_stringify (value));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	g_object_unref (data);

	/* check fields of table */
	data = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_FIELDS, &error, 1,
						   "name", value);
	g_assert (data);
	if (gda_data_model_get_n_rows (data) != 1) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): wrong number of rows : %d\n",
			   gda_data_model_get_n_rows (data));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	cvalue = gda_data_model_get_value_at (data, 0, 0, &error);
	g_assert (cvalue);
	g_value_set_string ((value2 = gda_value_new (G_TYPE_STRING)), "id");
	if (gda_value_compare (value2, cvalue)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): expected %s and got %s\n",
			   gda_value_stringify (cvalue), gda_value_stringify (value));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	gda_value_free (value);
	gda_value_free (value2);
	g_object_unref (data);

	/* ALTER table to add a column */
	operation = gda_server_provider_create_operation (provider, cnc,
							  GDA_SERVER_OPERATION_ADD_COLUMN, NULL, &error);
        g_assert (operation);
        gda_server_operation_set_value_at_path (operation, table_name, "/COLUMN_DEF_P/TABLE_NAME", NULL);
        gda_server_operation_set_value_at (operation, field_name, NULL, "/COLUMN_DEF_P/COLUMN_NAME", NULL);
        gda_server_operation_set_value_at (operation, "int", NULL, "/COLUMN_DEF_P/COLUMN_TYPE", NULL);
        if (! gda_server_provider_perform_operation (provider, cnc, operation, &error)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("perform_operation(ADD_COLUMN) failed: %s\n", error && error->message ? 
			   error->message : "???");
#endif
                g_clear_error (&error);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
                return 1;
        }

	/* update meta store */
#ifdef CHECK_EXTRA_INFO
	g_print ("Updating the complete meta store...\n");
#endif
	if (! gda_connection_update_meta_store (cnc, update_all ? NULL : &mcontext, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't FULL update meta store: %s\n",
			   error && error->message ? error->message : "???");
#endif
		g_clear_error (&error);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}

	/* check table */
#ifdef CHECK_EXTRA_INFO
	g_print ("Checking fetched meta data...\n");
#endif
	g_value_take_string ((value = gda_value_new (G_TYPE_STRING)),
			     gda_meta_store_sql_identifier_quote (table_name, cnc));
	data = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_TABLES, &error, 1,
						   "name", value);
	g_assert (data);
	if (gda_data_model_get_n_rows (data) != 1) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): wrong number of rows : %d\n",
			   gda_data_model_get_n_rows (data));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	cvalue = gda_data_model_get_value_at (data, 0, 0, &error);
	g_assert (cvalue);
	if (gda_value_compare (value, cvalue)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): expected %s and got %s\n",
			   gda_value_stringify (cvalue), gda_value_stringify (value));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	g_object_unref (data);

	/* check fields of table */
	g_value_take_string ((value2 = gda_value_new (G_TYPE_STRING)),
			     gda_meta_store_sql_identifier_quote (field_name, cnc));
	data = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_FIELDS, &error, 2,
						   "name", value, "field_name", value2);
	g_assert (data);
	if (gda_data_model_get_n_rows (data) != 1) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): wrong number of rows : %d\n",
			   gda_data_model_get_n_rows (data));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	cvalue = gda_data_model_get_value_at (data, 0, 0, &error);
	g_assert (cvalue);
	if (gda_value_compare (value2, cvalue)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("gda_connection_get_meta_store_data(): expected %s and got %s\n",
			   gda_value_stringify (cvalue), gda_value_stringify (value));
#endif
		gda_value_free (value);
		g_object_unref (data);
		g_object_set (G_OBJECT (cnc), "options", options, NULL);
		return 1;
	}
	gda_value_free (value);
	g_object_unref (data);

	g_object_set (G_OBJECT (cnc), "options", options, NULL);
	return 0;
}

/*
 *
 * LOAD_DATA
 *
 */
int
prov_test_common_load_data (void)
{
	int number_failed = 0;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	if (!prov_test_load_data (cnc, "actor"))
		number_failed++;
	if (!prov_test_load_data (cnc, "language"))
		number_failed++;
	if (!prov_test_load_data (cnc, "film"))
		number_failed++;
	if (!prov_test_load_data (cnc, "film_actor"))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CHECK_CURSOR_MODELS
 *
 */
int
prov_test_common_check_cursor_models (void)
{
	int number_failed = 0;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	if (!prov_test_check_table_cursor_model (cnc, "actor"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "language"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "film"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "film_actor"))
		number_failed++;
	if (!prov_test_check_types_schema (cnc))
		number_failed++;

	return number_failed;	
}

/*
 *
 * Check for the GdaDataModel returned from a SELECT combined with the GDA_STATEMENT_MODEL_ALLOW_NOPARAM
 * flag
 *
 */
int 
prov_test_common_check_data_select (void)
{
	GdaSqlParser *parser = NULL;
	GdaStatement *stmt = NULL;
	GdaSet *params = NULL;
	GError *error = NULL;
	int number_failed = 0;
	GdaDataModel *model = NULL;
	const gchar *remain;
	GSList *columns;
	gint i, ncols;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	/* create statement */
	stmt = gda_sql_parser_parse_string (parser, "SELECT * FROM actor WHERE actor_id <= ##theid::gint", 
					    &remain, &error);
	if (!stmt) {
		number_failed ++;
		goto out;
	}
	if (remain) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_PARSING,
			     "Parsing error, remains: %s", remain);
		number_failed ++;
		goto out;
	}
	
	/* get model */
	if (! (gda_statement_get_parameters (stmt, &params, &error))) {
		number_failed ++;
		goto out;
	}

	model = gda_connection_statement_execute_select_full (cnc, stmt, params, GDA_STATEMENT_MODEL_ALLOW_NOPARAM,
                                                              NULL, &error);
	if (!model) {
		number_failed ++;
		goto out;
	}

	if (gda_data_model_get_n_rows (model) != 0) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model reports %d rows when 0 expected", gda_data_model_get_n_rows (model));
		number_failed ++;
		goto out;
	}

	ncols = gda_data_model_get_n_columns (model);
	for (columns = NULL, i = 0;
	     i < ncols; i++) 
		columns = g_slist_append (columns, gda_data_model_describe_column (model, i));

	/* change param */
	g_object_set (model, "auto-reset", TRUE, NULL);
	if (! gda_set_set_holder_value (params, &error, "theid", 9)) {
                number_failed++;
                goto out;
        }

	if (gda_data_model_get_n_rows (model) != 9) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model reports %d rows when 9 expected", gda_data_model_get_n_rows (model));
		number_failed ++;
		goto out;
	}

	/* check the columns haven't changed */
	ncols = gda_data_model_get_n_columns (model);
	if (i != ncols) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Number of columns has changed from %d to %d\n", i, ncols);
		number_failed ++;
		goto out;
	}

	for (i = 0; i < ncols; i++) {
		if (gda_data_model_describe_column (model, i) != g_slist_nth_data (columns, i)) {
			g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "GdaColumn %d has changed from %p to %p\n", i, 
				     g_slist_nth_data (columns, i),
				     gda_data_model_describe_column (model, i));
			number_failed ++;
			goto out;
		}
	}

 out:
	if (stmt)
		g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	if (model)
		g_object_unref (model);
	g_object_unref (parser);

#ifdef CHECK_EXTRA_INFO
	g_print ("GdaDataSelect test resulted in %d error(s)\n", number_failed);
	if (number_failed != 0) 
		g_print ("error: %s\n", error && error->message ? error->message : "No detail");
	if (error)
		g_error_free (error);
#endif

	return number_failed;
}


/*
 * Check that timezones are handled correctly when storing and retreiving timestamps
 */
static gboolean
timestamp_equal (const GValue *cv1, const GValue *cv2)
{
	g_assert (G_VALUE_TYPE (cv1) == G_TYPE_DATE_TIME);
	g_assert (G_VALUE_TYPE (cv2) == G_TYPE_DATE_TIME);
	GDateTime *ts1, *ts2;
	ts1 = g_value_get_boxed (cv1);
	ts2 = g_value_get_boxed (cv2);
	return g_date_time_compare (ts1, ts2) == 0 ? TRUE : FALSE;
}

int
prov_test_common_check_timestamp (void)
{
	GdaSqlParser *parser = NULL;
	GdaStatement *stmt = NULL;
	GdaSet *params = NULL;
	GError *error = NULL;
	int number_failed = 0;
	GdaDataModel *model = NULL;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	GValue *tso;
	tso = gda_value_new_date_time_from_timet (time (NULL));

	/* insert timestamp */
	stmt = gda_sql_parser_parse_string (parser, "INSERT INTO tstest (ts) VALUES (##ts::timestamp)", NULL, &error);
	if (!stmt ||
	    ! gda_statement_get_parameters (stmt, &params, &error) ||
	    ! gda_set_set_holder_value (params, &error, "ts", g_value_get_boxed (tso)) ||
	    (gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, &error) == -1)) {
		number_failed ++;
		goto out;
	}

	g_print ("Inserted TS %s\n", gda_value_stringify (tso));

	/* retreive timestamp */
	stmt = gda_sql_parser_parse_string (parser, "SELECT ts FROM tstest", NULL, &error);
	if (!stmt) {
		number_failed ++;
		goto out;
	}

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		number_failed ++;
		goto out;
	}
	gda_data_model_dump (model, NULL);
	if (gda_data_model_get_n_rows (model) != 1) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model should have exactly 1 row");
		number_failed ++;
		goto out;
	}

	const GValue *cvalue;
	cvalue = gda_data_model_get_typed_value_at (model, 0, 0, G_TYPE_DATE_TIME, FALSE, &error);
	if (!cvalue) {
		number_failed ++;
		goto out;
	}
	if (! timestamp_equal (tso, cvalue)) {
		gchar *tmpg, *tmpe;
		tmpg = gda_value_stringify (cvalue);
		tmpe = gda_value_stringify (tso);
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Retreived time stamp differs from expected: got '%s' and expected '%s'", tmpg, tmpe);
		g_free (tmpg);
		g_free (tmpe);
		number_failed ++;
		goto out;
	}

	/* check that data handler is correctly configured: compare the same timestamp rendered by a data handler for
	 * timestamps with the date rendered as a string by the server (by appending a string to the timestamp field) */
	GdaDataHandler *dh;
	dh = gda_server_provider_get_data_handler_g_type (gda_connection_get_provider (cnc), cnc, G_TYPE_DATE_TIME);
	gchar *str;
	str = gda_data_handler_get_str_from_value (dh, cvalue);
	g_object_unref (model);

	stmt = gda_sql_parser_parse_string (parser, "SELECT ts || 'asstring' FROM tstest", NULL, &error); /* retreive timestamp as string */
	if (!stmt) {
		g_free (str);
		number_failed ++;
		goto out;
	}

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_free (str);
		number_failed ++;
		goto out;
	}
	gda_data_model_dump (model, NULL);
	if (gda_data_model_get_n_rows (model) != 1) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model should have exactly 1 row");
		g_free (str);
		number_failed ++;
		goto out;
	}
	cvalue = gda_data_model_get_typed_value_at (model, 0, 0, G_TYPE_STRING, FALSE, &error);
	if (!cvalue) {
		g_free (str);
		number_failed ++;
		goto out;
	}	
	if (strncmp (str, g_value_get_string (cvalue), 10)) { /* only compare date parts */
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Returned GdaDataHandler returned wrong result: '%s' and expected '%s'", str, g_value_get_string (cvalue));
		g_free (str);
		number_failed ++;
		goto out;
	}

out:
	if (stmt)
		g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	if (model)
		g_object_unref (model);
	g_object_unref (parser);

#ifdef CHECK_EXTRA_INFO
	g_print ("Timestamp test resulted in %d error(s)\n", number_failed);
	if (number_failed != 0) 
		g_print ("error: %s\n", error && error->message ? error->message : "No detail");
	if (error)
		g_error_free (error);
#endif

	return number_failed;
}

static GValue *
value_new_date_from_timet (time_t val)
{
	GValue *value;
	GDate *date;
	date = g_date_new ();
	g_date_set_time_t (date, val);

	value = gda_value_new (G_TYPE_DATE);
	g_value_set_boxed (value, date);
	g_date_free (date);

        return value;
}

static gboolean
date_equal (const GValue *cv1, const GValue *cv2)
{
	g_assert (G_VALUE_TYPE (cv1) == G_TYPE_DATE);
	g_assert (G_VALUE_TYPE (cv2) == G_TYPE_DATE);
	const GDate *ts1, *ts2;
	ts1 = (GDate*) g_value_get_boxed (cv1);
	ts2 = (GDate*) g_value_get_boxed (cv2);
	return g_date_compare (ts1, ts2) ? FALSE : TRUE;
}

int
prov_test_common_check_date (void)
{
	GdaSqlParser *parser = NULL;
	GdaStatement *stmt = NULL;
	GdaSet *params = NULL;
	GError *error = NULL;
	int number_failed = 0;
	GdaDataModel *model = NULL;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	GValue *tso;
	tso = value_new_date_from_timet (time (NULL));

	/* insert date */
	stmt = gda_sql_parser_parse_string (parser, "INSERT INTO datetest (thedate) VALUES (##thedate::date)", NULL, &error);
	if (!stmt ||
	    ! gda_statement_get_parameters (stmt, &params, &error) ||
	    ! gda_set_set_holder_value (params, &error, "thedate", (GDate*) g_value_get_boxed (tso)) ||
	    (gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, &error) == -1)) {
		number_failed ++;
		goto out;
	}

	g_print ("Inserted date %s\n", gda_value_stringify (tso));

	/* retreive date */
	stmt = gda_sql_parser_parse_string (parser, "SELECT thedate FROM datetest", NULL, &error);
	if (!stmt) {
		number_failed ++;
		goto out;
	}

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		number_failed ++;
		goto out;
	}
	gda_data_model_dump (model, NULL);
	if (gda_data_model_get_n_rows (model) != 1) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model should have exactly 1 row");
		number_failed ++;
		goto out;
	}

	const GValue *cvalue;
	cvalue = gda_data_model_get_typed_value_at (model, 0, 0, G_TYPE_DATE, FALSE, &error);
	if (!cvalue) {
		number_failed ++;
		goto out;
	}
	if (! date_equal (tso, cvalue)) {
		gchar *tmpg, *tmpe;
		tmpg = gda_value_stringify (cvalue);
		tmpe = gda_value_stringify (tso);
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Retreived date differs from expected: got '%s' and expected '%s'", tmpg, tmpe);
		g_free (tmpg);
		g_free (tmpe);
		number_failed ++;
		goto out;
	}

	/* check that data handler is correctly configured: compare the same timestamp rendered by a data handler for
	 * timestamps with the date rendered as a string by the server (by appending a string to the timestamp field) */
	GdaDataHandler *dh;
	dh = gda_server_provider_get_data_handler_g_type (gda_connection_get_provider (cnc), cnc, G_TYPE_DATE);
	gchar *str;
	str = gda_data_handler_get_str_from_value (dh, cvalue);
	g_object_unref (model);

	stmt = gda_sql_parser_parse_string (parser, "SELECT thedate || 'asstring' FROM datetest", NULL, &error); /* retreive date as string */
	if (!stmt) {
		g_free (str);
		number_failed ++;
		goto out;
	}

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_free (str);
		number_failed ++;
		goto out;
	}
	gda_data_model_dump (model, NULL);
	if (gda_data_model_get_n_rows (model) != 1) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model should have exactly 1 row");
		g_free (str);
		number_failed ++;
		goto out;
	}
	cvalue = gda_data_model_get_typed_value_at (model, 0, 0, G_TYPE_STRING, FALSE, &error);
	if (!cvalue) {
		g_free (str);
		number_failed ++;
		goto out;
	}	
	if (strncmp (str, g_value_get_string (cvalue), 10)) { /* only compare date parts */
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Returned GdaDataHandler returned wrong result: '%s' and expected '%s'", str, g_value_get_string (cvalue));
		g_free (str);
		number_failed ++;
		goto out;
	}

out:
	if (stmt)
		g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	if (model)
		g_object_unref (model);
	g_object_unref (parser);

#ifdef CHECK_EXTRA_INFO
	g_print ("Date test resulted in %d error(s)\n", number_failed);
	if (number_failed != 0) 
		g_print ("error: %s\n", error && error->message ? error->message : "No detail");
	if (error)
		g_error_free (error);
#endif

	return number_failed;
}

int
prov_test_common_check_bigint (void)
{
	GdaSqlParser *parser = NULL;
	GdaStatement *stmt = NULL;
	GdaSet *params = NULL;
	GError *error = NULL;
	int number_failed = 0;
	GdaDataModel *model = NULL;

#ifdef CHECK_EXTRA_INFO
	g_print ("\n============= %s() =============\n", __FUNCTION__);
#endif

	parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	GValue *tso;
	tso = gda_value_new (G_TYPE_INT64);
	g_value_set_int64 (tso, 4294967296);

	/* insert date */
	stmt = gda_sql_parser_parse_string (parser, "INSERT INTO bigint (thebigint) VALUES (##thebigint::gint64)", NULL, &error);
	if (!stmt ||
	    ! gda_statement_get_parameters (stmt, &params, &error) ||
	    ! gda_set_set_holder_value (params, &error, "thebigint", g_value_get_int64 (tso)) ||
	    (gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, &error) == -1)) {
		number_failed ++;
		goto out;
	}

	g_print ("Inserted int %s\n", gda_value_stringify (tso));

	/* retreive date */
	stmt = gda_sql_parser_parse_string (parser, "SELECT thebigint FROM bigint", NULL, &error);
	if (!stmt) {
		number_failed ++;
		goto out;
	}

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		number_failed ++;
		goto out;
	}
	gda_data_model_dump (model, NULL);
	if (gda_data_model_get_n_rows (model) != 1) {
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Data model should have exactly 1 row");
		number_failed ++;
		goto out;
	}

	const GValue *cvalue;
	cvalue = gda_data_model_get_typed_value_at (model, 0, 0, G_TYPE_INT64, FALSE, &error);
	if (!cvalue) {
		number_failed ++;
		goto out;
	}
	if (g_value_get_int64 (tso) != g_value_get_int64 (cvalue)) {
		gchar *tmpg, *tmpe;
		tmpg = gda_value_stringify (cvalue);
		tmpe = gda_value_stringify (tso);
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Retreived date differs from expected: got '%s' and expected '%s'", tmpg, tmpe);
		g_free (tmpg);
		g_free (tmpe);
		number_failed ++;
		goto out;
	}


out:
	if (stmt)
		g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	if (model)
		g_object_unref (model);
	g_object_unref (parser);

#ifdef CHECK_EXTRA_INFO
	g_print ("Date test resulted in %d error(s)\n", number_failed);
	if (number_failed != 0)
		g_print ("error: %s\n", error && error->message ? error->message : "No detail");
	if (error)
		g_error_free (error);
#endif

	return number_failed;
}
