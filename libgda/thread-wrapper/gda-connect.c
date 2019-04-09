/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "gda-connect.h"
#include "itsignaler.h"
#include <gda-debug-macros.h>
#include <glib/gi18n-lib.h>

#define DEBUG_NOTIFICATION
#undef DEBUG_NOTIFICATION

#ifdef DEBUG_NOTIFICATION
static gchar *
_value_stringify (const GValue *value)
{
        if (!value)
                return g_strdup ("NULL");
        if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING)) {
                GValue *string;
                gchar *str;

                string = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
                g_value_transform (value, string);
                str = g_value_dup_string (string);
                g_value_reset (string);
                g_free (string);
                return str;
        }
        else {
                GType type = G_VALUE_TYPE (value);
                if (type == G_TYPE_DATE) {
                        GDate *date;
                        date = (GDate *) g_value_get_boxed (value);
                        if (date) {
                                if (g_date_valid (date))
                                        return g_strdup_printf ("%04u-%02u-%02u",
                                                                g_date_get_year (date),
                                                                g_date_get_month (date),
                                                                g_date_get_day (date));
                                else
                                        return g_strdup_printf ("%04u-%02u-%02u",
                                                                date->year, date->month, date->day);
                        }
                        else
                                return g_strdup ("0000-00-00");
                }
                else if (G_TYPE_IS_OBJECT (type)) {
                        GObject *obj;
                        obj = g_value_get_object (value);
                        return g_strdup_printf ("%p (%s)", obj, G_OBJECT_TYPE_NAME (obj));
                }
                else
                        return g_strdup ("");
        }
}
#endif

typedef struct _SigClosure SigClosure;
struct _SigClosure
{
        GClosure      closure;
	GMutex        mutex;
	GMainContext *context; /* ref held */

        GClosure     *rcl; /* real closure, used in the main loop context */

	ITSignaler   *signal_its;
	ITSignaler   *return_its;
	guint         its_add_id;
};

static void
sig_closure_finalize (G_GNUC_UNUSED gpointer notify_data, GClosure *closure)
{
        SigClosure *sig_closure = (SigClosure *)closure;

	/*g_print ("%s (%p)\n", __FUNCTION__, closure);*/
	itsignaler_remove (sig_closure->signal_its, sig_closure->context, sig_closure->its_add_id);
	itsignaler_unref (sig_closure->signal_its);
	itsignaler_unref (sig_closure->return_its);
	if (sig_closure->rcl)
		g_closure_unref (sig_closure->rcl);
	g_mutex_clear (& (sig_closure->mutex));

	g_main_context_unref (sig_closure->context);
}

/* data to pass through ITSignaler and back */
typedef struct {
        GValue *return_value;
        guint n_param_values;
        GValue *param_values;
        gpointer invocation_hint; /* value is never copied */
	GClosure *rcl;
} PassedData;

/*
 * This function is called in the thread which emitted the signal
 *
 * In it we pack the data, push it through the ITSignaler to the thread in which the callback will be called, and
 * wait for the result before returning.
 */
static void
sig_closure_marshal (GClosure *closure,
                     GValue *return_value,
                     guint n_param_values,
                     const GValue *param_values,
                     gpointer invocation_hint,
                     G_GNUC_UNUSED gpointer marshal_data)
{
        SigClosure *sig_closure;
        sig_closure = (SigClosure *) closure;

#ifdef DEBUG_NOTIFICATION
        g_print ("Th%p: %s (closure->data = \"%s\") called\n", g_thread_self(), __FUNCTION__, (gchar*) closure->data);
        guint n;
        for (n = 0; n < n_param_values; n++) {
                gchar *tmp;
                tmp = _value_stringify (& (param_values [n]));
                g_print ("Th%p   Param value %d is [%s]\n", g_thread_self(), n, tmp);
                g_free (tmp);
        }
        if (return_value)
                g_print ("Th%p   Return value expected: of type %s\n", g_thread_self(), g_type_name (G_VALUE_TYPE (return_value)));
        else
                g_print ("Th%p   Return value expected: none\n", g_thread_self());
#endif

	gboolean is_owner;
	is_owner = g_main_context_is_owner (sig_closure->context);
        if (g_main_context_acquire (sig_closure->context)) {
		/* warning about not owner context */
		if (!is_owner)
			g_warning (_("The GMainContext passed when gda_signal_connect() was called is not owned by any thread, you may "
				     "expect some undesired behaviours, use g_main_context_acquire()"));

                /* the signal has to be propagated in the same thread */
                GClosure *rcl;
		rcl = g_closure_ref (sig_closure->rcl);
                g_closure_invoke (rcl, return_value, n_param_values, param_values, invocation_hint);
                g_closure_unref (rcl);
		g_main_context_release (sig_closure->context);
        }
        else {
		PassedData data;
                data.n_param_values = n_param_values;
                data.invocation_hint = invocation_hint;
                data.return_value = return_value;
		data.param_values = (GValue*) param_values;
		data.rcl = g_closure_ref (sig_closure->rcl);

		g_mutex_lock (& (sig_closure->mutex)); /* This mutex ensures that push_notification() and
							* the following pop_notification() are treated as a single atomic operation
							* in case there are some return values */
		if (itsignaler_push_notification (sig_closure->signal_its, &data, NULL)) {
			gpointer rdata;
			rdata = itsignaler_pop_notification (sig_closure->return_its, -1);
			g_assert (rdata == &data);
		}
		else
			g_warning ("Internal Gda connect error, the signal will not be propagated, please report the bug");
		g_mutex_unlock (& (sig_closure->mutex));

		g_closure_unref (data.rcl);
        }
}

/*
 * This function is called in the thread where the signal is intended by the
 * programmer to be propagated, when a PassedData pointer is available through the ITS
 */
static gboolean
propagate_signal (SigClosure *sig_closure)
{
	PassedData *data;
	data = itsignaler_pop_notification (sig_closure->signal_its, 0);
	if (data) {
		g_closure_invoke (data->rcl, data->return_value, data->n_param_values,
				  data->param_values, data->invocation_hint);
		if (! itsignaler_push_notification (sig_closure->return_its, data, NULL))
			g_warning ("Internal Gda Connect error, the signal result will not be propagated, please report the bug");
	}
	else
		g_warning ("Internal Gda Connect error: no signal data to process, please report a bug");
	return TRUE; /* don't remove the source! */
}

static SigClosure *
sig_closure_new (gpointer instance, GMainContext *context, gboolean swapped, GCallback c_handler, gpointer data)
{
	g_assert (context);
	g_assert (instance);
	g_assert (c_handler);

	ITSignaler *its1, *its2;
	its1 = itsignaler_new ();
	its2 = itsignaler_new ();
	if (! its1 || ! its2) {
		g_warning ("Not enough ressources to allocate internal communication object");
		itsignaler_unref (its1); /* in case it was created */
		return NULL;
	}

        GClosure *closure;
        SigClosure *sig_closure;

        closure = g_closure_new_simple (sizeof (SigClosure), data);
        sig_closure = (SigClosure *) closure;
	sig_closure->context = g_main_context_ref (context);
	sig_closure->signal_its = its1;
	sig_closure->return_its = its2;
	g_mutex_init (& (sig_closure->mutex));

	if (swapped)
		sig_closure->rcl = g_cclosure_new_swap (c_handler, closure->data, NULL);
	else
		sig_closure->rcl = g_cclosure_new (c_handler, closure->data, NULL);
	g_closure_set_marshal (sig_closure->rcl, g_cclosure_marshal_generic);

	sig_closure->its_add_id = itsignaler_add (sig_closure->signal_its, context,
						  (ITSignalerFunc) propagate_signal, sig_closure, NULL);

        /* initialize closure */
        g_closure_set_marshal (closure, sig_closure_marshal);
        gpointer notify_data = NULL; /* as unused in sig_closure_finalize() */
        g_closure_add_finalize_notifier (closure, notify_data, sig_closure_finalize);

	/*g_print ("%s (%p)\n", __FUNCTION__, closure);*/
        return sig_closure;
}

/**
 * gda_signal_connect:
 * @instance: the instance to connect to
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: the GCallback to connect
 * @data: (nullable): data to pass to @c_handler, or %NULL
 * @destroy_data: (nullable): function to destroy @data when not needed anymore, or %NULL
 * @connect_flags: a combination of #GConnectFlags.
 * @context: (nullable): the #GMainContext in which signals will actually be treated, or %NULL for the default one
 *
 * Connects a GCallback function to a signal for a particular object. The difference with g_signal_connect() is that the
 * callback will be called from withing the thread which is the owner of @context. If needed you may have to use g_main_context_acquire()
 * to ensure a specific thread is the owner of @context.
 *
 * Returns: the handler id, or %0 if an error occurred
 *
 * Since: 6.0
 */
gulong
gda_signal_connect (gpointer instance,
		    const gchar *detailed_signal,
		    GCallback c_handler,
		    gpointer data,
		    G_GNUC_UNUSED GClosureNotify destroy_data,
		    GConnectFlags connect_flags,
		    GMainContext *context)
{
        g_return_val_if_fail (instance, 0);
        g_return_val_if_fail (c_handler, 0);

        guint signal_id;
        if (! g_signal_parse_name (detailed_signal, G_TYPE_FROM_INSTANCE (instance), &signal_id, NULL, FALSE)) {
                g_warning (_("Could not find signal named \"%s\""), detailed_signal);
                return 0;
        }

        if (context)
		g_main_context_ref (context);
	else
                context = g_main_context_ref_thread_default ();

        SigClosure *sig_closure;
        sig_closure = sig_closure_new (instance, context,
				       connect_flags & G_CONNECT_SWAPPED, c_handler, data);
	g_main_context_unref (context);
	if (!sig_closure)
		return 0;

        GSignalQuery query;
        g_signal_query (signal_id, &query);
        g_assert (query.signal_id == signal_id);

        gulong handler_id;
        handler_id = g_signal_connect_closure (instance, detailed_signal, (GClosure *) sig_closure,
					       (connect_flags & G_CONNECT_AFTER) ? TRUE : FALSE);
        if (handler_id > 0)
                return handler_id;
        else {
                g_closure_unref ((GClosure *) sig_closure);
                return 0;
        }

}

/**
 * gda_signal_handler_disconnect:
 * @instance: the instance to disconnect from
 * @handler_id: the signal handler, as returned by gda_signal_connect()
 *
 * Disconnect a callback using the signal handler, see gda_signal_connect(). This function is similar to calling
 * g_signal_handler_disconnect().
 *
 * Since: 6.0
 */
void
gda_signal_handler_disconnect (gpointer instance, gulong handler_id)
{
	g_return_if_fail (instance);
	g_return_if_fail (handler_id > 0);

	g_signal_handler_disconnect (instance, handler_id);
}
