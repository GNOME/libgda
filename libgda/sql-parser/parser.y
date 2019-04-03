// All token codes are small integers with #defines that begin with "TK_"
%token_prefix T_

// The type of the data attached to each token is GValue.  This is also the
// default type for non-terminals.
//
%token_type {GValue *}
%default_type {GValue *}
%token_destructor {if ($$) {
#ifdef GDA_DEBUG_NO
		 gchar *str = gda_sql_value_stringify ($$);
		 g_print ("___ token destructor /%s/\n", str)
		 g_free (str);
#endif
		 g_value_unset ($$); g_free ($$);}}

// The generated parser function takes a 4th argument as follows:
%extra_argument {GdaSqlParserIface *pdata}

// This code runs whenever there is a syntax error
//
%syntax_error {
	gda_sql_parser_set_syntax_error (pdata->parser);
}
%stack_overflow {
	gda_sql_parser_set_overflow_error (pdata->parser);
}

// The name of the generated procedure that implements the parser
// is as follows:
%name priv_gda_sql_parser

// The following text is included near the beginning of the C source
// code file that implements the parser.
//
%include {
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <libgda/sql-parser/gda-sql-parser-private.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-trans.h>
#include <libgda/sql-parser/gda-statement-struct-insert.h>
#include <libgda/sql-parser/gda-statement-struct-update.h>
#include <libgda/sql-parser/gda-statement-struct-delete.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-compound.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>
#include <assert.h>

typedef struct {
	GValue *fname;
	GdaSqlExpr *expr;
} UpdateSet;

typedef struct {
	gboolean    distinct;
	GdaSqlExpr *expr;
} Distinct;

typedef struct {
	GdaSqlExpr *count;
	GdaSqlExpr *offset;
} Limit;

typedef struct {
	GSList *when_list;
	GSList *then_list;
} CaseBody;

static GdaSqlOperatorType
sql_operation_string_to_operator (const gchar *op)
{
	switch (g_ascii_toupper (*op)) {
	case 'A':
		return GDA_SQL_OPERATOR_TYPE_AND;
	case 'O':
		return GDA_SQL_OPERATOR_TYPE_OR;
	case 'N':
		return GDA_SQL_OPERATOR_TYPE_NOT;
	case '=':
		return GDA_SQL_OPERATOR_TYPE_EQ;
	case 'I':
		if (op[1] == 'S')
			return GDA_SQL_OPERATOR_TYPE_IS;
		else if (op[1] == 'N')
			return GDA_SQL_OPERATOR_TYPE_IN;
		else if (op[1] == 'I')
			return GDA_SQL_OPERATOR_TYPE_ILIKE;
		break;
	case 'L':
		return GDA_SQL_OPERATOR_TYPE_LIKE;
	case 'B':
		return GDA_SQL_OPERATOR_TYPE_BETWEEN;
	case '>':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_TYPE_GEQ;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_TYPE_GT;
		break;
	case '<':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_TYPE_LEQ;
		else if (op[1] == '>')
			return GDA_SQL_OPERATOR_TYPE_DIFF;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_TYPE_LT;
		break;
	case '!':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_TYPE_DIFF;
		else if (op[1] == '~') {
			if (op[2] == 0)
				return GDA_SQL_OPERATOR_TYPE_NOT_REGEXP;
			else if (op[2] == '*')
				return GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI;
		}
		break;
	case '~':
		if (op[1] == '*')
			return GDA_SQL_OPERATOR_TYPE_REGEXP_CI;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_TYPE_REGEXP;
		break;
	case 'S':
		return GDA_SQL_OPERATOR_TYPE_SIMILAR;
	case '|':
		if (op[1] == '|')
			return GDA_SQL_OPERATOR_TYPE_CONCAT;
		else
			return GDA_SQL_OPERATOR_TYPE_BITOR;
	case '+':
		return GDA_SQL_OPERATOR_TYPE_PLUS;
	case '-':
		return GDA_SQL_OPERATOR_TYPE_MINUS;
	case '*':
		return GDA_SQL_OPERATOR_TYPE_STAR;
	case '/':
		return GDA_SQL_OPERATOR_TYPE_DIV;
	case '%':
		return GDA_SQL_OPERATOR_TYPE_REM;
	case '&':
		return GDA_SQL_OPERATOR_TYPE_BITAND;
	}
	g_error ("Unhandled operator named '%s'\n", op);
	return 0;
}

static GdaSqlOperatorType
string_to_op_type (GValue *value)
{
	GdaSqlOperatorType op;
	op = sql_operation_string_to_operator (g_value_get_string (value));
	g_value_reset (value);
	g_free (value);
	return op;
}

static GdaSqlExpr *
compose_multiple_expr (GdaSqlOperatorType op, GdaSqlExpr *left, GdaSqlExpr *right) {
	GdaSqlExpr *ret;
	if (left->cond && (left->cond->operator_type == op)) {
		ret = left;
		ret->cond->operands = g_slist_append (ret->cond->operands, right);
	}
	else {
		GdaSqlOperation *cond;
		ret = gda_sql_expr_new (NULL);
		cond = gda_sql_operation_new (GDA_SQL_ANY_PART (ret));
		ret->cond = cond;
		cond->operator_type = op;
		cond->operands = g_slist_prepend (NULL, right);
		GDA_SQL_ANY_PART (right)->parent = GDA_SQL_ANY_PART (cond);
		cond->operands = g_slist_prepend (cond->operands, left);
		GDA_SQL_ANY_PART (left)->parent = GDA_SQL_ANY_PART (cond);
	}
	return ret;
}

static GdaSqlExpr *
create_two_expr (GdaSqlOperatorType op, GdaSqlExpr *left, GdaSqlExpr *right) {
	GdaSqlExpr *ret;
	GdaSqlOperation *cond;
	ret = gda_sql_expr_new (NULL);
	cond = gda_sql_operation_new (GDA_SQL_ANY_PART (ret));
	ret->cond = cond;
	cond->operator_type = op;
	cond->operands = g_slist_prepend (NULL, right);
	GDA_SQL_ANY_PART (right)->parent = GDA_SQL_ANY_PART (cond);
	cond->operands = g_slist_prepend (cond->operands, left);
	GDA_SQL_ANY_PART (left)->parent = GDA_SQL_ANY_PART (cond);
	return ret;
}

static GdaSqlExpr *
create_uni_expr (GdaSqlOperatorType op, GdaSqlExpr *expr) {
	GdaSqlExpr *ret;
	GdaSqlOperation *cond;
	ret = gda_sql_expr_new (NULL);
	cond = gda_sql_operation_new (GDA_SQL_ANY_PART (ret));
	ret->cond = cond;
	cond->operator_type = op;
	cond->operands = g_slist_prepend (NULL, expr);
	GDA_SQL_ANY_PART (expr)->parent = GDA_SQL_ANY_PART (cond);
	return ret;
}

static GdaSqlStatement *
compose_multiple_compounds (GdaSqlStatementCompoundType ctype, GdaSqlStatement *left, GdaSqlStatement *right) {
	GdaSqlStatement *ret = NULL;
	GdaSqlStatementCompound *lc = (GdaSqlStatementCompound*) left->contents;
	if (lc->compound_type == ctype) {
		GdaSqlStatementCompound *rc = (GdaSqlStatementCompound*) right->contents;
		if (!rc->stmt_list->next || rc->compound_type == ctype) {
			GSList *list;
			for (list = rc->stmt_list; list; list = list->next)
				GDA_SQL_ANY_PART (((GdaSqlStatement*)list->data)->contents)->parent = GDA_SQL_ANY_PART (lc);

			ret = left;
			lc->stmt_list = g_slist_concat (lc->stmt_list, rc->stmt_list);
			rc->stmt_list = NULL;
			gda_sql_statement_free (right);
		}
	}
	else {
		ret = gda_sql_statement_new (GDA_SQL_STATEMENT_COMPOUND);
		gda_sql_statement_compound_set_type (ret, ctype);
		gda_sql_statement_compound_take_stmt (ret, left);
		gda_sql_statement_compound_take_stmt (ret, right);
	}
	return ret;
}

}

// The following directive causes tokens ABORT, AFTER, ASC, etc. to
// fallback to ID if they will not parse as their original value.
%fallback ID
  ABORT AFTER ANALYZE ASC ATTACH BEFORE BEGIN CASCADE CAST CONFLICT
  DATABASE DEFERRED DESC DETACH EACH END EXCLUSIVE EXPLAIN FAIL FOR
  IGNORE IMMEDIATE INITIALLY INSTEAD LIKE ILIKE MATCH PLAN
  QUERY KEY OF OFFSET PRAGMA RAISE REPLACE RESTRICT ROW
  TEMP TRIGGER VACUUM VIEW VIRTUAL
  REINDEX RENAME CTIME_KW IF
  DELIMITER COMMIT ROLLBACK ISOLATION LEVEL SERIALIZABLE READ COMMITTED 
  UNCOMMITTED REPEATABLE WRITE ONLY SAVEPOINT RELEASE COMMENT FORCE WAIT NOWAIT BATCH.
%fallback TEXTUAL STRING.

// Define operator precedence early so that this is the first occurance
// of the operator tokens in the grammer.  Keeping the operators together
// causes them to be assigned integer values that are close together,
// which keeps parser tables smaller.
%left OR.
%left AND.
%right NOT.
%left IS MATCH NOTLIKE LIKE NOTILIKE ILIKE IN ISNULL NOTNULL DIFF EQ.
%left BETWEEN.
%left GT LEQ LT GEQ.
%left REGEXP REGEXP_CI NOT_REGEXP NOT_REGEXP_CI.
%left SIMILAR.
%right ESCAPE.
%left BITAND BITOR LSHIFT RSHIFT.
%left PLUS MINUS.
%left STAR SLASH REM.
%left CONCAT.
%left COLLATE.
%right UMINUS UPLUS BITNOT.
%left LP RP.
%left JOIN INNER NATURAL LEFT RIGHT FULL CROSS.
%left UNION EXCEPT INTERSECT.
%left PGCAST.

// force the declaration of the ILLEGAL and SQLCOMMENT tokens
%nonassoc ILLEGAL.
%nonassoc SQLCOMMENT.

// Input is a single SQL command
%type stmt {GdaSqlStatement *}
%destructor stmt {g_print ("Statement destroyed by parser: %p\n", $$); gda_sql_statement_free ($$);}
stmt ::= cmd(C) eos. {pdata->parsed_statement = C;}
stmt ::= compound(C) eos. {
	GdaSqlStatementCompound *scompound = (GdaSqlStatementCompound *) C->contents;
	if (scompound->stmt_list->next)
		/* real compound (multiple statements) */
		pdata->parsed_statement = C;
	else {
		/* false compound (only 1 select) */
		pdata->parsed_statement = (GdaSqlStatement*) scompound->stmt_list->data;
		GDA_SQL_ANY_PART (pdata->parsed_statement->contents)->parent = NULL;
		g_slist_free (scompound->stmt_list);
		scompound->stmt_list = NULL;
		gda_sql_statement_free (C);
	}
}
cmd(C) ::= LP cmd(E) RP. {C = E;}
compound(C) ::= LP compound(E) RP. {C = E;}

eos ::= SEMI.
eos ::= END_OF_FILE.

%type cmd {GdaSqlStatement *}
%destructor cmd {gda_sql_statement_free ($$);}

//
// Transactions
//
cmd(C) ::= BEGIN. {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);}
cmd(C) ::= BEGIN TRANSACTION nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
					 gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= BEGIN transtype(Y) TRANSACTION nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
						      gda_sql_statement_trans_take_mode (C, Y);
						      gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= BEGIN transtype(Y) nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
					  gda_sql_statement_trans_take_mode (C, Y);
					  gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= BEGIN transilev(L). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
				gda_sql_statement_trans_set_isol_level (C, L);
}

cmd(C) ::= BEGIN TRANSACTION transilev(L). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
					    gda_sql_statement_trans_set_isol_level (C, L);
}

cmd(C) ::= BEGIN TRANSACTION transtype(Y). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
					    gda_sql_statement_trans_take_mode (C, Y);
}

cmd(C) ::= BEGIN TRANSACTION transtype(Y) opt_comma transilev(L). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
								   gda_sql_statement_trans_take_mode (C, Y);
								   gda_sql_statement_trans_set_isol_level (C, L);
}

cmd(C) ::= BEGIN TRANSACTION transilev(L) opt_comma transtype(Y). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
								   gda_sql_statement_trans_take_mode (C, Y);
								   gda_sql_statement_trans_set_isol_level (C, L);
}

cmd(C) ::= BEGIN transtype(Y) opt_comma transilev(L). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
						       gda_sql_statement_trans_take_mode (C, Y);
						       gda_sql_statement_trans_set_isol_level (C, L);
}
	
cmd(C) ::= BEGIN transilev(L) opt_comma transtype(Y). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_BEGIN);
						       gda_sql_statement_trans_take_mode (C, Y);
						       gda_sql_statement_trans_set_isol_level (C, L);
}

cmd(C) ::= END trans_opt_kw nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);
					gda_sql_statement_trans_take_name (C, R);
}
	
cmd(C) ::= COMMIT nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);
			      gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= COMMIT TRANSACTION nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);
					  gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= COMMIT FORCE STRING. {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);}
cmd(C) ::= COMMIT FORCE STRING COMMA INTEGER. {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);}
cmd(C) ::= COMMIT COMMENT STRING. {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);}
cmd(C) ::= COMMIT COMMENT STRING ora_commit_write. {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);}
cmd(C) ::= COMMIT ora_commit_write. {C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMMIT);}

cmd(C) ::= ROLLBACK trans_opt_kw nm_opt(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_ROLLBACK);
					     gda_sql_statement_trans_take_name (C, R);
}

ora_commit_write ::= WRITE IMMEDIATE.
ora_commit_write ::= WRITE BATCH.
ora_commit_write ::= WRITE WAIT.
ora_commit_write ::= WRITE NOWAIT.
ora_commit_write ::= WRITE IMMEDIATE WAIT.
ora_commit_write ::= WRITE IMMEDIATE NOWAIT.
ora_commit_write ::= WRITE BATCH WAIT.
ora_commit_write ::= WRITE BATCH NOWAIT.

trans_opt_kw ::= .
trans_opt_kw ::= TRANSACTION.

opt_comma ::= .
opt_comma ::= COMMA.

%type transilev {GdaTransactionIsolation}
transilev(L) ::= ISOLATION LEVEL SERIALIZABLE. {L = GDA_TRANSACTION_ISOLATION_SERIALIZABLE;}
transilev(L) ::= ISOLATION LEVEL REPEATABLE READ. {L = GDA_TRANSACTION_ISOLATION_REPEATABLE_READ;}
transilev(L) ::= ISOLATION LEVEL READ COMMITTED. {L = GDA_TRANSACTION_ISOLATION_READ_COMMITTED;}
transilev(L) ::= ISOLATION LEVEL READ UNCOMMITTED. {L = GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED;}

nm_opt(R) ::= . {R = NULL;}
nm_opt(R) ::= nm(N). {R = N;}

transtype(A) ::= DEFERRED(X).  {A = X;}
transtype(A) ::= IMMEDIATE(X). {A = X;}
transtype(A) ::= EXCLUSIVE(X). {A = X;}
transtype(A) ::= READ WRITE. {A = g_new0 (GValue, 1);
			      g_value_init (A, G_TYPE_STRING);
			      g_value_set_string (A, "READ_WRITE");
}
transtype(A) ::= READ ONLY. {A = g_new0 (GValue, 1);
			     g_value_init (A, G_TYPE_STRING);
			     g_value_set_string (A, "READ_ONLY");
}

//
// Savepoints
//
cmd(C) ::= SAVEPOINT nm(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_SAVEPOINT);
				    gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= RELEASE SAVEPOINT nm(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE_SAVEPOINT);
				     gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= RELEASE nm(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE_SAVEPOINT);
			   gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= ROLLBACK trans_opt_kw TO nm(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT);
					    gda_sql_statement_trans_take_name (C, R);
}

cmd(C) ::= ROLLBACK trans_opt_kw TO SAVEPOINT nm(R). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT);
						      gda_sql_statement_trans_take_name (C, R);
}

//
// INSERT
//
cmd(C) ::= INSERT opt_on_conflict(O) INTO fullname(X) inscollist_opt(F) VALUES LP rexprlist(Y) RP. {
	C = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
	gda_sql_statement_insert_take_table_name (C, X);
	gda_sql_statement_insert_take_fields_list (C, F);
	gda_sql_statement_insert_take_1_values_list (C, g_slist_reverse (Y));
	gda_sql_statement_insert_take_on_conflict (C, O);
}

cmd(C) ::= INSERT opt_on_conflict(O) INTO fullname(X) inscollist_opt(F) VALUES LP rexprlist(Y) RP ins_extra_values(E). {
	C = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
	gda_sql_statement_insert_take_table_name (C, X);
	gda_sql_statement_insert_take_fields_list (C, F);
	gda_sql_statement_insert_take_1_values_list (C, g_slist_reverse (Y));
	gda_sql_statement_insert_take_extra_values_list (C, E);
	gda_sql_statement_insert_take_on_conflict (C, O);
}

cmd(C) ::= INSERT opt_on_conflict(O) INTO fullname(X) inscollist_opt(F) compound(S). {
        C = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
        gda_sql_statement_insert_take_table_name (C, X);
        gda_sql_statement_insert_take_fields_list (C, F);
        gda_sql_statement_insert_take_select (C, S);
        gda_sql_statement_insert_take_on_conflict (C, O);
}


opt_on_conflict(O) ::= . {O = NULL;}
opt_on_conflict(O) ::= OR ID(V). {O = V;}

%type ins_extra_values {GSList*}
%destructor ins_extra_values {GSList *list;
		for (list = $$; list; list = list->next) {
			g_slist_free_full ((GSList*) list->data, (GDestroyNotify) gda_sql_field_free);
		}
		g_slist_free ($$);
}
ins_extra_values(E) ::= ins_extra_values(A) COMMA LP rexprlist(L) RP. {E = g_slist_append (A, g_slist_reverse (L));}
ins_extra_values(E) ::= COMMA LP rexprlist(L) RP. {E = g_slist_append (NULL, g_slist_reverse (L));}

%type inscollist_opt {GSList*}
%destructor inscollist_opt {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_field_free);}}
inscollist_opt(A) ::= .                       {A = NULL;}
inscollist_opt(A) ::= LP inscollist(X) RP.    {A = X;}

%type inscollist {GSList*}
%destructor inscollist {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_field_free);}}
inscollist(A) ::= inscollist(X) COMMA fullname(Y). {GdaSqlField *field;
						    field = gda_sql_field_new (NULL);
						    gda_sql_field_take_name (field, Y);
						    A = g_slist_append (X, field);
}
inscollist(A) ::= fullname(Y). {GdaSqlField *field = gda_sql_field_new (NULL);
				gda_sql_field_take_name (field, Y);
				A = g_slist_prepend (NULL, field);
}

// DELETE
cmd(C) ::= DELETE FROM fullname(T) where_opt(X). {C = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE); 
						  gda_sql_statement_delete_take_table_name (C, T);
						  gda_sql_statement_delete_take_condition (C, X);}

%type where_opt {GdaSqlExpr *}
%destructor where_opt {gda_sql_expr_free ($$);}
where_opt(A) ::= .                    {A = NULL;}
where_opt(A) ::= WHERE expr(X).       {A = X;}

// UPDATE
cmd(C) ::= UPDATE opt_on_conflict(O) fullname(T) SET setlist(S) where_opt(X). {
	GSList *list;
	C = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
	gda_sql_statement_update_take_table_name (C, T);
	gda_sql_statement_update_take_on_conflict (C, O);
	gda_sql_statement_update_take_condition (C, X);
	for (list = S; list; list = list->next) {
		UpdateSet *set = (UpdateSet*) list->data;
		gda_sql_statement_update_take_set_value (C, set->fname, set->expr);
		g_free (set);
	}
	g_slist_free (S);
}

%type setlist {GSList*}
%destructor setlist {GSList *list;
	for (list = $$; list; list = list->next) {
		UpdateSet *set = (UpdateSet*) list->data;
		g_value_reset (set->fname); g_free (set->fname);
		gda_sql_expr_free (set->expr);
		g_free (set);
	}
	g_slist_free ($$);
}
setlist(A) ::= setlist(Z) COMMA fullname(X) EQ expr(Y). {UpdateSet *set;
							 set = g_new (UpdateSet, 1);
							 set->fname = X;
							 set->expr = Y;
							 A = g_slist_append (Z, set);
}
setlist(A) ::= fullname(X) EQ expr(Y). {UpdateSet *set;
					set = g_new (UpdateSet, 1);
					set->fname = X;
					set->expr = Y;
					A = g_slist_append (NULL, set);
}

// COMPOUND SELECT
%type compound {GdaSqlStatement *}
%destructor compound {gda_sql_statement_free ($$);}
//compound(C) ::= LP compound(E) RP. {C = E;}
compound(C) ::= selectcmd(S). {
	C = gda_sql_statement_new (GDA_SQL_STATEMENT_COMPOUND);
	gda_sql_statement_compound_take_stmt (C, S);
}
compound(C) ::= compound(L) UNION opt_compound_all(A) compound(R). {
	C = compose_multiple_compounds (A ? GDA_SQL_STATEMENT_COMPOUND_UNION_ALL : GDA_SQL_STATEMENT_COMPOUND_UNION,
					L, R);
}

compound(C) ::= compound(L) EXCEPT opt_compound_all(A) compound(R). {
	C = compose_multiple_compounds (A ? GDA_SQL_STATEMENT_COMPOUND_EXCEPT_ALL : GDA_SQL_STATEMENT_COMPOUND_EXCEPT,
					L, R);
} 

compound(C) ::= compound(L) INTERSECT opt_compound_all(A) compound(R). {
	C = compose_multiple_compounds (A ? GDA_SQL_STATEMENT_COMPOUND_INTERSECT_ALL : GDA_SQL_STATEMENT_COMPOUND_INTERSECT,
					L, R);
} 

%type opt_compound_all {gboolean}
opt_compound_all(A) ::= .    {A = FALSE;}
opt_compound_all(A) ::= ALL. {A = TRUE;}


// SELECT
%type selectcmd {GdaSqlStatement *}
%destructor selectcmd {gda_sql_statement_free ($$);}
selectcmd(C) ::= SELECT distinct(D) selcollist(W) from(F) where_opt(Y)
	 groupby_opt(P) having_opt(Q) orderby_opt(Z) limit_opt(L). {
	C = gda_sql_statement_new (GDA_SQL_STATEMENT_SELECT);
	if (D) {
		gda_sql_statement_select_take_distinct (C, D->distinct, D->expr);
		g_free (D);
	}
	gda_sql_statement_select_take_expr_list (C, W);
	gda_sql_statement_select_take_from (C, F);
	gda_sql_statement_select_take_where_cond (C, Y);
	gda_sql_statement_select_take_group_by (C, P);
	gda_sql_statement_select_take_having_cond (C, Q);
	gda_sql_statement_select_take_order_by (C, Z);
	gda_sql_statement_select_take_limits (C, L.count, L.offset);
}

%type limit_opt {Limit}
%destructor limit_opt {gda_sql_expr_free ($$.count); gda_sql_expr_free ($$.offset);}
limit_opt(A) ::= .                     {A.count = NULL; A.offset = NULL;}
limit_opt(A) ::= LIMIT expr(X).        {A.count = X; A.offset = NULL;}
limit_opt(A) ::= LIMIT expr(X) OFFSET expr(Y). {A.count = X; A.offset = Y;}
limit_opt(A) ::= LIMIT expr(X) COMMA expr(Y). {A.count = X; A.offset = Y;}

%type orderby_opt {GSList *}
%destructor orderby_opt {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_select_order_free);}}
orderby_opt(A) ::= .                          {A = 0;}
orderby_opt(A) ::= ORDER BY sortlist(X).      {A = X;}

%type sortlist {GSList *}
%destructor sortlist {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_select_order_free);}}
sortlist(A) ::= sortlist(X) COMMA expr(Y) sortorder(Z). {GdaSqlSelectOrder *order;
							 order = gda_sql_select_order_new (NULL);
							 order->expr = Y;
							 order->asc = Z;
							 A = g_slist_append (X, order);
}
sortlist(A) ::= expr(Y) sortorder(Z). {GdaSqlSelectOrder *order;
				       order = gda_sql_select_order_new (NULL);
				       order->expr = Y;
				       order->asc = Z;
				       A = g_slist_prepend (NULL, order);
}

%type sortorder {gboolean}
sortorder(A) ::= ASC.           {A = TRUE;}
sortorder(A) ::= DESC.          {A = FALSE;}
sortorder(A) ::= .              {A = TRUE;}


%type having_opt {GdaSqlExpr *}
%destructor having_opt {gda_sql_expr_free ($$);}
having_opt(A) ::= .                     {A = NULL;}
having_opt(A) ::= HAVING expr(X).       {A = X;}

%type groupby_opt {GSList*}
%destructor groupby_opt {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_expr_free);}}
groupby_opt(A) ::= .                      {A = 0;}
groupby_opt(A) ::= GROUP BY rnexprlist(X). {A = g_slist_reverse (X);}

%type from {GdaSqlSelectFrom *}
%destructor from {gda_sql_select_from_free ($$);}
from(F) ::= .                   {F = NULL;}
from(F) ::= FROM seltablist(X). {F = X;}

%type seltablist {GdaSqlSelectFrom *}
%destructor seltablist {gda_sql_select_from_free ($$);}
%type stl_prefix {GdaSqlSelectFrom *}
%destructor stl_prefix {gda_sql_select_from_free ($$);}

seltablist(L) ::= stl_prefix(P) seltarget(T) on_cond(C) using_opt(U). {
	GSList *last;
	if (P)
		L = P;
	else 
		L = gda_sql_select_from_new (NULL);
	gda_sql_select_from_take_new_target (L, T);
	last = g_slist_last (L->joins);
	if (last) {
		GdaSqlSelectJoin *join = (GdaSqlSelectJoin *) (last->data);
		join->expr = C;
		join->position = g_slist_length (L->targets) - 1;
		join->use = U;
	}
}

%type using_opt {GSList*}
%destructor using_opt {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_field_free);}}
using_opt(U) ::= USING LP inscollist(L) RP. {U = L;}
using_opt(U) ::= .                          {U = NULL;}

stl_prefix(P) ::= . {P = NULL;}
stl_prefix(P) ::= seltablist(L) jointype(J). {GdaSqlSelectJoin *join;
					      P = L;
					      join = gda_sql_select_join_new (GDA_SQL_ANY_PART (P));
					      join->type = J;
					      gda_sql_select_from_take_new_join (P, join);
}
					      

%type on_cond {GdaSqlExpr *}
%destructor on_cond {gda_sql_expr_free ($$);}
on_cond(N) ::= ON expr(E).  {N = E;}
on_cond(N) ::= .            {N = NULL;}

%type jointype {GdaSqlSelectJoinType}
jointype(J) ::= COMMA. {J = GDA_SQL_SELECT_JOIN_CROSS;}
jointype(J) ::= JOIN. {J = GDA_SQL_SELECT_JOIN_INNER;}
jointype(J) ::= CROSS JOIN. {J = GDA_SQL_SELECT_JOIN_CROSS;}
jointype(J) ::= INNER JOIN. {J = GDA_SQL_SELECT_JOIN_INNER;}
jointype(J) ::= NATURAL JOIN. {J = GDA_SQL_SELECT_JOIN_NATURAL;}
jointype(J) ::= LEFT JOIN. {J = GDA_SQL_SELECT_JOIN_LEFT;}
jointype(J) ::= LEFT OUTER JOIN. {J = GDA_SQL_SELECT_JOIN_LEFT;}
jointype(J) ::= RIGHT JOIN. {J = GDA_SQL_SELECT_JOIN_RIGHT;}
jointype(J) ::= RIGHT OUTER JOIN. {J = GDA_SQL_SELECT_JOIN_RIGHT;}
jointype(J) ::= FULL JOIN. {J = GDA_SQL_SELECT_JOIN_FULL;}
jointype(J) ::= FULL OUTER JOIN. {J = GDA_SQL_SELECT_JOIN_FULL;}


%type seltarget {GdaSqlSelectTarget *}
%destructor seltarget {gda_sql_select_target_free ($$);}
seltarget(T) ::= fullname(F) as(A). {T = gda_sql_select_target_new (NULL);
				     gda_sql_select_target_take_alias (T, A);
				     gda_sql_select_target_take_table_name (T, F);
}
seltarget(T) ::= fullname(F) ID(A). {T = gda_sql_select_target_new (NULL);
                                     gda_sql_select_target_take_alias (T, A);
                                     gda_sql_select_target_take_table_name (T, F);
}
seltarget(T) ::= LP compound(S) RP as(A). {T = gda_sql_select_target_new (NULL);
					     gda_sql_select_target_take_alias (T, A);
					     gda_sql_select_target_take_select (T, S);
}
seltarget(T) ::= LP compound(S) RP ID(A). {T = gda_sql_select_target_new (NULL);
					     gda_sql_select_target_take_alias (T, A);
					     gda_sql_select_target_take_select (T, S);
}
%type selcollist {GSList *}
%destructor selcollist {g_slist_free_full ($$, (GDestroyNotify) gda_sql_select_field_free);}

%type sclp {GSList *}
%destructor sclp {g_slist_free_full ($$, (GDestroyNotify) gda_sql_select_field_free);}
sclp(A) ::= selcollist(X) COMMA.             {A = X;}
sclp(A) ::= .                                {A = NULL;}

selcollist(L) ::= sclp(E) expr(X) as(A). {GdaSqlSelectField *field;
					  field = gda_sql_select_field_new (NULL);
					  gda_sql_select_field_take_expr (field, X);
					  gda_sql_select_field_take_alias (field, A); 
					  L = g_slist_append (E, field);}
selcollist(L) ::= sclp(E) starname(X). {GdaSqlSelectField *field;
					field = gda_sql_select_field_new (NULL);
					gda_sql_select_field_take_star_value (field, X);
					L = g_slist_append (E, field);}

starname(S) ::= STAR(X). {S = X;}
starname(A) ::= nm(S) DOT STAR(X). {gchar *str;
				  str = g_strdup_printf ("%s.%s", g_value_get_string (S), g_value_get_string (X));
				  A = g_new0 (GValue, 1);
				  g_value_init (A, G_TYPE_STRING);
				  g_value_take_string (A, str);
				  g_value_reset (S); g_free (S);
				  g_value_reset (X); g_free (X);
}

starname(A) ::= nm(C) DOT nm(S) DOT STAR(X). {gchar *str;
				  str = g_strdup_printf ("%s.%s.%s", g_value_get_string (C), 
							 g_value_get_string (S), g_value_get_string (X));
				  A = g_new0 (GValue, 1);
				  g_value_init (A, G_TYPE_STRING);
				  g_value_take_string (A, str);
				  g_value_reset (C); g_free (C);
				  g_value_reset (S); g_free (S);
				  g_value_reset (X); g_free (X);
}

as(A) ::= AS fullname(F). {A = F;}
as(A) ::= AS value(F). {A = F;}
as(A) ::= . {A = NULL;}

%type distinct {Distinct *}
%destructor distinct {if ($$) {if ($$->expr) gda_sql_expr_free ($$->expr); g_free ($$);}}
distinct(E) ::= . {E = NULL;}
distinct(E) ::= ALL. {E = NULL;}
distinct(E) ::= DISTINCT. {E = g_new0 (Distinct, 1); E->distinct = TRUE;}
distinct(E) ::= DISTINCT ON expr(X). [OR] {E = g_new0 (Distinct, 1); E->distinct = TRUE; E->expr = X;}

// Non empty list of expressions
%type rnexprlist {GSList *}
%destructor rnexprlist {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_expr_free);}}
rnexprlist(L) ::= rnexprlist(E) COMMA expr(X). {L = g_slist_prepend (E, X);}
rnexprlist(L) ::= expr(E). {L = g_slist_append (NULL, E);}

// List of expressions
%type rexprlist {GSList *}
%destructor rexprlist {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_expr_free);}}
rexprlist(L) ::= . {L = NULL;}
rexprlist(L) ::= rexprlist(E) COMMA expr(X). {L = g_slist_prepend (E, X);}
rexprlist(L) ::= expr(E). {L = g_slist_append (NULL, E);}

// A single expression
%type expr {GdaSqlExpr *}
%destructor expr {gda_sql_expr_free ($$);}
expr(E) ::= pvalue(V). {E = V;}
expr(E) ::= value(V). {E = gda_sql_expr_new (NULL); E->value = V;}
expr(E) ::= LP expr(X) RP. {E = X;}
expr(E) ::= fullname(V). {E = gda_sql_expr_new (NULL); E->value = V;}
expr(E) ::= fullname(V) LP rexprlist(A) RP. {GdaSqlFunction *func;
					    E = gda_sql_expr_new (NULL); 
					    func = gda_sql_function_new (GDA_SQL_ANY_PART (E)); 
					    gda_sql_function_take_name (func, V);
					    gda_sql_function_take_args_list (func, g_slist_reverse (A));
					    E->func = func;}
expr(E) ::= fullname(V) LP compound(S) RP. {GdaSqlFunction *func;
					     GdaSqlExpr *expr;
					     E = gda_sql_expr_new (NULL); 
					     func = gda_sql_function_new (GDA_SQL_ANY_PART (E)); 
					     gda_sql_function_take_name (func, V);
					     expr = gda_sql_expr_new (GDA_SQL_ANY_PART (func)); 
					     gda_sql_expr_take_select (expr, S);
					     gda_sql_function_take_args_list (func, g_slist_prepend (NULL, expr));
					     E->func = func;}
expr(E) ::= fullname(V) LP starname(A) RP. {GdaSqlFunction *func;
					    GdaSqlExpr *expr;
					    E = gda_sql_expr_new (NULL); 
					    func = gda_sql_function_new (GDA_SQL_ANY_PART (E));
					    gda_sql_function_take_name (func, V);
					    expr = gda_sql_expr_new (GDA_SQL_ANY_PART (func)); 
					    expr->value = A;
					    gda_sql_function_take_args_list (func, g_slist_prepend (NULL, expr));
					    E->func = func;}
expr(E) ::= CAST LP expr(A) AS fullname(T) RP. {E = A;
						A->cast_as = g_value_dup_string (T);
						g_value_reset (T);
						g_free (T);}
expr(E) ::= expr(A) PGCAST fullname(T). {E = A;
					 A->cast_as = g_value_dup_string (T);
					 g_value_reset (T);
					 g_free (T);}

expr(C) ::= expr(L) PLUS|MINUS(O) expr(R). {C = compose_multiple_expr (string_to_op_type (O), L, R);}
expr(C) ::= expr(L) STAR expr(R). {C = compose_multiple_expr (GDA_SQL_OPERATOR_TYPE_STAR, L, R);}
expr(C) ::= expr(L) SLASH|REM(O) expr(R). {C = create_two_expr (string_to_op_type (O), L, R);}
expr(C) ::= expr(L) BITAND|BITOR(O) expr(R). {C = create_two_expr (string_to_op_type (O), L, R);}

expr(C) ::= MINUS expr(X). [UMINUS] {C = create_uni_expr (GDA_SQL_OPERATOR_TYPE_MINUS, X);}
expr(C) ::= PLUS expr(X). [UPLUS] {C = create_uni_expr (GDA_SQL_OPERATOR_TYPE_PLUS, X);}

expr(C) ::= expr(L) AND expr(R). {C = compose_multiple_expr (GDA_SQL_OPERATOR_TYPE_AND, L, R);}
expr(C) ::= expr(L) OR expr(R). {C = compose_multiple_expr (GDA_SQL_OPERATOR_TYPE_OR, L, R);}
expr(C) ::= expr(L) CONCAT expr(R). {C = compose_multiple_expr (GDA_SQL_OPERATOR_TYPE_CONCAT, L, R);}

expr(C) ::= expr(L) GT|LEQ|GEQ|LT(O) expr(R). {C = create_two_expr (string_to_op_type (O), L, R);}
expr(C) ::= expr(L) DIFF|EQ(O) expr(R). {C = create_two_expr (string_to_op_type (O), L, R);}
expr(C) ::= expr(L) LIKE expr(R). {C = create_two_expr (GDA_SQL_OPERATOR_TYPE_LIKE, L, R);}
expr(C) ::= expr(L) ILIKE expr(R). {C = create_two_expr (GDA_SQL_OPERATOR_TYPE_ILIKE, L, R);}
expr(C) ::= expr(L) NOTLIKE expr(R). {C = create_two_expr (GDA_SQL_OPERATOR_TYPE_NOTLIKE, L, R);}
expr(C) ::= expr(L) NOTILIKE expr(R). {C = create_two_expr (GDA_SQL_OPERATOR_TYPE_NOTILIKE, L, R);}
expr(C) ::= expr(L) REGEXP|REGEXP_CI|NOT_REGEXP|NOT_REGEXP_CI|SIMILAR(O) expr(R). {C = create_two_expr (string_to_op_type (O), L, R);}
expr(C) ::= expr(L) BETWEEN expr(R) AND expr(E). {GdaSqlOperation *cond;
						  C = gda_sql_expr_new (NULL);
						  cond = gda_sql_operation_new (GDA_SQL_ANY_PART (C));
						  C->cond = cond;
						  cond->operator_type = GDA_SQL_OPERATOR_TYPE_BETWEEN;
						  cond->operands = g_slist_append (NULL, L);
						  GDA_SQL_ANY_PART (L)->parent = GDA_SQL_ANY_PART (cond);
						  cond->operands = g_slist_append (cond->operands, R);
						  GDA_SQL_ANY_PART (R)->parent = GDA_SQL_ANY_PART (cond);
						  cond->operands = g_slist_append (cond->operands, E);
						  GDA_SQL_ANY_PART (E)->parent = GDA_SQL_ANY_PART (cond);
}

expr(C) ::= expr(L) NOT BETWEEN expr(R) AND expr(E). {GdaSqlOperation *cond;
						      GdaSqlExpr *expr;
						      expr = gda_sql_expr_new (NULL);
						      cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
						      expr->cond = cond;
						      cond->operator_type = GDA_SQL_OPERATOR_TYPE_BETWEEN;
						      cond->operands = g_slist_append (NULL, L);
						      GDA_SQL_ANY_PART (L)->parent = GDA_SQL_ANY_PART (cond);
						      cond->operands = g_slist_append (cond->operands, R);
						      GDA_SQL_ANY_PART (R)->parent = GDA_SQL_ANY_PART (cond);
						      cond->operands = g_slist_append (cond->operands, E);
						      GDA_SQL_ANY_PART (E)->parent = GDA_SQL_ANY_PART (cond);

						      C = gda_sql_expr_new (NULL);
						      cond = gda_sql_operation_new (GDA_SQL_ANY_PART (C));
						      C->cond = cond;
						      cond->operator_type = GDA_SQL_OPERATOR_TYPE_NOT;
						      cond->operands = g_slist_prepend (NULL, expr);
						      GDA_SQL_ANY_PART (expr)->parent = GDA_SQL_ANY_PART (cond);
}

expr(C) ::= NOT expr(R). {C = create_uni_expr (GDA_SQL_OPERATOR_TYPE_NOT, R);}
expr(C) ::= BITNOT expr(R). {C = create_uni_expr (GDA_SQL_OPERATOR_TYPE_BITNOT, R);}
expr(C) ::= expr(R) uni_op(O) . {C = create_uni_expr (O, R);}

expr(C) ::= expr(L) IS expr(R). {C = create_two_expr (GDA_SQL_OPERATOR_TYPE_IS, L, R);}
expr(E) ::= LP compound(S) RP. {E = gda_sql_expr_new (NULL); gda_sql_expr_take_select (E, S);}
expr(E) ::= expr(R) IN LP rexprlist(L) RP. {GdaSqlOperation *cond;
					   GSList *list;
					   E = gda_sql_expr_new (NULL);
					   cond = gda_sql_operation_new (GDA_SQL_ANY_PART (E));
					   E->cond = cond;
					   cond->operator_type = GDA_SQL_OPERATOR_TYPE_IN;
					   cond->operands = g_slist_prepend (g_slist_reverse (L), R);
					   for (list = cond->operands; list; list = list->next)
						   GDA_SQL_ANY_PART (list->data)->parent = GDA_SQL_ANY_PART (cond);
}
expr(E) ::= expr(R) IN LP compound(S) RP. {GdaSqlOperation *cond;
					    GdaSqlExpr *expr;
					    E = gda_sql_expr_new (NULL);
					    cond = gda_sql_operation_new (GDA_SQL_ANY_PART (E));
					    E->cond = cond;
					    cond->operator_type = GDA_SQL_OPERATOR_TYPE_IN;
					    
					    expr = gda_sql_expr_new (GDA_SQL_ANY_PART (cond));
					    gda_sql_expr_take_select (expr, S);
					    cond->operands = g_slist_prepend (NULL, expr);
					    cond->operands = g_slist_prepend (cond->operands, R);
					    GDA_SQL_ANY_PART (R)->parent = GDA_SQL_ANY_PART (cond);
}
expr(E) ::= expr(R) NOT IN LP rexprlist(L) RP. {GdaSqlOperation *cond;
					       GSList *list;
					       E = gda_sql_expr_new (NULL);
					       cond = gda_sql_operation_new (GDA_SQL_ANY_PART (E));
					       E->cond = cond;
					       cond->operator_type = GDA_SQL_OPERATOR_TYPE_NOTIN;
					       cond->operands = g_slist_prepend (g_slist_reverse (L), R);
					       for (list = cond->operands; list; list = list->next)
						       GDA_SQL_ANY_PART (list->data)->parent = GDA_SQL_ANY_PART (cond);
}
expr(E) ::= expr(R) NOT IN LP compound(S) RP. {GdaSqlOperation *cond;
					       GdaSqlExpr *expr;
					       E = gda_sql_expr_new (NULL);
					       cond = gda_sql_operation_new (GDA_SQL_ANY_PART (E));
					       E->cond = cond;
					       cond->operator_type = GDA_SQL_OPERATOR_TYPE_NOTIN;
					       
					       expr = gda_sql_expr_new (GDA_SQL_ANY_PART (cond));
					       gda_sql_expr_take_select (expr, S);
					       cond->operands = g_slist_prepend (NULL, expr);
					       cond->operands = g_slist_prepend (cond->operands, R);
					       GDA_SQL_ANY_PART (R)->parent = GDA_SQL_ANY_PART (cond);
}
expr(A) ::= CASE case_operand(X) case_exprlist(Y) case_else(Z) END. {
	GdaSqlCase *sc;
	GSList *list;
	A = gda_sql_expr_new (NULL);
	sc = gda_sql_case_new (GDA_SQL_ANY_PART (A));
	sc->base_expr = X;
	sc->else_expr = Z;
	sc->when_expr_list = Y.when_list;
	sc->then_expr_list = Y.then_list;
	A->case_s = sc;
	for (list = sc->when_expr_list; list; list = list->next)
		GDA_SQL_ANY_PART (list->data)->parent = GDA_SQL_ANY_PART (sc);
	for (list = sc->then_expr_list; list; list = list->next)
		GDA_SQL_ANY_PART (list->data)->parent = GDA_SQL_ANY_PART (sc);
}
	
%type case_operand {GdaSqlExpr*}
%destructor case_operand {gda_sql_expr_free ($$);}
case_operand(A) ::= expr(X).            {A = X;}
case_operand(A) ::= .                   {A = NULL;}

%type case_exprlist {CaseBody}
%destructor case_exprlist {g_slist_free_full ($$.when_list, (GDestroyNotify) gda_sql_expr_free);
	g_slist_free_full ($$.then_list, (GDestroyNotify) gda_sql_expr_free);}

case_exprlist(A) ::= case_exprlist(X) WHEN expr(Y) THEN expr(Z). {
	A.when_list = g_slist_append (X.when_list, Y);
	A.then_list = g_slist_append (X.then_list, Z);
}
case_exprlist(A) ::= WHEN expr(Y) THEN expr(Z). {
	A.when_list = g_slist_prepend (NULL, Y);
	A.then_list = g_slist_prepend (NULL, Z);
}

%type case_else {GdaSqlExpr*}
%destructor case_else {gda_sql_expr_free ($$);}
case_else(A) ::= ELSE expr(X).       {A = X;}
case_else(A) ::= .                   {A = NULL;}

%type uni_op {GdaSqlOperatorType}
uni_op(O) ::= ISNULL. {O = GDA_SQL_OPERATOR_TYPE_ISNULL;}
uni_op(O) ::= IS NOTNULL. {O = GDA_SQL_OPERATOR_TYPE_ISNOTNULL;}


// Values: for all constants (G_TYPE_STRING GValue)
value(V) ::= NULL. {V = NULL;}
value(V) ::= STRING(S). {V = S;}
//value(V) ::= TEXTUAL(T). {V = T;}
value(V) ::= INTEGER(I). {V = I;}
value(V) ::= FLOAT(F). {V = F;}

// pvalue: values which are parameters (GdaSqlExpr)
%type pvalue {GdaSqlExpr *}
%destructor pvalue {gda_sql_expr_free ($$);}
pvalue(E) ::= UNSPECVAL LSBRACKET paramspec(P) RSBRACKET. {E = gda_sql_expr_new (NULL); E->param_spec = P;}
pvalue(E) ::= value(V) LSBRACKET paramspec(P) RSBRACKET. {E = gda_sql_expr_new (NULL); E->value = V; E->param_spec = P;}
pvalue(E) ::= SIMPLEPARAM(S). {E = gda_sql_expr_new (NULL); E->param_spec = gda_sql_param_spec_new (S);}

// paramspec: parameter's specifications
%type paramspec {GdaSqlParamSpec *}
%destructor paramspec {gda_sql_param_spec_free ($$);}
paramspec(P) ::= . {P = NULL;}
paramspec(P) ::= paramspec(E) PNAME(N). {if (!E) P = gda_sql_param_spec_new (NULL); else P = E; 
					 gda_sql_param_spec_take_name (P, N);}
paramspec(P) ::= paramspec(E) PDESCR(N). {if (!E) P = gda_sql_param_spec_new (NULL); else P = E; 
					 gda_sql_param_spec_take_descr (P, N);}
paramspec(P) ::= paramspec(E) PTYPE(N). {if (!E) P = gda_sql_param_spec_new (NULL); else P = E; 
					 gda_sql_param_spec_take_type (P, N);}
paramspec(P) ::= paramspec(E) PNULLOK(N). {if (!E) P = gda_sql_param_spec_new (NULL); else P = E; 
					   gda_sql_param_spec_take_nullok (P, N);}

// The name of a column or table can be any of the following:
//
nm(A) ::= JOIN(X).       {A = X;}
nm(A) ::= ID(X).       {A = X;}
nm(A) ::= TEXTUAL(X). {A = X;}
nm(A) ::= LIMIT(X). {A = X;}

// Fully qualified name
fullname(A) ::= nm(X). {A = X;}
fullname(A) ::= nm(S) DOT nm(X). {gchar *str;
				  str = g_strdup_printf ("%s.%s", g_value_get_string (S), g_value_get_string (X));
				  A = g_new0 (GValue, 1);
				  g_value_init (A, G_TYPE_STRING);
				  g_value_take_string (A, str);
				  g_value_reset (S); g_free (S);
				  g_value_reset (X); g_free (X);
}

fullname(A) ::= nm(C) DOT nm(S) DOT nm(X). {gchar *str;
				  str = g_strdup_printf ("%s.%s.%s", g_value_get_string (C), 
							 g_value_get_string (S), g_value_get_string (X));
				  A = g_new0 (GValue, 1);
				  g_value_init (A, G_TYPE_STRING);
				  g_value_take_string (A, str);
				  g_value_reset (C); g_free (C);
				  g_value_reset (S); g_free (S);
				  g_value_reset (X); g_free (X);
}
