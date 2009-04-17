/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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
        GdaServerProvider *cnc_provider;
        GdaThreadWrapper *wrapper;
	GArray *handlers_ids; /* array of gulong */

	/* current async. tasks */
	GSList *async_tasks; /* list of ThreadConnectionAsyncTask pointers */
} ThreadConnectionData; /* Per connection private data for */
void           _gda_connection_force_transaction_status (GdaConnection *cnc, GdaConnection *wrapped_cnc);

G_END_DECLS

#endif
