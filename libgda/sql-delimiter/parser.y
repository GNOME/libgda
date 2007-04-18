%{
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "gda-sql-delimiter.h"
#include "gda-delimiter-tree.h"

int gda_delimitererror (char *);
int gda_delimiterlex ();

static gchar *
remove_quotes (gchar *str)
{
        glong total;
        gchar *ptr;
        glong offset = 0;

        total = strlen (str);
        g_assert (*str == '\'');
        g_assert (str[total-1] == '\'');
        g_memmove (str, str+1, total-2);
        total -=2;
        str[total] = 0;

        ptr = (gchar *) str;
        while (offset < total) {
                /* we accept the "''" as a synonym of "\'" */
                if (*ptr == '\'') {
                        if (*(ptr+1) == '\'') {
                                g_memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                *str = 0;
                                return str;
                        }
                }
                if (*ptr == '\\') {
                        if (*(ptr+1) == '\\') {
                                g_memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                if (*(ptr+1) == '\'') {
                                        *ptr = '\'';
                                        g_memmove (ptr+1, ptr+2, total - offset);
                                        offset += 2;
                                }
                                else {
                                        *str = 0;
                                        return str;
                                }
                        }
                }
                else
                        offset ++;

                ptr++;
        }

        return str;
}

extern GList                        *all_sql_results; /* list of GdaDelimiterStatement */
extern GdaDelimiterStatement *last_sql_result; /* last GdaDelimiterStatement identified */
gboolean is_first_token;

#define YYDEBUG 1
%}

%union{
void            *v;
char            *str;
GList           *list;
GdaDelimiterParamSpec     *ps;
}

%token L_WORD L_CHAR L_SPC
%token L_STRING L_TEXTUAL

%token L_LSBRACKET L_RSBRACKET
%token L_PNAME L_PTYPE L_PISPARAM L_PDESCR L_PNULLOK
%token L_UNSPECVAL

%token L_SC 

%type <v> expr statement
%type <str> L_WORD L_CHAR L_STRING L_TEXTUAL L_SC chunck
%type <list> param_spec param_spec_list expr_list statements
%type <ps> param_spec_item

%%

statements: statement		     {last_sql_result = $1; $$ = all_sql_results = g_list_append (NULL, $1);}
	| statement L_SC	     {last_sql_result = $1; $$ = all_sql_results = g_list_append (NULL, $1);}
	| statement L_SC statements  {last_sql_result = $1; $$ = all_sql_results = g_list_prepend ($3, $1);}
	| L_SC			     {last_sql_result = NULL; $$ = all_sql_results;}
	|			     {last_sql_result = NULL; $$ = all_sql_results;}
	;

statement: expr_list		     {$$ = gda_delimiter_statement_build (GDA_DELIMITER_UNKNOWN, $1); gda_delimiter_lex_reset ();}
	;

expr_list: expr           {$$ = g_list_prepend (NULL, $1);}
	| expr expr_list  {$$ = g_list_prepend ($2, $1);}
	| expr_list expr  {$$ = g_list_append ($1, $2);}
        ;

expr: L_TEXTUAL                   {$$ = gda_delimiter_expr_build (g_strdup_printf ("\"%s\"", $1), NULL); g_free ($1);}
        | L_STRING                {$$ = gda_delimiter_expr_build ($1, NULL);}
        | L_WORD                  {$$ = gda_delimiter_expr_build ($1, NULL);}
        | L_UNSPECVAL param_spec  {$$ = gda_delimiter_expr_build (NULL, $2);}
	| L_WORD param_spec	  {$$ = gda_delimiter_expr_build ($1, $2);}
	| L_STRING param_spec     {$$ = gda_delimiter_expr_build ($1, $2);}
	| L_TEXTUAL param_spec	  {$$ = gda_delimiter_expr_build (g_strdup_printf ("\"%s\"", $1), $2); g_free ($1);}
	| chunck		  {$$ = gda_delimiter_expr_build ($1, NULL);}
	| chunck param_spec	  {$$ = gda_delimiter_expr_build ($1, $2);}
        ;

chunck: L_CHAR			  {$$ = $1;}
	| chunck L_CHAR		  {$$ = g_strdup_printf ("%s%s", $1, $2); g_free ($1); g_free ($2);}
	;

param_spec: L_LSBRACKET param_spec_list L_RSBRACKET {$$ = $2;}
	;

param_spec_list: param_spec_item 			{$$ = g_list_append (NULL, $1);}
	| param_spec_item param_spec_list		{$$ = g_list_prepend ($2, $1);}
	;


param_spec_item: L_PNAME L_TEXTUAL      {$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NAME, $2);}
	| L_PDESCR L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_DESCR, $2);}
	| L_PTYPE L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_TYPE, $2);}
	| L_PISPARAM L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_ISPARAM, $2);}
	| L_PNULLOK L_TEXTUAL		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NULLOK, $2);}
	| L_PNAME L_STRING            	{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NAME, remove_quotes ($2));}
        | L_PDESCR L_STRING             {$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_DESCR, remove_quotes ($2));}
        | L_PTYPE L_STRING              {$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_TYPE, remove_quotes ($2));}
        | L_PISPARAM L_STRING           {$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_ISPARAM, remove_quotes ($2));}
        | L_PNULLOK L_STRING            {$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NULLOK, remove_quotes ($2));}
	| L_PNAME L_WORD      		{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NAME, $2);}
        | L_PDESCR L_WORD            	{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_DESCR, $2);}
        | L_PTYPE L_WORD             	{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_TYPE, $2);}
        | L_PISPARAM L_WORD          	{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_ISPARAM, $2);}
        | L_PNULLOK L_WORD           	{$$ = gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NULLOK, $2);}
	;
%%
