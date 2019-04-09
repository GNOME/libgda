/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
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

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

static GdaDataModel *get_source_model (GdaConnection *cnc);

static gint test_field_formats (GdaDataPivot *pivot);
static gint test_column_formats (GdaDataPivot *pivot);
static gint test_on_data (GdaConnection *cnc);

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	int number_failed = 0;
	GdaDataModel *source;
	GdaDataPivot *pivot;
	gchar *fname;
	GdaConnection *cnc;
	GError *error = NULL;

        gda_init ();

        /* open connection */
        gchar *cnc_string;
        fname = g_build_filename (ROOT_DIR, "tests", "data-models", NULL);
        cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=pivot", fname);
        g_free (fname);
        cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL,
                                               GDA_CONNECTION_OPTIONS_READ_ONLY, &error);
	g_free (cnc_string);
        if (!cnc) {
		g_print ("Failed to open connection, cnc_string = %s; Error: %s\n", cnc_string, error->message);
        }
	g_assert (cnc != NULL);

	source = get_source_model (cnc);
	pivot = GDA_DATA_PIVOT (gda_data_pivot_new (source));
	g_object_unref (source);
	number_failed += test_field_formats (pivot);
	number_failed += test_column_formats (pivot);
	g_object_unref (pivot);

	number_failed += test_on_data (cnc);

	/* end of tests */
	g_object_unref (cnc);
	if (number_failed == 0)
		g_print ("Ok.\n");
	else
		g_print ("%d failed\n", number_failed);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static GdaDataModel *
get_source_model (GdaConnection *cnc)
{
	GdaDataModel *model;
	GdaStatement *stmt;
	GError *error = NULL;
	stmt = gda_connection_parse_sql_string (cnc,
						"select * from food",
						NULL, &error);
	if (stmt == NULL) {
		g_print ("Error parsing statement: %s\n", error->message);
		g_error_free (error);
	}
	g_assert (stmt);

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
	if (model == NULL) {
		g_print ("Error executing statement: %s\n", error->message);
		g_error_free (error);
	}
	g_assert (model != NULL);
	g_print ("==== Source data:\n");
	gda_data_model_dump (model, NULL);
	return model;
}

typedef struct {
	gchar    *field;
	gchar    *alias;
	gboolean  result;
} AFieldFormat;

AFieldFormat fields_test [] = {
	{"person", "alias", TRUE},
	{"person", NULL, TRUE},
	{"name", NULL, FALSE},
	{"person)", NULL, FALSE},
	{"person || qty", NULL, TRUE},
	{"person, qty", NULL, TRUE},
};

static gint
test_field_formats (GdaDataPivot *pivot)
{
	guint i, nfailed = 0;
	GError *error = NULL;
	for (i = 0; i < sizeof (fields_test) / sizeof (AFieldFormat); i++) {
		AFieldFormat *test = &(fields_test [i]);
		gboolean res;
		res = gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_ROW,
						test->field, test->alias, &error);
		if (res != test->result) {
			if (test->result)
				g_print ("Error: field setting failed for [%s][%s] when "
					 "it should have succedded, error: [%s]\n",
					 test->field, test->alias,
					 error && error->message ? error->message : "no detail");
			else
				g_print ("Error: field setting succedded for [%s][%s] when "
					 "it should have failed\n",
					 test->field, test->alias);
			nfailed ++;
		}
		g_clear_error (&error);
	}
	return nfailed;
}

static gint
test_column_formats (GdaDataPivot *pivot)
{
	guint i, nfailed = 0;
	GError *error = NULL;
	for (i = 0; i < sizeof (fields_test) / sizeof (AFieldFormat); i++) {
		AFieldFormat *test = &(fields_test [i]);
		gboolean res;
		res = gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_COLUMN,
						test->field, test->alias, &error);
		if (res != test->result) {
			if (test->result)
				g_print ("Error: field setting failed for [%s][%s] when "
					 "it should have succedded, error: [%s]\n",
					 test->field, test->alias,
					 error && error->message ? error->message : "no detail");
			else
				g_print ("Error: field setting succedded for [%s][%s] when "
					 "it should have failed\n",
					 test->field, test->alias);
			nfailed ++;
		}
		g_clear_error (&error);
	}
	return nfailed;
}

static gint
test_on_data (GdaConnection *cnc)
{
	GdaDataModel *source;
	GdaDataPivot *pivot;
	GError *error = NULL;
	gint number_failed = 0;

	source = get_source_model (cnc);
	pivot = GDA_DATA_PIVOT (gda_data_pivot_new (source));
	g_object_unref (source);

	gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_ROW,
				  //"person,week",
				  "person",
				  NULL, &error);
	gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_COLUMN,
				  "food",
				  //"CASE food WHEN 'Car' THEN 'Not food' ELSE 'food' END",
				  NULL, &error);

	gda_data_pivot_add_data (pivot, GDA_DATA_PIVOT_SUM,
				 "qty", "sumqty", &error);

	gda_data_pivot_add_data (pivot, GDA_DATA_PIVOT_COUNT,
				 "food", "thefood", &error);

	if (! gda_data_pivot_populate (pivot, &error)) {
		g_print ("Failed to populate pivot: %s\n",
			 error && error->message ? error->message : "no detail");
		number_failed ++;
		goto out;
	}

	gda_data_model_dump (GDA_DATA_MODEL (pivot), NULL);

 out:
	g_object_unref (pivot);
	return number_failed;
}
