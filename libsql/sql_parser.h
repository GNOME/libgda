#ifndef SQL_PARSER_H
#define SQL_PARSER_H

#include <glib.h>
#define YY_NO_UNISTD_H


typedef struct sql_statement        sql_statement;
typedef struct sql_select_statement sql_select_statement;
typedef struct sql_insert_statement sql_insert_statement;
typedef struct sql_update_statement sql_update_statement;
typedef struct sql_delete_statement sql_delete_statement;

typedef struct sql_table            sql_table;
typedef struct sql_where            sql_where;
typedef struct sql_condition        sql_condition;
typedef struct sql_order_field      sql_order_field;

typedef struct sql_field            sql_field;
typedef struct sql_field_item       sql_field_item;
typedef struct param_spec           param_spec;
typedef struct sql_wherejoin sql_wherejoin;

/* 
 * global SQL statement structures 
 */
typedef enum
{
	SQL_select,
	SQL_insert,
	SQL_delete,
	SQL_update
}
sql_statement_type;


struct sql_statement
{
	sql_statement_type  type;
	char               *full_query;
	void               *statement;
};

struct sql_select_statement
{
	int        distinct;
	GList     *fields; /* list of sql_field */
	GList     *from; /* list of sql_table */
	sql_where *where;
	GList     *order; /* list of sql_order_field */
	GList     *group;
};

struct sql_insert_statement
{
	sql_table *table;
	GList     *fields;
	GList     *values;
};

struct sql_update_statement
{
	sql_table *table;
	GList     *set;
	sql_where *where;
};

struct sql_delete_statement
{
	sql_table *table;
	sql_where *where;
};



/* 
 * Fields structures
 */
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

struct sql_field
{
	sql_field_item *item;
 	char           *as;
	GList          *param_spec;
};

typedef enum
{
	PARAM_name,
	PARAM_descr,
	PARAM_type,
	PARAM_isparam,
	PARAM_nullok
} param_spec_type;

struct param_spec
{
	param_spec_type  type;
	char            *content;
};



/*
 * Tables structures
 */
typedef enum
{
	SQL_simple,
	SQL_nestedselect,
    SQL_tablefunction
}
sql_table_type;

typedef enum
{
	SQL_cross_join, /* default */
	SQL_inner_join,
	SQL_left_join,
	SQL_right_join,
	SQL_full_join
}
sql_join_type;

struct sql_table
{
	sql_table_type type;
	union
	{
		char                 *simple;
		sql_select_statement *select;	        
		struct
		{
			gchar *funcname;
			GList *funcarglist;
		} function;
	}
	d;
	char          *as;
	sql_join_type  join_type;
	sql_where     *join_cond;
};


/*
 * Conditions structures
 */
typedef enum
{
	SQL_eq,
	SQL_is,
	SQL_in,
	SQL_like,
	SQL_between,
	SQL_gt,
	SQL_lt,
	SQL_geq,
	SQL_leq,
	SQL_diff,
	SQL_regexp,
	SQL_regexp_ci,
	SQL_not_regexp,
	SQL_not_regexp_ci,
	SQL_similar,
    SQL_not
}
sql_condition_operator;

struct sql_condition
{
	sql_condition_operator op;
	gboolean               negated;

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

/*
 * ORDER BY structure
 */
typedef enum
{
	SQL_asc,
	SQL_desc
}
sql_ordertype;

struct sql_order_field
{
	sql_ordertype  order_type;
	GList         *name;
};

/* This is only used by sql_statement_get_wherejoin() */
struct sql_wherejoin
	{
	GList *leftfield; /* glist contains a string in data tag */
	GList *rightfield; /* glist is in table then field order */
	sql_condition_operator condopr;
	/* if left or right side is a constaint mark as true. ie. 
	 * 'value' or 1234 */
	gboolean rightconstaint;
	gboolean leftconstaint;
	gboolean isajoin;	/* If it is a join not a where statement */
	
	sql_where *orginalwhere;
	};

int   sql_display (sql_statement * statement);
int   sql_destroy (sql_statement * statement);
char *sql_stringify (sql_statement * statement);

sql_statement *sql_parse (const char *sql_statement);
sql_statement *sql_parse_with_error (const char *sql_statement, GError ** error);

int sql_statement_append_field (sql_statement * statement, char *tablename,
				char *fieldname, char *as);
int sql_statement_append_tablejoin (sql_statement * statement,
				    char *lefttable, char *righttable,
				    char *leftfield, char *rightfield);
int sql_statement_append_where (sql_statement * statement, char *leftfield,
				char *rightfield, sql_logic_operator logicopr,
				sql_condition_operator condopr);

GList *sql_statement_get_fields (sql_statement * statement);
GList *sql_statement_get_tables (sql_statement * statement);
void sql_statement_free_tables (GList * tables);
void sql_statement_free_fields (GList * fields);

char *sql_statement_get_first_table (sql_statement * statement);

/* Returns a list of sql_wherelist */
GList *sql_statement_get_wherejoin (sql_statement * statement);
void sql_statement_free_wherejoin(GList **wherelist);
gint sql_statement_test_wherejoin(void);
void sql_statement_get_wherejoin_components(
	sql_wherejoin *wherejoin, char **table, char **field, 
        char leftside);

#endif /* SQL_PARSER_H */
