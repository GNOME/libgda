%{
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "gda-sql-delimiter.h"
#include "gda-delimiter-tree.h"

int gda_delimitererror (char *);
int gda_delimiterlex ();

extern GdaDelimiterStatement *sql_result;
#define YYDEBUG 1
%}

%union{
void            *v;
char            *str;
GList           *list;
GdaDelimiterParamSpec     *ps;
}

%token L_SELECT 
%token L_INSERT L_UPDATE
%token L_IDENT
%token L_STRING L_TEXTUAL
%token L_DELETE

%token L_LSBRACKET L_RSBRACKET
%token L_PNAME L_PTYPE L_PISPARAM L_PDESCR L_PNULLOK
%token L_UNSPECVAL

%token L_EQ

%type <v> select_statement insert_statement update_statement delete_statement expr
%type <str> L_IDENT L_STRING L_TEXTUAL L_EQ
%type <list> param_spec param_spec_list expr_list
%type <ps> param_spec_item

%%

statement: select_statement     {sql_result = $1;}
        | insert_statement      {sql_result = $1;}
        | update_statement      {sql_result = $1;}
        | delete_statement      {sql_result = $1;}
	| expr_list		{sql_result = gda_delimiter_statement_build (GDA_DELIMITER_UNKNOWN, $1);}
        ;

select_statement: L_SELECT expr_list {$$ = gda_delimiter_statement_build (GDA_DELIMITER_SQL_SELECT, $2);}
	;

insert_statement: L_INSERT expr_list {$$ = gda_delimiter_statement_build (GDA_DELIMITER_SQL_INSERT, $2);}
	;

update_statement: L_UPDATE expr_list {$$ = gda_delimiter_statement_build (GDA_DELIMITER_SQL_UPDATE, $2);}
	;

delete_statement: L_DELETE expr_list {$$ = gda_delimiter_statement_build (GDA_DELIMITER_SQL_DELETE, $2);}
        ;

expr_list: expr           {$$ = g_list_prepend (NULL, $1);}
	| expr expr_list  {$$ = g_list_prepend ($2, $1);}
        ;

expr: L_TEXTUAL                   {$$ = gda_delimiter_expr_build (g_strdup_printf ("\"%s\"", $1), NULL); g_free ($1);}
        | L_STRING                {$$ = gda_delimiter_expr_build ($1, NULL);}
        | L_IDENT                 {$$ = gda_delimiter_expr_build ($1, NULL);}
        | L_UNSPECVAL param_spec  {$$ = gda_delimiter_expr_build (NULL, $2);}
	| L_EQ			  {$$ = gda_delimiter_expr_build (g_strdup ("="), NULL);}
        ;

param_spec: L_LSBRACKET param_spec_list L_RSBRACKET {$$ = $2;}
	;

param_spec_list: param_spec_item 			{$$ = g_list_append (NULL, $1);}
	| param_spec_item param_spec_list		{$$ = g_list_prepend ($2, $1);}
	;


param_spec_item: L_PNAME L_EQ L_TEXTUAL      	{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NAME, $3);}
	| L_PDESCR L_EQ L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_DESCR, $3);}
	| L_PTYPE L_EQ L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_TYPE, $3);}
	| L_PISPARAM L_EQ L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_ISPARAM, $3);}
	| L_PNULLOK L_EQ L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NULLOK, $3);}
	;
%%
