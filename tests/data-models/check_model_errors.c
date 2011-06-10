/*
 * Copyright (C) 2010 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <tests/gda-ddl-creator.h>
#include <glib/gstdio.h>
#include "../test-cnc-utils.h"
#include "../data-model-errors.h"

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

#define CHECK_EXTRA_INFO

static void dump_data_model (GdaDataModel *model);

typedef gboolean (*TestFunc) (GdaConnection *);
static gint test1 (GdaConnection *cnc);

TestFunc tests[] = {
	test1,
};

int
main (int argc, char **argv)
{
	gint i, ntests = 0, number_failed = 0;

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

static gboolean
check_iter_contents (GdaDataModel *model, GdaDataModelIter *iter)
{
	gint i, row;
	row = gda_data_model_iter_get_row (iter);
	for (i = 0; i < gda_data_model_get_n_columns (model); i++) {
		GdaHolder *holder;
		const GValue *vi, *vm;

		holder = gda_set_get_nth_holder (GDA_SET (iter), i);
		if (gda_holder_is_valid (holder))
			vi = gda_holder_get_value (holder);
		else
			vi = NULL;
		vm = gda_data_model_get_value_at (model, i, row, NULL);
		if ((vi == vm) ||
		    (vi && vm && !gda_value_differ (vi, vm)))
			return TRUE;
		else {
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
test1 (GdaConnection *cnc)
{
	GdaDataModel *model;
	gint nfailed = 0;

	model = data_model_errors_new ();
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

	/* iterate backward */
	i--;
	if (gda_data_model_iter_move_to_row (iter, i)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Error iterating: move to last row (%d) should have reported an error\n", i);
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
		g_set_error (&lerror, 0, 0,
			     "%s", "There are some differences when comparing data models...");
                goto onerror;
        }
        g_object_unref (cmp);

        return TRUE;

 onerror:
        g_propagate_error (error, lerror);
        return FALSE;
}
