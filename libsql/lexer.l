%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "sql_parser.h"
#include "parser.h"
#include "mem.h"

#warning "strings are limited to 2000 characters"
#define MAX_STR_CONST 2000
char string_buf[MAX_STR_CONST];
char *string_buf_ptr;
%}

%option noyywrap
%option yylineno
%option caseless
%x STRING
%%
"select"	{return L_SELECT;}
"insert"	{return L_INSERT;}
"update" {return L_UPDATE;}
"from"		{return L_FROM;}
"where"		{return L_WHERE;}
"order"		{return L_ORDER;}
"distinct"	{return L_DISTINCT;}
"between"	{return L_BETWEEN;}
"group"		{return L_GROUP;}
"into"		{return L_INTO;}
"values"	{return L_VALUES;}
"as"		{return L_AS;}
"join"		{return L_JOIN;}
"like"		{return L_LIKE;}
"set"			{return L_SET;}
"on"		{return L_ON;}
"by"		{return L_BY;}
"in"		{return L_IN;}

"null"		{return L_NULL;}

"not"		{return L_NOT;}
"and"		{return L_AND;}
"or"		{return L_OR;}

"="		{return L_EQ;}
"is"		{return L_IS;}

"."		{return L_DOT;}
"*"		{return L_STAR;}
","		{return L_COMMA;}
"("		{return L_LBRACKET;}
")"		{return L_RBRACKET;}

"-"		{return L_MINUS;}
"+"		{return L_PLUS;}
"*"		{return L_TIMES;}
"/"		{return L_DIV;}

[a-zA-Z0-9]+	{sqllval.str = memsql_strdup (sqltext); return L_IDENT;}
[ \t\n\r]	{;}
\'		{string_buf_ptr = string_buf; BEGIN(STRING);}
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
%%
