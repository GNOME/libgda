#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sql_parser.h"

#define output(fmt,args...) fprintf (stdout, "%*s" fmt "\n" , indent * 2, "" , ##args)

static int sql_display_select(int indent, sql_select_statement * statement);
static int sql_display_field(int indent, sql_field * field);

static int
sql_display_field_item(int indent, sql_field_item * item)
   {
   GList *cur;

   switch (item->type)
      {
   case SQL_name:
      fprintf(stdout, "%*s", indent * 2, "");
      for (cur = item->d.name; cur != NULL; cur = cur->next)
         fprintf(stdout, "%s%s", (char *)cur->data, cur->next ? "." : "\n");
      break;

   case SQL_equation:
      output("equation: %d", item->d.equation.op);
      output("left:");
      sql_display_field_item(indent + 1, item->d.equation.left);
      output("right");
      sql_display_field_item(indent + 1, item->d.equation.right);
      break;

   case SQL_inlineselect:
      output("select:");
      sql_display_select(indent + 1, item->d.select);
      break;

	case SQL_function:
		output ("function: %s", item->d.function.funcname);
		for (cur = item->d.function.funcarglist; cur != NULL; cur = cur->next)
			sql_display_field (indent + 1, cur->data);
		break;

      }
   return 0;
   }

static int
sql_display_field(int indent, sql_field * field)
   {
   sql_display_field_item(indent, field->item);

   if (field->as)
      output("as: %s", field->as);
   return 0;
   }

static int
sql_display_condition(int indent, sql_condition * cond)
   {
   if (!cond)
      return 0;

   output("op: %d", cond->op);
   switch (cond->op)
      {
   case SQL_eq:
   case SQL_is:
   case SQL_not:
   case SQL_in:
   case SQL_notin:
   case SQL_like:
      output("left:");
      sql_display_field(indent + 1, cond->d.pair.left);
      output("right:");
      sql_display_field(indent + 1, cond->d.pair.right);
      break;

   case SQL_between:
      output("field:");
      sql_display_field(indent + 1, cond->d.between.field);
      output("lower:");
      sql_display_field(indent + 1, cond->d.between.lower);
      output("upper:");
      sql_display_field(indent + 1, cond->d.between.upper);
      break;
      }

   return 0;
   }

static int
sql_display_where(int indent, sql_where * where)
   {
   switch (where->type)
      {
   case SQL_single:
      sql_display_condition(indent + 1, where->d.single);
      break;

   case SQL_negated:
      output("negated:");
      sql_display_where(indent + 1, where->d.negated);
      break;

   case SQL_pair:
      output("pair: %d", where->d.pair.op);
      output("left:");
      sql_display_where(indent + 1, where->d.pair.left);
      output("right:");
      sql_display_where(indent + 1, where->d.pair.right);
      break;
      }

   return 0;
   }

static int
sql_display_table(int indent, sql_table * table)
   {
   switch (table->type)
      {
   case SQL_simple:
      output("table: %s", table->d.simple);
      break;

   case SQL_join:
      output("table:");
      output("cond:");
      sql_display_condition(indent + 1, table->d.join.cond);
      output("left");
      sql_display_table(indent + 1, table->d.join.left);
      output("right");
      sql_display_table(indent + 1, table->d.join.right);
      break;

   case SQL_nestedselect:
      output("table:");
      sql_display_select(indent + 1, table->d.select);
      break;
      }
   return 0;
   }

static int
sql_display_select(int indent, sql_select_statement * statement)
   {
   GList *cur;

   if (statement->distinct)
      output("distinct");

   output("fields:");

   for (cur = statement->fields; cur != NULL; cur = cur->next)
      sql_display_field(indent + 1, cur->data);

   output("from:");

   for (cur = statement->from; cur != NULL; cur = cur->next)
      sql_display_table(indent + 1, cur->data);

   if (statement->where)
      {
      output("where:");
      sql_display_where(indent + 1, statement->where);
      }

   if (statement->order)
      output("order by:");
   for (cur = statement->order; cur != NULL; cur = cur->next)
      sql_display_field(indent + 1, cur->data);

   if (statement->group)
      output("group by:");
   for (cur = statement->group; cur != NULL; cur = cur->next)
      sql_display_field(indent + 1, cur->data);

   return 0;
   }

static int
sql_display_insert (int indent, sql_insert_statement *insert)
{
	GList *walk;

	output ("table");
	sql_display_table (indent + 1, insert->table);

	if (insert->fields)
		{
		output ("fields:");
		for (walk = insert->fields; walk != NULL; walk = walk->next)
			sql_display_field (indent + 1, walk->data);
		}

	output ("values:");
	for (walk = insert->values; walk != NULL; walk = walk->next)
		sql_display_field (indent + 1, walk->data);

	return 0;
}

static int 
sql_display_update(int indent, sql_update_statement *update)
{
	GList *walk;

	output ("table:");
	sql_display_table (indent + 1, update->table);

	output ("set:");
	for (walk = update->set; walk != NULL; walk = walk->next)
		{
		sql_display_condition (indent + 1, walk->data);
		}

	if (update->where)
		{
		output ("where:");
		sql_display_where (indent + 1, update->where);
		}

	return 0;}

int
sql_display(sql_statement * statement)
   {
   int indent = 0;

   output("query: %s", statement->full_query);
   switch (statement->type)
      {
   case SQL_select:
      sql_display_select(indent + 1, statement->statement);
      break;

	case SQL_insert:
	   sql_display_insert (indent + 1, statement->statement);
		break;

	case SQL_update:
		sql_display_update (indent + 1, statement->statement);
		break;

   default:
      fprintf(stderr, "Unknown statement type: %d", statement->type);
      }

   return 0;
   }
