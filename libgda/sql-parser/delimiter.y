// All token codes are small integers with #defines that begin with "TK_"
%token_prefix D_

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
%name priv_gda_sql_delimiter

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
#include <libgda/sql-parser/gda-statement-struct-unknown.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>
#include <assert.h>
}

// The following directive causes tokens to
// fallback to RAWSTRING if they will not parse as their original value.
// However in this particular case, it serves to declare the LOOP,... tokens used by the
// GdaSqlParser object
%fallback RAWSTRING LOOP ENDLOOP DECLARE CREATE BLOB.

// force the declaration of the ILLEGAL and SQLCOMMENT tokens
%nonassoc ILLEGAL.
%nonassoc SQLCOMMENT.

// Input is a single SQL command
// A single statement
%type stmt {GdaSqlStatement *}
%destructor stmt {g_print ("Statement destroyed by parser: %p\n", $$); gda_sql_statement_free ($$);}
stmt ::= exprlist(L) SEMI. {pdata->parsed_statement = gda_sql_statement_new (GDA_SQL_STATEMENT_UNKNOWN); 
			    /* FIXME: set SQL */
			    gda_sql_statement_unknown_take_expressions (pdata->parsed_statement, g_slist_reverse (L));
}
stmt ::= exprlist(L) END_OF_FILE. {pdata->parsed_statement = gda_sql_statement_new (GDA_SQL_STATEMENT_UNKNOWN); 
				   /* FIXME: set SQL */
				   gda_sql_statement_unknown_take_expressions (pdata->parsed_statement, g_slist_reverse (L));
}

// List of expressions
%type exprlist {GSList *}
%destructor exprlist {if ($$) {g_slist_free_full ($$, (GDestroyNotify) gda_sql_expr_free);}}
exprlist(L) ::= exprlist(E) expr(X). {L = g_slist_prepend (E, X);}
exprlist(L) ::= expr(E). {L = g_slist_append (NULL, E);}

// A single expression
%type expr {GdaSqlExpr *}
%destructor expr {gda_sql_expr_free ($$);}
expr(E) ::= pvalue(V). {E = V;}
expr(E) ::= RAWSTRING(R). {E = gda_sql_expr_new (NULL); E->value = R;}
expr(E) ::= SPACE(S). {E =gda_sql_expr_new (NULL); E->value = S;}
expr(E) ::= value(V). {E =gda_sql_expr_new (NULL); E->value = V;}

// Values: for all constants (G_TYPE_STRING GValue)
value(V) ::= STRING(S). {V = S;}
value(V) ::= TEXTUAL(T). {V = T;}
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
