/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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

#ifndef __GDA_CONNECTION_H__
#define __GDA_CONNECTION_H__

#include "gda-decl.h"
#include <libgda/gda-data-model.h>
#include <libgda/gda-connection-event.h>
#include <libgda/gda-transaction-status.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-server-operation.h>

G_BEGIN_DECLS

#define GDA_TYPE_CONNECTION            (gda_connection_get_type())
#define GDA_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION, GdaConnection))
#define GDA_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONNECTION, GdaConnectionClass))
#define GDA_IS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CONNECTION))
#define GDA_IS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION))

/* error reporting */
extern GQuark gda_connection_error_quark (void);
#define GDA_CONNECTION_ERROR gda_connection_error_quark ()

typedef enum {
	GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
	GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR,
	GDA_CONNECTION_PROVIDER_ERROR,
        GDA_CONNECTION_CONN_OPEN_ERROR,
        GDA_CONNECTION_DO_QUERY_ERROR,
	GDA_CONNECTION_NONEXIST_DSN_ERROR,
	GDA_CONNECTION_NO_CNC_SPEC_ERROR,
	GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
	GDA_CONNECTION_OPEN_ERROR,
	GDA_CONNECTION_EXECUTE_COMMAND_ERROR,
	GDA_CONNECTION_STATEMENT_TYPE_ERROR
} GdaConnectionError;

struct _GdaConnection {
	GObject               object;
	GdaConnectionPrivate *priv;
};

struct _GdaConnectionClass {
	GObjectClass          object_class;

	/* signals */
	void   (*error)                     (GdaConnection *cnc, GdaConnectionEvent *error);
        void   (*conn_opened)               (GdaConnection *obj);
        void   (*conn_to_close)             (GdaConnection *obj);
        void   (*conn_closed)               (GdaConnection *obj);
	void   (*dsn_changed)               (GdaConnection *obj);
	void   (*transaction_status_changed)(GdaConnection *obj);
};

typedef enum {
        GDA_CONNECTION_OPTIONS_NONE = 0,
	GDA_CONNECTION_OPTIONS_READ_ONLY = 1 << 0,
} GdaConnectionOptions;

typedef enum {
	GDA_CONNECTION_FEATURE_AGGREGATES,
	GDA_CONNECTION_FEATURE_BLOBS,
	GDA_CONNECTION_FEATURE_INDEXES,
	GDA_CONNECTION_FEATURE_INHERITANCE,
	GDA_CONNECTION_FEATURE_NAMESPACES,
	GDA_CONNECTION_FEATURE_PROCEDURES,
	GDA_CONNECTION_FEATURE_SEQUENCES,
	GDA_CONNECTION_FEATURE_SQL,
	GDA_CONNECTION_FEATURE_TRANSACTIONS,
	GDA_CONNECTION_FEATURE_SAVEPOINTS,
	GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE,
	GDA_CONNECTION_FEATURE_TRIGGERS,
	GDA_CONNECTION_FEATURE_UPDATABLE_CURSOR,
	GDA_CONNECTION_FEATURE_USERS,
	GDA_CONNECTION_FEATURE_VIEWS,
	GDA_CONNECTION_FEATURE_XA_TRANSACTIONS,

	GDA_CONNECTION_FEATURE_LAST
} GdaConnectionFeature;

typedef enum {
	GDA_CONNECTION_SCHEMA_AGGREGATES,
	GDA_CONNECTION_SCHEMA_DATABASES,
	GDA_CONNECTION_SCHEMA_FIELDS,
	GDA_CONNECTION_SCHEMA_INDEXES,
	GDA_CONNECTION_SCHEMA_LANGUAGES,
	GDA_CONNECTION_SCHEMA_NAMESPACES,
	GDA_CONNECTION_SCHEMA_PARENT_TABLES,
	GDA_CONNECTION_SCHEMA_PROCEDURES,
	GDA_CONNECTION_SCHEMA_SEQUENCES,
	GDA_CONNECTION_SCHEMA_TABLES,
	GDA_CONNECTION_SCHEMA_TRIGGERS,
	GDA_CONNECTION_SCHEMA_TYPES,
	GDA_CONNECTION_SCHEMA_USERS,
	GDA_CONNECTION_SCHEMA_VIEWS,
	GDA_CONNECTION_SCHEMA_CONSTRAINTS,
	GDA_CONNECTION_SCHEMA_TABLE_CONTENTS
} GdaConnectionSchema;

typedef enum {
	GDA_CONNECTION_META_NAMESPACES,
	GDA_CONNECTION_META_TYPES,
	GDA_CONNECTION_META_TABLES,
	GDA_CONNECTION_META_VIEWS,
	GDA_CONNECTION_META_FIELDS
} GdaConnectionMetaType;


GType                gda_connection_get_type             (void) G_GNUC_CONST;
GdaConnection       *gda_connection_open_from_dsn        (const gchar *dsn, const gchar *auth_string,
							  GdaConnectionOptions options, GError **error);
GdaConnection       *gda_connection_open_from_string     (const gchar *provider_name, 
							  const gchar *cnc_string, const gchar *auth_string,
							  GdaConnectionOptions options, GError **error);
gboolean             gda_connection_open                 (GdaConnection *cnc, GError **error);
void                 gda_connection_close                (GdaConnection *cnc);
void                 gda_connection_close_no_warning     (GdaConnection *cnc);
gboolean             gda_connection_is_opened            (GdaConnection *cnc);

GdaConnectionOptions gda_connection_get_options          (GdaConnection *cnc);

GdaServerProvider   *gda_connection_get_provider         (GdaConnection *cnc);
const gchar         *gda_connection_get_provider_name    (GdaConnection *cnc);

GdaServerOperation  *gda_connection_create_operation     (GdaConnection *cnc, GdaServerOperationType type,
                                                          GdaSet *options, GError **error);
gboolean             gda_connection_perform_operation    (GdaConnection *cnc, GdaServerOperation *op, GError **error);
                                                          
const gchar         *gda_connection_get_dsn              (GdaConnection *cnc);
const gchar         *gda_connection_get_cnc_string       (GdaConnection *cnc);
const gchar         *gda_connection_get_authentication   (GdaConnection *cnc);

const GList         *gda_connection_get_events           (GdaConnection *cnc);

GdaSqlParser        *gda_connection_create_parser        (GdaConnection *cnc);
GSList              *gda_connection_batch_execute        (GdaConnection *cnc,
							  GdaBatch *batch, GdaSet *params,
							  GdaStatementModelUsage model_usage, GError **error);

gchar               *gda_connection_statement_to_sql     (GdaConnection *cnc,
							  GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
							  GSList **params_used, GError **error);
gboolean             gda_connection_statement_prepare    (GdaConnection *cnc,
							  GdaStatement *stmt, GError **error);
GObject             *gda_connection_statement_execute    (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
							  GdaStatementModelUsage model_usage, GdaSet **last_insert_row,
							  GError **error);
GdaDataModel        *gda_connection_statement_execute_select (GdaConnection *cnc, GdaStatement *stmt,
							      GdaSet *params, GError **error);
GdaDataModel        *gda_connection_statement_execute_select_fullv (GdaConnection *cnc, GdaStatement *stmt,
								    GdaSet *params, GdaStatementModelUsage model_usage,
								    GError **error, ...);
GdaDataModel        *gda_connection_statement_execute_select_full (GdaConnection *cnc, GdaStatement *stmt,
								   GdaSet *params, GdaStatementModelUsage model_usage,
								   GType *col_types, GError **error);
gint                 gda_connection_statement_execute_non_select (GdaConnection *cnc, GdaStatement *stmt,
								  GdaSet *params, GdaSet **last_insert_row, GError **error);

gboolean             gda_connection_begin_transaction    (GdaConnection *cnc, const gchar *name, 
							  GdaTransactionIsolation level, GError **error);
gboolean             gda_connection_commit_transaction   (GdaConnection *cnc, const gchar *name, GError **error);
gboolean             gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *name, GError **error);

gboolean             gda_connection_add_savepoint        (GdaConnection *cnc, const gchar *name, GError **error);
gboolean             gda_connection_rollback_savepoint   (GdaConnection *cnc, const gchar *name, GError **error);
gboolean             gda_connection_delete_savepoint     (GdaConnection *cnc, const gchar *name, GError **error);

GdaTransactionStatus *gda_connection_get_transaction_status (GdaConnection *cnc);

gchar               *gda_connection_value_to_sql_string  (GdaConnection *cnc, GValue *from);

gboolean             gda_connection_supports_feature     (GdaConnection *cnc, GdaConnectionFeature feature);
GdaMetaStore        *gda_connection_get_meta_store       (GdaConnection *cnc);
gboolean             gda_connection_update_meta_store    (GdaConnection *cnc, GdaMetaContext *context, GError **error);
GdaDataModel        *gda_connection_get_meta_store_data  (GdaConnection *cnc, GdaConnectionMetaType meta_type,
							  GError **error, gint nb_filters, ...);
GdaDataModel        *gda_connection_get_meta_store_data_v(GdaConnection *cnc, GdaConnectionMetaType meta_type,
							  GList* filters, GError **error);
G_END_DECLS

#endif
