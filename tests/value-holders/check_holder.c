/*
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include <string.h>
#include "common.h"

/*
 * Signals testing
 */
GSList *signals_list;
typedef struct {
	GObject  *obj;
	gchar    *signal_name;
} EmittedSignal;
static void     emitted_signal_add (EmittedSignal *es);
static void     emitted_signals_reset (void);
static gboolean emitted_signals_find (gpointer obj, const gchar *signal_name, GError **error);
static gboolean emitted_signals_notfind (gpointer obj, const gchar *signal_name, GError **error);
static gboolean emitted_signals_check_empty (gpointer obj, const gchar *signal_name, GError **error);
static void     emitted_signals_monitor_holder (GdaHolder *h);

/* 
 * tests
 */
typedef gboolean (*TestFunc) (GError **);
static gboolean test1 (GError **error);
static gboolean test2 (GError **error);
static gboolean test3 (GError **error);
static gboolean test4 (GError **error);
static gboolean test5 (GError **error);
static gboolean test6 (GError **error);
static gboolean test7 (GError **error);
static gboolean test8 (GError **error);
static gboolean test9 (GError **error);
static gboolean test10 (GError **error);
static gboolean test11 (GError **error);
static gboolean test12 (GError **error);
static gboolean test13 (GError **error);

TestFunc tests[] = {
	test1,
	test2,
	test3,
	test4,
	test5,
	test6,
	test7,
	test8,
	test9,
	test10,
	test11,
	test12,
	test13
};

int 
main ()
{
#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif
	gda_init ();

	guint failures = 0;
	guint i, ntests = 0;
  
	for (i = 0; i < sizeof (tests) / sizeof (TestFunc); i++) {
		GError *error = NULL;
		if (! tests[i] (&error)) {
			g_print ("Test %d failed: %s\n", i+1, 
				 error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
			failures ++;
		}
		ntests ++;
	}

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);

	return failures != 0 ? 1 : 0;
}

/*
 * Individual GdaHolder's manipulations
 */
static gboolean
test1 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value, *copy;

	h = gda_holder_new (G_TYPE_STRING, "id");
	emitted_signals_monitor_holder (h);
	cvalue = gda_holder_get_value (h);
	if (!gda_value_is_null (cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Expecting NULL value from new GdaHolder");
		return FALSE;
	}

	/***/
	value = gda_value_new_from_string ("my string", G_TYPE_STRING);
	emitted_signals_reset ();
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	value = gda_value_new_from_string ("my other string", G_TYPE_STRING);
	copy = gda_value_copy (value);
	emitted_signals_reset ();
	if (!gda_holder_take_value (h, value, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (copy, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (copy);
	g_object_unref (h);

	return TRUE;
}

/*
 * gda_holder_new_inline() test
 */
static gboolean
test2 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value;

	h = gda_holder_new_boolean ("ABOOL", TRUE);
	value = gda_value_new_from_string ("TRUE", G_TYPE_BOOLEAN);
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);
	if (strcmp (gda_holder_get_id (h), "ABOOL")) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's ID is incorrect");
		return FALSE;
	}
	g_object_unref (h);

	/***/
	h = gda_holder_new_string ("Astring", "A string value");
	value = gda_value_new_from_string ("A string value", G_TYPE_STRING);
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);
	if (strcmp (gda_holder_get_id (h), "Astring")) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's ID is incorrect");
		return FALSE;
	}

	/***/
	h = gda_holder_new_int ("AnInt",  15);
	value = gda_value_new_from_string ("15", G_TYPE_INT);
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);
	if (strcmp (gda_holder_get_id (h), "AnInt")) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's ID is incorrect");
		return FALSE;
	}

	g_object_unref (h);

	return TRUE;
}

/*
 * gda_holder_new_inline() test
 */
static gboolean
test3 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value;

	h = gda_holder_new_boolean ("ABOOL", TRUE);
	emitted_signals_monitor_holder (h);
	if (!gda_holder_is_valid (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder is invalid");
		return FALSE;
	}

	/***/
	gda_holder_force_invalid (h);
	if (gda_holder_is_valid (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder is valid");
		return FALSE;
	}
	cvalue = gda_holder_get_value (h);
	if (cvalue) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}

	/***/
	emitted_signals_reset ();
	value = gda_value_new_from_string ("FALSE", G_TYPE_BOOLEAN);
	if (! gda_holder_take_value (h, value, error))
		return FALSE;
	if (!gda_holder_is_valid (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder is invalid");
		return FALSE;
	}

	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	g_object_unref (h);

	return TRUE;
}

/*
 * default value to NULL
 */
static gboolean
test4 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value;

	h = gda_holder_new_string ("ID", "A string");
	emitted_signals_monitor_holder (h);
	
	cvalue = gda_holder_get_default_value (h);
	if (cvalue) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder should not have a default value");
		return FALSE;
	}

	/***/

	if (gda_holder_set_value_to_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Should not set GdaHolder's value to default");
		return FALSE;
	}
  printf ("Checking signals \"changed\" after try to set to default\n");
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	/** Set a default value to NULL */
	value = gda_value_new_null ();
	gda_holder_set_default_value (h, value);
  printf ("Checking signals \"changed\" after change default value\n");
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_default_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's default value is invalid");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	if (gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should not be default");
		return FALSE;
	}

	/** Check value */
	if (! gda_holder_set_value_to_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Could not set GdaHolder's value to default");
		return FALSE;
	}
  printf ("Find \"changed\" signals after reset to default\n");
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
  printf ("Checking empty signals \"changed\" after reset to default\n");
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (cvalue) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}

  printf ("Changing value to 'hey'\n");
	/** set value to "hey" */
	value = gda_value_new_from_string ("hey", G_TYPE_STRING);
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
  printf ("Checking signals \"changed\"");
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}
	gda_value_free (value);

	/** Set a default value to NULL */
	value = gda_value_new_null ();
	gda_holder_set_default_value (h, value);
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_default_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's default value is invalid");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	if (gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should not be default");
		return FALSE;
	}

	g_object_unref (h);

	return TRUE;
}

/*
 * default value to a string in a string holder
 */
static gboolean
test5 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value, *defvalue;

	h = gda_holder_new_string ("ID", "A string");
	emitted_signals_monitor_holder (h);
	emitted_signals_reset ();

	cvalue = gda_holder_get_default_value (h);
	if (cvalue) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder should not have a default value");
		return FALSE;
	}

	/** Set a default value to "ABC" */
	defvalue = gda_value_new_from_string ("ABC", G_TYPE_STRING);
	gda_holder_set_default_value (h, defvalue);
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_default_value (h);
	if (!cvalue || gda_value_compare (defvalue, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's default value is invalid");
		return FALSE;
	}

	/***/
	if (gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should not be default");
		return FALSE;
	}

	/** Check value */
	if (! gda_holder_set_value_to_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Could not set GdaHolder's value to default");
		return FALSE;
	}
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (defvalue, cvalue)) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}

	/** set value to "hey" */
	value = gda_value_new_from_string ("hey", G_TYPE_STRING);
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}
	gda_value_free (value);
	
	/***/
	if (gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should not be default");
		return FALSE;
	}

	/** set value to "ABC" */
	value = gda_value_new_from_string ("ABC", G_TYPE_STRING);
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	if (!gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should be default");
		return FALSE;
	}

	gda_value_free (defvalue);
	g_object_unref (h);

	return TRUE;
}

/*
 * default value to an int in a string holder
 */
static gboolean
test6 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value, *defvalue;

	h = gda_holder_new_string ("ID", "A string");
	cvalue = gda_holder_get_default_value (h);
	emitted_signals_monitor_holder (h);
	emitted_signals_reset ();
	if (cvalue) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder should not have a default value");
		return FALSE;
	}

	/** Set a default value to 1234 */
	defvalue = gda_value_new_from_string ("1234", G_TYPE_INT);
	gda_holder_set_default_value (h, defvalue);
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_default_value (h);
	if (!cvalue || gda_value_compare (defvalue, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's default value is invalid");
		return FALSE;
	}

	/***/
	if (gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should not be default");
		return FALSE;
	}

	/** Check value */
	if (! gda_holder_set_value_to_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Could not set GdaHolder's value to default");
		return FALSE;
	}
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h);
	if (cvalue) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}

	/** set value to "hey" */
	value = gda_value_new_from_string ("hey", G_TYPE_STRING);
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		tests_common_display_value ("cvalue", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is invalid");
		return FALSE;
	}
	gda_value_free (value);
	
	/***/
	if (gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should not be default");
		return FALSE;
	}

	gda_value_free (defvalue);
	g_object_unref (h);

	return TRUE;
}

/*
 * binding
 */
static gboolean
test7 (GError **error)
{
	GdaHolder *h1, *h2;
	const GValue *cvalue;
	GValue *value;

	h1 = gda_holder_new_string ("Slave", "Slave string");
	h2 = gda_holder_new_string ("Master", "Master string");
	emitted_signals_monitor_holder (h1);
	emitted_signals_monitor_holder (h2);
	emitted_signals_reset ();

	if (! gda_holder_set_bind (h1, h2, error))
		return FALSE;
	if (!emitted_signals_find (h1, "changed", error))
		return FALSE;
	if (!emitted_signals_notfind (h2, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	value = gda_value_new_from_string ("Master string", G_TYPE_STRING);
	cvalue = gda_holder_get_value (h1);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		tests_common_display_value ("Slave", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Slave GdaHolder's value is wrong");
		return FALSE;
	}
	gda_value_free (value);
	
	/***/
	value = gda_value_new_from_string ("A string", G_TYPE_STRING);
	if (!gda_holder_set_value (h1, value, error))
		return FALSE;
	if (!emitted_signals_find (h1, "changed", error))
		return FALSE;
	if (!emitted_signals_notfind (h2, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h1);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		tests_common_display_value ("Slave", cvalue);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Slave GdaHolder's value is wrong");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	value = gda_value_new_from_string ("A string 2", G_TYPE_STRING);
	if (!gda_holder_set_value (h2, value, error))
		return FALSE;
	if (!emitted_signals_find (h1, "changed", error))
		return FALSE;
	if (!emitted_signals_find (h2, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h2);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Master GdaHolder's value is wrong");
		return FALSE;
	}
	cvalue = gda_holder_get_value (h1);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Slave GdaHolder's value is wrong");
		return FALSE;
	}

	/***/
	if (! gda_holder_set_bind (h1, NULL, error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h1);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Slave GdaHolder's value is wrong");
		return FALSE;
	}
	gda_value_free (value);

	value = gda_value_new_from_string ("Another string", G_TYPE_STRING);
	if (!gda_holder_set_value (h1, value, error))
		return FALSE;
	if (!emitted_signals_find (h1, "changed", error))
		return FALSE;
	if (!emitted_signals_notfind (h2, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	cvalue = gda_holder_get_value (h1);
	if (!cvalue || gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Slave GdaHolder's value is wrong");
		return FALSE;
	}

	cvalue = gda_holder_get_value (h2);
	if (!cvalue || !gda_value_compare (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Master GdaHolder's value is wrong");
		return FALSE;
	}
	gda_value_free (value);

	

	g_object_unref (h1);
	g_object_unref (h2);

	return TRUE;
}

/*
 * default value to NULL
 */
static gboolean
test8 (GError **error)
{
	GdaHolder *h;
	GValue *value;

	h = gda_holder_new (G_TYPE_STRING, "id");
	emitted_signals_monitor_holder (h);
	emitted_signals_reset ();

	/** Set a default value to NULL */
	value = gda_value_new_null ();
	gda_holder_set_default_value (h, value);
	gda_value_free (value);

	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	/***/
	if (!gda_holder_value_is_default (h)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value should be default");
		return FALSE;
	}

	g_object_unref (h);
	return TRUE;
}

/*
 * Source data model
 */
static gboolean
test9 (GError **error)
{
	GdaHolder *h;
	GdaDataModel *model;
	gchar *model_data="1,John\n2,Jack";
	gint col;

	h = gda_holder_new (G_TYPE_STRING, "id");
	emitted_signals_monitor_holder (h);
	emitted_signals_reset ();

	model = gda_data_model_import_new_mem (model_data, TRUE, NULL);
	
	if (!gda_holder_set_source_model (h, model, 1, error)) 
		return FALSE;
	if (!emitted_signals_find (h, "source-changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "source-changed", error))
		return FALSE;

	if ((gda_holder_get_source_model (h, &col) != model) || (col != 1)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's source is wrong");
		return FALSE;
	}
		
	/***/
	if (!gda_holder_set_source_model (h, model, 0, error))
		return FALSE;
	if (!emitted_signals_find (h, "source-changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "source-changed", error))
		return FALSE;
	if ((gda_holder_get_source_model (h, &col) != model) || (col != 0)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's source is wrong");
		return FALSE;
	}

	/***/
	if (!gda_holder_set_source_model (h, NULL, 0, error))
		return FALSE;
	if (!emitted_signals_find (h, "source-changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "source-changed", error))
		return FALSE;

	if (gda_holder_get_source_model (h, NULL)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's source should be NULL");
		return FALSE;
	}


	g_object_unref (model);
	g_object_unref (h);
	return TRUE;
}

/*
 * "validate-change" signal
 */

static GError *
t10_validate_change_cb (G_GNUC_UNUSED GdaHolder *h, const GValue *value, gchar *token)
{
	/* only accept GDA_VALUE_NULL or the "hi!" G_TYPE_STRING value */
	g_assert (!strcmp (token, "AToken"));

	if (gda_value_is_null (value) || !strcmp (g_value_get_string (value), "hi!")) {
		g_print ("GdaHolder change accepted\n");
		return NULL;
	}
	else {
		GError *error = NULL;
		g_print ("GdaHolder change refused\n");
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "GdaHolder change refused!");
		return error;
	}
}

static gboolean
test10 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value;

	h = gda_holder_new (G_TYPE_STRING, "id");
	emitted_signals_monitor_holder (h);

	/***/
	value = gda_value_new_from_string ("my string", G_TYPE_STRING);
	emitted_signals_reset ();
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;
	gda_value_free (value);
	
	/***/
	g_signal_connect (G_OBJECT (h), "validate-change",
			  G_CALLBACK (t10_validate_change_cb), "AToken");

	/***/
	value = gda_value_new_from_string ("my other string", G_TYPE_STRING);
	emitted_signals_reset ();
	if (gda_holder_set_value (h, value, error)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's change should have failed");
		return FALSE;
	}
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;
	gda_value_free (value);

	/***/
	value = gda_value_new_from_string ("my string", G_TYPE_STRING);
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_differ (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);

	
	/***/
	value = gda_value_new_from_string ("hi!", G_TYPE_STRING);
	emitted_signals_reset ();
	if (!gda_holder_set_value (h, value, error)) 
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	/***/
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_differ (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);

	g_object_unref (h);

	return TRUE;
}

static gboolean
test11 (GError **error)
{
	GdaHolder *h;
	const GValue *cvalue;
	GValue *value;

	h = gda_holder_new (GDA_TYPE_NULL, "id");
	emitted_signals_monitor_holder (h);

	/***/
	value = gda_value_new_from_string ("my string", G_TYPE_STRING);
	emitted_signals_reset ();
	if (gda_holder_set_value (h, value, NULL)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's change should have failed");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	if (!gda_holder_set_value (h, NULL, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;
	
	/***/
	g_object_set (G_OBJECT (h), "g-type", G_TYPE_STRING, NULL);
	value = gda_value_new_from_string ("my other string", G_TYPE_STRING);
	emitted_signals_reset ();
	if (!gda_holder_set_value (h, value, error))
		return FALSE;
	if (!emitted_signals_find (h, "changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (NULL, "changed", error))
		return FALSE;

	/***/
	cvalue = gda_holder_get_value (h);
	if (!cvalue || gda_value_differ (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);

	g_object_unref (h);

	return TRUE;
}

static gboolean
test12 (GError **error)
{
	GdaHolder *h;

	h = gda_holder_new (GDA_TYPE_NULL, "id");

	GdaHolder *copy;
	gchar *name;

	g_object_set (G_OBJECT (h), "name", "thename", NULL);
	copy = gda_holder_copy (h);

	g_object_get (G_OBJECT (copy), "name", &name, NULL);
	if (strcmp (name, "thename")) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "gda_holder_copy() did not copy the name");
		return FALSE;
	}
	g_free (name);

	g_object_unref (copy);
	g_object_unref (h);

	return TRUE;
}


static gboolean
test13 (GError **error)
{
	GdaHolder *h1, *h2;
	h1 = gda_holder_new (GDA_TYPE_NULL, "id1");
	h2 = gda_holder_new (GDA_TYPE_NULL, "id2");
	if (! gda_holder_set_bind (h1, h2, error)) {
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_set (h2, "g-type", G_TYPE_STRING, NULL);
	if (gda_holder_get_g_type (h1) != G_TYPE_STRING) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "Bind-to holder type set did not propagate to holder's type, case 1");
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_unref (h1);
	g_object_unref (h2);
	
	/* another test */
	h1 = gda_holder_new (GDA_TYPE_NULL, "1");
	h2 = gda_holder_new (G_TYPE_INT, "2");
	if (! gda_holder_set_bind (h1, h2, error)) {
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	if (gda_holder_get_g_type (h1) != G_TYPE_INT) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "Bind-to holder type set did not propagate to holder's type, case 2");
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_unref (h1);
	g_object_unref (h2);

	/* another test */
	h1 = gda_holder_new (G_TYPE_STRING, "1");
	h2 = gda_holder_new (GDA_TYPE_NULL, "2");
	if (! gda_holder_set_bind (h1, h2, error)) {
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_set (h2, "g-type", G_TYPE_STRING, NULL);
	if (gda_holder_get_g_type (h1) != G_TYPE_STRING) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "Holder type changed when it should not have, case 1");
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	if (gda_holder_get_bind (h1) != h2) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "Bind broken when it should not have been");
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_unref (h1);
	g_object_unref (h2);

	/* another test */
	h1 = gda_holder_new (G_TYPE_STRING, "1");
	h2 = gda_holder_new (GDA_TYPE_NULL, "2");
	if (! gda_holder_set_bind (h1, h2, error)) {
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_set (h2, "g-type", G_TYPE_INT, NULL);
	if (gda_holder_get_g_type (h1) != G_TYPE_STRING) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "Holder type changed when it should not have, case 2");
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	if (gda_holder_get_bind (h1) == h2) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "Bind not broken when it should have been");
		g_object_unref (h1);
		g_object_unref (h2);
		return FALSE;
	}
	g_object_unref (h1);
	g_object_unref (h2);

	return TRUE;
}

/*
 * Signals testing
 */
static void
emitted_signal_add (EmittedSignal *es)
{
	signals_list = g_slist_append (signals_list, es);
}

static void
emitted_signals_reset (void)
{
	g_slist_free_full (signals_list, (GDestroyNotify) g_free);
	signals_list = NULL;
}

static gboolean
emitted_signals_find (gpointer obj, const gchar *signal_name, GError **error)
{
	GSList *list;
	for (list = signals_list; list; list = list->next) {
		EmittedSignal *es = (EmittedSignal *) list->data;
		if ((es->obj == obj) && (!strcmp (es->signal_name, signal_name))) {
			signals_list = g_slist_delete_link (signals_list, list);
			return TRUE;
		}
	}
	g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
		     "Signal \"%s\" has not been emitted", signal_name);
	g_object_unref (obj);
	return FALSE;
}

static gboolean
emitted_signals_notfind (gpointer obj, const gchar *signal_name, GError **error)
{
	GSList *list;
	for (list = signals_list; list; list = list->next) {
		EmittedSignal *es = (EmittedSignal *) list->data;
		if ((es->obj == obj) && (!strcmp (es->signal_name, signal_name))) {
			signals_list = g_slist_delete_link (signals_list, list);
			g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "Signal \"%s\" has been emitted", signal_name);
			g_object_unref (obj);

			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
emitted_signals_check_empty (gpointer obj, const gchar *signal_name, GError **error)
{
	GSList *list;
	for (list = signals_list; list; list = list->next) {
		EmittedSignal *es = (EmittedSignal *) list->data;
		if ((!obj || (es->obj == obj)) && 
		    (!signal_name || (!strcmp (es->signal_name, signal_name)))) {
			g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "Signal \"%s\" has been emitted", es->signal_name);
			emitted_signals_reset ();
			return FALSE;
		}
	}
	return TRUE;
}

static void
changed_cb (GObject *obj)
{
	EmittedSignal *es;
	es = g_new0 (EmittedSignal, 1);
	es->obj = obj;
	es->signal_name = "changed";
	emitted_signal_add (es);
}

static void
source_changed_cb (GObject *obj)
{
	EmittedSignal *es;
	es = g_new0 (EmittedSignal, 1);
	es->obj = obj;
	es->signal_name = "source-changed";
	emitted_signal_add (es);
}

static void
emitted_signals_monitor_holder (GdaHolder *h)
{
	g_signal_connect (G_OBJECT (h), "changed",
			  G_CALLBACK (changed_cb), NULL);
	g_signal_connect (G_OBJECT (h), "source-changed",
			  G_CALLBACK (source_changed_cb), NULL);
}
