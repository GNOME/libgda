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

static GString *gstring;

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
<PSPEC>\'		{gstring = g_string_new (""); BEGIN(PSTRING);}
<PSPEC>\"		{gstring = g_string_new (""); BEGIN(PTEXTUAL);}

\'		{gstring = g_string_new (""); BEGIN(STRING);}
<STRING,PSTRING>\\' {g_string_append (gstring, "\\'");}
<STRING>\'	{BEGIN(INITIAL);
		 g_string_prepend_c (gstring, '\'');
		 g_string_append_c (gstring, '\'');
		 gda_delimiterlval.str = gstring->str;
		 g_string_free (gstring, FALSE);
		 return L_STRING;
		}
<PSTRING>\'     {BEGIN(PSPEC);
		 g_string_prepend_c (gstring, '\'');
                 g_string_append_c (gstring, '\'');
                 gda_delimiterlval.str = gstring->str;
		 g_string_free (gstring, FALSE);
                 return L_STRING;
                }
<STRING,PSTRING>\\n	{g_string_append_c (gstring, '\n');}
<STRING,PSTRING>\\t	{g_string_append_c (gstring, '\t');}
<STRING,PSTRING>\\r	{g_string_append_c (gstring, '\r');}
<STRING,PSTRING>\\b	{g_string_append_c (gstring, '\b');}
<STRING,PSTRING>\\f	{g_string_append_c (gstring, '\f');}
<STRING,PSTRING>\\(.|\n) {g_string_append_c (gstring, gda_delimitertext[0]); g_string_append_c (gstring, gda_delimitertext[1]);}
<STRING,PSTRING>.	{g_string_append_c (gstring, gda_delimitertext[0]);}
<STRING,PSTRING><<EOF>> {
         		gda_delimitererror (_("unterminated string"));
			BEGIN(INITIAL);
         		yyterminate();
         		}

\"		{gstring = g_string_new (""); BEGIN(TEXTUAL);}
<TEXTUAL>\"     {BEGIN(INITIAL);
		 gda_delimiterlval.str = gstring->str;
		 g_string_free (gstring, FALSE);
                 return L_TEXTUAL;
                }
<PTEXTUAL>\"    {BEGIN(PSPEC);
		 gda_delimiterlval.str = gstring->str;
                 g_string_free (gstring, FALSE);
                 return L_TEXTUAL;
                }
<TEXTUAL,PTEXTUAL>\\n     {g_string_append_c (gstring, '\n');}
<TEXTUAL,PTEXTUAL>\\t     {g_string_append_c (gstring, '\t');}
<TEXTUAL,PTEXTUAL>\\r     {g_string_append_c (gstring, '\r');}
<TEXTUAL,PTEXTUAL>\\b     {g_string_append_c (gstring, '\b');}
<TEXTUAL,PTEXTUAL>\\f     {g_string_append_c (gstring, '\f');}
<TEXTUAL,PTEXTUAL>\\(.|\n) {g_string_append_c (gstring, gda_delimitertext[0]); g_string_append_c (gstring, gda_delimitertext[1]);}
<TEXTUAL,PTEXTUAL>.       {g_string_append_c (gstring, gda_delimitertext[0]);}
<TEXTUAL,PTEXTUAL><<EOF>> {
			gda_delimitererror (_("unterminated textual expression"));
			BEGIN(INITIAL);
                        yyterminate();
                        }

\\n		{gda_delimiterlval.str = g_strdup ("\n"); return L_CHAR;}
.               {gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_CHAR;}

%%
