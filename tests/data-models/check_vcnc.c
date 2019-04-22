/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2017,2018 Daniel Espinosa <esodan@gmail.com>
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
#include <gio/gio.h>

#define NTHREADS 50

typedef enum {
	ACTION_COMPARE,
	ACTION_EXPORT
} Action;

static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql,
				     gboolean iter_only, GError **error);
static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params, GError **error);
static GdaDataModel *assert_run_sql_select (GdaConnection *cnc, const gchar *sql,
					    Action action, const gchar *compare_file, GError **error);
static gboolean assert_run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params, GError **error);
GdaDataModel *load_from_file (const gchar *filename);
static gboolean assert_data_model_equal (GdaDataModel *model, GdaDataModel *ref, GError **error);

typedef struct {
  gint fails;
  GMainLoop *loop;
  GFile *fdb;
} Data;

static GdaConnection *open_destination_connection (Data *data, GError **error);
static gboolean check_update_delete (GdaConnection *virtual, GError **error);
static gboolean check_simultanous_select_random (GdaConnection *virtual, GError **error);
static gboolean check_simultanous_select_forward (GdaConnection *virtual, GError **error);
static void check_threads_select_random (GdaConnection *virtual);
static gboolean check_date (GdaConnection *virtual, GError **error);


static gboolean test1 (Data *data);

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
  GMainLoop *loop;
  gda_init ();
  loop = g_main_loop_new (NULL, FALSE);
  Data *data = g_new0(Data, 1);
  data->fails = 0;
  data->loop = loop;
  g_idle_add ((GSourceFunc) test1, data);
  g_main_loop_run (loop);
  if (data->fails != 0)
    g_print ("FAIL %d\n", data->fails);
  else
    g_print ("All Ok\n");
  g_main_loop_unref (loop);
  if (G_IS_OBJECT (data->fdb)) {
    if (g_file_query_exists (data->fdb, NULL)) {
      g_file_delete (data->fdb, NULL, NULL);
      g_object_unref (data->fdb);
    }
  }
  g_free (data);
  return 0;
}

static gboolean
test1 (Data *data) {
	GError *error = NULL;	
	GdaConnection *virtual, *out_cnc;
	GdaVirtualProvider *provider;
	gchar *file;

	provider = gda_vprovider_hub_new ();
	virtual = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_assert (virtual);
	g_assert (GDA_IS_VCONNECTION_HUB (virtual));

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

	g_message ("Adding data models to virtal connection as inputs");
	g_message ("Add data models to virtual connection: city");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual),
						   city_model, "city", &error)) {
		g_message ("Add city model error: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}
	g_message ("Add data models to virtual connection: country");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual),
						   country_model, "country", &error)) {
		g_message ("Add country model error: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}
	// Test data models in virtal connection
	GdaDataModel *datamodel;
	datamodel = gda_connection_execute_select_command (virtual, "SELECT * FROM city", &error);
	if (datamodel == NULL) {
		g_message ("Select data from city model error: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}
	gchar *dump = NULL;
	dump = gda_data_model_dump_as_string (datamodel);
	g_message ("Selected data from imported data model city:\n%s", dump);
	g_free (dump);
	g_object_unref (datamodel);

	datamodel = gda_connection_execute_select_command (virtual, "SELECT * FROM country", &error);
	if (datamodel == NULL) {
		g_message ("Select data from city model error: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}
	dump = gda_data_model_dump_as_string (datamodel);
	g_message ("Selected data from imported data model country:\n%s", dump);
	g_free (dump);
	g_object_unref (datamodel);

	/* SQLite connection for outputs */
	g_message ("Open SQLite connection for outputs");
	out_cnc = open_destination_connection (data, &error);
	if (out_cnc == NULL) {
		g_message ("Error opening destination %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}

	/* adding connections to the virtual connection */
	g_message ("Add output connection to virtual connection hub");
	if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual), out_cnc, "out", &error)) {
		g_message ("Could not add connection to virtual connection: %s",
			error && error->message ? error->message : "No detail");
		g_clear_error (&error);
    data->fails += 1;
    g_main_loop_quit (data->loop);
    return G_SOURCE_REMOVE;
  }

	g_message ("Update/Delete check");
	if (!check_update_delete (virtual, &error)) {
		g_message ("Error at update-delete: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}

	g_message ("*** Copying data into 'countries' virtual table...\n");
	if (!assert_run_sql_non_select (virtual, "INSERT INTO out.countries SELECT * FROM country", NULL, &error)) {
		g_message ("Error: Check Siumultaneos select random: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}

	g_message ("Check simultaneous select ramdom");
	if (!check_simultanous_select_random (virtual, &error)) {
		g_message ("Error: Check Siumultaneos select random: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}
	g_message ("Check simultaneous select forward");
	if (!check_simultanous_select_forward (virtual, &error)) {
		g_message ("Error: Check Simultaneos select forward: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}

	if (!check_date (virtual, &error)) {
		g_message ("Error: Check Date: %s", error && error->message ? error->message : "No detail");
    data->fails += 1;
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}

	g_message ("Check Threads select ramdom");
	check_threads_select_random (virtual);


  if (! gda_connection_close (virtual, &error)) {
		g_message ("gda_connection_close(virtual) error: %s", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
    data->fails += 1;
    g_main_loop_quit (data->loop);
    return G_SOURCE_REMOVE;
	}
  if (! gda_connection_close (out_cnc, &error)) {
		g_message ("gda_connection_close(out_cnc) error: %s", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
    data->fails += 1;
    g_main_loop_quit (data->loop);
    return G_SOURCE_REMOVE;
  }
  g_main_loop_quit (data->loop);
  return G_SOURCE_REMOVE;
}

static gchar*
create_connection_string (Data *data) {
  GFile *d;
  GRand *rand;
  gint i;

  rand = g_rand_new ();
  gchar** envp = g_get_environ ();
  const gchar* build_dir = g_environ_getenv (envp, "GDA_TOP_BUILD_DIR");
  gchar *sd = g_strdup_printf ("file://%s/tests/data-models", build_dir);
  g_strfreev (envp);
  d = g_file_new_for_uri (sd);
  g_assert (g_file_query_exists (d, NULL));
  i = g_rand_int (rand);
  gchar *sdb = g_strdup_printf ("%s/vcnc%d.db", sd, i);
  data->fdb = g_file_new_for_uri (sdb);

  gchar *cstr = g_strdup_printf ("DB_DIR=%s;DB_NAME=vcnc%d", sd, i);
  g_free (sd);
  g_print ("Connecting using: %s\n", cstr);
  g_object_unref (d);
  return cstr;
}

GdaConnection *
open_destination_connection (Data *data, GError **error)
{
	/* create connection */
	GdaConnection *cnc;

	gchar *cstr = create_connection_string (data);
	cnc = gda_connection_new_from_string ("SQLite", cstr,
					      NULL,
					      GDA_CONNECTION_OPTIONS_NONE,
					      error);
	if (cnc == NULL) {
		return NULL;
	}
        g_free (cstr);
        if (!cnc) {
                g_message ("Could not create connection to local SQLite database:");
                return NULL;
        }
	if (! gda_connection_open (cnc, error)) {
		g_message ("gda_connection_open() error");
		g_object_unref (cnc);
		return NULL;
	}

	/* table "cities" */
	g_message ("Initializing DB");
	if (!assert_run_sql_non_select (cnc, "DROP table IF EXISTS cities", NULL, error)) {
		g_message ("Error drop table cities");
		g_object_unref (cnc);
		return NULL;
	}
	if (!assert_run_sql_non_select (cnc, "CREATE table cities (name string not NULL primary key, countrycode string not null, population int)", NULL, error)) {
		g_message ("Error create table cities");
		g_object_unref (cnc);
		return NULL;
	}

	/* table "countries" */
	if (!assert_run_sql_non_select (cnc, "DROP table IF EXISTS countries", NULL, error)) {
		g_message ("Error drop table countries");
		g_object_unref (cnc);
		return NULL;
	}
	if (!assert_run_sql_non_select (cnc, "CREATE table countries (code string not null primary key, name string not null)", NULL, error)) {
		g_message ("Error create table countries");
		g_object_unref (cnc);
		return NULL;
	}

	/* table "misc" */
	if (!assert_run_sql_non_select (cnc, "DROP table IF EXISTS misc", NULL, error)) {
		g_message ("Error drop table misc");
		g_object_unref (cnc);
		return NULL;
	}
	if (!assert_run_sql_non_select (cnc, "CREATE table misc (ts timestamp, adate date, atime time)", NULL, error)) {
		g_message ("Error create table misc");
		g_object_unref (cnc);
		return NULL;
	}

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

  stmt = gda_connection_parse_sql_string (cnc, sql, NULL, error);

  nrows = gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, error);
  g_object_unref (stmt);
  if (nrows == -1)
  return FALSE;
  return TRUE;
}

static gboolean
assert_data_model_equal (GdaDataModel *model, GdaDataModel *ref, GError **error)
{
	GdaDataComparator *comp;
	comp = GDA_DATA_COMPARATOR (gda_data_comparator_new (ref, model));
	if (! gda_data_comparator_compute_diff (comp, error)) {
		g_message ("Error comparing data models");
		return FALSE;
	}
	if (gda_data_comparator_get_n_diffs (comp) > 0) {
		g_print ("Data models differ!\n");
		g_print ("Expected:\n");
		gda_data_model_dump (ref, NULL);
		g_print ("Got:\n");
		gda_data_model_dump (model, NULL);
		return FALSE;
	}
	g_object_unref (comp);
	return TRUE;
}

static GdaDataModel *
assert_run_sql_select (GdaConnection *cnc, const gchar *sql,
		       Action action, const gchar *compare_file,
		       GError **error)
{
	GdaDataModel *model = run_sql_select (cnc, sql, FALSE, error);
	if (!model) {
		return NULL;
	}

	gda_data_model_dump (model, NULL);
	if (compare_file) {
		gchar *file;
		file = g_build_filename (CHECK_FILES, "tests", "data-models", compare_file, NULL);
		if (action == ACTION_EXPORT) {
			if (! gda_data_model_export_to_file (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							     file,
							     NULL, 0, NULL, 0, NULL, error)) {
				g_message ("Error exporting to file '%s'", file);
				return NULL;
			}
			g_print ("Generated '%s'\n", file);
		}
		else if (action == ACTION_COMPARE) {
			GdaDataModel *ref;
			ref = load_from_file (compare_file);
			if (!assert_data_model_equal (model, ref, error)) {
				g_object_unref (ref);
				return NULL;
			}
			g_object_unref (ref);
		}
		else
			g_assert_not_reached ();
		g_free (file);
	}

	return model;
}

static gboolean
assert_run_sql_non_select (GdaConnection *cnc, const gchar *sql, GdaSet *params, GError **error)
{
	return run_sql_non_select (cnc, sql, params, error);
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
move_iter_forward (GdaDataModelIter *iter, G_GNUC_UNUSED const gchar *iter_name, gint nb, GdaDataModel *ref, gint start_row)
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

static gboolean
check_simultanous_select_random (GdaConnection *virtual, GError **error)
{
	GdaDataModel *m1, *m2;
	GdaDataModel *refA = NULL, *refB = NULL;
	GdaDataModelIter *iter1, *iter2;

	g_message ("*** simultaneous SELECT RANDOM 1\n");
	m1 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'A%' ORDER BY code",
			     FALSE, error);
	if (!m1) {
		g_message ("Could not execute SELECT (1)");
                return FALSE;
	}

	g_print ("*** simultaneous SELECT RANDOM 2\n");
	m2 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'B%' ORDER BY code",
			     FALSE, error);
	if (!m2) {
		g_message ("Could not execute SELECT (2)");
                return FALSE;
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
		g_message ("Error exporting to file '%s'", file);
		return FALSE;
	}
	g_print ("Generated '%s'\n", file);
	g_free (file);

	file = g_build_filename (CHECK_FILES, "tests", "data-models", "countriesB.xml", NULL);
	if (! gda_data_model_export_to_file (m2, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
					     file,
					     NULL, 0, NULL, 0, NULL, error)) {
		g_message ("Error exporting to file '%s'", file);
		return FALSE;
	}
	g_print ("Generated '%s'\n", file);
	g_free (file);
#else
	refA = load_from_file ("countriesA.xml");
	if (!assert_data_model_equal (m1, refA, error)) {
		g_message ("No equial m1, refA");
		return FALSE;
	}

	refB = load_from_file ("countriesB.xml");
	if (!assert_data_model_equal (m2, refB, error)) {
		g_message ("No equial m1, refA");
		return FALSE;
	}
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
	return TRUE;
}

static gboolean
check_simultanous_select_forward (GdaConnection *virtual, GError **error)
{
	GdaDataModel *m1, *m2;
	GdaDataModel *refA, *refB;
	GdaDataModelIter *iter1, *iter2;

	g_print ("*** simultaneous SELECT FORWARD 1\n");
	m1 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'A%' ORDER BY code",
			     TRUE, error);
	if (!m1) {
		g_message ("Could not execute SELECT with forward iter only (1)");
                return FALSE;
	}

	g_print ("*** simultaneous SELECT FORWARD 2\n");
	m2 = run_sql_select (virtual, "SELECT * FROM countries WHERE code LIKE 'B%' ORDER BY code",
			     TRUE, error);
	if (!m2) {
		g_print ("Could not execute SELECT with forward iter only (2)");
		return FALSE;
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
	return TRUE;
}


static gboolean
check_update_delete (GdaConnection *virtual, GError **error)
{
	/* Check DELETE and UPDATE */
	g_print ("*** Copying data into virtual 'cities' table...\n");
	if (!assert_run_sql_non_select (virtual,
			"INSERT INTO out.cities SELECT * FROM city WHERE population >= 500000",
			NULL, error)) {
		return FALSE;
	}
	g_print ("*** Showing list of cities WHERE population >= 1000000\n");
	if (!assert_run_sql_select (virtual,
			"SELECT * FROM out.cities WHERE population >= 1000000 ORDER BY name",
			ACTION_COMPARE, "cities1.xml", error)) {
		return FALSE;
	}

	g_print ("*** Deleting data where population < 2000000...\n");
	if (!assert_run_sql_non_select (virtual, "DELETE FROM out.cities WHERE population < 2000000",
					NULL, error)) {
		return FALSE;
	}

	g_print ("*** Showing (shorter) list of cities WHERE population >= 1000000\n");
	if (!assert_run_sql_select (virtual,
			"SELECT * FROM out.cities WHERE population >= 1000000 ORDER BY name",
			ACTION_COMPARE, "cities2.xml", error)) {
		return FALSE;
	}

	g_print ("*** Updating data where population > 3000000...\n");
	if (!assert_run_sql_non_select (virtual, "UPDATE out.cities SET population = 3000000 WHERE "
				   "population >= 3000000", NULL, error)) {
		return FALSE;
	}
	
	g_print ("*** Showing list of cities WHERE population >= 2100000\n");
	if (!assert_run_sql_select (virtual, "SELECT * FROM out.cities WHERE population >= 2100000 "
			       "ORDER BY name", ACTION_COMPARE, "cities3.xml", error)) {
		return FALSE;
	}
	return TRUE;
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
		g_message ("Running thread %d\n", d->th_id);
		d->thread = g_thread_new ("For_test_multiple_threads", func, d);
	}

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		g_thread_join (d->thread);
	}
	
	g_message ("All threads finished\n");
	return TRUE;
}

/* executed in another thread */
gpointer
threads_select_random_start_thread (ThData *data)
{
	GdaDataModel *model;
	GdaDataModel *ref;
	GdaDataModelIter *iter;
	GThread *self;
	gchar *str;
  GError *error = NULL;

	self = g_thread_self ();
	g_message ("*** sub thread %p SELECT RANDOM\n", self);
	model = run_sql_select (data->virtual, "SELECT * FROM countries WHERE code LIKE 'A%' ORDER BY code",
				FALSE, &error);
	if (!model) {
		g_message ("Could not execute SELECT with forward iter only: %s",
               error != NULL && error->message != NULL ? error->message : "No Datail");
    g_clear_error (&error);
		return NULL;
	}

	ref = load_from_file ("countriesA.xml");
	if (!assert_data_model_equal (model, ref, &error)) {
		g_message ("Error: equal model vs ref %s",
               error != NULL && error->message != NULL ? error->message : "No Datail");
    g_clear_error (&error);
		return NULL;
	}

	iter = gda_data_model_create_iter (model);

	str = g_strdup_printf ("iter thread %p", self);
	move_iter_forward (iter, str, 10, ref, 0);
	move_iter_forward (iter, str, 3, ref, 10);
	g_free (str);

	g_object_unref (iter);
	g_object_unref (ref);
	g_object_unref (model);
	g_message ("thread %p finishing\n", self);

	return NULL;
}

static void
check_threads_select_random (GdaConnection *virtual)
{
	g_message ("Thread test start");
	test_multiple_threads ((GThreadFunc) threads_select_random_start_thread, virtual);
}

static gboolean
check_date (GdaConnection *virtual, GError **error)
{
	g_print ("*** insert dates into 'misc' table...\n");
	GdaSet *set;
	GDateTime *ts = g_date_time_new_utc (2011, 01, 31, 12, 34, 56.0);
	gchar* tss = g_date_time_format ((GDateTime*) ts, "%FT%T");
	g_print ("Created Timestamp: %s\n", tss);
	g_free (tss);
	GdaTime* atime = gda_time_new_from_values (13, 45, 59, 0, 0);
	GDate *adate;
	GdaDataModel *model;

	adate = g_date_new_dmy (23, G_DATE_FEBRUARY, 2010);
	set = gda_set_new_inline (3,
				  "ts", G_TYPE_DATE_TIME, ts,
				  "adate", G_TYPE_DATE, adate,
				  "atime", GDA_TYPE_TIME, atime);
	g_date_free (adate);

	if (!assert_run_sql_non_select (virtual,
			"INSERT INTO out.misc VALUES (##ts::timestamp, ##adate::date, ##atime::time)",
			set, error)) {
		return FALSE;
	}

	g_print ("*** Showing contents of 'misc'\n");
	model = assert_run_sql_select (virtual, "SELECT * FROM out.misc",
				       ACTION_EXPORT, NULL, error);
	if (model == NULL) {
		return FALSE;
	}
	const GValue *cvalue, *exp;
	cvalue = gda_data_model_get_value_at (model, 0, 0, error);
	if (! cvalue) {
		g_warning ("Could not get timestamp value");
		return FALSE;
	}
	exp = gda_set_get_holder_value (set, "ts");
	if (gda_value_differ (cvalue, exp)) {
		g_warning ("Expected Timestamp value '%s', got '%s'\n",
		    gda_value_stringify (exp), gda_value_stringify (cvalue));
		return FALSE;
	}

	cvalue = gda_data_model_get_value_at (model, 1, 0, error);
	if (! cvalue) {
		g_warning ("Could not get timestamp value");
    return FALSE;
	}
	exp = gda_set_get_holder_value (set, "adate");
	if (gda_value_differ (cvalue, exp)) {
		g_warning ("Expected Date value '%s', got '%s'\n",
		    gda_value_stringify (exp), gda_value_stringify (cvalue));
		return FALSE;
	}

	cvalue = gda_data_model_get_value_at (model, 2, 0, error);
	if (! cvalue) {
		g_warning ("Could not get timestamp value");
    return FALSE;
	}
	exp = gda_set_get_holder_value (set, "atime");
	if (gda_value_differ (cvalue, exp)) {
		g_warning ("Expected Time value '%s', got '%s'\n",
		    gda_value_stringify (exp), gda_value_stringify (cvalue));
		return FALSE;
	}

	g_object_unref (set);
	g_date_time_unref (ts);
	return TRUE;
}
