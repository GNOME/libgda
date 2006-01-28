%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "gda-sql-delimiter.h"

/* can conflict with L_SET defined in /usr/include/unistd.h */
#ifdef L_SET
#undef L_SET
#endif

#include "parser.h"

#warning "strings are limited to 2048 characters"
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

		BEGIN(INITIAL);

"select"	{return L_SELECT;}
"insert"	{return L_INSERT;}
"update" 	{return L_UPDATE;}
"delete"        {return L_DELETE;}
"="		{return L_EQ;}

[a-zA-Z0-9_,+-/|().!~*=]+	{gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_IDENT;}
[ \t\n\r]	{;}
\'		{string_buf_ptr = string_buf; BEGIN(STRING);}
<STRING>\\'     {*string_buf_ptr++='\\'; *string_buf_ptr++='\'';}
<STRING>\'		{BEGIN(INITIAL);
		 *string_buf_ptr = '\0';
		 gda_delimiterlval.str = g_strdup_printf ("'%s'", string_buf);
		 return L_STRING;
		}
<STRING>\\n	{*string_buf_ptr++='\n';}
<STRING>\\t	{*string_buf_ptr++='\t';}
<STRING>\\r	{*string_buf_ptr++='\r';}
<STRING>\\b	{*string_buf_ptr++='\b';}
<STRING>\\f	{*string_buf_ptr++='\f';}
<STRING>\\(.|\n)	{*string_buf_ptr++=gda_delimitertext[0];}
<STRING>.	{*string_buf_ptr++=gda_delimitertext[0];}

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
                 gda_delimiterlval.str = g_strdup (string_buf);
                 return L_TEXTUAL;
                }
<TEXTUAL>\\n     {*string_buf_ptr++='\n';}
<TEXTUAL>\\t     {*string_buf_ptr++='\t';}
<TEXTUAL>\\r     {*string_buf_ptr++='\r';}
<TEXTUAL>\\b     {*string_buf_ptr++='\b';}
<TEXTUAL>\\f     {*string_buf_ptr++='\f';}
<TEXTUAL>\\(.|\n)        {*string_buf_ptr++=gda_delimitertext[0];}
<TEXTUAL>.       {*string_buf_ptr++=gda_delimitertext[0];}

.		 {;}

%%
