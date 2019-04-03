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

#ifndef __ITSIGNALER_H__
#define __ITSIGNALER_H__

#include <glib.h>
#ifdef G_OS_WIN32
  #include <winsock2.h>
#endif

G_BEGIN_DECLS

typedef struct _ITSignaler ITSignaler;

ITSignaler *itsignaler_new           (void);

#ifdef G_OS_WIN32
SOCKET      itsignaler_windows_get_poll_fd (ITSignaler *its);
#else
int         itsignaler_unix_get_poll_fd (ITSignaler *its);
#endif

ITSignaler *itsignaler_ref           (ITSignaler *its);
void        itsignaler_unref         (ITSignaler *its);

gboolean    itsignaler_push_notification (ITSignaler *its, gpointer data, GDestroyNotify destroy_func);
gpointer    itsignaler_pop_notification  (ITSignaler *its, gint timeout_ms);

GSource    *itsignaler_create_source (ITSignaler *its);

/*
 * Returns: %FALSE if the source should be removed from the poll
 */
typedef gboolean (*ITSignalerFunc) (gpointer user_data);

guint       itsignaler_add (ITSignaler *its, GMainContext *context, ITSignalerFunc func, gpointer data, GDestroyNotify notify);
gboolean    itsignaler_remove (ITSignaler *its, GMainContext *context, guint id);

/*
 * Private
 */
void       _itsignaler_bg_unref (ITSignaler *its);

G_END_DECLS

#endif
