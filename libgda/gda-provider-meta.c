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
#include <libgda/gda-provider-meta.h>

G_DEFINE_INTERFACE(GdaProviderMeta, gda_provider_meta, G_TYPE_OBJECT)

static void
gda_provider_meta_default_init (GdaProviderMetaInterface *iface) {}


/**
 * gda_provider_meta_info:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_info (GdaProviderMeta *prov, GdaConnection *cnc,
                        GdaMetaStore *meta, GdaMetaContext *ctx, GError **error) {
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->info) {
    return iface->info (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}

/* _builtin_data_types */
/**
 * gda_provider_meta_btypes:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_btypes (GdaProviderMeta *prov, GdaConnection *cnc,
                          GdaMetaStore *meta, GdaMetaContext *ctx,
                          GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->btypes) {
    return iface->btypes (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}

/* _udt */
/**
 * gda_provider_meta_udt:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_udt (GdaProviderMeta *prov, GdaConnection *cnc,
                       GdaMetaStore *meta, GdaMetaContext *ctx, GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt) {
    return iface->udt (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_udt_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_udt_full (GdaProviderMeta *prov, GdaConnection *cnc,
                            GdaMetaStore *meta, GdaMetaContext *ctx,
                            const GValue *udt_catalog, const GValue *udt_schema,
                            GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt_full) {
    return iface->udt_full (prov, cnc, meta, ctx, udt_catalog, udt_schema, error);
  }
  return FALSE;
}

/* _udt_columns */
/**
 * gda_provider_meta_udt_cols:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_udt_cols (GdaProviderMeta *prov, GdaConnection *cnc,
                            GdaMetaStore *meta, GdaMetaContext *ctx,
                            GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt_cols) {
    return iface->udt_cols (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_udt_cols_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_udt_cols_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                 GdaMetaStore *meta, GdaMetaContext *ctx,
                                 const GValue *udt_catalog,
                                 const GValue *udt_schema,
                                 const GValue *udt_name, GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->udt_cols_full) {
    return iface->udt_cols_full (prov, cnc, meta, ctx, udt_catalog, udt_schema, udt_name, error);
  }
  return FALSE;
}

/* _enums */
/**
 * gda_provider_meta_enums:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_enums (GdaProviderMeta *prov, GdaConnection *cnc,
                         GdaMetaStore *meta, GdaMetaContext *ctx,
                         GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->enums) {
    return iface->enums (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_enums_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_enums_full (GdaProviderMeta *prov,
                              GdaConnection *cnc, GdaMetaStore *meta,
                              GdaMetaContext *ctx,
                              const GValue *udt_catalog,
                              const GValue *udt_schema,
                              const GValue *udt_name,
                              GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->enums_full) {
    return iface->enums_full (prov, cnc, meta, ctx, udt_catalog, udt_schema, udt_name, error);
  }
  return FALSE;
}

/* _domains */
/**
 * gda_provider_meta_domains:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_domains (GdaProviderMeta *prov, GdaConnection *cnc,
                           GdaMetaStore *meta, GdaMetaContext *ctx,
                           GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domains) {
    return iface->domains (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_domains_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_domains_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *domain_catalog,
                                const GValue *domain_schema,
                                GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->domains_full) {
    return iface->domains_full (prov, cnc, meta, ctx, domain_catalog, domain_schema, error);
  }
  return FALSE;
}

/* _domain_constraints */
/**
 * gda_provider_meta_constraints_dom:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_constraints_dom (GdaProviderMeta *prov, GdaConnection *cnc,
                                   GdaMetaStore *meta, GdaMetaContext *ctx,
                                   GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_dom) {
    return iface->constraints_dom (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_constraints_dom_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_constraints_dom_full (GdaProviderMeta *prov,
                                        GdaConnection *cnc, GdaMetaStore *meta,
                                        GdaMetaContext *ctx,
                                        const GValue *domain_catalog,
                                        const GValue *domain_schema,
                                        const GValue *domain_name,
                                        GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_dom_full) {
    return iface->constraints_dom_full (prov, cnc, meta, ctx, domain_catalog, domain_schema, domain_name, error);
  }
  return FALSE;
}

/* _element_types */
/**
 * gda_provider_meta_el_types:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_el_types (GdaProviderMeta *prov, GdaConnection *cnc,
                            GdaMetaStore *meta, GdaMetaContext *ctx,
                            GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->el_types) {
    return iface->el_types (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_el_types_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_el_types_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                 GdaMetaStore *meta, GdaMetaContext *ctx,
                                 const GValue *specific_name, GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->el_types_full) {
    return iface->el_types_full (prov, cnc, meta, ctx, specific_name, error);
  }
  return FALSE;
}

/* _collations */
/**
 * gda_provider_meta_collations:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_collations (GdaProviderMeta *prov, GdaConnection *cnc,
                              GdaMetaStore *meta, GdaMetaContext *ctx,
                              GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->collations) {
    return iface->collations (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_collations_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_collations_full  (GdaProviderMeta *prov,
                                    GdaConnection *cnc, GdaMetaStore *meta,
                                    GdaMetaContext *ctx,
                                    const GValue *collation_catalog,
                                    const GValue *collation_schema,
                                    const GValue *collation_name_n, GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->collations_full) {
    return iface->collations_full (prov, cnc, meta, ctx, collation_catalog,
                                   collation_schema, collation_name_n, error);
  }
  return FALSE;
}

/* _character_sets */
/**
 * gda_provider_meta_character_sets:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_character_sets (GdaProviderMeta *prov,
                                  GdaConnection *cnc, GdaMetaStore *meta,
                                  GdaMetaContext *ctx, GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->character_sets) {
    return iface->character_sets (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_character_sets_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_character_sets_full (GdaProviderMeta *prov,
                                       GdaConnection *cnc,
                                       GdaMetaStore *meta,
                                       GdaMetaContext *ctx,
                                       const GValue *chset_catalog,
                                       const GValue *chset_schema,
                                       const GValue *chset_name_n,
                                       GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->character_sets_full) {
    return iface->character_sets_full (prov, cnc, meta, ctx, chset_catalog,
                                       chset_schema, chset_name_n, error);
  }
  return FALSE;
}

/* _schemata */
/**
 * gda_provider_meta_schemata:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_schemata (GdaProviderMeta *prov, GdaConnection *cnc,
                            GdaMetaStore *meta, GdaMetaContext *ctx,
                            GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->schemata) {
    return iface->schemata (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_schemata_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_schemata_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                 GdaMetaStore *meta, GdaMetaContext *ctx,
                                 const GValue *catalog_name,
                                 const GValue *schema_name_n,
                                 GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->schemata_full) {
    return iface->schemata_full (prov, cnc, meta, ctx, catalog_name, schema_name_n, error);
  }
  return FALSE;
}

/* _tables or _views */
/**
 * gda_provider_meta_tables_views:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_tables_views (GdaProviderMeta *prov, GdaConnection *cnc,
                                GdaMetaStore *meta, GdaMetaContext *ctx,
                                GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->tables_views) {
    return iface->tables_views (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_tables_views_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_tables_views_full (GdaProviderMeta *prov,
                                     GdaConnection *cnc, GdaMetaStore *meta,
                                     GdaMetaContext *ctx,
                                     const GValue *table_catalog,
                                     const GValue *table_schema,
                                     const GValue *table_name_n,
                                     GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->tables_views_full) {
    return iface->tables_views_full (prov, cnc, meta, ctx, table_catalog,
                                          table_schema, table_name_n, error);
  }
  return FALSE;
}

/* _columns */
/**
 * gda_provider_meta_columns:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_columns (GdaProviderMeta *prov, GdaConnection *cnc,
                           GdaMetaStore *meta, GdaMetaContext *ctx,
                           GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->columns) {
    return iface->columns (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_columns_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_columns_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog,
                                const GValue *table_schema,
                                const GValue *table_name,
                                GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->columns_full) {
    return iface->columns_full (prov, cnc, meta, ctx, table_catalog,
                                     table_schema, table_name, error);
  }
  return FALSE;
}

/* _view_column_usage */
/**
 * gda_provider_meta_view_cols:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_view_cols (GdaProviderMeta *prov, GdaConnection *cnc,
                             GdaMetaStore *meta, GdaMetaContext *ctx,
                             GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->view_cols) {
    return iface->view_cols (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_view_cols_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_view_cols_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                  GdaMetaStore *meta, GdaMetaContext *ctx,
                                  const GValue *view_catalog,
                                  const GValue *view_schema,
                                  const GValue *view_name,
                                  GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->view_cols_full) {
    return iface->view_cols_full (prov, cnc, meta, ctx, view_catalog,
                                       view_schema, view_name, error);
  }
  return FALSE;
}

/* _table_constraints */
/**
 * gda_provider_meta_constraints_tab:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_constraints_tab (GdaProviderMeta *prov, GdaConnection *cnc,
                                   GdaMetaStore *meta,
                                   GdaMetaContext *ctx,
                                   GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_tab) {
    return iface->constraints_tab (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_constraints_tab_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_constraints_tab_full (GdaProviderMeta *prov,
                                        GdaConnection *cnc,
                                        GdaMetaStore *meta,
                                        GdaMetaContext *ctx,
                                        const GValue *table_catalog,
                                        const GValue *table_schema,
                                        const GValue *table_name,
                                        const GValue *constraint_name_n,
                                        GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_tab_full) {
    return iface->constraints_tab_full (prov, cnc, meta, ctx,
                                       table_catalog,
                                       table_schema,
                                       table_name,
                                       constraint_name_n,
                                       error);
  }
  return FALSE;
}

/* _referential_constraints */
/**
 * gda_provider_meta_constraints_ref:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_constraints_ref (GdaProviderMeta *prov,
                                   GdaConnection *cnc,
                                   GdaMetaStore *meta,
                                   GdaMetaContext *ctx,
                                   GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_ref) {
    return iface->constraints_ref (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_constraints_ref_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_constraints_ref_full (GdaProviderMeta *prov,
                                        GdaConnection *cnc,
                                        GdaMetaStore *meta,
                                        GdaMetaContext *ctx,
                                        const GValue *table_catalog,
                                        const GValue *table_schema,
                                        const GValue *table_name,
                                        const GValue *constraint_name,
                                        GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->constraints_ref_full) {
    return iface->constraints_ref_full (prov, cnc, meta, ctx,
                                             table_catalog, table_schema,
                                             table_name, constraint_name,
                                             error);
  }
  return FALSE;
}

/* _key_column_usage */
/**
 * gda_provider_meta_key_columns:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_key_columns (GdaProviderMeta *prov, GdaConnection *cnc,
                               GdaMetaStore *meta, GdaMetaContext *ctx,
                               GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->key_columns) {
    return iface->key_columns (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_key_columns_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_key_columns_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                    GdaMetaStore *meta, GdaMetaContext *ctx,
                                    const GValue *table_catalog,
                                    const GValue *table_schema,
                                    const GValue *table_name,
                                    const GValue *constraint_name,
                                    GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->key_columns_full) {
    return iface->key_columns_full (prov, cnc, meta, ctx,
                                         table_catalog, table_schema,
                                         table_name,
                                         constraint_name,
                                         error);
  }
  return FALSE;
}

/* _check_column_usage */
/**
 * gda_provider_meta_check_columns:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_check_columns (GdaProviderMeta *prov,
                                 GdaConnection *cnc, GdaMetaStore *meta,
                                 GdaMetaContext *ctx,
                                 GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->check_columns) {
    return iface->check_columns (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_check_columns_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_check_columns_full (GdaProviderMeta *prov,
                                      GdaConnection *cnc,
                                      GdaMetaStore *meta,
                                      GdaMetaContext *ctx,
                                      const GValue *table_catalog,
                                      const GValue *table_schema,
                                      const GValue *table_name,
                                      const GValue *constraint_name,
                                      GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->check_columns_full) {
    return iface->check_columns_full (prov, cnc, meta, ctx,
                                           table_catalog,
                                           table_schema,
                                           table_name,
                                           constraint_name,
                                           error);
  }
  return FALSE;
}

/* _triggers */
/**
 * gda_provider_meta_triggers:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_triggers (GdaProviderMeta *prov, GdaConnection *cnc,
                            GdaMetaStore *meta, GdaMetaContext *ctx,
                            GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->triggers) {
    return iface->triggers (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_triggers_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_triggers_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                 GdaMetaStore *meta, GdaMetaContext *ctx,
                                 const GValue *table_catalog,
                                 const GValue *table_schema,
                                 const GValue *table_name,
                                 GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->triggers_full) {
    return iface->triggers_full (prov, cnc, meta, ctx,
                                      table_catalog, table_schema,
                                      table_name, error);
  }
  return FALSE;
}

/* _routines */
/**
 * gda_provider_meta_routines:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_routines (GdaProviderMeta *prov, GdaConnection *cnc,
                            GdaMetaStore *meta, GdaMetaContext *ctx,
                            GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routines) {
    return iface->routines (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_routines_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_routines_full (GdaProviderMeta *prov, GdaConnection *cnc,
                                 GdaMetaStore *meta, GdaMetaContext *ctx,
                                 const GValue *routine_catalog,
                                 const GValue *routine_schema,
                                 const GValue *routine_name_n,
                                 GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routines_full) {
    return iface->routines_full (prov, cnc, meta, ctx,
                                      routine_catalog, routine_schema,
                                      routine_name_n, error);
  }
  return FALSE;
}

/* _routine_columns */
/**
 * gda_provider_meta_routine_col:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_routine_col (GdaProviderMeta *prov, GdaConnection *cnc,
                               GdaMetaStore *meta, GdaMetaContext *ctx,
                               GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine_col) {
    return iface->routine_col (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_routine_col_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_routine_col_full (GdaProviderMeta *prov,
                                    GdaConnection *cnc,
                                    GdaMetaStore *meta,
                                    GdaMetaContext *ctx,
                                    const GValue *rout_catalog,
                                    const GValue *rout_schema,
                                    const GValue *rout_name,
                                    GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine_col_full) {
    return iface->routine_col_full (prov, cnc, meta, ctx,
                                         rout_catalog, rout_schema,
                                         rout_name, error);
  }
  return FALSE;
}

/* _parameters */
/**
 * gda_provider_meta_routine_par:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_routine_par (GdaProviderMeta *prov, GdaConnection *cnc,
                               GdaMetaStore *meta, GdaMetaContext *ctx,
                               GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine_par) {
    return iface->routine_par (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_routine_par_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_routine_par_full (GdaProviderMeta *prov,
                                    GdaConnection *cnc,
                                    GdaMetaStore *meta,
                                    GdaMetaContext *ctx,
                                    const GValue *rout_catalog,
                                    const GValue *rout_schema,
                                    const GValue *rout_name,
                                    GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->routine_par_full) {
    return iface->routine_par_full (prov, cnc, meta, ctx,
                                         rout_catalog, rout_schema,
                                         rout_name, error);
  }
  return FALSE;
}
/* _table_indexes */
/**
 * gda_provider_meta_indexes_tab:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_indexes_tab (GdaProviderMeta *prov,
                               GdaConnection *cnc,
                               GdaMetaStore *meta,
                               GdaMetaContext *ctx,
                               GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->indexes_tab) {
    return iface->indexes_tab (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_indexes_tab_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_indexes_tab_full (GdaProviderMeta *prov,
                                    GdaConnection *cnc,
                                    GdaMetaStore *meta,
                                    GdaMetaContext *ctx,
                                    const GValue *table_catalog,
                                    const GValue *table_schema,
                                    const GValue *table_name,
                                    const GValue *index_name_n,
                                    GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->indexes_tab_full) {

    return iface->indexes_tab_full (prov, cnc, meta, ctx,
                                         table_catalog, table_schema,
                                         table_name, index_name_n,
                                         error);
  }
  return FALSE;
}

/* _index_column_usage */
/**
 * gda_provider_meta_index_colsgda_provider_meta_index_cols:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_index_cols (GdaProviderMeta *prov, GdaConnection *cnc,
                              GdaMetaStore *meta,
                              GdaMetaContext *ctx,
                              GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->index_cols) {
    return iface->index_cols (prov, cnc, meta, ctx, error);
  }
  return FALSE;
}
/**
 * gda_provider_meta_index_cols_full:
 *
 * Since: 6.0
 * Stability: Unstable
 */
gboolean
gda_provider_meta_index_cols_full (GdaProviderMeta *prov,
                                   GdaConnection *cnc,
                                   GdaMetaStore *meta,
                                   GdaMetaContext *ctx,
                                   const GValue *table_catalog,
                                   const GValue *table_schema,
                                   const GValue *table_name,
                                   const GValue *index_name, GError **error)
{
  g_return_val_if_fail (prov, FALSE);
  GdaProviderMetaInterface *iface = GDA_PROVIDER_META_GET_IFACE (prov);
  if (iface->index_cols_full) {
    return iface->index_cols_full (prov, cnc, meta, ctx,
                                        table_catalog, table_schema,
                                        table_name, index_name, error);
  }
  return FALSE;
}


