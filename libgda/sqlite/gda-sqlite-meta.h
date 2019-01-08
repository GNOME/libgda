/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_SQLITE_META_H__
#define __GDA_SQLITE_META_H__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

void     _gda_sqlite_provider_meta_init    (GdaServerProvider *provider);

/* _information_schema_catalog_name */
gboolean _gda_sqlite_meta__info            (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);

/* _builtin_data_types */
gboolean _gda_sqlite_meta__btypes          (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);

/* _udt */
gboolean _gda_sqlite_meta__udt             (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_udt              (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *udt_catalog, const GValue *udt_schema);

/* _udt_columns */
gboolean _gda_sqlite_meta__udt_cols        (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_udt_cols         (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name);

/* _enums */
gboolean _gda_sqlite_meta__enums           (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_enums            (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *udt_catalog, const GValue *udt_schema, const GValue *udt_name);

/* _domains */
gboolean _gda_sqlite_meta__domains         (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_domains          (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *domain_catalog, const GValue *domain_schema);

/* _domain_constraints */
gboolean _gda_sqlite_meta__constraints_dom (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_constraints_dom  (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *domain_catalog, const GValue *domain_schema, 
					    const GValue *domain_name);

/* _element_types */
gboolean _gda_sqlite_meta__el_types        (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_el_types         (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *specific_name);

/* _collations */
gboolean _gda_sqlite_meta__collations      (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_collations       (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *collation_catalog, const GValue *collation_schema, 
					    const GValue *collation_name_n);

/* _character_sets */
gboolean _gda_sqlite_meta__character_sets  (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_character_sets   (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *chset_catalog, const GValue *chset_schema, 
					    const GValue *chset_name_n);

/* _schemata */
gboolean _gda_sqlite_meta__schemata        (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_schemata         (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error, 
					    const GValue *catalog_name, const GValue *schema_name_n);

/* _tables or _views */
gboolean _gda_sqlite_meta__tables_views    (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_tables_views     (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, 
					    const GValue *table_name_n);

/* _columns */
gboolean _gda_sqlite_meta__columns         (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_columns          (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, 
					    const GValue *table_name);

/* _view_column_usage */
gboolean _gda_sqlite_meta__view_cols       (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_view_cols        (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *view_catalog, const GValue *view_schema, 
					    const GValue *view_name);

/* _table_constraints */
gboolean _gda_sqlite_meta__constraints_tab (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_constraints_tab  (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error, 
					    const GValue *table_catalog, const GValue *table_schema, 
					    const GValue *table_name, const GValue *constraint_name_n);

/* _referential_constraints */
gboolean _gda_sqlite_meta__constraints_ref (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_constraints_ref  (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
					    const GValue *constraint_name);

/* _key_column_usage */
gboolean _gda_sqlite_meta__key_columns     (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_key_columns      (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, 
					    const GValue *table_name, const GValue *constraint_name);

/* _check_column_usage */
gboolean _gda_sqlite_meta__check_columns   (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_check_columns    (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, 
					    const GValue *table_name, const GValue *constraint_name);

/* _triggers */
gboolean _gda_sqlite_meta__triggers        (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_triggers         (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, 
					    const GValue *table_name);

/* _routines */
gboolean _gda_sqlite_meta__routines       (GdaServerProvider *prov, GdaConnection *cnc, 
					   GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_routines        (GdaServerProvider *prov, GdaConnection *cnc, 
					   GdaMetaStore *store, GdaMetaContext *context, GError **error,
					   const GValue *routine_catalog, const GValue *routine_schema, 
					   const GValue *routine_name_n);

/* _routine_columns */
gboolean _gda_sqlite_meta__routine_col     (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_routine_col      (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *rout_catalog, const GValue *rout_schema, 
					    const GValue *rout_name, const GValue *col_name, const GValue *ordinal_position);

/* _parameters */
gboolean _gda_sqlite_meta__routine_par     (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_routine_par      (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *rout_catalog, const GValue *rout_schema, 
					    const GValue *rout_name);

/* _table_indexes */
gboolean _gda_sqlite_meta__indexes_tab     (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_indexes_tab      (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
					    const GValue *index_name_n);

/* _index_column_usage */
gboolean _gda_sqlite_meta__index_cols      (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_sqlite_meta_index_cols       (GdaServerProvider *prov, GdaConnection *cnc, 
					    GdaMetaStore *store, GdaMetaContext *context, GError **error,
					    const GValue *table_catalog, const GValue *table_schema,
					    const GValue *table_name, const GValue *index_name);

G_END_DECLS

#endif

