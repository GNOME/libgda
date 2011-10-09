/*
 * Copyright (C) 2000 Reinhard MÃ¼ller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Cleber Rodrigues <cleberrrjr@bol.com.br>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2003 Filip Van Raemdonck <mechanix@debian.org>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2004 José María Casanova Crespo <jmcasanova@igalia.com>
 * Copyright (C) 2005 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
	GDA_CONNECTION_NO_CNC_SPEC_ERROR,
	GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
	GDA_CONNECTION_OPEN_ERROR,
	GDA_CONNECTION_STATEMENT_TYPE_ERROR,
	GDA_CONNECTION_CANT_LOCK_ERROR,
	GDA_CONNECTION_TASK_NOT_FOUND_ERROR,
	GDA_CONNECTION_UNSUPPORTED_THREADS_ERROR,
	GDA_CONNECTION_CLOSED_ERROR,
	GDA_CONNECTION_META_DATA_CONTEXT_ERROR
} GdaConnectionError;

#define GDA_CONNECTION_NONEXIST_DSN_ERROR GDA_CONNECTION_DSN_NOT_FOUND_ERROR

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

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * GdaConnectionOptions:
 * @GDA_CONNECTION_OPTIONS_NONE: no specific aspect
 * @GDA_CONNECTION_OPTIONS_READ_ONLY: this flag specifies that the connection to open should be in a read-only mode
 *                                    (this policy is not correctly enforced at the moment)
 * @GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE: this flag specifies that SQL identifiers submitted as input
 *                                    to Libgda have to keep their case sensitivity. 
 * @GDA_CONNECTION_OPTIONS_THREAD_SAFE: this flag specifies that the connection to open will be used 
 *                                     by several threads at once so it has to be thread safe
 * @GDA_CONNECTION_OPTIONS_THREAD_ISOLATED: this flag specifies that the connection to open will be used 
 *                                     by several threads at once and requests that the real connection be used
 *                                     only in a sub thread created specifically for it
 * @GDA_CONNECTION_OPTIONS_AUTO_META_DATA: this flags specifies that if a #GdaMetaStore has been associated
 *                                     to the connection, then it is kept up to date with the evolutions in the
 *                                     database's structure. Be aware however that there are some drawbacks
 *                                     explained below.
 *
 * Specifies some aspects of a connection when opening it.
 *
 * Additional information about the GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE flag:
 * <itemizedlist>
 *   <listitem><para>For example without this flag, if the table
 *       name specified in a #GdaServerOperation to create a table is
 *       <emphasis>MyTable</emphasis>, then usually the database will create a table named
 *       <emphasis>mytable</emphasis>, whereas with this flag, the table will be created
 *       as <emphasis>MyTable</emphasis> (note that in the end the database may still decide
 *       to name the table <emphasis>mytable</emphasis> or differently if it can't do
 *       otherwise).</para></listitem>
 *   <listitem><para>Libgda will not apply this rule when parsing SQL code, the SQL code being parsed
 *       has to be conform to the database it will be used with</para></listitem>
 * </itemizedlist>
 *
 * Additional information about the GDA_CONNECTION_OPTIONS_THREAD_SAFE and GDA_CONNECTION_OPTIONS_THREAD_ISOLATED flags:
 * The GDA_CONNECTION_OPTIONS_THREAD_SAFE flag specifies that it has to be able to use the returned connection object from
 * several threads at once (locking is ensured by the #GdaConnection itself). Depending on the database provider's
 * implementation and on the native libraries it uses, the "normal" connection object might not respect this requirement,
 * and in this case a specific thread is started and used as the unique thread which will manipulate the actual connection,
 * while a "wrapper connection" is actually returned and used by the caller (that wrapper connection passes method calls
 * from the calling thread to the actual connection's specific thread, and gets the results back).
 *
 * The GDA_CONNECTION_OPTIONS_THREAD_ISOLATED forces using a specific thread and a "wrapper connection" even if the
 * "normal" connection would itself be thread safe; this is useful for example to be sure the asynchronous API can
 * always be used (see gda_connection_async_statement_execute()).
 *
 * Having a specific thread and a "wrapper connection" definitely has an impact on the performances (because it involves
 * messages passing between threads for every method call), so using the
 * GDA_CONNECTION_OPTIONS_THREAD_SAFE or GDA_CONNECTION_OPTIONS_THREAD_ISOLATED flags should be carefully considered.
 *
 * Note about the @GDA_CONNECTION_OPTIONS_AUTO_META_DATA flag:
 * <itemizedlist>
 *  <listitem><para>Every time a DDL statement is successfully executed, the associated mate data, if
              defined, will be updated, which has a impact on performances</para></listitem>
 *  <listitem><para>If a transaction is started and some DDL statements are executed and the transaction
 *            is not rolled back or committed, then the meta data may end up being wrong</para></listitem>
 * </itemizedlist>
 */
typedef enum {
	GDA_CONNECTION_OPTIONS_NONE = 0,
	GDA_CONNECTION_OPTIONS_READ_ONLY = 1 << 0,
	GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE = 1 << 1,
	GDA_CONNECTION_OPTIONS_THREAD_SAFE = 1 << 2,
	GDA_CONNECTION_OPTIONS_THREAD_ISOLATED = 1 << 3,
	GDA_CONNECTION_OPTIONS_AUTO_META_DATA = 1 << 4
} GdaConnectionOptions;


/**
 * GdaConnectionFeature
 * @GDA_CONNECTION_FEATURE_AGGREGATES: test for aggregates support
 * @GDA_CONNECTION_FEATURE_BLOBS: test for BLOBS (binary large objects) support
 * @GDA_CONNECTION_FEATURE_INDEXES: test for indexes support
 * @GDA_CONNECTION_FEATURE_INHERITANCE: test for tables inheritance support
 * @GDA_CONNECTION_FEATURE_NAMESPACES: test for namespaces support
 * @GDA_CONNECTION_FEATURE_PROCEDURES: test for functions support
 * @GDA_CONNECTION_FEATURE_SEQUENCES: test for sequences support
 * @GDA_CONNECTION_FEATURE_SQL: test for SQL language (even specific to the database) support
 * @GDA_CONNECTION_FEATURE_TRANSACTIONS: test for transactions support
 * @GDA_CONNECTION_FEATURE_SAVEPOINTS: test for savepoints within transactions support
 * @GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE: test if savepoints can be removed
 * @GDA_CONNECTION_FEATURE_TRIGGERS: test for triggers support
 * @GDA_CONNECTION_FEATURE_UPDATABLE_CURSOR: test for updatable cursors support
 * @GDA_CONNECTION_FEATURE_USERS: test for users support
 * @GDA_CONNECTION_FEATURE_VIEWS: test for views support
 * @GDA_CONNECTION_FEATURE_XA_TRANSACTIONS: test for distributed transactions support
 * @GDA_CONNECTION_FEATURE_MULTI_THREADING: test for native multi-threading support
 * @GDA_CONNECTION_FEATURE_LAST: not used
 *
 * Used in gda_connection_supports_feature() and gda_server_provider_supports_feature() to test if a connection
 * or a database provider supports some specific feature.
 */
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
	
	GDA_CONNECTION_FEATURE_MULTI_THREADING,

	GDA_CONNECTION_FEATURE_LAST
} GdaConnectionFeature;


/**
 * GdaConnectionSchema
 *
 * Deprecated: 4.2: This was a leftover from the pre 4.0 area
 */
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

/**
 * GdaConnectionMetaType:
 * @GDA_CONNECTION_META_NAMESPACES: lists the <link linkend="GdaConnectionMetaTypeGDA_CONNECTION_META_NAMESPACES">namespaces</link> (or schemas for PostgreSQL)
 * @GDA_CONNECTION_META_TYPES: lists the <link linkend="GdaConnectionMetaTypeGDA_CONNECTION_META_TYPES">database types</link>
 * @GDA_CONNECTION_META_TABLES: lists the <link linkend="GdaConnectionMetaTypeGDA_CONNECTION_META_TABLES">tables</link>
 * @GDA_CONNECTION_META_VIEWS: lists the <link linkend="GdaConnectionMetaTypeGDA_CONNECTION_META_VIEWS">views</link>
 * @GDA_CONNECTION_META_FIELDS: lists the <link linkend="GdaConnectionMetaTypeGDA_CONNECTION_META_FIELDS">table's or view's fields</link>
 * @GDA_CONNECTION_META_INDEXES: lists the <link linkend="GdaConnectionMetaTypeGDA_CONNECTION_META_INDEXES">table's indexes</link>
 *
 * Used with gda_connection_get_meta_store_data() to describe what meta data to extract from
 * a connection's associated #GdaMetaStore.
 */
typedef enum {
	GDA_CONNECTION_META_NAMESPACES,
	GDA_CONNECTION_META_TYPES,
	GDA_CONNECTION_META_TABLES,
	GDA_CONNECTION_META_VIEWS,
	GDA_CONNECTION_META_FIELDS,
	GDA_CONNECTION_META_INDEXES
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

GdaStatement        *gda_connection_parse_sql_string     (GdaConnection *cnc, const gchar *sql, GdaSet **params,
                    GError **error);

/*
 * Quick commands execution
 */
GdaDataModel*       gda_connection_execute_select_command        (GdaConnection *cnc, const gchar *sql, GError **error);
gint                gda_connection_execute_non_select_command    (GdaConnection *cnc, const gchar *sql, GError **error);

/*
 * Data in tables manipulation
 */
gboolean            gda_connection_insert_row_into_table        (GdaConnection *cnc, const gchar *table, GError **error, ...);
gboolean            gda_connection_insert_row_into_table_v      (GdaConnection *cnc, const gchar *table,
						      GSList *col_names, GSList *values,
						      GError **error);

gboolean            gda_connection_update_row_in_table          (GdaConnection *cnc, const gchar *table,
						      const gchar *condition_column_name,
						      GValue *condition_value, GError **error, ...);
gboolean            gda_connection_update_row_in_table_v        (GdaConnection *cnc, const gchar *table,
						      const gchar *condition_column_name,
						      GValue *condition_value,
						      GSList *col_names, GSList *values,
						      GError **error);
gboolean            gda_connection_delete_row_from_table        (GdaConnection *cnc, const gchar *table,
						      const gchar *condition_column_name,
						      GValue *condition_value, GError **error);

const GList         *gda_connection_get_events           (GdaConnection *cnc);

GdaSqlParser        *gda_connection_create_parser        (GdaConnection *cnc);
GSList              *gda_connection_batch_execute        (GdaConnection *cnc,
							  GdaBatch *batch, GdaSet *params,
							  GdaStatementModelUsage model_usage, GError **error);

gchar               *gda_connection_quote_sql_identifier (GdaConnection *cnc, const gchar *id);
gchar               *gda_connection_statement_to_sql     (GdaConnection *cnc,
							  GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
							  GSList **params_used, GError **error);
/* synchronous exec */
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

/* Async. execution */
guint                gda_connection_async_statement_execute (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
							     GdaStatementModelUsage model_usage, GType *col_types,
							     gboolean need_last_insert_row, 
							     GError **error);
GObject             *gda_connection_async_fetch_result      (GdaConnection *cnc, guint task_id, GdaSet **last_insert_row, GError **error);
gboolean             gda_connection_async_cancel            (GdaConnection *cnc, guint task_id, GError **error);


/* repetitive statement */
GSList             *gda_connection_repetitive_statement_execute (GdaConnection *cnc, GdaRepetitiveStatement *rstmt,
								 GdaStatementModelUsage model_usage, GType *col_types,
								 gboolean stop_on_error, GError **error);

/* transactions */
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
