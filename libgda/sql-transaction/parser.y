%{
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "gda-sql-transaction-parser.h"
#include "gda-sql-transaction-tree.h"

int tranerror (char *);
int tranlex ();

extern GdaSqlTransaction *tran_result;
#define IUNK GDA_TRANSACTION_ISOLATION_UNKNOWN

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

%}

%union{
void            *v;
char            *str;
int              i;
}

%token L_STRING L_TEXTUAL L_IDENT
%token L_START L_BEGIN L_COMMIT L_ROLLBACK L_SAVEPOINT L_RELEASE
%token L_TRANSACTION L_ISOLATION L_LEVEL
%token L_SERIALIZABLE L_READ L_WRITE L_COMMITTED L_UNCOMMITTED L_REPEATABLE
%token L_END L_ONLY L_W_MARK
%token L_DEFERRED L_IMMEDIATE L_EXCLUSIVE
%token L_COMMENT L_FORCE L_TO

%type <v> statement 
%type <str> L_IDENT L_STRING L_TEXTUAL opt_trans_name
%type <i> opt_ilv opt_perm lock

%%
statement: 
	/* PostgreSQL */
          L_BEGIN opt_ilv opt_perm			{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_BEGIN, NULL, $2);}
	| L_BEGIN L_TRANSACTION	opt_ilv	opt_perm	{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_BEGIN, NULL, $3);}
	| L_START L_TRANSACTION opt_ilv opt_perm        {tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_BEGIN, NULL, $3);}
	| L_COMMIT L_TRANSACTION                        {tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_COMMIT, NULL, IUNK);}
	| L_ROLLBACK L_TRANSACTION                      {tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_ROLLBACK, NULL, IUNK);}
	| L_SAVEPOINT L_IDENT				{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_SAVEPOINT_ADD, $2, IUNK);}
	| L_ROLLBACK opt_trans_kw L_TO opt_svp_kw L_IDENT {tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_SAVEPOINT_ROLLBACK, $5, IUNK);}
	| L_RELEASE opt_svp_kw L_IDENT			{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_SAVEPOINT_REMOVE, $3, IUNK);}
	/* SQLite */
	| L_BEGIN lock opt_trans_name			{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_BEGIN, $3, IUNK);}
	| L_BEGIN opt_trans_name    			{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_BEGIN, $2, IUNK);}
	| L_COMMIT opt_trans_name			{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_COMMIT, $2, IUNK);}
	| L_END opt_trans_name                      	{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_COMMIT, $2, IUNK);}
	| L_ROLLBACK opt_trans_name			{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_ROLLBACK, $2, IUNK);}
	/* Sql Server */
	| L_BEGIN L_TRANSACTION L_IDENT L_W_MARK L_STRING {tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_BEGIN, $3, IUNK); g_free ($5);}
	/* Oracle */
	| L_COMMIT opt_trans_kw L_COMMENT L_STRING	{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_COMMIT, NULL, IUNK); g_free ($4);}
	| L_COMMIT opt_trans_kw L_FORCE L_STRING     	{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_COMMIT, NULL, IUNK); g_free ($4);}
	| L_ROLLBACK opt_trans_kw L_FORCE L_STRING	{tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_ROLLBACK, NULL, IUNK); g_free ($4);}
	| L_ROLLBACK opt_trans_kw L_TO opt_svp_kw L_STRING {tran_result = gda_sql_transaction_build (GDA_SQL_TRANSACTION_SAVEPOINT_ROLLBACK, remove_quotes ($5), IUNK);}
	;

opt_trans_kw:
	| L_TRANSACTION
	;
opt_svp_kw:
	| L_SAVEPOINT
	;

opt_trans_name:				{$$ = NULL;}
	| L_TRANSACTION L_IDENT			{$$ = $2;}
	;
	
opt_ilv:					{$$ = IUNK;}
	| L_ISOLATION L_LEVEL L_SERIALIZABLE	{$$ = GDA_TRANSACTION_ISOLATION_SERIALIZABLE;}
        | L_ISOLATION L_LEVEL L_READ L_COMMITTED {$$ = GDA_TRANSACTION_ISOLATION_READ_COMMITTED;}
        | L_ISOLATION L_LEVEL L_READ L_UNCOMMITTED {$$ = GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED;}
        | L_ISOLATION L_LEVEL L_REPEATABLE L_READ  {$$ = GDA_TRANSACTION_ISOLATION_REPEATABLE_READ;}
	;

opt_perm:					{$$ = 0;}
	| L_READ L_WRITE			{$$ = 0;}
	| L_READ L_ONLY			       	{$$ = 1;}
	;
lock:
	  L_DEFERRED				{$$ = 1;}
	| L_IMMEDIATE				{$$ = 2;}
	| L_EXCLUSIVE				{$$ = 3;}
	;
%%
