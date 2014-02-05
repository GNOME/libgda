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
//#undef DEBUG_NOTIFICATION

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

static GHashTable *handlers_hash = NULL;

typedef struct _SigClosure SigClosure;
struct _SigClosure
{
        GClosure      closure;
	GMainContext *context; /* ref held */
        gulong        ext_handler_id;

        GCallback     callback;

	ITSignaler   *signal_its;
	ITSignaler   *return_its;
	guint         its_add_id;
};

static void
sig_closure_finalize (G_GNUC_UNUSED gpointer notify_data, GClosure *closure)
{
        SigClosure *sig_closure = (SigClosure *)closure;

	g_print ("%s()\n", __FUNCTION__);
	itsignaler_remove (sig_closure->signal_its, sig_closure->context, sig_closure->its_add_id);
	itsignaler_unref (sig_closure->signal_its);
	itsignaler_unref (sig_closure->return_its);
	g_main_context_unref (sig_closure->context);
}

/* data to pass through ITSignaler and back */
typedef struct {
        GValue *return_value;
        guint n_param_values;
        GValue *param_values;
        gpointer invocation_hint; /* FIXME: problem here to copy value! */

        GClosure *rcl; /* real closure, used in the main loop context */
} PassedData;

/*
 * This function is called in the thread which emitted the signal
 *
 * In it we pack the data, push it through the ITSignaler to the thead from which the callback will be called, and
 * wait for the result before returning.
 */
static void
sig_closure_marshal (GClosure *closure,
                     GValue *return_value,
                     guint n_param_values,
                     const GValue *param_values,
                     gpointer invocation_hint,
                     gpointer marshal_data)
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

        if (g_main_context_acquire (sig_closure->context)) {
                /* the signal has to be propagated in the same thread */
                GClosure *rcl;
                rcl = g_cclosure_new (sig_closure->callback, closure->data, NULL);
                g_closure_set_marshal (rcl, g_cclosure_marshal_generic);
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
                data.rcl = g_cclosure_new (sig_closure->callback, closure->data, NULL);
                g_closure_set_marshal (data.rcl, g_cclosure_marshal_generic);
		if (itsignaler_push_notification (sig_closure->signal_its, &data, NULL)) {
			gpointer rdata;
			rdata = itsignaler_pop_notification (sig_closure->return_its, -1);
			g_assert (rdata == &data);
		}
		else
			g_warning ("Internal Gda Connect error, the signal will not be propagated, please report the bug");

		g_closure_unref (data.rcl);
        }
}

/*
 * This function is called in the thread where the signal is intended by the
 * programmer to be propagated, when a PassedData pointer is available through the ITS
 */
static gboolean
propagate_signal (ITSignaler *its, SigClosure *sig_closure)
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


/*
 * FIXME: what is @data for ?
 */
static SigClosure *
sig_closure_new (gpointer instance, GMainContext *context, gpointer data)
{
	g_assert (context);

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
        sig_closure->ext_handler_id = 0;
        sig_closure->callback = NULL;
	sig_closure->signal_its = its1;
	sig_closure->return_its = its2;

	sig_closure->its_add_id = itsignaler_add (sig_closure->signal_its, context,
						  (ITSignalerFunc) propagate_signal, sig_closure, NULL);

        /* initialize closure */
        g_closure_set_marshal (closure, sig_closure_marshal);
        gpointer notify_data = NULL; /* as unused in sig_closure_finalize() */
        g_closure_add_finalize_notifier (closure, notify_data, sig_closure_finalize);

        return sig_closure;
}

static guint
ulong_hash (gconstpointer key)
{
	return (guint) *((gulong*) key);
}

static gboolean
ulong_equal (gconstpointer a, gconstpointer b)
{
	return *((const gulong*) a) == *((const gulong*) b);
}


/**
 * gda_connect:
 * @instance: the instance to connect to
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: the GCallback to connect
 * @data: (allow-none): data to pass to @c_handler, or %NULL
 * @destroy_data: (allow-none): function to destroy @data when not needed anymore, or %NULL
 * @connect_flags:
 * @context: (allow-none): the #GMainContext in which signals will actually be treated, or %NULL for the default one
 *
 * Connects a GCallback function to a signal for a particular object.
 *
 * Returns: the handler id, or %0 if an error occurred
 *
 * Since: 6.0
 */
gulong
gda_connect (gpointer instance,
	     const gchar *detailed_signal,
	     GCallback c_handler,
	     gpointer data,
	     GClosureNotify destroy_data,
	     GConnectFlags connect_flags,
	     GMainContext *context)
{
	static gulong ids = 1;
        g_return_val_if_fail (instance, 0);

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
        sig_closure = sig_closure_new (instance, context, data);
	g_main_context_unref (context);
	if (!sig_closure)
		return 0;

        GSignalQuery query;
        g_signal_query (signal_id, &query);
        g_assert (query.signal_id == signal_id);

        gulong hid;
        hid = g_signal_connect_closure (instance, detailed_signal, (GClosure *) sig_closure, 0);
        g_closure_sink ((GClosure *) sig_closure);
        sig_closure->callback = c_handler;

        if (hid > 0) {
                if (!handlers_hash)
                        handlers_hash = g_hash_table_new_full (ulong_hash, ulong_equal,
                                                               NULL, (GDestroyNotify) g_closure_unref);
		sig_closure->ext_handler_id = ids++;
                g_hash_table_insert (handlers_hash, &(sig_closure->ext_handler_id),
				     (GClosure*) sig_closure);

                return sig_closure->ext_handler_id;
        }
        else {
                g_closure_unref ((GClosure *) sig_closure);
                return 0;
        }

}

void
gda_disconnect (gpointer instance, gulong handler_id, GMainContext *context)
{
	SigClosure *sig_closure = NULL;
	if (handlers_hash)
		sig_closure = g_hash_table_lookup (handlers_hash, &handler_id);
	if (sig_closure)
		g_hash_table_remove (handlers_hash, &handler_id);
	else
		g_warning (_("Could not find handler %lu"), handler_id);
}
