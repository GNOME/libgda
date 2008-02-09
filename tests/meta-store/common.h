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

#endif
