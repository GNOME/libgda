%{
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "sql_parser.h"
#include "sql_tree.h"
#include "mem.h"

int sqlerror (char *);
int sqllex ();

extern sql_statement *sql_result;
%}

%union{
void *v;
char *str;
int i;
GList *list;
sql_field *f;
sql_field_item *fi;
sql_table *t;
sql_condition *c;
sql_where *w;
param_spec *ps;
}

%token L_SELECT L_FROM L_WHERE L_AS L_JOIN L_ON L_ORDER L_BY 
%token L_DISTINCT L_BETWEEN L_IN L_GROUP L_INSERT L_INTO L_VALUES L_UPDATE L_SET
%token L_DOT L_STAR L_COMMA L_NULL L_LBRACKET L_RBRACKET
%token L_IDENT
%token L_EQ L_IS L_LIKE
%token L_NOT L_AND L_OR
%token L_MINUS L_PLUS L_TIMES L_DIV
%token L_STRING L_TEXTUAL
%token L_DELETE

%token L_LSBRACKET L_RSBRACKET
%token L_PNAME L_PTYPE L_PISPARAM L_PDESCR L_PNULLOK

%type <v> select_statement insert_statement update_statement delete_statement
%type <str> L_IDENT opt_table_as L_STRING L_TEXTUAL
%type <list> fields_list tables_list field_name dotted_name opt_orderby opt_groupby opt_fields_list set_list param_spec param_spec_list
%type <f> field
%type <fi> field_raw
%type <t> table table_simple
%type <i> logic_operator condition_operator field_op opt_distinct
%type <w> where_list opt_where
%type <c> where_item set_item
%type <ps> param_spec_item

%left L_AND L_OR

%left L_PLUS L_MINUS
%left L_TIMES L_DIV
%%
statement: select_statement	{sql_result = sql_statement_build (SQL_select, $1);}
	| insert_statement 	{sql_result = sql_statement_build (SQL_insert, $1);}
	| update_statement	{sql_result = sql_statement_build (SQL_update, $1);}
	| delete_statement	{sql_result = sql_statement_build (SQL_delete, $1);}
	;
	
select_statement: L_SELECT opt_distinct fields_list L_FROM tables_list opt_where opt_orderby opt_groupby
			{$$ = sql_select_statement_build ($2, $3, $5, $6, $7, $8);}
	| L_SELECT opt_distinct fields_list opt_where opt_orderby opt_groupby
			{$$ = sql_select_statement_build ($2, $3, NULL, $4, $5, $6);}
	;

insert_statement: L_INSERT L_INTO table opt_fields_list L_VALUES L_LBRACKET fields_list L_RBRACKET
			{$$ = sql_insert_statement_build ($3, $4, $7);}
	;

update_statement: L_UPDATE table L_SET set_list opt_where
			{$$ = sql_update_statement_build ($2, $4, $5);}
			;

delete_statement: L_DELETE L_FROM table opt_where
			{$$ = sql_delete_statement_build ($3, $4);}
		;

set_list: set_item					{$$ = g_list_append (NULL, $1);}
	| set_item L_COMMA set_list	{$$ = g_list_prepend ($3, $1);}
	;

opt_fields_list: L_LBRACKET fields_list L_RBRACKET {$$ = $2;}
	|																{$$ = NULL;}
	;

opt_distinct: L_DISTINCT	{$$ = 1;}
	|			{$$ = 0;}
	;

opt_where: L_WHERE where_list	{$$ = $2;}
	| 			{$$ = NULL;}
	;

opt_orderby: L_ORDER L_BY fields_list	{$$ = $3;}
	|				{$$ = NULL;}
	;

opt_groupby: L_GROUP L_BY fields_list 	{$$ = $3;}
	|				{$$ = NULL;}
	;

fields_list: field			{$$ = g_list_append (NULL, $1);}
	| field L_COMMA fields_list	{$$ = g_list_prepend ($3, $1);}
	;

tables_list: table			{$$ = g_list_append (NULL, $1);}
	| table L_COMMA tables_list	{$$ = g_list_prepend ($3, $1);}
	;

field: field_raw			{$$ = sql_field_build ($1);}
	| field_raw L_AS L_IDENT	{$$ = sql_field_set_as (sql_field_build ($1), $3);}
	| field_raw param_spec		{$$ = sql_field_set_param_spec (sql_field_build ($1), $2);}
	| field_raw param_spec L_AS L_IDENT        {$$ = sql_field_set_as (sql_field_set_param_spec (sql_field_build ($1), $2), $4);}
	;

dotted_name: L_IDENT			{$$ = g_list_append (NULL, memsql_strdup ($1)); memsql_free ($1);}
	| L_IDENT L_DOT dotted_name	{$$ = g_list_prepend ($3, memsql_strdup ($1)); memsql_free ($1);}
	;

field_name: dotted_name			{$$ = $1;}
	| L_STAR			{$$ = g_list_append (NULL, memsql_strdup ("*"));}
	| L_NULL			{$$ = g_list_append (NULL, memsql_strdup ("null"));}
	| L_STRING			{$$ = g_list_append (NULL, $1);}
	;

field_op: L_MINUS			{$$ = SQL_minus;}
	| L_PLUS			{$$ = SQL_plus;}
	| L_TIMES			{$$ = SQL_times;}
	| L_DIV				{$$ = SQL_div;}
	;

field_raw: field_name			{$$ = sql_field_item_build ($1);}
	| field_raw field_op field_raw	{$$ = sql_field_item_build_equation ($1, $3, $2);}
	| select_statement		{$$ = sql_field_item_build_select ($1);}
	| L_LBRACKET field_raw L_RBRACKET	{$$ = $2;}
	| L_IDENT L_LBRACKET fields_list L_RBRACKET	{$$ = sql_field_build_function($1, $3); }
        | L_IDENT L_LBRACKET L_RBRACKET     {$$ = sql_field_build_function($1, NULL); }
	;

table: table_simple opt_table_as		{$$ = sql_table_set_as ($1, $2);}
	;

opt_table_as: L_AS L_IDENT	{$$ = $2;}
	|			{$$ = NULL;}
	;

table_simple: L_IDENT				{$$ = sql_table_build ($1); memsql_free ($1);}
	| table L_JOIN table_simple L_ON where_item	{$$ = sql_table_build_join ($1, $3, $5); }
	| select_statement		{$$ = sql_table_build_select ($1);}
	| L_LBRACKET table_simple L_RBRACKET	{$$ = $2;}
	;

logic_operator: L_AND			{$$ = SQL_and;}
	| L_OR				{$$ = SQL_or;}
	;

where_list: where_item			{$$ = sql_where_build_single ($1);}
	| L_NOT where_list		{$$ = sql_where_build_negated ($2);}
	| where_list logic_operator where_list
					{$$ = sql_where_build_pair ($1, $3, $2);}
	| L_LBRACKET where_list L_RBRACKET	{$$ = $2;}
	;

set_item: field L_EQ field		{$$ = sql_build_condition ($1, $3, SQL_eq);}
	;

condition_operator: L_EQ		{$$ = SQL_eq;}
	| L_IS				{$$ = SQL_is;}
	| L_NOT				{$$ = SQL_not;}
	| L_IN				{$$ = SQL_in;}
	| L_NOT L_IN			{$$ = SQL_notin;}
	| L_LIKE			{$$ = SQL_like;}
	;

where_item: field condition_operator field
			{$$ = sql_build_condition ($1, $3, $2);}
	| field L_BETWEEN field L_AND field
			{$$ = sql_build_condition_between ($1, $3, $5);}
	;

param_spec: L_LSBRACKET param_spec_list L_RSBRACKET {$$ = $2;}
	;

param_spec_list: param_spec_item 			{$$ = g_list_append (NULL, $1);}
	| param_spec_item param_spec_list		{$$ = g_list_prepend ($2, $1);}
	|						{;}
	;


param_spec_item: L_PNAME L_EQ L_TEXTUAL      	{$$ = param_spec_build (PARAM_name, $3);}
	| L_PDESCR L_EQ L_TEXTUAL		{$$ = param_spec_build (PARAM_descr, $3);}
	| L_PTYPE L_EQ L_TEXTUAL		{$$ = param_spec_build (PARAM_type, $3);}
	| L_PISPARAM L_EQ L_TEXTUAL		{$$ = param_spec_build (PARAM_isparam, $3);}
	| L_PNULLOK L_EQ L_TEXTUAL		{$$ = param_spec_build (PARAM_nullok, $3);}
	;
%%

