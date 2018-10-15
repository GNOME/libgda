/*
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#include <glib-object.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-data-handler.h>

#ifndef __GDA_PROVIDER_H__
#define __GDA_PROVIDER_H__

G_BEGIN_DECLS

#define GDA_TYPE_PROVIDER gda_provider_get_type()

G_DECLARE_INTERFACE(GdaProvider, gda_provider, GDA, PROVIDER, GObject)

struct _GdaProviderInterface
{
  GTypeInterface g_iface;

  const gchar        *(* get_name)            (GdaProvider *provider);
  const gchar        *(* get_version)         (GdaProvider *provider);
  const gchar        *(* get_server_version)  (GdaProvider *provider, GdaConnection *cnc);
  gboolean            (* supports_feature)    (GdaProvider *provider, GdaConnection *cnc,
                                               GdaConnectionFeature feature);
  GdaConnection      *(* create_connection)   (GdaProvider *provider); /* may be NULL */
  GdaSqlParser       *(* create_parser)       (GdaProvider *provider, GdaConnection *cnc); /* may be NULL */
  GdaDataHandler     *(* get_data_handler)    (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                               GType g_type, const gchar *dbms_type);
  const gchar        *(* get_def_dbms_type)   (GdaProvider *provider, GdaConnection *cnc, GType g_type); /* may be NULL */
  gboolean            (* supports_operation)  (GdaProvider *provider, GdaConnection *cnc,
                                               GdaServerOperationType type, GdaSet *options);
  GdaServerOperation *(* create_operation)    (GdaProvider *provider, GdaConnection *cnc,
                                               GdaServerOperationType type, GdaSet *options, GError **error);
  gchar              *(* render_operation)    (GdaProvider *provider, GdaConnection *cnc,
                                               GdaServerOperation *op, GError **error);
  gchar              *(* statement_to_sql)    (GdaProvider *provider, GdaConnection *cnc,
                                               GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
                                               GSList **params_used, GError **error);
  gchar              *(* identifier_quote)    (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                               const gchar *id,
                                               gboolean for_meta_store, gboolean force_quotes);
  GdaSqlStatement    *(* statement_rewrite)   (GdaProvider *provider, GdaConnection *cnc,
                                               GdaStatement *stmt, GdaSet *params, GError **error);
  gboolean            (* open_connection)     (GdaProvider *provider, GdaConnection *cnc,
                                               GdaQuarkList *params, GdaQuarkList *auth);
  gboolean            (* prepare_connection)  (GdaProvider *provider, GdaConnection *cnc,
                                               GdaQuarkList *params, GdaQuarkList *auth);
  gboolean            (* close_connection)    (GdaProvider *provider, GdaConnection *cnc);
  gchar              *(* escape_string)       (GdaProvider *provider, GdaConnection *cnc, const gchar *str); /* may be NULL */
  gchar              *(* unescape_string)     (GdaProvider *provider, GdaConnection *cnc, const gchar *str); /* may be NULL */
  gboolean            (* perform_operation)   (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                               GdaServerOperation *op, GError **error);
  gboolean            (* begin_transaction)   (GdaProvider *provider, GdaConnection *cnc,
                                               const gchar *name, GdaTransactionIsolation level, GError **error);
  gboolean            (* commit_transaction)  (GdaProvider *provider, GdaConnection *cnc,
                                               const gchar *name, GError **error);
  gboolean            (* rollback_transaction)(GdaProvider *provider, GdaConnection *cnc,
                                               const gchar *name, GError **error);
  gboolean            (* add_savepoint)       (GdaProvider *provider, GdaConnection *cnc,
                                               const gchar *name, GError **error);
  gboolean            (* rollback_savepoint)  (GdaProvider *provider, GdaConnection *cnc,
                                               const gchar *name, GError **error);
  gboolean            (* delete_savepoint)    (GdaProvider *provider, GdaConnection *cnc,
                                               const gchar *name, GError **error);
  gboolean            (* statement_prepare)   (GdaProvider *provider, GdaConnection *cnc,
                                               GdaStatement *stmt, GError **error);
  GObject            *(* statement_execute)   (GdaProvider *provider, GdaConnection *cnc,
                                               GdaStatement *stmt, GdaSet *params,
                                               GdaStatementModelUsage model_usage,
                                               GType *col_types, GdaSet **last_inserted_row,
                                               GError **error);
  GdaSet             *(* get_last_inserted)    (GdaProvider *provider, GdaConnection *cnc,
                                               GError **error);

  /* Padding for future expansion */
  gpointer padding[12];
};


const gchar        *gda_provider_get_name              (GdaProvider *provider);
const gchar        *gda_provider_get_version           (GdaProvider *provider);
const gchar        *gda_provider_get_server_version    (GdaProvider *provider, GdaConnection *cnc);
gboolean            gda_provider_supports_feature      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaConnectionFeature feature);
GdaSqlParser       *gda_provider_create_parser         (GdaProvider *provider, GdaConnection *cnc); /* may be NULL */
GdaConnection      *gda_provider_create_connection     (GdaProvider *provider);
GdaDataHandler     *gda_provider_get_data_handler      (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                                        GType g_type, const gchar *dbms_type);
const gchar        *gda_provider_get_def_dbms_type     (GdaProvider *provider, GdaConnection *cnc,
                                                        GType g_type); /* may be NULL */
gboolean            gda_provider_supports_operation    (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaServerOperationType type, GdaSet *options);
GdaServerOperation *gda_provider_create_operation      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaServerOperationType type, GdaSet *options,
                                                        GError **error);
gchar              *gda_provider_render_operation      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaServerOperation *op, GError **error);
gchar              *gda_provider_statement_to_sql      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GdaSet *params,
                                                        GdaStatementSqlFlag flags,
                                                        GSList **params_used, GError **error);
gchar              *gda_provider_identifier_quote      (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                                        const gchar *id,
                                                        gboolean for_meta_store, gboolean force_quotes);
GdaSqlStatement    *gda_provider_statement_rewrite     (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GdaSet *params, GError **error);
gboolean            gda_provider_open_connection       (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaQuarkList *params, GdaQuarkList *auth);
gboolean            gda_provider_prepare_connection          (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaQuarkList *params, GdaQuarkList *auth);
gboolean            gda_provider_close_connection      (GdaProvider *provider, GdaConnection *cnc);
gchar              *gda_provider_escape_string         (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *str); /* may be NULL */
gchar              *gda_provider_unescape_string       (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *str); /* may be NULL */
gboolean            gda_provider_perform_operation     (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                                        GdaServerOperation *op, GError **error);
gboolean            gda_provider_begin_transaction     (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GdaTransactionIsolation level,
                                                        GError **error);
gboolean            gda_provider_commit_transaction    (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
gboolean            gda_provider_rollback_transaction  (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
gboolean            gda_provider_add_savepoint         (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
gboolean            gda_provider_rollback_savepoint    (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
gboolean            gda_provider_delete_savepoint      (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
gboolean            gda_provider_statement_prepare     (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GError **error);
GObject            *gda_provider_statement_execute     (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GdaSet *params,
                                                        GdaStatementModelUsage model_usage,
                                                        GType *col_types, GdaSet **last_inserted_row,
                                                        GError **error);
GdaSet             *gda_provider_get_last_inserted     (GdaProvider *provider, GdaConnection *cnc,
                                                        GError **error);

G_END_DECLS

#endif
