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
 */
#define G_LOG_DOMAIN "GDA-provider-meta"

#include <libgda/gda-provider-meta.h>
#include <libgda/gda-connection.h>
#include <glib/gi18n-lib.h>

/* module error */
GQuark gda_provider_meta_error_quark (void)
{
  static GQuark quark;
  if (!quark)
          quark = g_quark_from_static_string ("gda_provider_meta_error");
  return quark;
}

enum {
  PROP_CONNECTION = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_INTERFACE(GdaProviderMeta, gda_provider_meta, G_TYPE_OBJECT)

static void
gda_provider_meta_default_init (GdaProviderMetaInterface *iface) {

  obj_properties[PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "Connection to database to get metadata from",
                         GDA_TYPE_CONNECTION,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_interface_install_property (iface,
                                     obj_properties[PROP_CONNECTION]);
}

/**
 * gda_provider_meta_execute_query:
 * @prov: a #GdaProviderMeta
 * @sql: a string with the SQL to execute on provider
 * @params: (nullable): a #GdaSet with all paramaters to use in query
 * @error: place to store errors or %NULL
 *
 * SQL is specific for current provider.
 *
 * Returns: (transfer full) (nullable): a new #GdaDataModel with as a result of the query
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel  *gda_provider_meta_execute_query (GdaProviderMeta *prov,
                                                const gchar *sql,
                                                GdaSet *params,
                                                GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  g_return_val_if_fail (sql, NULL);

  GdaConnection *cnc = NULL;
  GdaStatement *stmt;
  g_object_get (G_OBJECT (prov), "connection", &cnc, NULL);
  if (cnc == NULL) {
    g_set_error (error, GDA_PROVIDER_META_ERROR, GDA_PROVIDER_META_NO_CONNECTION_ERROR,
                 _("No connection is set"));
    return NULL;
  }
  if (!gda_connection_is_opened (cnc)) {
    g_set_error (error, GDA_PROVIDER_META_ERROR, GDA_PROVIDER_META_NO_CONNECTION_ERROR,
                 _("Connection is no opened"));
    return NULL;
  }
  stmt = gda_connection_parse_sql_string (cnc, sql, NULL, error);
  if (stmt == NULL) {
    return NULL;
  }
  if (!gda_connection_statement_prepare (cnc, stmt, error)) {
    return NULL;
  }
  return gda_connection_statement_execute_select (cnc, stmt, params, error);
}

/**
 * gda_provider_meta_execute_query_row:
 * @prov: a #GdaProviderMeta
 * @sql: a string with the SQL to execute on provider
 * @error: place to store errors or %NULL
 *
 * SQL is specific for current provider.
 *
 * Returns: (transfer full) (nullable): a new #GdaDataModel with as a result of the query
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_execute_query_row (GdaProviderMeta *prov,
                                     const gchar *sql,
                                     GdaSet *params,
                                     GError **error)
{
  GdaDataModel *model = NULL;
  model = gda_provider_meta_execute_query (prov, sql, params, error);
  if (model == NULL) {
    return NULL;
  }
  if (gda_data_model_get_n_rows (model) != 1) {
    g_set_error (error, GDA_PROVIDER_META_ERROR, GDA_PROVIDER_META_NO_CONNECTION_ERROR,
                 _("No rows or more than one row was returned from query"));
    return NULL;
  }
  return gda_row_new_from_data_model (model, 0);
}

/**
 * gda_provider_meta_get_connection:
 * @prov:
 *
 * Returns: (transfer full): a #GdaConnection used by this object.
 * Since: 6.0
 * Stability: Unstable
 */
GdaConnection*
gda_provider_meta_get_connection (GdaProviderMeta *prov) {
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  GdaConnection *cnc = NULL;
  g_object_get (G_OBJECT(prov), "connection", &cnc, NULL);
  return cnc;
}

/* _builtin_data_types */
/**
 * gda_provider_meta_btypes:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_btypes (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->btypes) {
    return iface->btypes (prov, error);
  }
  return NULL;
}

/* _udt */
/**
 * gda_provider_meta_udts:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_udts (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udts) {
    return iface->udts (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_udt:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_udt (GdaProviderMeta *prov,
                      const gchar *udt_catalog, const gchar *udt_schema,
                      GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt) {
    return iface->udt (prov, udt_catalog, udt_schema, error);
  }
  return NULL;
}

/* _udt_columns */
/**
 * gda_provider_meta_udt_cols:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_udt_cols (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt_cols) {
    return iface->udt_cols (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_udt_col:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_udt_col (GdaProviderMeta *prov,
                           const gchar *udt_catalog,
                           const gchar *udt_schema,
                           const gchar *udt_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt_col) {
    return iface->udt_col (prov, udt_catalog, udt_schema, udt_name, error);
  }
  return NULL;
}

/* _enums */
/**
 * gda_provider_meta_enums_type:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_enums_type (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->enums_type) {
    return iface->enums_type (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_enum_type:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_enum_type (GdaProviderMeta *prov,
                              const gchar *udt_catalog,
                              const gchar *udt_schema,
                              const gchar *udt_name,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->enum_type) {
    return iface->enum_type (prov, udt_catalog, udt_schema, udt_name, error);
  }
  return NULL;
}

/* _domains */
/**
 * gda_provider_meta_domains:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_domains (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domains) {
    return iface->domains (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_domain:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_domain (GdaProviderMeta *prov,
                                const gchar *domain_catalog,
                                const gchar *domain_schema,
                                GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domain) {
    return iface->domain (prov, domain_catalog, domain_schema, error);
  }
  return NULL;
}

/* _domain_constraints */
/**
 * gda_provider_meta_domains_constraints:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_domains_constraints (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domains_constraints) {
    return iface->domains_constraints (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_domain_constraints:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_domain_constraints (GdaProviderMeta *prov,
                                      const gchar *domain_catalog,
                                      const gchar *domain_schema,
                                      const gchar *domain_name,
                                      GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domain_constraint) {
    return iface->domain_constraints (prov, domain_catalog, domain_schema, domain_name, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_domain_constraint:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_domain_constraint (GdaProviderMeta *prov,
                                    const gchar *domain_catalog,
                                    const gchar *domain_schema,
                                    const gchar *domain_name,
                                    const gchar *constraint_name,
                                    GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domain_constraint) {
    return iface->domain_constraint (prov, domain_catalog, domain_schema,
                                     domain_name, constraint_name, error);
  }
  return NULL;
}

/* _element_types */
/**
 * gda_provider_meta_element_types:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_element_types (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->element_types) {
    return iface->element_types (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_element_type:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_element_type (GdaProviderMeta *prov,
                                 const gchar *specific_name,
                                 GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->element_type) {
    return iface->element_type (prov, specific_name, error);
  }
  return NULL;
}

/* _collations */
/**
 * gda_provider_meta_collations:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_collations (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->collations) {
    return iface->collations (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_collation:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_collation  (GdaProviderMeta *prov,
                              const gchar *collation_catalog,
                              const gchar *collation_schema,
                              const gchar *collation_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->collation) {
    return iface->collation (prov, collation_catalog,
                                   collation_schema, collation_name_n, error);
  }
  return NULL;
}

/* _character_sets */
/**
 * gda_provider_meta_character_sets:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_character_sets (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->character_sets) {
    return iface->character_sets (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_character_set:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_character_set (GdaProviderMeta *prov,
                                       const gchar *chset_catalog,
                                       const gchar *chset_schema,
                                       const gchar *chset_name_n,
                                       GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->character_set) {
    return iface->character_set (prov, chset_catalog,
                                       chset_schema, chset_name_n, error);
  }
  return NULL;
}

/* _schemata */
/**
 * gda_provider_meta_schematas:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_schematas (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->schematas) {
    return iface->schematas (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_schemata:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_schemata (GdaProviderMeta *prov,
                           const gchar *catalog_name,
                           const gchar *schema_name_n,
                           GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->schemata) {
    return iface->schemata (prov, catalog_name, schema_name_n, error);
  }
  return NULL;
}

/* _tables or _views */
/**
 * gda_provider_meta_tables:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_tables (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->tables) {
    return iface->tables (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_table:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_table (GdaProviderMeta *prov,
                                     const gchar *table_catalog,
                                     const gchar *table_schema,
                                     const gchar *table_name_n,
                                     GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->table) {
    return iface->table (prov, table_catalog,
                              table_schema, table_name_n, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_views:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_views (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->views) {
    return iface->views (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_view:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_view (GdaProviderMeta *prov,
                                     const gchar *view_catalog,
                                     const gchar *view_schema,
                                     const gchar *view_name_n,
                                     GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->view) {
    return iface->view (prov, view_catalog,
                              view_schema, view_name_n, error);
  }
  return NULL;
}

/* _columns */
/**
 * gda_provider_meta_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_columns (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->columns) {
    return iface->columns (prov, error);
  }
  return NULL;
}

/* _tables_column_usage */
/**
 * gda_provider_meta_tables_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_tables_columns (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->views_columns) {
    return iface->views_columns (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_table_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_table_columns (GdaProviderMeta *prov,
                           const gchar *table_catalog,
                           const gchar *table_schema,
                           const gchar *table_name,
                           GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->table_columns) {
    return iface->table_columns (prov, table_catalog, table_schema,
                                 table_name, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_table_column:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_table_column (GdaProviderMeta *prov,
                          const gchar *table_catalog,
                          const gchar *table_schema,
                          const gchar *table_name,
                          const gchar *column_name,
                          GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->table_column) {
    return iface->table_column (prov, table_catalog,
                                table_schema, table_name,
                                column_name, error);
  }
  return NULL;
}

/* _view_column_usage */
/**
 * gda_provider_meta_views_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_views_columns (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->views_columns) {
    return iface->views_columns (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_view_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_view_columns (GdaProviderMeta *prov,
                            const gchar *view_catalog,
                            const gchar *view_schema,
                            const gchar *view_name,
                            GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->view_columns) {
    return iface->view_columns (prov, view_catalog,
                             view_schema, view_name, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_view_column:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_view_column (GdaProviderMeta *prov,
                            const gchar *view_catalog,
                            const gchar *view_schema,
                            const gchar *view_name,
                            const gchar *column_name,
                            GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->view_column) {
    return iface->view_column (prov, view_catalog,
                            view_schema, view_name, column_name, error);
  }
  return NULL;
}


/* _table_constraints */
/**
 * gda_provider_meta_constraints_tables:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_constraints_tables (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_tables) {
    return iface->constraints_tables (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_constraints_table:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_constraints_table (GdaProviderMeta *prov,
                                     const gchar *table_catalog,
                                     const gchar *table_schema,
                                     const gchar *table_name,
                                     GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_table) {
    return iface->constraints_table (prov, table_catalog,
                                     table_schema,
                                     table_name, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_constraint_table:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_constraint_table (GdaProviderMeta *prov,
                                    const gchar *table_catalog,
                                    const gchar *table_schema,
                                    const gchar *table_name,
                                    const gchar *constraint_name_n,
                                    GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraint_table) {
    return iface->constraint_table (prov, table_catalog,
                                   table_schema,
                                   table_name,
                                   constraint_name_n,
                                   error);
  }
  return NULL;
}

/* _referential_constraints */
/**
 * gda_provider_meta_constraints_ref:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_constraints_ref (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_ref) {
    return iface->constraints_ref (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_constraints_ref_table:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_constraints_ref_table (GdaProviderMeta *prov,
                                  const gchar *table_catalog,
                                  const gchar *table_schema,
                                  const gchar *table_name,
                                  GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_ref_table) {
    return iface->constraints_ref_table (prov, table_catalog, table_schema,
                                         table_name, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_constraint_ref:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_constraint_ref (GdaProviderMeta *prov,
                                  const gchar *table_catalog,
                                  const gchar *table_schema,
                                  const gchar *table_name,
                                  const gchar *constraint_name,
                                  GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraint_ref) {
    return iface->constraint_ref (prov, table_catalog, table_schema,
                                 table_name, constraint_name,
                                 error);
  }
  return NULL;
}

/* _key_column_usage */
/**
 * gda_provider_meta_key_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_key_columns (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->key_columns) {
    return iface->key_columns (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_key_column:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_key_column (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->key_column) {
    return iface->key_column (prov, table_catalog, table_schema,
                                         table_name,
                                         constraint_name,
                                         error);
  }
  return NULL;
}

/* _check_column_usage */
/**
 * gda_provider_meta_check_columns:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_check_columns (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->check_columns) {
    return iface->check_columns (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_check_column:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_check_column (GdaProviderMeta *prov,
                                const gchar *table_catalog,
                                const gchar *table_schema,
                                const gchar *table_name,
                                const gchar *constraint_name,
                                GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->check_column) {
    return iface->check_column (prov, table_catalog,
                                           table_schema,
                                           table_name,
                                           constraint_name,
                                           error);
  }
  return NULL;
}

/* _triggers */
/**
 * gda_provider_meta_triggers:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_triggers (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->triggers) {
    return iface->triggers (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_trigger:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_trigger (GdaProviderMeta *prov,
                                 const gchar *table_catalog,
                                 const gchar *table_schema,
                                 const gchar *table_name,
                                 GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->trigger) {
    return iface->trigger (prov, table_catalog, table_schema,
                                      table_name, error);
  }
  return NULL;
}

/* _routines */
/**
 * gda_provider_meta_routines:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_routines (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routines) {
    return iface->routines (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_routine:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_routine (GdaProviderMeta *prov,
                                 const gchar *routine_catalog,
                                 const gchar *routine_schema,
                                 const gchar *routine_name_n,
                                 GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine) {
    return iface->routine (prov, routine_catalog, routine_schema,
                                      routine_name_n, error);
  }
  return NULL;
}

/* _routine_columns */
/**
 * gda_provider_meta_routines_col:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_routines_col (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routines_col) {
    return iface->routines_col (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_routine_col:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_routine_col (GdaProviderMeta *prov,
                                    const gchar *rout_catalog,
                                    const gchar *rout_schema,
                                    const gchar *rout_name,
                                    GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine_col) {
    return iface->routine_col (prov, rout_catalog, rout_schema,
                                         rout_name, error);
  }
  return NULL;
}

/* _parameters */
/**
 * gda_provider_meta_routines_pars:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_routines_pars (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routines_pars) {
    return iface->routines_pars (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_routine_pars:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_routine_pars (GdaProviderMeta *prov,
                                    const gchar *rout_catalog,
                                    const gchar *rout_schema,
                                    const gchar *rout_name,
                                    GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine_pars) {
    return iface->routine_pars (prov,rout_catalog, rout_schema,
                                         rout_name, error);
  }
  return NULL;
}
/* _table_indexes */
/**
 * gda_provider_meta_indexes_tables:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_indexes_tables (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->indexes_tables) {
    return iface->indexes_tables (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_indexes_table:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_indexes_table (GdaProviderMeta *prov,
                                const gchar *table_catalog,
                                const gchar *table_schema,
                                const gchar *table_name,
                                GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->indexes_table) {

    return iface->indexes_table (prov, table_catalog, table_schema,
                                 table_name, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_index_table:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_index_table (GdaProviderMeta *prov,
                                    const gchar *table_catalog,
                                    const gchar *table_schema,
                                    const gchar *table_name,
                                    const gchar *index_name_n,
                                    GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->index_table) {

    return iface->index_table (prov, table_catalog, table_schema,
                               table_name, index_name_n,
                               error);
  }
  return NULL;
}

/* _index_column_usage */
/**
 * gda_provider_meta_index_cols:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaDataModel*
gda_provider_meta_index_cols (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->index_cols) {
    return iface->index_cols (prov, error);
  }
  return NULL;
}
/**
 * gda_provider_meta_index_col:
 *
 * Returns: (transfer full):
 * Since: 6.0
 * Stability: Unstable
 */
GdaRow*
gda_provider_meta_index_col (GdaProviderMeta *prov,
                                   const gchar *table_catalog,
                                   const gchar *table_schema,
                                   const gchar *table_name,
                                   const gchar *index_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->index_col) {
    return iface->index_col (prov, table_catalog, table_schema,
                                        table_name, index_name, error);
  }
  return NULL;
}


