/* GDA Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gda-thread.h"
#include <pthread.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

typedef struct
{
	GdaThread *thr;
	gpointer user_data;
}
thread_data_t;

/* GdaThread object signals */
enum
{
	LAST_SIGNAL
};

static gint gda_thread_singals[LAST_SIGNAL] = { 0, };

/*
 * Private functions
 */
gpointer
thread_func (gpointer data)
{
	gpointer ret = NULL;
	thread_data_t *thr_data = (thread_data_t *) data;

	if (thr_data != NULL && thr_data->thr != NULL
	    && thr_data->thr->func != NULL) {
		thr_data->thr->is_running = TRUE;
		ret = thr_data->thr->func (thr_data->thr,
					   thr_data->user_data);
	}

	/* free memory allocated when calling the thread function */
	thr_data->thr->is_running = FALSE;
	g_free ((gpointer) thr_data);
	pthread_exit (ret);
}

/*
 * GdaThread object implementation
 */
static void
gda_thread_class_init (GdaThreadClass * klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
}

static void
gda_thread_init (GdaThread * thr)
{
	g_return_if_fail (GDA_IS_THREAD (thr));
	thr->func = NULL;
	thr->is_running = FALSE;
}

GtkType
gda_thread_get_type (void)
{
	static guint type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaThread",
			sizeof (GdaThread),
			sizeof (GdaThreadClass),
			(GtkClassInitFunc) gda_thread_class_init,
			(GtkObjectInitFunc) gda_thread_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		type = gtk_type_unique (gtk_object_get_type (), &info);
	}
	return type;
}

/**
 * gda_thread_new
 * @func: function to be called
 *
 * Create a new #GdaThread object. This function just creates the internal
 * structures and initializes all the data, but does not start the thread.
 * To do so, you must use #gda_thread_start.
 */
GdaThread *
gda_thread_new (GdaThreadFunc func)
{
	GdaThread *thr;

	/* initialize GLib threads */
	if (!g_thread_supported ())
		g_thread_init (NULL);

	thr = GDA_THREAD (gtk_type_new (GDA_TYPE_THREAD));
	thr->func = func;
	return thr;
}

/**
 * gda_thread_free
 */
void
gda_thread_free (GdaThread * thr)
{
	g_return_if_fail (GDA_IS_THREAD (thr));

	if (gda_thread_is_running (thr))
		gda_thread_stop (thr);

	gtk_object_unref (GTK_OBJECT (thr));
}

/**
 * gda_thread_start
 * @thr: a #GdaThread object
 * @user_data: data to pass to signals
 */
void
gda_thread_start (GdaThread * thr, gpointer user_data)
{
	g_return_if_fail (GDA_IS_THREAD (thr));

	if (!gda_thread_is_running (thr)) {
		thread_data_t *thr_data = g_new0 (thread_data_t, 1);
		thr_data->thr = thr;
		thr_data->user_data = user_data;
		pthread_create (&thr->tid, NULL, thread_func,
				(gpointer) thr_data);
		thr->is_running = TRUE;
	}
	else
		gda_log_error (_("thread is already running"));
}

/**
 * gda_thread_stop
 */
void
gda_thread_stop (GdaThread * thr)
{
	g_return_if_fail (GDA_IS_THREAD (thr));
	g_return_if_fail (gda_thread_is_running (thr));

	pthread_cancel (thr->tid);
	thr->is_running = FALSE;
}

/**
 * gda_thread_is_running
 * @thr: a #GdaThread object
 *
 * Checks whether the given thread object is running or not
 */
gboolean
gda_thread_is_running (GdaThread * thr)
{
	g_return_val_if_fail (GDA_IS_THREAD (thr), FALSE);
	return thr->is_running;
}
