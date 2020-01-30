/*
 * Copyright (C) 2008 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2017-2018 Daniel Espinosa <esodan@gmail.com>
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

#undef GDA_DISABLE_DEPRECATED

#define G_LOG_DOMAIN "GDA-statement"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>
#include <libgda/sql-parser/gda-statement-struct-unknown.h>
#include <libgda/sql-parser/gda-statement-struct-insert.h>
#include <libgda/sql-parser/gda-statement-struct-delete.h>
#include <libgda/sql-parser/gda-statement-struct-update.h>
#include <libgda/sql-parser/gda-statement-struct-compound.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/gda-marshal.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-statement-extra.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-set.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-util.h>
#include <libgda/libgda.h>

/* 
 * Main static functions 
 */
static void gda_statement_dispose (GObject *object);

static void gda_statement_set_property (GObject *object,
					guint param_id,
					const GValue *value,
					GParamSpec *pspec);
static void gda_statement_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec);

typedef struct {
	GdaSqlStatement *internal_struct;
	GType           *requested_types;
} GdaStatementPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaStatement, gda_statement, G_TYPE_OBJECT)
/* signals */
enum
{
	RESET,
	CHECKED,
	LAST_SIGNAL
};

static gint gda_statement_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_STRUCTURE
};

/* module error */
GQuark gda_statement_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_statement_error");
	return quark;
}


static void
gda_statement_class_init (GdaStatementClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	/**
	 * GdaStatement::reset:
	 * @stmt: the #GdaStatement object
	 *
	 * Gets emitted whenever the @stmt has changed
	 */
	gda_statement_signals[RESET] =
		g_signal_new ("reset",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaStatementClass, reset),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	/**
	 * GdaStatement::checked:
	 * @stmt: the #GdaStatement object
	 * @cnc: a #GdaConnection
	 * @checked: whether @stmt have been verified
	 *
	 * Gets emitted whenever the structure and contents
	 * of @stmt have been verified (emitted after gda_statement_check_validity()).
	 */
	gda_statement_signals[CHECKED] =
		g_signal_new ("checked",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaStatementClass, checked),
			      NULL, NULL,
			      _gda_marshal_VOID__OBJECT_BOOLEAN, G_TYPE_NONE,
			      2, GDA_TYPE_CONNECTION, G_TYPE_BOOLEAN);

	klass->reset = NULL;
	klass->checked = NULL;

	object_class->dispose = gda_statement_dispose;

	/* Properties */
	object_class->set_property = gda_statement_set_property;
	object_class->get_property = gda_statement_get_property;
	g_object_class_install_property (object_class, PROP_STRUCTURE,
					 g_param_spec_boxed ("structure", NULL, NULL, GDA_TYPE_SQL_STATEMENT,
							       G_PARAM_WRITABLE | G_PARAM_READABLE));
}

static void
gda_statement_init (GdaStatement * stmt)
{
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	priv->internal_struct = NULL;
	priv->requested_types = NULL;
}

/**
 * gda_statement_new:
 *
 * Creates a new #GdaStatement object
 *
 * Returns: the new object
 */
GdaStatement*
gda_statement_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_STATEMENT, NULL);
	return GDA_STATEMENT (obj);
}

GdaSqlStatement *
_gda_statement_get_internal_struct (GdaStatement *stmt)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	return priv->internal_struct;
}

/**
 * gda_statement_copy:
 * @orig: a #GdaStatement to make a copy of
 * 
 * Copy constructor
 *
 * Returns: (transfer full): a the new copy of @orig
 */
GdaStatement *
gda_statement_copy (GdaStatement *orig)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_STATEMENT (orig), NULL);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (orig);

	obj = g_object_new (GDA_TYPE_STATEMENT, "structure", priv->internal_struct, NULL);
	return GDA_STATEMENT (obj);
}

static void
gda_statement_dispose (GObject *object)
{
	GdaStatement *stmt;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_STATEMENT (object));
	stmt = GDA_STATEMENT (object);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	if (priv->requested_types != NULL) {
		g_free (priv->requested_types);
		priv->requested_types = NULL;
	}
	if (priv->internal_struct != NULL) {
		gda_sql_statement_free (priv->internal_struct);
		priv->internal_struct = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gda_statement_parent_class)->dispose (object);
}

static void
gda_statement_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaStatement *stmt;

	stmt = GDA_STATEMENT (object);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	switch (param_id) {
		case PROP_STRUCTURE:
			if (priv->internal_struct) {
				gda_sql_statement_free (priv->internal_struct);
				priv->internal_struct = NULL;
			}
			if (priv->requested_types) {
				g_free (priv->requested_types);
				priv->requested_types = NULL;
			}
			priv->internal_struct = g_value_dup_boxed (value);
			g_signal_emit (stmt, gda_statement_signals [RESET], 0);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_statement_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaStatement *stmt;
	stmt = GDA_STATEMENT (object);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	switch (param_id) {
		case PROP_STRUCTURE:
			g_value_set_boxed (value, priv->internal_struct);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_statement_get_statement_type:
 * @stmt: a #GdaStatement object
 *
 * Get the type of statement held by @stmt. It returns GDA_SQL_STATEMENT_NONE if
 * @stmt does not hold any statement
 *
 * Returns: (transfer none): the statement type
 */
GdaSqlStatementType
gda_statement_get_statement_type (GdaStatement *stmt)
{
	g_return_val_if_fail (stmt != NULL, GDA_SQL_STATEMENT_NONE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), GDA_SQL_STATEMENT_NONE);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	g_return_val_if_fail (priv, GDA_SQL_STATEMENT_NONE);

	if (priv->internal_struct)
		return priv->internal_struct->stmt_type;

	return GDA_SQL_STATEMENT_NONE;
}

/**
 * gda_statement_is_useless:
 * @stmt: a #GdaStatement object
 *
 * Tells if @stmt is composed only of spaces (that is it has no real SQL code), and is completely
 * useless as such.
 *
 * Returns: TRUE if executing @stmt does nothing
 */
gboolean
gda_statement_is_useless (GdaStatement *stmt)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	g_return_val_if_fail (priv, FALSE);

	if (priv->internal_struct &&
	    priv->internal_struct->stmt_type == GDA_SQL_STATEMENT_UNKNOWN) {
		GSList *list;
		GdaSqlStatementUnknown *unknown;
		unknown = (GdaSqlStatementUnknown*) priv->internal_struct->contents;
		for (list = unknown->expressions; list; list = list->next) {
			GdaSqlExpr *expr = (GdaSqlExpr *) list->data;
			if (expr->param_spec)
				return FALSE;
			if (expr->value) {
				if (G_VALUE_TYPE (expr->value) == G_TYPE_STRING) {
					const gchar *str;
					for (str = g_value_get_string (expr->value); 
					     (*str == ' ') || (*str == '\t') || (*str == '\n') || 
						     (*str == '\f') || (*str == '\r'); str++);
					if (*str)
						return FALSE;
				}
				else {
					TO_IMPLEMENT;
					return FALSE;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * gda_statement_check_structure:
 * @stmt: a #GdaStatement object
 * @error: a place to store errors, or %NULL
 * 
 * Checks that @stmt's structure is correct.
 *
 * Returns: TRUE if @stmt's structure is correct
 */
gboolean
gda_statement_check_structure (GdaStatement *stmt, GError **error)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	return gda_sql_statement_check_structure (priv->internal_struct, error);
}

/**
 * gda_statement_check_validity:
 * @stmt: a #GdaStatement object
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * If @cnc is not %NULL then checks that every object (table, field, function) used in @stmt 
 * actually exists in @cnc's database
 *
 * If @cnc is %NULL, then cleans anything related to @cnc in @stmt.
 *
 * See gda_sql_statement_check_validity() for more information.
 *
 * Returns: TRUE if every object actually exists in @cnc's database
 */
gboolean
gda_statement_check_validity (GdaStatement *stmt, GdaConnection *cnc, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), FALSE);

	retval = gda_sql_statement_check_validity (priv->internal_struct, cnc, error);
	g_signal_emit (stmt, gda_statement_signals [CHECKED], 0, cnc, retval);

	return retval;
}

/**
 * gda_statement_normalize:
 * @stmt: a #GdaStatement object
 * @cnc: a #GdaConnection object
 * @error: a place to store errors, or %NULL
 *
 * "Normalizes" some parts of @stmt, see gda_sql_statement_normalize() for more
 * information.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_statement_normalize (GdaStatement *stmt, GdaConnection *cnc, GError **error)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	return gda_sql_statement_normalize (priv->internal_struct, cnc, error);
}

/**
 * gda_statement_serialize:
 * @stmt: a #GdaStatement object
 *
 * Creates a string representing the contents of @stmt.
 *
 * Returns: a string containing the serialized version of @stmt
 */
gchar *
gda_statement_serialize (GdaStatement *stmt)
{
	gchar *str;
	GString *string;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	string = g_string_new ("{");
	g_string_append (string, "\"statement\":");
	str = gda_sql_statement_serialize (priv->internal_struct);
	if (str) {
		g_string_append (string, str);
		g_free (str);
	}
	else
		g_string_append (string, "null");
	g_string_append_c (string, '}');

	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

static gboolean
get_params_foreach_func (GdaSqlAnyPart *node, GdaSet **params, GError **error)
{
	GdaSqlParamSpec *pspec;
	if (!node) return TRUE;

	if ((node->type == GDA_SQL_ANY_EXPR) &&
	    (pspec = ((GdaSqlExpr*) node)->param_spec)) {
		GdaHolder *h;

		if (pspec->g_type == GDA_TYPE_NULL) {
			g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_PARAM_TYPE_ERROR,
				     _("Could not determine GType for parameter '%s'"),
				     pspec->name ? pspec->name : _("Unnamed"));
			return FALSE;
		}
		if (!*params) 
			*params = gda_set_new (NULL);
		h = gda_holder_new (pspec->g_type, pspec->name);
		g_object_set (G_OBJECT (h), "name", pspec->name,
			      "description", pspec->descr, NULL);
		gda_holder_set_not_null (h, ! pspec->nullok);
		if (((GdaSqlExpr*) node)->value) {
			/* Expr's value is "SQL encoded" => we need to convert it to a real value */
			GValue *evalue;
			evalue = ((GdaSqlExpr*) node)->value;
			if (G_VALUE_TYPE (evalue) == G_TYPE_STRING) {
				GdaDataHandler *dh;
				GValue *value;
				dh = gda_data_handler_get_default (pspec->g_type);
				if (!dh) {
					g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_PARAM_TYPE_ERROR,
						     _("Can't handle default value of type '%s'"),
						     gda_g_type_to_string (pspec->g_type));
					g_object_unref (h);
					return FALSE;
				}
				value = gda_data_handler_get_value_from_sql (dh,
									     g_value_get_string (evalue),
									     pspec->g_type);
				g_object_unref (dh);

				if (!value)
					value = gda_value_new_default (g_value_get_string (evalue));
				gda_holder_set_default_value (h, value);
				gda_value_free (value);
			}
			else
				gda_holder_set_default_value (h, ((GdaSqlExpr*) node)->value);
			gda_holder_set_value_to_default (h);
		}
		gda_set_add_holder (*params, h);
		g_object_unref (h);
	}
	return TRUE;
}

/**
 * gda_statement_get_parameters:
 * @stmt: a #GdaStatement object
 * @out_params: (out) (nullable) (transfer full): a place to store a new #GdaSet object, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Get a new #GdaSet object which groups all the execution parameters
 * which @stmt needs. This new object is returned though @out_params.
 *
 * Note that if @stmt does not need any parameter, then @out_params is set to %NULL.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_statement_get_parameters (GdaStatement *stmt, GdaSet **out_params, GError **error)
{
	GdaSet *set = NULL;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	if (out_params)
		*out_params = NULL;

	if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (priv->internal_struct->contents),
				       (GdaSqlForeachFunc) get_params_foreach_func, &set, error)) {
		if (set) {
			g_object_unref (set);
			set = NULL;
		}
		return FALSE;
	}
	
	if (out_params)
		*out_params = set;
	else
		g_object_unref (set);
	return TRUE;
}

/*
 * _gda_statement_get_requested_types:
 * @stmt: a #GdaStatement
 *
 * Returns: a new #GType, suitable to use with gda_connection_statement_execute_select_full(), or %NULL
 */
const GType *
_gda_statement_get_requested_types (GdaStatement *stmt)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);
	if (! priv->internal_struct)
		return NULL;
	if (priv->requested_types)
		return priv->requested_types;
	if (priv->internal_struct->stmt_type != GDA_SQL_STATEMENT_SELECT)
		return NULL;

	GdaSqlStatementSelect *selst;
	GSList *list;
	GArray *array = NULL;
 rewind:
	selst = (GdaSqlStatementSelect*) priv->internal_struct->contents;
	for (list = selst->expr_list; list; list = list->next) {
		GdaSqlExpr *expr;
		GType type = G_TYPE_INVALID;
		expr = ((GdaSqlSelectField*) list->data)->expr;
		if (expr->cast_as && *expr->cast_as)
			type = gda_g_type_from_string (expr->cast_as);
		if (array) {
			if (type == G_TYPE_INVALID)
				type = 0;
			g_array_append_val (array, type);
		}
		else if (type != G_TYPE_INVALID) {
			array = g_array_new (TRUE, FALSE, sizeof (GType));
			goto rewind;
		}
	}
	if (array) {
		GType *retval;
		guint len;
		len = array->len;
		retval = (GType*) g_array_free (array, FALSE);
		retval [len] = G_TYPE_NONE;
		priv->requested_types = retval;
		return retval;
	}
	else
		return NULL;
}


/*
 * SQL rendering
 */
static gchar *default_render_value (const GValue *value, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_param_spec (GdaSqlParamSpec *pspec, GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
					 gboolean *is_default, gboolean *is_null, GError **error);

static gchar *default_render_unknown (GdaSqlStatementUnknown *stmt, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_select (GdaSqlStatementSelect *stmt, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_insert (GdaSqlStatementInsert *stmt, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_delete (GdaSqlStatementDelete *stmt, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_update (GdaSqlStatementUpdate *stmt, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_compound (GdaSqlStatementCompound *stmt, GdaSqlRenderingContext *context, GError **error);

static gchar *default_render_expr (GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
				   gboolean *is_default, gboolean *is_null, GError **error);
static gchar *default_render_table (GdaSqlTable *table, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_field (GdaSqlField *field, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_function (GdaSqlFunction *func, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_operation (GdaSqlOperation *op, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_case (GdaSqlCase *case_s, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_select_field (GdaSqlSelectField *field, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_select_target (GdaSqlSelectTarget *target, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_select_join (GdaSqlSelectJoin *join, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_select_from (GdaSqlSelectFrom *from, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_select_order (GdaSqlSelectOrder *order, GdaSqlRenderingContext *context, GError **error);
static gchar *default_render_distinct (GdaSqlStatementSelect *select, GdaSqlRenderingContext *context, GError **error);

/**
 * gda_statement_to_sql_real:
 * @stmt: a #GdaStatement object
 * @context: a #GdaSqlRenderingContext context
 * @error: a place to store errors, or %NULL
 *
 * Renders @stmt to its SQL representation, using @context to specify how each part of @stmt must
 * be rendered. This function is mainly used by database provider's implementations which require
 * to specialize some aspects of SQL rendering to be adapted to the database,'s own SQL dialect
 * (for example SQLite rewrites the 'FALSE' and 'TRUE' literals as '0' and 'NOT 0').
 * 
 * Returns: (transfer full): a new string, or %NULL if an error occurred
 */
gchar *
gda_statement_to_sql_real (GdaStatement *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GdaSqlStatementContentsInfo *cinfo;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	GdaStatementPrivate *priv = gda_statement_get_instance_private (stmt);

	if (!context->render_value) 
		context->render_value = default_render_value;
	if (!context->render_param_spec) 
		context->render_param_spec = default_render_param_spec;
	if (!context->render_expr) 
		context->render_expr = default_render_expr;

	if (!context->render_unknown) 
		context->render_unknown = (GdaSqlRenderingFunc) default_render_unknown;
	if (!context->render_select) 
		context->render_select = (GdaSqlRenderingFunc) default_render_select;
	if (!context->render_insert) 
		context->render_insert = (GdaSqlRenderingFunc) default_render_insert;
	if (!context->render_delete) 
		context->render_delete = (GdaSqlRenderingFunc) default_render_delete;
	if (!context->render_update) 
		context->render_update = (GdaSqlRenderingFunc) default_render_update;
	if (!context->render_compound) 
		context->render_compound = (GdaSqlRenderingFunc) default_render_compound;

	if (!context->render_table) 
		context->render_table = (GdaSqlRenderingFunc) default_render_table;
	if (!context->render_field) 
		context->render_field = (GdaSqlRenderingFunc) default_render_field;
	if (!context->render_function) 
		context->render_function = (GdaSqlRenderingFunc) default_render_function;
	if (!context->render_operation) 
		context->render_operation = (GdaSqlRenderingFunc) default_render_operation;
	if (!context->render_case) 
		context->render_case = (GdaSqlRenderingFunc) default_render_case;
	if (!context->render_select_field)
		context->render_select_field = (GdaSqlRenderingFunc) default_render_select_field;
	if (!context->render_select_target)
		context->render_select_target = (GdaSqlRenderingFunc) default_render_select_target;
	if (!context->render_select_join)
		context->render_select_join = (GdaSqlRenderingFunc) default_render_select_join;
	if (!context->render_select_from)
		context->render_select_from = (GdaSqlRenderingFunc) default_render_select_from;
	if (!context->render_select_order)
		context->render_select_order = (GdaSqlRenderingFunc) default_render_select_order;
	if (!context->render_distinct)
		context->render_distinct = (GdaSqlRenderingFunc) default_render_distinct;

	cinfo = gda_sql_statement_get_contents_infos (priv->internal_struct->stmt_type);
	if (cinfo->check_structure_func && !cinfo->check_structure_func (GDA_SQL_ANY_PART (priv->internal_struct->contents),
									 NULL, error))
		return NULL;

	switch (GDA_SQL_ANY_PART (priv->internal_struct->contents)->type) {
	case GDA_SQL_ANY_STMT_UNKNOWN:
		return context->render_unknown (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
	case GDA_SQL_ANY_STMT_BEGIN:
		if (context->render_begin)
			return context->render_begin (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
		break;
	case GDA_SQL_ANY_STMT_ROLLBACK:
		if (context->render_rollback)
			return context->render_rollback (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
		break;
        case GDA_SQL_ANY_STMT_COMMIT:
		if (context->render_commit)
			return context->render_commit (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
		break;
        case GDA_SQL_ANY_STMT_SAVEPOINT:
		if (context->render_savepoint)
			return context->render_savepoint (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
		break;
        case GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT:
		if (context->render_rollback_savepoint)
			return context->render_rollback_savepoint (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
		break;
        case GDA_SQL_ANY_STMT_DELETE_SAVEPOINT:
		if (context->render_delete_savepoint)
			return context->render_delete_savepoint (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
		break;
	case GDA_SQL_ANY_STMT_SELECT:
		return context->render_select (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
	case GDA_SQL_ANY_STMT_INSERT:
		return context->render_insert (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
	case GDA_SQL_ANY_STMT_DELETE:
		return context->render_delete (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
	case GDA_SQL_ANY_STMT_UPDATE:
		return context->render_update (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
	case GDA_SQL_ANY_STMT_COMPOUND:
		return context->render_compound (GDA_SQL_ANY_PART (priv->internal_struct->contents), context, error);
	default:
		TO_IMPLEMENT;
		return NULL;
		break;
	}

	/* default action is to use priv->internal_struct->sql */
	if (priv->internal_struct->sql)
		return g_strdup (priv->internal_struct->sql);
	else {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Missing SQL code"));
		return NULL;
	}
}

static gchar *
default_render_value (const GValue *value, GdaSqlRenderingContext *context, GError **error)
{
	if (value && !gda_value_is_null (value)) {
		GdaDataHandler *dh;
		if (context->provider) {
			dh = gda_server_provider_get_data_handler_g_type (context->provider, context->cnc, G_VALUE_TYPE (value));
			g_object_ref (dh); // dh here is not a full transfer.
		}
		else  			
			dh = gda_data_handler_get_default (G_VALUE_TYPE (value));

		if (!dh) {
			if (g_type_is_a (G_VALUE_TYPE (value), GDA_TYPE_DEFAULT))
				return g_strdup ("DEFAULT");
			else {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("No data handler for type '%s'"),
					     g_type_name (G_VALUE_TYPE (value)));
				return NULL;
			}
		}
		if (context->flags & GDA_STATEMENT_SQL_TIMEZONE_TO_GMT) {
			if (g_type_is_a (G_VALUE_TYPE (value), GDA_TYPE_TIME)) {
				GdaTime *t;
        GdaTime *nt;
				t = g_value_get_boxed (value);

        if (t != NULL) {
          nt = gda_time_to_utc (t);
			    GValue v = {0};
			    g_value_init (&v, GDA_TYPE_TIME);
			    g_value_take_boxed (&v, nt);
				  gchar *tmp;
				  tmp = gda_data_handler_get_sql_from_value (dh, &v);
				  g_value_reset (&v);
				  return tmp;
        }
			}
			else if (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_DATE_TIME)) {
				GDateTime *nts;
				nts = (GDateTime*) g_value_get_boxed (value);
				if (nts != NULL) {
					nts = (GDateTime*) g_date_time_to_utc ((GDateTime*) nts);
          if (nts != NULL) {
					  GValue v = {0};
					  g_value_init (&v, G_TYPE_DATE_TIME);
					  g_value_set_boxed (&v, nts);
					  gchar *tmp;
					  tmp = gda_data_handler_get_sql_from_value (dh, &v);
					  g_value_reset (&v);
					  return tmp;
          }
				}
			}
		}

		gchar *res;

		res = gda_data_handler_get_sql_from_value (dh, value);
		g_object_unref (dh);

		return res;
	}
	else
		return g_strdup ("NULL");
}

/**
 * gda_statement_to_sql_extended:
 * @stmt: a #GdaStatement object
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @params: (nullable): parameters contained in a single #GdaSet object, or %NULL
 * @flags: a set of flags to control the rendering
 * @params_used: (element-type GdaHolder) (out) (transfer container) (nullable):a place to store the list of actual #GdaHolder objects in @params used to do the rendering, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Renders @stmt as an SQL statement, with some control on how it is rendered.
 *
 * If @cnc is not %NULL, then the rendered SQL will better be suited to be used by @cnc (in particular
 * it may include some SQL tweaks and/or proprietary extensions specific to the database engine used by @cnc):
 * in this case the result is similar to calling gda_connection_statement_to_sql().
 *
 * Returns: (transfer full): a new string if no error occurred
 */
gchar *
gda_statement_to_sql_extended (GdaStatement *stmt, GdaConnection *cnc, GdaSet *params,
			       GdaStatementSqlFlag flags, 
			       GSList **params_used, GError **error)
{
	gchar *str;
	GdaSqlRenderingContext context;

	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	memset (&context, 0, sizeof (context));
	context.params = params;
	context.flags = flags;
	if (cnc) {
		if (gda_connection_is_opened (cnc))
			return _gda_server_provider_statement_to_sql (gda_connection_get_provider (cnc), cnc,
								      stmt, params, flags,
								      params_used, error);
		else
			context.provider = gda_connection_get_provider (cnc);
	}

	str = gda_statement_to_sql_real (stmt, &context, error);

	if (str) {
		if (params_used)
			*params_used = context.params_used;
		else
			g_slist_free (context.params_used);
	}
	else {
		if (params_used)
			*params_used = NULL;
		g_slist_free (context.params_used);
	}
	return str;
}

static gchar *
default_render_unknown (GdaSqlStatementUnknown *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_UNKNOWN, NULL);

	string = g_string_new ("");
	for (list = stmt->expressions; list; list = list->next) {
		str = context->render_expr ((GdaSqlExpr*) list->data, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_insert (GdaSqlStatementInsert *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;
	gboolean pretty = context->flags & GDA_STATEMENT_SQL_PRETTY;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_INSERT, NULL);

	string = g_string_new ("INSERT ");
	
	/* conflict algo */
	if (stmt->on_conflict)
		g_string_append_printf (string, "OR %s ", stmt->on_conflict);

	/* INTO */
	g_string_append (string, "INTO ");
	str = context->render_table (GDA_SQL_ANY_PART (stmt->table), context, error);
	if (!str) goto err;
	g_string_append (string, str);
	g_free (str);

	/* fields list */
	for (list = stmt->fields_list; list; list = list->next) {
		if (list == stmt->fields_list)
			g_string_append (string, " (");
		else
			g_string_append (string, ", ");
		str = context->render_field (GDA_SQL_ANY_PART (list->data), context, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}
	if (stmt->fields_list)
		g_string_append_c (string, ')');

	/* values */
	if (stmt->select) {
		if (pretty)
			g_string_append_c (string, '\n');
		else
			g_string_append_c (string, ' ');
		str = context->render_select (GDA_SQL_ANY_PART (stmt->select), context, error);
		if (!str) goto err;
		g_string_append (string, str);
			g_free (str);
	}
	else {
		for (list = stmt->values_list; list; list = list->next) {
			GSList *rlist;
			if (list == stmt->values_list) {
				if (pretty)
					g_string_append (string, "\nVALUES");
				else
					g_string_append (string, " VALUES");
			}
			else
				g_string_append_c (string, ',');
			for (rlist = (GSList*) list->data; rlist; rlist = rlist->next) {
				if (rlist == (GSList*) list->data)
					g_string_append (string, " (");
				else
					g_string_append (string, ", ");
				str = context->render_expr ((GdaSqlExpr*) rlist->data, context, NULL, NULL, error);
				if (!str) goto err;
				if (pretty && (rlist != (GSList*) list->data))
					g_string_append (string, "\n\t");
				g_string_append (string, str);
				g_free (str);
			}
			g_string_append_c (string, ')');
		}

		if (!stmt->fields_list && !stmt->values_list)
			g_string_append (string, " DEFAULT VALUES");
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;	
}

static gchar *
default_render_delete (GdaSqlStatementDelete *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_DELETE, NULL);

	string = g_string_new ("DELETE FROM ");
	
	/* FROM */
	str = context->render_table (GDA_SQL_ANY_PART (stmt->table), context, error);
	if (!str) goto err;
	g_string_append (string, str);
	g_free (str);

	/* cond */
	if (stmt->cond) {
		g_string_append (string, " WHERE ");
		str = context->render_expr (stmt->cond, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}	

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;	
}

static gchar *
default_render_update (GdaSqlStatementUpdate *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *flist, *elist;
	gboolean pretty = context->flags & GDA_STATEMENT_SQL_PRETTY;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_UPDATE, NULL);

	string = g_string_new ("UPDATE ");
	/* conflict algo */
	if (stmt->on_conflict)
		g_string_append_printf (string, "OR %s ", stmt->on_conflict);

	/* table */
	str = context->render_table (GDA_SQL_ANY_PART (stmt->table), context, error);
	if (!str) goto err;
	g_string_append (string, str);
	g_free (str);

	/* columns set */
	if (pretty)
		g_string_append (string, "\nSET ");
	else
		g_string_append (string, " SET ");
	for (flist = stmt->fields_list, elist = stmt->expr_list;
	     flist && elist;
	     flist = flist->next, elist = elist->next) {
		if (flist != stmt->fields_list) {
			g_string_append (string, ", ");
			if (pretty)
				g_string_append (string, "\n\t");
		}
		str = context->render_field (GDA_SQL_ANY_PART (flist->data), context, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
		g_string_append_c (string, '=');
		str = context->render_expr ((GdaSqlExpr *) elist->data, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	/* cond */
	if (stmt->cond) {
		if (pretty)
			g_string_append (string, "\nWHERE ");
		else
			g_string_append (string, " WHERE ");
		str = context->render_expr (stmt->cond, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}	

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_compound (GdaSqlStatementCompound *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_COMPOUND, NULL);

	string = g_string_new ("");

	for (list = stmt->stmt_list; list; list = list->next) {
		GdaSqlStatement *sqlstmt = (GdaSqlStatement*) list->data;
		if (list != stmt->stmt_list) {
			switch (stmt->compound_type) {
			case GDA_SQL_STATEMENT_COMPOUND_UNION:
				g_string_append (string, " UNION ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_UNION_ALL:
				g_string_append (string, " UNION ALL ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_INTERSECT:
				g_string_append (string, " INTERSECT ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_INTERSECT_ALL:
				g_string_append (string, " INTERSECT ALL ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_EXCEPT:
				g_string_append (string, " EXCEPT ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_EXCEPT_ALL:
				g_string_append (string, " EXCEPT ALL ");
				break;
			default:
				g_assert_not_reached ();
			}
		}
		switch (sqlstmt->stmt_type) {
		case GDA_SQL_ANY_STMT_SELECT:
			str = context->render_select (GDA_SQL_ANY_PART (sqlstmt->contents), context, error);
			if (!str) goto err;
			g_string_append_c (string, '(');
			g_string_append (string, str);
			g_string_append_c (string, ')');
			g_free (str);
			break;
		case GDA_SQL_ANY_STMT_COMPOUND:
			str = context->render_compound (GDA_SQL_ANY_PART (sqlstmt->contents), context, error);
			if (!str) goto err;
			g_string_append_c (string, '(');
			g_string_append (string, str);
			g_string_append_c (string, ')');
			g_free (str);
			break;
		default:
			g_assert_not_reached ();
		}
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;	
}

static gchar *
default_render_distinct (GdaSqlStatementSelect *stmt, GdaSqlRenderingContext *context, GError **error)
{
	gboolean pretty = context->flags & GDA_STATEMENT_SQL_PRETTY;
	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_SELECT, NULL);

	if (stmt->distinct) {
		GString *string;
		string = g_string_new ("DISTINCT");
		if (stmt->distinct_expr) {
			gchar *str;
			str = context->render_expr (stmt->distinct_expr, context, NULL, NULL, error);
			if (!str) {
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, " ON (");
			g_string_append (string, str);
			g_string_append (string, ") ");
			g_free (str);
		}
		if (pretty)
			g_string_append_c (string, '\n');
		return g_string_free (string, FALSE);
	}
	else
		return NULL;
}

static gchar *
default_render_select (GdaSqlStatementSelect *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;
	gboolean pretty = context->flags & GDA_STATEMENT_SQL_PRETTY;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_SELECT, NULL);

	string = g_string_new ("SELECT ");
	/* distinct */
	if (stmt->distinct) {
		str = context->render_distinct (GDA_SQL_ANY_PART (stmt), context, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_string_append_c (string, ' ');
		g_free (str);
	}
	
	/* selected expressions */
	for (list = stmt->expr_list; list; list = list->next) {
		str = context->render_select_field (GDA_SQL_ANY_PART (list->data), context, error);
		if (!str) goto err;
		if (list != stmt->expr_list)
			g_string_append (string, ", ");
		if (pretty)
			g_string_append (string, "\n\t");
		g_string_append (string, str);
		g_free (str);
	}

	/* FROM */
	if (stmt->from) {
		str = context->render_select_from (GDA_SQL_ANY_PART (stmt->from), context, error);
		if (!str) goto err;
		if (pretty)
			g_string_append_c (string, '\n');
		else
			g_string_append_c (string, ' ');
		g_string_append (string, str);
		g_free (str);
	}

	/* WHERE */
	if (stmt->where_cond) {
		if (pretty)
			g_string_append (string, "\nWHERE ");
		else
			g_string_append (string, " WHERE ");
		str = context->render_expr (stmt->where_cond, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	/* GROUP BY */
	for (list = stmt->group_by; list; list = list->next) {
		str = context->render_expr (list->data, context, NULL, NULL, error);
		if (!str) goto err;
		if (list != stmt->group_by)
			g_string_append (string, ", ");
		else {
			if (pretty)
				g_string_append (string, "\nGROUP BY ");
			else
				g_string_append (string, " GROUP BY ");
		}
		g_string_append (string, str);
		g_free (str);
	}

	/* HAVING */
	if (stmt->having_cond) {
		if (pretty)
			g_string_append (string, "\nHAVING ");
		else
			g_string_append (string, " HAVING ");
		str = context->render_expr (stmt->having_cond, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	/* ORDER BY */
	for (list = stmt->order_by; list; list = list->next) {
		str = context->render_select_order (GDA_SQL_ANY_PART (list->data), context, error);
		if (!str) goto err;
		if (list != stmt->order_by)
			g_string_append (string, ", ");
		else {
			if (pretty)
				g_string_append (string, "\nORDER BY ");
			else
				g_string_append (string, " ORDER BY ");
		}
		g_string_append (string, str);
		g_free (str);
	}

	/* LIMIT */
	if (stmt->limit_count) {
		g_string_append (string, " LIMIT ");
		str = context->render_expr (stmt->limit_count, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
		if (stmt->limit_offset) {
			g_string_append (string, " OFFSET ");
			str = context->render_expr (stmt->limit_offset, context, NULL, NULL, error);
			if (!str) goto err;
			g_string_append (string, str);
			g_free (str);
		}
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

/*
 * Randers @pspec, and the default value stored in @expr->value if it exists
 */
static gchar *
default_render_param_spec (GdaSqlParamSpec *pspec, GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
			   gboolean *is_default, gboolean *is_null, GError **error)
{
	GString *string;
	gchar *str;
	GdaHolder *h = NULL;
	gboolean render_pspec; /* if TRUE, then don't render parameter as its value but as a param specification */

	g_return_val_if_fail (pspec, NULL);

	render_pspec = FALSE;
	if (context->flags & (GDA_STATEMENT_SQL_PARAMS_LONG |
			      GDA_STATEMENT_SQL_PARAMS_SHORT |
			      GDA_STATEMENT_SQL_PARAMS_AS_COLON |
			      GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR |
			      GDA_STATEMENT_SQL_PARAMS_AS_QMARK |
			      GDA_STATEMENT_SQL_PARAMS_AS_UQMARK))
		render_pspec = TRUE;

	if (is_default)
		*is_default = FALSE;
	if (is_null)
		*is_null = FALSE;

	string = g_string_new ("");

	/* try to use a GdaHolder in context->params */
	if (context->params) {
		h = gda_set_get_holder (context->params, pspec->name);
		if (h && (gda_holder_get_g_type (h) != pspec->g_type)) {
			g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_PARAM_ERROR,
				     _("Wrong parameter type for '%s': expected type '%s' and got '%s'"), 
				     pspec->name, g_type_name (pspec->g_type), g_type_name (gda_holder_get_g_type (h)));
			goto err;
		}
	}
	if (!h && 
	    (!render_pspec ||
	     (context->flags & (GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR | 
				GDA_STATEMENT_SQL_PARAMS_AS_QMARK |
				GDA_STATEMENT_SQL_PARAMS_AS_UQMARK)))) {
		/* a real value is needed or @context->params_used needs to be correct, and no GdaHolder found */
		g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_PARAM_ERROR,
			     _("Missing parameter '%s'"), pspec->name);
		goto err;
	}
	if (h) {
		/* keep track of the params used */
		context->params_used = g_slist_append (context->params_used, h);

		if (! render_pspec) {
			const GValue *cvalue;
			
			if (!gda_holder_is_valid (h)) {
				g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_PARAM_ERROR,
					     _("Parameter '%s' is invalid"), pspec->name);
				goto err;
			}
			cvalue = gda_holder_get_value (h);
			if (cvalue) {
				str = context->render_value ((GValue*) cvalue, context, error);
				if (!str) goto err;
				g_string_append (string, str);
				g_free (str);
				if (is_null && gda_value_is_null (cvalue))
					*is_null = TRUE;
			}
			else {
				/* @h is set to a default value */
				g_string_append (string, "DEFAULT");
				if (is_default)
					*is_default = TRUE;
			}
			goto out;
		}
	}

	/* no parameter found in context->params => insert an SQL parameter */
	if (context->flags & GDA_STATEMENT_SQL_PARAMS_AS_COLON) {
		gchar *str;

		str = gda_text_to_alphanum (pspec->name);
		g_string_append_printf (string, ":%s", str);
		g_free (str);
	}
	else if (context->flags & (GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR | 
				   GDA_STATEMENT_SQL_PARAMS_AS_QMARK |
				   GDA_STATEMENT_SQL_PARAMS_AS_UQMARK)) {
		if (context->flags & GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR)
			g_string_append_printf (string, "$%d", g_slist_length (context->params_used));
		else if (context->flags & GDA_STATEMENT_SQL_PARAMS_AS_QMARK)
			g_string_append_printf (string, "?%d", g_slist_length (context->params_used));
		else
			g_string_append_c (string, '?');
	}
	else {
		GdaStatementSqlFlag flag = context->flags;
		gchar *quoted_pname;

		if (! pspec->name) {
			g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_PARAM_ERROR,
				     "%s", _("Unnamed parameter"));
			goto err;
		}
		quoted_pname = gda_sql_identifier_force_quotes (pspec->name);

		if (! (flag & (GDA_STATEMENT_SQL_PARAMS_LONG | GDA_STATEMENT_SQL_PARAMS_SHORT))) {
			if (!expr->value || gda_value_is_null (expr->value) || strcmp (quoted_pname, pspec->name))
				flag = GDA_STATEMENT_SQL_PARAMS_LONG;
			else
				flag = GDA_STATEMENT_SQL_PARAMS_SHORT;
		}

		if (flag & GDA_STATEMENT_SQL_PARAMS_LONG) {
			if (expr->value) {
				if (G_VALUE_TYPE (expr->value) == G_TYPE_STRING)
					str = g_value_dup_string (expr->value);
				else
					str = context->render_value (expr->value, context, error);
				if (!str) {
					g_free (quoted_pname);
					goto err;
				}
				g_string_append (string, str);
				g_free (str);
			}
			else
				g_string_append (string, "##");
			
			g_string_append (string, " /* ");
			g_string_append_printf (string, "name:%s", quoted_pname);
			if (pspec->g_type) {
				str = gda_sql_identifier_force_quotes (gda_g_type_to_string (pspec->g_type));
				g_string_append_printf (string, " type:%s", str);
				g_free (str);
			}
			if (pspec->descr) {
				str = gda_sql_identifier_force_quotes (pspec->descr);
				g_string_append_printf (string, " descr:%s", str);
				g_free (str);
			}
			if (pspec->nullok) 
				g_string_append (string, " nullok:true");

			g_string_append (string, " */");
		}
		else {
			g_string_append (string, "##");
			g_string_append (string, pspec->name);
			if (pspec->g_type != GDA_TYPE_NULL) {
				g_string_append (string, "::");
				g_string_append (string, gda_g_type_to_string (pspec->g_type));
				if (pspec->nullok) 
					g_string_append (string, "::NULL");
			}
		}

		g_free (quoted_pname);
	}

 out:
	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_expr (GdaSqlExpr *expr, GdaSqlRenderingContext *context, gboolean *is_default,
		     gboolean *is_null, GError **error)
{
	GString *string;
	gchar *str = NULL;

	g_return_val_if_fail (expr, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (expr)->type == GDA_SQL_ANY_EXPR, NULL);

	if (is_default)
		*is_default = FALSE;
	if (is_null)
		*is_null = FALSE;

	/* can't have:
	 *  - expr->cast_as && expr->param_spec
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (expr), error)) return NULL;

	string = g_string_new ("");
	if (expr->param_spec) {
		str = context->render_param_spec (expr->param_spec, expr, context, is_default, is_null, error);
		if (!str) goto err;
	}
	else if (expr->value) {
		if (G_VALUE_TYPE (expr->value) == G_TYPE_STRING) {
			/* specific treatment for strings, see documentation about GdaSqlExpr's value attribute */
			const gchar *vstr;
			vstr = g_value_get_string (expr->value);
			if (vstr) {
				if (expr->value_is_ident) {
					gchar **ids_array;
					gint i;
					GString *string = NULL;
					GdaConnectionOptions cncoptions = 0;
					if (context->cnc)
						g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
					ids_array = gda_sql_identifier_split (vstr);
					if (!ids_array)
						str = g_strdup (vstr);
					else if (!(ids_array[0])) goto err;
					else {
						for (i = 0; ids_array[i]; i++) {
							gchar *tmp;
							if (!string)
								string = g_string_new ("");
							else
								g_string_append_c (string, '.');
							tmp = gda_sql_identifier_quote (ids_array[i], context->cnc,
											context->provider, FALSE,
											cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
							g_string_append (string, tmp);
							g_free (tmp);
						}
						g_strfreev (ids_array);
						str = g_string_free (string, FALSE);
					}
				}
				else {
					/* we don't have an identifier */
					if (!g_ascii_strcasecmp (vstr, "default")) {
						if (is_default)
							*is_default = TRUE;
						str = g_strdup ("DEFAULT");
					}
					else
						str = g_strdup (vstr);
				}
			}
			else {
				str = g_strdup ("NULL");
				if (is_null)
					*is_null = TRUE;
			}
		}
		if (!str) {
			/* use a GdaDataHandler to render the value as valid SQL */
			GdaDataHandler *dh;
			if (context->cnc) {
				GdaServerProvider *prov;
				prov = gda_connection_get_provider (context->cnc);
				dh = gda_server_provider_get_data_handler_g_type (prov, context->cnc,
										  G_VALUE_TYPE (expr->value));
				if (!dh)
				  goto err;
				else
				  g_object_ref (dh); // We need this, since dh is not a full transfer
			}
			else
				dh = gda_data_handler_get_default (G_VALUE_TYPE (expr->value));

			if (dh) {
				str = gda_data_handler_get_sql_from_value (dh, expr->value);
				g_object_unref (dh);
			}
			else
				str = gda_value_stringify (expr->value);
			if (!str) goto err;
		}
	}
	else if (expr->func) {
		str = context->render_function (GDA_SQL_ANY_PART (expr->func), context, error);
		if (!str) goto err;
	}
	else if (expr->cond) {
		gchar *tmp;
		tmp = context->render_operation (GDA_SQL_ANY_PART (expr->cond), context, error);
		if (!tmp) goto err;
		str = NULL;
		if (GDA_SQL_ANY_PART (expr)->parent) {
			if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_SELECT) {
				GdaSqlStatementSelect *selst;
				selst = (GdaSqlStatementSelect*) (GDA_SQL_ANY_PART (expr)->parent);
				if ((expr == selst->where_cond) ||
				    (expr == selst->having_cond))
					str = tmp;
			}
			else if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_DELETE) {
				GdaSqlStatementDelete *delst;
				delst = (GdaSqlStatementDelete*) (GDA_SQL_ANY_PART (expr)->parent);
				if (expr == delst->cond)
					str = tmp;
			}
			else if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_UPDATE) {
				GdaSqlStatementUpdate *updst;
				updst = (GdaSqlStatementUpdate*) (GDA_SQL_ANY_PART (expr)->parent);
				if (expr == updst->cond)
					str = tmp;
			}
		}

		if (!str) {
			str = g_strconcat ("(", tmp, ")", NULL);
			g_free (tmp);
		}
	}
	else if (expr->select) {
		gchar *str1;
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			str1 = context->render_select (GDA_SQL_ANY_PART (expr->select), context, error);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str1 = context->render_compound (GDA_SQL_ANY_PART (expr->select), context, error);
		else
			g_assert_not_reached ();
		if (!str1) goto err;

		if (! GDA_SQL_ANY_PART (expr)->parent ||
		    (GDA_SQL_ANY_PART (expr)->parent->type != GDA_SQL_ANY_SQL_FUNCTION)) {
			str = g_strconcat ("(", str1, ")", NULL);
			g_free (str1);
		}
		else
			str = str1;
	}
	else if (expr->case_s) {
		str = context->render_case (GDA_SQL_ANY_PART (expr->case_s), context, error);
		if (!str) goto err;
	}
	else {
		if (is_null)
			*is_null = TRUE;
		str = g_strdup ("NULL");
	}

	if (!str) goto err;

	if (expr->cast_as)
		g_string_append_printf (string, "CAST (%s AS %s)", str, expr->cast_as);
	else
		g_string_append (string, str);
	g_free (str);

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_field (GdaSqlField *field, GdaSqlRenderingContext *context, GError **error)
{
	g_return_val_if_fail (field, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (field)->type == GDA_SQL_ANY_SQL_FIELD, NULL);

	/* can't have: field->field_name not a valid SQL identifier */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (field), error)) return NULL;

	GdaConnectionOptions cncoptions = 0;
	if (context->cnc)
		g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
	return gda_sql_identifier_quote (field->field_name, context->cnc, context->provider,
					 FALSE,
					 cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
}

static gchar *
default_render_table (GdaSqlTable *table, GdaSqlRenderingContext *context, GError **error)
{
	g_return_val_if_fail (table, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (table)->type == GDA_SQL_ANY_SQL_TABLE, NULL);

	/* can't have: table->table_name not a valid SQL identifier */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (table), error)) return NULL;

	gchar **ids_array;
	ids_array = gda_sql_identifier_split (table->table_name);
	if (!ids_array) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Malformed table name"));
		return NULL;
	}
	
	gint i;
	GString *string;
	GdaConnectionOptions cncoptions = 0;
	if (context->cnc)
		g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
	string = g_string_new ("");
	for (i = 0; ids_array [i]; i++) {
		gchar *tmp;
		tmp = gda_sql_identifier_quote (ids_array [i], context->cnc, context->provider,
						FALSE,
						cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
		g_free (ids_array [i]);
		ids_array [i] = tmp;
		if (i != 0)
			g_string_append_c (string, '.');
		g_string_append (string, ids_array [i]);
	}
	g_strfreev (ids_array);
	return g_string_free (string, FALSE);
}

static gchar *
default_render_function (GdaSqlFunction *func, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (func, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (func)->type == GDA_SQL_ANY_SQL_FUNCTION, NULL);

	/* can't have: func->function_name == NULL */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (func), error)) return NULL;

	string = g_string_new (func->function_name);
	g_string_append (string, " (");
	for (list = func->args_list; list; list = list->next) {
		if (list != func->args_list)
			g_string_append (string, ", ");
		str = context->render_expr (list->data, context, NULL, NULL, error);
		if (!str) goto err;
		if (((GdaSqlExpr*) list->data)->select)
			g_string_append_c (string, '(');
		g_string_append (string, str);
		if (((GdaSqlExpr*) list->data)->select)
			g_string_append_c (string, ')');
		g_free (str);
	}
	g_string_append_c (string, ')');

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_operation (GdaSqlOperation *op, GdaSqlRenderingContext *context, GError **error)
{
	gchar *str;
	GSList *list;
	GSList *sql_list; /* list of SqlOperand */
	GString *string;
	gchar *multi_op = NULL;

	typedef struct {
		gchar    *sql;
		gboolean  is_null;
		gboolean  is_default;
		gboolean  is_composed;
	} SqlOperand;
#define SQL_OPERAND(x) ((SqlOperand*)(x))

	g_return_val_if_fail (op, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (op)->type == GDA_SQL_ANY_SQL_OPERATION, NULL);

	/* can't have: 
	 *  - op->operands == NULL 
	 *  - incorrect number of operands
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (op), error)) return NULL;

	/* render operands */
	for (list = op->operands, sql_list = NULL; list; list = list->next) {
		SqlOperand *sqlop = g_new0 (SqlOperand, 1);
		GdaSqlExpr *expr = (GdaSqlExpr*) list->data;
		str = context->render_expr (expr, context, &(sqlop->is_default), &(sqlop->is_null), error);
		if (!str) {
			g_free (sqlop);
			goto out;
		}
		sqlop->sql = str;
		if (expr->case_s || expr->select)
			sqlop->is_composed = TRUE;
		sql_list = g_slist_prepend (sql_list, sqlop);
	}
	sql_list = g_slist_reverse (sql_list);

	str = NULL;
	switch (op->operator_type) {
	case GDA_SQL_OPERATOR_TYPE_EQ:
		if (SQL_OPERAND (sql_list->next->data)->is_null) 
			str = g_strdup_printf ("%s IS NULL", SQL_OPERAND (sql_list->data)->sql);
		else
			str = g_strdup_printf ("%s = %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_IS:
		str = g_strdup_printf ("%s IS %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_LIKE:
		str = g_strdup_printf ("%s LIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOTLIKE:
		str = g_strdup_printf ("%s NOT LIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_ILIKE:
		str = g_strdup_printf ("%s ILIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOTILIKE:
		str = g_strdup_printf ("%s NOT ILIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_GT:
		str = g_strdup_printf ("%s > %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_LT:
		str = g_strdup_printf ("%s < %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_GEQ:
		str = g_strdup_printf ("%s >= %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_LEQ:
		str = g_strdup_printf ("%s <= %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_DIFF:
		str = g_strdup_printf ("%s != %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_REGEXP:
		str = g_strdup_printf ("%s ~ %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_REGEXP_CI:
		str = g_strdup_printf ("%s ~* %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP:
		str = g_strdup_printf ("%s !~ %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI:
		str = g_strdup_printf ("%s !~* %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_SIMILAR:
		str = g_strdup_printf ("%s SIMILAR TO %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_REM:
		str = g_strdup_printf ("%s %% %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_DIV:
		str = g_strdup_printf ("%s / %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BITAND:
		str = g_strdup_printf ("%s & %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BITOR:
		str = g_strdup_printf ("%s | %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BETWEEN:
		str = g_strdup_printf ("%s BETWEEN %s AND %s", SQL_OPERAND (sql_list->data)->sql, 
				       SQL_OPERAND (sql_list->next->data)->sql,
				       SQL_OPERAND (sql_list->next->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_ISNULL:
		str = g_strdup_printf ("%s IS NULL", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_ISNOTNULL:
		str = g_strdup_printf ("%s IS NOT NULL", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BITNOT:
		str = g_strdup_printf ("~ %s", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOT:
		str = g_strdup_printf ("NOT %s", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_IN:
	case GDA_SQL_OPERATOR_TYPE_NOTIN: {
		gboolean add_p = TRUE;
		if (sql_list->next && !(sql_list->next->next) &&
		    *(SQL_OPERAND (sql_list->next->data)->sql)=='(')
			add_p = FALSE;

		string = g_string_new (SQL_OPERAND (sql_list->data)->sql);
		if (op->operator_type == GDA_SQL_OPERATOR_TYPE_IN)
			g_string_append (string, " IN ");
		else
			g_string_append (string, " NOT IN ");
		if (add_p)
			g_string_append_c (string, '(');
		for (list = sql_list->next; list; list = list->next) {
			if (list != sql_list->next)
				g_string_append (string, ", ");
			g_string_append (string, SQL_OPERAND (list->data)->sql);
		}
		if (add_p)
			g_string_append_c (string, ')');
		str = string->str;
		g_string_free (string, FALSE);
		break;
	}
	case GDA_SQL_OPERATOR_TYPE_CONCAT:
		multi_op = "||";
		break;
	case GDA_SQL_OPERATOR_TYPE_PLUS:
		multi_op = "+";
		break;
	case GDA_SQL_OPERATOR_TYPE_MINUS:
		multi_op = "-";
		break;
	case GDA_SQL_OPERATOR_TYPE_STAR:
		multi_op = "*";
		break;
	case GDA_SQL_OPERATOR_TYPE_AND:
		multi_op = "AND";
		break;
	case GDA_SQL_OPERATOR_TYPE_OR:
		multi_op = "OR";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (multi_op) {
		if (!sql_list->next) {
			/* 1 operand only */
			string = g_string_new ("");
			g_string_append_printf (string, "%s %s", multi_op, SQL_OPERAND (sql_list->data)->sql);
		}
		else {
			/* 2 or more operands */
			if (SQL_OPERAND (sql_list->data)->is_composed) {
				string = g_string_new ("(");
				g_string_append (string, SQL_OPERAND (sql_list->data)->sql);
				g_string_append_c (string, ')');
			}
			else
				string = g_string_new (SQL_OPERAND (sql_list->data)->sql);
			for (list = sql_list->next; list; list = list->next) {
				g_string_append_printf (string, " %s ", multi_op);
				if (SQL_OPERAND (list->data)->is_composed) {
					g_string_append_c (string, '(');
					g_string_append (string, SQL_OPERAND (list->data)->sql);
					g_string_append_c (string, ')');
				}
				else
					g_string_append (string, SQL_OPERAND (list->data)->sql);
			}
		}
		str = string->str;
		g_string_free (string, FALSE);
	}

 out:
	for (list = sql_list; list; list = list->next) {
		g_free (((SqlOperand*)list->data)->sql);
		g_free (list->data);
	}
	g_slist_free (sql_list);

	return str;
}

static gchar *
default_render_case (GdaSqlCase *case_s, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *wlist, *tlist;

	g_return_val_if_fail (case_s, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (case_s)->type == GDA_SQL_ANY_SQL_CASE, NULL);

	/* can't have:
	 *  - case_s->when_expr_list == NULL
	 *  - g_slist_length (sc->when_expr_list) != g_slist_length (sc->then_expr_list)
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (case_s), error)) return NULL;

	string = g_string_new ("CASE");
	if (case_s->base_expr) {
		g_string_append_c (string, ' ');
		str = context->render_expr (case_s->base_expr, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	for (wlist = case_s->when_expr_list, tlist = case_s->then_expr_list;
	     wlist && tlist;
	     wlist = wlist->next, tlist = tlist->next) {
		g_string_append (string, " WHEN ");
		str = context->render_expr ((GdaSqlExpr*) wlist->data, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
		g_string_append (string, " THEN ");
		str = context->render_expr ((GdaSqlExpr*) tlist->data, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	if (case_s->else_expr) {
		g_string_append (string, " ELSE ");
		str = context->render_expr (case_s->else_expr, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	g_string_append (string, " END");
	
	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gboolean
alias_is_quoted (const gchar *alias)
{
	g_assert (alias);
	if ((*alias == '\'') || (*alias == '"'))
		return TRUE;
	else
		return FALSE;
}

static gchar *
default_render_select_field (GdaSqlSelectField *field, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;

	g_return_val_if_fail (field, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (field)->type == GDA_SQL_ANY_SQL_SELECT_FIELD, NULL);

	/* can't have: field->expr == NULL */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (field), error)) return NULL;

	string = g_string_new ("");
	str = context->render_expr (field->expr, context, NULL, NULL, error);
	if (!str) goto err;
	g_string_append (string, str);
	g_free (str);

	if (field->as) {
		if (! alias_is_quoted (field->as)) {
			GdaConnectionOptions cncoptions = 0;
			gchar *tmp;
			if (context->cnc)
				g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
			tmp = gda_sql_identifier_quote (field->as, context->cnc,
							context->provider, FALSE,
							cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
			g_string_append_printf (string, " AS %s", tmp);
			g_free (tmp);
		}
		else
			g_string_append_printf (string, " AS %s", field->as);
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_select_target (GdaSqlSelectTarget *target, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;

	g_return_val_if_fail (target, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (target)->type == GDA_SQL_ANY_SQL_SELECT_TARGET, NULL);

	/* can't have: target->expr == NULL */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (target), error)) return NULL;

	if (! target->expr->value || (G_VALUE_TYPE (target->expr->value) != G_TYPE_STRING)) {
		str = context->render_expr (target->expr, context, NULL, NULL, error);
		if (!str)
			return NULL;
		string = g_string_new (str);
		g_free (str);
	}
	else {
		gboolean tmp;
		tmp = target->expr->value_is_ident;
		target->expr->value_is_ident = TRUE;
		str = context->render_expr (target->expr, context, NULL, NULL, error);
		target->expr->value_is_ident = tmp;
		string = g_string_new (str);
		g_free (str);
	}

	if (target->as) {
		if (! alias_is_quoted (target->as)) {
			GdaConnectionOptions cncoptions = 0;
			gchar *tmp;
			if (context->cnc)
				g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
			tmp = gda_sql_identifier_quote (target->as, context->cnc,
							context->provider, FALSE,
							cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
			g_string_append_printf (string, " AS %s", tmp);
			g_free (tmp);
		}
		else
			g_string_append_printf (string, " AS %s", target->as);
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

static gchar *
default_render_select_join (GdaSqlSelectJoin *join, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	gboolean pretty = context->flags & GDA_STATEMENT_SQL_PRETTY;

	g_return_val_if_fail (join, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (join)->type == GDA_SQL_ANY_SQL_SELECT_JOIN, NULL);

	/* can't have: 
	 *  - join->expr && join->use 
	 *  - (join->type == GDA_SQL_SELECT_JOIN_CROSS) && (join->expr || join->use)
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (join), error)) return NULL;

	switch (join->type) {
	case GDA_SQL_SELECT_JOIN_CROSS:
		if (pretty)
			string = g_string_new (",\n\t");
		else
			string = g_string_new (",");
		break;
        case GDA_SQL_SELECT_JOIN_NATURAL:
		if (pretty)
			string = g_string_new ("\n\tNATURAL JOIN");
		else
			string = g_string_new ("NATURAL JOIN");
		break;
        case GDA_SQL_SELECT_JOIN_INNER:
		if (pretty)
			string = g_string_new ("\n\tINNER JOIN");
		else
			string = g_string_new ("INNER JOIN");
		break;
        case GDA_SQL_SELECT_JOIN_LEFT:
		if (pretty)
			string = g_string_new ("\n\tLEFT JOIN");
		else
			string = g_string_new ("LEFT JOIN");
		break;
        case GDA_SQL_SELECT_JOIN_RIGHT:
		if (pretty)
			string = g_string_new ("\n\tRIGHT JOIN");
		else
			string = g_string_new ("RIGHT JOIN");
		break;
        case GDA_SQL_SELECT_JOIN_FULL:
		if (pretty)
			string = g_string_new ("\n\tFULL JOIN");
		else
			string = g_string_new ("FULL JOIN");
		break;
	default:
		g_assert_not_reached ();
	}

	/* find joinned target */
	GdaSqlSelectFrom *from = (GdaSqlSelectFrom *) GDA_SQL_ANY_PART (join)->parent;
	if (!from || (GDA_SQL_ANY_PART (from)->type != GDA_SQL_ANY_SQL_SELECT_FROM)) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Join is not in a FROM statement"));
		goto err;
	}
	GdaSqlSelectTarget *target;
	target = g_slist_nth_data (from->targets, join->position);
	if (!target || (GDA_SQL_ANY_PART (target)->type != GDA_SQL_ANY_SQL_SELECT_TARGET)) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Could not find target the join is for"));
		goto err;
	}
	str = context->render_select_target (GDA_SQL_ANY_PART (target), context, error);
	if (!str) goto err;

	g_string_append_c (string, ' ');
	g_string_append (string, str);
	g_free (str);

	if (join->expr) {
		g_string_append (string, " ON (");
		str = context->render_expr (join->expr, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
		g_string_append_c (string, ')');
	}
	else if (join->use) {
		GSList *list;
		g_string_append (string, " USING (");
		for (list = join->use; list; list = list->next) {
			if (list != join->use)
				g_string_append (string, ", ");
			str = context->render_field (GDA_SQL_ANY_PART (list->data), context, error);
			if (!str) goto err;
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ')');
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static GdaSqlSelectJoin *
find_join_for_pos (GSList *joins_list, gint pos)
{
	GSList *list;
	for (list = joins_list; list; list = list->next) {
		if (((GdaSqlSelectJoin*) list->data)->position == pos)
			return (GdaSqlSelectJoin*) list->data;
	}
	return NULL;
}

static gchar *
default_render_select_from (GdaSqlSelectFrom *from, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *tlist;
	gint i;
	gboolean pretty = context->flags & GDA_STATEMENT_SQL_PRETTY;

	g_return_val_if_fail (from, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (from)->type == GDA_SQL_ANY_SQL_SELECT_FROM, NULL);

	/* can't have: from->targets == NULL */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (from), error)) return NULL;

	string = g_string_new ("FROM ");
	for (tlist = from->targets, i = 0; tlist; tlist = tlist->next, i++) {
		GdaSqlSelectJoin *join = NULL;
		if (tlist != from->targets) 
			join = find_join_for_pos (from->joins, i);
		
		if (join) {
			str = context->render_select_join (GDA_SQL_ANY_PART (join), context, error);
			if (!str) goto err;
			if (!pretty)
				g_string_append_c (string, ' ');
			g_string_append (string, str);
			g_free (str);
			if (!pretty)
				g_string_append_c (string, ' ');
		}
		else {
			if (tlist != from->targets) {
				if (pretty)
					g_string_append (string, ",\n\t");
				else
					g_string_append (string, ", ");
			}
			str = context->render_select_target (GDA_SQL_ANY_PART (tlist->data), context, error);
			if (!str) goto err;
			g_string_append (string, str);
			g_free (str);
		}
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

static gchar *
default_render_select_order (GdaSqlSelectOrder *order, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;

	g_return_val_if_fail (order, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (order)->type == GDA_SQL_ANY_SQL_SELECT_ORDER, NULL);

	/* can't have: order->expr == NULL */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (order), error)) return NULL;

	string = g_string_new ("");
	str = context->render_expr (order->expr, context, NULL, NULL, error);
	if (!str) goto err;
	g_string_append (string, str);
	g_free (str);

	if (order->collation_name) 
		g_string_append_printf (string, " COLLATE %s", order->collation_name);

	if (order->asc)
		g_string_append (string, " ASC");
	else
		g_string_append (string, " DESC");

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}
