/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
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

#ifndef __GDA_SERVER_PROVIDER_H__
#define __GDA_SERVER_PROVIDER_H__

#include <libgda/gda-server-operation.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-index.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-meta-store.h>

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
	GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
	GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
	GDA_SERVER_PROVIDER_OPERATION_ERROR,
	GDA_SERVER_PROVIDER_INTERNAL_ERROR,
	GDA_SERVER_PROVIDER_BUSY_ERROR,
	GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR
} GdaServerProviderError;

struct _GdaServerProvider {
	GObject                   object;
	GdaServerProviderPrivate *priv;
};

typedef struct {
	gboolean (*info)          (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **);
	gboolean (*btypes)        (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **);
	gboolean (*schemata)      (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, 
				   const GValue *schema_name);
	gboolean (*tables_views)  (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **);
	gboolean (*tables_views_s)(GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, 
				   const GValue *table_schema, const GValue *table_name);
	gboolean (*columns)       (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **);
	gboolean (*columns_t)     (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, 
				   const GValue *table_schema, const GValue *table_name);
	gboolean (*columns_c)     (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, 
				   const GValue *table_schema, const GValue *table_name, const GValue *column_name);
} GdaServerProviderMeta;

typedef void (*GdaServerProviderAsyncCallback) (GdaServerProvider *provider, GdaConnection *cnc, guint task_id, 
						gboolean result_status, gpointer data);

struct _GdaServerProviderClass {
	GObjectClass parent_class;

	/* provider information */
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
	gchar                  *(* statement_to_sql)     (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
							  GSList **params_used, GError **error);
	gboolean                (* statement_prepare)    (GdaServerProvider *provider, GdaConnection *cnc,
							  GdaStatement *stmt, GError **error);
	GObject                *(* statement_execute)    (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaStatement *stmt, GdaSet *params, 
							  GdaStatementModelUsage model_usage, 
							  GType *col_types, GdaSet **last_inserted_row, 
							  guint *task_id, GdaServerProviderAsyncCallback async_cb, 
							  gpointer cb_data, GError **error);

	/* Misc */
	gboolean                (* is_busy)              (GdaServerProvider *provider, GdaConnection *cnc, GError **error);
	gboolean                (* cancel)               (GdaServerProvider *provider, GdaConnection *cnc, 
							  guint task_id, GError **error);
	GdaConnection          *(* create_connection)    (GdaServerProvider *provider);
	GdaServerProviderMeta      meta_funcs;

	/* Padding for future expansion */
	void                    (*_gda_reserved1)        (void);
	void                    (*_gda_reserved2)        (void);
	void                    (*_gda_reserved3)        (void);
	void                    (*_gda_reserved4)        (void);
	void                    (*_gda_reserved5)        (void);
	void                    (*_gda_reserved6)        (void);
};

GType                  gda_server_provider_get_type (void) G_GNUC_CONST;

/* provider information */
const gchar           *gda_server_provider_get_name           (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_version        (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);
gboolean               gda_server_provider_supports_feature   (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaConnectionFeature feature);

/* types and values manipulation */
GdaDataHandler        *gda_server_provider_get_data_handler_gtype(GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType for_type);
GdaDataHandler        *gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *for_type);
GValue                *gda_server_provider_string_to_value       (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *string, 
								  GType prefered_type,
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
