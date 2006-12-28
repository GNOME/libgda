%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "sql_parser.h"

/* can conflict with L_SET defined in /usr/include/unistd.h */
#ifdef L_SET
#undef L_SET
#endif

#include "parser.h"
#include "mem.h"

/* Remove this warning, because it breaks the build with -Werror: #warning "strings are limited to 2048 characters" */
#define MAX_STR_CONST 2048
char string_buf[MAX_STR_CONST];
char *string_buf_ptr;
%}

%option noyywrap
%option yylineno
%option caseless
%x STRING
%x TEXTUAL
%%
"select"	{return L_SELECT;}
"insert"	{return L_INSERT;}
"update" 	{return L_UPDATE;}
"delete"        {return L_DELETE;}
"from"		{return L_FROM;}
"where"		{return L_WHERE;}
"order"		{return L_ORDER;}
"asc"		{return L_ORDER_ASC;}
"desc"		{return L_ORDER_DSC;}
"distinct"	{return L_DISTINCT;}
"between"	{return L_BETWEEN;}
"group"		{return L_GROUP;}
"into"		{return L_INTO;}
"values"	{return L_VALUES;}
"as"		{return L_AS;}
"join"		{return L_JOIN;}
"inner"         {return L_INNER;}
"left"		{return L_LEFT;}
"right"       	{return L_RIGHT;}
"full"		{return L_FULL;}
"outer"		{return L_OUTER;}
"like"		{return L_LIKE;}
"set"		{return L_SET;}
"on"		{return L_ON;}
"by"		{return L_BY;}
"in"		{return L_IN;}

"null"		{return L_NULL;}

"not"		{return L_NOT;}
"and"		{return L_AND;}
"or"		{return L_OR;}

"!="            {return L_DIFF;}
"<>"		{return L_DIFF;}
">="		{return L_GEQ;}
"<="		{return L_LEQ;}
">"		{return L_GT;}
"<"		{return L_LT;}
"="		{return L_EQ;}
"is"		{return L_IS;}
"~"		{return L_REGEXP;}
"~*"		{return L_REGEXP_CI;}
"!~"            {return L_NOTREGEXP;}
"!~*"           {return L_NOTREGEXP_CI;}
"SIMILAR TO"	{return L_SIMILAR;}

"."		{return L_DOT;}
"*"		{return L_TIMES;}
","		{return L_COMMA;}
"("		{return L_LBRACKET;}
")"		{return L_RBRACKET;}

"-"		{return L_MINUS;}
"+"		{return L_PLUS;}
"/"		{return L_DIV;}

[0-9\.]+	{sqllval.str = memsql_strdup (sqltext); return L_NUM;}
[a-zA-Z0-9_]+	{sqllval.str = memsql_strdup (sqltext); return L_IDENT;}
[ \t\n\r]	{;}
\'		{string_buf_ptr = string_buf; BEGIN(STRING);}
<STRING>\\'     {*string_buf_ptr++='\\'; *string_buf_ptr++='\'';}
<STRING>\'		{BEGIN(INITIAL);
		 *string_buf_ptr = '\0';
		 sqllval.str = memsql_strappend_free (memsql_strdup ("'"), memsql_strappend_free (memsql_strdup (string_buf), memsql_strdup ("'")));
		 return L_STRING;
		}
<STRING>\\n	{*string_buf_ptr++='\n';}
<STRING>\\t	{*string_buf_ptr++='\t';}
<STRING>\\r	{*string_buf_ptr++='\r';}
<STRING>\\b	{*string_buf_ptr++='\b';}
<STRING>\\f	{*string_buf_ptr++='\f';}
<STRING>\\(.|\n)	{*string_buf_ptr++=sqltext[0];}
<STRING>.	{*string_buf_ptr++=sqltext[0];}

"["		{return L_LSBRACKET;}
"]"             {return L_RSBRACKET;}

":name"		{return L_PNAME;}
":descr"        {return L_PDESCR;}
":type"		{return L_PTYPE;}
":isparam"	{return L_PISPARAM;}
":nullok"       {return L_PNULLOK;}
"##"            {return L_UNSPECVAL;}

\"		{string_buf_ptr = string_buf; BEGIN(TEXTUAL);}
<TEXTUAL>\"              {BEGIN(INITIAL);
                 *string_buf_ptr = '\0';
                 sqllval.str = memsql_strdup (string_buf);
                 return L_TEXTUAL;
                }
<TEXTUAL>\\n     {*string_buf_ptr++='\n';}
<TEXTUAL>\\t     {*string_buf_ptr++='\t';}
<TEXTUAL>\\r     {*string_buf_ptr++='\r';}
<TEXTUAL>\\b     {*string_buf_ptr++='\b';}
<TEXTUAL>\\f     {*string_buf_ptr++='\f';}
<TEXTUAL>\\(.|\n)        {*string_buf_ptr++=sqltext[0];}
<TEXTUAL>.       {*string_buf_ptr++=sqltext[0];}

.		 {;}
%%
