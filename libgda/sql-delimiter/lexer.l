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
static gint gda_delimiter_bloc_level;
static gboolean is_start = TRUE;
static gboolean ignore_next = FALSE;
static gchar *string_delim;
static gboolean can_use_is = TRUE;

void 
gda_delimiter_lex_reset ()
{
	is_start = TRUE;
	ignore_next = FALSE;
	gda_delimiter_bloc_level = 0;
	can_use_is = TRUE;
}

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

;			{if ((gda_delimiter_bloc_level == 0) && !ignore_next) {
				is_start = TRUE; 
				return L_SC;
			 } 
			 REJECT;
			}

[ ]*##[a-zA-Z\+\-][a-zA-Z0-9_\+\-/@#%]*               {gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_SIMPLE_PARAM;}
[ ]*##[a-zA-Z\+\-][a-zA-Z0-9_\+\-/@#%]*::[a-zA-Z0-9_]+ {gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_SIMPLE_PARAM;}
[ ]*##[a-zA-Z\+\-][a-zA-Z0-9_\+\-/@#%]*::[a-zA-Z0-9_]+::NULL {gda_delimiterlval.str = g_strdup (gda_delimitertext); return L_SIMPLE_PARAM;}
[ ]*##           	{return L_UNSPECVAL;}
[ ]*\/\*            	{BEGIN(PSPEC); return L_LSBRACKET;}
"declare"		{if (!is_start && (gda_delimiter_bloc_level == 0)) ignore_next = TRUE; REJECT;}
is[ \t]+not[ \t]+null	{can_use_is = FALSE; REJECT;}
is[ \t]+null   		{can_use_is = FALSE; REJECT;}
"is"			{if (!is_start && (gda_delimiter_bloc_level == 0) && can_use_is) ignore_next = TRUE; can_use_is = TRUE; REJECT;}
"begin"			{ignore_next = FALSE; 
			 if (!is_start)
				gda_delimiter_bloc_level ++; 
			 REJECT;
			}
"end;"			{if (gda_delimiter_bloc_level > 0)
				gda_delimiter_bloc_level --;
			 REJECT;
			}
--.*$			{/* Eat the comments up to the EOL */;}
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

\$[a-zA-Z0-9_\-]*\$	{string_delim = g_strdup (gda_delimitertext); gstring = g_string_new (""); BEGIN(STRING);}
\'			{gstring = g_string_new (""); BEGIN(STRING);}
<STRING,PSTRING>\\\\ 	{if (string_delim) /* so that \\' does not go into the \' state */
                                REJECT;
			 g_string_append (gstring, "\\\\");
			}
<STRING,PSTRING>\\' 	{if (string_delim)
                                REJECT;
			 g_string_append (gstring, "\\'");
			}
<STRING>\'		{
			 if (string_delim)
				REJECT;
			 BEGIN(INITIAL);
		 	 g_string_prepend_c (gstring, '\'');
		 	 g_string_append_c (gstring, '\'');
		 	 gda_delimiterlval.str = gstring->str;
		 	 g_string_free (gstring, FALSE);
		  	 return L_STRING;
			}
<STRING>\$[a-zA-Z0-9_\-]*\$ {
			 if (!string_delim || strcmp (string_delim, gda_delimitertext))
				REJECT;
			 BEGIN(INITIAL);
                         g_string_prepend (gstring, string_delim);
                         g_string_append (gstring, string_delim);
                         gda_delimiterlval.str = gstring->str;
                         g_string_free (gstring, FALSE);
			 g_free (string_delim);
			 string_delim = NULL;
                         return L_STRING;
			}
<PSTRING>\'     {BEGIN(PSPEC);
		 g_string_prepend_c (gstring, '\'');
                 g_string_append_c (gstring, '\'');
                 gda_delimiterlval.str = gstring->str;
		 g_string_free (gstring, FALSE);
                 return L_STRING;
                }
<STRING,PSTRING>\n	{g_string_append_c (gstring, '\n');}
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
<TEXTUAL,PTEXTUAL>\n     {g_string_append_c (gstring, '\n');}
<TEXTUAL,PTEXTUAL>.      {g_string_append_c (gstring, gda_delimitertext[0]);}
<TEXTUAL,PTEXTUAL><<EOF>> {
			gda_delimitererror (_("unterminated textual expression"));
			BEGIN(INITIAL);
                        yyterminate();
                        }

\n		{gda_delimiterlval.str = g_strdup ("\n"); return L_CHAR;}
\t           	{gda_delimiterlval.str = g_strdup ("\t"); return L_CHAR;}
" "		{gda_delimiterlval.str = g_strdup (" "); return L_CHAR;}
.               {gda_delimiterlval.str = g_strdup (gda_delimitertext); is_start = FALSE; return L_CHAR;}

%%
