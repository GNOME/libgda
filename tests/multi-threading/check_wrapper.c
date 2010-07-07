#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgda/libgda.h>
#include <thread-wrapper/gda-thread-wrapper.h>
#include "dummy-object.h"
#include <stdarg.h>

#define NTHREADS 10
#define NCALLS 10
#define PRINT_CALLS
#undef PRINT_CALLS

static gint test1 (void);
static gint test2 (void);

GdaThreadWrapper *wrapper; /* global object */

int 
main (int argc, char** argv)
{
	gint failures = 0;

	gda_init ();
	
	failures += test1 ();
	failures += test2 ();

	g_print ("Threads COUNT: %d, CALLS per thread:%d\n", NTHREADS, NCALLS*2 + 1);
	g_print ("FAILURES: %d\n", failures);
  
	return failures != 0 ? 1 : 0;
}



/*
 * This test creates a single GdaThreadWrapper and runs NTHREADS threads which use the
 * same GdaThreadWrapper object (each thread executes 2*NCALLS function calls plus a random
 * number of function calls which return void).
 *
 * No signal is handled in this test
 */
static gpointer main_thread_func (gpointer int_id);
static gpointer exec_in_wrapped_thread (gpointer int_in, GError **error);
static gchar *exec_in_wrapped_thread2 (gchar *str, GError **error);
static void exec_in_wrapped_thread_v (gchar *str, GError **error);
static gint
test1 (void)
{
	gint failures = 0;
	gint i;
	GSList *threads = NULL;

	wrapper = gda_thread_wrapper_new ();

	/* use GdaThreadWrapper from several threads */
	for (i = 0; i < NTHREADS; i++) {
		GThread *th;
		th = g_thread_create (main_thread_func, GINT_TO_POINTER (i), TRUE, NULL);
		if (!th) {
			g_print ("Can't create thread %d (not part of the test)\n", i);
			exit (1);
		}
		threads = g_slist_prepend (threads, th);
	}

	/* join the threads */
	GSList *list;
	for (list = threads; list; list = list->next)
		failures += GPOINTER_TO_INT (g_thread_join ((GThread*) list->data));
	g_slist_free (threads);

	g_object_unref (wrapper);
	return failures;
}

static gpointer
exec_in_wrapped_thread (gpointer int_in, GError **error)
{
	g_print ("function_1 (%d)\n", GPOINTER_TO_INT (int_in));
	return int_in;
}

static gchar *
exec_in_wrapped_thread2 (gchar *str, GError **error)
{
	g_print ("function_2 (%s)\n", str);
	return g_strdup (str);
}

static void
exec_in_wrapped_thread_v (gchar *str, GError **error)
{
	g_print ("function_VOID (%s)\n", str);
}

static void
free_arg (gchar *str)
{
	g_print ("\tFreeing: %s\n", str);
	g_free (str);
}

static gpointer
main_thread_func (gpointer int_id)
{
	gint i;
	guint *ids;
	gchar *estr;

	gint total_jobs = 0;
	gint nfailed = 0;

	g_print ("NEW thread %p <=> %d\n", g_thread_self(), GPOINTER_TO_INT (int_id));

	ids = g_new0 (guint, 2*NCALLS);
	for (i = 0; i < NCALLS; i++) {
		guint id;
		GError *error = NULL;

		/* func1 */
		id = gda_thread_wrapper_execute (wrapper, (GdaThreadWrapperFunc) exec_in_wrapped_thread,
						 GINT_TO_POINTER (i + GPOINTER_TO_INT (int_id) * 1000), NULL, &error);
		if (id == 0) {
			g_print ("Error in %s() for thread %d: %s\n", __FUNCTION__, GPOINTER_TO_INT (int_id),
				 error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
			return GINT_TO_POINTER (1);
		}
		ids[2*i] = id;
#ifdef PRINT_CALLS
		g_print ("--> Thread %d jobID %u arg %d\n", GPOINTER_TO_INT (int_id), id, i + GPOINTER_TO_INT (int_id) * 1000);
#endif
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();


		if (rand () >= RAND_MAX / 2.) {
			id = gda_thread_wrapper_execute_void (wrapper, (GdaThreadWrapperVoidFunc) exec_in_wrapped_thread_v,
							      g_strdup_printf ("perturbator %d.%d", GPOINTER_TO_INT (int_id), i), 
							      (GDestroyNotify) free_arg, &error);
			if (id == 0) {
				g_print ("Error in %s() for thread %d: %s\n", __FUNCTION__, GPOINTER_TO_INT (int_id),
					 error && error->message ? error->message : "No detail");
				if (error)
					g_error_free (error);
				return GINT_TO_POINTER (1);
			}
			total_jobs ++;
		}
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();

		/* func2 */
		id = gda_thread_wrapper_execute (wrapper, (GdaThreadWrapperFunc) exec_in_wrapped_thread2,
						 g_strdup_printf ("Hello %d.%d", GPOINTER_TO_INT (int_id), i), 
						 (GDestroyNotify) free_arg, &error);
		if (id == 0) {
			g_print ("Error in %s() for thread %d: %s\n", __FUNCTION__, GPOINTER_TO_INT (int_id),
				 error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
			return GINT_TO_POINTER (1);
		}
		ids[2*i+1] = id;
		estr = g_strdup_printf ("Hello %d.%d", GPOINTER_TO_INT (int_id), i);
#ifdef PRINT_CALLS
		g_print ("--> Thread %d jobID %u arg %s\n", GPOINTER_TO_INT (int_id), id, estr);
#endif
		g_free (estr);
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();
	}
	total_jobs += 2*NCALLS;
	g_thread_yield ();

	/* pick up results */
	for (i = 0; i < NCALLS; i++) {
		gpointer res;
		res = gda_thread_wrapper_fetch_result (wrapper, TRUE, ids[2*i], NULL);
		if (GPOINTER_TO_INT (res) != i + GPOINTER_TO_INT (int_id) * 1000) {
			g_print ("Error in %s() for thread %d: sub thread's exec result is wrong\n",
				 __FUNCTION__, GPOINTER_TO_INT (int_id));
			nfailed ++;
		}
#ifdef PRINT_CALLS
		g_print ("<-- Thread %d jobID %u arg %d\n", GPOINTER_TO_INT (int_id), id, i + GPOINTER_TO_INT (int_id) * 1000);
#endif
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();

		gchar *str;
		estr = g_strdup_printf ("Hello %d.%d", GPOINTER_TO_INT (int_id), i);
		str = gda_thread_wrapper_fetch_result (wrapper, TRUE, ids[2*i+1], NULL);
		if (!str || strcmp (str, estr)) {
			g_print ("Error in %s() for thread %d: sub thread's exec result is wrong: got %s, exp: %s\n",
				 __FUNCTION__, GPOINTER_TO_INT (int_id), str, estr);
			nfailed ++;
		}
		g_free (estr);
#ifdef PRINT_CALLS
		g_print ("<-- Thread %d jobID %u arg %s\n", GPOINTER_TO_INT (int_id), id, str);
#endif
		g_free (str);
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();
	}
	g_free (ids);

	return GINT_TO_POINTER (nfailed);
}


/*
 * This test creates a single #GdaThreadWrapper object and a single dummy object (DummyObject) which is
 * used to emit signals by functions executed in sub threads.
 */
typedef struct {
	DummyObject *dummy;
	GSList      *signals_sent; /* list of TestSignal structures */
	GMutex      *mutex;
} t2ExecData;
static void add_to_signals_sent (t2ExecData *data, gpointer what_to_add);
static gpointer t2_main_thread_func (DummyObject *dummy);
static gpointer t2_exec_in_wrapped_thread (t2ExecData *data, GError **error);
static void t2_exec_in_wrapped_thread_v (t2ExecData *data, GError **error);
#define INT_TOKEN 1034

static void
add_to_signals_sent (t2ExecData *data, gpointer what_to_add)
{
	g_mutex_lock (data->mutex);
	data->signals_sent = g_slist_append (data->signals_sent, what_to_add);
	g_mutex_unlock (data->mutex);
}

static gint
test2 (void)
{
	gint failures = 0;
	gint i;
	GSList *threads = NULL;
	DummyObject *dummy;

	wrapper = gda_thread_wrapper_new ();
	dummy = dummy_object_new ();

	/* use GdaThreadWrapper from several threads */
	for (i = 0; i < NTHREADS; i++) {
		GThread *th;
		th = g_thread_create ((GThreadFunc) t2_main_thread_func, dummy, TRUE, NULL);
		if (!th) {
			g_print ("Can't create thread %d (not part of the test)\n", i);
			exit (1);
		}
		threads = g_slist_prepend (threads, th);
	}

	/* join the threads */
	GSList *list;
	for (list = threads; list; list = list->next) {
		g_signal_emit_by_name (dummy, "sig0", NULL); /* this signal should be ignored */
		failures += GPOINTER_TO_INT (g_thread_join ((GThread*) list->data));
	}
	g_slist_free (threads);

	g_object_unref (wrapper);
	g_object_unref (dummy);

	return failures;
}

/*
 * structure to hold information about either a signal sent or received
 */
typedef struct {
	gchar  *name;
	guint   n_param_values;
        GValue *param_values; /* array of GValue structures */
} TestSignal;

/*
 * ... is a list of GType and value as native type (there must be exactly @n_param_values pairs)
 */
static TestSignal *
test_signal_new (const gchar *signame, guint n_param_values, ...)
{
	TestSignal *ts;
	va_list ap;
	guint i;

	ts = g_new0 (TestSignal, 1);
	ts->name = g_strdup (signame);
	ts->n_param_values = n_param_values;
	ts->param_values = g_new0 (GValue, n_param_values);

	va_start (ap, n_param_values);
	for (i = 0; i < n_param_values; i++) {
		GValue *value;
		GType type;
		type = va_arg (ap, GType);

		value = ts->param_values + i;
		g_value_init (value, type);
		if (type == G_TYPE_INT) {
			gint v;
			v = va_arg (ap, gint);
			g_value_set_int (value, v);
		}
		else if (type == G_TYPE_STRING) {
			gchar *v;
			v = va_arg (ap, gchar *);
			g_value_set_string (value, v);
		}
		else
			g_assert_not_reached ();
	}
	va_end (ap);

	return ts;
}

/*
 * Returns: TRUE if lists are equal
 */
static gboolean
compare_signals_lists (GSList *explist, GSList *gotlist)
{
	GSList *elist, *glist;
	for (elist = explist, glist = gotlist;
	     elist && glist;
	     elist = elist->next, glist = glist->next) {
		TestSignal *es = (TestSignal*) elist->data;
		TestSignal *gs = (TestSignal*) glist->data;

		if (strcmp (es->name, gs->name)) {
			g_print ("Error: expected signal '%s', got signal '%s'\n", es->name, gs->name);
			return FALSE;
		}
		if (es->n_param_values != gs->n_param_values) {
			g_print ("Error: expected %d arguments in '%s', got %d\n", es->n_param_values, 
				 es->name, gs->n_param_values);
			return FALSE;
		}
		gint i;
		for (i = 0; i < es->n_param_values; i++) {
			GValue *ev, *gv;
			ev = es->param_values + i;
			gv = gs->param_values + i;
			if (gda_value_differ (ev, gv)) {
				g_print ("Error: expected value %s in '%s', got %s\n",  
					 gda_value_stringify (ev), es->name, gda_value_stringify (gv));
				return FALSE;
			}
		}
		/*g_print ("Checked signal %s\n", es->name);*/
	}

	if (elist) {
		g_print ("Error: Some signals have not been received:\n");
		for (; elist; elist = elist->next) {
			TestSignal *es = (TestSignal*) elist->data;
			gint i;
			g_print ("\tSignal: %s", es->name);
			for (i = 0; i < es->n_param_values; i++)
				g_print (" %s", gda_value_stringify (es->param_values + i));
			g_print ("\n");
		}
		return FALSE;
	}

	if (glist) {
		g_print ("Error: Received too many signals:\n");
		for (; glist; glist = glist->next) {
			TestSignal *gs = (TestSignal*) glist->data;
			gint i;
			g_print ("\tSignal: %s", gs->name);
			for (i = 0; i < gs->n_param_values; i++)
				g_print (" %s", gda_value_stringify (gs->param_values + i));
			g_print ("\n");
		}
		return FALSE;
	}

	return TRUE;
}

static gpointer
t2_exec_in_wrapped_thread (t2ExecData *data, GError **error)
{
#ifdef PRINT_CALLS
	g_print ("TH %p ** emit sig0 (dummy=>%p)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig0", NULL);
	add_to_signals_sent (data, test_signal_new ("sig0", 0));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig1 (dummy=>%p, i=>123)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig1", 123, NULL);
	add_to_signals_sent (data, test_signal_new ("sig1", 1, G_TYPE_INT, 123));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig0 (dummy=>%p)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig0", NULL);
	add_to_signals_sent (data, test_signal_new ("sig0", 0));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig2 (dummy=>%p, i=>456 str=>Hello)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig2", 456, "Hello", NULL);
	add_to_signals_sent (data, test_signal_new ("sig2", 2, G_TYPE_INT, 456, G_TYPE_STRING, "Hello"));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig0 (dummy=>%p)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig0", NULL);
	add_to_signals_sent (data, test_signal_new ("sig0", 0));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig1 (dummy=>%p, i=>789)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig1", 789, NULL);
	add_to_signals_sent (data, test_signal_new ("sig1", 1, G_TYPE_INT, 789));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig2 (dummy=>%p, i=>32 str=>World)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig2", 32, "World", NULL);
	add_to_signals_sent (data, test_signal_new ("sig2", 2, G_TYPE_INT, 32, G_TYPE_STRING, "World"));

	return GINT_TO_POINTER (INT_TOKEN);
}

static void
t2_exec_in_wrapped_thread_v (t2ExecData *data, GError **error)
{
#ifdef PRINT_CALLS
	g_print ("TH %p ** emit sig0 (dummy=>%p)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig0", NULL);
	add_to_signals_sent (data, test_signal_new ("sig0", 0));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig1 (dummy=>%p, i=>321)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig1", 321, NULL);
	add_to_signals_sent (data, test_signal_new ("sig1", 1, G_TYPE_INT, 321));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig0 (dummy=>%p)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig0", NULL);
	add_to_signals_sent (data, test_signal_new ("sig0", 0));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig2 (dummy=>%p, i=>654 str=>Thread)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig2", 654, "Thread", NULL);
	add_to_signals_sent (data, test_signal_new ("sig2", 2, G_TYPE_INT, 654, G_TYPE_STRING, "Thread"));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig0 (dummy=>%p)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig0", NULL);
	add_to_signals_sent (data, test_signal_new ("sig0", 0));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig1 (dummy=>%p, i=>987)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig1", 987, NULL);
	add_to_signals_sent (data, test_signal_new ("sig1", 1, G_TYPE_INT, 987));

#ifdef PRINT_CALLS
        g_print ("TH %p ** emit sig2 (dummy=>%p, i=>23 str=>Thread)\n", g_thread_self (), data->dummy);
#endif
        g_signal_emit_by_name (data->dummy, "sig2", 23, "Thread", NULL);
	add_to_signals_sent (data, test_signal_new ("sig2", 2, G_TYPE_INT, 23, G_TYPE_STRING, "Thread"));
}

static void
wrapper_callback (GdaThreadWrapper *wrapper, gpointer instance, const gchar *signame,
		  gint n_param_values, const GValue *param_values, gpointer gda_reserved, GSList **sig_list)
{
	gint i;
	/*
	g_print ("RECEIVED signal '%s' on thread %p\n", signame, g_thread_self ());
	for (i = 0; i < n_param_values; i++) {
		gchar *str = gda_value_stringify (param_values  + i);
		g_print ("\tParam %d: %s\n", i, str);
		g_free (str);
	}
	*/

	TestSignal *ts;

	ts = g_new0 (TestSignal, 1);
	ts->name = g_strdup (signame);
	ts->n_param_values = n_param_values;
	ts->param_values = g_new0 (GValue, n_param_values);
	for (i = 0; i < n_param_values; i++) {
		GValue *dest = ts->param_values + i;
		const GValue *src = param_values + i;
		if (G_VALUE_TYPE (src) != GDA_TYPE_NULL) {
			g_value_init (dest, G_VALUE_TYPE (src));
			g_value_copy (src, dest);
		}
	}
	*sig_list = g_slist_append (*sig_list, ts);
}

static gpointer
t2_main_thread_func (DummyObject *dummy)
{
	gint i;
	guint *ids;

	gint total_jobs = 0;
	gint nfailed = 0;

	gulong sigid[3];
	t2ExecData edata;
	GSList *received_list = NULL;

	/*g_print ("NEW test thread: %p\n", g_thread_self());*/
	sigid[0] = gda_thread_wrapper_connect_raw (wrapper, dummy, "sig0", TRUE, TRUE,
						   (GdaThreadWrapperCallback) wrapper_callback,
						   &received_list);
	sigid[1] = gda_thread_wrapper_connect_raw (wrapper, dummy, "sig1", TRUE, TRUE,
						   (GdaThreadWrapperCallback) wrapper_callback,
						   &received_list);
	sigid[2] = gda_thread_wrapper_connect_raw (wrapper, dummy, "sig2", TRUE, TRUE,
						   (GdaThreadWrapperCallback) wrapper_callback,
						   &received_list);

	edata.dummy = dummy;
	edata.signals_sent = NULL;
	edata.mutex = g_mutex_new ();

	ids = g_new0 (guint, NCALLS);
	for (i = 0; i < NCALLS; i++) {
		guint id;
		GError *error = NULL;

		/* func1 */
		id = gda_thread_wrapper_execute (wrapper,
						 (GdaThreadWrapperFunc) t2_exec_in_wrapped_thread,
						 &edata, NULL, &error);
		if (id == 0) {
			g_print ("Error in %s() for thread %p: %s\n", __FUNCTION__, g_thread_self (),
				 error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
			return GINT_TO_POINTER (1);
		}
		ids[i] = id;
#ifdef PRINT_CALLS
		g_print ("--> Thread %p jobID %u\n", g_thread_self (), id);
#endif
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();


		if (rand () >= RAND_MAX / 2.) {
			id = gda_thread_wrapper_execute_void (wrapper,
							      (GdaThreadWrapperVoidFunc) t2_exec_in_wrapped_thread_v,
							      &edata, 
							      NULL, &error);
			if (id == 0) {
				g_print ("Error in %s() for thread %p: %s\n", __FUNCTION__, g_thread_self (),
					 error && error->message ? error->message : "No detail");
				if (error)
					g_error_free (error);
				return GINT_TO_POINTER (1);
			}
			total_jobs ++;
		}
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();
	}
	total_jobs += NCALLS;
	g_thread_yield ();

	/* pick up results */
	for (i = 0; i < NCALLS; i++) {
		gpointer res;

		g_signal_emit_by_name (dummy, "sig0", NULL); /* this signal should be ignored */

		res = gda_thread_wrapper_fetch_result (wrapper, TRUE, ids[i], NULL);
		if (GPOINTER_TO_INT (res) != INT_TOKEN) {
			g_print ("Error in %s() for thread %p: sub thread's exec result is wrong\n",
				 __FUNCTION__, g_thread_self ());
			nfailed ++;
		}
#ifdef PRINT_CALLS
		g_print ("<-- Thread %p jobID %u arg %d\n", g_thread_self (), ids[i], INT_TOKEN);
#endif
		if (rand () >= RAND_MAX / 2.)
			g_thread_yield ();
		g_signal_emit_by_name (dummy, "sig1", 666, NULL); /* this signal should be ignored */
	}

	while (gda_thread_wrapper_get_waiting_size (wrapper) > 0) {
		gda_thread_wrapper_iterate (wrapper, FALSE);
		g_usleep (10000);
	}

	g_mutex_lock (edata.mutex);
	if (! compare_signals_lists (edata.signals_sent, received_list))
		nfailed++;
	g_mutex_unlock (edata.mutex);


#ifdef PRINT_CALLS
	g_print ("Disconnecting signals\n");
#endif
	gda_thread_wrapper_disconnect (wrapper, sigid[0]);
	gda_thread_wrapper_disconnect (wrapper, sigid[1]);
	gda_thread_wrapper_disconnect (wrapper, sigid[2]);

	g_free (ids);

	/* we don't care about mem leaks here... */
	received_list = NULL;
	guint id;
	GError *error = NULL;
	id = gda_thread_wrapper_execute_void (wrapper,
					      (GdaThreadWrapperVoidFunc) t2_exec_in_wrapped_thread_v,
					      &edata, 
					      NULL, &error);
	if (id == 0) {
		g_print ("Error in %s() for thread %p: %s\n", __FUNCTION__, g_thread_self (),
			 error && error->message ? error->message : "No detail");
		if (error)
			g_error_free (error);
		return GINT_TO_POINTER (1);
	}
	total_jobs ++;

	while (gda_thread_wrapper_get_waiting_size (wrapper) > 0) {
		gda_thread_wrapper_iterate (wrapper, FALSE);
		g_usleep (10000);
	}
	g_mutex_free (edata.mutex);

	if (received_list) {
		g_print ("Error: signals should not be received anymore...\n");
		nfailed ++;
	}

	return GINT_TO_POINTER (nfailed);
}
