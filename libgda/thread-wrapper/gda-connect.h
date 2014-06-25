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

#ifndef __GDA_CONNECT_H__
#define __GDA_CONNECT_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION:gda-connect
 * @short_description: Inter thread signal propagation
 * @title: GdaConnect
 * @stability: Stable
 * @see_also:
 *
 * The purpose of the gda_signal_connect() is to allow one to connect to a signal emitted by an object and be sure that
 * the acutal signal handling will occur _only_ when running a specific #GMainContext. The gda_signal_handler_disconnect() actually
 * disconnects the handler.
 *
 * Use these functions for exemple when you need to handle signals from objects which are emitted from within a thread
 * which is not the main UI thread.
 *
 * These function implement their own locking mechanism and can safely be used from multiple
 * threads at once without needing further locking.
 */

gulong gda_signal_connect    (gpointer instance,
			      const gchar *detailed_signal,
			      GCallback c_handler,
			      gpointer data,
			      GClosureNotify destroy_data,
			      GConnectFlags connect_flags,
			      GMainContext *context);

void   gda_signal_handler_disconnect (gpointer instance,
				      gulong handler_id);

G_END_DECLS

#endif
