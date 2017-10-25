/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_CONNECTION_INTERNAL_H_
#define __GDA_CONNECTION_INTERNAL_H_

#include <libgda/gda-decl.h>
#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

/* 
 * Warnings
 */
#ifdef GDA_DEBUG
#define ASSERT_TABLE_NAME(x,y) g_assert (!strcmp ((x), (y)))
#define WARN_META_UPDATE_FAILURE(x,method) if (!(x)) g_print ("%s:%d %s (meta method => %s) ERROR: %s\n", __FILE__, __LINE__, __FUNCTION__, (method), error && *error && (*error)->message ? (*error)->message : "???")
#else
#define ASSERT_TABLE_NAME(x,y)
#define WARN_META_UPDATE_FAILURE(x,method)
#endif

/*
 * Misc.
 */
GdaWorker         *_gda_connection_get_worker (GdaConnection *cnc);
guint              _gda_connection_get_exec_slowdown (GdaConnection *cnc);

void               _gda_connection_set_status (GdaConnection *cnc, GdaConnectionStatus status);
void               gda_connection_increase_usage (GdaConnection *cnc);
void               gda_connection_decrease_usage (GdaConnection *cnc);

/*
 * Opens a connection to an SQLite database. This function is intended to be used
 * internally when objects require an SQLite connection, for example for the GdaMetaStore
 * object
 */
GdaConnection     *_gda_open_internal_sqlite_connection (const gchar *cnc_string);

/*
 * Used by virtual connections to keep meta data up to date when a table
 * is added or removed, without using the update_meta_store_after_statement_exec()
 * because it may not be called everytime
 */
void               _gda_connection_signal_meta_table_update (GdaConnection *cnc, const gchar *table_name);
gchar             *_gda_connection_compute_table_virtual_name (GdaConnection *cnc, const gchar *table_name);

G_END_DECLS

#endif
