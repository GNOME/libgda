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

#if !defined(__gda_thread_h__)
#  define __gda_thread_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define GDA_TYPE_THREAD    (gda_thread_get_type())
#ifdef HAVE_GOBJECT
#  define GDA_THREAD(obj) \
          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_THREAD, GdaThread)
#  define GDA_IS_THREAD(obj) \
          G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_THREAD)
#else
#  define GDA_THREAD(obj)    GTK_CHECK_CAST(obj, GDA_TYPE_THREAD, GdaThread)
#  define GDA_IS_THREAD(obj) GTK_CHECK_TYPE(obj, GDA_TYPE_THREAD)
#endif

typedef struct _GdaThread      GdaThread;
typedef struct _GdaThreadClass GdaThreadClass;

typedef gpointer (*GdaThreadFunc)(GdaThread *thr, gpointer user_data);

struct _GdaThread
{
#ifdef HAVE_GOBJECT
  GObject        object;
#else
  GtkObject      object;
#endif
  GdaThreadFunc func;
  gulong         tid;
  gboolean       is_running;
};

struct _GdaThreadClass
{
#ifdef HAVE_GOBJECT
  GObjectClass parent_class;
#else
  GtkObjectClass parent_class;
#endif
};

#ifdef HAVE_GOBJECT
GType       gda_thread_get_type   (void);
#else
GtkType     gda_thread_get_type   (void);
#endif

GdaThread* gda_thread_new        (GdaThreadFunc func);
void        gda_thread_free       (GdaThread *thr);
void        gda_thread_start      (GdaThread *thr, gpointer user_data);
void        gda_thread_stop       (GdaThread *thr);
gboolean    gda_thread_is_running (GdaThread *thr);

#if defined(__cplusplus)
}
#endif

#endif
