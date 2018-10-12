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

#ifndef __GDA_PROVIDER_META_H__
#define __GDA_PROVIDER_META_H__

#include <glib-object.h>
#include <libgda/gda-set.h>
#include <libgda/gda-row.h>

G_BEGIN_DECLS

typedef enum {
  GDA_PROVIDER_META_NO_CONNECTION_ERROR,
  GDA_PROVIDER_META_QUERY_ERROR
} GdaProviderMetaError;

extern GQuark gda_provider_meta_error_quark (void);
#define GDA_PROVIDER_META_ERROR gda_provider_meta_error_quark ()

#define GDA_TYPE_PROVIDER_META gda_provider_meta_get_type()

G_DECLARE_INTERFACE(GdaProviderMeta, gda_provider_meta, GDA, PROVIDER_META, GObject)

struct _GdaProviderMetaInterface
{
  GTypeInterface g_iface;

  /* _builtin_data_types */
  GdaDataModel *(*btypes)               (GdaProviderMeta *prov, GError **error);

  /* _udt */
  GdaDataModel *(*udts)                 (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*udt)                  (GdaProviderMeta *prov,
                                         const gchar *udt_catalog,
                                         const gchar *udt_schema,
                                         GError **error);

  /* _udt_columns */
  GdaDataModel *(*udt_cols)             (GdaProviderMeta *prov, GError **error);
    GdaRow       *(*udt_col)            (GdaProviderMeta *prov,
                                         const gchar *udt_catalog,
                                         const gchar *udt_schema,
                                         const gchar *udt_name,
                                         GError **error);

  /* _enums */
  GdaDataModel *(*enums_type)           (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*enum_type)            (GdaProviderMeta *prov,
                                         const gchar *udt_catalog,
                                         const gchar *udt_schema,
                                         const gchar *udt_name,
                                         GError **error);

  /* _domains */
  GdaDataModel *(*domains)              (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*domain)               (GdaProviderMeta *prov,
                                         const gchar *domain_catalog,
                                         const gchar *domain_schema,
                                         GError **error);

  /* _domain_constraints */
  GdaDataModel *(*domains_constraints)  (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*domain_constraints)   (GdaProviderMeta *prov,
                                         const gchar *domain_catalog,
                                         const gchar *domain_schema,
                                         const gchar *domain_name,
                                         GError **error);
  GdaRow       *(*domain_constraint)    (GdaProviderMeta *prov,
                                         const gchar *domain_catalog,
                                         const gchar *domain_schema,
                                         const gchar *domain_name,
                                         const gchar *constraint_name,
                                         GError **error);

  /* _element_types */
  GdaDataModel *(*element_types)        (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*element_type)         (GdaProviderMeta *prov,
                                         const gchar *specific_name, GError **error);

  /* _collations */
  GdaDataModel *(*collations)           (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*collation)            (GdaProviderMeta *prov,
                                         const gchar *collation_catalog,
                                         const gchar *collation_schema,
                                         const gchar *collation_name_n,
                                         GError **error);

  /* _character_sets */
  GdaDataModel *(*character_sets)       (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*character_set)        (GdaProviderMeta *prov,
                                         const gchar *chset_catalog, const gchar *chset_schema, const gchar *chset_name_n, GError **error);

  /* _schemata */
  GdaDataModel *(*schematas)            (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*schemata)             (GdaProviderMeta *prov,
                                         const gchar *catalog_name,
                                         const gchar *schema_name_n,
                                         GError **error);

  /* _tables or _views */
  GdaDataModel *(*tables_columns)           (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*tables)         (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*table)           (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name_n, GError **error);
  GdaDataModel *(*views)         (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*view)           (GdaProviderMeta *prov,
                                         const gchar *view_catalog,
                                         const gchar *view_schema,
                                         const gchar *view_name_n, GError **error);

  /* _columns */
  GdaDataModel *(*columns)              (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*table_columns)        (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         GError **error);
  GdaRow       *(*table_column)         (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         const gchar *column_name,
                                         GError **error);

  /* _view_column_usage */
  GdaDataModel *(*views_columns)           (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*view_columns)            (GdaProviderMeta *prov,
                                         const gchar *view_catalog,
                                         const gchar *view_schema,
                                         const gchar *view_name, GError **error);
  GdaRow       *(*view_column)             (GdaProviderMeta *prov,
                                         const gchar *view_catalog,
                                         const gchar *view_schema,
                                         const gchar *view_name,
                                         const gchar *column_name,
                                         GError **error);

  /* _table_constraints */
  GdaDataModel *(*constraints_tables)   (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*constraints_table)    (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema, const gchar *table_name,
                                         GError **error);
  GdaRow       *(*constraint_table)     (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema, const gchar *table_name,
                                         const gchar *constraint_name_n, GError **error);

  /* _referential_constraints */
  GdaDataModel *(*constraints_ref)      (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*constraints_ref_table) (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         GError **error);
  GdaRow       *(*constraint_ref)       (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         const gchar *constraint_name,
                                         GError **error);

  /* _key_column_usage */
  GdaDataModel *(*key_columns)          (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*key_column)           (GdaProviderMeta *prov,
                                         const gchar *table_catalog, const gchar *table_schema,
                                         const gchar *table_name,
                                         const gchar *constraint_name, GError **error);

  /* _check_column_usage */
  GdaDataModel *(*check_columns)        (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*check_column)         (GdaProviderMeta *prov,
                                         const gchar *table_catalog, const gchar *table_schema,
                                         const gchar *table_name,
                                         const gchar *constraint_name, GError **error);

  /* _triggers */
  GdaDataModel *(*triggers)             (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*trigger)              (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         GError **error);

  /* _routines */
  GdaDataModel *(*routines)             (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*routine)              (GdaProviderMeta *prov,
                                         const gchar *routine_catalog,
                                         const gchar *routine_schema,
                                         const gchar *routine_name_n,
                                         GError **error);

  /* _routine_columns */
  GdaDataModel *(*routines_col)         (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*routine_col)          (GdaProviderMeta *prov,
                                         const gchar *rout_catalog,
                                         const gchar *rout_schema,
                                         const gchar *rout_name, GError **error);

  /* _parameters */
  GdaDataModel *(*routines_pars)        (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*routine_pars)         (GdaProviderMeta *prov,
                                         const gchar *rout_catalog,
                                         const gchar *rout_schema,
                                         const gchar *rout_name, GError **error);
  /* _table_indexes */
  GdaDataModel *(*indexes_tables)       (GdaProviderMeta *prov, GError **error);
  GdaDataModel *(*indexes_table)        (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name, GError **error);
  GdaRow       *(*index_table)          (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         const gchar *index_name_n, GError **error);

  /* _index_column_usage */
  GdaDataModel *(*index_cols)           (GdaProviderMeta *prov, GError **error);
  GdaRow       *(*index_col)            (GdaProviderMeta *prov,
                                         const gchar *table_catalog,
                                         const gchar *table_schema,
                                         const gchar *table_name,
                                         const gchar *index_name, GError **error);

  /* Padding for future expansion */
  gpointer padding[12];
};

GdaDataModel  *gda_provider_meta_execute_query        (GdaProviderMeta *prov,
                                                       const gchar *sql,
                                                       GdaSet *params,
                                                       GError **error);

GdaRow        *gda_provider_meta_execute_query_row    (GdaProviderMeta *prov,
                                                       const gchar *sql,
                                                       GdaSet *params,
                                                       GError **error);

GdaConnection *gda_provider_meta_get_connection       (GdaProviderMeta *prov);

/* _builtin_data_types */
GdaDataModel *gda_provider_meta_btypes                (GdaProviderMeta *prov,
                              GError **error);

/* _udt */
GdaDataModel *gda_provider_meta_udts                  (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_udt                   (GdaProviderMeta *prov,
                              const gchar *udt_catalog, const gchar *udt_schema,
                              GError **error);

/* _udt_columns */
GdaDataModel *gda_provider_meta_udt_cols              (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_udt_col               (GdaProviderMeta *prov,
                              const gchar *udt_catalog, const gchar *udt_schema,
                              const gchar *udt_name, GError **error);

/* _enums */
GdaDataModel *gda_provider_meta_enums_type            (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_enum_type             (GdaProviderMeta *prov,
                              const gchar *udt_catalog, const gchar *udt_schema, const gchar *udt_name, GError **error);

/* _domains */
GdaDataModel *gda_provider_meta_domains               (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_domain                (GdaProviderMeta *prov,
                              const gchar *domain_catalog, const gchar *domain_schema, GError **error);

/* _domain_constraints */
GdaDataModel *gda_provider_meta_domains_constraints   (GdaProviderMeta *prov,
                              GError **error);
GdaDataModel *gda_provider_meta_domain_constraints    (GdaProviderMeta *prov,
                              const gchar *domain_catalog, const gchar *domain_schema,
                              const gchar *domain_name, GError **error);
GdaRow       *gda_provider_meta_domain_constraint     (GdaProviderMeta *prov,
                              const gchar *domain_catalog, const gchar *domain_schema,
                              const gchar *domain_name, const gchar *contraint_name,
                              GError **error);

/* _element_types */
GdaDataModel *gda_provider_meta_element_types         (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_element_type          (GdaProviderMeta *prov,
                              const gchar *specific_name, GError **error);

/* _collations */
GdaDataModel *gda_provider_meta_collations            (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_collation             (GdaProviderMeta *prov,
                              const gchar *collation_catalog, const gchar *collation_schema,
                              const gchar *collation_name_n, GError **error);

/* _character_sets */
GdaDataModel *gda_provider_meta_character_sets        (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_character_set         (GdaProviderMeta *prov,
                              const gchar *chset_catalog, const gchar *chset_schema,
                              const gchar *chset_name_n, GError **error);

/* _schemata */
GdaDataModel *gda_provider_meta_schematas             (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_schemata              (GdaProviderMeta *prov,
                              const gchar *catalog_name, const gchar *schema_name_n, GError **error);

/* _tables or _views */
GdaDataModel *gda_provider_meta_tables          (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_table            (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name_n, GError **error);
GdaDataModel *gda_provider_meta_views          (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_view            (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name_n, GError **error);

/* _columns */
GdaDataModel *gda_provider_meta_columns               (GdaProviderMeta *prov,
                              GError **error);
GdaDataModel *gda_provider_meta_tables_columns            (GdaProviderMeta *prov,
                              GError **error);
GdaDataModel *gda_provider_meta_table_columns         (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name, GError **error);
GdaRow       *gda_provider_meta_table_column          (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *column_name, GError **error);

/* _view_column_usage */
GdaDataModel *gda_provider_meta_views_columns            (GdaProviderMeta *prov,
                              GError **error);
GdaDataModel *gda_provider_meta_view_columns             (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name, GError **error);
GdaRow       *gda_provider_meta_view_column   (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name,
                              const gchar *column_name,
                              GError **error);

/* _table_constraints */
GdaDataModel *gda_provider_meta_constraints_tables    (GdaProviderMeta *prov, GError **error);
GdaDataModel *gda_provider_meta_constraints_table     (GdaProviderMeta *prov,
                             const gchar *table_catalog, const gchar *table_schema,
                             const gchar *table_name,
                             GError **error);
GdaRow       *gda_provider_meta_constraint_table      (GdaProviderMeta *prov,
                             const gchar *table_catalog, const gchar *table_schema,
                             const gchar *table_name,
                             const gchar *constraint_name_n, GError **error);

/* _referential_constraints */
GdaDataModel *gda_provider_meta_constraints_ref       (GdaProviderMeta *prov,
                             GError **error);
GdaDataModel *gda_provider_meta_constraints_ref_table (GdaProviderMeta *prov,
                             const gchar *table_catalog,
                             const gchar *table_schema, const gchar *table_name,
                              GError **error);
GdaRow       *gda_provider_meta_constraint_ref  (GdaProviderMeta *prov,
                             const gchar *table_catalog,
                             const gchar *table_schema, const gchar *table_name,
                             const gchar *constraint_name, GError **error);

/* _key_column_usage */
GdaDataModel *gda_provider_meta_key_columns           (GdaProviderMeta *prov,
                            GError **error);
GdaRow       *gda_provider_meta_key_column            (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name, GError **error);

/* _check_column_usage */
GdaDataModel *gda_provider_meta_check_columns         (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_check_column          (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name, GError **error);

/* _triggers */
GdaDataModel *gda_provider_meta_triggers              (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_trigger               (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name, GError **error);

/* _routines */
GdaDataModel *gda_provider_meta_routines              (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_routine               (GdaProviderMeta *prov,
                              const gchar *routine_catalog, const gchar *routine_schema,
                              const gchar *routine_name_n, GError **error);

/* _routine_columns */
GdaDataModel *gda_provider_meta_routines_col          (GdaProviderMeta *prov, GError **error);
GdaRow       *gda_provider_meta_routine_col           (GdaProviderMeta *prov,
                              const gchar *rout_catalog, const gchar *rout_schema,
                              const gchar *rout_name, GError **error);

/* _parameters */
GdaDataModel *gda_provider_meta_routines_pars         (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_routine_pars          (GdaProviderMeta *prov,
                              const gchar *rout_catalog, const gchar *rout_schema,
                              const gchar *rout_name, GError **error);
/* _table_indexes */
GdaDataModel *gda_provider_meta_indexes_tables        (GdaProviderMeta *prov,
                              GError **error);
GdaDataModel *gda_provider_meta_indexes_table         (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name, GError **error);
GdaRow       *gda_provider_meta_index_table (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *index_name_n, GError **error);

/* _index_column_usage */
GdaDataModel *gda_provider_meta_index_cols            (GdaProviderMeta *prov,
                              GError **error);
GdaRow       *gda_provider_meta_index_col             (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *index_name, GError **error);

G_END_DECLS

#endif
