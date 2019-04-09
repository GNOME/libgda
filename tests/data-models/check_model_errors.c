/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <glib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include <tests/raw-ddl-creator.h>
#include <glib/gstdio.h>
#include "../test-cnc-utils.h"
#include "../data-model-errors.h"
#include <virtual/libgda-virtual.h>

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

#define CHECK_EXTRA_INFO

static void dump_data_model (GdaDataModel *model);

typedef gboolean (*TestFunc) (GdaConnection *);
static gint test1 (GdaConnection *cnc);
static gint test2 (GdaConnection *cnc);

TestFunc tests[] = {
	test1,
	test2
};

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	guint i, ntests = 0, number_failed = 0;

	gda_init ();
	
	for (i = 0; i < sizeof (tests) / sizeof (TestFunc); i++) {
		g_print ("---------- test %d ----------\n", i+1);
		gint n = tests[i] (NULL);
		number_failed += n;
		if (n > 0) 
			g_print ("Test %d failed\n", i+1);
		ntests ++;
	}

	g_print ("TESTS COUNT: %d\n", i);
	g_print ("FAILURES: %d\n", number_failed);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Commented out because it is not used:
static GdaStatement *
stmt_from_string (const gchar *sql)
{
	GdaStatement *stmt;
	GError *error = NULL;

	static GdaSqlParser *parser = NULL;
	if (!parser)
		parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
	if (!stmt) {
		g_print ("Cound not parse SQL: %s\nSQL was: %s\n",
			 error && error->message ? error->message : "No detail",
			 sql);
		exit (EXIT_FAILURE);
	}
	return stmt;
}
*/

static gboolean
check_iter_contents (GdaDataModel *model, GdaDataModelIter *iter)
{
	gint i, row;
	row = gda_data_model_iter_get_row (iter);
	for (i = 0; i < gda_data_model_get_n_columns (model); i++) {
		GdaHolder *holder;
		const GValue *vi, *vm;
		GError *lerror1 = NULL, *lerror2 = NULL;

		holder = gda_set_get_nth_holder (GDA_SET (iter), i);
		if (gda_holder_is_valid_e (holder, &lerror1))
			vi = gda_holder_get_value (holder);
		else
			vi = NULL;
		vm = gda_data_model_get_value_at (model, i, row, &lerror2);
		if (vi == vm) {
			if (!vi) {
				if (! lerror1 || !lerror1->message) {
#ifdef CHECK_EXTRA_INFO
					g_print ("Iterator reported invalid value without any error set\n");
#endif
					return FALSE;
				}
				else if (! lerror2 || !lerror2->message) {
#ifdef CHECK_EXTRA_INFO
					g_print ("Data model reported invalid value without any error set\n");
#endif
					return FALSE;
				}
				else if (lerror1->code != lerror2->code) {
#ifdef CHECK_EXTRA_INFO
					g_print ("Error codes differ!\n");
#endif
					return FALSE;
				}
				else if (lerror1->domain != lerror2->domain) {
#ifdef CHECK_EXTRA_INFO
					g_print ("Error domains differ!\n");
#endif
					return FALSE;
				}
				else if (strcmp (lerror1->message, lerror2->message)) {
#ifdef CHECK_EXTRA_INFO
					g_print ("Error messages differ:\n\t%s\t%s",
						 lerror1->message, lerror2->message);
#endif
					return FALSE;
				}
			}
			else if (gda_value_differ (vi, vm)) {
#ifdef CHECK_EXTRA_INFO
				g_print ("Iter's contents at (%d,%d) is wrong: expected [%s], got [%s]\n",
					 i, row, 
					 vm ? gda_value_stringify (vm) : "ERROR",
					 vi ? gda_value_stringify (vi) : "ERROR");
#endif
				return FALSE;
			}
		}
		else if (gda_value_differ (vi, vm)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Iter's contents at (%d,%d) is wrong: expected [%s], got [%s]\n",
				 i, row, 
				 vm ? gda_value_stringify (vm) : "ERROR",
				 vi ? gda_value_stringify (vi) : "ERROR");
#endif
			return FALSE;	
		}
	}
	return TRUE;
}

/*
 * check modifications statements' setting
 * 
 * Returns the number of failures 
 */
static gint
test1 (G_GNUC_UNUSED GdaConnection *cnc)
{
	GdaDataModel *model;
	gint nfailed = 0;

	model = gda_data_model_errors_new ();
	if (!model) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not create DataModelErrors!\n");
#endif
		goto out;
	}
	dump_data_model (model);

	/* iterate forward */
	GdaDataModelIter *iter;
	gint i, row;
	iter = gda_data_model_create_iter (model);
	for (i = 0, gda_data_model_iter_move_next (iter);
	     gda_data_model_iter_is_valid (iter);
	     i++, gda_data_model_iter_move_next (iter)) {
		row = gda_data_model_iter_get_row (iter);
		if (i != row) {
			nfailed++;
#ifdef CHECK_EXTRA_INFO
			g_print ("Error iterating: expected to be at row %d and reported row is %d!\n",
				 i, row);
#endif
			goto out;
		}
		g_print ("Now at row %d\n", i);

		if (! check_iter_contents (model, iter)) {
			nfailed++;
			goto out;
		}
	}
	g_print ("Now at row %d\n", gda_data_model_iter_get_row (iter));

	/* iterate backward */
	i--;
	if (!gda_data_model_iter_move_to_row (iter, i)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: move to last row (%d) failed\n", i);
#endif
		goto out;
	}
	row = gda_data_model_iter_get_row (iter);
	if (row != i) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: move to last row (%d) should have set row to %d and is set to %d\n",
			 i, i, row);
#endif
		goto out;
	}
	for (;
	     gda_data_model_iter_is_valid (iter);
	     i--, gda_data_model_iter_move_prev (iter)) {
		row = gda_data_model_iter_get_row (iter);
		if (i != row) {
			nfailed++;
#ifdef CHECK_EXTRA_INFO
			g_print ("Error iterating: expected to be at row %d and reported row is %d!\n",
				 i, row);
#endif
			goto out;
		}
		g_print ("Now at row %d\n", i);

		if (! check_iter_contents (model, iter)) {
			nfailed++;
			goto out;
		}
	}
	g_print ("Now at row %d\n", gda_data_model_iter_get_row (iter));

 out:
	g_object_unref (model);
	
	return nfailed;
}

static void
dump_data_model (GdaDataModel *model)
{
        gint i, ncols;
        g_print ("=== Data Model Dump ===\n");
        ncols = gda_data_model_get_n_columns (model);
        for (i = 0; i < ncols; i++) {
                GdaColumn *col;
                col = gda_data_model_describe_column (model, i);
                if (!col)
                        g_print ("Missing column %d\n", i);
                g_print ("Column %d: ptr=>%p type=>%s\n", i, col, g_type_name (gda_column_get_g_type (col)));
        }
        gda_data_model_dump (model, stdout);
}

/* Commented out because it is not used.
static gboolean
compare_data_models (GdaDataModel *model1, GdaDataModel *model2, GError **error)
{
        GdaDataComparator *cmp;
        GError *lerror = NULL;
        cmp = (GdaDataComparator*) gda_data_comparator_new (model1, model2);
        if (! gda_data_comparator_compute_diff (cmp, &lerror)) {
#ifdef CHECK_EXTRA_INFO
                g_print ("Could not compute the data model differences: %s\n",
                         lerror && lerror->message ? lerror->message : "No detail");
		g_print ("Model1 is:\n");
                gda_data_model_dump (model1, stdout);
                g_print ("Model2 is:\n");
                gda_data_model_dump (model2, stdout);
#endif
                goto onerror;
        }
        if (gda_data_comparator_get_n_diffs (cmp) != 0) {
#ifdef CHECK_EXTRA_INFO
                g_print ("There are some differences when comparing data models...\n");
                g_print ("Model1 is:\n");
                gda_data_model_dump (model1, stdout);
                g_print ("Model2 is:\n");
                gda_data_model_dump (model2, stdout);
#endif
		g_set_error (&lerror, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "There are some differences when comparing data models...");
                goto onerror;
        }
        g_object_unref (cmp);

        return TRUE;

 onerror:
        g_propagate_error (error, lerror);
        return FALSE;
}
*/

/*
 * check modifications statements' setting
 * 
 * Returns the number of failures 
 */
static gint
test2 (G_GNUC_UNUSED GdaConnection *cnc)
{
	GdaDataModel *model = NULL;
	gint nfailed = 0;
	GdaVirtualProvider *virtual_provider = NULL;
	GError *lerror = NULL;
	GdaConnection *vcnc = NULL;
	GdaDataModelIter *iter = NULL;
	GdaStatement *stmt;

#define TABLE_NAME "data"

	model = gda_data_model_errors_new ();
	if (!model) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not create DataModelErrors!\n");
#endif
		goto out;
	}

	virtual_provider = gda_vprovider_data_model_new ();
	vcnc = gda_virtual_connection_open (virtual_provider, GDA_CONNECTION_OPTIONS_NONE, &lerror);
	if (!vcnc) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Virtual ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
#endif
		nfailed++;
		g_clear_error (&lerror);
		goto out;
	}

	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (vcnc), model,
						   TABLE_NAME, &lerror)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Add model as table ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
#endif
		nfailed++;
		g_clear_error (&lerror);
		goto out;
	}
	g_object_unref (model);

	/* Execute SELECT */
	stmt = gda_connection_parse_sql_string (vcnc, "SELECT * FROM " TABLE_NAME, NULL, NULL);
	g_assert (stmt);
	model = gda_connection_statement_execute_select (vcnc, stmt, NULL, &lerror);
	g_object_unref (stmt);
	if (!model) {
		#ifdef CHECK_EXTRA_INFO
		g_print ("SELECT ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
#endif
		nfailed++;
		g_clear_error (&lerror);
		goto out;
	}

	dump_data_model (model);

	/* iterate forward */
	gint i, row;
	iter = gda_data_model_create_iter (model);
	for (i = 0, gda_data_model_iter_move_next (iter);
	     gda_data_model_iter_is_valid (iter);
	     i++, gda_data_model_iter_move_next (iter)) {
		row = gda_data_model_iter_get_row (iter);
		if (i != row) {
			nfailed++;
#ifdef CHECK_EXTRA_INFO
			g_print ("Error iterating: expected to be at row %d and reported row is %d!\n",
				 i, row);
#endif
			goto out;
		}
		g_print ("Now at row %d\n", i);

		if (! check_iter_contents (model, iter)) {
			nfailed++;
			goto out;
		}
	}
	if (i != 4) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: expected to have read %d rows, and read %d\n", 3,
			 i - 1);
		nfailed++;
		goto out;
#endif
	}
	if (gda_data_model_iter_get_row (iter) != -1) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: expected to be at row -1 and reported row is %d!\n",
			 gda_data_model_iter_get_row (iter));
#endif
		nfailed++;
		goto out;
	}

	/* bind as another table */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (vcnc), model,
						   TABLE_NAME "2", &lerror)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Add model as table2 ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
#endif
		nfailed++;
		g_clear_error (&lerror);
		goto out;
	}
	g_object_unref (model);

	stmt = gda_connection_parse_sql_string (vcnc, "SELECT * FROM " TABLE_NAME "2", NULL, NULL);
	g_assert (stmt);
	model = gda_connection_statement_execute_select (vcnc, stmt, NULL, &lerror);
	g_object_unref (stmt);
	if (!model) {
		#ifdef CHECK_EXTRA_INFO
		g_print ("SELECT ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
#endif
		nfailed++;
		g_clear_error (&lerror);
		goto out;
	}
	
	/* iterate forward */
	iter = gda_data_model_create_iter (model);
	for (i = 0, gda_data_model_iter_move_next (iter);
	     gda_data_model_iter_is_valid (iter);
	     i++, gda_data_model_iter_move_next (iter)) {
		row = gda_data_model_iter_get_row (iter);
		if (i != row) {
			nfailed++;
#ifdef CHECK_EXTRA_INFO
			g_print ("Error iterating: expected to be at row %d and reported row is %d!\n",
				 i, row);
#endif
			goto out;
		}
		g_print ("Now at row %d\n", i);

		if (! check_iter_contents (model, iter)) {
			nfailed++;
			goto out;
		}
	}
	if (i != 4) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: expected to have read %d rows, and read %d\n", 3,
			 i - 1);
		nfailed++;
		goto out;
#endif
	}
	if (gda_data_model_iter_get_row (iter) != -1) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: expected to be at row -1 and reported row is %d!\n",
			 gda_data_model_iter_get_row (iter));
#endif
		nfailed++;
		goto out;
	}

 out:
	if (iter != NULL) {
		g_object_unref (iter);
	}
	if (model != NULL) {
		g_object_unref (model);
	}
	if (vcnc != NULL) {
		g_object_unref (vcnc);
	}
	if (virtual_provider != NULL) {
		g_object_unref (virtual_provider);
	}
	
	return nfailed;
}
