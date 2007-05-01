%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "sql_parser.h"

/* can conflict with L_SET defined in /usr/include/unistd.h */
#ifdef L_SET
#undef L_SET
#endif

#include "parser.h"
#include "mem.h"

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
\'		{gstring = g_string_new (""); BEGIN(STRING);}
<STRING,PSTRING>\\\\ {g_string_append (gstring, "\\\\");}
<STRING,PSTRING>\\' {g_string_append (gstring, "\\'");}
<STRING>\'	{BEGIN(INITIAL);
		 sqllval.str = memsql_strappend_free (memsql_strdup ("'"), 
				memsql_strappend_free (memsql_strdup (gstring->str), memsql_strdup ("'")));
		 g_string_free (gstring, TRUE);
		 return L_STRING;
		}
<PSTRING>\'     {BEGIN(PSPEC);
                 sqllval.str = memsql_strappend_free (memsql_strdup ("'"), 
                                memsql_strappend_free (memsql_strdup (gstring->str), memsql_strdup ("'")));
		 g_string_free (gstring, TRUE);
                 return L_STRING;
                }
<STRING,PSTRING>\n     	{g_string_append_c (gstring, '\n');}
<STRING,PSTRING>.	{g_string_append_c (gstring, sqltext[0]);}
<STRING,PSTRING><<EOF>> {
                        sqlerror (_("unterminated string"));
			BEGIN(INITIAL);
                        yyterminate();
                        }

"##"            	{return L_UNSPECVAL;}
"/*"			{BEGIN(PSPEC); return L_LSBRACKET;}
<PSPEC>"*/"             {BEGIN(INITIAL); return L_RSBRACKET;}
<PSPEC>"name:"		{return L_PNAME;}
<PSPEC>"descr:"         {return L_PDESCR;}
<PSPEC>"type:"		{return L_PTYPE;}
<PSPEC>"isparam:"	{return L_PISPARAM;}
<PSPEC>"nullok:"        {return L_PNULLOK;}
<PSPEC>[a-zA-Z0-9_\-\+]+ {sqllval.str = memsql_strdup (sqltext); return L_IDENT;}
<PSPEC>[ \t\n\r]        {;}
<PSPEC>\'               {gstring = g_string_new (""); BEGIN(PSTRING);}
<PSPEC>\"               {gstring = g_string_new (""); BEGIN(PTEXTUAL);}

\"		{gstring = g_string_new (""); BEGIN(TEXTUAL);}
<TEXTUAL>\"     {BEGIN(INITIAL);
                 sqllval.str = memsql_strdup (gstring->str);
		 g_string_free (gstring, TRUE);
                 return L_TEXTUAL;
                }
<PTEXTUAL>\"    {BEGIN(PSPEC);
                 sqllval.str = memsql_strdup (gstring->str);
		 g_string_free (gstring, TRUE);
                 return L_TEXTUAL;
                }
<TEXTUAL,PTEXTUAL>.       {g_string_append_c (gstring, sqltext[0]);}
<TEXTUAL,PTEXTUAL><<EOF>> {
                        sqlerror (_("unterminated textual expression"));
			BEGIN(INITIAL);
                        yyterminate();
                        }

;		{;}
.		 {gchar *str;
		  str = g_strdup_printf (_("Unknown char '%s'"), sqltext);
		  sqlerror (str);
		  g_free (str);
		  yyterminate();
		 }
%%
