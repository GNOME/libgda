/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <glib.h>
#include <strings.h>
#include <string.h>

#include "sql_parser.h"
#include "sql_tree.h"
#include "mem.h"

#ifndef _
#define _(x) (x)
#endif

extern char *sqltext;
extern void sql_switch_to_buffer (void *buffer);
extern void *sql_scan_string (const char *string);

int sqlerror (char *error);

sql_statement *sql_result;
GError **sql_error;

int sqlparse (void);

static int sql_destroy_select (sql_select_statement * select);
static int sql_destroy_insert (sql_insert_statement * insert);
static char *sql_select_stringify (sql_select_statement * select);
static int sql_destroy_field (sql_field * field);
static int sql_destroy_param_spec (param_spec * pspec);

static char *sql_field_stringify (sql_field * field);

/**
 * sqlerror:
 * 
 * Internal function for displaying error messages used by the lexer parser.
 */
int
sqlerror (char *string)
{
	if (sql_error) {
		if (!strcmp (string, "parse error"))
			g_set_error (sql_error, 0, 0, _("Parse error near `%s'\n"), sqltext);
		if (!strcmp (string, "syntax error"))
			g_set_error (sql_error, 0, 0, _("Syntax error near `%s'\n"), sqltext);
	}
	else
		fprintf (stderr, "SQL Parser error: %s near `%s'\n", string, sqltext);
	return 0;
}

static int
sql_destroy_field_item (sql_field_item * item)
{
	GList *walk;

	if (!item)
		return 0;

	switch (item->type) {
	case SQL_name:
		for (walk = item->d.name; walk != NULL; walk = walk->next)
			memsql_free (walk->data);

		g_list_free (item->d.name);
		break;

	case SQL_equation:
		sql_destroy_field_item (item->d.equation.left);
		sql_destroy_field_item (item->d.equation.right);
		break;

	case SQL_inlineselect:
		sql_destroy_select (item->d.select);
		break;

	case SQL_function:
		memsql_free (item->d.function.funcname);
		for (walk = item->d.function.funcarglist; walk != NULL; walk = walk->next)
			sql_destroy_field (walk->data);

		g_list_free (item->d.function.funcarglist);
	}


	memsql_free (item);
	return 0;
}

static int
sql_destroy_field (sql_field * field)
{
	if (!field)
		return 0;

	sql_destroy_field_item (field->item);
	memsql_free (field->as);
	if (field->param_spec) {
		GList *walk;
		for (walk = field->param_spec; walk != NULL; walk = walk->next)
			sql_destroy_param_spec ((param_spec *) walk->data);
		g_list_free (field->param_spec);
	}
	memsql_free (field);
	return 0;
}


static int
sql_destroy_param_spec (param_spec * pspec)
{
	if (!pspec)
		return 0;

	memsql_free (pspec->content);
	memsql_free (pspec);
	return 0;
}

static int
sql_destroy_condition (sql_condition * cond)
{
	if (!cond)
		return 0;

	switch (cond->op) {
	case SQL_eq:
	case SQL_is:
	case SQL_not:
	case SQL_in:
	case SQL_notin:
	case SQL_like:
	case SQL_gt:
	case SQL_lt:
	case SQL_geq:
	case SQL_leq:
		sql_destroy_field (cond->d.pair.left);
		sql_destroy_field (cond->d.pair.right);
		break;

	case SQL_between:
		sql_destroy_field (cond->d.between.field);
		sql_destroy_field (cond->d.between.lower);
		sql_destroy_field (cond->d.between.upper);
		break;
	}

	memsql_free (cond);
	return 0;
}

static int
sql_destroy_table (sql_table * table)
{
	if (!table)
		return 0;

	switch (table->type) {
	case SQL_simple:
		memsql_free (table->d.simple);
		break;

	case SQL_join:
		sql_destroy_table (table->d.join.left);
		sql_destroy_table (table->d.join.right);
		sql_destroy_condition (table->d.join.cond);
		break;

	case SQL_nestedselect:
		sql_destroy_select (table->d.select);
		break;
	}

	memsql_free (table);
	return 0;
}

static int
sql_destroy_where (sql_where * where)
{
	if (!where)
		return 0;

	switch (where->type) {
	case SQL_single:
		sql_destroy_condition (where->d.single);
		break;

	case SQL_negated:
		sql_destroy_where (where->d.negated);
		break;

	case SQL_pair:
		sql_destroy_where (where->d.pair.left);
		sql_destroy_where (where->d.pair.right);
		break;
	}

	memsql_free (where);
	return 0;
}

static int
sql_destroy_insert (sql_insert_statement * insert)
{
	GList *walk;

	sql_destroy_table (insert->table);

	for (walk = insert->fields; walk != NULL; walk = walk->next)
		sql_destroy_field (walk->data);
	g_list_free (insert->fields);

	for (walk = insert->values; walk != NULL; walk = walk->next)
		sql_destroy_field (walk->data);
	g_list_free (insert->values);

	memsql_free (insert);

	return 0;
}

static int
sql_destroy_select (sql_select_statement * select)
{
	GList *walk;

	for (walk = select->fields; walk != NULL; walk = walk->next)
		sql_destroy_field (walk->data);

	for (walk = select->from; walk != NULL; walk = walk->next)
		sql_destroy_table (walk->data);

	for (walk = select->order; walk != NULL; walk = walk->next)
		sql_destroy_field (walk->data);

	for (walk = select->group; walk != NULL; walk = walk->next)
		sql_destroy_field (walk->data);

	g_list_free (select->fields);
	g_list_free (select->from);
	g_list_free (select->order);
	g_list_free (select->group);

	sql_destroy_where (select->where);

	memsql_free (select);

	return 0;
}

static int
sql_destroy_update (sql_update_statement * update)
{
	GList *walk;

	sql_destroy_table (update->table);

	for (walk = update->set; walk != NULL; walk = walk->next)
		sql_destroy_condition (walk->data);
	g_list_free (update->set);

	sql_destroy_where (update->where);

	memsql_free (update);

	return 0;
}

static int
sql_destroy_delete (sql_delete_statement * delete)
{
	sql_destroy_table (delete->table);
	sql_destroy_where (delete->where);

	memsql_free (delete);

	return 0;
}

/**
 * sql_destroy:
 * @statement: Sql statement generated by sql_parse()
 * 
 * Free up a sql_statement generated by sql_parse().
 */

int
sql_destroy (sql_statement * statement)
{
	if (!statement)
		return 0;

	switch (statement->type) {
	case SQL_select:
		sql_destroy_select (statement->statement);
		break;

	case SQL_insert:
		sql_destroy_insert (statement->statement);
		break;

	case SQL_update:
		sql_destroy_update (statement->statement);
		break;
	case SQL_delete:
		sql_destroy_delete (statement->statement);
		break;

	default:
		fprintf (stderr, "Unknown statement type: %d\n", statement->type);
	}

	memsql_free (statement->full_query);
	memsql_free (statement);
	return 0;
}

/**
 * sql_parse_with_error:
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 * @error: a place where to store an error, or %NULL
 * 
 * Generate in memory a structure of the @sqlquery in an easy
 * to view way.  You can also modify the returned structure and
 * regenerate the sql query using sql_stringify().  The structure
 * contains information on what type of sql statement it is, what
 * tables its getting from, what fields are selected, the where clause,
 * joins etc.
 * 
 * Returns: A generated sql_statement or %NULL on error.
 */
sql_statement *
sql_parse_with_error (char *sqlquery, GError ** error)
{
	if (sqlquery == NULL) {
		if (error)
			g_set_error (error, 0, 0, _("Empty query to parse"));
		else
			fprintf (stderr, "SQL parse error, you can not specify NULL");
		return NULL;

	}

	sql_error = error;
	sql_switch_to_buffer (sql_scan_string (sqlquery));

	if (sqlparse () == 0) {
		sql_result->full_query = memsql_strdup (sqlquery);
		return sql_result;
	}
	else {
		if (!error)
			fprintf (stderr, "Error on SQL statement: %s\n", sqlquery);
		return NULL;
	}
}

/**
 * sql_parse:
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 *
 * Generate in memory a structure of the @sqlquery in an easy
 * to view way.  You can also modify the returned structure and
 * regenerate the sql query using sql_stringify().  The structure
 * contains information on what type of sql statement it is, what
 * tables its getting from, what fields are selected, the where clause,
 * joins etc.
 *
 * Returns: A generated sql_statement or %NULL on error.
 */
sql_statement *
sql_parse (char *sqlquery)
{
	return sql_parse_with_error (sqlquery, NULL);
}

static char *
sql_field_op_stringify (sql_field_operator op)
{
	switch (op) {
	case SQL_plus:
		return memsql_strdup ("+");
	case SQL_minus:
		return memsql_strdup ("-");
	case SQL_times:
		return memsql_strdup ("*");
	case SQL_div:
		return memsql_strdup ("/");
	}

	fprintf (stderr, "Invalid op: %d\n", op);

	return NULL;
}

static char *
sql_field_name_stringify (GList * name)
{
	GList *walk;
	char *result = NULL;

	for (walk = name; walk != NULL; walk = walk->next) {
		result = memsql_strappend_free (result, memsql_strdup (walk->data));
		if (walk->next)
			result = memsql_strappend_free (result, memsql_strdup ("."));

	}

	return result;
}

static char *
sql_field_item_stringify (sql_field_item * item)
{
	char *retval = NULL;
	GList *walk;

	if (!item)
		return NULL;

	switch (item->type) {
	case SQL_name:
		retval = sql_field_name_stringify (item->d.name);
		break;

	case SQL_equation:
		retval =
			memsql_strappend_free (sql_field_item_stringify (item->d.equation.left),
					       sql_field_op_stringify (item->d.equation.op));
		retval =
			memsql_strappend_free (retval,
					       sql_field_item_stringify (item->d.equation.
									 right));
		break;

	case SQL_inlineselect:
		retval =
			memsql_strappend_free (memsql_strdup ("("),
					       sql_select_stringify (item->d.select));
		retval = memsql_strappend_free (retval, memsql_strdup (")"));
		break;

	case SQL_function:
		retval =
			memsql_strappend_free (memsql_strdup (item->d.function.funcname),
					       memsql_strdup ("("));
		for (walk = item->d.function.funcarglist; walk != NULL; walk = walk->next) {
			retval =
				memsql_strappend_free (retval, sql_field_stringify (walk->data));
			if (walk->next)
				retval = memsql_strappend_free (retval, memsql_strdup (", "));
		}
		retval = memsql_strappend_free (retval, memsql_strdup (")"));
		break;

	}

	return retval;
}

static char *
sql_field_stringify (sql_field * field)
{
	char *retval;

	if (!field)
		return NULL;

	retval = sql_field_item_stringify (field->item);

	if (field->as) {
		retval = memsql_strappend_free (retval, memsql_strdup (" as "));
		retval = memsql_strappend_free (retval, memsql_strdup (field->as));
	}

	return retval;
}

static char *
sql_condition_op_stringify (sql_condition_operator op)
{
	switch (op) {
	case SQL_eq:
		return memsql_strdup ("=");
	case SQL_is:
		return memsql_strdup ("is");
	case SQL_not:
		return memsql_strdup ("not");
	case SQL_like:
		return memsql_strdup ("like");
	case SQL_in:
		return memsql_strdup ("in");
	case SQL_notin:
		return memsql_strdup ("not in");
	default:
		fprintf (stderr, "Invalid condition op: %d\n", op);
	}
	return NULL;
}

static char *
sql_condition_stringify (sql_condition * cond)
{
	char *retval;

	if (!cond)
		return NULL;
	switch (cond->op) {
	case SQL_eq:
	case SQL_is:
	case SQL_not:
	case SQL_in:
	case SQL_notin:
	case SQL_like:
		retval =
			memsql_strappend_free (sql_field_stringify (cond->d.pair.left),
					       memsql_strdup (" "));
		retval =
			memsql_strappend_free (retval, sql_condition_op_stringify (cond->op));
		retval = memsql_strappend_free (retval, memsql_strdup (" "));
		retval =
			memsql_strappend_free (retval,
					       sql_field_stringify (cond->d.pair.right));
		break;

	case SQL_between:
		retval =
			memsql_strappend_free (sql_field_stringify (cond->d.between.field),
					       memsql_strdup (" between "));
		retval =
			memsql_strappend_free (retval,
					       sql_field_stringify (cond->d.between.lower));
		retval = memsql_strappend_free (retval, memsql_strdup (" and "));
		retval =
			memsql_strappend_free (retval,
					       sql_field_stringify (cond->d.between.upper));
		break;

	default:
		fprintf (stderr, "Invalid  condition type: %d\n", cond->op);
		retval = NULL;
	}

	return retval;
}

static char *
sql_table_stringify (sql_table * table)
{
	char *retval;

	if (!table)
		return NULL;

	switch (table->type) {
	case SQL_simple:
		retval = memsql_strdup (table->d.simple);
		break;

	case SQL_join:
		retval =
			memsql_strappend_free (sql_table_stringify (table->d.join.left),
					       memsql_strdup (" join "));
		retval =
			memsql_strappend_free (retval,
					       sql_table_stringify (table->d.join.right));
		if (table->d.join.cond) {
			retval = memsql_strappend_free (retval, memsql_strdup (" on "));
			retval =
				memsql_strappend_free (retval,
						       sql_condition_stringify (table->d.join.cond));
		}
		break;

	case SQL_nestedselect:
		retval =
			memsql_strappend_free (memsql_strdup ("("),
					       sql_select_stringify (table->d.select));
		retval = memsql_strappend_free (retval, memsql_strdup (")"));
		break;

	default:
		fprintf (stderr, "Invalid table type: %d\n", table->type);
		retval = NULL;
	}

	return retval;
}

static char *
sql_logic_op_stringify (sql_logic_operator op)
{
	switch (op) {
	case SQL_and:
		return memsql_strdup ("and");
	case SQL_or:
		return memsql_strdup ("or");
	default:
		fprintf (stderr, "invalid logic op: %d", op);
	}
	return NULL;
}

static char *
sql_where_stringify (sql_where * where)
{
	char *retval = NULL;

	if (!where)
		return NULL;

	switch (where->type) {
	case SQL_single:
		retval = sql_condition_stringify (where->d.single);
		break;

	case SQL_negated:
		retval =
			memsql_strappend_free (memsql_strdup ("not "),
					       sql_where_stringify (where->d.negated));
		break;

	case SQL_pair:
		retval =
			memsql_strappend_free (sql_where_stringify (where->d.pair.left),
					       memsql_strdup (" "));
		retval =
			memsql_strappend_free (retval,
					       sql_logic_op_stringify (where->d.pair.op));
		retval = memsql_strappend_free (retval, memsql_strdup (" "));
		retval =
			memsql_strappend_free (retval,
					       sql_where_stringify (where->d.pair.right));
	}

	retval = memsql_strappend_free (memsql_strdup ("("), retval);
	retval = memsql_strappend_free (retval, memsql_strdup (")"));

	return retval;
}

static char *
sql_insert_stringify (sql_insert_statement * insert)
{
	char *result;
	GList *walk;

	result = memsql_strdup ("insert into ");
	result =
		memsql_strappend_free (result, sql_table_stringify (insert->table));

	if (insert->fields) {
		result = memsql_strappend_free (result, memsql_strdup (" ("));
		for (walk = insert->fields; walk != NULL; walk = walk->next) {
			result =
				memsql_strappend_free (result, sql_field_stringify (walk->data));
			if (walk->next)
				result = memsql_strappend_free (result, memsql_strdup (", "));
		}
		result = memsql_strappend_free (result, memsql_strdup (")"));
	}

	result = memsql_strappend_free (result, memsql_strdup (" ("));

	for (walk = insert->values; walk != NULL; walk = walk->next) {
		result = memsql_strappend_free (result, sql_field_stringify (walk->data));
		if (walk->next)
			result = memsql_strappend_free (result, memsql_strdup (", "));
	}

	result = memsql_strappend_free (result, memsql_strdup (")"));

	return result;
}

static char *
sql_select_stringify (sql_select_statement * select)
{
	char *result;
	char *fields;
	char *tables;
	char *where;
	char *order;
	char *group;
	char *temp;
	GList *walk;

	result = memsql_strdup ("select ");

	if (select->distinct)
		result = memsql_strappend_free (result, memsql_strdup ("distinct "));

	fields = NULL;
	for (walk = select->fields; walk != NULL; walk = walk->next) {
		temp = sql_field_stringify (walk->data);
		fields = memsql_strappend_free (fields, temp);

		if (walk->next)
			fields = memsql_strappend_free (fields, memsql_strdup (", "));

	}

	result = memsql_strappend_free (result, fields);
	result = memsql_strappend_free (result, memsql_strdup (" from "));

	tables = NULL;
	for (walk = select->from; walk != NULL; walk = walk->next) {
		temp = sql_table_stringify (walk->data);
		tables = memsql_strappend_free (tables, temp);
		if (walk->next)
			tables = memsql_strappend_free (tables, memsql_strdup (", "));
	}

	result = memsql_strappend_free (result, tables);

	if (select->where)
		where =
			memsql_strappend_free (memsql_strdup (" where "),
					       sql_where_stringify (select->where));
	else
		where = NULL;

	result = memsql_strappend_free (result, where);

	if (select->order) {
		order = memsql_strdup (" order by ");

		for (walk = select->order; walk != NULL; walk = walk->next) {
			order = memsql_strappend_free (order, sql_field_stringify (walk->data));
			if (walk->next)
				order = memsql_strappend_free (order, memsql_strdup (", "));
		}
	}
	else
		order = NULL;
	result = memsql_strappend_free (result, order);

	if (select->group) {
		group = memsql_strdup (" group by ");

		for (walk = select->group; walk != NULL; walk = walk->next) {
			group = memsql_strappend_free (group, sql_field_stringify (walk->data));
			if (walk->next)
				group = memsql_strappend_free (group, memsql_strdup (", "));
		}
	}
	else
		group = NULL;

	result = memsql_strappend_free (result, group);

	return result;
}

static char *
sql_update_stringify (sql_update_statement * update)
{
	char *result;
	GList *walk;

	result =
		memsql_strappend_free (memsql_strdup ("update "),
				       sql_table_stringify (update->table));
	result = memsql_strappend_free (result, memsql_strdup (" set "));

	for (walk = update->set; walk != NULL; walk = walk->next) {
		result =
			memsql_strappend_free (result, sql_condition_stringify (walk->data));
		if (walk->next)
			result = memsql_strappend_free (result, memsql_strdup (", "));
	}

	if (update->where) {
		result = memsql_strappend_free (result, memsql_strdup (" where "));
		result =
			memsql_strappend_free (result, sql_where_stringify (update->where));
	}

	return result;
}

static char *
sql_delete_stringify (sql_delete_statement * delete)
{
	char *result;

	result =
		memsql_strappend_free (memsql_strdup ("delete from "),
				       sql_table_stringify (delete->table));
	if (delete->where) {
		result = memsql_strappend_free (result, memsql_strdup (" where "));
		result =
			memsql_strappend_free (result, sql_where_stringify (delete->where));
	}

	return result;
}

/**
 * sql_stringify:
 * 
 * Covert a sql_statement into a string.  This is very useful for building
 * up sql queries.
 * 
 * Returns: The SQL statement or a %NULL.  Free after use.
 */

char *
sql_stringify (sql_statement * statement)
{
	char *result = NULL;
	char *final;

	if (!statement)
		return NULL;

	switch (statement->type) {
	case SQL_select:
		result = sql_select_stringify (statement->statement);
		break;

	case SQL_insert:
		result = sql_insert_stringify (statement->statement);
		break;

	case SQL_update:
		result = sql_update_stringify (statement->statement);
		break;

	case SQL_delete:
		result = sql_delete_stringify (statement->statement);
		break;

	default:
		fprintf (stderr, "Invalid statement type: %d\n", statement->type);
	}

	if (result)
		final = g_strdup (result);
	else
		final = NULL;

	memsql_free (result);

	return final;
}

static int
sql_statement_select_append_field (sql_select_statement * select,
				   sql_field * field)
{
	select->fields = g_list_append (select->fields, field);
	return 0;
}

/**
 * sql_statement_append_field:
 * @statment: A Sql statement generated from parsing an sql statement
 * @table: Name of table to add, this can be %NULL
 * @fieldname: Field to add to the statment
 *
 * Adds an field into a select statement
 * 
 * Returns: non-zero on error.
 */
int
sql_statement_append_field (sql_statement * statement, char *table,
			    char *fieldname, char *as)
{
	sql_field_item *item;
	sql_field *field;
	GList *name = NULL;

	if (!fieldname)
		return -1;

	if (table)
		name = g_list_append (name, memsql_strdup (table));

	name = g_list_append (name, memsql_strdup (fieldname));
	item = sql_field_item_build (name);
	field = sql_field_build (item);
	if (!as)			/* FIXME ? */
		sql_field_set_as (field, as);

	switch (statement->type) {
	case SQL_select:
		sql_statement_select_append_field (statement->statement, field);
		break;

	case SQL_insert:
	default:
		fprintf (stderr, "Invalid statement type: %d", statement->type);
	}

	return 0;
}

/**
 * sql_statement_append_tablejoin:
 * @statement: A Sql statement generated from parsing an sql statement
 * @table: Name of table to add.
 *
 * Adds a table into a select statement
 * 
 * Returns: non-zero on error.
 */
int
sql_statement_append_tablejoin (sql_statement * statement, char *lefttable,
				char *righttable, char *leftfield,
				char *rightfield)
{
	sql_field_item *leftfitem, *rightfitem;
	sql_field *leftf, *rightf;
	sql_table *table;
	sql_where *where, *otherwhere;
	sql_condition *cond;
	sql_select_statement *select;

	g_assert (lefttable);
	g_assert (righttable);
	g_assert (leftfield);
	g_assert (rightfield);

	if (statement->type != SQL_select) {
		fprintf (stderr, "Invalid statement type: %d", statement->type);
		return -1;
	}

	table = memsql_calloc (sizeof (sql_table));
	table->type = SQL_simple;
	table->d.simple = memsql_strdup (righttable);

	leftf = memsql_calloc (sizeof (sql_field));
	rightf = memsql_calloc (sizeof (sql_field));
	leftfitem = memsql_calloc (sizeof (sql_field_item));
	rightfitem = memsql_calloc (sizeof (sql_field_item));

	leftfitem->type = SQL_name;
	leftfitem->d.name =
		g_list_prepend (leftfitem->d.name,
				memsql_strdup_printf ("%s.%s", lefttable, leftfield));
	rightfitem->type = SQL_name;
	rightfitem->d.name =
		g_list_prepend (rightfitem->d.name,
				memsql_strdup_printf ("%s.%s", righttable, rightfield));
	leftf->item = leftfitem;
	rightf->item = rightfitem;

	cond = memsql_calloc (sizeof (sql_condition));
	cond->op = SQL_eq;
	cond->d.pair.left = leftf;
	cond->d.pair.right = rightf;

	where = memsql_calloc (sizeof (sql_where));
	where->type = SQL_single;
	where->d.single = cond;

	select = statement->statement;
	select->from = g_list_append (select->from, table);

	if (select->where == NULL) {
		select->where = where;
	}
	else {
		otherwhere = select->where;

		select->where = memsql_calloc (sizeof (sql_where));
		select->where->type = SQL_pair;
		select->where->d.pair.left = otherwhere;
		select->where->d.pair.right = where;
		select->where->d.pair.op = SQL_and;
	}
	return 0;
}

int
sql_statement_append_where (sql_statement * statement, char *leftfield,
			    char *rightfield, sql_logic_operator logicopr,
			    sql_condition_operator condopr)
{
	gboolean freerightfield = FALSE;
	sql_field_item *leftfitem, *rightfitem;
	sql_field *leftf, *rightf;
	sql_where *where, *otherwhere, *tmpwhere, *parentwhere;
	sql_condition *cond;
	sql_select_statement *select;

	g_assert (leftfield);
	/* only works for SELECT statements */
	if (statement->type != SQL_select) {
		fprintf (stderr, "Invalid statement type: %d", statement->type);
		return -1;
	}
	/* in case null passed in on the rightfield. Modify it to handle it correctly */
	if (!rightfield) {
		if (condopr == SQL_eq || condopr == SQL_like)
			condopr = SQL_is;
		else
			condopr = SQL_not;
		rightfield = memsql_strdup ("NULL");
		freerightfield = TRUE;
	}
	/* The actual work */
	leftf = memsql_calloc (sizeof (sql_field));
	rightf = memsql_calloc (sizeof (sql_field));
	leftfitem = memsql_calloc (sizeof (sql_field_item));
	rightfitem = memsql_calloc (sizeof (sql_field_item));

	leftfitem->type = SQL_name;
	leftfitem->d.name =
		g_list_prepend (leftfitem->d.name,
				memsql_strdup_printf ("%s", leftfield));
	rightfitem->type = SQL_name;
	rightfitem->d.name =
		g_list_prepend (rightfitem->d.name,
				memsql_strdup_printf ("%s", rightfield));
	leftf->item = leftfitem;
	rightf->item = rightfitem;

	cond = memsql_calloc (sizeof (sql_condition));
	cond->op = condopr;
	cond->d.pair.left = leftf;
	cond->d.pair.right = rightf;

	where = memsql_calloc (sizeof (sql_where));
	where->type = SQL_single;
	where->d.single = cond;

	select = statement->statement;

	if (select->where == NULL) {
		select->where = where;
	}
	else {
		tmpwhere = select->where;
		parentwhere = NULL;
		/* break tree in half if its a and after a Or statement. 
		 * This is to handle the second and statement in this example:
		 * x=y and (x=1 or x=2 or x=3) and y=z */
		if (logicopr == SQL_and) {
			while (tmpwhere->type != SQL_single) {
				if (tmpwhere->d.pair.op == SQL_or) {
					/* insert a record above this or item */
					otherwhere = tmpwhere;

					tmpwhere = memsql_calloc (sizeof (sql_where));
					tmpwhere->type = SQL_pair;
					tmpwhere->d.pair.left = otherwhere;
					tmpwhere->d.pair.right = where;
					tmpwhere->d.pair.op = logicopr;
					/* insert into parent to store this item */
					if (parentwhere == NULL)
						select->where = tmpwhere;
					else
						parentwhere->d.pair.right = tmpwhere;
					return 0;
				}
				parentwhere = tmpwhere;
				tmpwhere = tmpwhere->d.pair.right;
			}
			tmpwhere = select->where;
		}

		/* Find the end of the list */
		while (tmpwhere->type != SQL_single) {
			parentwhere = tmpwhere;
			tmpwhere = tmpwhere->d.pair.right;
		}
		/* Now append onto it */
		otherwhere = tmpwhere;

		tmpwhere = memsql_calloc (sizeof (sql_where));
		tmpwhere->type = SQL_pair;
		tmpwhere->d.pair.left = otherwhere;
		tmpwhere->d.pair.right = where;
		tmpwhere->d.pair.op = logicopr;

		/* insert into parent to store this item */
		if (parentwhere == NULL)
			select->where = tmpwhere;
		else
			parentwhere->d.pair.right = tmpwhere;
	}
	if (freerightfield)
		memsql_free (rightfield);
	return 0;
}

GList *
sql_statement_get_fields (sql_statement * statement)
{
	GList *retval = NULL;
	GList *walk;
	gchar *temp1, *temp2;
	sql_select_statement *select;

	if (!statement)
		return NULL;

	if (!statement->type == SQL_select)
		return NULL;

	select = statement->statement;

	for (walk = select->fields; walk != NULL; walk = walk->next) {
		temp1 = sql_field_stringify (walk->data);
		temp2 = g_strdup (temp1);
		memsql_free (temp1);

		retval = g_list_append (retval, temp2);
	}
	return retval;
}

void
sql_statement_free_fields (GList * fields)
{
	GList *walk;

	for (walk = g_list_first (fields); walk != NULL; walk = walk->next)
		g_free (walk->data);
	g_list_free (fields);
}

void
sql_statement_free_tables (GList * tables)
{
	GList *walk;

	for (walk = g_list_first (tables); walk != NULL; walk = walk->next)
		g_free (walk->data);
	g_list_free (tables);
}

GList *
sql_statement_get_tables (sql_statement * statement)
{
	GList *retval = NULL;
	GList *walk;
	gchar *temp1, *temp2;
	sql_select_statement *select;

	if (!statement)
		return NULL;

	if (!statement->type == SQL_select)
		return NULL;

	select = statement->statement;

	for (walk = select->from; walk != NULL; walk = walk->next) {
		temp1 = sql_table_stringify (walk->data);
		temp2 = g_strdup (temp1);
		memsql_free (temp1);

		retval = g_list_append (retval, temp2);
	}

	return retval;
}

gchar *
sql_statement_get_first_table (sql_statement * statement)
{
	sql_select_statement *select;
	gchar *retval, *temp;

	if (!statement)
		return NULL;

	if (!statement->type == SQL_select)
		return NULL;

	select = statement->statement;

	temp = sql_table_stringify (select->from->data);
	retval = g_strdup (temp);
	memsql_free (temp);

	return retval;
}

static sql_where *
sql_statement_searchwhere_rec (sql_where * where, gchar * lookfor)
{
	sql_condition *single;
	sql_where *retwalk;
	sql_field_item *item;
	GList *walk;

	if (where) {
		switch (where->type) {
		case SQL_single:
			single = where->d.single;
			/* Check left pair items */
			item = single->d.pair.left->item;
			if (item->type == SQL_name) {
				for (walk = g_list_first (item->d.name); walk != NULL;
				     walk = walk->next) {
					if (strcasecmp ((gchar *) walk->data, lookfor) == 0)
						return where;
				}
			}
			/* Check right pair items */
			item = single->d.pair.right->item;
			if (item->type == SQL_name) {
				for (walk = g_list_first (item->d.name); walk != NULL;
				     walk = walk->next) {
					if (strcasecmp ((gchar *) walk->data, lookfor) == 0)
						return where;
				}
			}
			/* incase its wierd between case */
			if (single->op == SQL_between) {
				item = single->d.between.upper->item;
				if (item->type == SQL_name) {
					for (walk = g_list_first (item->d.name); walk != NULL;
					     walk = walk->next) {
						if (strcasecmp ((gchar *) walk->data, lookfor) == 0)
							return where;
					}
				}
			}
			break;
		case SQL_negated:
			return sql_statement_searchwhere_rec (where, lookfor);
		case SQL_pair:
			retwalk = sql_statement_searchwhere_rec (where->d.pair.left, lookfor);
			if (retwalk)
				return retwalk;
			return sql_statement_searchwhere_rec (where->d.pair.right, lookfor);
			break;
		}
	}
	return NULL;
}

static gint
sql_statement_get_where_ontable (sql_where * where, gchar * ontable,
				 GList ** leftfield, GList ** rightfield,
				 sql_condition_operator * condopr)
{
	sql_condition *single;
	sql_field_item *item;

	single = where->d.single;
	item = single->d.pair.left->item;
	if (item->type == SQL_name)
		*leftfield = item->d.name;

	item = single->d.pair.right->item;
	if (item->type == SQL_name)
		*leftfield = item->d.name;

	if (*leftfield && *rightfield) {
		*condopr = single->op;
		return 0;
	}

	*leftfield = NULL;
	*rightfield = NULL;
	return -1;
}

gint
sql_statement_get_wherejoin_ontable (sql_statement * statement,
				     gchar * ontable, GList ** leftfield,
				     GList ** rightfield,
				     sql_condition_operator * condopr)
{
	sql_where *where;
	sql_select_statement *select;

	g_assert (statement);
	*leftfield = NULL;
	*rightfield = NULL;
	*condopr = 0;

	if (statement->type != SQL_select) {
		fprintf (stderr, "Invalid statement type: %d", statement->type);
		return -1;
	}
	select = statement->statement;
	/* Check for a where statement */
	where = sql_statement_searchwhere_rec (select->where, ontable);
	if (where) {
		if (where->type == SQL_single)
			if (!sql_statement_get_where_ontable
			    (where, ontable, leftfield, rightfield, condopr))
				return 0;
	}


	/* Check for a join */


	return -1;
}
