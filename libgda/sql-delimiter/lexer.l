%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "gda-sql-delimiter.h"

/* can conflict with L_SET defined in /usr/include/unistd.h */
#ifdef L_SET
#undef L_SET
#endif

#include "parser.h"

/* Remove this warning, because it breaks the build with -Werror: #warning "strings are limited to 2048 characters" */
#define MAX_STR_CONST 2048
static char string_buf[MAX_STR_CONST];
static char *string_buf_ptr;
%}

%option noyywrap
%option yylineno
%option caseless
%x STRING
%x TEXTUAL
%x PSPEC
%x PSTRING
%x PTEXTUAL

%%

;		{return L_SC;}
[ ]*##           {return L_UNSPECVAL;}
[ ]*\/\*            {BEGIN(PSPEC); return L_LSBRACKET;}

<PSPEC>"*/"             {BEGIN(INITIAL); return L_RSBRACKET;}
<PSPEC>"name:"        	{return L_PNAME;}
<PSPEC>"descr:"       	{return L_PDESCR;}
<PSPEC>"type:"        	{return L_PTYPE;}
<PSPEC>"isparam:"     	{return L_PISPARAM;}
<PSPEC>"nullok:"      	{return L_PNULLOK;}
<PSPEC>[a-zA-Z0-9_\-\+]+    {gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_WORD;}
<PSPEC>[ \t\n\r]        {;}
<PSPEC>\'		{string_buf_ptr = string_buf; BEGIN(PSTRING);}
<PSPEC>\"		{string_buf_ptr = string_buf; BEGIN(PTEXTUAL);}

\'		{string_buf_ptr = string_buf; BEGIN(STRING);}
<STRING,PSTRING>\\' {*string_buf_ptr++='\\'; *string_buf_ptr++='\'';}
<STRING>\'	{BEGIN(INITIAL);
		 *string_buf_ptr = '\0';
		 gda_delimiterlval.str = g_strdup_printf ("'%s'", string_buf);
		 return L_STRING;
		}
<PSTRING>\'     {BEGIN(PSPEC);
                 *string_buf_ptr = '\0';
                 gda_delimiterlval.str = g_strdup_printf ("'%s'", string_buf);
                 return L_STRING;
                }
<STRING,PSTRING>\\n	{*string_buf_ptr++='\n';}
<STRING,PSTRING>\\t	{*string_buf_ptr++='\t';}
<STRING,PSTRING>\\r	{*string_buf_ptr++='\r';}
<STRING,PSTRING>\\b	{*string_buf_ptr++='\b';}
<STRING,PSTRING>\\f	{*string_buf_ptr++='\f';}
<STRING,PSTRING>\\(.|\n) {*string_buf_ptr++=gda_delimitertext[0]; *string_buf_ptr++=gda_delimitertext[1];}
<STRING,PSTRING>.	{*string_buf_ptr++=gda_delimitertext[0];}
<STRING,PSTRING><<EOF>> {
         		gda_delimitererror (_("unterminated string"));
			BEGIN(INITIAL);
         		yyterminate();
         		}

\"		{string_buf_ptr = string_buf; BEGIN(TEXTUAL);}
<TEXTUAL>\"     {BEGIN(INITIAL);
                 *string_buf_ptr = '\0';
                 gda_delimiterlval.str = g_strdup (string_buf);
                 return L_TEXTUAL;
                }
<PTEXTUAL>\"    {BEGIN(PSPEC);
                 *string_buf_ptr = '\0';
                 gda_delimiterlval.str = g_strdup (string_buf);
                 return L_TEXTUAL;
                }
<TEXTUAL,PTEXTUAL>\\n     {*string_buf_ptr++='\n';}
<TEXTUAL,PTEXTUAL>\\t     {*string_buf_ptr++='\t';}
<TEXTUAL,PTEXTUAL>\\r     {*string_buf_ptr++='\r';}
<TEXTUAL,PTEXTUAL>\\b     {*string_buf_ptr++='\b';}
<TEXTUAL,PTEXTUAL>\\f     {*string_buf_ptr++='\f';}
<TEXTUAL,PTEXTUAL>\\(.|\n) {*string_buf_ptr++=gda_delimitertext[0]; *string_buf_ptr++=gda_delimitertext[1];}
<TEXTUAL,PTEXTUAL>.       {*string_buf_ptr++=gda_delimitertext[0];}
<TEXTUAL,PTEXTUAL><<EOF>> {
			gda_delimitererror (_("unterminated textual expression"));
			BEGIN(INITIAL);
                        yyterminate();
                        }

\\n		{gda_delimiterlval.str = g_strdup ("\n"); return L_CHAR;}
.               {gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_CHAR;}

%%
