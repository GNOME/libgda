/*
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <glib/gi18n-lib.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-sql-parser-private.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/token_types.h>
#include <libgda/gda-lockable.h>

/*
 * Main static functions
 */
static void gda_sql_parser_dispose (GObject *object);
static void gda_sql_parser_finalize (GObject *object);

static void gda_sql_parser_set_property (GObject *object,
					guint param_id,
					const GValue *value,
					GParamSpec *pspec);
static void gda_sql_parser_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec);

/* GdaLockable interface */
static void                 gda_sql_parser_lockable_init (GdaLockableInterface *iface);
static void                 gda_sql_parser_lock      (GdaLockable *lockable);
static gboolean             gda_sql_parser_trylock   (GdaLockable *lockable);
static void                 gda_sql_parser_unlock    (GdaLockable *lockable);

static void gda_sql_parser_reset (GdaSqlParser *parser);
static GValue *tokenizer_get_next_token (GdaSqlParser *parser);

static void push_tokenizer_context (GdaSqlParser *parser);
static void pop_tokenizer_context (GdaSqlParser *parser);
static gint fetch_forward (GdaSqlParser *parser, gint *out_nb_pushed, ...);
static void merge_tokenizer_contexts (GdaSqlParser *parser, gint n_contexts);

/*
 * The interface to the LEMON-generated parser
 */
void *priv_gda_sql_delimiterAlloc (void*(*)(size_t));
void priv_gda_sql_delimiterFree (void*, void(*)(void*));
void priv_gda_sql_delimiterTrace (void*, char *);
void priv_gda_sql_delimiter (void*, int, GValue *, GdaSqlParserIface *);

void *priv_gda_sql_parserAlloc (void*(*)(size_t));
void priv_gda_sql_parserFree (void*, void(*)(void*));
void priv_gda_sql_parserTrace (void*, char *);
void priv_gda_sql_parser (void*, int, GValue *, GdaSqlParserIface *);



G_DEFINE_TYPE_WITH_CODE (GdaSqlParser, gda_sql_parser, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaSqlParser)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_LOCKABLE, gda_sql_parser_lockable_init))

/* signals */
enum
{
	CHANGED,
	LAST_SIGNAL
};

/* properties */
enum
{
	PROP_0,
	PROP_FLAVOUR,
	PROP_MODE,
	PROP_LINE_ERROR,
	PROP_COL_ERROR
#ifdef GDA_DEBUG
	,PROP_DEBUG
#endif
};

/* module error */
GQuark gda_sql_parser_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_sql_parser_error");
	return quark;
}


static void
gda_sql_parser_class_init (GdaSqlParserClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gda_sql_parser_dispose;
	object_class->finalize = gda_sql_parser_finalize;

	/* Properties */
	object_class->set_property = gda_sql_parser_set_property;
	object_class->get_property = gda_sql_parser_get_property;
	g_object_class_install_property (object_class, PROP_FLAVOUR,
					 g_param_spec_int ("tokenizer-flavour", NULL, NULL,
							   GDA_SQL_PARSER_FLAVOUR_STANDARD,
							   GDA_SQL_PARSER_FLAVOUR_POSTGRESQL,
							   GDA_SQL_PARSER_FLAVOUR_STANDARD,
							   G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_MODE,
					 g_param_spec_int ("mode", NULL, NULL,
							   GDA_SQL_PARSER_MODE_PARSE,
							   GDA_SQL_PARSER_MODE_DELIMIT,
							   GDA_SQL_PARSER_MODE_PARSE,
							   G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_LINE_ERROR,
					 g_param_spec_int ("line-error", NULL, NULL,
							   0, G_MAXINT, 0,
							   G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_COL_ERROR,
					 g_param_spec_int ("column-error", NULL, NULL,
							   0, G_MAXINT, 0,
							   G_PARAM_READABLE));
#ifdef GDA_DEBUG
	g_object_class_install_property (object_class, PROP_DEBUG,
					 g_param_spec_boolean ("debug", NULL, NULL,
							       FALSE,
							       G_PARAM_WRITABLE));
#endif
}

static void
gda_sql_parser_lockable_init (GdaLockableInterface *iface)
{
	iface->lock = gda_sql_parser_lock;
	iface->trylock = gda_sql_parser_trylock;
	iface->unlock = gda_sql_parser_unlock;
}

static void
gda_sql_parser_reset (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	if (priv->sql)
		g_free (priv->sql);
	priv->sql = NULL;
	g_array_free (priv->passed_tokens, TRUE);
	priv->passed_tokens = g_array_new (FALSE, FALSE, sizeof (gint));

	if (priv->error_msg)
		g_free (priv->error_msg);
	priv->error_msg = NULL;
	priv->error_line = 0;
	priv->error_col = 0;
	priv->error_pos = 0;

	if (priv->parsed_statements) {
		g_slist_free_full (priv->parsed_statements, (GDestroyNotify) gda_sql_statement_free);
		priv->parsed_statements = NULL;
	}

	if (priv->pushed_contexts) {
		g_slist_free_full (priv->pushed_contexts, (GDestroyNotify) g_free);
		priv->pushed_contexts = NULL;
	}

	priv->context->next_token_start = NULL;
	priv->context->in_param_spec = FALSE;
	priv->context->block_level = 0;
	priv->context->last_token_start = NULL;
}

static void
gda_sql_parser_init (GdaSqlParser *parser)
{
	GdaSqlParserClass *klass;

	klass = (GdaSqlParserClass*) G_OBJECT_GET_CLASS (parser);

	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	g_rec_mutex_init (& (priv->mutex));
	priv->flavour = GDA_SQL_PARSER_FLAVOUR_STANDARD;
	if (klass->delim_alloc)
		priv->lemon_delimiter = klass->delim_alloc ((void*(*)(size_t)) g_malloc);
	else
		priv->lemon_delimiter = priv_gda_sql_delimiterAlloc ((void*(*)(size_t)) g_malloc);
	if (klass->parser_alloc)
		priv->lemon_parser = klass->parser_alloc ((void*(*)(size_t)) g_malloc);
	else
		priv->lemon_parser = priv_gda_sql_parserAlloc ((void*(*)(size_t)) g_malloc);
	priv->mode = GDA_SQL_PARSER_MODE_PARSE;
	priv->flavour = GDA_SQL_PARSER_FLAVOUR_STANDARD;

	priv->sql = NULL;
	priv->passed_tokens = g_array_new (FALSE, FALSE, sizeof (gint));

	priv->context = g_new0 (TokenizerContext, 1);
	priv->context->delimiter = ';';
	priv->context->in_param_spec = FALSE;
	priv->context->block_level = 0;
	priv->context->next_token_start = NULL;
	priv->context->last_token_start = NULL;

	priv->error_msg = NULL;
	priv->error_line = 0;
	priv->error_col = 0;
	priv->error_pos = 0;
}

/**
 * gda_sql_parser_new:
 *
 * Creates a new #GdaSqlParser object
 *
 * Returns: the new object
 */
GdaSqlParser*
gda_sql_parser_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_SQL_PARSER, NULL);
	return GDA_SQL_PARSER (obj);
}

static void
gda_sql_parser_dispose (GObject *object)
{
	GdaSqlParser *parser;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SQL_PARSER (object));

	parser = GDA_SQL_PARSER (object);
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	gda_sql_parser_reset (parser);
	if (priv->context)
		g_free (priv->context);
	priv->context = NULL;
	if (priv->passed_tokens)
		g_array_free (priv->passed_tokens, TRUE);
	priv->passed_tokens = NULL;

	/* parent class */
	G_OBJECT_CLASS (gda_sql_parser_parent_class)->dispose (object);
}

static void
gda_sql_parser_finalize (GObject *object)
{
	GdaSqlParser *parser;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SQL_PARSER (object));

	parser = GDA_SQL_PARSER (object);
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	GdaSqlParserClass *klass;

	klass = (GdaSqlParserClass*) G_OBJECT_GET_CLASS (parser);

	if (klass->delim_alloc) {
		g_assert (klass->delim_free);
		klass->delim_free (priv->lemon_delimiter, g_free);
	}
	else
		priv_gda_sql_delimiterFree (priv->lemon_delimiter, g_free);
	if (klass->parser_alloc) {
		g_assert (klass->parser_free);
		klass->parser_free (priv->lemon_parser, g_free);
	}
	else
		priv_gda_sql_parserFree (priv->lemon_parser, g_free);

	g_rec_mutex_clear (& (priv->mutex));

	/* parent class */
	G_OBJECT_CLASS (gda_sql_parser_parent_class)->finalize (object);
}


static void
gda_sql_parser_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaSqlParser *parser;

	parser = GDA_SQL_PARSER (object);
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	if (priv) {
		g_rec_mutex_lock (& (priv->mutex));
		switch (param_id) {
		case PROP_FLAVOUR:
			priv->flavour = g_value_get_int (value);
			break;
		case PROP_MODE:
			priv->mode = g_value_get_int (value);
			break;
#ifdef GDA_DEBUG
		case PROP_DEBUG: {
			gboolean debug = g_value_get_boolean (value);
			GdaSqlParserClass *klass;

			klass = (GdaSqlParserClass*) G_OBJECT_GET_CLASS (parser);
			if (klass->delim_alloc) {
				g_assert (klass->delim_trace);
				klass->delim_trace ((priv->mode == GDA_SQL_PARSER_MODE_DELIMIT) && debug ?
						    stdout : NULL, ".......DELIMITER DEBUG:");
			}
			else
				priv_gda_sql_delimiterTrace ((priv->mode == GDA_SQL_PARSER_MODE_DELIMIT) && debug ?
							stdout : NULL, ".......DELIMITER DEBUG:");
			if (klass->parser_alloc) {
				g_assert (klass->parser_trace);
				klass->parser_trace ((priv->mode == GDA_SQL_PARSER_MODE_PARSE) && debug ?
						     stdout : NULL, ".......PARSE DEBUG:");
			}
			else
				priv_gda_sql_parserTrace ((priv->mode == GDA_SQL_PARSER_MODE_PARSE) && debug ?
						     stdout : NULL, ".......PARSE DEBUG:");
			break;
		}
#endif
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
		g_rec_mutex_unlock (& (priv->mutex));
	}
}

static void
gda_sql_parser_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaSqlParser *parser;
	parser = GDA_SQL_PARSER (object);
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	if (priv) {
		switch (param_id) {
		case PROP_FLAVOUR:
			g_value_set_int (value, priv->flavour);
			break;
		case PROP_MODE:
			g_value_set_int (value, priv->mode);
			break;
		case PROP_LINE_ERROR:
			g_value_set_int (value, priv->error_line);
			break;
		case PROP_COL_ERROR:
			g_value_set_int (value, priv->error_col);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_sql_parser_parse_string:
 * @parser: a #GdaSqlParser object
 * @sql: the SQL string to parse
 * @remain: (out) (nullable): location to store a pointer to remaining part of @sql in case @sql has multiple statement, or %NULL
 * @error: location to store error, or %NULL
 *
 * Parses @sql and creates a #GdaStatement statement from the first SQL statement contained in @sql: if @sql
 * contains more than one statement, then the remaining part of the string is not parsed at all, and @remain (if
 * not %NULL) will point at the first non parsed character.
 *
 * To include variables in the @sql string, see the
 * <link linkend="GdaSqlParser.description">GdaSqlParser's object description</link>.
 *
 * Returns: (transfer full) (nullable): a new #GdaStatement object, or %NULL if an error occurred
 */
GdaStatement *
gda_sql_parser_parse_string (GdaSqlParser *parser, const gchar *sql, const gchar **remain, GError **error)
{
	GdaStatement *stmt = NULL;
	GValue *value;
	GdaSqlParserIface piface;
	gint ntokens = 0;
	GdaSqlParserMode parse_mode;
	GdaSqlParserClass *klass;
	void (*_delimit) (void*, int, GValue *, GdaSqlParserIface *) = priv_gda_sql_delimiter;
	void (*_parse) (void*, int, GValue *, GdaSqlParserIface *) = priv_gda_sql_parser;
	gint *delim_trans = delim_tokens;
	gint *parser_trans= parser_tokens;

	if (remain)
		*remain = NULL;

	g_return_val_if_fail (GDA_IS_SQL_PARSER (parser), NULL);
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	if (!sql)
		return NULL;

	g_rec_mutex_lock (& (priv->mutex));

	klass = (GdaSqlParserClass*) G_OBJECT_GET_CLASS (parser);
	if (klass->delim_alloc) {
		g_assert (klass->delim_parse);
		_delimit = klass->delim_parse;
		g_assert (klass->delim_tokens_trans);
		delim_trans = klass->delim_tokens_trans;
	}
	if (klass->parser_alloc) {
		g_assert (klass->parser_parse);
		_parse = klass->parser_parse;
		g_assert (klass->parser_tokens_trans);
		parser_trans = klass->parser_tokens_trans;
	}

	/* re-init parser */
	gda_sql_parser_reset (parser);
	priv->sql = g_strdup (sql);
	priv->context->next_token_start = priv->sql;

	parse_mode = priv->mode; /* save current mode */
	/*g_print ("\t\t===== %s MODE =====\n",
	  parse_mode == GDA_SQL_PARSER_MODE_PARSE ? "PARSE" : "DELIMIT");*/
	piface.parser = parser;
	piface.parsed_statement = NULL;

	for (value = tokenizer_get_next_token (parser);
	     priv->context->token_type != L_ILLEGAL;
	     value = tokenizer_get_next_token (parser)) {
		switch (priv->context->token_type) {
		case L_SQLCOMMENT:
			gda_value_free (value);
			value = NULL;
			break;
		case L_SPACE:
		default:
			if (priv->context->token_type == L_SPACE) {
				if (priv->context->in_param_spec ||
					  (priv->mode == GDA_SQL_PARSER_MODE_PARSE)) {
					/* ignore space */
					gda_value_free (value);
					value = NULL;
					continue;
				}
			}
			if (priv->mode == GDA_SQL_PARSER_MODE_DELIMIT) {
				if ((priv->context->token_type == L_BEGIN) &&
				    (priv->passed_tokens->len != 0) &&
				    !g_ascii_strcasecmp (g_value_get_string (value), "BEGIN")) {
					/* a BEGIN command increments the block level only if it's not the
					 * first word of statement */
					priv->context->block_level ++;
				}
				if (priv->context->token_type == L_LOOP) {
					priv->context->block_level ++;
				}
				else if ((priv->context->token_type == L_END) &&
					 (priv->passed_tokens->len != 0)) {
					/* a END command decrements the block level only if it's not the
					 * first word of statement */
					priv->context->block_level --;
					if (priv->context->block_level == 0)
						priv->context->ignore_semi = FALSE;
				}
				else if (priv->context->token_type == L_ENDLOOP) {
					priv->context->block_level --;
				}
				else if (priv->context->token_type == L_DECLARE)
					priv->context->ignore_semi = TRUE;
				else if ((priv->flavour == GDA_SQL_PARSER_FLAVOUR_ORACLE) &&
					 ((priv->context->token_type == L_AS) ||
					  (priv->context->token_type == L_IS))) {
					/* examine tokens history to see if we have somewhere a CREATE FUNCTION or
					 * CREATE PROCEDURE */
					gint i;
					for (i = priv->passed_tokens->len - 1; i >= 0; i --) {
						gint ttype;
						ttype = g_array_index (priv->passed_tokens, gint, i);
						if (ttype == L_CREATE)
							break;
					}
					if (i >= 0)
						priv->context->ignore_semi = TRUE;
				}
				else if ((priv->flavour == GDA_SQL_PARSER_FLAVOUR_MYSQL) &&
					 (priv->context->token_type == L_DELIMITER)) {
					GValue *v;
					push_tokenizer_context (parser);
					v = tokenizer_get_next_token (parser);
					if (priv->context->token_type == L_SPACE) {
						push_tokenizer_context (parser);
						g_value_reset (v);
						g_free (v);
						v = tokenizer_get_next_token (parser);
						pop_tokenizer_context (parser);
					}
					pop_tokenizer_context (parser);
					if (v) {
						if (G_VALUE_TYPE (v) == G_TYPE_STRING) {
							const gchar *str;
							str = g_value_get_string (v);
							if (str)
								priv->context->delimiter = *str;
						}
						else
							TO_IMPLEMENT;
						g_value_reset (v);
						g_free (v);
					}
				}
			}

			/* send found tokens until end of buffer */
			g_array_append_val (priv->passed_tokens, priv->context->token_type);

			switch (priv->mode) {
			case GDA_SQL_PARSER_MODE_PARSE:
				/*g_print ("TRANS %d => %d\n", priv->context->token_type,
				  parser_trans [priv->context->token_type]);*/
				_parse (priv->lemon_parser,
					parser_trans [priv->context->token_type], value, &piface);
				value = NULL;
				break;
			case GDA_SQL_PARSER_MODE_DELIMIT:
				_delimit (priv->lemon_delimiter,
					  delim_trans [priv->context->token_type], value, &piface);
				value = NULL;
				break;
			default:
				TO_IMPLEMENT;
			}

			ntokens++;
			break;
		}
		if ((priv->context->token_type == L_END_OF_FILE) || (priv->context->token_type == L_SEMI)) {
			/* end of statement */
			break;
		}
		if (priv->error_pos != 0)
			break;
	}
	if (priv->context->token_type == L_ILLEGAL) {
		if (value)
			gda_value_free (value);
		gda_sql_parser_set_syntax_error (parser);
	}

	/* send the EOF token to the LEMON parser */
	switch (priv->mode) {
	case GDA_SQL_PARSER_MODE_PARSE:
		_parse (priv->lemon_parser, 0, NULL, &piface);
		break;
	case GDA_SQL_PARSER_MODE_DELIMIT:
		_delimit (priv->lemon_delimiter, 0, NULL, &piface);
		break;
	default:
		TO_IMPLEMENT;
	}
	if (remain) {
		if (* priv->context->next_token_start) {
			gint i = priv->context->next_token_start - priv->sql;
			*remain = sql + i;
			while (**remain == ' ' || **remain == '\n' || **remain == '\r' || **remain == '\t') {
				*remain = sql + (++i);
			}
		}
	}

	if (piface.parsed_statement) {
		gchar hold;

		hold = *priv->context->next_token_start;
		*priv->context->next_token_start = 0;
		piface.parsed_statement->sql = g_strdup (priv->sql);
		*priv->context->next_token_start = hold;

		stmt = g_object_new (GDA_TYPE_STATEMENT,
				     "structure", piface.parsed_statement, NULL);
		gda_sql_statement_free (piface.parsed_statement);
	}
	else {
		if (priv->mode == GDA_SQL_PARSER_MODE_PARSE) {
			/* try to create a statement using the delimiter mode */
			priv->mode = GDA_SQL_PARSER_MODE_DELIMIT;
			stmt = gda_sql_parser_parse_string (parser, sql, remain, error);
		}
		else if (error) {
			if ((ntokens <= 1) && (priv->context->token_type != L_ILLEGAL))
				g_set_error (error, GDA_SQL_PARSER_ERROR, GDA_SQL_PARSER_EMPTY_SQL_ERROR,
					 "%s", _("SQL code does not contain any statement"));
			else
				g_set_error (error, GDA_SQL_PARSER_ERROR, priv->error_type,
					 "%s", priv->error_msg);
		}
	}

	priv->mode = parse_mode;

	g_rec_mutex_unlock (& (priv->mutex));

	return stmt;
}

/**
 * gda_sql_parser_parse_string_as_batch:
 * @parser: a #GdaSqlParser object
 * @sql: the SQL string to parse
 * @remain: (out) (nullable): location to store a pointer to remaining part of @sql in case an error occurred while parsing @sql, or %NULL
 * @error: location to store error, or %NULL
 *
 * Parse @sql and creates a #GdaBatch object which contains all the #GdaStatement objects created while parsing (one object
 * per SQL statement). Empty statements (composed of spaces only) do not appear in the resulting object.
 *
 * @sql is parsed and #GdaStatement objects are created as long as no error is found in @sql. If an error is found
 * at some point, then the parsing stops and @remain may contain a non %NULL pointer, @error may be set, and %NULL
 * is returned.
 *
 * if @sql is %NULL, then the returned #GdaBatch object will contain no statement.
 *
 * To include variables in the @sql string, see the
 * <link linkend="GdaSqlParser.description">GdaSqlParser's object description</link>.
 *
 * Returns: (transfer full) (nullable): a new #GdaBatch object, or %NULL if an error occurred
 */
GdaBatch *
gda_sql_parser_parse_string_as_batch (GdaSqlParser *parser, const gchar *sql, const gchar **remain,
				      GError **error)
{
	GdaBatch *batch;
	GdaStatement *stmt;
	const gchar *int_sql;
	gboolean allok = TRUE;
	gint n_stmt = 0;
	gint n_empty = 0;

	g_return_val_if_fail (GDA_IS_SQL_PARSER (parser), NULL);
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	if (remain)
		*remain = NULL;

	batch = gda_batch_new ();
	if (!sql)
		return batch;

	g_rec_mutex_lock (& (priv->mutex));

	int_sql = sql;
	while (int_sql && allok) {
		GError *lerror = NULL;
		const gchar *int_remain = NULL;

		stmt = gda_sql_parser_parse_string (parser, int_sql, &int_remain, &lerror);
		if (stmt) {
			if (gda_statement_is_useless (stmt))
				n_empty++;
			else {
				gda_batch_add_statement (batch, stmt);
				n_stmt++;
			}
			g_object_unref (stmt);
		}
		else if (lerror && (lerror->domain == GDA_SQL_PARSER_ERROR) &&
			 (lerror->code == GDA_SQL_PARSER_EMPTY_SQL_ERROR))
			n_empty++;
		else {
			if (int_remain)
				allok = FALSE;
			if (lerror) {
				g_propagate_error (error, lerror);
				lerror = NULL;
			}
		}
		if (lerror)
			g_error_free (lerror);
		int_sql = int_remain;
	}

	if ((n_stmt == 0) && (n_empty != 0))
		g_set_error (error, GDA_SQL_PARSER_ERROR, GDA_SQL_PARSER_EMPTY_SQL_ERROR,
			      "%s", _("SQL code does not contain any statement"));

	if (!allok || (n_stmt == 0)) {
		if (remain)
			*remain = int_sql;
		g_object_unref (batch);
		batch = NULL;
	}

	g_rec_mutex_unlock (& (priv->mutex));

	return batch;
}

/**
 * gda_sql_parser_parse_file_as_batch:
 * @parser: a #GdaSqlParser object
 * @filename: name of the file to parse
 * @error: location to store error, or %NULL
 *
 * Parse @filename's contents and creates a #GdaBatch object which contains all the
 *  #GdaStatement objects created while parsing (one object per SQL statement).
 *
 * @filename's contents are parsed and #GdaStatement objects are created as long as no error is found. If an error is found
 * at some point, then the parsing stops, @error may be set and %NULL is returned
 *
 * if @sql is %NULL, then the returned #GdaBatch object will contain no statement.
 *
 * Returns: (transfer full) (nullable): a new #GdaBatch object, or %NULL if an error occurred
 */
GdaBatch *
gda_sql_parser_parse_file_as_batch (GdaSqlParser *parser, const gchar *filename, GError **error)
{
	gchar *contents;

	g_return_val_if_fail (GDA_IS_SQL_PARSER (parser), NULL);
	g_return_val_if_fail (filename, NULL);

	if (!g_file_get_contents (filename, &contents, NULL, error))
		return NULL;
	else {
		GdaBatch *batch;
		batch = gda_sql_parser_parse_string_as_batch (parser, contents, NULL, error);
		g_free (contents);
		return batch;
	}
}


/*
 *
 * Private API
 *
 */
static gint
get_position (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	gint total;
	gchar *z = priv->sql;
	gint i, l, c;

	total = priv->context->last_token_start - priv->sql;
	for (i = 0, l = 0, c = 0; i < total; i++) {
		if (z[i] == '\n') {
			l++;
			c = 0;
		}
		else
			c++;
	}
	priv->error_line = l + 1;
	priv->error_col = c + 1;

	return total + 1;
}

void
gda_sql_parser_set_syntax_error (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	if (!priv->error_msg) {
		priv->error_type = GDA_SQL_PARSER_SYNTAX_ERROR;
		priv->error_pos = get_position (parser);
		priv->error_msg = g_strdup_printf (_("Syntax error at line %d, column %d"),
							   priv->error_line, priv->error_col);
		/*g_print ("@syntax error at line %d, col %d\n",
		  priv->error_line, priv->error_col);*/
	}
}

void
gda_sql_parser_set_overflow_error (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	if (!priv->error_msg) {
		priv->error_type = GDA_SQL_PARSER_OVERFLOW_ERROR;
		priv->error_pos = get_position (parser);
		priv->error_msg = g_strdup_printf (_("Overflow error at line %d, column %d"),
							   priv->error_line, priv->error_col);
		/*g_print ("@overflow error at line %d, col %d\n",
		  priv->error_line, priv->error_col);*/
	}
}



/*
 *
 * Tokenizer
 *
 */
/*
** If X is a character that can be used in an identifier then
** IdChar(X) will be true.  Otherwise it is false.
**
** For ASCII, any character with the high-order bit set is
** allowed in an identifier.  For 7-bit characters,
** sqlite3IsIdChar[X] must be 1.
*/
static const char AsciiIdChar[] = {
     /* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 2x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
};
#define IdChar(C) (((c=C)&0x80)!=0 || (c>0x1f && AsciiIdChar[c-0x20]))

static const unsigned char UpperToLower[] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,
    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,
    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,
    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,
    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,
    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,
    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,
    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,
    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,
    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,
    252,253,254,255
};

static gboolean
str_caseequal (gconstpointer v1, gconstpointer v2)
{
	const gchar *string1 = v1;
	const gchar *string2 = v2;

	return strcasecmp (string1, string2) == 0;
}

static guint
str_casehash (gconstpointer v)
{
	/* 31 bit hash function */
	const signed char *p = v;
	guint32 h = UpperToLower[*p];

	if (h)
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + UpperToLower[*p];

	return h;
}

static gint
keywordCode (GdaSqlParser *parser, gchar *str, gint len)
{
	static GMutex mutex;
	static GHashTable *keywords = NULL;
	gint type;
	gchar oldc;
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	g_mutex_lock (&mutex);
	if (!keywords) {
		/* if keyword begins with a number, it is the GdaSqlParserFlavour to which it only applies:
		* for example "4start" would refer to the START keyword for PostgreSQL only */
		keywords = g_hash_table_new (str_casehash, str_caseequal);
		g_hash_table_insert (keywords, "all", GINT_TO_POINTER (L_ALL));
		g_hash_table_insert (keywords, "and", GINT_TO_POINTER (L_AND));
		g_hash_table_insert (keywords, "as", GINT_TO_POINTER (L_AS));
		g_hash_table_insert (keywords, "asc", GINT_TO_POINTER (L_ASC));
		g_hash_table_insert (keywords, "3batch", GINT_TO_POINTER (L_BATCH));
		g_hash_table_insert (keywords, "begin", GINT_TO_POINTER (L_BEGIN));
		g_hash_table_insert (keywords, "between", GINT_TO_POINTER (L_BETWEEN));
		g_hash_table_insert (keywords, "by", GINT_TO_POINTER (L_BY));
		g_hash_table_insert (keywords, "case", GINT_TO_POINTER (L_CASE));
		g_hash_table_insert (keywords, "cast", GINT_TO_POINTER (L_CAST));
		g_hash_table_insert (keywords, "comment", GINT_TO_POINTER (L_COMMENT));
		g_hash_table_insert (keywords, "commit", GINT_TO_POINTER (L_COMMIT));
		g_hash_table_insert (keywords, "committed", GINT_TO_POINTER (L_COMMITTED));
		g_hash_table_insert (keywords, "create", GINT_TO_POINTER (L_CREATE));
		g_hash_table_insert (keywords, "cross", GINT_TO_POINTER (L_CROSS));
		g_hash_table_insert (keywords, "declare", GINT_TO_POINTER (L_DECLARE));
		g_hash_table_insert (keywords, "delete", GINT_TO_POINTER (L_DELETE));
		g_hash_table_insert (keywords, "deferred", GINT_TO_POINTER (L_DEFERRED));
		g_hash_table_insert (keywords, "delimiter", GINT_TO_POINTER (L_DELIMITER));
		g_hash_table_insert (keywords, "desc", GINT_TO_POINTER (L_DESC));
		g_hash_table_insert (keywords, "distinct", GINT_TO_POINTER (L_DISTINCT));
		g_hash_table_insert (keywords, "else", GINT_TO_POINTER (L_ELSE));
		g_hash_table_insert (keywords, "end", GINT_TO_POINTER (L_END));
		g_hash_table_insert (keywords, "except", GINT_TO_POINTER (L_EXCEPT));
		g_hash_table_insert (keywords, "exclusive", GINT_TO_POINTER (L_EXCLUSIVE));
		g_hash_table_insert (keywords, "3force", GINT_TO_POINTER (L_FORCE));
		g_hash_table_insert (keywords, "from", GINT_TO_POINTER (L_FROM));
		g_hash_table_insert (keywords, "full", GINT_TO_POINTER (L_FULL));
		g_hash_table_insert (keywords, "group", GINT_TO_POINTER (L_GROUP));
		g_hash_table_insert (keywords, "having", GINT_TO_POINTER (L_HAVING));
		g_hash_table_insert (keywords, "ilike", GINT_TO_POINTER (L_ILIKE));
		g_hash_table_insert (keywords, "immediate", GINT_TO_POINTER (L_IMMEDIATE));
		g_hash_table_insert (keywords, "in", GINT_TO_POINTER (L_IN));
		g_hash_table_insert (keywords, "inner", GINT_TO_POINTER (L_INNER));
		g_hash_table_insert (keywords, "insert", GINT_TO_POINTER (L_INSERT));
		g_hash_table_insert (keywords, "intersect", GINT_TO_POINTER (L_INTERSECT));
		g_hash_table_insert (keywords, "into", GINT_TO_POINTER (L_INTO));
		g_hash_table_insert (keywords, "is", GINT_TO_POINTER (L_IS));
		g_hash_table_insert (keywords, "isolation", GINT_TO_POINTER (L_ISOLATION));
		g_hash_table_insert (keywords, "join", GINT_TO_POINTER (L_JOIN));
		g_hash_table_insert (keywords, "left", GINT_TO_POINTER (L_LEFT));
		g_hash_table_insert (keywords, "level", GINT_TO_POINTER (L_LEVEL));
		g_hash_table_insert (keywords, "like", GINT_TO_POINTER (L_LIKE));
		g_hash_table_insert (keywords, "limit", GINT_TO_POINTER (L_LIMIT));
		g_hash_table_insert (keywords, "loop", GINT_TO_POINTER (L_LOOP));
		g_hash_table_insert (keywords, "natural", GINT_TO_POINTER (L_NATURAL));
		g_hash_table_insert (keywords, "not", GINT_TO_POINTER (L_NOT));
		g_hash_table_insert (keywords, "3nowait", GINT_TO_POINTER (L_NOWAIT));
		g_hash_table_insert (keywords, "null", GINT_TO_POINTER (L_NULL));
		g_hash_table_insert (keywords, "offset", GINT_TO_POINTER (L_OFFSET));
		g_hash_table_insert (keywords, "on", GINT_TO_POINTER (L_ON));
		g_hash_table_insert (keywords, "only", GINT_TO_POINTER (L_ONLY));
		g_hash_table_insert (keywords, "or", GINT_TO_POINTER (L_OR));
		g_hash_table_insert (keywords, "order", GINT_TO_POINTER (L_ORDER));
		g_hash_table_insert (keywords, "outer", GINT_TO_POINTER (L_OUTER));
		g_hash_table_insert (keywords, "right", GINT_TO_POINTER (L_RIGHT));
		g_hash_table_insert (keywords, "read", GINT_TO_POINTER (L_READ));
		g_hash_table_insert (keywords, "release", GINT_TO_POINTER (L_RELEASE));
		g_hash_table_insert (keywords, "repeatable", GINT_TO_POINTER (L_REPEATABLE));
		g_hash_table_insert (keywords, "rollback", GINT_TO_POINTER (L_ROLLBACK));
		g_hash_table_insert (keywords, "savepoint", GINT_TO_POINTER (L_SAVEPOINT));
		g_hash_table_insert (keywords, "select", GINT_TO_POINTER (L_SELECT));
		g_hash_table_insert (keywords, "serializable", GINT_TO_POINTER (L_SERIALIZABLE));
		g_hash_table_insert (keywords, "set", GINT_TO_POINTER (L_SET));
		g_hash_table_insert (keywords, "similar", GINT_TO_POINTER (L_SIMILAR));
		g_hash_table_insert (keywords, "start", GINT_TO_POINTER (L_BEGIN));
		g_hash_table_insert (keywords, "then", GINT_TO_POINTER (L_THEN));
		g_hash_table_insert (keywords, "to", GINT_TO_POINTER (L_TO));
		g_hash_table_insert (keywords, "transaction", GINT_TO_POINTER (L_TRANSACTION));
		g_hash_table_insert (keywords, "uncommitted", GINT_TO_POINTER (L_UNCOMMITTED));
		g_hash_table_insert (keywords, "union", GINT_TO_POINTER (L_UNION));
		g_hash_table_insert (keywords, "update", GINT_TO_POINTER (L_UPDATE));
		g_hash_table_insert (keywords, "using", GINT_TO_POINTER (L_USING));
		g_hash_table_insert (keywords, "values", GINT_TO_POINTER (L_VALUES));
		g_hash_table_insert (keywords, "3wait", GINT_TO_POINTER (L_WAIT));
		g_hash_table_insert (keywords, "when", GINT_TO_POINTER (L_WHEN));
		g_hash_table_insert (keywords, "where", GINT_TO_POINTER (L_WHERE));
		g_hash_table_insert (keywords, "work", GINT_TO_POINTER (L_TRANSACTION));
		g_hash_table_insert (keywords, "write", GINT_TO_POINTER (L_WRITE));
	}
	g_mutex_unlock (&mutex);

	oldc = str[len];
	str[len] = 0;
	type = GPOINTER_TO_INT (g_hash_table_lookup (keywords, str));
	if (type == 0) {
		/* try prepending the current flavour */
		gchar *tmp;
		tmp = g_strdup_printf ("%d%s", priv->flavour, str);
		type = GPOINTER_TO_INT (g_hash_table_lookup (keywords, tmp));
		g_free (tmp);
		if (type == 0) {
			if (priv->mode == GDA_SQL_PARSER_MODE_PARSE)
				type = L_ID;
			else
				type = L_RAWSTRING;
		}
	}
	/*g_print ("Looking for /%s/ -> %d\n", str, type);*/
	str[len] = oldc;

	return type;
}

static GValue *
token_as_string (gchar *ptr, gint len)
{
	gchar *end, oldc;
	GValue *retval;

	end = ptr + len;
	oldc = *end;
	*end = 0;

	retval = g_new0 (GValue, 1);
	g_value_init (retval, G_TYPE_STRING);
	g_value_set_string (retval, ptr);

	*end = oldc;

	return retval;
}

static gboolean
handle_composed_2_keywords (GdaSqlParser *parser, GValue *retval, gint second, gint replacer);

/*
 *
 * Returns: the token Id, or:
 *   L_ILLEGAL if an error occurred
 *   L_END_OF_FILE if nothing more to analyze
 */
static GValue *
getToken (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	int i, c;
	gchar *z = priv->context->next_token_start;
	GValue *retval = NULL;
	gint consumed_chars = 1;

	/* init to capture non treaded cases */
	priv->context->token_type = G_MININT;

	if (!*z) {
		priv->context->token_type = L_END_OF_FILE;
		consumed_chars = 0;
		goto tok_end;
	}

#ifdef GDA_DEBUG_NO
	gchar debug_hold = 0;
	gboolean debugcut = FALSE;
	if (strlen (z) > 50) {
		debugcut = TRUE;
		debug_hold = z[50];
		z[50] = 0;
	}

	g_print ("TOK for `%s` (delim='%c') is: ", z, priv->context->delimiter);
	if (debugcut)
		z[50] = debug_hold;
#endif

	if (*z == priv->context->delimiter) {
		if (!priv->context->ignore_semi && (priv->context->block_level == 0))
			priv->context->token_type = L_SEMI;
		else
			priv->context->token_type = L_RAWSTRING;
		consumed_chars = 1;
		retval = token_as_string (priv->context->next_token_start, 1);
		goto tok_end;
	}

	switch (*z) {
	case ' ': case '\t': case '\n': case '\f': case '\r': {
		for (i=1; isspace (z[i]); i++){}
		if ((z[i] == '/') && (z[i+1] == '*')) {
			priv->context->token_type = L_LSBRACKET;
			consumed_chars = i + 2;
			priv->context->in_param_spec = TRUE;
		}
		else {
			priv->context->token_type = L_SPACE;
			consumed_chars = i;
		}
		break;
	}
	case '-':
		if ( z[1]=='-' ){
			for (i=2;  (c=z[i])!=0 && c!='\n'; i++){}
			priv->context->token_type = L_SQLCOMMENT;
			consumed_chars = i;
		}
		else {
			priv->context->token_type = L_MINUS;
			consumed_chars = 1;
		}
		break;

	case '(':
		priv->context->token_type = L_LP;
		consumed_chars = 1;
		break;
	case ')':
		priv->context->token_type = L_RP;
		consumed_chars = 1;
		break;
	case '+':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			priv->context->token_type = L_PLUS;
			consumed_chars = 1;
		}
		break;
	case '*':
		if (z[1] == '/') {
			priv->context->token_type = L_RSBRACKET;
			consumed_chars = 2;
			priv->context->in_param_spec = FALSE;
		}
		else {
			priv->context->token_type = L_STAR;
			consumed_chars = 1;
		}
		break;
	case '%':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			priv->context->token_type = L_REM;
			consumed_chars = 1;
		}
		break;
	case '/':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			if (z[1] != '*' || z[2] == 0) {
				priv->context->token_type = L_SLASH;
				consumed_chars = 1;
			}
		}
		else if (z[1] == '*') {
			/* delimit mode */
			priv->context->token_type = L_LSBRACKET;
			consumed_chars = 2;
			priv->context->in_param_spec = TRUE;
		}
		break;
	case '=':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			priv->context->token_type = L_EQ;
			consumed_chars = 1 + (z[1] == '=');
		}
		break;
	case '<':
		if ((c = z[1]) == '=') {
			priv->context->token_type = L_LEQ;
			consumed_chars = 2;
		}
		else if (c == '>') {
			priv->context->token_type = L_DIFF;
			consumed_chars = 2;
		}
		else if (c== '<') {
			priv->context->token_type = L_LSHIFT;
			consumed_chars = 2;
		}
		else {
			priv->context->token_type = L_LT;
			consumed_chars = 1;
		}
		break;
	case '>':
		if ((c = z[1]) == '=') {
			priv->context->token_type = L_GEQ;
			consumed_chars = 2;
		}
		else if (c == '>') {
			priv->context->token_type = L_RSHIFT;
			consumed_chars = 2;
		}
		else {
			priv->context->token_type = L_GT;
			consumed_chars = 1;
		}
		break;
	case '!':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			if ((c = z[1]) == '=') {
				priv->context->token_type = L_DIFF;
				consumed_chars = 2;
			}
			else if (c == '~') {
				if (z[2] == '*') {
					priv->context->token_type = L_NOT_REGEXP_CI;
					consumed_chars = 3;
				}
				else {
					priv->context->token_type = L_NOT_REGEXP;
					consumed_chars = 2;
				}
			}
		}
		break;
	case '|':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			if (z[1] != '|') {
				priv->context->token_type = L_BITOR;
				consumed_chars = 1;
			} else {
				priv->context->token_type = L_CONCAT;
				consumed_chars = 2;
			}
		}
		else {
			priv->context->token_type = L_RAWSTRING;
			consumed_chars = 1;
		}
		break;
	case ',':
		priv->context->token_type = L_COMMA;
		consumed_chars = 1;
		break;
	case '&':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			priv->context->token_type = L_BITAND;
			consumed_chars = 1;
		}
		break;
	case '~':
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			if (z[1] == '*') {
				priv->context->token_type = L_REGEXP_CI;
				consumed_chars = 2;
			}
			else {
				if (priv->flavour == GDA_SQL_PARSER_FLAVOUR_POSTGRESQL)
					priv->context->token_type = L_REGEXP;
				else
					priv->context->token_type = L_BITNOT;
				consumed_chars = 1;
			}
		}
		break;
	case '\'':
	case '"': {
		char delim = z[0];
		for (i = 1; (c = z[i]) != 0; i++) {
			if (c == delim) {
				if (z[i+1] == delim)
					i++;
				else
					break;
			}
			else if (c == '\\')
				i++;
		}
		if (c) {
			if (delim == '"')
				priv->context->token_type = L_TEXTUAL;
			else
				priv->context->token_type = L_STRING;
			consumed_chars = i+1;
		}
		else {
			priv->context->token_type = L_ILLEGAL;
			consumed_chars = 0;
		}
		break;
	}


	case '.': case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9': {
		if ((*z) == '.') {
			if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
				if (! isdigit (z[1])) {
					priv->context->token_type = L_DOT;
					consumed_chars = 1;
					break;
				}
				/* If the next character is a digit, this is a floating point
				** number that begins with ".".  Fall thru into the next case */
			}
		}
		priv->context->token_type = L_INTEGER;
		if ((z[0] == '0') && ((z[1] == 'x') || (z[1] == 'X')) && (z[2] != 0)) {
			/* hexadecimal */
			for (i=2; isxdigit (z[i]); i++){}
		}
		else {
			for (i=0; isdigit (z[i]); i++){}
			if (z[i] == '.') {
				i++;
				while (isdigit (z[i])) {i++;}
				priv->context->token_type = L_FLOAT;
			}
			if ((z[i]=='e' || z[i]=='E') &&
			    (isdigit (z[i+1]) ||
			     ((z[i+1]=='+' || z[i+1]=='-') && isdigit (z[i+2])))) {
				i += 2;
				while (isdigit (z[i])) {i++;}
				priv->context->token_type = L_FLOAT;
			}
		}
		if (priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) {
			while (IdChar (z[i])) {
				priv->context->token_type = L_ILLEGAL;
				i++;
			}
		}
		else {
			while (IdChar (z[i])) {
				priv->context->token_type = L_RAWSTRING;
				i++;
			}
		}
		consumed_chars = i;
		break;
	}
	case '?':
		if (priv->flavour == GDA_SQL_PARSER_FLAVOUR_SQLITE) {
			for(i=1; isdigit(z[i]); i++){}
			priv->context->token_type = L_SIMPLEPARAM;
			retval = token_as_string (priv->context->next_token_start + 1, i - 1);
			consumed_chars = i;
		}
		break;
	case '#': {
		if (z[1] == '#') {
			/* parameter */
			for (i=2; (z[i]) &&
				     (IdChar (z[i]) || (z[i] == '+') || (z[i] == '-') || (z[i] == '.') || (z[i] == ':') ||
				      (z[i] == '|') || (z[i] == '@') || (z[i] == '?')) &&
				     (z[i] != '/') && (z[i] != priv->context->delimiter)
				     /*(!isspace (z[i])) && (z[i] != '/') &&
				       (z[i] != priv->context->delimiter)*/; i++) {}
			if (i > 2) {
				priv->context->token_type = L_SIMPLEPARAM;
				retval = token_as_string (priv->context->next_token_start + 2, i - 2);
			}
			else
				priv->context->token_type = L_UNSPECVAL;
			consumed_chars = i;
		}
		else {
			if (priv->flavour == GDA_SQL_PARSER_FLAVOUR_MYSQL) {
				/* comment */
				for (i=1;  (c=z[i])!=0 && c!='\n'; i++){}
				priv->context->token_type = L_SQLCOMMENT;
				consumed_chars = i;
			}
			else
				priv->context->token_type = L_ILLEGAL;
		}
		break;
	}
	case '$':
		if (priv->flavour == GDA_SQL_PARSER_FLAVOUR_POSTGRESQL) {
			for(i=1; isalnum(z[i]) || (z[i] == '_'); i++){}
			if (z[i] == '$') {
				/* this is the start of the PostgreSQL's Dollar-Quoted strings */
				gchar *tag_start = z;
				gint tag_len = i+1;

				/* no matching tag found => error */
				priv->context->token_type = L_ILLEGAL;

				i++;
				while (z[i]) {
					/* go to the next '$' */
					gint j;
					for (; z[i] && (z[i] != '$'); i++);
					for (j = 0; j < tag_len; j++, i++) {
						if (z[i] != tag_start[j])
							break; /* for */
					}
					if (j == tag_len) {
						/* tags matched */
						priv->context->token_type = L_STRING;
						consumed_chars = i;

						retval = token_as_string (priv->context->next_token_start, consumed_chars);
						/* remove comments from returned string */
						gchar *tmp, *ptr;
						tmp = (gchar*) g_value_get_string (retval);
						for (ptr = tmp; *ptr; ptr++) {
							if (((ptr == tmp) || (*(ptr-1) == '\n')) && (*ptr == '-') && (ptr[1] == '-')) {
								/* we have a comment */
								gchar *ptr2;
								for (ptr2 = ptr + 2; ptr2 && (*ptr2 != '\n'); ptr2++) {};
								memmove (ptr, ptr2, sizeof (char) * (strlen (ptr2) + 1));
							}
						}

						break; /* while */
					}
					i++;
				}
			}
		}
		else if (priv->flavour == GDA_SQL_PARSER_FLAVOUR_SQLITE) {
			for(i=1; isalnum(z[i]); i++){}
			priv->context->token_type = L_SIMPLEPARAM;
			retval = token_as_string (priv->context->next_token_start + 1, i - 1);
			consumed_chars = i;
		}
		break;
	case '@':
	case ':':
		if (z[1] == ':') {
			priv->context->token_type = L_PGCAST;
			consumed_chars = 2;
		}
		else if (priv->flavour == GDA_SQL_PARSER_FLAVOUR_SQLITE) {
			for(i=1; isalnum(z[i]) || (z[i] == '_'); i++){}
			priv->context->token_type = L_SIMPLEPARAM;
			retval = token_as_string (priv->context->next_token_start + 1, i - 1);
			consumed_chars = i;
		}
		break;
#ifndef SQLITE_OMIT_BLOB_LITERAL
	case 'x': case 'X': {
		if (  (c=z[1])=='\'' || c=='"' ){
			int delim = c;
			priv->context->token_type = L_BLOB;
			for (i=2;  (c=z[i])!=0; i++){
				if ( c==delim ){
					if ( i%2 ) priv->context->token_type = L_ILLEGAL;
					break;
				}
				if ( !isxdigit (c) ){
					priv->context->token_type = L_ILLEGAL;
					consumed_chars = i;
					break;
				}
			}
			if ( c ) i++;
			consumed_chars = i;
		}
		break;
	}
#endif
	default:
		break;
	}

	if (priv->context->token_type == G_MININT) {
		/* now treat non treated cases */
		if ((priv->mode != GDA_SQL_PARSER_MODE_DELIMIT) && (! priv->context->in_param_spec)) {
			if (IdChar (*z)) {
				for (i=1; IdChar (z[i]); i++){}
				priv->context->token_type = keywordCode (parser, (char*)z, i);
				consumed_chars = i;
			}
		}
		else {
			if ((! priv->context->in_param_spec) && IdChar (*z)) {
				gint ttype;

				for (i=1; IdChar (z[i]); i++){}
				ttype = keywordCode (parser, (char*)z, i);
				if (ttype != L_RAWSTRING) {
					priv->context->token_type = ttype;
					consumed_chars = i;
				}
			}

			if (priv->context->token_type == G_MININT) {
				if (!strncmp (priv->context->next_token_start, "name:", 5)) {
					priv->context->next_token_start += 5;
					retval = getToken (parser);
					priv->context->token_type = L_PNAME;
					consumed_chars = 0;
				}
				else if (!strncmp (priv->context->next_token_start, "type:", 5)) {
					priv->context->next_token_start += 5;
					retval = getToken (parser);
					priv->context->token_type = L_PTYPE;
					consumed_chars = 0;
				}
				else if (!strncmp (priv->context->next_token_start, "descr:", 6)) {
					priv->context->next_token_start += 6;
					retval = getToken (parser);
					priv->context->token_type = L_PDESCR;
					consumed_chars = 0;
				}
				else if (!strncmp (priv->context->next_token_start, "nullok:", 7)) {
					priv->context->next_token_start += 7;
					retval = getToken (parser);
					priv->context->token_type = L_PNULLOK;
					consumed_chars = 0;
				}
				else {
					for (i=1; z[i] && (! isspace (z[i])) &&
						     (z[i] != priv->context->delimiter) && (z[i] != '*') &&
						     (z[i] != '\'') && (z[i] != '"') && (z[i] != '#'); i++){}
					priv->context->token_type = L_RAWSTRING;
					consumed_chars = i;
				}
			}
		}
	}

	/* fallback for the token type */
	if (priv->context->token_type == G_MININT)
		priv->context->token_type = L_ILLEGAL;

	if (!retval)
		retval = token_as_string (priv->context->next_token_start, consumed_chars);
 tok_end:
	priv->context->last_token_start = priv->context->next_token_start;
	priv->context->next_token_start += consumed_chars;

	if (priv->context->token_type == L_END)
		handle_composed_2_keywords (parser, retval, L_LOOP, L_ENDLOOP);
	else if (priv->context->token_type == L_IS)
		handle_composed_2_keywords (parser, retval, L_NULL, L_ISNULL);
	else if (priv->context->token_type == L_NOT) {
		if (!handle_composed_2_keywords (parser, retval, L_NULL, L_NOTNULL)) {
			if (!handle_composed_2_keywords (parser, retval, L_LIKE, L_NOTLIKE)) {
				handle_composed_2_keywords (parser, retval, L_ILIKE, L_NOTILIKE);
			}
		}
	}
	else if (priv->context->token_type == L_SIMILAR)
		handle_composed_2_keywords (parser, retval, L_TO, L_SIMILAR);

#ifdef GDA_DEBUG_NO
	if (retval) {
		gchar *str = gda_sql_value_stringify (retval);
		g_print ("%d (%s)\n", priv->context->token_type, str);
		g_free (str);
	}
	else if (priv->context->token_type == L_END_OF_FILE)
		g_print ("%d (END OF FILE)\n", priv->context->token_type);
	else
		g_print ("%d\n", priv->context->token_type);
#endif
	return retval;
}

static gboolean
handle_composed_2_keywords (GdaSqlParser *parser, GValue *retval, gint second, gint replacer)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	gint npushed, nmatched;
	GValue *v = NULL;
	gboolean match;
	nmatched = fetch_forward (parser, &npushed, second, &v, 0);
	match = (nmatched == 1);
	if (match) {
		gchar *newstr;
		merge_tokenizer_contexts (parser, npushed);
		priv->context->token_type = replacer;

		newstr = g_strdup_printf ("%s %s", g_value_get_string (retval), g_value_get_string (v));
		g_value_reset (retval);
		g_value_take_string (retval, newstr);
	}
	if (v) {
		g_value_reset (v);
		g_free (v);
	}
	return match;
}

static GValue *
tokenizer_get_next_token (GdaSqlParser *parser)
{
	return getToken (parser);
}

static void
push_tokenizer_context (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	TokenizerContext *nc;

	nc = g_new (TokenizerContext, 1);
	*nc = *priv->context;
	priv->pushed_contexts = g_slist_prepend (priv->pushed_contexts, priv->context);
	priv->context = nc;
#ifdef GDA_DEBUG_NO
	g_print ("Push context\n");
#endif
}

static void
pop_tokenizer_context (GdaSqlParser *parser)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	g_return_if_fail (priv->pushed_contexts);
	g_free (priv->context);
	priv->context = (TokenizerContext*) priv->pushed_contexts->data;
	priv->pushed_contexts = g_slist_remove (priv->pushed_contexts, priv->context);
#ifdef GDA_DEBUG_NO
	g_print ("Pop context\n");
#endif
}

/*
 * Looks forward which tokens are available next, and returns the number of tokens corresponding to
 * expected token(s)
 *
 * extra arguments are a list of (gint expected_type, GValue **value) followed by 0
 */
static gint
fetch_forward (GdaSqlParser *parser, gint *out_nb_pushed, ...)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	gint nmatched = 0;
	gint npushed = 0;
	va_list ap;

	gint exp_type;
	va_start (ap, out_nb_pushed);
	exp_type = va_arg (ap, gint);
	while (exp_type != 0) {
		GValue **holder;
		GValue *v1;
		gint ttype;

		holder = va_arg (ap, GValue **);
		if (holder)
			*holder = NULL;

		push_tokenizer_context (parser); npushed++;
		v1 = getToken (parser);
		ttype = priv->context->token_type;
		if (ttype == L_SPACE) {
			GValue *v2;
			push_tokenizer_context (parser); npushed++;
			v2 = getToken (parser);
			ttype = priv->context->token_type;
			g_value_reset (v1);
			g_free (v1);
			v1 = v2;
		}
		if (ttype != exp_type) {
			if (v1) {
				g_value_reset (v1);
				g_free (v1);
			}

			/* not what was expected => pop all the contexts */
			for (; npushed > nmatched ; npushed--)
				pop_tokenizer_context (parser);
			break; /* while */
		}
		else {
			if (holder)
				*holder = v1;
			else {
				g_value_reset (v1);
				g_free (v1);
			}
			nmatched ++;
		}
		exp_type = va_arg (ap, gint);
	}
	va_end (ap);

	if (out_nb_pushed)
		*out_nb_pushed = npushed;
	return nmatched;
}

/* merge the @n_contexts into the current context */
static void
merge_tokenizer_contexts (GdaSqlParser *parser, gint n_contexts)
{
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);
	TokenizerContext *c;
	gint i;
	g_return_if_fail (n_contexts > 0);

	c = g_slist_nth_data (priv->pushed_contexts, n_contexts - 1);
	g_return_if_fail (c);

	priv->context->token_type = c->token_type;
	priv->context->last_token_start = c->last_token_start;
	priv->context->delimiter = c->delimiter;
	for (i = 0; i < n_contexts; i++) {
		g_free (priv->pushed_contexts->data);
		priv->pushed_contexts = g_slist_remove (priv->pushed_contexts,
								priv->pushed_contexts->data);
	}
}

static void
gda_sql_parser_lock (GdaLockable *lockable)
{
	GdaSqlParser *parser = (GdaSqlParser *) lockable;
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	g_rec_mutex_lock (& (priv->mutex));
}

static gboolean
gda_sql_parser_trylock (GdaLockable *lockable)
{
	GdaSqlParser *parser = (GdaSqlParser *) lockable;
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	return g_rec_mutex_trylock (& (priv->mutex));
}

static void
gda_sql_parser_unlock (GdaLockable *lockable)
{
	GdaSqlParser *parser = (GdaSqlParser *) lockable;
	GdaSqlParserPrivate *priv = gda_sql_parser_get_instance_private (parser);

	g_rec_mutex_unlock (& (priv->mutex));
}
