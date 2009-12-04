/* gda-sql-builder.c
 *
 * Copyright (C) 2008 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include <stdarg.h>
#include <libgda/gda-sql-builder.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-easy.h>
#include <sql-parser/gda-sql-parser-enum-types.h>

/*
 * Main static functions
 */
static void gda_sql_builder_class_init (GdaSqlBuilderClass *klass);
static void gda_sql_builder_init (GdaSqlBuilder *builder);
static void gda_sql_builder_dispose (GObject *object);
static void gda_sql_builder_finalize (GObject *object);

static void gda_sql_builder_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_sql_builder_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;


typedef struct {
	GdaSqlAnyPart *part;
}  SqlPart;

struct _GdaSqlBuilderPrivate {
	GdaSqlStatement *main_stmt;
	GHashTable *parts_hash; /* key = part ID as an guint, value = SqlPart */
	guint next_assigned_id;
};


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* signals */
enum {
	DUMMY,
	LAST_SIGNAL
};

static gint gda_sql_builder_signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
	PROP_0,
	PROP_TYPE
};

/* module error */
GQuark gda_sql_builder_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_sql_builder_error");
	return quark;
}

GType
gda_sql_builder_get_type (void) {
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaSqlBuilderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sql_builder_class_init,
			NULL,
			NULL,
			sizeof (GdaSqlBuilder),
			0,
			(GInstanceInitFunc) gda_sql_builder_init
		};
		
		g_static_rec_mutex_lock (&init_mutex);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaSqlBuilder", &info, 0);
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
}


static void
gda_sql_builder_class_init (GdaSqlBuilderClass *klass) 
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	
	/* Properties */
	object_class->set_property = gda_sql_builder_set_property;
	object_class->get_property = gda_sql_builder_get_property;
	object_class->dispose = gda_sql_builder_dispose;
	object_class->finalize = gda_sql_builder_finalize;

	/**
	 * GdaSqlBuilder:stmt-type:
	 *
	 * Specifies the type of statement to be built, can only be
	 * GDA_SQL_STATEMENT_SELECT, GDA_SQL_STATEMENT_INSERT, GDA_SQL_STATEMENT_UPDATE
	 * or GDA_SQL_STATEMENT_DELETE
	 */
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_enum ("stmt-type", NULL, "Statement Type",
							    GDA_SQL_PARSER_TYPE_SQL_STATEMENT_TYPE,
							    GDA_SQL_STATEMENT_UNKNOWN,
							    (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
}

static void
any_part_free (SqlPart *part)
{
	switch (part->part->type) {
	case GDA_SQL_ANY_EXPR:
		gda_sql_expr_free ((GdaSqlExpr*) part->part);
		break;
	default:
		TO_IMPLEMENT;
	}

	g_free (part);
}

static void
gda_sql_builder_init (GdaSqlBuilder *builder) 
{
	builder->priv = g_new0 (GdaSqlBuilderPrivate, 1);
	builder->priv->main_stmt = NULL;
	builder->priv->parts_hash = g_hash_table_new_full (g_int_hash, g_int_equal,
							   g_free, (GDestroyNotify) any_part_free);
	builder->priv->next_assigned_id = G_MAXUINT;
}


/**
 * gda_sql_builder_new
 * @stmt_type: the type of statement to build
 *
 * Create a new #GdaSqlBuilder object to build #GdaStatement or #GdaSqlStatement
 * objects of type @stmt_type
 *
 * Returns: the newly created object, or %NULL if an error occurred (such as unsupported
 * statement type)
 *
 * Since: 4.2
 */
GdaSqlBuilder *
gda_sql_builder_new (GdaSqlStatementType stmt_type) 
{
	GdaSqlBuilder *builder;

	builder = (GdaSqlBuilder*) g_object_new (GDA_TYPE_SQL_BUILDER, "stmt-type", stmt_type, NULL);
	return builder;
}


static void
gda_sql_builder_dispose (GObject *object) 
{
	GdaSqlBuilder *builder;
	
	g_return_if_fail (GDA_IS_SQL_BUILDER (object));
	
	builder = GDA_SQL_BUILDER (object);
	if (builder->priv) {
		if (builder->priv->main_stmt) {
			gda_sql_statement_free (builder->priv->main_stmt);
			builder->priv->main_stmt = NULL;
		}
		if (builder->priv->parts_hash) {
			g_hash_table_destroy (builder->priv->parts_hash);
			builder->priv->parts_hash = NULL;
		}
	}
	
	/* parent class */
	parent_class->dispose (object);
}

static void
gda_sql_builder_finalize (GObject *object)
{
	GdaSqlBuilder *builder;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SQL_BUILDER (object));
	
	builder = GDA_SQL_BUILDER (object);
	if (builder->priv) {
		g_free (builder->priv);
		builder->priv = NULL;
	}
	
	/* parent class */
	parent_class->finalize (object);
}

static void
gda_sql_builder_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec) 
{
	GdaSqlBuilder *builder;
	GdaSqlStatementType stmt_type;
  
	builder = GDA_SQL_BUILDER (object);
	if (builder->priv) {
		switch (param_id) {
		case PROP_TYPE:
			stmt_type = g_value_get_enum (value);
			if ((stmt_type != GDA_SQL_STATEMENT_SELECT) &&
			    (stmt_type != GDA_SQL_STATEMENT_UPDATE) &&
			    (stmt_type != GDA_SQL_STATEMENT_INSERT) &&
			    (stmt_type != GDA_SQL_STATEMENT_DELETE) &&
			    (stmt_type != GDA_SQL_STATEMENT_COMPOUND)) {
				g_critical ("Unsupported statement type: %d", stmt_type);
				return;
			}
			builder->priv->main_stmt = gda_sql_statement_new (stmt_type);
			if (stmt_type == GDA_SQL_STATEMENT_COMPOUND)
				gda_sql_builder_compound_set_type (builder, GDA_SQL_STATEMENT_COMPOUND_UNION);
			break;
		}
	}
}

static void
gda_sql_builder_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec) 
{
	GdaSqlBuilder *builder;
	builder = GDA_SQL_BUILDER (object);
	
	if (builder->priv) {
		switch (param_id) {
		}
	}
}

static guint
add_part (GdaSqlBuilder *builder, guint id, GdaSqlAnyPart *part)
{
	SqlPart *p;
	guint *realid = g_new0 (guint, 1);
	if (id == 0)
		id = builder->priv->next_assigned_id --;
	*realid = id;
	p = g_new0 (SqlPart, 1);
	p->part = part;
	g_hash_table_insert (builder->priv->parts_hash, realid, p);
	return id;
}

static SqlPart *
get_part (GdaSqlBuilder *builder, guint id, GdaSqlAnyPartType req_type)
{
	SqlPart *p;
	guint lid = id;
	if (id == 0)
		return NULL;
	p = g_hash_table_lookup (builder->priv->parts_hash, &lid);
	if (!p) {
		g_warning (_("Unknown part ID %u"), id);
		return NULL;
	}
	if (p->part->type != req_type) {
		g_warning (_("Unknown part type"));
		return NULL;
	}
	return p;
}

static GdaSqlAnyPart *
use_part (SqlPart *p, GdaSqlAnyPart *parent)
{
	if (!p)
		return NULL;

	/* copy */
	GdaSqlAnyPart *anyp = NULL;
	switch (p->part->type) {
	case GDA_SQL_ANY_EXPR:
		anyp = (GdaSqlAnyPart*) gda_sql_expr_copy ((GdaSqlExpr*) p->part);
		break;
	default:
		TO_IMPLEMENT;
		return NULL;
	}
	if (anyp)
		anyp->parent = parent;
	return anyp;
}

/**
 * gda_sql_builder_get_statement
 * @builder: a #GdaSqlBuilder object
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaStatement statement from @builder's contents.
 *
 * Returns: a new #GdaStatement object, or %NULL if an error occurred
 *
 * Since: 4.2
 */
GdaStatement *
gda_sql_builder_get_statement (GdaSqlBuilder *builder, GError **error)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), NULL);
	if (!builder->priv->main_stmt) {
		g_set_error (error, GDA_SQL_BUILDER_ERROR, GDA_SQL_BUILDER_MISUSE_ERROR,
			     _("SqlBuilder is empty"));
		return NULL;
	}
	if (! gda_sql_statement_check_structure (builder->priv->main_stmt, error))
		return NULL;

	return (GdaStatement*) g_object_new (GDA_TYPE_STATEMENT, "structure", builder->priv->main_stmt, NULL);
}

/**
 * gda_sql_builder_get_sql_statement
 * @builder: a #GdaSqlBuilder object
 * @copy_it: set to %TRUE to be able to reuse @builder
 *
 * Creates a new #GdaSqlStatement structure from @builder's contents.
 *
 * If @copy_it is %FALSE, then the returned pointer is considered to be stolen from @builder's internal
 * representation and will make it unusable anymore (resulting in a %GDA_SQL_BUILDER_MISUSE_ERROR error or
 * some warnings if one tries to reuse it).
 * If, on the other
 * hand it is set to %TRUE, then the returned #GdaSqlStatement is a copy of the on @builder uses
 * internally, making it reusable.
 *
 * Returns: a #GdaSqlStatement pointer
 *
 * Since: 4.2
 */
GdaSqlStatement *
gda_sql_builder_get_sql_statement (GdaSqlBuilder *builder, gboolean copy_it)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), NULL);
	if (!builder->priv->main_stmt) 
		return NULL;
	if (copy_it)
		return gda_sql_statement_copy (builder->priv->main_stmt);
	else {
		GdaSqlStatement *stmt = builder->priv->main_stmt;
		builder->priv->main_stmt = NULL;
		return stmt;
	}
}

/**
 * gda_sql_builder_set_table
 * @builder: a #GdaSqlBuilder object
 * @table_name: a table name
 *
 * Valid only for: INSERT, UPDATE, DELETE statements
 *
 * Sets the name of the table on which the built statement operates.
 *
 * Since: 4.2
 */
void
gda_sql_builder_set_table (GdaSqlBuilder *builder, const gchar *table_name)
{
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	g_return_if_fail (table_name && *table_name);

	GdaSqlTable *table = NULL;

	switch (builder->priv->main_stmt->stmt_type) {
	case GDA_SQL_STATEMENT_DELETE: {
		GdaSqlStatementDelete *del = (GdaSqlStatementDelete*) builder->priv->main_stmt->contents;
		if (!del->table)
			del->table = gda_sql_table_new (GDA_SQL_ANY_PART (del));
		table = del->table;
		break;
	}
	case GDA_SQL_STATEMENT_UPDATE: {
		GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) builder->priv->main_stmt->contents;
		if (!upd->table)
			upd->table = gda_sql_table_new (GDA_SQL_ANY_PART (upd));
		table = upd->table;
		break;
	}
	case GDA_SQL_STATEMENT_INSERT: {
		GdaSqlStatementInsert *ins = (GdaSqlStatementInsert*) builder->priv->main_stmt->contents;
		if (!ins->table)
			ins->table = gda_sql_table_new (GDA_SQL_ANY_PART (ins));
		table = ins->table;
		break;
	}
	default:
		g_warning (_("Wrong statement type"));
		break;
	}

	g_assert (table);
	if (table->table_name)
		g_free (table->table_name);
	table->table_name = g_strdup (table_name);
}

/**
 * gda_sql_builder_set_where
 * @builder: a #GdaSqlBuilder object
 * @cond_id: the ID of the expression to set as WHERE condition, or 0 to unset any previous WHERE condition
 *
 * Valid only for: UPDATE, DELETE, SELECT statements
 *
 * Sets the WHERE condition of the statement
 *
 * Since: 4.2
 */
void
gda_sql_builder_set_where (GdaSqlBuilder *builder, guint cond_id)
{
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);

	SqlPart *p = NULL;
	if (cond_id > 0) {
		p = get_part (builder, cond_id, GDA_SQL_ANY_EXPR);
		if (!p)
			return;
	}

	switch (builder->priv->main_stmt->stmt_type) {
	case GDA_SQL_STATEMENT_UPDATE: {
		GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) builder->priv->main_stmt->contents;
		if (upd->cond)
			gda_sql_expr_free (upd->cond);
		upd->cond = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (upd));
		break;
	}
	case GDA_SQL_STATEMENT_DELETE:{
		GdaSqlStatementDelete *del = (GdaSqlStatementDelete*) builder->priv->main_stmt->contents;
		if (del->cond)
			gda_sql_expr_free (del->cond);
		del->cond = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (del));
		break;
	}
	case GDA_SQL_STATEMENT_SELECT:{
		GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) builder->priv->main_stmt->contents;
		if (sel->where_cond)
			gda_sql_expr_free (sel->where_cond);
		sel->where_cond = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (sel));
		break;
	}
	default:
		g_warning (_("Wrong statement type"));
		break;
	}
}

/**
 * gda_sql_builder_select_add_field
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @table_name: a table name, or %NULL
 * @alias: an alias (eg. for the "AS" clause), or %NULL
 *
 * Valid only for: SELECT statements.
 *
 * Add a selected selected item to the SELECT statement.
 *
 * Since: 4.2
 */
void
gda_sql_builder_select_add_field (GdaSqlBuilder *builder, const gchar *field_name, const gchar *table_name, const gchar *alias)
{
	gchar *tmp;
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_warning (_("Wrong statement type"));
		return;
	}
	g_return_if_fail (field_name && *field_name);

	if (table_name && *table_name)
		tmp = g_strdup_printf ("%s.%s", table_name, field_name);
	else
		tmp = (gchar*) field_name;

	if (alias && *alias)
		gda_sql_builder_add_field_id (builder,
					      gda_sql_builder_add_id (builder, 0, tmp),
					      gda_sql_builder_add_id (builder, 0, alias));
	else
		gda_sql_builder_add_field_id (builder,
					      gda_sql_builder_add_id (builder, 0, tmp),
					      0);
	if (table_name)
		g_free (tmp);
}

static GValue *
create_typed_value (GType type, va_list *ap)
{
	GValue *v = NULL;
	if (type == G_TYPE_STRING)
		g_value_set_string ((v = gda_value_new (G_TYPE_STRING)), va_arg (*ap, gchar*));
	else if (type == G_TYPE_INT)
		g_value_set_int ((v = gda_value_new (G_TYPE_INT)), va_arg (*ap, gint));
	else if (type == G_TYPE_FLOAT)
		g_value_set_float ((v = gda_value_new (G_TYPE_FLOAT)), va_arg (*ap, double));
	else
		g_warning (_("Could not convert value to type '%s'"), g_type_name (type));
	return v;
}

/**
 * gda_sql_builder_add_field
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @type: the GType of the following argument
 * @...: value to set the field to, of the type specified by @type
 *
 * Valid only for: INSERT, UPDATE statements.
 *
 * Specifies that the field represented by @field_name will be set to the value identified 
 * by @... of type @type.
 *
 * Since: 4.2
 */
void
gda_sql_builder_add_field (GdaSqlBuilder *builder, const gchar *field_name, GType type, ...)
{
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	g_return_if_fail (field_name && *field_name);

	if ((builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_UPDATE) &&
	    (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_INSERT)) {
		g_warning (_("Wrong statement type"));
		return;
	}

	guint id1, id2;
	GValue *value;
	va_list ap;

	va_start (ap, type);
	value = create_typed_value (type, &ap);
	va_end (ap);

	if (!value)
		return;
	id1 = gda_sql_builder_add_id (builder, 0, field_name);
	id2 = gda_sql_builder_add_expr_value (builder, 0, NULL, value);
	gda_value_free (value);
	gda_sql_builder_add_field_id (builder, id1, id2);
}

/**
 * gda_sql_builder_add_field_value
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @value: value to set the field to
 *
 * Valid only for: INSERT, UPDATE statements.
 *
 * Specifies that the field represented by @field_name will be set to the value identified 
 * by @value
 *
 * Since: 4.2
 */
void
gda_sql_builder_add_field_value (GdaSqlBuilder *builder, const gchar *field_name, const GValue *value)
{
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	g_return_if_fail (field_name && *field_name);
	g_return_if_fail (value);

	if ((builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_UPDATE) &&
	    (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_INSERT)) {
		g_warning (_("Wrong statement type"));
		return;
	}

	guint id1, id2;
	id1 = gda_sql_builder_add_id (builder, 0, field_name);
	id2 = gda_sql_builder_add_expr_value (builder, 0, NULL, value);
	gda_sql_builder_add_field_id (builder, id1, id2);
}

/**
 * gda_sql_builder_add_field_id
 * @builder: a #GdaSqlBuilder object
 * @field_id: the ID of the field's name or definition
 * @value_id: the ID of the value to set the field to, or %0
 *
 * Valid only for: INSERT, UPDATE, SELECT statements
 * <itemizedlist>
 * <listitem><para>For UPDATE: specifies that the field represented by @field_id will be set to the value identified 
 *    by @value_id.</para></listitem>
 * <listitem><para>For SELECT: add a selected item to the statement, and if @value_id is not %0, then use it as an
 *    alias</para></listitem>
 * <listitem><para>For INSERT: if @field_id represents an SQL identifier (obtained using gda_sql_builder_add_id()): then if
 *    @value_id is not %0 then specifies that the field represented by @field_id will be set to the
 *    value identified by @value_id, otherwise just specifies a named field to be given a value.
 *    If @field_id represents a sub SELECT (obtained using gda_sql_builder_add_sub_select()), then
 *    this method call defines the sub SELECT from which values to insert are taken.</para></listitem>
 * </itemizedlist>
 * Since: 4.2
 */
void
gda_sql_builder_add_field_id (GdaSqlBuilder *builder, guint field_id, guint value_id)
{
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);

	SqlPart *value_part, *field_part;
	GdaSqlExpr *field_expr;
	value_part = get_part (builder, value_id, GDA_SQL_ANY_EXPR);
	field_part = get_part (builder, field_id, GDA_SQL_ANY_EXPR);
	if (!field_part)
		return;
	field_expr = (GdaSqlExpr*) (field_part->part);

	/* checks */
	switch (builder->priv->main_stmt->stmt_type) {
	case GDA_SQL_STATEMENT_UPDATE:
	case GDA_SQL_STATEMENT_INSERT:
		if (!field_expr->select) {
			if (!field_expr->value || (G_VALUE_TYPE (field_expr->value) !=  G_TYPE_STRING)) {
				g_warning (_("Wrong field format"));
				return;
			}
		}
		break;
	case GDA_SQL_STATEMENT_SELECT:
		/* no specific check */
		break;
	default:
		g_warning (_("Wrong statement type"));
		return;
	}

	/* work */
	switch (builder->priv->main_stmt->stmt_type) {
	case GDA_SQL_STATEMENT_UPDATE: {
		GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) builder->priv->main_stmt->contents;
		GdaSqlField *field = gda_sql_field_new (GDA_SQL_ANY_PART (upd));
		field->field_name = g_value_dup_string (field_expr->value);
		upd->fields_list = g_slist_append (upd->fields_list, field);
		upd->expr_list = g_slist_append (upd->expr_list, use_part (value_part, GDA_SQL_ANY_PART (upd)));
		break;
	}
	case GDA_SQL_STATEMENT_INSERT:{
		GdaSqlStatementInsert *ins = (GdaSqlStatementInsert*) builder->priv->main_stmt->contents;
		
		if (field_expr->select) {
			switch (GDA_SQL_ANY_PART (field_expr->select)->type) {
			case GDA_SQL_STATEMENT_SELECT:
				ins->select = _gda_sql_statement_select_copy (field_expr->select);
				break;
			case GDA_SQL_STATEMENT_COMPOUND:
				ins->select = _gda_sql_statement_compound_copy (field_expr->select);
				break;
			default:
				g_assert_not_reached ();
			}
		}
		else {
			GdaSqlField *field = gda_sql_field_new (GDA_SQL_ANY_PART (ins));
			field->field_name = g_value_dup_string (field_expr->value);
			
			ins->fields_list = g_slist_append (ins->fields_list, field);
			if (value_part) {
				if (! ins->values_list)
					ins->values_list = g_slist_append (NULL,
									   g_slist_append (NULL,
									   use_part (value_part, GDA_SQL_ANY_PART (ins))));
				else
					ins->values_list->data = g_slist_append ((GSList*) ins->values_list->data, 
										 use_part (value_part, GDA_SQL_ANY_PART (ins)));
			}
		}
		break;
	}
	case GDA_SQL_STATEMENT_SELECT: {
		GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) builder->priv->main_stmt->contents;
		GdaSqlSelectField *field;
		field = gda_sql_select_field_new (GDA_SQL_ANY_PART (sel));
		field->expr = (GdaSqlExpr*) use_part (field_part, GDA_SQL_ANY_PART (field));
		if (value_part) {
			GdaSqlExpr *value_expr = (GdaSqlExpr*) (value_part->part);
			if (G_VALUE_TYPE (value_expr->value) ==  G_TYPE_STRING)
				field->as = g_value_dup_string (value_expr->value);
		}
		sel->expr_list = g_slist_append (sel->expr_list, field);
		break;
	}
	default:
		g_warning (_("Wrong statement type"));
		break;
	}
}

/**
 * gda_sql_builder_add_expr_value
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @dh: a #GdaDataHandler to use, or %NULL
 * @value: value to set the expression to
 *
 * Defines an expression in @builder which may be reused to build other parts of a statement.
 *
 * The new expression will contain the value passed as the @value argument. It is possible to
 * customize how the value has to be interpreted by passing a specific #GdaDataHandler object as @dh.
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_expr_value (GdaSqlBuilder *builder, guint id, GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	g_return_val_if_fail (value != NULL, 0);

	GType type = G_VALUE_TYPE(value);
	gchar *str;

	if (!dh)
		dh = gda_get_default_handler (type);
	else {
		if (! gda_data_handler_accepts_g_type (dh, type)) {
			g_warning (_("Unhandled data type '%s'"), g_type_name (type));
			return 0;
		}
	}
	if (!dh) {
		g_warning (_("Unhandled data type '%s'"), g_type_name (type));
		return 0;
	}

	str = gda_data_handler_get_sql_from_value (dh, value);

	if (str) {
		GdaSqlExpr *expr;
		expr = gda_sql_expr_new (NULL);
		expr->value = gda_value_new (G_TYPE_STRING);
		g_value_take_string (expr->value, str);
		return add_part (builder, id, (GdaSqlAnyPart *) expr);
	}
	else {
		g_warning (_("Could not convert value to type '%s'"), g_type_name (type));
		return 0;
	}
}

/**
 * gda_sql_builder_add_expr
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @dh: a #GdaDataHandler to use, or %NULL
 * @type: the GType of the following argument
 * @...: value to set the expression to, of the type specified by @type
 *
 * Defines an expression in @builder which may be reused to build other parts of a statement.
 *
 * The new expression will contain the value passed as the @v argument. It is possible to
 * customize how the value has to be interpreted by passing a specific #GdaDataHandler object as @dh.
 *
 * For example:
 * <programlisting>
 * gda_sql_builder_add_expr (b, 0, G_TYPE_INT, 15);
 * gda_sql_builder_add_expr (b, 5, G_TYPE_STRING, "joe")
 * </programlisting>
 *
 * will be rendered as SQL as:
 * <programlisting>
 * 15
 * 'joe'
 * </programlisting>
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_expr (GdaSqlBuilder *builder, guint id, GdaDataHandler *dh, GType type, ...)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	
	va_list ap;
	GValue *value;
	guint retval;

	va_start (ap, type);
	value = create_typed_value (type, &ap);
	va_end (ap);

	if (!value)
		return 0;
	retval = gda_sql_builder_add_expr_value (builder, id, dh, value);

	gda_value_free (value);

	return retval;
}

/**
 * gda_sql_builder_add_id
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @string: a string
 *
 * Defines an expression representing an identifier in @builder,
 * which may be reused to build other parts of a statement.
 *
 * The new expression will contain the @string literal.
 * For example:
 * <programlisting>
 * gda_sql_builder_add_id (b, 0, "name")
 * gda_sql_builder_add_id (b, 0, "date")
 * </programlisting>
 *
 * will be rendered as SQL as:
 * <programlisting>
 * name
 * "date"
 * </programlisting>
 *
 * because "date" is an SQL reserved keyword.
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_id (GdaSqlBuilder *builder, guint id, const gchar *string)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);
	if (string) {
		expr->value = gda_value_new (G_TYPE_STRING);
		g_value_set_string (expr->value, string);
		expr->value_is_ident = (gpointer) 0x1;
	}
	
	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_param
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @param_name: parameter's name
 * @type: parameter's type
 * @nullok: TRUE if the parameter can be set to %NULL
 *
 * Defines a parameter in @builder which may be reused to build other parts of a statement.
 *
 * The new expression will contain the @string literal.
 * For example:
 * <programlisting>
 * gda_sql_builder_add_param (b, 0, "age", G_TYPE_INT, FALSE)
 * </programlisting>
 *
 * will be rendered as SQL as:
 * <programlisting><![CDATA[
 * ##age::int
 * ]]>
 * </programlisting>
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_param (GdaSqlBuilder *builder, guint id, const gchar *param_name, GType type, gboolean nullok)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	g_return_val_if_fail (param_name && *param_name, 0);
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);
	expr->param_spec = g_new0 (GdaSqlParamSpec, 1);
	expr->param_spec->name = g_strdup (param_name);
	expr->param_spec->is_param = TRUE;
	expr->param_spec->nullok = nullok;
	expr->param_spec->g_type = type;

	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_cond
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @op: type of condition
 * @op1: the ID of the 1st argument (not 0)
 * @op2: the ID of the 2nd argument (may be %0 if @op needs only one operand)
 * @op3: the ID of the 3rd argument (may be %0 if @op needs only one or two operand)
 *
 * Builds a new expression which reprenents a condition (or operation).
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_cond (GdaSqlBuilder *builder, guint id, GdaSqlOperatorType op, guint op1, guint op2, guint op3)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);

	SqlPart *p1, *p2;
	p1 = get_part (builder, op1, GDA_SQL_ANY_EXPR);
	if (!p1)
		return 0;
	p2 = get_part (builder, op2, GDA_SQL_ANY_EXPR);
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);
	expr->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
	expr->cond->operator_type = op;
	expr->cond->operands = g_slist_append (NULL, use_part (p1, GDA_SQL_ANY_PART (expr->cond)));
	if (p2) {
		SqlPart *p3;
		expr->cond->operands = g_slist_append (expr->cond->operands,
						       use_part (p2, GDA_SQL_ANY_PART (expr->cond)));
		p3 = get_part (builder, op3, GDA_SQL_ANY_EXPR);
		if (p3)
			expr->cond->operands = g_slist_append (expr->cond->operands,
							       use_part (p3, GDA_SQL_ANY_PART (expr->cond)));	
	}

	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_cond_v
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @op: type of condition
 * @op_ids: an array of ID for the arguments (not %0)
 * @ops_ids_size: size of @ops_ids
 *
 * Builds a new expression which reprenents a condition (or operation).
 *
 * As a side case, if @ops_ids_size is 1,
 * then @op is ignored, and the returned ID represents @op_ids[0] (this avoids any problem for example
 * when @op is GDA_SQL_OPERATOR_TYPE_AND and there is in fact only one operand).
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_cond_v (GdaSqlBuilder *builder, guint id, GdaSqlOperatorType op,
			const guint *op_ids, gint op_ids_size)
{
	gint i;
	SqlPart **parts;

	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	g_return_val_if_fail (op_ids, 0);
	g_return_val_if_fail (op_ids_size > 0, 0);

	parts = g_new (SqlPart *, op_ids_size);
	for (i = 0; i < op_ids_size; i++) {
		parts [i] = get_part (builder, op_ids [i], GDA_SQL_ANY_EXPR);
		if (!parts [i]) {
			g_free (parts);
			return 0;
		}
	}

	if (op_ids_size == 1) {
		SqlPart *part = parts [0];
		g_free (parts);
		if (id)
			return add_part (builder, id, use_part (part, NULL));
		else
			return op_ids [0]; /* return the same ID as none was specified */
	}
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);
	expr->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
	expr->cond->operator_type = op;
	expr->cond->operands = NULL;
	for (i = 0; i < op_ids_size; i++)
		expr->cond->operands = g_slist_append (expr->cond->operands,
						       use_part (parts [i],
								 GDA_SQL_ANY_PART (expr->cond)));
	g_free (parts);

	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}


typedef struct {
	GdaSqlSelectTarget target; /* inheritance! */
	guint part_id; /* copied from this part ID */
} BuildTarget;

/**
 * gda_sql_builder_select_add_target_id
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @table_id: the ID of the expression holding a table reference (not %0)
 * @alias: the alias to give to the target, or %NULL
 *
 * Adds a new target to a SELECT statement
 *
 * Returns: the ID of the new target, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_select_add_target_id (GdaSqlBuilder *builder, guint id, guint table_id, const gchar *alias)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_warning (_("Wrong statement type"));
		return 0;
	}

	SqlPart *p;
	p = get_part (builder, table_id, GDA_SQL_ANY_EXPR);
	if (!p)
		return 0;
	
	BuildTarget *btarget;
	GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) builder->priv->main_stmt->contents;
	btarget = g_new0 (BuildTarget, 1);
        GDA_SQL_ANY_PART (btarget)->type = GDA_SQL_ANY_SQL_SELECT_TARGET;
        GDA_SQL_ANY_PART (btarget)->parent = GDA_SQL_ANY_PART (sel->from);
	if (id)
		btarget->part_id = id;
	else
		btarget->part_id = builder->priv->next_assigned_id --;
	
	((GdaSqlSelectTarget*) btarget)->expr = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (btarget));
	if (alias) 
		((GdaSqlSelectTarget*) btarget)->as = g_strdup (alias);
	if (g_value_get_string (((GdaSqlSelectTarget*) btarget)->expr->value))
		((GdaSqlSelectTarget*) btarget)->table_name = 
			g_value_dup_string (((GdaSqlSelectTarget*) btarget)->expr->value);

	/* add target to sel->from. NOTE: @btarget is NOT added to the "repository" or GdaSqlAnyPart parts
	* like others */
	if (!sel->from)
		sel->from = gda_sql_select_from_new (GDA_SQL_ANY_PART (sel));
	sel->from->targets = g_slist_append (sel->from->targets, btarget);

	return btarget->part_id;
}


/**
 * gda_sql_builder_select_add_target
 * @builder: a #GdaSqlBuilder object
 * @table_name: the name of the target table
 * @alias: the alias to give to the target, or %NULL
 *
 * Adds a new target to a SELECT statement
 *
 * Returns: the ID of the new target, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_select_add_target (GdaSqlBuilder *builder, const gchar *table_name, const gchar *alias)
{
	gchar *tmp;
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_warning (_("Wrong statement type"));
		return;
	}
	g_return_if_fail (table_name && *table_name);

	gda_sql_builder_select_add_target_id (builder,
					      0, 
					      gda_sql_builder_add_id (builder, 0, table_name),
					      alias);
	if (table_name)
		g_free (tmp);
}


typedef struct {
	GdaSqlSelectJoin join; /* inheritance! */
	guint part_id; /* copied from this part ID */
} BuilderJoin;

/**
 * gda_sql_builder_select_join_targets
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @left_target_id: the ID of the left target to use (not %0)
 * @right_target_id: the ID of the right target to use (not %0)
 * @join_type: the type of join
 * @join_expr: joining expression's ID, or %0
 *
 * Joins two targets in a SELECT statement
 *
 * Returns: the ID of the new join, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_select_join_targets (GdaSqlBuilder *builder, guint id,
				     guint left_target_id, guint right_target_id,
				     GdaSqlSelectJoinType join_type,
				     guint join_expr)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_warning (_("Wrong statement type"));
		return 0;
	}

	/* determine join position */
	GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) builder->priv->main_stmt->contents;
	GSList *list;
	gint left_pos = -1, right_pos = -1;
	gint pos;
	for (pos = 0, list = sel->from ? sel->from->targets : NULL;
	     list;
	     pos++, list = list->next) {
		BuildTarget *btarget = (BuildTarget*) list->data;
		if (btarget->part_id == left_target_id)
			left_pos = pos;
		else if (btarget->part_id == right_target_id)
			right_pos = pos;
		if ((left_pos != -1) && (right_pos != -1))
			break;
	}
	if ((left_pos == -1) || (right_pos == -1)) {
		g_warning (_("Unknown part ID %u"), (left_pos == -1) ? left_pos : right_pos);
		return 0;
	}
	
	if (left_pos > right_pos) {
		TO_IMPLEMENT;
	}
	
	/* create join */
	BuilderJoin *bjoin;
	GdaSqlSelectJoin *join;

	bjoin = g_new0 (BuilderJoin, 1);
	GDA_SQL_ANY_PART (bjoin)->type = GDA_SQL_ANY_SQL_SELECT_JOIN;
        GDA_SQL_ANY_PART (bjoin)->parent = GDA_SQL_ANY_PART (sel->from);
	if (id)
		bjoin->part_id = id;
	else
		bjoin->part_id = builder->priv->next_assigned_id --;
	join = (GdaSqlSelectJoin*) bjoin;
	join->type = join_type;
	join->position = right_pos;
	
	SqlPart *ep;
	ep = get_part (builder, join_expr, GDA_SQL_ANY_EXPR);
	if (ep)
		join->expr = (GdaSqlExpr*) use_part (ep, GDA_SQL_ANY_PART (join));

	sel->from->joins = g_slist_append (sel->from->joins, bjoin);

	return bjoin->part_id;
}

/**
 * gda_sql_builder_join_add_field
 * @builder: a #GdaSqlBuilder object
 * @join_id: the ID of the join to modify (not %0)
 * @field_name: the name of the field to use in the join condition (not %NULL)
 *
 * Alter a joins in a SELECT statement to make its condition on the field which name
 * is @field_name
 *
 * Returns: the ID of the new join, or 0 if there was an error
 *
 * Since: 4.2
 */
void
gda_sql_builder_join_add_field (GdaSqlBuilder *builder, guint join_id, const gchar *field_name)
{
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	g_return_if_fail (field_name);
	
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_warning (_("Wrong statement type"));
		return;
	}

	/* determine join */
	GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) builder->priv->main_stmt->contents;
	GdaSqlSelectJoin *join = NULL;
	GSList *list;
	for (list = sel->from ? sel->from->joins : NULL;
	     list;
	     list = list->next) {
		BuilderJoin *bjoin = (BuilderJoin*) list->data;
		if (bjoin->part_id == join_id) {
			join = (GdaSqlSelectJoin*) bjoin;
			break;
		}
	}
	if (!join) {
		g_warning (_("Unknown part ID %u"), join_id);
		return;
	}

	GdaSqlField *field;
	field = gda_sql_field_new (GDA_SQL_ANY_PART (join));
	field->field_name = g_strdup (field_name);
	join->use = g_slist_append (join->use, field);
}

/**
 * gda_sql_builder_select_order_by
 * @builder: a #GdaSqlBuiler
 * @expr_id: the ID of the expression to use during sorting (not %0)
 * @asc: %TRUE for an ascending sorting
 * @collation_name: name of the collation to use when sorting, or %NULL
 *
 * Adds a new ORDER BY expression to a SELECT statement.
 * 
 * Since: 4.2
 */
void
gda_sql_builder_select_order_by (GdaSqlBuilder *builder, guint expr_id,
				 gboolean asc, const gchar *collation_name)
{
	SqlPart *part;
	GdaSqlStatementSelect *sel;
	GdaSqlSelectOrder *sorder;

	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (expr_id > 0);

	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_warning (_("Wrong statement type"));
		return;
	}

	part = get_part (builder, expr_id, GDA_SQL_ANY_EXPR);
	if (!part)
		return;
	sel = (GdaSqlStatementSelect*) builder->priv->main_stmt->contents;
	
	sorder = gda_sql_select_order_new (GDA_SQL_ANY_PART (sel));
	sorder->expr = (GdaSqlExpr*) use_part (part, GDA_SQL_ANY_PART (sorder));
	sorder->asc = asc;
	if (collation_name)
		sorder->collation_name = g_strdup (collation_name);
	sel->order_by = g_slist_append (sel->order_by, sorder);
}

/**
 * gda_sql_builder_add_function
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @function_id: the ID of the functions's name
 * @...: a list, terminated with %0, of each function's argument's ID
 *
 * Builds a new expression which reprenents a function applied to some arguments
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_function (GdaSqlBuilder *builder, guint id, const gchar *func_name, ...)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);

	GdaSqlExpr *expr;
	GSList *list = NULL;
	va_list ap;
	SqlPart *part;
	guint aid;

	expr = gda_sql_expr_new (NULL);
	expr->func = gda_sql_function_new (GDA_SQL_ANY_PART (expr));
	expr->func->function_name = g_strdup (func_name);

	va_start (ap, func_name);
	for (aid = va_arg (ap, guint); aid; aid = va_arg (ap, guint)) {
		part = get_part (builder, aid, GDA_SQL_ANY_EXPR);
		if (!part) {
			expr->func->args_list = list;
			gda_sql_expr_free (expr);
			return 0;
		}
		list = g_slist_prepend (list, use_part (part, GDA_SQL_ANY_PART (expr->func)));
	}
	va_end (ap);
	expr->func->args_list = g_slist_reverse (list);

	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_function_v
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @function_id: the ID of the functions's name
 * @args: an array of IDs representing the function's arguments
 * @args_size: @args's size
 *
 * Builds a new expression which represents a function applied to some arguments
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_function_v (GdaSqlBuilder *builder, guint id, const gchar *func_name,
				const guint *args, gint args_size)
{
	gint i;
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	g_return_val_if_fail (func_name && *func_name, 0);

	GdaSqlExpr *expr;
	GSList *list = NULL;
	expr = gda_sql_expr_new (NULL);
	expr->func = gda_sql_function_new (GDA_SQL_ANY_PART (expr));
	expr->func->function_name = g_strdup (func_name);

	for (i = 0; i < args_size; i++) {
		SqlPart *part;
		part = get_part (builder, args[i], GDA_SQL_ANY_EXPR);
		if (!part) {
			expr->func->args_list = list;
			gda_sql_expr_free (expr);
			return 0;
		}
		list = g_slist_prepend (list, use_part (part, GDA_SQL_ANY_PART (expr->func)));
	}
	expr->func->args_list = g_slist_reverse (list);

	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_sub_select
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @sqlst: a pointer to a #GdaSqlStatement, which has to be a SELECT or compound SELECT
 * @steal: if %TRUE, then @sqlst will be "stolen" by @b and should not be used anymore
 *
 * Adds an expression which is a subselect.
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_sub_select (GdaSqlBuilder *builder, guint id, GdaSqlStatement *sqlst, gboolean steal)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);
	g_return_val_if_fail (sqlst, 0);
	g_return_val_if_fail ((sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) ||
			      (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND), 0);
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);
	if (steal) {
		expr->select = sqlst->contents;
		sqlst->contents = NULL;
		gda_sql_statement_free (sqlst);
	}
	else {
		switch (sqlst->stmt_type) {
		case GDA_SQL_STATEMENT_SELECT:
			expr->select = _gda_sql_statement_select_copy (sqlst->contents);
			break;
		case GDA_SQL_STATEMENT_COMPOUND:
			expr->select = _gda_sql_statement_compound_copy (sqlst->contents);
			break;
		default:
			g_assert_not_reached ();
		}
	}
	GDA_SQL_ANY_PART (expr->select)->parent = GDA_SQL_ANY_PART (expr);

	return add_part (builder, id, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_compound_set_type
 * @builder: a #GdaSqlBuilder object
 * @compound_type: a type of compound
 *
 * Changes the type of compound which @builder is making, for a COMPOUND statement
 *
 * Since: 4.2
 */
void
gda_sql_builder_compound_set_type (GdaSqlBuilder *builder, GdaSqlStatementCompoundType compound_type)
{
	GdaSqlStatementCompound *cstmt;
	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_COMPOUND) {
		g_warning (_("Wrong statement type"));
		return;
	}

	cstmt = (GdaSqlStatementCompound*) builder->priv->main_stmt->contents;
	cstmt->compound_type = compound_type;
}

/**
 * gda_sql_builder_compound_add_sub_select
 * @builder: a #GdaSqlBuilder object
 * @sqlst: a pointer to a #GdaSqlStatement, which has to be a SELECT or compound SELECT
 * @steal: if %TRUE, then @sqlst will be "stolen" by @b and should not be used anymore
 *
 * Add a sub select to a COMPOUND statement
 * 
 * Since: 4.2
 */
void
gda_sql_builder_compound_add_sub_select (GdaSqlBuilder *builder, GdaSqlStatement *sqlst, gboolean steal)
{
	GdaSqlStatementCompound *cstmt;
	GdaSqlStatement *sub;

	g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
	g_return_if_fail (builder->priv->main_stmt);
	if (builder->priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_COMPOUND) {
		g_warning (_("Wrong statement type"));
		return;
	}
	g_return_if_fail (sqlst);
	g_return_if_fail ((sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) ||
			  (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND));

	cstmt = (GdaSqlStatementCompound*) builder->priv->main_stmt->contents;
	if (steal)
		sub = sqlst;
	else
		sub = gda_sql_statement_copy (sqlst);

	cstmt->stmt_list = g_slist_append (cstmt->stmt_list, sub);
}

/**
 * gda_sql_builder_add_case
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @test_expr: the expression ID representing the test of the CASE, or %0
 * @else_expr: the expression ID representing the ELSE expression, or %0
 * @...: a list, terminated by a %0, of (WHEN expression ID, THEN expression ID) representing
 *       all the test cases
 *
 * Creates a new CASE ... WHEN ... THEN ... ELSE ... END expression.
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_case (GdaSqlBuilder *builder, guint id,
			  guint test_expr, guint else_expr, ...)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);

	SqlPart *ptest, *pelse;
	ptest = get_part (builder, test_expr, GDA_SQL_ANY_EXPR);
	pelse = get_part (builder, else_expr, GDA_SQL_ANY_EXPR);
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);

	expr->case_s = gda_sql_case_new (GDA_SQL_ANY_PART (expr));
	if (ptest)
		expr->case_s->base_expr = (GdaSqlExpr*) use_part (ptest, GDA_SQL_ANY_PART (expr->case_s));
	if (pelse)
		expr->case_s->else_expr = (GdaSqlExpr*) use_part (pelse, GDA_SQL_ANY_PART (expr->case_s));
	
	va_list ap;
	guint id1;
	va_start (ap, else_expr);
	for (id1 = va_arg (ap, guint); id1; id1 = va_arg (ap, guint)) {
		guint id2;
		SqlPart *pwhen, *pthen;
		id2 = va_arg (ap, guint);
		if (!id2)
			goto cleanups;
		pwhen = get_part (builder, id1, GDA_SQL_ANY_EXPR);
		if (!pwhen)
			goto cleanups;
		pthen = get_part (builder, id2, GDA_SQL_ANY_EXPR);
		if (!pthen)
			goto cleanups;
		expr->case_s->when_expr_list = g_slist_prepend (expr->case_s->when_expr_list,
								use_part (pwhen, GDA_SQL_ANY_PART (expr->case_s)));
		expr->case_s->then_expr_list = g_slist_prepend (expr->case_s->then_expr_list,
								use_part (pthen, GDA_SQL_ANY_PART (expr->case_s)));
	}
	va_end (ap);
	expr->case_s->when_expr_list = g_slist_reverse (expr->case_s->when_expr_list);
	expr->case_s->then_expr_list = g_slist_reverse (expr->case_s->then_expr_list);
	return add_part (builder, id, (GdaSqlAnyPart *) expr);
	
 cleanups:
	gda_sql_expr_free (expr);
	return 0;
}

/**
 * gda_sql_builder_add_case_v
 * @builder: a #GdaSqlBuilder object
 * @id: the requested ID, or 0 if to be determined by @builder
 * @test_expr: the expression ID representing the test of the CASE, or %0
 * @else_expr: the expression ID representing the ELSE expression, or %0
 * @when_array: an array containing each WHEN expression ID, having at least @args_size elements
 * @then_array: an array containing each THEN expression ID, having at least @args_size elements
 * @args_size: the size of @when_array and @then_array
 *
 * Creates a new CASE ... WHEN ... THEN ... ELSE ... END expression. The WHEN expression and the THEN
 * expression IDs are taken from the @when_array and @then_array at the same index, for each index inferior to
 * @args_size.
 *
 * Returns: the ID of the new expression, or 0 if there was an error
 *
 * Since: 4.2
 */
guint
gda_sql_builder_add_case_v (GdaSqlBuilder *builder, guint id,
			    guint test_expr, guint else_expr,
			    const guint *when_array, const guint *then_array, gint args_size)
{
	g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
	g_return_val_if_fail (builder->priv->main_stmt, 0);

	SqlPart *ptest, *pelse;
	ptest = get_part (builder, test_expr, GDA_SQL_ANY_EXPR);
	pelse = get_part (builder, else_expr, GDA_SQL_ANY_EXPR);
	
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (NULL);

	expr->case_s = gda_sql_case_new (GDA_SQL_ANY_PART (expr));
	if (ptest)
		expr->case_s->base_expr = (GdaSqlExpr*) use_part (ptest, GDA_SQL_ANY_PART (expr->case_s));
	if (pelse)
		expr->case_s->else_expr = (GdaSqlExpr*) use_part (pelse, GDA_SQL_ANY_PART (expr->case_s));
	
	gint i;
	for (i = 0; i < args_size; i++) {
		SqlPart *pwhen, *pthen;
		pwhen = get_part (builder, when_array[i], GDA_SQL_ANY_EXPR);
		if (!pwhen)
			goto cleanups;
		pthen = get_part (builder, then_array[i], GDA_SQL_ANY_EXPR);
		if (!pthen)
			goto cleanups;
		expr->case_s->when_expr_list = g_slist_prepend (expr->case_s->when_expr_list,
								use_part (pwhen, GDA_SQL_ANY_PART (expr->case_s)));
		expr->case_s->then_expr_list = g_slist_prepend (expr->case_s->then_expr_list,
								use_part (pthen, GDA_SQL_ANY_PART (expr->case_s)));
	}
	expr->case_s->when_expr_list = g_slist_reverse (expr->case_s->when_expr_list);
	expr->case_s->then_expr_list = g_slist_reverse (expr->case_s->then_expr_list);
	return add_part (builder, id, (GdaSqlAnyPart *) expr);
	
 cleanups:
	gda_sql_expr_free (expr);
	return 0;
}
