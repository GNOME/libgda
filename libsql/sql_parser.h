#ifndef SQL_PARSER_H
#define SQL_PARSER_H

#include <glib.h>

typedef enum
{
   SQL_select,
   SQL_insert,
   SQL_delete,
   SQL_update
}
sql_statement_type;

typedef struct
   {
   sql_statement_type type;
   char *full_query;
   void *statement;
   }
sql_statement;

typedef struct sql_where sql_where;
typedef struct sql_condition sql_condition;
typedef struct sql_table sql_table;

typedef struct
   {
   int distinct;
   GList *fields;
   GList *from;
   sql_where *where;
   GList *order;
   GList *group;
   }
sql_select_statement;

typedef struct
   {
   sql_table *table;
	GList *fields;
   GList *values;
   }
sql_insert_statement;

typedef struct
	{
	sql_table *table;
	GList *set;
	sql_where *where;
	}
sql_update_statement;

typedef enum
{
   SQL_name,
   SQL_equation,
   SQL_inlineselect,
   SQL_function
}
sql_field_type;

typedef enum
{
   SQL_plus,
   SQL_minus,
   SQL_times,
   SQL_div
}
sql_field_operator;

typedef struct sql_field_item sql_field_item;

struct sql_field_item
   {
   sql_field_type type;

   union
     {
      GList *name;
      struct
         {
         sql_field_item *left;
         sql_field_item *right;
         sql_field_operator op;
         }
      equation;
      sql_select_statement *select;
      struct
      {
	     gchar *funcname;
	     GList *funcarglist;
      } function;
     } d;
   };

typedef struct
   {
   sql_field_item *item;
   char *as;
   }
sql_field;

typedef enum
{
   SQL_simple,
   SQL_join,
   SQL_nestedselect
}
sql_table_type;

struct sql_table
   {
   sql_table_type type;
   union
      {
      char *simple;
      struct
         {
         sql_table *left;
         sql_table *right;
         sql_condition *cond;
         }
      join;
      sql_select_statement *select;
      }
   d;
   char *as;
   };

typedef enum
{
   SQL_eq,
   SQL_is,
   SQL_not,
   SQL_in,
   SQL_like,
   SQL_notin,
   SQL_between
}
sql_condition_operator;

struct sql_condition
   {
   sql_condition_operator op;

   union
      {
      struct
         {
         sql_field *left;
         sql_field *right;
         }
      pair;
      struct
         {
         sql_field *field;
         sql_field *lower;
         sql_field *upper;
         }
      between;
      }
   d;
   };

typedef enum
{
   SQL_and,
   SQL_or
}
sql_logic_operator;

typedef enum
{
   SQL_single,
   SQL_negated,
   SQL_pair
}
sql_where_type;

struct sql_where
   {
   sql_where_type type;

   union
      {
      sql_condition *single;
      sql_where *negated;
      struct
         {
         sql_where *left;
         sql_where *right;
         sql_logic_operator op;
         }
      pair;
      }
   d;
   };

int sql_display(sql_statement * statement);
int sql_destroy(sql_statement * statement);
sql_statement *sql_parse(char *sql_statement);

char *sql_stringify(sql_statement * statement);
int sql_statement_append_field(sql_statement * statement, char *tablename, char *fieldname);

GList *sql_statement_get_fields(sql_statement * statement);
GList *sql_statement_get_tables(sql_statement * statement);

char *sql_statement_get_first_table(sql_statement * statement);

#endif /* SQL_PARSER_H */
