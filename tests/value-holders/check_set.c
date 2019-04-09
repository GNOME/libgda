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
	GObject   *obj;
	gchar     *signal_name;
	GdaHolder *holder;
} EmittedSignal;
static void     emitted_signal_add (EmittedSignal *es);
static void     emitted_signals_reset (void);
static gboolean emitted_signals_find (gpointer obj, const gchar *signal_name, GError **error);
/* Not used: static gboolean emitted_signals_notfind (gpointer obj, const gchar *signal_name, GError **error); */
static gboolean emitted_signals_check_empty (gpointer obj, const gchar *signal_name, GError **error);
static void     emitted_signals_monitor_set (GdaSet *set);

/* 
 * tests
 */
typedef gboolean (*TestFunc) (GError **);
static gboolean test1 (GError **error);
static gboolean test2 (GError **error);
static gboolean test3 (GError **error);
static gboolean test4 (GError **error);
static gboolean test5 (GError **error);

GHashTable *data;
TestFunc tests[] = {
	test1,
	test2,
	test3,
	test4,
	test5
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
  
	data = tests_common_load_data ("set.data");
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
 * new_inline
 */
static gboolean
test1 (GError **error)
{
	GdaSet *set;

	set = gda_set_new_inline (3, 
				  "H1", G_TYPE_STRING, "A string",
				  "H2", G_TYPE_INT, 1234,
				  "H3", G_TYPE_CHAR, 'r');

	if (!tests_common_check_set (data, "T1.1", set, error)) return FALSE;
	g_object_unref (set);

	/***/
	set = gda_set_new_inline (3, 
				  "H1", G_TYPE_STRING, "A string",
				  "H2", G_TYPE_INT, 1234,
				  "H1", G_TYPE_STRING, "Hey!");
	if (!tests_common_check_set (data, "T1.2", set, error)) return FALSE;
	g_object_unref (set);

	return TRUE;
}

/*
 * holders with sources
 */
static gboolean
test2 (GError **error)
{
	GdaSet *set;
	GdaHolder *h;
	GdaDataModel *model;	
	gchar *model_data="1,John\n2,Jack";

	model = gda_data_model_import_new_mem (model_data, TRUE, NULL);
	g_object_set_data (G_OBJECT (model), "name", "model1");
	
	set = gda_set_new_inline (3, 
				  "H1", G_TYPE_STRING, "A string",
				  "H2", G_TYPE_STRING, "1234",
				  "H3", G_TYPE_CHAR, 'r');
	emitted_signals_monitor_set (set);
	emitted_signals_reset ();

	h = gda_set_get_holder (set, "H1");
	if (!h) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Could not find GdaHolder H1");
		return FALSE;
	}

	if (!gda_holder_set_source_model (h, model, 1, error)) 
		return FALSE;
	if (!tests_common_check_set (data, "T2.1", set, error)) return FALSE;

	if (!emitted_signals_find (set, "public-data-changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (set, "public-data-changed", error))
		return FALSE;

	if (!tests_common_check_set (data, "T2.1", set, error)) return FALSE;

	/***/
	h = gda_set_get_holder (set, "H2");
	if (!gda_holder_set_source_model (h, model, 0, error)) 
		return FALSE;
	if (!tests_common_check_set (data, "T2.2", set, error)) return FALSE;

	if (!emitted_signals_find (set, "public-data-changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (set, "public-data-changed", error))
		return FALSE;

	/***/
	h = gda_set_get_holder (set, "H1");
	if (!gda_holder_set_source_model (h, NULL, 0, error)) 
		return FALSE;
	if (!tests_common_check_set (data, "T2.3", set, error)) return FALSE;

	if (!emitted_signals_find (set, "public-data-changed", error))
		return FALSE;
	if (!emitted_signals_check_empty (set, "public-data-changed", error))
		return FALSE;

	return TRUE;
}

/*
 * "validate-holder-change" signal
 */
static GError *
t3_validate_holder_change (G_GNUC_UNUSED GdaSet *set, GdaHolder *h, const GValue *value, gchar *token)
{
	/* only accept GDA_VALUE_NULL or 444 value */
	g_assert (!g_strcmp0 (token, "MyToken"));

	if (!strcmp (gda_holder_get_id (h), "H2")) {
		if (gda_value_is_null (value) || (g_value_get_int (value) == 444)) {
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
	else
		/* change accepted */
		return NULL;
}

static gboolean
test3 (GError **error)
{
	GdaSet *set;
	GdaHolder *h;
	GValue *value;
	const GValue *cvalue;
	GError *lerror = NULL;

	set = gda_set_new_inline (3, 
				  "H1", G_TYPE_STRING, "A string",
				  "H2", G_TYPE_INT, 1234,
				  "H3", G_TYPE_CHAR, 'r');

	g_signal_connect (G_OBJECT (set), "validate-holder-change",
			  G_CALLBACK (t3_validate_holder_change), "MyToken");

	h = gda_set_get_holder (set, "H2");
	g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 333);
	if (gda_holder_set_value (h, value, &lerror)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "gda_holder_set_value() should have been refused and failed");
		return FALSE;
	}
	gda_value_free (value);
	if (!lerror) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Returned GError should not be NULL");
		return FALSE;
	}
	if (strcmp (lerror->message, "GdaHolder change refused!")) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Returned GError's message is wrong, should be 'GdaHolder change refused!' and is '%s'",
			     lerror->message);
		return FALSE;
	}
	g_error_free (lerror);
	lerror = NULL;

	/***/
	g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 1234);
	cvalue = gda_set_get_holder_value (set, "H2");
	if (!cvalue || gda_value_differ (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);

	/***/
	g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 444);
	if (!gda_holder_set_value (h, value, error)) 
		return FALSE;
	
	/***/cvalue = gda_set_get_holder_value (set, "H2");
	if (!cvalue || gda_value_differ (value, cvalue)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "GdaHolder's value is incorrect");
		return FALSE;
	}
	gda_value_free (value);
	

	g_object_unref (set);

	return TRUE;
}

/*
 * "holder-attr-changed" signal test
 */
static gboolean
test4 (GError **error)
{
	GdaSet *set;
	GdaHolder *h;

	set = gda_set_new_inline (3,
				  "H1", G_TYPE_STRING, "A string",
				  "H2", G_TYPE_STRING, "1234",
				  "H3", G_TYPE_CHAR, 'r');
	emitted_signals_monitor_set (set);
	emitted_signals_reset ();

	h = gda_set_get_holder (set, "H1");
	if (!h) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Could not find GdaHolder H1");
		return FALSE;
	}
	g_object_set ((GObject*) h, "description", "Hello!", NULL);

	if (!emitted_signals_find (set, "holder-attr-changed", error))
		return FALSE;

	return TRUE;
}

/*
 * "validate-change" signal
 */
static GError *
t5_validate_change (GdaSet *set, gchar *token)
{
	GdaHolder *h3, *h2;
	const GValue *v2, *v3;
	g_assert (!strcmp (token, "MyToken2"));

	/* validate only if h2==125 and h3=='d' */
	h2 = gda_set_get_holder (set, "H2");
	h3 = gda_set_get_holder (set, "H3");
	g_assert (h2 && h3);

	v2 = gda_holder_get_value (h2);
	v3 = gda_holder_get_value (h3);

	if (v2 && (G_VALUE_TYPE (v2) == G_TYPE_INT) &&
	    v3 && (G_VALUE_TYPE (v3) == G_TYPE_CHAR) &&
	    (g_value_get_int (v2) == 125) &&
	    (g_value_get_schar (v3) == 'd')) {
		g_print ("GdaSet change accepted\n");
		return NULL;
	}
	else {
		GError *error = NULL;
		g_print ("GdaSet change refused\n");
		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "%s", "GdaSet change refused!");
		return error;
	}
}

static gboolean
test5 (GError **error)
{
	GdaSet *set;
	GdaHolder *h;
	GValue *value;
	GError *lerror = NULL;

	set = gda_set_new_inline (3, 
				  "H1", G_TYPE_STRING, "A string",
				  "H2", G_TYPE_INT, 1234,
				  "H3", G_TYPE_CHAR, 'r');

	g_signal_connect (G_OBJECT (set), "validate-set",
			  G_CALLBACK (t5_validate_change), "MyToken2");

	if (gda_set_is_valid (set, &lerror)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "gda_set_is_valid() should have returned FALSE");
		return FALSE;
	}

	if (!lerror) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Returned GError should not be NULL");
		return FALSE;
	}
	if (strcmp (lerror->message, "GdaSet change refused!")) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Returned GError's message is wrong, should be 'GdaHolder change refused!' and is '%s'",
			     lerror->message);
		return FALSE;
	}
	g_error_free (lerror);
	lerror = NULL;

	/**/
	h = gda_set_get_holder (set, "H2");
	g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 125);
	if (!gda_holder_set_value (h, value, NULL)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "gda_holder_set_value() should not have failed");
		return FALSE;
	}
	gda_value_free (value);

	h = gda_set_get_holder (set, "H3");
	g_value_set_schar ((value = gda_value_new (G_TYPE_CHAR)), 'd');
	if (!gda_holder_set_value (h, value, NULL)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "gda_holder_set_value() should not have failed");
		return FALSE;
	}
	gda_value_free (value);

	if (!gda_set_is_valid (set, error)) 
		return FALSE;

	g_object_unref (set);

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

/* Commented out because it is not used.
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
*/

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
set_3_cb (GObject *obj, GdaHolder *holder, G_GNUC_UNUSED const gchar *attr_name,
          G_GNUC_UNUSED const GValue *value, gchar *sig_name)
{
	EmittedSignal *es;
	es = g_new0 (EmittedSignal, 1);
	es->obj = obj;
	es->signal_name = sig_name;
	es->holder = holder;
	emitted_signal_add (es);
}

static void
set_1_cb (GObject *obj, GdaHolder *holder, gchar *sig_name)
{
	EmittedSignal *es;
	es = g_new0 (EmittedSignal, 1);
	es->obj = obj;
	es->signal_name = sig_name;
	es->holder = holder;
	emitted_signal_add (es);
}

static void
set_0_cb (GObject *obj, gchar *sig_name)
{
	EmittedSignal *es;
	es = g_new0 (EmittedSignal, 1);
	es->obj = obj;
	es->signal_name = sig_name;
	es->holder = NULL;
	emitted_signal_add (es);
}

static void 
emitted_signals_monitor_set (GdaSet *set)
{
	g_signal_connect (G_OBJECT (set), "holder-changed",
			  G_CALLBACK (set_1_cb), "holder-changed");
	g_signal_connect (G_OBJECT (set), "holder-attr-changed",
			  G_CALLBACK (set_3_cb), "holder-attr-changed");
	g_signal_connect (G_OBJECT (set), "public-data-changed",
			  G_CALLBACK (set_0_cb), "public-data-changed");
}
