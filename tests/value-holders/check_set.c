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
static gboolean emitted_signals_notfind (gpointer obj, const gchar *signal_name, GError **error);
static gboolean emitted_signals_chech_empty (gpointer obj, const gchar *signal_name, GError **error);
static void     emitted_signals_monitor_set (GdaSet *set);

/* 
 * tests
 */
typedef gboolean (*TestFunc) (GError **);
static gboolean test1 (GError **error);
static gboolean test2 (GError **error);

GHashTable *data;
TestFunc tests[] = {
	test1,
	test2
};

int 
main (int argc, char** argv)
{
	g_type_init ();
	gda_init ();

	gint failures = 0;
	gint i, ntests = 0;
  
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
		g_set_error (error, 0, 0,
			     "Could not find GdaHolder H1");
		return FALSE;
	}

	if (!gda_holder_set_source_model (h, model, 1, error)) 
		return FALSE;
	if (!tests_common_check_set (data, "T2.1", set, error)) return FALSE;

	if (!emitted_signals_find (set, "public_data_changed", error))
		return FALSE;
	if (!emitted_signals_chech_empty (set, "public_data_changed", error))
		return FALSE;

	if (!tests_common_check_set (data, "T2.1", set, error)) return FALSE;

	/***/
	h = gda_set_get_holder (set, "H2");
	if (!gda_holder_set_source_model (h, model, 0, error)) 
		return FALSE;
	if (!tests_common_check_set (data, "T2.2", set, error)) return FALSE;

	if (!emitted_signals_find (set, "public_data_changed", error))
		return FALSE;
	if (!emitted_signals_chech_empty (set, "public_data_changed", error))
		return FALSE;

	/***/
	h = gda_set_get_holder (set, "H1");
	if (!gda_holder_set_source_model (h, NULL, 0, error)) 
		return FALSE;
	if (!tests_common_check_set (data, "T2.3", set, error)) return FALSE;

	if (!emitted_signals_find (set, "public_data_changed", error))
		return FALSE;
	if (!emitted_signals_chech_empty (set, "public_data_changed", error))
		return FALSE;

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
	g_slist_foreach (signals_list, (GFunc) g_free, NULL);
	g_slist_free (signals_list);
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
	g_set_error (error, 0, 0,
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
			g_set_error (error, 0, 0,
				     "Signal \"%s\" has been emitted", signal_name);
			g_object_unref (obj);

			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
emitted_signals_chech_empty (gpointer obj, const gchar *signal_name, GError **error)
{
	GSList *list;
	for (list = signals_list; list; list = list->next) {
		EmittedSignal *es = (EmittedSignal *) list->data;
		if ((!obj || (es->obj == obj)) && 
		    (!signal_name || (!strcmp (es->signal_name, signal_name)))) {
			g_set_error (error, 0, 0,
				     "Signal \"%s\" has been emitted", es->signal_name);
			emitted_signals_reset ();
			return FALSE;
		}
	}
	return TRUE;
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
	g_signal_connect (G_OBJECT (set), "holder_changed",
			  G_CALLBACK (set_1_cb), "holder_changed");
	g_signal_connect (G_OBJECT (set), "holder_plugin_changed",
			  G_CALLBACK (set_1_cb), "holder_plugin_changed");
	g_signal_connect (G_OBJECT (set), "holder_attr_changed",
			  G_CALLBACK (set_1_cb), "holder_attr_changed");
	g_signal_connect (G_OBJECT (set), "public_data_changed",
			  G_CALLBACK (set_0_cb), "public_data_changed");
}
