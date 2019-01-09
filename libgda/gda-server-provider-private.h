/*
 * Copyright (C) 2005 Dan Winship <danw@src.gnome.org>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peńa <alvaropg@telefonica.net>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDA_SERVER_PROVIDER_PRIVATE__
#define __GDA_SERVER_PROVIDER_PRIVATE__

#include <libgda/gda-meta-store.h>
#include <libgda/gda-server-provider-impl.h>

/*
 * Getting the GMainContext to use with the GdaWorker
 */
GMainContext *gda_server_provider_get_real_main_context (GdaConnection *cnc);

/*
 * GdaServerProvider's virtual functions access
 */
gpointer _gda_server_provider_get_impl_functions (GdaServerProvider *provider, GdaWorker *worker,
						  GdaServerProviderFunctionsType type);

/*
 * private API to access the provider's virtual methods, but only through the GdaWorker's prism
 */
GdaConnection *
_gda_server_provider_create_connection (GdaServerProvider *provider, const gchar *dsn_string, const gchar *cnc_string,
					const gchar *auth_string, GdaConnectionOptions options);

#define _gda_server_provider_open_connection_sync(p,c,pa,a,e) \
	_gda_server_provider_open_connection((p), (c), (pa), (a), NULL, NULL, NULL, (e))
#define _gda_server_provider_open_connection_async(p,c,pa,a,f,d,ou,e)	\
	_gda_server_provider_open_connection((p), (c), (pa), (a), (f), (d), (ou), (e))

gboolean
_gda_server_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaQuarkList *params, GdaQuarkList *auth,
				      GdaConnectionOpenFunc cb_func, gpointer data, guint *out_job_id,
				      GError **error);
gboolean
_gda_server_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc,
				       GError **error);

gboolean
_gda_server_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GError **error);

gchar *
_gda_server_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
					GSList **params_used, GError **error);

gchar *
_gda_server_provider_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *id, gboolean for_meta_store, gboolean force_quotes);

GObject *
_gda_server_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GdaSet *params,
					GdaStatementModelUsage model_usage,
					GType *col_types, GdaSet **last_inserted_row, GError **error);
gboolean
_gda_server_provider_meta_0arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, GError **error);

gboolean
_gda_server_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GdaTransactionIsolation level, GError **error);
gboolean

_gda_server_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error);

gboolean
_gda_server_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error);

gboolean
_gda_server_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error);

gboolean
_gda_server_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error);

gboolean
_gda_server_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error);

typedef enum {
	GDA_XA_START,
	GDA_XA_END,
	GDA_XA_PREPARE,
	GDA_XA_COMMIT,
	GDA_XA_ROLLBACK,
	GDA_XA_RECOVER
} GdaXaType;

gboolean
_gda_server_provider_xa (GdaServerProvider *provider, GdaConnection *cnc, const GdaXaTransactionId *trx, GdaXaType type, GError **error);
#define  _gda_server_provider_xa_start(prov,cnc,trx,error) _gda_server_provider_xa((prov),(cnc),(trx),GDA_XA_START,(error))
#define  _gda_server_provider_xa_end(prov,cnc,trx,error) _gda_server_provider_xa((prov),(cnc),(trx),GDA_XA_END,(error))
#define  _gda_server_provider_xa_prepare(prov,cnc,trx,error) _gda_server_provider_xa((prov),(cnc),(trx),GDA_XA_PREPARE,(error))
#define  _gda_server_provider_xa_commit(prov,cnc,trx,error) _gda_server_provider_xa((prov),(cnc),(trx),GDA_XA_COMMIT,(error))
#define  _gda_server_provider_xa_rollback(prov,cnc,trx,error) _gda_server_provider_xa((prov),(cnc),(trx),GDA_XA_ROLLBACK,(error))

GList *
_gda_server_provider_xa_recover (GdaServerProvider *prov, GdaConnection *cnc, GError **error);

gboolean
_gda_server_provider_meta_1arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, GError **error);

gboolean
_gda_server_provider_meta_2arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1, GError **error);

gboolean
_gda_server_provider_meta_3arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1, const GValue *value2, GError **error);

gboolean
_gda_server_provider_meta_4arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1, const GValue *value2, const GValue *value3, GError **error);


gboolean
_gda_server_provider_meta_5arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1, const GValue *value2, const GValue *value3, const GValue *value4, GError **error);


G_END_DECLS

#endif



