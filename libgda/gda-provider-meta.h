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

#ifndef __GDA_PROVIDER_META_H__
#define __GDA_PROVIDER_META_H__

G_BEGIN_DECLS

#define GDA_TYPE_PROVIDER_META gda_provider_meta_get_type()

G_DECLARE_INTERFACE(GdaProviderMeta, gda_provider_meta, GDA, PROVIDER_META, GObject)

struct _GdaProviderMetaInterface
{
  GTypeInterface g_iface;

  gboolean (*info)             (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);

  /* _builtin_data_types */
  gboolean (*btypes)           (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);

  /* _udt */
  gboolean (*udt)              (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*udt_full)         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *udt_catalog, const GValue *udt_schema, GError **error);

  /* _udt_columns */
  gboolean (*udt_cols)         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*udt_cols_full)    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name, GError **error);

  /* _enums */
  gboolean (*enums)            (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*enums_full)       (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name, GError **error);

  /* _domains */
  gboolean (*domains)          (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*domains_full)     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *domain_catalog, const GValue *domain_schema, GError **error);

  /* _domain_constraints */
  gboolean (*constraints_dom)  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*constraints_dom_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *domain_catalog, const GValue *domain_schema, const GValue *domain_name, GError **error);

  /* _element_types */
  gboolean (*el_types)         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*el_types_full)    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *specific_name, GError **error);

  /* _collations */
  gboolean (*collations)       (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*collations_full)  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *collation_catalog, const GValue *collation_schema,
                                const GValue *collation_name_n, GError **error);

  /* _character_sets */
  gboolean (*character_sets)   (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*character_sets_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *chset_catalog, const GValue *chset_schema, const GValue *chset_name_n, GError **error);

  /* _schemata */
  gboolean (*schemata)         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*schemata_full)    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *catalog_name, const GValue *schema_name_n, GError **error);

  /* _tables or _views */
  gboolean (*tables_views)     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*tables_views_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n, GError **error);

  /* _columns */
  gboolean (*columns)          (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*columns_full)     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, GError **error);

  /* _view_column_usage */
  gboolean (*view_cols)        (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*view_cols_full)   (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *view_catalog, const GValue *view_schema, const GValue *view_name, GError **error);

  /* _table_constraints */
  gboolean (*constraints_tab)  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*constraints_tab_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                                const GValue *constraint_name_n, GError **error);

  /* _referential_constraints */
  gboolean (*constraints_ref)  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*constraints_ref_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                                const GValue *constraint_name, GError **error);

  /* _key_column_usage */
  gboolean (*key_columns)      (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*key_columns_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                                const GValue *constraint_name, GError **error);

  /* _check_column_usage */
  gboolean (*check_columns)    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*check_columns_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                                const GValue *constraint_name, GError **error);

  /* _triggers */
  gboolean (*triggers)         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*triggers_full)    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, GError **error);

  /* _routines */
  gboolean (*routines)         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*routines_full)    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *routine_catalog, const GValue *routine_schema,
                                const GValue *routine_name_n, GError **error);

  /* _routine_columns */
  gboolean (*routine_col)      (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*routine_col_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *rout_catalog, const GValue *rout_schema, const GValue *rout_name, GError **error);

  /* _parameters */
  gboolean (*routine_par)      (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*routine_par_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *rout_catalog, const GValue *rout_schema, const GValue *rout_name, GError **error);
  /* _table_indexes */
  gboolean (*indexes_tab)      (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*indexes_tab_full) (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                                const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                                const GValue *index_name_n, GError **error);

  /* _index_column_usage */
  gboolean (*index_cols)       (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
  gboolean (*index_cols_full)  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                               const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                               const GValue *index_name, GError **error);

  /* Padding for future expansion */
  gpointer padding[12];
};

gboolean gda_provider_meta_info              (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);

/* _builtin_data_types */
gboolean gda_provider_meta_btypes           (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);

/* _udt */
gboolean gda_provider_meta_udt              (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_udt_full         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *udt_catalog, const GValue *udt_schema, GError **error);

/* _udt_columns */
gboolean gda_provider_meta_udt_cols         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_udt_cols_full    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name, GError **error);

/* _enums */
gboolean gda_provider_meta_enums            (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_enums_full       (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name, GError **error);

/* _domains */
gboolean gda_provider_meta_domains          (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_domains_full     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *domain_catalog, const GValue *domain_schema, GError **error);

/* _domain_constraints */
gboolean gda_provider_meta_constraints_dom  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_constraints_dom_full  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *domain_catalog, const GValue *domain_schema, const GValue *domain_name, GError **error);

/* _element_types */
gboolean gda_provider_meta_el_types         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_el_types_full    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *specific_name, GError **error);

/* _collations */
gboolean gda_provider_meta_collations       (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_collations_full  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *collation_catalog, const GValue *collation_schema,
                              const GValue *collation_name_n, GError **error);

/* _character_sets */
gboolean gda_provider_meta_character_sets   (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_character_sets_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *chset_catalog, const GValue *chset_schema, const GValue *chset_name_n, GError **error);

/* _schemata */
gboolean gda_provider_meta_schemata         (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_schemata_full    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *catalog_name, const GValue *schema_name_n, GError **error);

/* _tables or _views */
gboolean gda_provider_meta_tables_views     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_tables_views_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n, GError **error);

/* _columns */
gboolean gda_provider_meta_columns          (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_columns_full     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, GError **error);

/* _view_column_usage */
gboolean gda_provider_meta_view_cols        (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_view_cols_full   (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *view_catalog, const GValue *view_schema, const GValue *view_name, GError **error);

/* _table_constraints */
gboolean gda_provider_meta_constraints_tab  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_constraints_tab_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                              const GValue *constraint_name_n, GError **error);

/* _referential_constraints */
gboolean gda_provider_meta_constraints_ref  (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_constraints_ref_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                              const GValue *constraint_name, GError **error);

/* _key_column_usage */
gboolean gda_provider_meta_key_columns      (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_key_columns_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                              const GValue *constraint_name, GError **error);

/* _check_column_usage */
gboolean gda_provider_meta_check_columns    (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_check_columns_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                              const GValue *constraint_name, GError **error);

/* _triggers */
gboolean gda_provider_meta_triggers        (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_triggers_full   (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, GError **error);

/* _routines */
gboolean gda_provider_meta_routines        (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_routines_full   (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *routine_catalog, const GValue *routine_schema,
                              const GValue *routine_name_n, GError **error);

/* _routine_columns */
gboolean gda_provider_meta_routine_col     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_routine_col_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *rout_catalog, const GValue *rout_schema, const GValue *rout_name, GError **error);

/* _parameters */
gboolean gda_provider_meta_routine_par     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_routine_par_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *rout_catalog, const GValue *rout_schema, const GValue *rout_name, GError **error);
/* _table_indexes */
gboolean gda_provider_meta_indexes_tab     (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_indexes_tab_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                              const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                              const GValue *index_name_n, GError **error);

/* _index_column_usage */
gboolean gda_provider_meta_index_cols      (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx, GError **error);
gboolean gda_provider_meta_index_cols_full (GdaProviderMeta *prov, GdaConnection *cnc, GdaMetaStore *meta, GdaMetaContext *ctx,
                             const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
                             const GValue *index_name, GError **error);

G_END_DECLS

#endif
