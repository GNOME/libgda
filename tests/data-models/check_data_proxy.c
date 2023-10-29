/*
 * Copyright (C) 2007 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <stdarg.h>
#include "../test-errors.h"

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/
static gboolean do_test_signal (void);
static gboolean do_test_common_read (GdaDataModel *proxy);
static gboolean do_test_read_direct_1 (void);
static gboolean do_test_read_direct_2 (void);

static gboolean do_test_common_write (GdaDataModel *proxy);
static gboolean do_test_array (void);

static gboolean do_test_prop_change (void);
static gboolean do_test_proxied_model_modif (void);

static gboolean do_test_delete_rows (void);

static gboolean do_test_modifs_persistance (void);

/* 
 * the tests are all run with all the possible combinations of defer_sync (TRUE/FALSE) and 
 * prepend_null_row (TRUE/FALSE), so the ADJUST_ROW() macro allows for row adjustment
 * when specifying expected results
 */
gboolean defer_sync;
gboolean prepend_null_row;
#define ADJUST_ROW(x) (prepend_null_row ? (x)+1 : (x))
#define ADJUST_ROW_FOR_MODEL(model,x) (GDA_IS_DATA_PROXY(model)? (prepend_null_row ? (x)+1 : (x)) : (x))

static void proxy_reset_cb (GdaDataModel *model, gchar *detail);
static void proxy_row_cb (GdaDataModel *model, gint row, gchar *detail);
static void declare_expected_signals (const gchar *expected, const gchar *name);
static void clean_expected_signals (GdaDataModel *model);
static void wait_for_signals (void);
static gboolean check_data_model_value (GdaDataModel *model, gint row, gint col, GType type, const gchar *value);
static gboolean check_data_model_n_rows (GdaDataModel *model, gint nrow);
static gboolean check_proxy_set_filter (GdaDataProxy *proxy, const gchar *filter);
static gboolean check_proxy_row_is_deleted (GdaDataProxy *proxy, gint row, gboolean is_deleted);
static gboolean check_data_model_append_row (GdaDataModel *model);
static gboolean check_data_model_set_values (GdaDataModel *model, gint row, GList *values);
static gboolean check_data_model_remove_row (GdaDataModel *model, gint row);

static GList *make_values_list (gint dummy, ...);
static void free_values_list (GList *list);


int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	int number_failed = 0;

	gda_init ();

	prepend_null_row = FALSE;
	defer_sync = FALSE;
	if (!do_test_modifs_persistance ())
		number_failed ++;

	defer_sync = TRUE;
	if (!do_test_modifs_persistance ())
		number_failed ++;

	prepend_null_row = TRUE;
 	defer_sync = FALSE;
	if (!do_test_modifs_persistance ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_modifs_persistance ())
		number_failed ++;

	prepend_null_row = FALSE;
	defer_sync = FALSE;
	if (!do_test_delete_rows ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_delete_rows ())
		number_failed ++;

	prepend_null_row = TRUE;
 	defer_sync = FALSE;
	if (!do_test_delete_rows ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_delete_rows ())
		number_failed ++;
	
	if (!do_test_signal ())
		number_failed ++;

	prepend_null_row = FALSE;
	defer_sync = FALSE;
	if (!do_test_read_direct_1 ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_read_direct_1 ())
		number_failed ++;

	defer_sync = FALSE;
	if (!do_test_read_direct_2 ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_read_direct_2 ())
		number_failed ++;

	defer_sync = FALSE;
	if (!do_test_array ())
		number_failed ++;

	defer_sync = TRUE;
	if (!do_test_array ())
		number_failed ++;

	prepend_null_row = TRUE;
	defer_sync = FALSE;
	if (!do_test_read_direct_1 ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_read_direct_1 ())
		number_failed ++;

	defer_sync = FALSE;
	if (!do_test_read_direct_2 ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_read_direct_2 ())
		number_failed ++;

	defer_sync = FALSE;
	if (!do_test_array ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_array ())
		number_failed ++;

	prepend_null_row = FALSE;
	defer_sync = FALSE;
	if (!do_test_prop_change ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_prop_change ())
		number_failed ++;

	prepend_null_row = FALSE;
	defer_sync = FALSE;
	if (!do_test_proxied_model_modif ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_proxied_model_modif ())
		number_failed ++;

	prepend_null_row = TRUE;
	defer_sync = FALSE;
	if (!do_test_proxied_model_modif ())
		number_failed ++;
	defer_sync = TRUE;
	if (!do_test_proxied_model_modif ())
		number_failed ++;

	if (number_failed == 0)
		g_print ("Ok.\n");
	else
		g_print ("%d failed\n", number_failed);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static GError *
validate_row_changes (GdaDataProxy *proxy, gint row, G_GNUC_UNUSED gint proxied_row, G_GNUC_UNUSED gchar *token)
{
	const GValue *cvalue;
	/* refuse if population < 100 */
	
	cvalue = gda_data_model_get_value_at ((GdaDataModel*) proxy, 2, row, NULL);
	g_assert (cvalue);
	if (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)
		return NULL;
	else {
		gint pop;
		pop = g_value_get_int (cvalue);
		if (pop < 100) {
			GError *error = NULL;
			g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "%s", "Population is too small");
			return error;
		}
		else
			return NULL;
	}
}

static gboolean
do_test_signal (void)
{
#define FILE "city.csv"
	gchar *file;
	GdaDataModel *import, *proxy, *rw_model;
	GSList *errors;
	GdaSet *options;
	GValue *value;
	GError *lerror = NULL;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (2, 
				      "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE,
				      "G_TYPE_2", G_TYPE_GTYPE, G_TYPE_INT);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	rw_model = (GdaDataModel*) gda_data_model_array_copy_model (import, NULL);
	g_assert (rw_model);
	g_object_unref (import);
	proxy = g_object_new (GDA_TYPE_DATA_PROXY, "model", rw_model, "sample-size", 0, NULL);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}

	g_signal_connect (G_OBJECT (proxy), "validate_row_changes",
			  G_CALLBACK (validate_row_changes), "Token");

	/**/
	g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 23);
	g_assert (gda_data_model_set_value_at (proxy, 2, 5, value, NULL));
	g_value_set_int (value, 100);
	g_assert (gda_data_model_set_value_at (proxy, 2, 3, value, NULL));
	gda_value_free (value);

	if (gda_data_proxy_apply_all_changes (GDA_DATA_PROXY (proxy), &lerror)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: gda_data_proxy_apply_all_changes() should have failed\n");
#endif
		return FALSE; 
	}
	if (!lerror) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: returned error should not be NULL\n");
#endif
		return FALSE; 
	}
	if (strcmp (lerror->message, "Population is too small")) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: returned error message should be 'Population is too small', and got '%s'\n", 
			 lerror->message);
#endif
		return FALSE; 
	}
	g_error_free (lerror);

	/**/
	g_assert (gda_data_proxy_cancel_all_changes (GDA_DATA_PROXY (proxy)));
	g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 123);
	g_assert (gda_data_model_set_value_at (proxy, 2, 5, value, NULL));
	gda_value_free (value);
	if (!gda_data_proxy_apply_all_changes (GDA_DATA_PROXY (proxy), &lerror)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: gda_data_proxy_apply_all_changes() should not have failed, error: %s\n",
			 lerror && lerror->message ? lerror->message : "No detail");
#endif
		return FALSE; 
	}

	g_object_unref (proxy);
	g_object_unref (rw_model);

	return TRUE;
}

static gboolean
do_test_read_direct_1 ()
{
	#define FILE "city.csv"
	gchar *file;
	GdaDataModel *import, *proxy;
	GSList *errors;
	GdaSet *options;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	proxy = g_object_new (GDA_TYPE_DATA_PROXY, "model", import, "sample-size", 0, NULL);
	g_object_unref (import);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	return do_test_common_read (proxy);
}

static gboolean
do_test_read_direct_2 ()
{
	#define FILE "city.csv"
	gchar *file;
	GdaDataModel *import, *proxy;
	GSList *errors;
	GdaSet *options;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	proxy = (GdaDataModel *) gda_data_proxy_new (import);
	g_object_unref (import);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	return do_test_common_read (proxy);
}

static gboolean
do_test_array ()
{
	#define FILE "city.csv"
	GError *error;
	gchar *file;
	GdaDataModel *import, *model, *proxy;
	GSList *errors;
	GdaSet *options;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	model = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!model) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}
	g_object_unref (import);

	proxy = (GdaDataModel *) gda_data_proxy_new (model);
	g_object_unref (model);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	return do_test_common_write (proxy);
}

static gboolean
do_test_prop_change (void)
{
#define FILE "city.csv"
	GError *error;
	gchar *file;
	GdaDataModel *import, *model, *proxy;
	GSList *errors;
	GdaSet *options;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	model = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!model) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}
	g_object_unref (import);

	proxy = (GdaDataModel *) gda_data_proxy_new (model);
	g_object_unref (model);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	g_object_set (G_OBJECT (proxy), "defer-sync", defer_sync, NULL);

	gboolean retval = FALSE;

	g_signal_connect (G_OBJECT (proxy), "reset",
			  G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_connect (G_OBJECT (proxy), "row-inserted",
			  G_CALLBACK (proxy_row_cb), "I");
	g_signal_connect (G_OBJECT (proxy), "row-updated",
			  G_CALLBACK (proxy_row_cb), "U");
	g_signal_connect (G_OBJECT (proxy), "row-removed",
			  G_CALLBACK (proxy_row_cb), "R");

	/*
	 * Set prepend-null-entry to TRUE
	 */
	declare_expected_signals ("I0", "Set prepend-null-entry to TRUE (no chunk)");
	g_object_set (G_OBJECT (proxy), "prepend-null-entry", TRUE, NULL);
	clean_expected_signals (proxy);

	/*
	 * Set prepend-null-entry to FALSE
	 */
	declare_expected_signals ("R0", "Set prepend-null-entry to FALSE (no chunk)");
	g_object_set (G_OBJECT (proxy), "prepend-null-entry", FALSE, NULL);
	clean_expected_signals (proxy);

	/*
	 * change sample size to 20
	 */
	declare_expected_signals ("138R20", "change sample size to 20");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 20);
	clean_expected_signals (proxy);

	/*
	 * Set prepend-null-entry to TRUE
	 */
	declare_expected_signals ("I0", "Set prepend-null-entry to TRUE (with chunk)");
	g_object_set (G_OBJECT (proxy), "prepend-null-entry", TRUE, NULL);
	clean_expected_signals (proxy);

	/*
	 * Set prepend-null-entry to FALSE
	 */
	declare_expected_signals ("R0", "Set prepend-null-entry to FALSE (with chunk)");
	g_object_set (G_OBJECT (proxy), "prepend-null-entry", FALSE, NULL);
	clean_expected_signals (proxy);

	retval = TRUE;
	g_object_unref (proxy);
	return retval;
}

static gboolean
do_test_proxied_model_modif (void)
{
#define FILE "city.csv"
	GError *error = NULL;
	gchar *file;
	GdaDataModel *import, *model, *proxy;
	GSList *errors;
	GdaSet *options;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	model = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!model) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}
	g_object_unref (import);

	proxy = (GdaDataModel *) gda_data_proxy_new (model);
	g_object_unref (model);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	g_object_set (G_OBJECT (proxy), "defer-sync", defer_sync, 
		      "prepend-null-entry", prepend_null_row, NULL);

	gboolean retval = FALSE;

	g_signal_connect (G_OBJECT (proxy), "reset",
			  G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_connect (G_OBJECT (proxy), "row-inserted",
			  G_CALLBACK (proxy_row_cb), "I");
	g_signal_connect (G_OBJECT (proxy), "row-updated",
			  G_CALLBACK (proxy_row_cb), "U");
	g_signal_connect (G_OBJECT (proxy), "row-removed",
			  G_CALLBACK (proxy_row_cb), "R");

	
	GValue *value;
	GList *values;
	/* 
	 * update a row in proxied model
	 */
	declare_expected_signals ("U157", "update a row in proxied model - 1");
	g_value_set_string (value = gda_value_new (G_TYPE_STRING), "BigCity");
	if (!gda_data_model_set_value_at (model, 0, 157, value, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to set value: %s\n", error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 157, 0, G_TYPE_STRING, "BigCity")) goto out;
	if (!check_data_model_value (proxy, 157, 2, G_TYPE_STRING, "299801")) goto out;
	if (!check_data_model_value (proxy, 157, 3, G_TYPE_STRING, "BigCity")) goto out;
	if (!check_data_model_value (proxy, 157, 5, G_TYPE_STRING, "299801")) goto out;

	/*
	 * insert a row in the proxied data model
	 */
	declare_expected_signals ("I158", "insert a row in proxied model - 1");
	if (! check_data_model_append_row (model)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 159)) goto out;

	values = make_values_list (0, G_TYPE_STRING, "BigCity2", G_TYPE_STRING, NULL, G_TYPE_STRING, "567890", (GType) 0);
	declare_expected_signals ("U158", "Set values of new proxied row - 1");
	if (!check_data_model_set_values (model, 158, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 158, 0, G_TYPE_STRING, "BigCity2")) goto out;
	if (!check_data_model_value (proxy, 158, 1, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 158, 2, G_TYPE_STRING, "567890")) goto out;
	if (!check_data_model_value (proxy, 158, 3, G_TYPE_STRING, "BigCity2")) goto out;
	if (!check_data_model_value (proxy, 158, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 158, 5, G_TYPE_STRING, "567890")) goto out;

	/* 
	 * delete a proxy row
	 */
	gda_data_model_dump (proxy, stdout);
	declare_expected_signals ("R5", "Delete a proxy row - 1");
	if (!check_data_model_remove_row (model, 5)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 158)) goto out;
	gda_data_model_dump (proxy, stdout);

	/*
	 * Set sample size
	 */
	declare_expected_signals ("138R20", "change sample size to 20");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 20);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 20)) goto out;

	/* 
	 * update a row in proxied model
	 */
	declare_expected_signals ("U10", "update a row in proxied model - 2");
	g_value_set_string (value = gda_value_new (G_TYPE_STRING), "SmallCity");
	if (!gda_data_model_set_value_at (model, 0, 10, value, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to set value: %s\n", error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 10, 0, G_TYPE_STRING, "SmallCity")) goto out;
	if (!check_data_model_value (proxy, 10, 2, G_TYPE_STRING, "595")) goto out;
	if (!check_data_model_value (proxy, 10, 3, G_TYPE_STRING, "SmallCity")) goto out;
	if (!check_data_model_value (proxy, 10, 5, G_TYPE_STRING, "595")) goto out;

	/*
	 * insert a row in the proxied data model, no signal is emited by @proxy
	 */
	declare_expected_signals (NULL, "insert a row in proxied model - 2");
	if (! check_data_model_append_row (model)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 20)) goto out;

	/*
	 * Set sample start
	 */
	declare_expected_signals ("20U0", "change sample start to 20");
	gda_data_proxy_set_sample_start (GDA_DATA_PROXY (proxy), 20);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 20)) goto out;

	/*
	 * change new proxied's rows values
	 */
	values = make_values_list (0, G_TYPE_STRING, "SmallCity2", G_TYPE_STRING, NULL, G_TYPE_STRING, "4907", (GType) 0);
	declare_expected_signals ("U1", "Set values of new proxied row - 2");
	if (!check_data_model_set_values (model, 21, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 1, 0, G_TYPE_STRING, "SmallCity2")) goto out;
	if (!check_data_model_value (proxy, 1, 1, G_TYPE_STRING, "ARG")) goto out;
	if (!check_data_model_value (proxy, 1, 2, G_TYPE_STRING, "4907")) goto out;
	if (!check_data_model_value (proxy, 1, 3, G_TYPE_STRING, "SmallCity2")) goto out;
	if (!check_data_model_value (proxy, 1, 4, G_TYPE_STRING, "ARG")) goto out;
	if (!check_data_model_value (proxy, 1, 5, G_TYPE_STRING, "4907")) goto out;

	/* 
	 * delete a proxy row, not in current proxy's chunk
	 */
	gda_data_model_dump (proxy, stdout);
	declare_expected_signals (NULL, "Delete a proxy row - 2");
	if (!check_data_model_remove_row (model, 5)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 20)) goto out;
	gda_data_model_dump (proxy, stdout);

	/* 
	 * delete a proxy row, in current proxy's chunk
	 */
	declare_expected_signals ("R6", "Delete a proxy row - 3");
	if (!check_data_model_remove_row (model, 25)) goto out;
	clean_expected_signals (proxy);
	gda_data_model_dump (proxy, stdout);
	if (!check_data_model_n_rows (proxy, 19)) goto out;

	retval = TRUE;
 out:
	g_object_unref (proxy);
	return retval;
}

static gboolean
do_test_common_read (GdaDataModel *proxy)
{
	const gchar *filter;
	gboolean retval = FALSE;

	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 10);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	g_object_set (G_OBJECT (proxy), "defer-sync", defer_sync, 
		      "prepend-null-entry", prepend_null_row, NULL);
	g_signal_connect (G_OBJECT (proxy), "reset",
			  G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_connect (G_OBJECT (proxy), "row-inserted",
			  G_CALLBACK (proxy_row_cb), "I");
	g_signal_connect (G_OBJECT (proxy), "row-updated",
			  G_CALLBACK (proxy_row_cb), "U");
	g_signal_connect (G_OBJECT (proxy), "row-removed",
			  G_CALLBACK (proxy_row_cb), "R");

	/* 
	 * try some filter with multiple statements, should not be accepted 
	 */
	if (gda_data_proxy_set_filter_expr (GDA_DATA_PROXY (proxy), "countrycode = 'BFA'; SELECT * FROM proxy", NULL)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Should not accept multi statement SQL filter!\n");
#endif
		g_object_unref (proxy);
		return FALSE;
	}
	filter = gda_data_proxy_get_filter_expr (GDA_DATA_PROXY (proxy));
	if (filter != NULL) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Filter should be NULL\n");
#endif
		g_object_unref (proxy);
		return FALSE;
	}

	/* 
	 * try a correct filter, and check for the signals 
	 */
	declare_expected_signals ("3U0/7R3", "try a correct filter, and check for the signals");
	if (! check_proxy_set_filter (GDA_DATA_PROXY (proxy), "countrycode = 'BFA'")) goto out;
	clean_expected_signals (proxy);


	filter = gda_data_proxy_get_filter_expr (GDA_DATA_PROXY (proxy));
	if (!filter || strcmp (filter, "countrycode = 'BFA'")) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Filter should be '%s' and is '%s'\n", "countrycode = 'BFA'", filter);
#endif
		g_object_unref (proxy);
		return FALSE;
	}

	/* 
	 * try another correct filter, and check for the signals 
	 */
	declare_expected_signals ("3U0/7I3", "try another correct filter, and check for the signals");
	if (! check_proxy_set_filter (GDA_DATA_PROXY (proxy), "countrycode = 'BGD'")) goto out;
	clean_expected_signals (proxy);

	/*
	 * change sample start
	 */
	declare_expected_signals ("10U0", "change sample start to 2");
	gda_data_proxy_set_sample_start (GDA_DATA_PROXY (proxy), 2);
	clean_expected_signals (proxy);

	/*
	 * change sample size to 13
	 */
	declare_expected_signals ("3I10", "change sample size to 13");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 13);
	clean_expected_signals (proxy);

	/*
	 * change sample size to 0 => no chunking
	 */
	declare_expected_signals ("13U0/11I13", "change sample size to 0 => no chunking");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	clean_expected_signals (proxy);

	/*
	 * change sample size to 7
	 */
	declare_expected_signals ("17R7", "change sample size to 7");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 7);
	clean_expected_signals (proxy);

	/*
	 * change sample start to 22
	 */
	declare_expected_signals ("2U0/5R2", "change sample start to 22");
	gda_data_proxy_set_sample_start (GDA_DATA_PROXY (proxy), 22);
	clean_expected_signals (proxy);

	/*
	 * remove filter
	 */
	declare_expected_signals ("2U0/5I2", "unset filter");
	if (! check_proxy_set_filter (GDA_DATA_PROXY (proxy), NULL)) goto out;
	clean_expected_signals (proxy);

	/*
	 * last change sample size to 0 (no chunking and no filter)
	 */
	declare_expected_signals ("7U0/151I7", "last change sample size to 0 (no chunking and no filter)");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	clean_expected_signals (proxy);

	retval = TRUE;
 out:
	g_object_unref (proxy);
	return retval;
}

static gboolean
do_test_common_write (GdaDataModel *proxy)
{
	GError *error = NULL;
	GValue *value;
	GList *values;
	gboolean retval = FALSE;

	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 10);
	g_object_set (G_OBJECT (proxy), "defer-sync", defer_sync, 
		      "prepend-null-entry", prepend_null_row, NULL);

	g_signal_connect (G_OBJECT (proxy), "reset",
			  G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_connect (G_OBJECT (proxy), "row-inserted",
			  G_CALLBACK (proxy_row_cb), "I");
	g_signal_connect (G_OBJECT (proxy), "row-updated",
			  G_CALLBACK (proxy_row_cb), "U");
	g_signal_connect (G_OBJECT (proxy), "row-removed",
			  G_CALLBACK (proxy_row_cb), "R");

	/*
	 * change sample size to 0 (no chunking and no filter)
	 */
	declare_expected_signals ("148I10", "change sample size to 0 (no chunking and no filter)");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	clean_expected_signals (proxy);

	/*
	 * append a row
	 */
	declare_expected_signals ("1I158", "Insert a row");
	if (! check_data_model_append_row (proxy)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 158, 0, G_TYPE_STRING, NULL)) goto out;

	/*
	 * Set value of the new row 
	 */
	declare_expected_signals ("U158", "Set value of new row");
	g_value_set_string (value = gda_value_new (G_TYPE_STRING), "MyCity");
	if (!gda_data_model_set_value_at (proxy, 0, ADJUST_ROW (158), value, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to set value: %s\n", error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 158, 0, G_TYPE_STRING, "MyCity")) goto out;
	if (!check_data_model_value (proxy, 158, 2, G_TYPE_STRING, NULL)) goto out;

	declare_expected_signals ("U158", "Set value of new row");
	if (!gda_data_proxy_apply_row_changes (GDA_DATA_PROXY (proxy), ADJUST_ROW (158), &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to apply changes: %s\n", error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 158, 0, G_TYPE_STRING, "MyCity")) goto out;
 
	/*
	 * append another row
	 */
	declare_expected_signals ("I159", "Insert another row");
	if (! check_data_model_append_row (proxy)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 159, 0, G_TYPE_STRING, NULL)) goto out;

	/*
	 * set several values for the new row 
	 */
	values = make_values_list (0, G_TYPE_STRING, "MyCity2", G_TYPE_STRING, NULL, G_TYPE_STRING, "12345", (GType) 0);
	declare_expected_signals ("U159", "Set values of new row - new row");
	if (!check_data_model_set_values (proxy, 159, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 159, 0, G_TYPE_STRING, "MyCity2")) goto out;
	if (!check_data_model_value (proxy, 159, 1, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 2, G_TYPE_STRING, "12345")) goto out;
	if (!check_data_model_value (proxy, 159, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_n_rows (proxy, 160)) goto out;

	values = make_values_list (0, G_TYPE_STRING, "MyCity3", G_TYPE_STRING, "ZZZ", G_TYPE_STRING, "787", (GType) 0);
	declare_expected_signals ("U157", "Set values of new row - existing row");
	if (!check_data_model_set_values (proxy, 157, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);

	if (!check_data_model_value (proxy, 157, 0, G_TYPE_STRING, "MyCity3")) goto out;
	if (!check_data_model_value (proxy, 157, 1, G_TYPE_STRING, "ZZZ")) goto out;
	if (!check_data_model_value (proxy, 157, 2, G_TYPE_STRING, "787")) goto out;
	if (!check_data_model_value (proxy, 157, 3, G_TYPE_STRING, "Varna")) goto out;
	if (!check_data_model_value (proxy, 157, 4, G_TYPE_STRING, "BGR")) goto out;
	if (!check_data_model_value (proxy, 157, 5, G_TYPE_STRING, "299801")) goto out;

	/*
	 * Change sample size to 50 
	 */
	declare_expected_signals ("U50/109R51", "change sample size to 50 (still no filter)");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 50);
	clean_expected_signals (proxy);

	if (!check_data_model_value (proxy, 50, 0, G_TYPE_STRING, "MyCity2")) goto out;
	if (!check_data_model_value (proxy, 50, 1, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 2, G_TYPE_STRING, "12345")) goto out;
	if (!check_data_model_value (proxy, 49, 3, G_TYPE_STRING, "Merlo")) goto out;
	if (!check_data_model_value (proxy, 49, 4, G_TYPE_STRING, "ARG")) goto out;
	if (!check_data_model_value (proxy, 49, 5, G_TYPE_STRING, "463846")) goto out;
	if (!check_data_model_n_rows (proxy, 51)) goto out;
	
	/*
	 * Set filter 
	 */
	declare_expected_signals ("3U0/48R3", "set filter to 'MyCity%'");
	if (! check_proxy_set_filter (GDA_DATA_PROXY (proxy), "name LIKE 'MyCity%' ORDER BY name")) goto out;
	clean_expected_signals (proxy);
	
	if (!check_data_model_value (proxy, 0, 0, G_TYPE_STRING, "MyCity")) goto out;
	if (!check_data_model_value (proxy, 1, 0, G_TYPE_STRING, "MyCity2")) goto out;
	if (!check_data_model_value (proxy, 2, 0, G_TYPE_STRING, "MyCity3")) goto out;
	if (!check_data_model_n_rows (proxy, 3)) goto out;

	/*
	 * Cancel changes to existing row
	 */
	declare_expected_signals ("U2", "Cancel changes of existing row");
	gda_data_proxy_cancel_row_changes (GDA_DATA_PROXY (proxy), ADJUST_ROW (2), -1);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 3)) goto out;

	/*
	 * Cancel changes to row
	 */
	declare_expected_signals ("R1", "Cancel changes of new row");
	gda_data_proxy_cancel_row_changes (GDA_DATA_PROXY (proxy), ADJUST_ROW (1), -1);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 2)) goto out;

	/*
	 * Unset filter
	 */
	declare_expected_signals ("2U0/48I2", "Unset filter");
	if (! check_proxy_set_filter (GDA_DATA_PROXY (proxy), NULL)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 50)) goto out;

	/*
	 * Delete an existing row
	 */
	declare_expected_signals ("U5", "Delete an existing row");
	if (!check_data_model_remove_row (proxy, 5)) goto out;
	clean_expected_signals (proxy);
	if (!check_proxy_row_is_deleted (GDA_DATA_PROXY (proxy), 5, TRUE)) goto out;
	if (!check_data_model_n_rows (proxy, 50)) goto out;

	/* 
	 * Undelete an existing row
	 */
	declare_expected_signals ("U5", "Undelete an existing row");
	gda_data_proxy_undelete (GDA_DATA_PROXY (proxy), ADJUST_ROW (5));
	clean_expected_signals (proxy);
	if (!check_proxy_row_is_deleted (GDA_DATA_PROXY (proxy), 5, FALSE)) goto out;
	if (!check_data_model_n_rows (proxy, 50)) goto out;

	/*
	 * Add 4 new rows
	 */
	declare_expected_signals ("4I50", "Insert 4 rows");
	if (! check_data_model_append_row (proxy)) goto out;
	if (! check_data_model_append_row (proxy)) goto out;
	if (! check_data_model_append_row (proxy)) goto out;
	if (! check_data_model_append_row (proxy)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 54)) goto out;
	
	/* 
	 * Set values for those 3 new rows
	 */
	declare_expected_signals ("4U50", "Set values of 3 new rows");
	values = make_values_list (0, G_TYPE_STRING, "NR1", G_TYPE_STRING, "AAA", G_TYPE_STRING, "319", (GType) 0);
	if (!check_data_model_set_values (proxy, 50, values)) goto out;
	free_values_list (values);
	values = make_values_list (0, G_TYPE_STRING, "NR2", G_TYPE_STRING, "BBB", G_TYPE_STRING, "320", (GType) 0);
	if (!check_data_model_set_values (proxy, 51, values)) goto out;
	free_values_list (values);
	values = make_values_list (0, G_TYPE_STRING, "NR3", G_TYPE_STRING, "CCC", G_TYPE_STRING, "321", (GType) 0);
	if (!check_data_model_set_values (proxy, 52, values)) goto out;
	free_values_list (values);
	values = make_values_list (0, G_TYPE_STRING, "NR4", G_TYPE_STRING, "DDD", G_TYPE_STRING, "330", (GType) 0);
	if (!check_data_model_set_values (proxy, 53, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);

	if (!check_data_model_value (proxy, 50, 0, G_TYPE_STRING, "NR1")) goto out;
	if (!check_data_model_value (proxy, 50, 1, G_TYPE_STRING, "AAA")) goto out;
	if (!check_data_model_value (proxy, 50, 2, G_TYPE_STRING, "319")) goto out;
	if (!check_data_model_value (proxy, 50, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 0, G_TYPE_STRING, "NR2")) goto out;
	if (!check_data_model_value (proxy, 51, 1, G_TYPE_STRING, "BBB")) goto out;
	if (!check_data_model_value (proxy, 51, 2, G_TYPE_STRING, "320")) goto out;
	if (!check_data_model_value (proxy, 52, 0, G_TYPE_STRING, "NR3")) goto out;
	if (!check_data_model_value (proxy, 52, 1, G_TYPE_STRING, "CCC")) goto out;
	if (!check_data_model_value (proxy, 52, 2, G_TYPE_STRING, "321")) goto out;
	if (!check_data_model_value (proxy, 53, 0, G_TYPE_STRING, "NR4")) goto out;
	if (!check_data_model_value (proxy, 53, 1, G_TYPE_STRING, "DDD")) goto out;
	if (!check_data_model_value (proxy, 53, 2, G_TYPE_STRING, "330")) goto out;

	/*
	 * Remove 2nd row of the 4 new rows
	 */
	declare_expected_signals ("R51", "Remove 2nd row of the 4 new rows");
	if (!check_data_model_remove_row (proxy, 51)) goto out;
	if (!check_data_model_value (proxy, 50, 0, G_TYPE_STRING, "NR1")) goto out;
	if (!check_data_model_value (proxy, 50, 1, G_TYPE_STRING, "AAA")) goto out;
	if (!check_data_model_value (proxy, 50, 2, G_TYPE_STRING, "319")) goto out;
	if (!check_data_model_value (proxy, 50, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 0, G_TYPE_STRING, "NR3")) goto out;
	if (!check_data_model_value (proxy, 51, 1, G_TYPE_STRING, "CCC")) goto out;
	if (!check_data_model_value (proxy, 51, 2, G_TYPE_STRING, "321")) goto out;
	if (!check_data_model_value (proxy, 51, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 52, 0, G_TYPE_STRING, "NR4")) goto out;
	if (!check_data_model_value (proxy, 52, 1, G_TYPE_STRING, "DDD")) goto out;
	if (!check_data_model_value (proxy, 52, 2, G_TYPE_STRING, "330")) goto out;
	if (!check_data_model_value (proxy, 52, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 52, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 52, 5, G_TYPE_STRING, NULL)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 53)) goto out;

	/*
	 * cancel changes for the 1st new row
	 */
	declare_expected_signals ("R50", "Cancel changes of new row");
	gda_data_proxy_cancel_row_changes (GDA_DATA_PROXY (proxy), ADJUST_ROW (50), -1);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 52)) goto out;
	if (!check_data_model_value (proxy, 50, 0, G_TYPE_STRING, "NR3")) goto out;
	if (!check_data_model_value (proxy, 50, 1, G_TYPE_STRING, "CCC")) goto out;
	if (!check_data_model_value (proxy, 50, 2, G_TYPE_STRING, "321")) goto out;
	if (!check_data_model_value (proxy, 50, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 50, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 0, G_TYPE_STRING, "NR4")) goto out;
	if (!check_data_model_value (proxy, 51, 1, G_TYPE_STRING, "DDD")) goto out;
	if (!check_data_model_value (proxy, 51, 2, G_TYPE_STRING, "330")) goto out;
	if (!check_data_model_value (proxy, 51, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 51, 5, G_TYPE_STRING, NULL)) goto out;

	/*
	 * Clean changes
	 */
	declare_expected_signals ("2R50", "Cancel all changes");
	gda_data_proxy_cancel_all_changes (GDA_DATA_PROXY (proxy));
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 50)) goto out;

	/*
	 * Change sample size to 0 
	 */
	declare_expected_signals ("109I50", "change sample size to 0");
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	clean_expected_signals (proxy);

	/*
	 * Add 4 new rows
	 */
	declare_expected_signals ("4I159", "Insert 4 rows");
	if (! check_data_model_append_row (proxy)) goto out;
	if (! check_data_model_append_row (proxy)) goto out;
	if (! check_data_model_append_row (proxy)) goto out;
	if (! check_data_model_append_row (proxy)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 163)) goto out;
	
	/* 
	 * Set values for those 4 new rows
	 */
	declare_expected_signals ("4U159", "Set values of 4 new rows");
	values = make_values_list (0, G_TYPE_STRING, "NR1", G_TYPE_STRING, "AAA", G_TYPE_STRING, "319", (GType) 0);
	if (!check_data_model_set_values (proxy, 159, values)) goto out;
	free_values_list (values);
	values = make_values_list (0, G_TYPE_STRING, "NR2", G_TYPE_STRING, "BBB", G_TYPE_STRING, "320", (GType) 0);
	if (!check_data_model_set_values (proxy, 160, values)) goto out;
	free_values_list (values);
	values = make_values_list (0, G_TYPE_STRING, "NR3", G_TYPE_STRING, "CCC", G_TYPE_STRING, "321", (GType) 0);
	if (!check_data_model_set_values (proxy, 161, values)) goto out;
	free_values_list (values);
	values = make_values_list (0, G_TYPE_STRING, "NR4", G_TYPE_STRING, "DDD", G_TYPE_STRING, "330", (GType) 0);
	if (!check_data_model_set_values (proxy, 162, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);

	if (!check_data_model_value (proxy, 159, 0, G_TYPE_STRING, "NR1")) goto out;
	if (!check_data_model_value (proxy, 159, 1, G_TYPE_STRING, "AAA")) goto out;
	if (!check_data_model_value (proxy, 159, 2, G_TYPE_STRING, "319")) goto out;
	if (!check_data_model_value (proxy, 159, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 0, G_TYPE_STRING, "NR2")) goto out;
	if (!check_data_model_value (proxy, 160, 1, G_TYPE_STRING, "BBB")) goto out;
	if (!check_data_model_value (proxy, 160, 2, G_TYPE_STRING, "320")) goto out;
	if (!check_data_model_value (proxy, 161, 0, G_TYPE_STRING, "NR3")) goto out;
	if (!check_data_model_value (proxy, 161, 1, G_TYPE_STRING, "CCC")) goto out;
	if (!check_data_model_value (proxy, 161, 2, G_TYPE_STRING, "321")) goto out;
	if (!check_data_model_value (proxy, 162, 0, G_TYPE_STRING, "NR4")) goto out;
	if (!check_data_model_value (proxy, 162, 1, G_TYPE_STRING, "DDD")) goto out;
	if (!check_data_model_value (proxy, 162, 2, G_TYPE_STRING, "330")) goto out;

	/*
	 * Remove 2nd row of the 4 new rows
	 */
	declare_expected_signals ("R160", "Remove 2nd row of the 4 new rows");
	if (!check_data_model_remove_row (proxy, 160)) goto out;
	if (!check_data_model_value (proxy, 159, 0, G_TYPE_STRING, "NR1")) goto out;
	if (!check_data_model_value (proxy, 159, 1, G_TYPE_STRING, "AAA")) goto out;
	if (!check_data_model_value (proxy, 159, 2, G_TYPE_STRING, "319")) goto out;
	if (!check_data_model_value (proxy, 159, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 0, G_TYPE_STRING, "NR3")) goto out;
	if (!check_data_model_value (proxy, 160, 1, G_TYPE_STRING, "CCC")) goto out;
	if (!check_data_model_value (proxy, 160, 2, G_TYPE_STRING, "321")) goto out;
	if (!check_data_model_value (proxy, 160, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 161, 0, G_TYPE_STRING, "NR4")) goto out;
	if (!check_data_model_value (proxy, 161, 1, G_TYPE_STRING, "DDD")) goto out;
	if (!check_data_model_value (proxy, 161, 2, G_TYPE_STRING, "330")) goto out;
	if (!check_data_model_value (proxy, 161, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 161, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 161, 5, G_TYPE_STRING, NULL)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 162)) goto out;

	/*
	 * cancel changes for the 1st new row
	 */
	declare_expected_signals ("R159", "Cancel changes of new row");
	gda_data_proxy_cancel_row_changes (GDA_DATA_PROXY (proxy), ADJUST_ROW (159), -1);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 161)) goto out;
	if (!check_data_model_value (proxy, 159, 0, G_TYPE_STRING, "NR3")) goto out;
	if (!check_data_model_value (proxy, 159, 1, G_TYPE_STRING, "CCC")) goto out;
	if (!check_data_model_value (proxy, 159, 2, G_TYPE_STRING, "321")) goto out;
	if (!check_data_model_value (proxy, 159, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 159, 5, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 0, G_TYPE_STRING, "NR4")) goto out;
	if (!check_data_model_value (proxy, 160, 1, G_TYPE_STRING, "DDD")) goto out;
	if (!check_data_model_value (proxy, 160, 2, G_TYPE_STRING, "330")) goto out;
	if (!check_data_model_value (proxy, 160, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 160, 5, G_TYPE_STRING, NULL)) goto out;
	
	/*
	 * Clean changes
	 */
	declare_expected_signals ("R160/R159", "Cancel all changes");
	gda_data_proxy_cancel_all_changes (GDA_DATA_PROXY (proxy));
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 159)) goto out;

	/*
	 * Test specific if a NULL row is prepended
	 */
	if (prepend_null_row) {
		/*
		 * Try to remove first row
		 */
		if (gda_data_model_remove_row (proxy, 0, NULL)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: Should not be able to remove row 0 (as it is the prepended row)\n");
#endif
			goto out;
		}

		/*
		 * Try to alter first row
		 */
		values = make_values_list (0, G_TYPE_STRING, "MyCity3", G_TYPE_STRING, "ZZZ", G_TYPE_STRING, "787", (GType) 0);
		if (gda_data_model_set_values (proxy, 0, values, NULL)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: Should not be able to alter row 0 (as it is the prepended row)\n");
#endif
			free_values_list (values);
			goto out;
		}
		free_values_list (values);
	}
	 

	/* end */
	retval = TRUE;
 out:
	g_object_unref (proxy);
	return retval;
}

/*
 * list of (GType, value as string) pairs ended with a 0 (for type)
 */
static GList *
make_values_list (gint dummy, ...)
{
	GList *list = NULL;
	va_list ap;

	gchar *str;
	GType type;

	va_start (ap, dummy);
	for (type = va_arg (ap, GType); type != 0; type = va_arg (ap, GType)) {
		GValue *value;
		str = va_arg (ap, gchar *);
		if (str) {
			value = gda_value_new (type);
			g_assert (gda_value_set_from_string (value, str, type));
		}
		else
			value = NULL;
		list = g_list_append (list, value);
	}
	va_end (ap);
	return list;
}

static void
free_values_list (GList *list)
{
	GList *l;
	for (l = list; l; l = l->next)
		if (l->data)
			gda_value_free ((GValue *) l->data);
	g_list_free (list);
}


/*
 *
 * Signals test implementation
 *
 */

typedef struct {
	gchar sig;
	gint  row;
} ASignal;

GArray      *expected_signals;
const gchar *expected_signals_name;
guint         expected_signals_index;

/* Format is "<nb><type><row_nb>[/...]"
 * for example: 
 *   - 1U123 means 1 update of row 123
 *   - 2U123 means 2 updates, 1 for row 123 and 1 for row 124
 *   - 2I4 means 2 inserts, 1 for row 4 and 1 for row 5
 *   - 2R6 means 2 deletes, all for row 6
 *
 * Use lower case to avoid taking into account the prepend_null_row variable
 */
static void
declare_expected_signals (const gchar *expected, const gchar *name)
{
#ifdef CHECK_EXTRA_INFO
	g_print (">>> NEW_TEST: %s (defer_sync=%s, prepend_null_row=%s)\n", name,
		 defer_sync ? "TRUE" : "FALSE", prepend_null_row ? "TRUE" : "FALSE");
#endif
	expected_signals_name = name;
	if (!expected)
		return;

	expected_signals = g_array_new (FALSE, TRUE, sizeof (ASignal));
	const gchar *ptr;
	for (ptr = expected; *ptr;) {
		gint i, nb = 1, row = -1;
		gchar type;
		if ((*ptr <= '9') && (*ptr >= '0')) {
			nb = atoi (ptr);
			for (; *ptr; ptr++)
				if ((*ptr > '9') || (*ptr < '0'))
					break;
		}
		type = *ptr;
		ptr++;
		if ((*ptr <= '9') && (*ptr >= '0')) {
			row = atoi (ptr);
			for (; *ptr; ptr++)
				if ((*ptr > '9') || (*ptr < '0'))
					break;
		}
		if (*ptr == '/')
			ptr++;
		for (i = 0; i < nb; i++) {
			gint sigrow;
			gboolean use_prepend_null_row = TRUE;
			if (type >= 'a') {
				type += 'A' - 'a';
				use_prepend_null_row = FALSE;
			}
			switch (type) {
			case 'U':
			case 'I':
				sigrow = row + i;
				break;
			case 'R':
				sigrow = row;
				break;
			default:
				g_assert_not_reached ();
			}

			ASignal sig;
			sig.sig = type;
			if (use_prepend_null_row)
				sig.row = ADJUST_ROW (sigrow);
			else
				sig.row = sigrow;
			g_array_append_val (expected_signals, sig);
		}
	}

	expected_signals_index = 0;
}

static void
clean_expected_signals (GdaDataModel *model)
{
	if (!expected_signals)
		return;
	wait_for_signals ();
	if (expected_signals_index != expected_signals->len) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: There are some non received signals:\n");
		for (; expected_signals_index < expected_signals->len; expected_signals_index++) {
			ASignal sig = g_array_index (expected_signals, ASignal, expected_signals_index);
			g_print ("\t%c for row %d\n", sig.sig, sig.row);
		}
#endif
		exit (1);
	}
	g_array_free (expected_signals, TRUE);
	expected_signals = NULL;

	if (prepend_null_row) {
		gint col, ncols;
		/* check that row 0 is all NULL */
		if (gda_data_model_get_n_rows (model) <= 0) {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: As the \"prepend-null-entry\" property is set, there should always be a first "
				 "all-NULL entry\n");
#endif
			exit (1);
		}
		ncols = gda_data_model_get_n_columns (model);
		for (col = 0; col < ncols; col++) {
			GError *lerror = NULL;
			const GValue *value = gda_data_model_get_value_at (model, col, 0, &lerror);
			if (!value) {
#ifdef CHECK_EXTRA_INFO
				g_print ("Can't get data model's value: %s\n",
					 lerror && lerror->message ? lerror->message : "No detail");
#endif
				exit (1);
			}
			else if (! gda_value_is_null (value)) {
#ifdef CHECK_EXTRA_INFO
				g_print ("ERROR: As the \"prepend-null-entry\" property is set, there should always be a first "
					 "all-NULL entry, got '%s' at column %d\n", gda_value_stringify (value), col);
#endif
				exit (1);
			}
		}
	}
#ifdef CHECK_EXTRA_INFO
	g_print (">>> END OF TEST %s\n", expected_signals_name);
#endif
}

static gboolean
wait_timeout (GMainLoop *loop)
{
        g_main_loop_quit (loop);
#ifdef CHECK_EXTRA_INFO
	g_print ("End of wait.\n");
#endif
        return FALSE; /* remove the time out */
}

static void
wait_for_signals (void)
{
	GMainLoop *loop;
	guint ms;
	if (!defer_sync)
		return;

	ms = ((expected_signals->len / 5)+1) * 100; /* nice heuristic, heu? */
	loop = g_main_loop_new (NULL, FALSE);
#ifdef CHECK_EXTRA_INFO
	g_print ("Waiting %d ms...\n", ms);
#endif
	g_timeout_add (ms, (GSourceFunc) wait_timeout, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

static void
proxy_reset_cb (G_GNUC_UNUSED GdaDataModel *model, G_GNUC_UNUSED gchar *detail)
{
#ifdef CHECK_EXTRA_INFO
	g_print ("RESET\n");
#endif
}

static void
proxy_row_cb (G_GNUC_UNUSED GdaDataModel *model, gint row, gchar *detail)
{
	gchar *str;

	switch (*detail) {
		case 'I':
			str = g_strdup_printf ("row %d inserted", row);
			break;
		case 'U':
			str = g_strdup_printf ("row %d updated", row);
			break;
		case 'R':
			str = g_strdup_printf ("row %d removed", row);
			break;
		default:
			g_assert_not_reached ();
	}

	if (expected_signals) {
		if (expected_signals_index < expected_signals->len) {
			ASignal sig = g_array_index (expected_signals, ASignal, expected_signals_index);
			if (sig.sig != *detail) {
#ifdef CHECK_EXTRA_INFO
				g_print ("ERROR: %s: Expected signal %c for row %d, received %s (step %d)\n", expected_signals_name,
					 sig.sig, sig.row, str, expected_signals_index);
#endif
				exit (1);
			}
			if (sig.row != row) {
#ifdef CHECK_EXTRA_INFO
				g_print ("ERROR: %s: Expected signal for row %d, received %s (step %d)\n", expected_signals_name,
					 sig.row, str, expected_signals_index);
#endif
				exit (1);
			}
			expected_signals_index ++;
		}
		else {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: %s: Unexpected signal: %s\n", expected_signals_name, str);
#endif
			exit (1);
		}
	}
#ifdef CHECK_EXTRA_INFO
	else 
		g_print ("WARNING: Non tested signal: %s\n", str);
#endif

	g_free (str);
}

static gboolean
check_data_model_value (GdaDataModel *model, gint row, gint col, GType type, const gchar *value)
{
	GValue *gvalue;
	const GValue *mvalue;
	gboolean retval;
	GError *lerror = NULL;

	row = ADJUST_ROW_FOR_MODEL (model, row);

	if (value) {
		gvalue = gda_value_new (type);
		if (!gda_value_set_from_string (gvalue, value, type)) {
			gda_value_free (gvalue);
			return FALSE;
		}
	}
	else
		gvalue = gda_value_new_null ();	

	mvalue = gda_data_model_get_value_at (model, col, row, &lerror);
	if (!mvalue) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Can't get data model's value: %s\n",
			 lerror && lerror->message ? lerror->message : "No detail");
#endif
		return FALSE;
	}
	retval = !gda_value_compare (gvalue, mvalue);
	gda_value_free (gvalue);

#ifdef CHECK_EXTRA_INFO
	if (!retval) {
		gchar *str = gda_value_stringify (mvalue);
		g_print ("ERROR: At row=%d, col=%d, expected '%s' and got '%s'\n", row, col, value, str);
		g_free (str);
		g_print ("Model is:\n");
		gda_data_model_dump (model, stdout);
	}
#endif

	return retval;
}

static gboolean
check_data_model_n_rows (GdaDataModel *model, gint nrows)
{
	const GValue *value;

	nrows = ADJUST_ROW_FOR_MODEL (model, nrows);

	if (gda_data_model_get_n_rows (model) != nrows) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Number of rows is %d (expected %d)\n", gda_data_model_get_n_rows (model), nrows);
#endif
		return FALSE;
	}

	value = gda_data_model_get_value_at (model, 0, nrows, NULL);
	if (value) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Got a value at row %d, should not get any as it is out of bounds\n", nrows);
#endif
		return FALSE;
	}
	return TRUE;
}

static gboolean
check_proxy_set_filter (GdaDataProxy *proxy, const gchar *filter)
{
	GError *error = NULL;
	if (!gda_data_proxy_set_filter_expr (proxy, filter, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not set filter to '%s': %s\n", filter, 
			 error && error->message ? error->message : "No detail");
#endif
		if (error)
			g_error_free (error);
		return FALSE;
	}
	return TRUE;
}

static gboolean
check_proxy_row_is_deleted (GdaDataProxy *proxy, gint row, gboolean is_deleted)
{
	row = ADJUST_ROW (row);

	if (gda_data_proxy_row_is_deleted (proxy, row) != is_deleted) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Expected row %d %s which is not true\n", row, 
			 is_deleted ? "deleted": "not deleted");
#endif
		return FALSE;
	}
	return TRUE;
}

static gboolean
check_data_model_append_row (GdaDataModel *model)
{
	gint row;
	GError *error = NULL;
	row = gda_data_model_append_row (model, &error);
	if (row < 0) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to add a row: %s\n", error && error->message ? error->message : "No detail");
#endif
		if (error)
			g_error_free (error);
		return FALSE;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Added row %d\n", row);
#endif
	return TRUE;
}

static gboolean
check_data_model_set_values (GdaDataModel *model, gint row, GList *values)
{
	GError *error = NULL;
	row = ADJUST_ROW_FOR_MODEL (model, row);
	
	if (!gda_data_model_set_values (model, row, values, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to set values: %s\n", error && error->message ? error->message : "No detail");
#endif
		if (error)
			g_error_free (error);
		return FALSE;
	}

	/* check individual values */
	GList *list;
	gint col;
	for (col = 0, list = values; list; col++, list = list->next) {
		GError *lerror = NULL;
		const GValue *cvalue = gda_data_model_get_value_at (model, col, row, &lerror);
		if (!cvalue) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Can't get data model's value: %s\n",
				 lerror && lerror->message ? lerror->message : "No detail");
#endif
			return FALSE;
		}
		if (list->data && gda_value_compare (cvalue, (GValue *) list->data)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: Read value is not equal to set value: got '%s' and expected '%s'\n",
				 gda_value_stringify (cvalue), gda_value_stringify ((GValue *) list->data));
#endif
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
check_data_model_remove_row (GdaDataModel *model, gint row)
{
	GError *error = NULL;
	row = ADJUST_ROW_FOR_MODEL (model, row);

	if (!gda_data_model_remove_row (model, row, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to remove row: %s\n", error && error->message ? error->message : "No detail");
#endif
		if (error)
			g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

/* remove several rows */
static gboolean do_test_delete_rows (void)
{
#define FILE "city.csv"
	GError *error = NULL;
	gchar *file;
	GdaDataModel *import, *model, *proxy;
	GSList *errors;
	GdaSet *options;

	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}

	model = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!model) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}
	g_object_unref (import);

	proxy = (GdaDataModel *) gda_data_proxy_new (model);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	g_object_set (G_OBJECT (proxy), "defer-sync", defer_sync, 
		      "prepend-null-entry", prepend_null_row, NULL);

	gboolean retval = FALSE;

	g_signal_connect (G_OBJECT (proxy), "reset",
			  G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_connect (G_OBJECT (proxy), "row-inserted",
			  G_CALLBACK (proxy_row_cb), "I");
	g_signal_connect (G_OBJECT (proxy), "row-updated",
			  G_CALLBACK (proxy_row_cb), "U");
	g_signal_connect (G_OBJECT (proxy), "row-removed",
			  G_CALLBACK (proxy_row_cb), "R");

	
	/* 
	 * mark a proxy row to be deleted
	 */
	declare_expected_signals ("u3", "Prepare to delete row 3");
	gda_data_proxy_delete (GDA_DATA_PROXY (proxy), 3);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 158)) goto out;

	/* 
	 * mark a proxy row to be deleted
	 */
	declare_expected_signals ("u5", "Prepare to delete row 5");
	gda_data_proxy_delete (GDA_DATA_PROXY (proxy), 5);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 158)) goto out;

	/* 
	 * mark a proxy row to be deleted
	 */
	declare_expected_signals ("u1", "Prepare to delete row 1");
	gda_data_proxy_delete (GDA_DATA_PROXY (proxy), 1);
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 158)) goto out;

	if (!check_data_model_value (proxy, 12, 0, G_TYPE_STRING, "Tirana")) goto out;
	if (!check_data_model_value (proxy, 12, 2, G_TYPE_STRING, "270000")) goto out;
	if (!check_data_model_value (proxy, 12, 3, G_TYPE_STRING, "Tirana")) goto out;
	if (!check_data_model_value (proxy, 12, 5, G_TYPE_STRING, "270000")) goto out;

	/* commit changes */
	declare_expected_signals ("r1r4r2", "Commit row changes");

	if (! gda_data_proxy_apply_all_changes (GDA_DATA_PROXY (proxy), &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}

	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 155)) goto out;

	if (!check_data_model_value (model, 9, 0, G_TYPE_STRING, "Tirana")) goto out;
	if (!check_data_model_value (model, 9, 2, G_TYPE_STRING, "270000")) goto out;
	if (prepend_null_row) {
		if (!check_data_model_value (model, 0, 0, G_TYPE_STRING, "Herat")) goto out;
		if (!check_data_model_value (model, 1, 0, G_TYPE_STRING, "Mazar-e-Sharif")) goto out;
		if (!check_data_model_value (model, 2, 0, G_TYPE_STRING, "Benguela")) goto out;
	}
	else {
		if (!check_data_model_value (model, 0, 0, G_TYPE_STRING, "Oranjestad")) goto out;
		if (!check_data_model_value (model, 1, 0, G_TYPE_STRING, "Kabul")) goto out;
		if (!check_data_model_value (model, 2, 0, G_TYPE_STRING, "Qandahar")) goto out;
	}
	if (!check_data_model_value (model, 3, 0, G_TYPE_STRING, "Huambo")) goto out;
	if (!check_data_model_value (model, 4, 0, G_TYPE_STRING, "Lobito")) goto out;

	retval = TRUE;
 out:
	g_object_unref (model);
	g_object_unref (proxy);
	return retval;
}

/* remove several rows */
static gboolean do_test_modifs_persistance (void)
{
#define FILE "city.csv"
	GError *error = NULL;
	gchar *file;
	GdaDataModel *import, *model_1, *model_2, *proxy;
	GSList *errors;
	GdaSet *options;
	gint nrows, i;
	gboolean retval = FALSE;
	GValue *value;

	/* load complete city.csv file */
	file = g_build_filename (CHECK_FILES, "tests", "data-models", FILE, NULL);
	options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	import = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not load file '%s'\n", FILE);
#endif
		g_object_unref (import);
		return FALSE;
	}
	nrows = gda_data_model_get_n_rows (import);

	/* define model_1 and model_2: take one part of the file only:
	 * model_1 has a bit more than the 1st half of import, and model_2 has
	 * a bit more than the 2nd half of import */
	model_1 = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!model_1) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}
	for (i = 74 + 10; i < nrows; i++) {
		if (! gda_data_model_remove_row (model_1, 74 + 10, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: Could not trim end of GdaDataModelArray: %s\n", 
				 error && error->message ? error->message : "No detail");
#endif
			g_error_free (error);
			return FALSE;
		}
	}

	model_2 = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!model_2) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not copy GdaDataModelImport into a GdaDataModelArray: %s\n", 
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		return FALSE;
	}
	for (i = 0; i < 74 - 10; i++) {
		if (! gda_data_model_remove_row (model_2, 0, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("ERROR: Could not trim start of GdaDataModelArray: %s\n", 
				 error && error->message ? error->message : "No detail");
#endif
			g_error_free (error);
			return FALSE;
		}
	}
	g_object_unref (import);

	proxy = (GdaDataModel *) gda_data_proxy_new (model_1);
	if (!proxy) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Could not create GdaDataProxy\n");
#endif
		return FALSE;
	}
	g_object_set (G_OBJECT (proxy), "cache-changes", TRUE, NULL);
	g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, NULL);
	gda_data_proxy_set_sample_size (GDA_DATA_PROXY (proxy), 0);
	g_object_set (G_OBJECT (proxy), "defer-sync", defer_sync, 
		      "prepend-null-entry", prepend_null_row, NULL);

	g_signal_connect (G_OBJECT (proxy), "reset", G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_connect (G_OBJECT (proxy), "row-inserted", G_CALLBACK (proxy_row_cb), "I");
	g_signal_connect (G_OBJECT (proxy), "row-updated", G_CALLBACK (proxy_row_cb), "U");
	g_signal_connect (G_OBJECT (proxy), "row-removed", G_CALLBACK (proxy_row_cb), "R");

	/* 
	 * update a row
	 */
	declare_expected_signals ("U74", "update a row in proxied model_1 (1st half)");
	g_value_set_string (value = gda_value_new (G_TYPE_STRING), "BigCity");
	if (!gda_data_model_set_value_at (proxy, 0, ADJUST_ROW (74), value, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to set value: %s\n", error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 74, 0, G_TYPE_STRING, "BigCity")) goto out;
	if (!check_data_model_value (proxy, 74, 2, G_TYPE_STRING, "296226")) goto out;
	if (!check_data_model_value (proxy, 74, 3, G_TYPE_STRING, "Tigre")) goto out;
	if (!check_data_model_value (proxy, 74, 5, G_TYPE_STRING, "296226")) goto out;

	/*
	 * insert a row
	 */
	GList *values;
	declare_expected_signals ("I84", "insert a row in proxied model_1");
	if (! check_data_model_append_row (proxy)) goto out;
	clean_expected_signals (proxy);
	if (!check_data_model_n_rows (proxy, 85)) goto out;

	values = make_values_list (0, G_TYPE_STRING, "BigCity3", G_TYPE_STRING, NULL, G_TYPE_STRING, "123456", (GType) 0);
	declare_expected_signals ("U84", "Set values of new proxied row");
	if (!check_data_model_set_values (proxy, 84, values)) goto out;
	free_values_list (values);
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 84, 0, G_TYPE_STRING, "BigCity3")) goto out;
	if (!check_data_model_value (proxy, 84, 1, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 84, 2, G_TYPE_STRING, "123456")) goto out;
	if (!check_data_model_value (proxy, 84, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 84, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 84, 5, G_TYPE_STRING, NULL)) goto out;

	/*
	 * change to proxied model_2 & update a row
	 */
	g_print ("Setting proxied data model to model_2\n");
	g_object_set (G_OBJECT (proxy), "model", model_2, NULL);

	check_data_model_n_rows (proxy, 95);

	declare_expected_signals ("U21", "update a row in proxied model_2 (2nd half)");
	g_value_set_string (value = gda_value_new (G_TYPE_STRING), "BigCity2");
	if (!gda_data_model_set_value_at (proxy, 0, ADJUST_ROW (21), value, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("ERROR: Failed to set value: %s\n", error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	clean_expected_signals (proxy);
	if (!check_data_model_value (proxy, 21, 0, G_TYPE_STRING, "BigCity2")) goto out;
	if (!check_data_model_value (proxy, 21, 2, G_TYPE_STRING, "92273")) goto out;
	if (!check_data_model_value (proxy, 21, 3, G_TYPE_STRING, "Cairns")) goto out;
	if (!check_data_model_value (proxy, 21, 5, G_TYPE_STRING, "92273")) goto out;

	if (!check_data_model_value (proxy, 94, 0, G_TYPE_STRING, "BigCity3")) goto out;
	if (!check_data_model_value (proxy, 94, 1, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 94, 2, G_TYPE_STRING, "123456")) goto out;
	if (!check_data_model_value (proxy, 94, 3, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 94, 4, G_TYPE_STRING, NULL)) goto out;
	if (!check_data_model_value (proxy, 94, 5, G_TYPE_STRING, NULL)) goto out;

	/*
	 * change proxied model and make sure the changes are cached
	 */
	g_print ("Seting proxied data model to model_1\n");
	g_object_set (G_OBJECT (proxy), "model", model_1, NULL);
	if (!check_data_model_value (proxy, 74, 0, G_TYPE_STRING, "BigCity")) goto out;
	if (!check_data_model_value (proxy, 74, 2, G_TYPE_STRING, "296226")) goto out;
	if (!check_data_model_value (proxy, 74, 3, G_TYPE_STRING, "Tigre")) goto out;
	if (!check_data_model_value (proxy, 74, 5, G_TYPE_STRING, "296226")) goto out;

	g_print ("Setting proxied data model to model_2\n");
	g_object_set (G_OBJECT (proxy), "model", model_2, NULL);
	if (!check_data_model_value (proxy, 21, 0, G_TYPE_STRING, "BigCity2")) goto out;
	if (!check_data_model_value (proxy, 21, 2, G_TYPE_STRING, "92273")) goto out;
	if (!check_data_model_value (proxy, 21, 3, G_TYPE_STRING, "Cairns")) goto out;
	if (!check_data_model_value (proxy, 21, 5, G_TYPE_STRING, "92273")) goto out;

	if (!check_data_model_value (proxy, 10, 0, G_TYPE_STRING, "BigCity")) goto out;
	if (!check_data_model_value (proxy, 10, 2, G_TYPE_STRING, "296226")) goto out;
	if (!check_data_model_value (proxy, 10, 3, G_TYPE_STRING, "Tigre")) goto out;
	if (!check_data_model_value (proxy, 10, 5, G_TYPE_STRING, "296226")) goto out;

	//gda_data_model_dump (proxy, NULL);

	g_print ("Seting proxied data model to model_1\n");
	g_object_set (G_OBJECT (proxy), "model", model_1, NULL);
	if (!check_data_model_value (proxy, 74, 0, G_TYPE_STRING, "BigCity")) goto out;
	if (!check_data_model_value (proxy, 74, 2, G_TYPE_STRING, "296226")) goto out;
	if (!check_data_model_value (proxy, 74, 3, G_TYPE_STRING, "Tigre")) goto out;
	if (!check_data_model_value (proxy, 74, 5, G_TYPE_STRING, "296226")) goto out;

	gda_data_model_dump (proxy, NULL);

	g_print ("Setting proxied data model to model_2\n");
	g_object_set (G_OBJECT (proxy), "model", model_2, NULL);
	declare_expected_signals ("R94/U21/U10", "Resetting all the changes");
	gda_data_proxy_cancel_all_changes (GDA_DATA_PROXY (proxy));
	clean_expected_signals (proxy);

	retval = TRUE;
 out:
	g_signal_handlers_disconnect_by_func (G_OBJECT (proxy), G_CALLBACK (proxy_reset_cb), NULL);
	g_signal_handlers_disconnect_by_func (G_OBJECT (proxy), G_CALLBACK (proxy_row_cb), "I");
	g_signal_handlers_disconnect_by_func (G_OBJECT (proxy), G_CALLBACK (proxy_row_cb), "U");
	g_signal_handlers_disconnect_by_func (G_OBJECT (proxy), G_CALLBACK (proxy_row_cb), "R");

	g_object_unref (model_1);
	g_object_unref (model_2);
	g_object_unref (proxy);
	return retval;
}
