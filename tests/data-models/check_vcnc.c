/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2017 Daniel Espinosa <esodan@gmail.com>
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
#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>
#include <string.h>
#include <unistd.h>

#define NTHREADS 50

typedef enum {
	ACTION_COMPARE,
	ACTION_EXPORT
} Action;

static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql,
				     gboolean iter_only, GError **error);
static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params, GError **error);
static GdaDataModel *assert_run_sql_select (GdaConnection *cnc, const gchar *sql,
					    Action action, const gchar *compare_file);
static void assert_run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params);
GdaDataModel *load_from_file (const gchar *filename);
static void assert_data_model_equal (GdaDataModel *model, GdaDataModel *ref);

static GdaConnection *open_destination_connection (void);
static void check_update_delete (GdaConnection *virtual);
static void check_simultanous_select_random (GdaConnection *virtual);
static void check_simultanous_select_forward (GdaConnection *virtual);
static void check_threads_select_random (GdaConnection *virtual);
static void check_date (GdaConnection *virtual);

int
main (int argc, char *argv[])
{
	GError *error = NULL;	
	GdaConnection *virtual, *out_cnc;
	GdaVirtualProvider *provider;
	gchar *file;
	
	gda_init ();

	provider = gda_vprovider_hub_new ();
	virtual = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_assert (virtual);

	/* load CSV data models */
	GdaDataModel *country_model, *city_model;
	GdaSet *options = gda_set_new_inline (2, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE,
					      "G_TYPE_2", G_TYPE_GTYPE, G_TYPE_INT);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "city.csv", NULL);
	city_model = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "country.csv", NULL);
	country_model = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	/* Add data models to connection */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual), city_model, "city", &error)) 
		g_error ("Add city model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual), country_model, "country", &error)) 
		g_error ("Add country model error: %s\n", error && error->message ? error->message : "no detail");

	/* SQLite connection for outputs */
        out_cnc = open_destination_connection ();

	/* adding connections to the virtual connection */
        if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual), out_cnc, "out", &error)) {
                g_print ("Could not add connection to virtual connection: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

	check_update_delete (virtual);

	g_print ("*** Copying data into 'countries' virtual table...\n");
	assert_run_sql_non_select (virtual, "INSERT INTO out.countries SELECT * FROM country", NULL);

	check_simultanous_select_random (virtual);
	check_simultanous_select_forward (virtual);
	check_threads_select_random (virtual);
	check_date (virtual);

        if (! gda_connection_close (virtual, &error)) {
		g_print ("gda_connection_close(virtual) error: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
        if (! gda_connection_close (out_cnc, &error)) {
		g_print ("gda_connection_close(out_cnc) error: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	g_print ("All Ok\n");
        return 0;
}

GdaConnection *
open_destination_connection (void)
{
        /* create connection */
        GdaConnection *cnc;
        GError *error = NULL;
        cnc = gda_connection_new_from_string ("SQLite", "DB_DIR=.;DB_NAME=vcnc",
					      NULL,
					      GDA_CONNECTION_OPTIONS_NONE,
					      &error);
        if (!cnc) {
                g_print ("Could not create connection to local SQLite database: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
	if (! gda_connection_open (cnc, &error)) {
		 g_print ("gda_connection_open() error: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

        /* table "cities" */
        assert_run_sql_non_select (cnc, "DROP table IF EXISTS cities", NULL);
        assert_run_sql_non_select (cnc, "CREATE table cities (name string not NULL primary key, countrycode string not null, population int)", NULL);

        /* table "countries" */
        assert_run_sql_non_select (cnc, "DROP table IF EXISTS countries", NULL);
        assert_run_sql_non_select (cnc, "CREATE table countries (code string not null primary key, name string not null)", NULL);

	/* table "misc" */
        assert_run_sql_non_select (cnc, "DROP table IF EXISTS misc", NULL);
        assert_run_sql_non_select (cnc, "CREATE table misc (ts timestamp, adate date, atime time)", NULL);

        return cnc;
}

static GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql, gboolean iter_only, GError **error)
{
	GdaStatement *stmt;
	GdaDataModel *res;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);

	res = gda_connection_statement_execute_select_full (cnc, stmt, NULL,
							    iter_only ? GDA_STATEMENT_MODEL_CURSOR_FORWARD :
							    GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							    NULL, error);
        g_object_unref (stmt);
	return res;
}


static gboolean
run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params, GError **error)
{
	GdaStatement *stmt;
        gint nrows;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        nrows = gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, error);
        g_object_unref (stmt);
        if (nrows == -1)
		return FALSE;
	else
		return TRUE;
}

static void
assert_data_model_equal (GdaDataModel *model, GdaDataModel *ref)
{
	GdaDataComparator *comp;
	GError *error = NULL;
	comp = GDA_DATA_COMPARATOR (gda_data_comparator_new (ref, model));
	if (! gda_data_comparator_compute_diff (comp, &error)) {
		g_print ("Error comparing data models: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	if (gda_data_comparator_get_n_diffs (comp) > 0) {
		g_print ("Data models differ!\n");
		g_print ("Expected:\n");
		gda_data_model_dump (ref, NULL);
		g_print ("Got:\n");
		gda_data_model_dump (model, NULL);
		exit (1);
	}
	g_object_unref (comp);
}

static GdaDataModel *
assert_run_sql_select (GdaConnection *cnc, const gchar *sql, Action action, const gchar *compare_file)
{
	GError *error = NULL;
	GdaDataModel *model = run_sql_select (cnc, sql, FALSE, &error);
	if (!model) {
		g_print ("Error executing [%s]: %s\n",
			 sql,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	gda_data_model_dump (model, NULL);
	if (compare_file) {
		gchar *file;
		file = g_build_filename (CHECK_FILES, "tests", "data-models", compare_file, NULL);
		if (action == ACTION_EXPORT) {
			if (! gda_data_model_export_to_file (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							     file,
							     NULL, 0, NULL, 0, NULL, &error)) {
				g_print ("Error exporting to file '%s': %s\n",
					 file,
					 error && error->message ? error->message : "No detail");
				exit (1);
			}
			g_print ("Generated '%s'\n", file);
		}
		else if (action == ACTION_COMPARE) {
			GdaDataModel *ref;
			ref = load_from_file (compare_file);
			assert_data_model_equal (model, ref);
			g_object_unref (ref);
		}
		else
			g_assert_not_reached ();
		g_free (file);
	}

	return model;
}

static void
assert_run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params)
{
	GError *error = NULL;
	if (! run_sql_non_select (cnc, sql, params, &error)) {
		g_print ("Error executing [%s]: %s\n",
			 sql,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
}

GdaDataModel *
load_from_file (const gchar *filename)
{
	GdaDataModel *model;
	gchar *file;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", filename, NULL);
	model = gda_data_model_import_new_file (file, TRUE, NULL);
	if (gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model))) {
		g_print ("Error loading file '%s'\n", file);
		exit (1);
	}
	g_free (file);
	return model;
}

static void
move_iter_forward (GdaDataModelIter *iter, const gchar *iter_name, gint nb, GdaDataModel *ref, gint start_row)
{
	gint i;
	for (i = 0; i < nb; i++) {
#ifdef DEBUG_PRINT
		g_print ("*** moving iter %s forward... ", iter_name);
#endif
		if (! gda_data_model_iter_move_next (iter)) {
			g_print ("Could not move forward at step %d\n", i);
			exit (1);
		}
		else {
			const GValue *cvalue;
			cvalue = gda_data_model_iter_get_value_at (iter, 0);
#ifdef DEBUG_PRINT
			gchar *str;
			str = gda_value_stringify (cvalue);
			g_print ("Col0=[%s]", str);
			g_free (str);
#endif

			if (ref) {
				if (gda_data_model_iter_get_row (iter) != (start_row + i)) {
					g_print (" Wrong reported row %d instead of %d\n",
						 gda_data_model_iter_get_row (iter),  start_row + i);
					exit (1);
				}
				const GValue *rvalue;
				rvalue = gda_data_model_get_value_at (ref, 0, start_row + i, NULL);
				g_assert (rvalue);
				gchar *str1, *str2;
				str1 = gda_value_stringify (cvalue);
				str2 = gda_value_stringify (rvalue);
				if (strcmp (str1, str2)) {
					g_print (" Wrong reported value [%s] instead of [%s]\n",
						 str1, str2);
					exit (1);
				}
#ifdef DEBUG_PRINT
				else
					g_print (" Value Ok.");
#endif
				g_free (str1);
				g_free (str2);
			}
#ifdef DEBUG_PRINT
			g_print ("\n");
#endif
		}
	}
}

static void
check_simultanous_select_random (GdaConnection *virtual)
{
	GdaDataModel *m1, *m2;
	GdaDataModel *refA = NULL, *refB = NULL;
	GdaDataModelIter *iter1, *iter2;
	GError *error = NULL;

	g_print ("*** simultaneous SELECT RANDOM 1\n");
	m1 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'A%' ORDER BY code",
			     FALSE, &error);
	if (!m1) {
		g_print ("Could not execute SELECT (1): %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	g_print ("*** simultaneous SELECT RANDOM 2\n");
	m2 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'B%' ORDER BY code",
			     FALSE, &error);
	if (!m2) {
		g_print ("Could not execute SELECT (2): %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	gda_data_model_dump (m2, NULL);
	gda_data_model_dump (m1, NULL);

/*#define EXPORT*/
#ifdef EXPORT
	gchar *file;
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "countriesA.xml", NULL);
	if (! gda_data_model_export_to_file (m1, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
					     file,
					     NULL, 0, NULL, 0, NULL, &error)) {
		g_print ("Error exporting to file '%s': %s\n",
			 file,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("Generated '%s'\n", file);
	g_free (file);

	file = g_build_filename (CHECK_FILES, "tests", "data-models", "countriesB.xml", NULL);
	if (! gda_data_model_export_to_file (m2, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
					     file,
					     NULL, 0, NULL, 0, NULL, &error)) {
		g_print ("Error exporting to file '%s': %s\n",
			 file,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("Generated '%s'\n", file);
	g_free (file);
#else
	refA = load_from_file ("countriesA.xml");
	assert_data_model_equal (m1, refA);

	refB = load_from_file ("countriesB.xml");
	assert_data_model_equal (m2, refB);
#endif

	iter1 = gda_data_model_create_iter (m1);
	g_print ("*** simultaneous iter 1 %p\n", iter1);

	iter2 = gda_data_model_create_iter (m2);
	g_print ("*** simultaneous iter 2 %p\n", iter2);

	move_iter_forward (iter1, "iter1", 10, refA, 0);
	move_iter_forward (iter2, "iter2", 3, refB, 0);
	move_iter_forward (iter1, "iter1", 3, refA, 10);
	move_iter_forward (iter2, "iter2", 2, refB, 3);

	g_object_unref (iter1);
	g_object_unref (iter2);

	g_object_unref (m1);
	g_object_unref (m2);
#ifndef EXPORT
	g_object_unref (refA);
	g_object_unref (refB);
#endif
}

static void
check_simultanous_select_forward (GdaConnection *virtual)
{
	GdaDataModel *m1, *m2;
	GdaDataModel *refA, *refB;
	GdaDataModelIter *iter1, *iter2;
	GError *error = NULL;

	g_print ("*** simultaneous SELECT FORWARD 1\n");
	m1 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'A%' ORDER BY code",
			     TRUE, &error);
	if (!m1) {
		g_print ("Could not execute SELECT with forward iter only (1): %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	g_print ("*** simultaneous SELECT FORWARD 2\n");
	m2 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'B%' ORDER BY code",
			     TRUE, &error);
	if (!m2) {
		g_print ("Could not execute SELECT with forward iter only (2): %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	refA = load_from_file ("countriesA.xml");
	refB = load_from_file ("countriesB.xml");

	iter1 = gda_data_model_create_iter (m1);
	g_print ("*** simultaneous iter 1 %p\n", iter1);

	iter2 = gda_data_model_create_iter (m2);
	g_print ("*** simultaneous iter 2 %p\n", iter2);

	move_iter_forward (iter1, "iter1", 10, refA, 0);
	if (gda_data_model_iter_move_prev (iter1)) {
		g_print ("Iter should not be allowed to move backward!\n");
                exit (1);
	}
	move_iter_forward (iter2, "iter2", 3, refB, 0);
	move_iter_forward (iter1, "iter1", 3, refA, 10);
	move_iter_forward (iter2, "iter2", 2, refB, 3);

	g_object_unref (iter1);
	g_object_unref (iter2);

	g_object_unref (refA);
	g_object_unref (refB);

	g_object_unref (m1);
	g_object_unref (m2);
}


static void
check_update_delete (GdaConnection *virtual)
{
	/* Check DELETE and UPDATE */
	g_print ("*** Copying data into virtual 'cities' table...\n");
	assert_run_sql_non_select (virtual, "INSERT INTO out.cities SELECT * FROM city WHERE population >= 500000", NULL);
	g_print ("*** Showing list of cities WHERE population >= 1000000\n");
	assert_run_sql_select (virtual, "SELECT * FROM out.cities WHERE population >= 1000000 "
			       "ORDER BY name", ACTION_COMPARE, "cities1.xml");

	g_print ("*** Deleting data where population < 2000000...\n");
	assert_run_sql_non_select (virtual, "DELETE FROM out.cities WHERE population < 2000000", NULL);

	g_print ("*** Showing (shorter) list of cities WHERE population >= 1000000\n");
	assert_run_sql_select (virtual, "SELECT * FROM out.cities WHERE population >= 1000000 "
			       "ORDER BY name", ACTION_COMPARE, "cities2.xml");

	g_print ("*** Updating data where population > 3000000...\n");
	assert_run_sql_non_select (virtual, "UPDATE out.cities SET population = 3000000 WHERE "
				   "population >= 3000000", NULL);
	
	g_print ("*** Showing list of cities WHERE population >= 2100000\n");
	assert_run_sql_select (virtual, "SELECT * FROM out.cities WHERE population >= 2100000 "
			       "ORDER BY name", ACTION_COMPARE, "cities3.xml");
}

typedef struct {
	GThread  *thread;
	gint      th_id;
	GdaConnection *virtual;
} ThData;

static gboolean
test_multiple_threads (GThreadFunc func, GdaConnection *virtual)
{
	ThData data[NTHREADS];
	gint i;

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		d->th_id = i;
		d->virtual = virtual;
	}

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
#ifdef DEBUG_PRINT
		g_print ("Running thread %d\n", d->th_id);
#endif
		d->thread = g_thread_new ("For_test_multiple_threads", func, d);
	}

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		g_thread_join (d->thread);
	}
	
	g_print ("All threads finished\n");
	return TRUE;
}

/* executed in another thread */
gpointer
threads_select_random_start_thread (ThData *data)
{
	GdaDataModel *model;
	GdaDataModel *ref;
	GdaDataModelIter *iter;
	GError *error = NULL;
	GThread *self;
	gchar *str;

	self = g_thread_self ();
	g_print ("*** sub thread %p SELECT RANDOM\n", self);
	model = run_sql_select (data->virtual, "SELECT * FROM countries WHERE code LIKE 'A%' ORDER BY code",
				FALSE, &error);
	if (!model) {
		g_print ("Could not execute SELECT with forward iter only: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	ref = load_from_file ("countriesA.xml");
	assert_data_model_equal (model, ref);

	iter = gda_data_model_create_iter (model);

	str = g_strdup_printf ("iter thread %p", self);
	move_iter_forward (iter, str, 10, ref, 0);
	move_iter_forward (iter, str, 3, ref, 10);
	g_free (str);

	g_object_unref (iter);
	g_object_unref (model);
	g_object_unref (ref);
	g_print ("thread %p finished\n", self);

	return NULL;
}

static void
check_threads_select_random (GdaConnection *virtual)
{
	test_multiple_threads ((GThreadFunc) threads_select_random_start_thread, virtual);
}

static void
check_date (GdaConnection *virtual)
{
	g_print ("*** insert dates into 'misc' table...\n");
	GdaSet *set;
	GdaTimestamp *ts = gda_timestamp_new ();
	gda_timestamp_set_year (ts, 2011);
	gda_timestamp_set_month (ts, 01);
	gda_timestamp_set_day (ts, 31);
	gda_timestamp_set_hour (ts, 12);
	gda_timestamp_set_minute (ts, 34);
	gda_timestamp_set_second (ts, 56);
	gda_timestamp_set_fraction (ts, 0);
	gda_timestamp_set_timezone (ts, 0);
	GdaTime* atime = gda_time_new_from_values (13, 45, 59, 0, 0);
	GDate *adate;
	GdaDataModel *model;
	GError *error = NULL;

	adate = g_date_new_dmy (23, G_DATE_FEBRUARY, 2010);
	set = gda_set_new_inline (3,
				  "ts", GDA_TYPE_TIMESTAMP, ts,
				  "adate", G_TYPE_DATE, adate,
				  "atime", GDA_TYPE_TIME, atime);
	g_date_free (adate);

	assert_run_sql_non_select (virtual, "INSERT INTO out.misc VALUES (##ts::timestamp, "
				   "##adate::date, ##atime::time)", set);

	g_print ("*** Showing contents of 'misc'\n");
	model = assert_run_sql_select (virtual, "SELECT * FROM out.misc",
				       ACTION_EXPORT, NULL);
	const GValue *cvalue, *exp;
	cvalue = gda_data_model_get_value_at (model, 0, 0, &error);
	if (! cvalue) {
		g_print ("Could not get timestamp value: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	exp = gda_set_get_holder_value (set, "ts");
	if (gda_value_differ (cvalue, exp)) {
		g_print ("Expected value '%s', got '%s'\n",
			 gda_value_stringify (exp), gda_value_stringify (cvalue));
		exit (1);
	}

	cvalue = gda_data_model_get_value_at (model, 1, 0, &error);
	if (! cvalue) {
		g_print ("Could not get timestamp value: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	exp = gda_set_get_holder_value (set, "adate");
	if (gda_value_differ (cvalue, exp)) {
		g_print ("Expected value '%s', got '%s'\n",
			 gda_value_stringify (exp), gda_value_stringify (cvalue));
		exit (1);
	}

	cvalue = gda_data_model_get_value_at (model, 2, 0, &error);
	if (! cvalue) {
		g_print ("Could not get timestamp value: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	exp = gda_set_get_holder_value (set, "atime");
	if (gda_value_differ (cvalue, exp)) {
		g_print ("Expected value '%s', got '%s'\n",
			 gda_value_stringify (exp), gda_value_stringify (cvalue));
		exit (1);
	}

	g_object_unref (set);
	gda_timestamp_free (ts);
}
