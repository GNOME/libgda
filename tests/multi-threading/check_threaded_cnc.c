/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <stdio.h>
#include <string.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <thread-wrapper/gda-thread-recordset.h>
#include "common.h"

#define DBDIR "."

static void test_cnc_obj (GdaConnection *tcnc);
static void test_cnc_status (GdaConnection *tcnc, gboolean opened);

static gint test_select (GdaConnection *cnc, GdaConnection *tcnc);
static gint test_select_cursor (GdaConnection *cnc, GdaConnection *tcnc);
static gint test_insert (GdaConnection *tcnc);
static gint test_update (GdaConnection *tcnc);
static gint test_delete (GdaConnection *tcnc);
static gint test_meta_store (GdaConnection *cnc);
static gint test_meta_data (GdaConnection *cnc, GdaConnection *tcnc);
static gint test_signals (GdaConnection *cnc, GdaConnection *tcnc);

int 
main (int argc, char** argv)
{
	GdaConnection *cnc, *tcnc;
        GError *error = NULL;
	gint failures = 0;
	gint ntests = 0;
	gchar *fname;

        gda_init ();

	/* create test DB */
	fname = g_build_filename (ROOT_DIR, "tests", "multi-threading", "testdb.sql", NULL);
        if (!create_sqlite_db (DBDIR, "testdb", fname, &error)) {
                g_print ("Cannot create test database: %s\n", error && error->message ?
                         error->message : "no detail");
                return 1;
        }
        g_free (fname);


        /* create a connection in threaded mode */
        cnc = gda_connection_open_from_string ("SQlite", "DB_DIR=.;DB_NAME=testdb", NULL,
					       GDA_CONNECTION_OPTIONS_NONE, &error);
        if (!cnc) {
                g_print ("ERROR opening connection: %s\n",
                         error && error->message ? error->message : "No detail");
                return 1;
        }
        tcnc = gda_connection_open_from_string ("SQlite", "DB_DIR=.;DB_NAME=testdb", NULL,
					       GDA_CONNECTION_OPTIONS_THREAD_ISOLATED, &error);
        if (!tcnc) {
                g_print ("ERROR opening connection in thread safe mode: %s\n",
                         error && error->message ? error->message : "No detail");
                return 1;
        }

	test_cnc_obj (tcnc);
	test_cnc_status (tcnc, TRUE);
        gda_connection_close (tcnc);
	test_cnc_status (tcnc, FALSE);
        gda_connection_open (tcnc, NULL);
	test_cnc_status (tcnc, TRUE);

	ntests ++;
        failures += test_select (cnc, tcnc);

	ntests ++;
        failures += test_select_cursor (cnc, tcnc);

	ntests ++;
        failures += test_insert (tcnc);
        failures += test_select (cnc, tcnc);
        failures += test_select_cursor (cnc, tcnc);

	ntests ++;
        failures += test_update (tcnc);
        failures += test_select (cnc, tcnc);
        failures += test_select_cursor (cnc, tcnc);

	ntests ++;
        failures += test_delete (tcnc);
        failures += test_select (cnc, tcnc);
        failures += test_select_cursor (cnc, tcnc);

	ntests ++;
        failures += test_meta_data (cnc, tcnc);

	ntests ++;
        failures += test_meta_store (cnc);

	ntests ++;
	failures += test_signals (cnc, tcnc);

        /* get rid of connection */
        g_object_unref (cnc);
        g_object_unref (tcnc);
	g_usleep (300000);

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);
  
	return failures != 0 ? 1 : 0;
}

static void
test_cnc_obj (GdaConnection *tcnc)
{
	gboolean is_wrapper;
	g_object_get (G_OBJECT (tcnc), "is-wrapper", &is_wrapper, NULL);
	if (!is_wrapper) {
		g_print ("ERROR: connection is not a connection wrapper\n");
                exit (1);
	}
}

static void
test_cnc_status (GdaConnection *tcnc, gboolean opened)
{
	if (gda_connection_is_opened (tcnc) != opened) {
		g_print ("ERROR: connection is %s, expecting it %s\n",
                         gda_connection_is_opened (tcnc) ? "Opened" : "Closed",
                         opened ? "Opened" : "Closed");
                exit (1);
	}
}

static gint
test_select (GdaConnection *cnc, GdaConnection *tcnc)
{
	/* execute statement */
	gint failures = 0;
	GdaDataModel *model, *tmodel;
	
	model = run_sql_select (cnc, "SELECT * FROM person");
	tmodel = run_sql_select (tcnc, "SELECT * FROM person");
	if (! data_models_equal (model, tmodel))
		failures ++;

	if (strcmp (G_OBJECT_TYPE_NAME (tmodel), "GdaThreadRecordset")) {
		g_print ("ERROR: data model from SELECT's type is '%s' and not 'GdaThreadRecordset'\n",
                         G_OBJECT_TYPE_NAME (tmodel));
                exit (1);
	}
	if (gda_data_select_get_connection (GDA_DATA_SELECT (tmodel)) != tcnc) {
		g_print ("ERROR: %s() returned wrong result\n", "gda_data_select_get_connection");
		failures ++;
	}

	GdaDataModelIter *iter;
	GdaDataModelIter *titer;
	iter = gda_data_model_create_iter (model);
	titer = gda_data_model_create_iter (tmodel);
	if (!titer) {
		g_print ("ERROR: %s() returned NULL\n", "gda_data_model_create_iter");
		failures ++;
	}
	else {
		GdaDataModel *m1;
		g_object_get (G_OBJECT (titer), "data-model", &m1, NULL);
		if (m1 != tmodel) {
			g_print ("ERROR: \"data-model\" property of created iterator is wrong\n");
			failures ++;
		}
		g_object_unref (m1);

		/* check iter's contents */
		guint ncols;
		ncols = g_slist_length (GDA_SET (titer)->holders);
		if (ncols != g_slist_length (GDA_SET (iter)->holders)) {
			g_print ("ERROR: threaded iterator is at the wrong number of columns: "
				 "%d when it should be at %d\n",
				 ncols, g_slist_length (GDA_SET (iter)->holders));
			failures ++;
			ncols = -1;
		}
		else
			g_assert (ncols > 0);

		/* forward */
		if (gda_data_model_iter_is_valid (titer)) {
			g_print ("ERROR: threaded iterator is valid before any read\n");
			failures ++;
		}
		if (gda_data_model_iter_is_valid (iter)) {
			g_print ("ERROR: iterator is valid before any read\n");
			failures ++;
		}
		gint row;
		gboolean iterok;
		iterok = gda_data_model_iter_move_next (iter);
		iterok = gda_data_model_iter_move_next (titer) | iterok;
		for (row = 0; iterok; row++) {
			if (gda_data_model_iter_get_row (titer) != gda_data_model_iter_get_row (iter)) {
				g_print ("ERROR: threaded iterator is at the wrong row: %d when it should be at %d\n",
					 gda_data_model_iter_get_row (titer), gda_data_model_iter_get_row (iter));
				failures ++;
				break;
			}
			if (ncols > 0) {
				guint i;
				for (i = 0; i < ncols; i++) {
					const GValue *cv, *tcv;
					cv = gda_data_model_iter_get_value_at (iter, i);
					tcv = gda_data_model_iter_get_value_at (titer, i);
					if (gda_value_differ (cv, tcv)) {
						g_print ("ERROR: values in iterators differ at line %d, colunm %d\n",
							 row, i);
						failures ++;
					}
					//g_print ("%d,%d: %s\n", row, i, gda_value_stringify (tcv));
				}
			}
			gboolean v, tv;
			v = gda_data_model_iter_move_next (iter);
			tv = gda_data_model_iter_move_next (titer);
			if (!v || !tv)
				break;
		}
		if (gda_data_model_iter_is_valid (titer)) {
			g_print ("ERROR: threaded iterator is still valid\n");
			failures ++;
		}
		if (gda_data_model_iter_is_valid (iter)) {
			g_print ("ERROR: iterator is still valid\n");
			failures ++;
		}

		/* backward */
		for (; row >= 0; row--) {
			iterok = gda_data_model_iter_move_next (iter);
			iterok = gda_data_model_iter_move_next (titer) | iterok;
			if (!iterok)
				break;
		}
		for (row = 0; iterok; row++) {
			if (gda_data_model_iter_get_row (titer) != gda_data_model_iter_get_row (iter)) {
				g_print ("ERROR: threaded iterator is at the wrong row: %d when it should be at %d\n",
					 gda_data_model_iter_get_row (titer), gda_data_model_iter_get_row (iter));
				failures ++;
				break;
			}
			if (ncols > 0) {
				guint i;
				for (i = 0; i < ncols; i++) {
					const GValue *cv, *tcv;
					cv = gda_data_model_iter_get_value_at (iter, i);
					tcv = gda_data_model_iter_get_value_at (titer, i);
					if (gda_value_differ (cv, tcv)) {
						g_print ("ERROR: values in iterators differ at line %d, colunm %d\n",
							 row, i);
						failures ++;
					}
					//g_print ("%d,%d: %s\n", row, i, gda_value_stringify (tcv));
				}
			}
			gboolean v, tv;
			v = gda_data_model_iter_move_prev (iter);
			tv = gda_data_model_iter_move_prev (titer);
			if (!v || !tv)
				break;
		}
		if (gda_data_model_iter_is_valid (titer)) {
			g_print ("ERROR: threaded iterator is still valid\n");
			failures ++;
		}
		if (gda_data_model_iter_is_valid (iter)) {
			g_print ("ERROR: iterator is still valid\n");
			failures ++;
		}

		g_object_unref (iter);
		g_object_unref (titer);
	}

	g_object_unref (model);
	g_object_unref (tmodel);

	return failures;
}

static gint
test_select_cursor (GdaConnection *cnc, GdaConnection *tcnc)
{
	/* execute statement */
	gint failures = 0;
	GdaDataModel *model, *tmodel;
	
	model = run_sql_select_cursor (cnc, "SELECT * FROM person");
	tmodel = run_sql_select_cursor (tcnc, "SELECT * FROM person");

	if (gda_data_model_get_access_flags (model) !=
	    gda_data_model_get_access_flags (tmodel)) {
		g_print ("ERROR: data models' access flags differ: %d and %d for threaded model\n",
                         gda_data_model_get_access_flags (model),
			 gda_data_model_get_access_flags (tmodel));
		failures ++;
		return failures;
	}

	if (strcmp (G_OBJECT_TYPE_NAME (tmodel), "GdaThreadRecordset")) {
		g_print ("ERROR: data model from SELECT's type is '%s' and not 'GdaThreadRecordset'\n",
                         G_OBJECT_TYPE_NAME (tmodel));
                exit (1);
	}
	if (gda_data_select_get_connection (GDA_DATA_SELECT (tmodel)) != tcnc) {
		g_print ("ERROR: %s() returned wrong result\n", "gda_data_select_get_connection");
		failures ++;
	}

	GdaDataModelIter *iter, *titer;
	iter = gda_data_model_create_iter (model);
	titer = gda_data_model_create_iter (tmodel);
	if (!titer) {
		g_print ("ERROR: %s() returned NULL\n", "gda_data_model_create_iter");
		failures ++;
	}
	else {
		/* iter's properties */
		GdaDataModel *m1;
		g_object_get (G_OBJECT (titer), "data-model", &m1, NULL);
		if (m1 != tmodel) {
			g_print ("ERROR: \"data-model\" property of created iterator is wrong\n");
			failures ++;
		}
		g_object_unref (m1);

		/* check iter's contents */
		guint ncols;
		ncols = g_slist_length (GDA_SET (titer)->holders);
		if (ncols != g_slist_length (GDA_SET (iter)->holders)) {
			g_print ("ERROR: threaded iterator is at the wrong number of columns: "
				 "%d when it should be at %d\n",
				 ncols, g_slist_length (GDA_SET (iter)->holders));
			failures ++;
			ncols = -1;
		}
		else
			g_assert (ncols > 0);


		if (gda_data_model_iter_is_valid (titer)) {
			g_print ("ERROR: threaded iterator is valid before any read\n");
			failures ++;
		}
		if (gda_data_model_iter_is_valid (iter)) {
			g_print ("ERROR: iterator is valid before any read\n");
			failures ++;
		}
		gint row;
		gda_data_model_iter_move_next (iter);
		gda_data_model_iter_move_next (titer);
		for (row = 0; ; row++) {
			if (gda_data_model_iter_get_row (titer) != gda_data_model_iter_get_row (iter)) {
				g_print ("ERROR: threaded iterator is at the wrong row: %d when it should be at %d\n",
					 gda_data_model_iter_get_row (titer), gda_data_model_iter_get_row (iter));
				failures ++;
				break;
			}
			if (ncols > 0) {
				guint i;
				for (i = 0; i < ncols; i++) {
					const GValue *cv, *tcv;
					cv = gda_data_model_iter_get_value_at (iter, i);
					tcv = gda_data_model_iter_get_value_at (titer, i);
					if (gda_value_differ (cv, tcv)) {
						g_print ("ERROR: values in iterators differ at line %d, colunm %d\n",
							 row, i);
						failures ++;
					}
					//g_print ("%d,%d: %s\n", row, i, gda_value_stringify (tcv));
				}
			}
			gboolean v, tv;
			v = gda_data_model_iter_move_next (iter);
			tv = gda_data_model_iter_move_next (titer);
			if (!v || !tv)
				break;
		}
		if (gda_data_model_iter_is_valid (titer)) {
			g_print ("ERROR: threaded iterator is still valid\n");
			failures ++;
		}
		if (gda_data_model_iter_is_valid (iter)) {
			g_print ("ERROR: iterator is still valid\n");
			failures ++;
		}

		g_object_unref (iter);
		g_object_unref (titer);
	}	

	g_object_unref (model);
	g_object_unref (tmodel);

	return failures;
}

static gint
test_insert (GdaConnection *tcnc)
{
	gint failures = 0;
	if (!run_sql_non_select (tcnc, "INSERT INTO person (name, age) VALUES ('Alice', 12)")) {
		g_print ("ERROR: can't INSERT into threaded connection\n");
			failures ++;
	}
	return failures;
}

static gint
test_update (GdaConnection *tcnc)
{
	gint failures = 0;
	if (!run_sql_non_select (tcnc, "UPDATE person set name='Alf' where name='Grat'")) {
		g_print ("ERROR: can't UPDATE threaded connection\n");
			failures ++;
	}
	return failures;
}

static gint
test_delete (GdaConnection *tcnc)
{
	gint failures = 0;
	if (!run_sql_non_select (tcnc, "DELETE FROM person WHERE name='Alf'")) {
		g_print ("ERROR: can't DELETE from threaded connection\n");
			failures ++;
	}
	return failures;
}

static gint
test_meta_store (GdaConnection *cnc)
{
	GdaMetaStore *store;
	GdaConnection *tcnc;
	GError *error = NULL;

	g_print ("=== Starting test where threaded connection is used internally by a meta store\n");
	tcnc = gda_connection_open_from_string ("SQlite", "DB_DIR=.;DB_NAME=storedb", NULL,
						GDA_CONNECTION_OPTIONS_THREAD_ISOLATED, &error);
        if (!tcnc) {
                g_print ("ERROR opening connection in thread safe mode: %s\n",
                         error && error->message ? error->message : "No detail");
                return 1;
        }

	store = GDA_META_STORE (g_object_new (GDA_TYPE_META_STORE, "cnc", tcnc, NULL));
	g_object_unref (tcnc);
	g_object_set (G_OBJECT (cnc), "meta-store", store, NULL);
	g_object_unref (store);

	if (!gda_connection_update_meta_store (cnc, NULL, &error)) {
		g_print ("ERROR in gda_connection_update_meta_store(): %s\n",
                         error && error->message ? error->message : "No detail");
                return 1;
	}

	GdaDataModel *model;
	model = gda_meta_store_extract (store, "SELECT * FROM _tables", NULL);
	if (gda_data_model_get_n_rows (model) != 1) {
		g_print ("ERROR in gda_connection_update_meta_store(): the _tables table "
			 "should have exactly 1 row\n");
		g_object_unref (model);
		return 1;
	}
	g_object_unref (model);

	return 0;
}

static gint
compare_meta_data (GdaMetaStore *store1, GdaMetaStruct *mstruct1, 
		   GdaMetaStore *store2, GdaMetaStruct *mstruct2)
{
	GSList *all_dbo_list1, *all_dbo_list2, *list;

	all_dbo_list1 = gda_meta_struct_get_all_db_objects (mstruct1);
	all_dbo_list2 = gda_meta_struct_get_all_db_objects (mstruct2);

	for (list = all_dbo_list1; list; list = list->next) {
		GdaMetaDbObject *dbo1, *dbo2;
		GValue *v1, *v2, *v3;
		dbo1 = GDA_META_DB_OBJECT (list->data);

		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), dbo1->obj_catalog);
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), dbo1->obj_schema);
		g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), dbo1->obj_name);
		dbo2 = gda_meta_struct_get_db_object (mstruct2, v1, v2, v3);
		gda_value_free (v1);
		gda_value_free (v2);
		gda_value_free (v3);
		if (!dbo2) {
			g_print ("Error: Could not find object %s.%s.%s\n",
				 dbo1->obj_catalog,
				 dbo1->obj_schema,
				 dbo1->obj_name);
			return 1;
		}

		if (dbo1->obj_type != GDA_META_DB_TABLE)
			continue;

		g_print ("Checking meta store's table: %s\n", dbo1->obj_name);

		GdaDataModel *m1, *m2;
		gchar *str;
		str = g_strdup_printf ("SELECT * FROM %s", dbo1->obj_name);
		m1 = gda_meta_store_extract (store1, str, NULL);
		m2 = gda_meta_store_extract (store2, str, NULL);
		g_free (str);
		
		GdaDataComparator *cmp;
		GError *error = NULL;
		cmp = (GdaDataComparator*) gda_data_comparator_new (m1, m2);
		g_object_unref (m1);
		g_object_unref (m2);
		if (! gda_data_comparator_compute_diff (cmp, &error)) {
			g_print ("Can't compute data model differences for %s.%s.%s: %s\n", 
				 dbo1->obj_catalog,
				 dbo1->obj_schema,
				 dbo1->obj_name,
				 error && error->message ? error->message : "No detail");
			g_object_unref (cmp);
			return 1;
		}
		if (gda_data_comparator_get_n_diffs (cmp) != 0) {
			g_print ("There are some data model differences!\n");
			g_object_unref (cmp);
			return 1;
		}
		g_object_unref (cmp);
	}
	
	g_slist_free (all_dbo_list1);
	g_slist_free (all_dbo_list2);

	return 0;
}

static gint
test_meta_data (GdaConnection *cnc, GdaConnection *tcnc)
{
	GError *error = NULL;

	g_print ("=== Starting test where updating the threaded connection's meta data\n");
	if (!gda_connection_update_meta_store (tcnc, NULL, &error)) {
		g_print ("ERROR in gda_connection_update_meta_store() applied to threaded connection: %s\n",
                         error && error->message ? error->message : "No detail");
                return 1;
	}
	if (!gda_connection_update_meta_store (cnc, NULL, &error)) {
		g_print ("ERROR in gda_connection_update_meta_store() applied to non threaded connection: %s\n",
                         error && error->message ? error->message : "No detail");
                return 1;
	}

	GdaMetaStruct *mstruct, *tmstruct;
	mstruct = gda_meta_store_schema_get_structure (gda_connection_get_meta_store (cnc), NULL);
	tmstruct = gda_meta_store_schema_get_structure (gda_connection_get_meta_store (tcnc), NULL);
	g_assert (mstruct);
	g_assert (tmstruct);
	
	return compare_meta_data (gda_connection_get_meta_store (cnc), mstruct, 
				  gda_connection_get_meta_store (tcnc), tmstruct);
}


static void test_sig_error_cb (GdaConnection *cnc, GdaConnectionEvent *error, GdaConnectionEvent **ev);
static void test_sig_bool_cb (GdaConnection *cnc, gboolean *out_called);
static gint
test_signals (GdaConnection *cnc, GdaConnection *tcnc)
{
	gint failures = 0;
	
	/* test the "error" signal */
	GdaConnectionEvent *ev = NULL, *tev = NULL;
	g_signal_connect (G_OBJECT (cnc), "error",
			  G_CALLBACK (test_sig_error_cb), &ev);
	g_signal_connect (G_OBJECT (tcnc), "error",
			  G_CALLBACK (test_sig_error_cb), &tev);
	run_sql_non_select (cnc, "DELETE FROM person WHERE name = ##name::string");
	run_sql_non_select (tcnc, "DELETE FROM person WHERE name = ##name::string");
	
	g_assert (ev);
	if (!tev) {
		g_print ("ERROR: the threaded connection does not emit the \"error\" signal\n");
		return 1;
	}

	/* test the "conn_to_close" and "conn-closed" signal */
	gboolean called = FALSE, called2 = FALSE;
	g_signal_connect (G_OBJECT (tcnc), "conn-to-close",
			  G_CALLBACK (test_sig_bool_cb), &called);
	g_signal_connect (G_OBJECT (tcnc), "conn-closed",
			  G_CALLBACK (test_sig_bool_cb), &called2);
	gda_connection_close (tcnc);
	if (!called) {
		g_print ("ERROR: the threaded connection does not emit the \"conn-to-close\" signal\n");
		return 1;
	}
	if (!called2) {
		g_print ("ERROR: the threaded connection does not emit the \"conn-closed\" signal\n");
		return 1;
	}
	g_signal_handlers_disconnect_by_func (G_OBJECT (tcnc),
					      G_CALLBACK (test_sig_bool_cb), &called);
	g_signal_handlers_disconnect_by_func (G_OBJECT (tcnc),
					      G_CALLBACK (test_sig_bool_cb), &called2);

	/* test the "conn-opened" signal */
	called = FALSE;
	g_signal_connect (G_OBJECT (tcnc), "conn-opened",
			  G_CALLBACK (test_sig_bool_cb), &called);
	gda_connection_open (tcnc, NULL);
	g_assert (gda_connection_is_opened (tcnc));
	if (!called) {
		g_print ("ERROR: the threaded connection does not emit the \"conn-opened\" signal\n");
		return 1;
	}
	g_signal_handlers_disconnect_by_func (G_OBJECT (tcnc),
					      G_CALLBACK (test_sig_bool_cb), &called);

	/* test the "transaction-status-changed" signal */
	called = FALSE;
	g_signal_connect (G_OBJECT (tcnc), "transaction-status-changed",
			  G_CALLBACK (test_sig_bool_cb), &called);
	g_assert (gda_connection_begin_transaction (tcnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL));
	if (!called) {
		g_print ("ERROR: the threaded connection does not emit the \"transaction-status-changed\" signal\n");
		return 1;
	}
	g_signal_handlers_disconnect_by_func (G_OBJECT (tcnc),
					      G_CALLBACK (test_sig_bool_cb), &called);

	/* test the "dsn_changed" signal */
	called = FALSE;
	gda_connection_close (tcnc);
	if (gda_config_get_dsn_info ("SalesTest")) {
		g_signal_connect (G_OBJECT (tcnc), "dsn-changed",
				  G_CALLBACK (test_sig_bool_cb), &called);
		g_object_set (G_OBJECT (tcnc), "dsn", "SalesTest", NULL);
		if (!called) {
			g_print ("ERROR: the threaded connection does not emit the \"dsn-changed\" signal\n");
			return 1;
		}
		g_signal_handlers_disconnect_by_func (G_OBJECT (tcnc),
						      G_CALLBACK (test_sig_bool_cb), &called);
	}

	return failures;
}

static void
test_sig_error_cb (GdaConnection *cnc, GdaConnectionEvent *error, GdaConnectionEvent **ev)
{
	*ev = error;
}

static void
test_sig_bool_cb (GdaConnection *cnc, gboolean *out_called)
{
	*out_called = TRUE;
}
