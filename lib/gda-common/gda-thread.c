/* GNOME DB Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "gda-thread.h"
#include <pthread.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

typedef struct
{
  Gda_Thread* thr;
  gpointer    user_data;
} thread_data_t;

/* Gda_Thread object signals */
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
  gpointer       ret = NULL;
  thread_data_t* thr_data = (thread_data_t *) data;

  if (thr_data != NULL && thr_data->thr != NULL && thr_data->thr->func != NULL)
    {
      thr_data->thr->is_running = TRUE;
      ret = thr_data->thr->func(thr_data->thr, thr_data->user_data);
    }

  /* free memory allocated when calling the thread function */
  thr_data->thr->is_running = FALSE;
  g_free((gpointer) thr_data);
  pthread_exit(ret);
}

/*
 * Gda_Thread object implementation
 */
#ifdef HAVE_GOBJECT
static void
gda_thread_class_init (Gda_ThreadClass *klass, gpointer data)
{
}
#else
static void
gda_thread_class_init (Gda_ThreadClass *klass)
{
   GtkObjectClass* object_class = (GtkObjectClass *) klass;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_thread_init (Gda_Thread *thr, Gda_ThreadClass *klass)
#else
gda_thread_init (Gda_Thread *thr)
#endif
{
  g_return_if_fail(IS_GDA_THREAD(thr));
  thr->func = NULL;
  thr->is_running = FALSE;
}

#ifdef HAVE_GOBJECT
GType
gda_thread_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo info =
      {
        sizeof (Gda_ThreadClass),               /* class_size */
        NULL,                                   /* base_init */
        NULL,                                   /* base_finalize */
        (GClassInitFunc) gda_thread_class_init, /* class_init */
        NULL,                                   /* class_finalize */
        NULL,                                   /* class_data */
        sizeof (Gda_Thread),                    /* instance_size */
        0,                                      /* n_preallocs */
        (GInstanceInitFunc) gda_thread_init,    /* instance_init */
        NULL,                                   /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "Gda_Thread", &info);
    }
  return type;
}
#else
GtkType
gda_thread_get_type (void)
{
  static guint type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
        "Gda_Thread",
        sizeof (Gda_Thread),
        sizeof (Gda_ThreadClass),
        (GtkClassInitFunc) gda_thread_class_init,
        (GtkObjectInitFunc) gda_thread_init,
        (GtkArgSetFunc) NULL,
        (GtkArgSetFunc) NULL,
      };
      type = gtk_type_unique(gtk_object_get_type(), &info);
    }
  return type;
}
#endif

/**
 * gda_thread_new
 * @func: function to be called
 *
 * Create a new #Gda_Thread object. This function just creates the internal
 * structures and initializes all the data, but does not start the thread.
 * To do so, you must use #gda_thread_start.
 */
Gda_Thread *
gda_thread_new (Gda_ThreadFunc func)
{
  Gda_Thread* thr;

  /* initialize GLib threads */
  if (!g_thread_supported()) g_thread_init(NULL);

#ifdef HAVE_GOBJECT
  thr = GDA_THREAD (g_object_new (GDA_TYPE_THREAD, NULL));
#else
   thr = GDA_THREAD(gtk_type_new(GDA_TYPE_THREAD));
#endif
  thr->func = func;
  return thr;
}

/**
 * gda_thread_free
 */
void
gda_thread_free (Gda_Thread *thr)
{
  g_return_if_fail(IS_GDA_THREAD(thr));

  if (gda_thread_is_running(thr)) gda_thread_stop(thr);

#ifdef HAVE_GOBJECT
  g_object_unref (G_OBJECT (thr));
#else
   gtk_object_destroy(GTK_OBJECT(thr));
#endif
}

/**
 * gda_thread_start
 * @thr: a #Gda_Thread object
 * @user_data: data to pass to signals
 */
void
gda_thread_start (Gda_Thread *thr, gpointer user_data)
{
  g_return_if_fail(IS_GDA_THREAD(thr));
  
  if (!gda_thread_is_running(thr))
    {
      thread_data_t* thr_data = g_new0(thread_data_t, 1);
      thr_data->thr = thr;
      thr_data->user_data = user_data;
      pthread_create(&thr->tid, NULL, thread_func, (gpointer) thr_data);
      thr->is_running = TRUE;
    }
  else gda_log_error(_("thread is already running"));
}

/**
 * gda_thread_stop
 */
void
gda_thread_stop (Gda_Thread *thr)
{
  g_return_if_fail(IS_GDA_THREAD(thr));
  g_return_if_fail(gda_thread_is_running(thr));

  pthread_cancel(thr->tid);
  thr->is_running = FALSE;
}

/**
 * gda_thread_is_running
 * @thr: a #Gda_Thread object
 *
 * Checks whether the given thread object is running or not
 */
gboolean
gda_thread_is_running (Gda_Thread *thr)
{
  g_return_val_if_fail(IS_GDA_THREAD(thr), FALSE);
  return thr->is_running;
}

