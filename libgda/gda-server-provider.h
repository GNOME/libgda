/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
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

#ifndef __GDA_SERVER_PROVIDER_H__
#define __GDA_SERVER_PROVIDER_H__

#include <libgda/gda-server-operation.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-xa-transaction.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_PROVIDER            (gda_server_provider_get_type())
#define GDA_SERVER_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_PROVIDER, GdaServerProvider))
#define GDA_SERVER_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_PROVIDER, GdaServerProviderClass))
#define GDA_IS_SERVER_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_PROVIDER))
#define GDA_IS_SERVER_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_PROVIDER))

/* error reporting */
extern GQuark gda_server_provider_error_quark (void);
#define GDA_SERVER_PROVIDER_ERROR gda_server_provider_error_quark ()

typedef enum
{
        GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
	GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
	GDA_SERVER_PROVIDER_EMPTY_STMT_ERROR,
	GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
	GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
	GDA_SERVER_PROVIDER_OPERATION_ERROR,
	GDA_SERVER_PROVIDER_INTERNAL_ERROR,
	GDA_SERVER_PROVIDER_BUSY_ERROR,
	GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
	GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
	GDA_SERVER_PROVIDER_DATA_ERROR,
	GDA_SERVER_PROVIDER_DEFAULT_VALUE_HANDLING_ERROR,
	GDA_SERVER_PROVIDER_MISUSE_ERROR,
	GDA_SERVER_PROVIDER_FILE_NOT_FOUND_ERROR
} GdaServerProviderError;

struct _GdaServerProvider {
	GObject                   object;
	GdaServerProviderPrivate *priv;
};


/**
 * GdaServerProviderMeta:
 * @_info: 
 * @_btypes: 
 * @_udt: 
 * @udt: 
 * @_udt_cols: 
 * @udt_cols: 
 * @_enums: 
 * @enums: 
 * @_domains: 
 * @domains: 
 * @_constraints_dom: 
 * @constraints_dom: 
 * @_el_types: 
 * @el_types: 
 * @_collations: 
 * @collations: 
 * @_character_sets: 
 * @character_sets: 
 * @_schemata: 
 * @schemata: 
 * @_tables_views: 
 * @tables_views: 
 * @_columns: 
 * @columns: 
 * @_view_cols: 
 * @view_cols: 
 * @_constraints_tab: 
 * @constraints_tab: 
 * @_constraints_ref: 
 * @constraints_ref: 
 * @_key_columns: 
 * @key_columns: 
 * @_check_columns: 
 * @check_columns: 
 * @_triggers: 
 * @triggers: 
 * @_routines: 
 * @routines: 
 * @_routine_col: 
 * @routine_col: 
 * @_routine_par: 
 * @routine_par: 
 * @_indexes_tab: 
 * @indexes_tab: 
 * @_index_cols: 
 * @index_cols:
 *
 * These methods must be implemented by providers to update a connection's associated metadata (in a 
 * #GdaMetaStore object), see the <link linkend="prov-metadata">Virtual methods for providers/Methods - metadata</link>
 * for more information.
 */
typedef struct {
	/* _information_schema_catalog_name */
	gboolean (*_info)            (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);

	/* _builtin_data_types */
	gboolean (*_btypes)          (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);

	/* _udt */
	gboolean (*_udt)             (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*udt)              (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *udt_catalog, const GValue *udt_schema);

	/* _udt_columns */
	gboolean (*_udt_cols)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*udt_cols)         (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name);

	/* _enums */
	gboolean (*_enums)           (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*enums)            (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name);

	/* _domains */
	gboolean (*_domains)         (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*domains)          (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *domain_catalog, const GValue *domain_schema);

	/* _domain_constraints */
	gboolean (*_constraints_dom) (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*constraints_dom)  (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *domain_catalog, const GValue *domain_schema, const GValue *domain_name);

	/* _element_types */
	gboolean (*_el_types)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*el_types)         (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *specific_name);

	/* _collations */
	gboolean (*_collations)       (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*collations)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				       const GValue *collation_catalog, const GValue *collation_schema, 
				       const GValue *collation_name_n);

	/* _character_sets */
	gboolean (*_character_sets)  (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*character_sets)   (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *chset_catalog, const GValue *chset_schema, const GValue *chset_name_n);

	/* _schemata */
	gboolean (*_schemata)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*schemata)         (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error, 
				      const GValue *catalog_name, const GValue *schema_name_n);

	/* _tables or _views */
	gboolean (*_tables_views)    (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*tables_views)     (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n);

	/* _columns */
	gboolean (*_columns)         (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*columns)          (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name);

	/* _view_column_usage */
	gboolean (*_view_cols)       (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*view_cols)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *view_catalog, const GValue *view_schema, const GValue *view_name);

	/* _table_constraints */
	gboolean (*_constraints_tab) (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*constraints_tab)  (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error, 
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
				      const GValue *constraint_name_n);

	/* _referential_constraints */
	gboolean (*_constraints_ref) (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*constraints_ref)  (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				      const GValue *constraint_name);

	/* _key_column_usage */
	gboolean (*_key_columns)     (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*key_columns)      (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				      const GValue *constraint_name);

	/* _check_column_usage */
	gboolean (*_check_columns)   (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*check_columns)    (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
				      const GValue *constraint_name);

	/* _triggers */
	gboolean (*_triggers)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*triggers)         (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name);

	/* _routines */
	gboolean (*_routines)       (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*routines)        (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				     const GValue *routine_catalog, const GValue *routine_schema, 
				     const GValue *routine_name_n);

	/* _routine_columns */
	gboolean (*_routine_col)     (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*routine_col)      (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *rout_catalog, const GValue *rout_schema, const GValue *rout_name);

	/* _parameters */
	gboolean (*_routine_par)     (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*routine_par)      (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *rout_catalog, const GValue *rout_schema, const GValue *rout_name);
	/* _table_indexes */
	gboolean (*_indexes_tab)     (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*indexes_tab)      (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error, 
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
				      const GValue *index_name_n);

	/* _index_column_usage */
	gboolean (*_index_cols)      (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
	gboolean (*index_cols)       (GdaServerProvider *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error,
				      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, const GValue *index_name);
	
	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved5) (void);
	void (*_gda_reserved6) (void);
	void (*_gda_reserved7) (void);
	void (*_gda_reserved8) (void);
	void (*_gda_reserved9) (void);
	void (*_gda_reserved10) (void);
	void (*_gda_reserved11) (void);
	void (*_gda_reserved12) (void);
	void (*_gda_reserved13) (void);
	void (*_gda_reserved14) (void);
	void (*_gda_reserved15) (void);
	void (*_gda_reserved16) (void);

} GdaServerProviderMeta;

/* distributed transaction support */
typedef struct {
	gboolean (*xa_start)    (GdaServerProvider *prov, GdaConnection *cnc, const GdaXaTransactionId *trx, GError **error);

	gboolean (*xa_end)      (GdaServerProvider *prov, GdaConnection *cnc, const GdaXaTransactionId *trx, GError **error);
	gboolean (*xa_prepare)  (GdaServerProvider *prov, GdaConnection *cnc, const GdaXaTransactionId *trx, GError **error);

	gboolean (*xa_commit)   (GdaServerProvider *prov, GdaConnection *cnc, const GdaXaTransactionId *trx, GError **error);
	gboolean (*xa_rollback) (GdaServerProvider *prov, GdaConnection *cnc, const GdaXaTransactionId *trx, GError **error);

	GList   *(*xa_recover)  (GdaServerProvider *prov, GdaConnection *cnc, GError **error);
} GdaServerProviderXa;

/**
 * GdaServerProviderAsyncCallback:
 * @provider: 
 * @cnc: 
 * @task_id: 
 * @result_status: 
 * @error: 
 * @data: 
 *
 * Function to be called by Libgda when the associated asynchronous method invoked finishes.
 */
typedef void (*GdaServerProviderAsyncCallback) (GdaServerProvider *provider, GdaConnection *cnc, guint task_id, 
						gboolean result_status, const GError *error, gpointer data);
/**
 * GdaServerProviderExecCallback:
 * @provider: 
 * @cnc: 
 * @task_id: 
 * @result_obj: 
 * @error: 
 * @data: 
 *
 * Function to be called by Libgda when the associated asynchronous method invoked finishes
 */
typedef void (*GdaServerProviderExecCallback) (GdaServerProvider *provider, GdaConnection *cnc, guint task_id, 
					       GObject *result_obj, const GError *error, gpointer data);

/**
 * GDA_SERVER_PROVIDER_UNDEFINED_LIMITING_THREAD: (skip)
 */
#define GDA_SERVER_PROVIDER_UNDEFINED_LIMITING_THREAD ((gpointer)0x1)
struct _GdaServerProviderClass {
	GObjectClass parent_class;

	/* provider information */
	GThread                 * limiting_thread; /* if not NULL, then using the provider will be limited to this thread */
	const gchar           *(* get_name)              (GdaServerProvider *provider);
	const gchar           *(* get_version)           (GdaServerProvider *provider);
	const gchar           *(* get_server_version)    (GdaServerProvider *provider, GdaConnection *cnc);
	gboolean               (* supports_feature)      (GdaServerProvider *provider, GdaConnection *cnc,
							  GdaConnectionFeature feature);
	/* types and values manipulation */
	GdaDataHandler        *(* get_data_handler)      (GdaServerProvider *provider, GdaConnection *cnc,
							  GType g_type, const gchar *dbms_type);
	const gchar           *(*get_def_dbms_type)      (GdaServerProvider *provider, GdaConnection *cnc, GType g_type);
	gchar                 *(*escape_string)          (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str);
	gchar                 *(*unescape_string)        (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str);

	/* connections management */
	gboolean               (* open_connection)       (GdaServerProvider *provider, GdaConnection *cnc,
							  GdaQuarkList *params, GdaQuarkList *auth,
							  guint *task_id, GdaServerProviderAsyncCallback async_cb, 
							  gpointer cb_data);
	gboolean               (* close_connection)      (GdaServerProvider *provider, GdaConnection *cnc);
	
	const gchar           *(* get_database)          (GdaServerProvider *provider, GdaConnection *cnc);

	/* operations */
	gboolean               (* supports_operation)    (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaServerOperationType type, GdaSet *options);
	GdaServerOperation    *(* create_operation)      (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaServerOperationType type, GdaSet *options, GError **error);
	gchar                 *(* render_operation)      (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaServerOperation *op, GError **error);
	gboolean               (* perform_operation)     (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaServerOperation *op, 
							  guint *task_id, GdaServerProviderAsyncCallback async_cb, 
							  gpointer cb_data, GError **error);
	
	/* transactions */
	gboolean                (* begin_transaction)    (GdaServerProvider *provider, GdaConnection *cnc,
							  const gchar *name, GdaTransactionIsolation level, GError **error);
	gboolean                (* commit_transaction)   (GdaServerProvider *provider, GdaConnection *cnc,
							  const gchar *name, GError **error);
	gboolean                (* rollback_transaction) (GdaServerProvider *provider, GdaConnection *cnc,
							  const gchar *name, GError **error);
	gboolean                (* add_savepoint)        (GdaServerProvider *provider, GdaConnection *cnc,
							  const gchar *name, GError **error);
	gboolean                (* rollback_savepoint)   (GdaServerProvider *provider, GdaConnection *cnc,
							  const gchar *name, GError **error);
	gboolean                (* delete_savepoint)     (GdaServerProvider *provider, GdaConnection *cnc,
							  const gchar *name, GError **error);

	/* GdaStatement */
	GdaSqlParser           *(* create_parser)        (GdaServerProvider *provider, GdaConnection *cnc);
	
	/**
	 * statement_to_sql:
	 * @cnc: a #GdaConnection object
	 * @stmt: a #GdaStatement object
	 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
	 * @flags: SQL rendering flags, as #GdaStatementSqlFlag OR'ed values
	 * @params_used: (allow-none) (element-type Gda.Holder) (out) (transfer container): a place to store the list of individual #GdaHolder objects within @params which have been used
	 * @error: a place to store errors, or %NULL
	 *
	 * Renders @stmt as an SQL statement, adapted to the SQL dialect used by @cnc
	 *
	 * Returns: a new string, or %NULL if an error occurred
	 */
	gchar                  *(* statement_to_sql)     (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
							  GSList **params_used, GError **error);
	gboolean                (* statement_prepare)    (GdaServerProvider *provider, GdaConnection *cnc,
							  GdaStatement *stmt, GError **error);
	GObject                *(* statement_execute)    (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaStatement *stmt, GdaSet *params, 
							  GdaStatementModelUsage model_usage, 
							  GType *col_types, GdaSet **last_inserted_row, 
							  guint *task_id, GdaServerProviderExecCallback exec_cb, 
							  gpointer cb_data, GError **error);

	/* Misc */
	gboolean                (* is_busy)              (GdaServerProvider *provider, GdaConnection *cnc, GError **error);
	gboolean                (* cancel)               (GdaServerProvider *provider, GdaConnection *cnc, 
							  guint task_id, GError **error);
	GdaConnection          *(* create_connection)    (GdaServerProvider *provider);

	/* meta data reporting */
	GdaServerProviderMeta      meta_funcs;

	/* distributed transaction */
	GdaServerProviderXa       *xa_funcs; /* it is a pointer! => set to %NULL if unsupported by provider */

	/* SQL identifiers quoting */
	gchar                  *(* identifier_quote)    (GdaServerProvider *provider, GdaConnection *cnc,
							 const gchar *id,
							 gboolean for_meta_store, gboolean force_quotes);

	/* Async. handling */
	gboolean                (*handle_async)         (GdaServerProvider *provider, GdaConnection *cnc, GError **error);

	GdaSqlStatement        *(*statement_rewrite)    (GdaServerProvider *provider, GdaConnection *cnc,
							 GdaStatement *stmt, GdaSet *params, GError **error);
	/*< private >*/
	/* Padding for future expansion */
	void                    (*_gda_reserved1)        (void);
	void                    (*_gda_reserved2)        (void);
	void                    (*_gda_reserved3)        (void);
	void                    (*_gda_reserved4)        (void);
	void                    (*_gda_reserved5)        (void);
	void                    (*_gda_reserved6)        (void);
};

/**
 * SECTION:gda-server-provider
 * @short_description: Base class for all the DBMS providers
 * @title: GdaServerProvider
 * @stability: Stable
 * @see_also: #GdaConnection
 *
 * The #GdaServerProvider class is a virtual class which all the DBMS providers
 * must inherit, and implement its virtual methods.
 *
 * See the <link linkend="libgda-provider-class">Virtual methods for providers</link> section for more information
 * about how to implement the virtual methods.
 */

GType                  gda_server_provider_get_type (void) G_GNUC_CONST;

/* provider information */
const gchar           *gda_server_provider_get_name           (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_version        (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);
gboolean               gda_server_provider_supports_feature   (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaConnectionFeature feature);

/* types and values manipulation */
GdaDataHandler        *gda_server_provider_get_data_handler_g_type(GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType for_type);
GdaDataHandler        *gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *for_type);
GValue                *gda_server_provider_string_to_value       (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *string, 
								  GType preferred_type,
								  gchar **dbms_type);
gchar                 *gda_server_provider_value_to_sql_string   (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GValue *from);
const gchar           *gda_server_provider_get_default_dbms_type (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType type);
gchar                 *gda_server_provider_escape_string         (GdaServerProvider *provider,
								  GdaConnection *cnc, const gchar *str);
gchar                 *gda_server_provider_unescape_string       (GdaServerProvider *provider,
								  GdaConnection *cnc, const gchar *str);

/* actions with parameters */
gboolean               gda_server_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperationType type, GdaSet *options);
GdaServerOperation    *gda_server_provider_create_operation   (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperationType type, 
							       GdaSet *options, GError **error);
gchar                 *gda_server_provider_render_operation   (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperation *op, GError **error);
gboolean               gda_server_provider_perform_operation  (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperation *op, GError **error);

/* GdaStatement */
GdaSqlParser          *gda_server_provider_create_parser      (GdaServerProvider *provider, GdaConnection *cnc);

G_END_DECLS

#endif
