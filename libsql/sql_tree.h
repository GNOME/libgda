#ifndef SQL_TREE_H
#define SQL_TREE_H

#include "sql_parser.h"

/* statements */
sql_statement        *sql_statement_build        (sql_statement_type type, void *statement);
sql_select_statement *sql_select_statement_build (int distinct,
						  GList * fields,
						  GList * from,
						  sql_where * where,
						  GList * order,
						  GList * group);
sql_insert_statement *sql_insert_statement_build (sql_table * table,
						  GList * fields,
						  GList * values);
sql_update_statement *sql_update_statement_build (sql_table * table,
						  GList * set,
						  sql_where * where);
sql_delete_statement *sql_delete_statement_build (sql_table * table,
						  sql_where * where);

/* field item */
sql_field_item *sql_field_item_build (GList * name);
sql_field_item *sql_field_item_build_equation (sql_field_item * left,
					       sql_field_item * right,
					       sql_field_operator op);
sql_field_item *sql_field_item_build_select (sql_select_statement * select);
sql_field_item *sql_field_build_function (gchar * funcname,
					  GList * funcarglist);

/* field */
sql_field *sql_field_build (sql_field_item * item);
sql_field *sql_field_set_as (sql_field * field, char *as);
sql_field *sql_field_set_param_spec (sql_field * field, GList * param_spec);

/* table */
sql_table *sql_table_build (char *tablename);
sql_table *sql_table_build_function (gchar * funcname, GList * funcarglist);
sql_table *sql_table_set_join (sql_table * table, sql_join_type join_type, sql_where * cond);
sql_table *sql_table_build_select (sql_select_statement * select);
sql_table *sql_table_set_as (sql_table * table, char *as);

/* where */
sql_where *sql_where_build_single (sql_condition * cond);
sql_where *sql_where_build_negated (sql_where * where);
sql_where *sql_where_build_pair (sql_where * left, sql_where * right,
				 sql_logic_operator op);
/* condition */
sql_condition *sql_build_condition (sql_field * left, sql_field * right,
				    sql_condition_operator op);
sql_condition *sql_build_condition_between (sql_field * field,
					    sql_field * lower,
					    sql_field * upper);
sql_condition *sql_condition_negate (sql_condition *cond);
/* field order */
sql_order_field *sql_order_field_build (GList * name, sql_ordertype order_type);


/* param spec */
param_spec *param_spec_build (param_spec_type type, char *content);

#endif /* SQL_TREE_H */
