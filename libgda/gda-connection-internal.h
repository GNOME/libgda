/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/thread-wrapper/gda-thread-wrapper.h>

G_BEGIN_DECLS

/* 
 * Warnings
 */
#ifdef GDA_DEBUG
#define ASSERT_TABLE_NAME(x,y) g_assert (!strcmp ((x), (y)))
#define WARN_METHOD_NOT_IMPLEMENTED(prov,method) g_warning ("Provider '%s' does not implement the META method '%s()', please report the error to bugzilla.gnome.org", gda_server_provider_get_name (prov), (method))
#define WARN_META_UPDATE_FAILURE(x,method) if (!(x)) g_print ("%s (meta method => %s) ERROR: %s\n", __FUNCTION__, (method), error && *error && (*error)->message ? (*error)->message : "???")
#else
#define ASSERT_TABLE_NAME(x,y)
#define WARN_METHOD_NOT_IMPLEMENTED(prov,method)
#define WARN_META_UPDATE_FAILURE(x,method)
#endif

/*
 * Opens a connection to an SQLite database. This function is intended to be used
 * internally when objects require an SQLite connection, for example for the GdaMetaStore
 * object
 */
GdaConnection *_gda_open_internal_sqlite_connection (const gchar *cnc_string);

typedef struct {
	guint jid;
	GdaServerProviderExecCallback async_cb;
	gpointer cb_data;
} ThreadConnectionAsyncTask;
void _ThreadConnectionAsyncTask_free (ThreadConnectionAsyncTask *atd);

/*
 * Functions dedicated to implementing a GdaConnection which uses a GdaThreadWrapper around
 * another connection to make it thread safe.
 */
typedef struct {
        GdaConnection *sub_connection;
	gboolean sub_connection_has_closed;
        GdaServerProvider *cnc_provider;
        GdaThreadWrapper *wrapper;
	GArray *handlers_ids; /* array of gulong */

	/* current async. tasks */
	GSList *async_tasks; /* list of ThreadConnectionAsyncTask pointers */
} ThreadConnectionData; /* Per connection private data for */
void                  _gda_thread_connection_set_data (GdaConnection *cnc, ThreadConnectionData *cdata);
ThreadConnectionData *_gda_thread_connection_get_data (GdaConnection *cnc);
void                  _gda_thread_connection_data_free (ThreadConnectionData *cdata);
void                  _gda_connection_force_transaction_status (GdaConnection *cnc, GdaConnection *wrapped_cnc);
GdaServerProvider    *_gda_connection_get_internal_thread_provider (void);

void                  _gda_connection_define_as_thread_wrapper (GdaConnection *cnc);

/*
 * Used by virtual connections to keep meta data up to date when a table
 * is added or removed, without using the update_meta_store_after_statement_exec()
 * because it may not be called everytime
 */
void               _gda_connection_signal_meta_table_update (GdaConnection *cnc, const gchar *table_name);
gchar             *_gda_connection_compute_table_virtual_name (GdaConnection *cnc, const gchar *table_name);

G_END_DECLS

#endif
