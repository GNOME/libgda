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

#if !defined(__gda_thread_h__)
#  define __gda_thread_h__

#include <glib.h>
#include <gnome.h>

BEGIN_GNOME_DECLS

#define GDA_TYPE_THREAD    (gda_thread_get_type())
#define GDA_THREAD(obj)    GTK_CHECK_CAST(obj, GDA_TYPE_THREAD, Gda_Thread)
#define IS_GDA_THREAD(obj) GTK_CHECK_TYPE(obj, GDA_TYPE_THREAD)

typedef struct _Gda_Thread      Gda_Thread;
typedef struct _Gda_ThreadClass Gda_ThreadClass;

typedef gpointer (*Gda_ThreadFunc)(Gda_Thread *thr, gpointer user_data);

struct _Gda_Thread
{
  GtkObject      object;
  Gda_ThreadFunc func;
  gulong         tid;
  gboolean       is_running;
};

struct _Gda_ThreadClass
{
  GtkObjectClass parent_class;
};

GtkType     gda_thread_get_type   (void);
Gda_Thread* gda_thread_new        (Gda_ThreadFunc func);
void        gda_thread_free       (Gda_Thread *thr);
void        gda_thread_start      (Gda_Thread *thr, gpointer user_data);
void        gda_thread_stop       (Gda_Thread *thr);
gboolean    gda_thread_is_running (Gda_Thread *thr);

END_GNOME_DECLS

#endif
