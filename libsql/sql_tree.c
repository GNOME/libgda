#include <stdio.h>
#include <stdlib.h>

#include "mem.h"
#include "sql_parser.h"
#include "sql_tree.h"

sql_statement *
sql_statement_build (sql_statement_type type, void *statement)
{
	sql_statement *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->type = type;
	retval->statement = statement;
	retval->full_query = NULL;

	return retval;
}

sql_select_statement *
sql_select_statement_build (int distinct, GList * fields, GList * from,
			    sql_where * where, GList * order,
			    sql_ordertype ordertype, GList * group)
{
	sql_select_statement *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->distinct = distinct;
	retval->fields = fields;
	retval->from = from;
	retval->where = where;
	retval->order = order;
	retval->group = group;
	retval->ordertype = ordertype;

	return retval;
}

sql_insert_statement *
sql_insert_statement_build (sql_table * table, GList * fields, GList * values)
{
	sql_insert_statement *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->table = table;
	retval->values = values;
	retval->fields = fields;

	return retval;
}

sql_update_statement *
sql_update_statement_build (sql_table * table, GList * set, sql_where * where)
{
	sql_update_statement *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->table = table;
	retval->set = set;
	retval->where = where;

	return retval;
}

sql_delete_statement *
sql_delete_statement_build (sql_table * table, sql_where * where)
{
	sql_delete_statement *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->table = table;
	retval->where = where;

	return retval;
}

sql_field_item *
sql_field_build_function (gchar * funcname, GList * funcarglist)
{
	sql_field_item *item;

	item = memsql_alloc (sizeof *item);
	item->type = SQL_function;
	item->d.function.funcname = funcname;
	item->d.function.funcarglist = funcarglist;

	return item;

}

sql_field_item *
sql_field_item_build (GList * name)
{
	sql_field_item *item;

	item = memsql_alloc (sizeof *item);
	item->type = SQL_name;
	item->d.name = name;

	return item;
}

sql_field_item *
sql_field_item_build_equation (sql_field_item * left, sql_field_item * right,
			       sql_field_operator op)
{
	sql_field_item *item;

	item = memsql_alloc (sizeof *item);
	item->type = SQL_equation;
	item->d.equation.left = left;
	item->d.equation.right = right;
	item->d.equation.op = op;

	return item;
}

sql_field_item *
sql_field_item_build_select (sql_select_statement * select)
{
	sql_field_item *item;

	item = memsql_alloc (sizeof *item);
	item->type = SQL_inlineselect;
	item->d.select = select;

	return item;
}

sql_field *
sql_field_build (sql_field_item * item)
{
	sql_field *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->as = NULL;
	retval->item = item;
	retval->param_spec = NULL;

	return retval;
}

sql_field *
sql_field_set_as (sql_field * field, char *as)
{
	field->as = as;
	return field;
}

sql_field *
sql_field_set_param_spec (sql_field * field, GList * param_spec)
{
	field->param_spec = param_spec;
	return field;
}

param_spec *
param_spec_build (param_spec_type type, char *content)
{
	param_spec *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->type = type;
	retval->content = content;

	return retval;
}

sql_table *
sql_table_build (char *tablename)
{
	sql_table *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->type = SQL_simple;

	retval->d.simple = memsql_strdup (tablename);

	retval->as = NULL;

	return retval;
}

sql_table *
sql_table_build_join (sql_table * left, sql_table * right,
		      sql_condition * cond)
{
	sql_table *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->type = SQL_join;
	retval->d.join.left = left;
	retval->d.join.right = right;
	retval->d.join.cond = cond;
	retval->as = NULL;

	return retval;
}

sql_table *
sql_table_build_select (sql_select_statement * select)
{
	sql_table *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->type = SQL_nestedselect, retval->d.select = select;
	retval->as = NULL;

	return retval;
}

sql_table *
sql_table_set_as (sql_table * table, char *as)
{
	table->as = as;
	return table;
}

sql_where *
sql_where_build_single (sql_condition * cond)
{
	sql_where *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->type = SQL_single;
	retval->d.single = cond;

	return retval;
}

sql_where *
sql_where_build_negated (sql_where * where)
{
	sql_where *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->type = SQL_negated;
	retval->d.negated = where;

	return retval;
}

sql_where *
sql_where_build_pair (sql_where * left, sql_where * right,
		      sql_logic_operator op)
{
	sql_where *retval;

	retval = memsql_alloc (sizeof *retval);
	retval->type = SQL_pair;
	retval->d.pair.left = left;
	retval->d.pair.right = right;
	retval->d.pair.op = op;

	return retval;
}

sql_condition *
sql_build_condition (sql_field * left, sql_field * right,
		     sql_condition_operator op)
{
	sql_condition *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->op = op;
	retval->d.pair.left = left;
	retval->d.pair.right = right;

	return retval;
}

sql_condition *
sql_build_condition_between (sql_field * field, sql_field * lower,
			     sql_field * upper)
{
	sql_condition *retval;

	retval = memsql_alloc (sizeof *retval);

	retval->op = SQL_between;

	retval->d.between.field = field;
	retval->d.between.lower = lower;
	retval->d.between.upper = upper;

	return retval;
}
