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
#define G_LOG_DOMAIN "GDA-provider"

#include <libgda/gda-provider.h>

G_DEFINE_INTERFACE(GdaProvider, gda_provider, G_TYPE_OBJECT)

static void
gda_provider_default_init (GdaProviderInterface *iface) {}

/**
 * gda_provider_get_name:
 *
 * Returns: (transfer none):
 * Since: 6.0
 * Stability: Unstable
 */
const gchar*
gda_provider_get_name (GdaProvider *provider) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->get_name) {
    return iface->get_name (provider);
  }
  return NULL;
}
/**
 * gda_provider_get_version:
 *
 * Returns: (transfer none):
 * Since: 6.0
 * Stability: Unstable
 */
const gchar*
gda_provider_get_version (GdaProvider *provider) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->get_version) {
    return iface->get_version (provider);
  }
  return NULL;
}
/**
 * gda_provider_get_server_version:
 *
 * Returns: (transfer none):
 * Since: 6.0
 * Stability: Unstable
 */
const gchar*
gda_provider_get_server_version (GdaProvider *provider, GdaConnection *cnc) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->get_server_version) {
    return iface->get_server_version (provider, cnc);
  }
  return NULL;
}
/**
 * gda_provider_supports_feature:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_supports_feature (GdaProvider *provider, GdaConnection *cnc,
                               GdaConnectionFeature feature) {
  g_return_val_if_fail (provider, TRUE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->supports_feature) {
    return iface->supports_feature (provider, cnc, feature);
  }
  return TRUE;
}
/**
 * gda_provider_create_parser:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaSqlParser*
gda_provider_create_parser (GdaProvider *provider, GdaConnection *cnc) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->create_parser) {
    return iface->create_parser (provider, cnc);
  }
  return NULL;
} /* may be NULL */
/**
 * gda_provider_create_connection:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaConnection*
gda_provider_create_connection (GdaProvider *provider) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->create_connection) {
    return iface->create_connection (provider);
  }
  return NULL;
}
/**
 * gda_provider_get_data_handler:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataHandler*
gda_provider_get_data_handler (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                    GType g_type, const gchar *dbms_type) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->get_data_handler) {
    return iface->get_data_handler (provider, cnc, g_type, dbms_type);
  }
  return NULL;
}
/**
 * gda_provider_get_def_dbms_type:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
const gchar*
gda_provider_get_def_dbms_type (GdaProvider *provider, GdaConnection *cnc,
                                    GType g_type) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->get_def_dbms_type) {
    return iface->get_def_dbms_type (provider, cnc, g_type);
  }
  return NULL;
} /* may be NULL */
/**
 * gda_provider_supports_operation:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_supports_operation (GdaProvider *provider, GdaConnection *cnc,
                                    GdaServerOperationType type, GdaSet *options) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->supports_operation) {
    return iface->supports_operation (provider, cnc, type, options);
  }
  return FALSE;
}
/**
 * gda_provider_create_operation:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaServerOperation*
gda_provider_create_operation (GdaProvider *provider, GdaConnection *cnc,
                                    GdaServerOperationType type, GdaSet *options,
                                    GError **error) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->create_operation) {
    return iface->create_operation (provider, cnc, type, options, error);
  }
  return NULL;
}
/**
 * gda_provider_render_operation:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
gchar*
gda_provider_render_operation (GdaProvider *provider, GdaConnection *cnc,
                                    GdaServerOperation *op, GError **error) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->render_operation) {
    return iface->render_operation (provider, cnc, op, error);
  }
  return NULL;
}
/**
 * gda_provider_statement_to_sql:
 * @provider:
 * @cnc:
 * @stmt:
 * @params: (nullable):
 * @flags:
 * @params_used: (nullable) (element-type Gda.Holder) (out) (transfer container):
 * @error:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
gchar*
gda_provider_statement_to_sql (GdaProvider *provider, GdaConnection *cnc,
                               GdaStatement *stmt, GdaSet *params,
                               GdaStatementSqlFlag flags,
                               GSList **params_used, GError **error) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->statement_to_sql) {
    return iface->statement_to_sql (provider, cnc, stmt, params, flags, params_used, error);
  }
  return NULL;
}
/**
 * gda_provider_identifier_quote:
 * @provider:
 * @cnc: (nullable):
 * @id:
 * @for_meta_store:
 * @force_quotes:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
gchar*
gda_provider_identifier_quote (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                               const gchar *id,
                               gboolean for_meta_store, gboolean force_quotes) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->identifier_quote) {
    return iface->identifier_quote (provider, cnc, id, for_meta_store, force_quotes);
  }
  return NULL;
}
/**
 * gda_provider_statement_rewrite:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaSqlStatement*
gda_provider_statement_rewrite (GdaProvider *provider, GdaConnection *cnc,
                                GdaStatement *stmt, GdaSet *params, GError **error) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->statement_rewrite) {
    return iface->statement_rewrite (provider, cnc, stmt, params, error);
  }
  return NULL;
}
/**
 * gda_provider_open_connection:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_open_connection (GdaProvider *provider, GdaConnection *cnc,
                              GdaQuarkList *params, GdaQuarkList *auth) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->open_connection) {
    return iface->open_connection (provider, cnc, params, auth);
  }
  return FALSE;
}
/**
 * gda_provider_prepare_connection:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_prepare_connection (GdaProvider *provider, GdaConnection *cnc,
                                 GdaQuarkList *params, GdaQuarkList *auth) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->prepare_connection) {
    return iface->prepare_connection (provider, cnc, params, auth);
  }
  return FALSE;
}
/**
 * gda_provider_close_connection:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_close_connection (GdaProvider *provider, GdaConnection *cnc) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->close_connection) {
    return iface->close_connection (provider, cnc);
  }
  return FALSE;
}
/**
 * gda_provider_escape_string:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
gchar*
gda_provider_escape_string (GdaProvider *provider, GdaConnection *cnc,
                            const gchar *str) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->escape_string) {
    return iface->escape_string (provider, cnc, str);
  }
  return NULL;
} /* may be NULL */
/**
 * gda_provider_unescape_string:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
gchar*
gda_provider_unescape_string (GdaProvider *provider, GdaConnection *cnc,
                              const gchar *str) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->unescape_string) {
    return iface->unescape_string (provider, cnc, str);
  }
  return NULL;
} /* may be NULL */
/**
 * gda_provider_perform_operation:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_perform_operation (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                GdaServerOperation *op, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->perform_operation) {
    return iface->perform_operation (provider, cnc, op, error);
  }
  return FALSE;
}
/**
 * gda_provider_begin_transaction:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_begin_transaction (GdaProvider *provider, GdaConnection *cnc,
                                const gchar *name, GdaTransactionIsolation level,
                                GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->begin_transaction) {
    return iface->begin_transaction (provider, cnc, name, level, error);
  }
  return FALSE;
}
/**
 * gda_provider_commit_transaction:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_commit_transaction (GdaProvider *provider, GdaConnection *cnc,
                                 const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->commit_transaction) {
    return iface->commit_transaction (provider, cnc, name, error);
  }
  return FALSE;
}
/**
 * gda_provider_rollback_transaction:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_rollback_transaction (GdaProvider *provider, GdaConnection *cnc,
                                   const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->rollback_transaction) {
    return iface->rollback_transaction (provider, cnc, name, error);
  }
  return FALSE;
}
/**
 * gda_provider_add_savepoint:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_add_savepoint (GdaProvider *provider, GdaConnection *cnc,
                            const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->add_savepoint) {
    return iface->add_savepoint (provider, cnc, name, error);
  }
  return FALSE;
}
/**
 * gda_provider_rollback_savepoint:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_rollback_savepoint (GdaProvider *provider, GdaConnection *cnc,
                                 const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->rollback_savepoint) {
    return iface->rollback_savepoint (provider, cnc, name, error);
  }
  return FALSE;
}
/**
 * gda_provider_delete_savepoint:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_delete_savepoint (GdaProvider *provider, GdaConnection *cnc,
                               const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->delete_savepoint) {
    return iface->delete_savepoint (provider, cnc, name, error);
  }
  return FALSE;
}
/**
 * gda_provider_statement_prepare:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_statement_prepare (GdaProvider *provider, GdaConnection *cnc,
                                GdaStatement *stmt, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->statement_prepare) {
    return iface->statement_prepare (provider, cnc, stmt, error);
  }
  return FALSE;
}
/**
 * gda_provider_statement_execute:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GObject*
gda_provider_statement_execute (GdaProvider *provider, GdaConnection *cnc,
                                GdaStatement *stmt, GdaSet *params,
                                GdaStatementModelUsage model_usage,
                                GType *col_types, GdaSet **last_inserted_row,
                                GError **error) {
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->statement_execute) {
    return iface->statement_execute (provider, cnc, stmt, params, model_usage, col_types, last_inserted_row, error);
  }
  return NULL;
}

/**
 * gda_provider_get_last_inserted:
 * @provider: a #GdaProvider object
 * @cnc: a #GdaConnection to get last inserted from
 * @error: a place to put errors
 *
 * This command should be called inmediately called after a INSERT SQL command
 *
 * Return: (transfer full): a #GdaSet with all data of the last inserted row
 */
GdaSet*
gda_provider_get_last_inserted (GdaProvider *provider,
                                GdaConnection *cnc,
                                GError **error)
{
  g_return_val_if_fail (provider, NULL);
  GdaProviderInterface *iface = GDA_PROVIDER_GET_IFACE (provider);
  if (iface->get_last_inserted) {
    return iface->get_last_inserted (provider, cnc, error);
  }
  return NULL;
}
