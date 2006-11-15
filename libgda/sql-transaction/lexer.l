%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "gda-sql-transaction-parser.h"

/* can conflict with L_SET defined in /usr/include/unistd.h */
#ifdef L_SET
#undef L_SET
#endif

#include "parser.h"

#warning "strings are limited to 1024 characters"
#define MAX_STR_CONST 1024
char string_buf[MAX_STR_CONST];
char *string_buf_ptr;
%}

%option noyywrap
%option yylineno
%option caseless
%x STRING
%x TEXTUAL
%%
"begin"		{return L_BEGIN;}
"start"		{return L_START;}
"commit"	{return L_COMMIT;}
"end"		{return L_END;}
"rollback"	{return L_ROLLBACK;}
"savepoint"	{return L_SAVEPOINT;}
"release"	{return L_RELEASE;}
"tran"		{return L_TRANSACTION;}
"transaction"	{return L_TRANSACTION;}
"work"		{return L_TRANSACTION;}

"isolation"	{return L_ISOLATION;}
"level"		{return L_LEVEL;}
"serializable"  {return L_SERIALIZABLE;}
"repeatable" 	{return L_REPEATABLE;}
"read"		{return L_READ;}
"committed"  	{return L_COMMITTED;}
"uncommitted" 	{return L_UNCOMMITTED;}
"write"		{return L_WRITE;}
"only"		{return L_ONLY;}

"deferred"	{return L_DEFERRED;}
"immediate"	{return L_IMMEDIATE;}
"exclusive"	{return L_EXCLUSIVE;}

"with mark"	{return L_W_MARK;}
"comment"	{return L_COMMENT;}
"force"		{return L_FORCE;}
"to"		{return L_TO;}

[a-zA-Z0-9_]+	{tranlval.str = g_strdup (trantext); return L_IDENT;}
[ \t\n\r]	{;}
\'		{string_buf_ptr = string_buf; BEGIN(STRING);}
<STRING>\\'     {*string_buf_ptr++='\\'; *string_buf_ptr++='\'';}
<STRING>\'	{BEGIN(INITIAL);
		 *string_buf_ptr = '\0';
		 tranlval.str = g_strdup_printf ("'%s'", string_buf);
		 return L_STRING;
		}
<STRING>\\n	{*string_buf_ptr++='\n';}
<STRING>\\t	{*string_buf_ptr++='\t';}
<STRING>\\r	{*string_buf_ptr++='\r';}
<STRING>\\b	{*string_buf_ptr++='\b';}
<STRING>\\f	{*string_buf_ptr++='\f';}
<STRING>\\(.|\n)	{*string_buf_ptr++=trantext[0];}
<STRING>.	{*string_buf_ptr++=trantext[0];}

\"		{string_buf_ptr = string_buf; BEGIN(TEXTUAL);}
<TEXTUAL>\"     {BEGIN(INITIAL);
                 *string_buf_ptr = '\0';
                 tranlval.str = g_strdup (string_buf);
                 return L_TEXTUAL;
                }
<TEXTUAL>\\n     {*string_buf_ptr++='\n';}
<TEXTUAL>\\t     {*string_buf_ptr++='\t';}
<TEXTUAL>\\r     {*string_buf_ptr++='\r';}
<TEXTUAL>\\b     {*string_buf_ptr++='\b';}
<TEXTUAL>\\f     {*string_buf_ptr++='\f';}
<TEXTUAL>\\(.|\n)        {*string_buf_ptr++=trantext[0];}
<TEXTUAL>.       {*string_buf_ptr++=trantext[0];}

.		 {;}
%%
