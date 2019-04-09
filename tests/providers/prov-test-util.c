/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "prov-test-util.h"
#include <libgda/gda-server-provider-extra.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <unistd.h>

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

#define DB_NAME "gda_check_db"
#define CREATE_FILES 0
GdaSqlParser *parser = NULL;

/*
 * Data model content's check
 */
static gboolean
compare_data_model_with_expected (GdaDataModel *model, const gchar *expected_file)
{
	GdaDataModel *compare_m;
	GSList *errors;
	gboolean retval = TRUE;

	/* load file into data model */
	compare_m = gda_data_model_import_new_file (expected_file, TRUE, NULL);
	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (compare_m)))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not load the expected schema file '%s': ", expected_file);
		for (; errors; errors = errors->next) 
			g_print ("\t%s\n", 
				 ((GError *)(errors->data))->message ? ((GError *)(errors->data))->message : "No detail");
#endif
		g_object_unref (compare_m);
		return FALSE;
	}
	gchar *dmodel = gda_data_model_dump_as_string (compare_m);
	g_message ("Expected file: %s\nExpected schema:\n%s", expected_file, dmodel);
	g_free (dmodel);
	/* compare number of rows and columns */
	gint ncols, nrows, row, col;
	ncols = gda_data_model_get_n_columns (model);
	nrows = gda_data_model_get_n_rows (model);

	if (ncols != gda_data_model_get_n_columns (compare_m)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Data model has wrong number of columns: expected %d and got %d",
			   gda_data_model_get_n_columns (compare_m), ncols);
#endif
		g_object_unref (compare_m);
		return FALSE;
	}

	/* compare columns' types */
	for (col = 0; col < ncols; col++) {
		GdaColumn *m_col, *e_col;

		m_col = gda_data_model_describe_column (model, col);
		e_col = gda_data_model_describe_column (compare_m, col);
		if (gda_column_get_g_type (m_col) != gda_column_get_g_type (e_col)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Data model has wrong column type for column %d: expected %s and got %s",
				   col, g_type_name (gda_column_get_g_type (e_col)),
				   g_type_name (gda_column_get_g_type (m_col)));
			g_print ("========= Compared Data model =========\n");
			gda_data_model_dump (model, stdout);
			
			g_print ("========= Expected Data model =========\n");
			gda_data_model_dump (compare_m, stdout);
#endif
			g_object_unref (compare_m);
			return FALSE;
		}
	}
	
	/* compare contents */
	for (row = 0; row < nrows; row++) {
		for (col = 0; col < ncols; col++) {
			if (col == 3) /* FIXME: column to ignore */
				continue;
			const GValue *m_val, *e_val;
			GdaColumn *column;
			GError *lerror = NULL;

			column = gda_data_model_describe_column (model, col);
			if (!strcmp (gda_column_get_name (column), "Owner"))
				continue;
			m_val = gda_data_model_get_value_at (model, col, row, &lerror);
			if (! m_val) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Can't get data model's value: %s",
					   lerror && lerror->message ? lerror->message : "No detail");
#endif
				retval = FALSE;
			}
			e_val =  gda_data_model_get_value_at (compare_m, col, row, &lerror);
			if (! e_val) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Can't get data model's value: %s",
					   lerror && lerror->message ? lerror->message : "No detail");
#endif
				retval = FALSE;
			}
			if (retval && gda_value_compare (m_val, e_val)) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Reported schema error line %d, col %d: expected '%s' and got '%s'",
					   row, col, 
					   gda_value_stringify (e_val), gda_value_stringify (m_val));
#endif
				retval = FALSE;
			}
		}
	}
	
	/* FIXME: test that there are no left row in compare_m */
	g_object_unref (compare_m);

	return retval;
}

/*
 * 
 * Check data types' schema
 *
 */
gboolean
prov_test_check_types_schema (GdaConnection *cnc)
{
	if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	GdaDataModel *schema_m;
	GError *error = NULL;
	gchar *str;
	GdaMetaContext mcontext = {"_builtin_data_types", 0, NULL, NULL, NULL};
	g_print ("updating [%s]\n", mcontext.table_name);
	if (! gda_connection_update_meta_store (cnc, &mcontext, &error)) {
		g_warning ("Can't update meta store: %s\n",
			   error && error->message ? error->message : "???");
		return FALSE;
	}
	mcontext.table_name="_udt";
	g_print ("updating [%s]\n", mcontext.table_name);
	if (! gda_connection_update_meta_store (cnc, &mcontext, &error)) {
		g_warning ("Can't update meta store: %s\n",
			   error && error->message ? error->message : "???");
		return FALSE;
	}
	mcontext.table_name="_domains";
	g_print ("updating [%s]\n", mcontext.table_name);
	if (! gda_connection_update_meta_store (cnc, &mcontext, &error)) {
		g_warning ("Can't update meta store: %s\n",
			   error && error->message ? error->message : "???");
		return FALSE;
	}
	
	schema_m = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_TYPES, &error, 0);
	if (!schema_m) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not get the TYPES schema: %s",
			   error && error->message ? error->message : "No detail");
#endif
		return FALSE;
	}
	gchar *dmod = gda_data_model_dump_as_string (schema_m);
	g_message ("Meta Types from connection:\n%s", dmod);
	g_free (dmod);

	str = g_strdup_printf ("TYPES_SCHEMA_%s.xml", gda_connection_get_provider_name (cnc));
	if (CREATE_FILES) {
		/* export schema model to a file, to create first version of the files, not to be used in actual checks */
		GdaSet *plist;
		plist = gda_set_new_inline (1, "OVERWRITE", G_TYPE_BOOLEAN, TRUE);
		if (! (gda_data_model_export_to_file (schema_m, GDA_DATA_MODEL_IO_DATA_ARRAY_XML, str, 
						      NULL, 0, NULL, 0, plist, &error))) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not export schema to file '%s': %s", str,
				   error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}
		g_object_unref (plist);
	}
	else {
		/* compare schema with what's expected */
		gchar *file = g_build_filename (CHECK_SQL_FILES, "tests", "providers", str, NULL);
		if (!compare_data_model_with_expected (schema_m, file))
			return FALSE;
		g_free (file);
	}
	g_free (str);

	g_object_unref (schema_m);
	return TRUE;
}

/*
 * 
 * Check the datamodel accessed through cursors, if available,
 * the data model is a "SELECT * FROM <table>"
 *
 */
static gboolean iter_is_correct (GdaDataModelIter *iter, GdaDataModel *ref_model);
gboolean
prov_test_check_table_cursor_model (GdaConnection *cnc, const gchar *table)
{
	if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	/* create prepared statement */
	GdaStatement *stmt = NULL;
	gchar *str;
	if (!parser) 
		parser = gda_sql_parser_new ();
	str = g_strdup_printf ("SELECT * FROM %s", table);
	stmt = gda_sql_parser_parse_string (parser, str, NULL, NULL);
	g_free (str);
	if (!stmt) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't create GdaStatement!");
#endif
		return FALSE;
	}
	
	/* create models */
	GdaDataModel *ref_model, *cursor_model;
	ref_model = gda_connection_statement_execute_select (cnc, stmt, NULL, NULL);
	if (!ref_model) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't execute GdaStatement (random access requested)!");
#endif
		return FALSE;
	}
	if (! (gda_data_model_get_access_flags (ref_model) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Random access data model does not report supporting this access mode!");
#endif
		return FALSE;
	}
	cursor_model = gda_connection_statement_execute_select_fullv (cnc, stmt, NULL, GDA_STATEMENT_MODEL_CURSOR_FORWARD, NULL, -1);
	if (!cursor_model) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't execute GdaStatement (forward cursor access requested)!");
#endif
		return FALSE;
	}
	if (! (gda_data_model_get_access_flags (cursor_model) & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Forward cursor access data model does not report supporting this access mode!");
#endif
		return FALSE;
	}

	/* check cursor_model using ref_model as reference */
	GdaDataModelIter *iter;
	iter = gda_data_model_create_iter (cursor_model);
	while (gda_data_model_iter_move_next(iter)) {
                if (!iter_is_correct (iter, ref_model)) 
			return FALSE;
        }

	if (gda_data_model_get_n_rows (ref_model) != gda_data_model_get_n_rows (cursor_model)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Forward cursor access data model reports %d rows where %d are expected\n",
			   gda_data_model_get_n_rows (cursor_model), gda_data_model_get_n_rows (ref_model));
#endif
		return FALSE;
	}

#ifdef CHECK_EXTRA_INFO
	g_print ("Tested %d iter positions for rows into table '%s'\n", gda_data_model_get_n_rows (cursor_model), table);
#endif

	g_object_unref (ref_model);
	g_object_unref (cursor_model);
	return TRUE;
}

static gboolean
iter_is_correct (GdaDataModelIter *iter, GdaDataModel *ref_model)
{
        gint rownum;
        gint i, cols;
        GSList *list;

        g_object_get (G_OBJECT (iter), "current-row", &rownum, NULL);

        cols = gda_data_model_get_n_columns (ref_model);
        if (cols != (gint)g_slist_length (gda_set_get_holders (GDA_SET (iter)))) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("Number of columns in iter is not the same as for the referenced data model\n");
#endif
                return FALSE;
        }
        for (i = 0, list = gda_set_get_holders (GDA_SET (iter)); i < cols; i++, list = list->next) {
                const GValue *v1, *v2;
		GError *lerror = NULL;
                v1 = gda_holder_get_value (GDA_HOLDER (list->data));
		if (!v1) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("GdaHolder is set to default (unspecified) value");
#endif
			return FALSE;
		}
                v2 = gda_data_model_get_value_at (ref_model, i, rownum, &lerror);
		if (!v2) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't get data model's value: %s",
				   lerror && lerror->message ? lerror->message : "No detail");
#endif
			return FALSE;
		}
                if (gda_value_compare (v1, v2)) {
#ifdef CHECK_EXTRA_INFO
                        g_print ("Wrong value:\n\tgot: %s\n\texp: %s\n",
                                 gda_value_stringify (v1),
                                 gda_value_stringify (v2));
#endif
                        return FALSE;
                }
        }
        return TRUE;
}

/*
 *
 * Loads data into @table
 *
 */
gboolean
prov_test_load_data (GdaConnection *cnc, const gchar *table)
{
	if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	if (!parser) 
		parser = gda_sql_parser_new ();

	/* load data into @imodel */
	gchar *tmp, *filename;
	GdaDataModel *imodel;
	GSList *errors;
	tmp = g_strdup_printf ("DATA_%s.xml", table);
	filename = g_build_filename (CHECK_SQL_FILES, "tests", "providers", tmp, NULL);
	g_free (tmp);

	imodel = gda_data_model_import_new_file (filename, TRUE, NULL);
	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (imodel)))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not load the expected data file '%s': ", filename);
		for (; errors; errors = errors->next) 
			g_print ("\t%s\n", 
				 ((GError *)(errors->data))->message ? ((GError *)(errors->data))->message : "No detail");
#endif
		g_object_unref (imodel);
		return FALSE;
	}

	{
		gchar *csv;
		csv = gda_data_model_export_to_string (imodel, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
						       NULL, 0, NULL, 0, NULL);
		tmp = g_strdup_printf ("CSV_DATA_%s.csv", table);
		g_file_set_contents (tmp, csv, -1, NULL);
		g_free (tmp);
	}

	/* create INSERT GdaStatement */
	GdaStatement *insert;
	GdaSqlStatement *sqlst;
	GError *error = NULL;
	GValue *value;
	gint i;
	GSList *fields = NULL, *values = NULL;
	sqlst = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);

	g_value_set_string (value = gda_value_new (G_TYPE_STRING), table);
	gda_sql_statement_insert_take_table_name (sqlst, value);
	for (i = 0; i < gda_data_model_get_n_columns (imodel); i++) {
		GdaColumn *column;
		column = gda_data_model_describe_column (imodel, i);

		GdaSqlField *field;
		field = gda_sql_field_new (GDA_SQL_ANY_PART (sqlst->contents));
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), gda_column_get_name (column));
		gda_sql_field_take_name (field, value);
		fields = g_slist_append (fields, field);
		
		GdaSqlExpr *expr;
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (sqlst->contents));
		tmp = g_strdup_printf ("+%d::%s", i, g_type_name (gda_column_get_g_type (column)));
		value = gda_value_new (G_TYPE_STRING);
		g_value_take_string (value, tmp);
		expr->param_spec = gda_sql_param_spec_new (value);
		expr->param_spec->nullok = gda_column_get_allow_null (column);
		values = g_slist_append (values, expr);
	}
	gda_sql_statement_insert_take_fields_list (sqlst, fields);
	gda_sql_statement_insert_take_1_values_list (sqlst, values);
	if (! gda_sql_statement_check_structure (sqlst, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not build INSERT statement: %s", 
			   error && error->message ? error->message : "No detail");
#endif
		g_object_unref (imodel);
		return FALSE;
	}
	
	insert = gda_statement_new ();
	g_object_set (G_OBJECT (insert), "structure", sqlst, NULL);
	gda_sql_statement_free (sqlst);

	/*g_print ("SQL: %s\n", gda_statement_to_sql (insert, NULL, NULL));*/

	/* execute the INSERT statement */
	GdaSet *set;
	if (! gda_statement_get_parameters (insert, &set, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not get the INSERT statement's parameters: %s", 
			   error && error->message ? error->message : "No detail");
#endif
		g_object_unref (imodel);
		return FALSE;
	}
	
	/* try to start a transaction to spped things up */
	gboolean started_transaction;
	started_transaction = gda_connection_begin_transaction (cnc, NULL,
								GDA_TRANSACTION_ISOLATION_UNKNOWN,
								NULL);
	gint row, nrows;
	nrows = gda_data_model_get_n_rows (imodel);
	for (row = 0; row < nrows; row++) {
		for (i = 0; i < gda_data_model_get_n_columns (imodel); i++) {
			tmp = g_strdup_printf ("+%d", i);
			const GValue *value;
			GdaHolder *h;
			GError *lerror = NULL;
			value = gda_data_model_get_value_at (imodel, i, row, &lerror);
			if (!value) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Can't get data model's value: %s",
					   lerror && lerror->message ? lerror->message : "No detail");
#endif
				g_object_unref (imodel);
				return FALSE;
			}
			h = gda_set_get_holder (set, tmp);
			if (! gda_holder_set_value (h, value, &lerror)) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Could not set INSERT statement's parameter '%s': %s",
					   tmp,
					   lerror && lerror->message ? lerror->message : "No detail");
#endif
				g_object_unref (imodel);
				return FALSE;
			}
			g_free (tmp);			
		}

		if (gda_connection_statement_execute_non_select (cnc, insert, set, NULL, &error) == -1) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not execute the INSERT statement for row %d: %s",
				   row,
				   error && error->message ? error->message : "No detail");
#endif
			g_object_unref (imodel);
			return FALSE;
		}
	}

	if (started_transaction) 
		gda_connection_commit_transaction (cnc, NULL, NULL);

#ifdef CHECK_EXTRA_INFO
	g_print ("Loaded %d rows into table '%s'\n", nrows, table);
#endif

	return TRUE;
}
