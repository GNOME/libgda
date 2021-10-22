/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <libgda/libgda.h>
#include <string.h>

#ifndef _COMMON_H_
#define _COMMON_H_

void     common_drop_all_tables (GdaMetaStore *store);
void     common_declare_meta_store (GdaMetaStore *store);

void     test_information_schema_catalog_name (GdaMetaStore *store);

void     test_schemata_1 (GdaMetaStore *store);
void     test_schemata_2 (GdaMetaStore *store);

void     test_builtin_data_types (GdaMetaStore *store);
void     test_domains (GdaMetaStore *store);
void     test_tables (GdaMetaStore *store);
void     test_views (GdaMetaStore *store);
void     test_routines (GdaMetaStore *store);
void     test_triggers (GdaMetaStore *store);
void     test_columns (GdaMetaStore *store);
void     test_table_constraints (GdaMetaStore *store);
void     test_referential_constraints (GdaMetaStore *store);
void     test_check_constraints (GdaMetaStore *store);
void     test_key_column_usage (GdaMetaStore *store);
void     test_view_column_usage (GdaMetaStore *store);
void     test_domain_constraints (GdaMetaStore *store);
void     test_parameters (GdaMetaStore *store);

void     tests_group_1 (GdaMetaStore *store);
void     tests_group_2 (GdaMetaStore *store);

gboolean test_setup (const gchar *prov_id, const gchar *dbcreate_str);
gboolean test_finish (GdaConnection *cnc);

#endif
