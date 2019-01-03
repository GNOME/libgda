/*
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_CONNECTION_PRIVATE_H_
#define __GDA_CONNECTION_PRIVATE_H_

#include <libgda/gda-meta-store.h>
#include <libgda/providers-support/gda-pstmt.h>
#include <libgda/thread-wrapper/gda-worker.h>
#include <libgda/gda-transaction-status.h>
#include <libgda/gda-connection-event.h>
#include <libgda/gda-server-provider-impl.h>

G_BEGIN_DECLS

/*
 * Provider's specific connection data management
 */
/**
 * GdaServerProviderConnectionData:
 *
 * Opaque structure extended by database providers to store per-connection information (usually C handles
 * to the connection as required by the C API they use).
 *
 * Note: @worker part is created in _gda_server_provider_open_connection() by the provider itself, which allows it to
 * either create a #GdaWorker for each connection, or create only one #GdaWorker for all connections (if the underlying
 * for example does not support multi-threading at all)
 */
typedef struct {
	GdaWorker *    worker;
	GDestroyNotify provider_data_destroy_func;
	gpointer       pad1;
	gpointer       pad2;
} GdaServerProviderConnectionData;
#define GDA_TYPE_SERVER_PROVIDER_CONNECTION_DATA (gda_server_provider_connection_data_get_type())

GType              gda_server_provider_connection_data_get_type  (void) G_GNUC_CONST;

void                             gda_connection_internal_set_provider_data       (GdaConnection *cnc,
										  GdaServerProviderConnectionData *data,
										  GDestroyNotify destroy_func);
GdaServerProviderConnectionData *gda_connection_internal_get_provider_data_error (GdaConnection *cnc, GError **error);
void                             _gda_connection_internal_set_worker_thread (GdaConnection *cnc, GThread *thread);
GdaWorker                       *gda_connection_internal_get_worker (GdaServerProviderConnectionData *data);

/*
 * Connection's events
 */
GdaConnectionEvent  *gda_connection_point_available_event(GdaConnection *cnc, GdaConnectionEventType type);
void                 gda_connection_add_event            (GdaConnection *cnc, GdaConnectionEvent *event);
GdaConnectionEvent  *gda_connection_add_event_string     (GdaConnection *cnc, const gchar *str, ...);
void                 gda_connection_clear_events_list    (GdaConnection *cnc);

/*
 * Transaction related
 */
void gda_connection_internal_transaction_started (GdaConnection *cnc, const gchar *parent_trans, const gchar *trans_name, 
						  GdaTransactionIsolation isol_level);
void gda_connection_internal_transaction_rolledback (GdaConnection *cnc, const gchar *trans_name);
void gda_connection_internal_transaction_committed (GdaConnection *cnc, const gchar *trans_name);

void gda_connection_internal_statement_executed (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, GdaConnectionEvent *error);

void gda_connection_internal_savepoint_added (GdaConnection *cnc, const gchar *parent_trans, const gchar *svp_name);
void gda_connection_internal_savepoint_rolledback (GdaConnection *cnc, const gchar *svp_name);
void gda_connection_internal_savepoint_removed (GdaConnection *cnc, const gchar *svp_name);
void gda_connection_internal_change_transaction_state (GdaConnection *cnc,
						       GdaTransactionStatusState newstate);
void gda_connection_internal_reset_transaction_status (GdaConnection *cnc);

/* 
 * prepared statements support
 */
void      gda_connection_add_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt, GdaPStmt *prepared_stmt); 
void      gda_connection_del_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt); 
GdaPStmt *gda_connection_get_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt);

/*
 * Open an SQLite connection even if the SQLite provider is not installed
 */
GdaConnection *gda_connection_open_sqlite (const gchar *directory, const gchar *filename, gboolean auto_unlink);

G_END_DECLS

#endif
