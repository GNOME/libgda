/*
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 Johannes Schmid <jhs@gnome.org>
 * Copyright (C) 2009 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 Daniel Espinosa <despinosa@src.gnome.org>
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
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
#define G_LOG_DOMAIN "GDA-sql-builder"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <stdarg.h>
#include <libgda/gda-sql-builder.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-data-handler.h>
#include <libgda/sql-parser/gda-sql-parser-enum-types.h>
#include <libgda/gda-debug-macros.h>

/*
 * Main static functions
 */
static void gda_sql_builder_class_init (GdaSqlBuilderClass *klass);
static void gda_sql_builder_init (GdaSqlBuilder *builder);
static void gda_sql_builder_finalize (GObject *object);

static void gda_sql_builder_set_property (GObject *object,
                                          guint param_id,
                                          const GValue *value,
                                          GParamSpec *pspec);

static void gda_sql_builder_get_property (GObject *object,
                                          guint param_id,
                                          GValue *value,
                                          GParamSpec *pspec);


typedef struct {
  GdaSqlAnyPart *part;
}  SqlPart;

typedef struct {
  GdaSqlStatement *main_stmt;
  GHashTable *parts_hash; /* key = part ID as an GdaSqlBuilderId, value = SqlPart */
  GdaSqlBuilderId next_assigned_id;
} GdaSqlBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaSqlBuilder,gda_sql_builder,G_TYPE_OBJECT)

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

static void
gda_sql_builder_class_init (GdaSqlBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Properties */
  object_class->set_property = gda_sql_builder_set_property;
  object_class->get_property = gda_sql_builder_get_property;
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
                                                      GDA_TYPE_SQL_STATEMENT_TYPE,
                                                      GDA_SQL_STATEMENT_UNKNOWN,
                                                      (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
}

static void
any_part_free (SqlPart *part)
{
  switch (part->part->type)
    {
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
  GdaSqlBuilderPrivate *priv= gda_sql_builder_get_instance_private (builder);
  priv->main_stmt = NULL;
  priv->parts_hash = g_hash_table_new_full (g_int_hash, g_int_equal,
                                            g_free, (GDestroyNotify) any_part_free);
  priv->next_assigned_id = G_MAXUINT;
}


/**
 * gda_sql_builder_new:
 * @stmt_type: the type of statement to build
 *
 * Create a new #GdaSqlBuilder object to build #GdaStatement or #GdaSqlStatement
 * objects of type @stmt_type
 *
 * Returns: (transfer full): the newly created object, or %NULL if an error occurred (such as unsupported
 * statement type)
 *
 * Since: 4.2
 */
GdaSqlBuilder *
gda_sql_builder_new (GdaSqlStatementType stmt_type)
{
  return g_object_new (GDA_TYPE_SQL_BUILDER, "stmt-type", stmt_type, NULL);
}

static void
gda_sql_builder_finalize (GObject *object)
{
  GdaSqlBuilder *builder = GDA_SQL_BUILDER (object);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);

  if (priv->main_stmt)
    {
      gda_sql_statement_free (priv->main_stmt);
      priv->main_stmt = NULL;
    }
  if (priv->parts_hash)
    {
      g_hash_table_destroy (priv->parts_hash);
      priv->parts_hash = NULL;
    }

  /* parent class */
  G_OBJECT_CLASS(gda_sql_builder_parent_class)->finalize(object);
}

static void
gda_sql_builder_set_property (GObject *object,
           guint param_id,
           const GValue *value,
           GParamSpec *pspec)
{
  GdaSqlBuilder *builder = GDA_SQL_BUILDER (object);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  GdaSqlStatementType stmt_type;

  switch (param_id)
    {
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
      priv->main_stmt = gda_sql_statement_new (stmt_type);
      if (stmt_type == GDA_SQL_STATEMENT_COMPOUND)
        gda_sql_builder_compound_set_type (builder, GDA_SQL_STATEMENT_COMPOUND_UNION);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gda_sql_builder_get_property (GObject *object,
                              guint param_id,
                              G_GNUC_UNUSED GValue *value,
                              GParamSpec *pspec)
{
  switch (param_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static GdaSqlBuilderId
add_part (GdaSqlBuilder *builder, GdaSqlAnyPart *part)
{
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  SqlPart *p;
  GdaSqlBuilderId *realid = g_new0 (GdaSqlBuilderId, 1);
  const GdaSqlBuilderId id = priv->next_assigned_id--;
  *realid = id;
  p = g_new0 (SqlPart, 1);
  p->part = part;
  g_hash_table_insert (priv->parts_hash, realid, p);
  return id;
}

static SqlPart *
get_part (GdaSqlBuilder *builder, GdaSqlBuilderId id, GdaSqlAnyPartType req_type)
{
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  SqlPart *p;
  GdaSqlBuilderId lid = id;
  if (id == 0)
    return NULL;
  p = g_hash_table_lookup (priv->parts_hash, &lid);
  if (!p)
    {
      g_warning (_("Unknown part ID %u"), id);
      return NULL;
    }
  if (p->part->type != req_type)
    {
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
  switch (p->part->type)
    {
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
 * gda_sql_builder_get_statement:
 * @builder: a #GdaSqlBuilder object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Creates a new #GdaStatement statement from @builder's contents.
 *
 * Returns: (transfer full): a new #GdaStatement object, or %NULL if an error occurred
 *
 * Since: 4.2
 */
GdaStatement *
gda_sql_builder_get_statement (GdaSqlBuilder *builder, GError **error)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL,NULL);

  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);

  if (!priv->main_stmt)
    {
      g_set_error (error, GDA_SQL_BUILDER_ERROR, GDA_SQL_BUILDER_MISUSE_ERROR,
                   "%s", _("SqlBuilder is empty"));
      return NULL;
    }
  if (!gda_sql_statement_check_structure (priv->main_stmt, error))
    return NULL;

  return (GdaStatement*) g_object_new (GDA_TYPE_STATEMENT, "structure", priv->main_stmt, NULL);
}

/**
 * gda_sql_builder_get_sql_statement:
 * @builder: a #GdaSqlBuilder object
 *
 * Creates a new #GdaSqlStatement structure from @builder's contents.
 *
 * The returned pointer belongs to @builder's internal representation.
 * Use gda_sql_statement_copy() if you need to keep it.
 *
 * Returns: (transfer none) (nullable): a #GdaSqlStatement pointer
 *
 * Since: 4.2
 */
GdaSqlStatement *
gda_sql_builder_get_sql_statement (GdaSqlBuilder *builder)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), NULL);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  if (!priv->main_stmt)
    return NULL;

  return priv->main_stmt;
}

/**
 * gda_sql_builder_set_table:
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
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  g_return_if_fail (table_name && *table_name);

  GdaSqlTable *table = NULL;

  switch (priv->main_stmt->stmt_type)
    {
    case GDA_SQL_STATEMENT_DELETE:
        {
          GdaSqlStatementDelete *del = (GdaSqlStatementDelete*) priv->main_stmt->contents;
          if (!del->table)
            del->table = gda_sql_table_new (GDA_SQL_ANY_PART (del));
          table = del->table;
          break;
        }
    case GDA_SQL_STATEMENT_UPDATE:
        {
          GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) priv->main_stmt->contents;
          if (!upd->table)
            upd->table = gda_sql_table_new (GDA_SQL_ANY_PART (upd));
          table = upd->table;
          break;
        }
    case GDA_SQL_STATEMENT_INSERT:
        {
          GdaSqlStatementInsert *ins = (GdaSqlStatementInsert*) priv->main_stmt->contents;
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
 * gda_sql_builder_set_where:
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
gda_sql_builder_set_where (GdaSqlBuilder *builder, GdaSqlBuilderId cond_id)
{
  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);

  SqlPart *p = NULL;
  if (cond_id > 0)
    {
      p = get_part (builder, cond_id, GDA_SQL_ANY_EXPR);
      if (!p)
        return;
    }

  switch (priv->main_stmt->stmt_type)
    {
    case GDA_SQL_STATEMENT_UPDATE:
        {
          GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) priv->main_stmt->contents;
          if (upd->cond)
            gda_sql_expr_free (upd->cond);
          upd->cond = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (upd));
          break;
        }
    case GDA_SQL_STATEMENT_DELETE:
        {
          GdaSqlStatementDelete *del = (GdaSqlStatementDelete*) priv->main_stmt->contents;
          if (del->cond)
            gda_sql_expr_free (del->cond);
          del->cond = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (del));
          break;
        }
    case GDA_SQL_STATEMENT_SELECT:
        {
          GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
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
 * gda_sql_builder_select_add_field:
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @table_name: (nullable): a table name, or %NULL
 * @alias: (nullable): an alias (eg. for the "AS" clause), or %NULL
 *
 * Valid only for: SELECT statements.
 *
 * Add a selected selected item to the SELECT statement.
 *
 * For non-SELECT statements, see gda_sql_builder_add_field_id().
 *
 * Returns: the ID of the added field, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_select_add_field (GdaSqlBuilder *builder, const gchar *field_name, const gchar *table_name, const gchar *alias)
{
  gchar *tmp;
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT)
    {
      g_warning (_("Wrong statement type"));
      return 0;
    }
  g_return_val_if_fail (field_name && *field_name, 0);

  gboolean tmp_is_allocated = FALSE;
  if (table_name && *table_name)
    {
      tmp = g_strdup_printf ("%s.%s", table_name, field_name);
      tmp_is_allocated = TRUE;
    }
  else
    tmp = (gchar*) field_name;

  const GdaSqlBuilderId field_id = gda_sql_builder_add_id (builder, tmp);
  if (alias && *alias)
    gda_sql_builder_add_field_value_id (builder,
                                        field_id,
                                        gda_sql_builder_add_id (builder, alias));
  else
    gda_sql_builder_add_field_value_id (builder,
                                        field_id,
                                        0);

  if (tmp_is_allocated)
    g_free (tmp);

  return field_id;
}

static GValue *
create_typed_value (GType type, va_list *ap)
{
  GValue *v = NULL;
  if (type == G_TYPE_STRING)
    g_value_set_string ((v = gda_value_new (G_TYPE_STRING)), va_arg (*ap, gchar*));
  else if (type == G_TYPE_BOOLEAN)
    g_value_set_boolean ((v = gda_value_new (G_TYPE_BOOLEAN)), va_arg (*ap, gboolean));
  else if (type == G_TYPE_INT64)
    g_value_set_int64 ((v = gda_value_new (G_TYPE_INT64)), va_arg (*ap, gint64));
  else if (type == G_TYPE_UINT64)
    g_value_set_uint64 ((v = gda_value_new (G_TYPE_UINT64)), va_arg (*ap, guint64));
  else if (type == G_TYPE_INT)
    g_value_set_int ((v = gda_value_new (G_TYPE_INT)), va_arg (*ap, gint));
  else if (type == G_TYPE_UINT)
    g_value_set_uint ((v = gda_value_new (G_TYPE_UINT)), va_arg (*ap, guint));
  else if (type == GDA_TYPE_SHORT)
    gda_value_set_short ((v = gda_value_new (GDA_TYPE_SHORT)), va_arg (*ap, gint));
  else if (type == GDA_TYPE_USHORT)
    gda_value_set_ushort ((v = gda_value_new (GDA_TYPE_USHORT)), va_arg (*ap, guint));
  else if (type == G_TYPE_CHAR)
    g_value_set_schar ((v = gda_value_new (G_TYPE_CHAR)), va_arg (*ap, gint));
  else if (type == G_TYPE_UCHAR)
    g_value_set_uchar ((v = gda_value_new (G_TYPE_UCHAR)), va_arg (*ap, guint));
  else if (type == G_TYPE_FLOAT)
    g_value_set_float ((v = gda_value_new (G_TYPE_FLOAT)), va_arg (*ap, gdouble));
  else if (type == G_TYPE_DOUBLE)
    g_value_set_double ((v = gda_value_new (G_TYPE_DOUBLE)), va_arg (*ap, gdouble));
  else if (type == GDA_TYPE_NUMERIC) {
    GdaNumeric *numeric;
    numeric = va_arg (*ap, GdaNumeric *);
    gda_value_set_numeric ((v = gda_value_new (GDA_TYPE_NUMERIC)), numeric);
  }
  else if (type == G_TYPE_DATE) {
    GDate *gdate;
    gdate = va_arg (*ap, GDate *);
    g_value_set_boxed ((v = gda_value_new (G_TYPE_DATE)), gdate);
  }
  else if (type == GDA_TYPE_TIME) {
    GdaTime *timegda;
    timegda = va_arg (*ap, GdaTime *);
    gda_value_set_time ((v = gda_value_new (GDA_TYPE_TIME)), timegda);
  }
  else if (type == G_TYPE_DATE_TIME) {
    GDateTime *timestamp;
    timestamp = va_arg (*ap, GDateTime *);
    g_value_set_boxed ((v = gda_value_new (G_TYPE_DATE_TIME)), timestamp);
  }
  else if (type == GDA_TYPE_NULL)
    v = gda_value_new_null ();
  else if (type == G_TYPE_GTYPE)
    g_value_set_gtype ((v = gda_value_new (G_TYPE_GTYPE)), va_arg (*ap, GType));
  else if (type == G_TYPE_ULONG)
    g_value_set_ulong ((v = gda_value_new (G_TYPE_ULONG)), va_arg (*ap, gulong));
  else if (type == G_TYPE_LONG)
    g_value_set_long ((v = gda_value_new (G_TYPE_LONG)), va_arg (*ap, glong));
  else if (type == GDA_TYPE_BINARY) {
    GdaBinary *bin;
    bin = va_arg (*ap, GdaBinary *);
    gda_value_set_binary ((v = gda_value_new (GDA_TYPE_BINARY)), bin);
  }
  else if (type == GDA_TYPE_BLOB) {
    GdaBlob *blob;
    blob = va_arg (*ap, GdaBlob *);
    gda_value_set_blob ((v = gda_value_new (GDA_TYPE_BLOB)), blob);
  }
  else
    g_warning (_("Could not convert value to type '%s', value not defined"), g_type_name (type));
  return v;
}

/**
 * gda_sql_builder_add_field_value:
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @type: the GType of the following argument
 * @...: value to set the field to, of the type specified by @type
 *
 * Valid only for: INSERT, UPDATE statements.
 *
 * Specifies that the field represented by @field_name will be set to the value identified
 * by @... of type @type. See gda_sql_builder_add_expr() for more information.
 *
 * This is a C convenience function. See also gda_sql_builder_add_field_value_as_gvalue().
 *
 * Since: 4.2
 */
void
gda_sql_builder_add_field_value (GdaSqlBuilder *builder, const gchar *field_name, GType type, ...)
{
  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  g_return_if_fail (field_name && *field_name);

  if ((priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_UPDATE) &&
      (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_INSERT)) {
    g_warning (_("Wrong statement type"));
    return;
  }

  GdaSqlBuilderId id1, id2;
  GValue *value;
  va_list ap;

  va_start (ap, type);
  value = create_typed_value (type, &ap);
  va_end (ap);

  if (!value)
    return;
  id1 = gda_sql_builder_add_id (builder, field_name);
  id2 = gda_sql_builder_add_expr_value (builder, value);
  gda_value_free (value);
  gda_sql_builder_add_field_value_id (builder, id1, id2);
}

/**
 * gda_sql_builder_add_field_value_as_gvalue:
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @value: (nullable): value to set the field to, or %NULL or a GDA_TYPE_NULL value to represent an SQL NULL
 *
 * Valid only for: INSERT, UPDATE statements.
 *
 * Specifies that the field represented by @field_name will be set to the value identified
 * by @value
 *
 * Since: 4.2
 */
void
gda_sql_builder_add_field_value_as_gvalue (GdaSqlBuilder *builder, const gchar *field_name, const GValue *value)
{
  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  g_return_if_fail (field_name && *field_name);

  if ((priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_UPDATE) &&
      (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_INSERT)) {
    g_warning (_("Wrong statement type"));
    return;
  }

  GdaSqlBuilderId id1, id2;
  id1 = gda_sql_builder_add_id (builder, field_name);
  id2 = gda_sql_builder_add_expr_value (builder, value);
  gda_sql_builder_add_field_value_id (builder, id1, id2);
}

/**
 * gda_sql_builder_add_field_value_id:
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
 *
 * See also gda_sql_builder_add_field_value() and gda_sql_builder_add_field_value_as_gvalue().
 *
 * Since: 4.2
 */
void
gda_sql_builder_add_field_value_id (GdaSqlBuilder *builder, GdaSqlBuilderId field_id, GdaSqlBuilderId value_id)
{
  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);

  SqlPart *value_part, *field_part;
  GdaSqlExpr *field_expr;
  value_part = get_part (builder, value_id, GDA_SQL_ANY_EXPR);
  field_part = get_part (builder, field_id, GDA_SQL_ANY_EXPR);
  if (!field_part)
    return;
  field_expr = (GdaSqlExpr*) (field_part->part);

  /* checks */
  switch (priv->main_stmt->stmt_type)
    {
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
  switch (priv->main_stmt->stmt_type)
    {
    case GDA_SQL_STATEMENT_UPDATE:
        {
          GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) priv->main_stmt->contents;
          GdaSqlField *field = gda_sql_field_new (GDA_SQL_ANY_PART (upd));
          field->field_name = g_value_dup_string (field_expr->value);
          upd->fields_list = g_slist_append (upd->fields_list, field);
          upd->expr_list = g_slist_append (upd->expr_list, use_part (value_part, GDA_SQL_ANY_PART (upd)));
          break;
        }
    case GDA_SQL_STATEMENT_INSERT:
        {
          GdaSqlStatementInsert *ins = (GdaSqlStatementInsert*) priv->main_stmt->contents;

          if (field_expr->select)
            {
              switch (GDA_SQL_ANY_PART (field_expr->select)->type)
                {
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
          else
            {
              GdaSqlField *field = gda_sql_field_new (GDA_SQL_ANY_PART (ins));
              field->field_name = g_value_dup_string (field_expr->value);

              ins->fields_list = g_slist_append (ins->fields_list, field);
              if (value_part)
                {
                  if (! ins->values_list)
                    ins->values_list = g_slist_append (NULL,
                                                       g_slist_append (NULL,
                                                                       use_part (value_part,
                                                                                 GDA_SQL_ANY_PART (ins))));
                  else
                    ins->values_list->data = g_slist_append ((GSList*) ins->values_list->data,
                                                             use_part (value_part,
                                                                       GDA_SQL_ANY_PART (ins)));
                }
            }
          break;
        }
    case GDA_SQL_STATEMENT_SELECT:
      {
          GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
          GdaSqlSelectField *field;
          field = gda_sql_select_field_new (GDA_SQL_ANY_PART (sel));
          field->expr = (GdaSqlExpr*) use_part (field_part, GDA_SQL_ANY_PART (field));
          if (value_part)
            {
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
 * gda_sql_builder_add_expr_value:
 * @builder: a #GdaSqlBuilder object
 * @value: (nullable): value to set the expression to, or %NULL or a GDA_TYPE_NULL value to represent an SQL NULL
 *
 * Defines an expression in @builder which may be reused to build other parts of a statement.
 *
 * The new expression will contain the value passed as the @value argument.
 *
 * If @value's type is a string then it is possible to customize how the value has to be interpreted by passing a
 * specific #GdaDataHandler object as @dh. This feature is very rarely used and the @dh argument should generally
 * be %NULL.
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_expr_value (GdaSqlBuilder *builder, const GValue *value)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

  GdaSqlExpr *expr;
  expr = gda_sql_expr_new (NULL);
  if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL))
    {
      if (G_VALUE_TYPE (value) == G_TYPE_STRING)
        {
          GdaDataHandler *ldh;
          ldh = gda_data_handler_get_default (G_TYPE_STRING);
          expr->value = gda_value_new (G_TYPE_STRING);
          g_value_take_string (expr->value, gda_data_handler_get_sql_from_value (ldh, value));
	  g_object_unref (ldh);
        }
      else
        expr->value = gda_value_copy (value);
    }
  else
    {
      expr->value = gda_value_new (G_TYPE_STRING);
      g_value_set_string (expr->value, "NULL");
    }
  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_expr:
 * @builder: a #GdaSqlBuilder object
 * @dh: (nullable): deprecated useless argument, just pass %NULL
 * @type: the GType of the following argument
 * @...: value to set the expression to, of the type specified by @type
 *
 * Defines an expression in @builder which may be reused to build other parts of a statement.
 *
 * The new expression will contain the value passed as the @... argument.
 *
 * If @type is G_TYPE_STRING then it is possible to customize how the value has to be interpreted by passing a
 * specific #GdaDataHandler object as @dh. This feature is very rarely used and the @dh argument should generally
 * be %NULL.
 *
 * Note that for composite types such as #GdaNumeric, #Gdate, #GdaTime, ... pointer to these
 * structures are expected, they should no be passed by value. For example:
 * <programlisting><![CDATA[GDate *date = g_date_new_dmy (27, G_DATE_MAY, 1972);
id = gda_sql_builder_add_expr (b, NULL, G_TYPE_DATE, date);
g_date_free (date);

id = gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "my string");
id = gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 25);
]]></programlisting>
 *
 * will correspond in SQL to:
 * <programlisting>
 * '05-27-1972'
 * 'my string'
 * 25
 * </programlisting>
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_expr (GdaSqlBuilder *builder, G_GNUC_UNUSED GdaDataHandler *dh, GType type, ...)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

  va_list ap;
  GValue *value;
  GdaSqlBuilderId retval;

  va_start (ap, type);
  value = create_typed_value (type, &ap);
  va_end (ap);

  if (!value)
    return 0;
  retval = gda_sql_builder_add_expr_value (builder, value);

  gda_value_free (value);

  return retval;
}

/**
 * gda_sql_builder_add_id:
 * @builder: a #GdaSqlBuilder object
 * @str: a string
 *
 * Defines an expression representing an identifier in @builder,
 * which may be reused to build other parts of a statement,
 * for instance as a parameter to gda_sql_builder_add_cond() or
 * gda_sql_builder_add_field_value_id().
 *
 * The new expression will contain the @str literal.
 * For example:
 * <programlisting>
 * gda_sql_builder_add_id (b, "name")
 * gda_sql_builder_add_id (b, "date")
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
 * For fields, see gda_sql_builder_add_field_id().
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_id (GdaSqlBuilder *builder, const gchar *str)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

  GdaSqlExpr *expr;
  expr = gda_sql_expr_new (NULL);
  if (str)
    {
      expr->value = gda_value_new (G_TYPE_STRING);
      g_value_set_string (expr->value, str);
      expr->value_is_ident = TRUE;
    }

  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_field_id:
 * @builder: a #GdaSqlBuilder object
 * @field_name: a field name
 * @table_name: (nullable): a table name, or %NULL
 *
 * Defines an expression representing a field in @builder,
 * which may be reused to build other parts of a statement,
 * for instance as a parameter to gda_sql_builder_add_cond() or
 * gda_sql_builder_add_field_value_id().
 *
 * Calling this with a %NULL @table_name is equivalent to calling gda_sql_builder_add_id().
 *
 * For SELECT queries, see gda_sql_builder_select_add_field().
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_field_id (GdaSqlBuilder *builder, const gchar *field_name, const gchar *table_name)
{
  gchar* tmp = 0;
  gboolean tmp_is_allocated = FALSE;
  if (table_name && *table_name)
    {
      tmp = g_strdup_printf ("%s.%s", table_name, field_name);
      tmp_is_allocated = TRUE;
    }
  else
    tmp = (gchar*) field_name;

  guint field_id = gda_sql_builder_add_id (builder, tmp);

  if (tmp_is_allocated)
    g_free (tmp);

  return field_id;
}

/**
 * gda_sql_builder_add_param:
 * @builder: a #GdaSqlBuilder object
 * @param_name: parameter's name
 * @type: parameter's type
 * @nullok: TRUE if the parameter can be set to %NULL
 *
 * Defines a parameter in @builder which may be reused to build other parts of a statement.
 *
 * The new expression will contain the @string literal.
 * For example:
 * <programlisting>
 * gda_sql_builder_add_param (b, "age", G_TYPE_INT, FALSE)
 * </programlisting>
 *
 * will be rendered as SQL as:
 * <programlisting><![CDATA[
 * ##age::int
 * ]]>
 * </programlisting>
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_param (GdaSqlBuilder *builder, const gchar *param_name, GType type, gboolean nullok)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (param_name && *param_name, 0);

  GdaSqlExpr *expr;
  expr = gda_sql_expr_new (NULL);
  expr->param_spec = g_new0 (GdaSqlParamSpec, 1);
  expr->param_spec->name = g_strdup (param_name);
  expr->param_spec->is_param = TRUE;
  expr->param_spec->nullok = nullok;
  expr->param_spec->g_type = type;

  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_cond:
 * @builder: a #GdaSqlBuilder object
 * @op: type of condition
 * @op1: the ID of the 1st argument (not 0)
 * @op2: the ID of the 2nd argument (may be %0 if @op needs only one operand)
 * @op3: the ID of the 3rd argument (may be %0 if @op needs only one or two operand)
 *
 * Builds a new expression which represents a condition (or operation).
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_cond (GdaSqlBuilder *builder, GdaSqlOperatorType op, GdaSqlBuilderId op1, GdaSqlBuilderId op2, GdaSqlBuilderId op3)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

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
  if (p2)
    {
      SqlPart *p3;
      expr->cond->operands = g_slist_append (expr->cond->operands,
                                             use_part (p2, GDA_SQL_ANY_PART (expr->cond)));
      p3 = get_part (builder, op3, GDA_SQL_ANY_EXPR);
      if (p3)
        expr->cond->operands = g_slist_append (expr->cond->operands,
                                               use_part (p3, GDA_SQL_ANY_PART (expr->cond)));
    }

  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_cond_v:
 * @builder: a #GdaSqlBuilder object
 * @op: type of condition
 * @op_ids: (array length=op_ids_size): an array of ID for the arguments (not %0)
 * @op_ids_size: size of @ops_ids
 *
 * Builds a new expression which represents a condition (or operation).
 *
 * As a side case, if @ops_ids_size is 1,
 * then @op is ignored, and the returned ID represents @op_ids[0] (this avoids any problem for example
 * when @op is GDA_SQL_OPERATOR_TYPE_AND and there is in fact only one operand).
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_cond_v (GdaSqlBuilder *builder, GdaSqlOperatorType op,
          const GdaSqlBuilderId *op_ids, gint op_ids_size)
{
  gint i;
  SqlPart **parts;

  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (op_ids, 0);
  g_return_val_if_fail (op_ids_size > 0, 0);

  parts = g_new (SqlPart *, op_ids_size);
  for (i = 0; i < op_ids_size; i++)
    {
      parts [i] = get_part (builder, op_ids [i], GDA_SQL_ANY_EXPR);
      if (!parts [i])
        {
          g_free (parts);
          return 0;
        }
    }

  if (op_ids_size == 1) {
    g_free (parts);
    return op_ids [0];
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

  return add_part (builder, (GdaSqlAnyPart *) expr);
}


typedef struct {
  GdaSqlSelectTarget target; /* inheritance! */
  GdaSqlBuilderId part_id; /* copied from this part ID */
} BuildTarget;

/**
 * gda_sql_builder_select_add_target_id:
 * @builder: a #GdaSqlBuilder object
 * @table_id: the ID of the expression holding a table reference (not %0)
 * @alias: (nullable): the alias to give to the target, or %NULL
 *
 * Adds a new target to a SELECT statement. If there already exists a target representing
 * the same table and the same alias (or with the same absence of alias) then the same target
 * ID is returned instead of the ID of a new target.
 *
 * Returns: the ID of the new (or existing) target, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_select_add_target_id (GdaSqlBuilder *builder, GdaSqlBuilderId table_id, const gchar *alias)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT)
    {
      g_warning (_("Wrong statement type"));
      return 0;
    }

  SqlPart *p;
  p = get_part (builder, table_id, GDA_SQL_ANY_EXPR);
  if (!p)
    return 0;

  /* check for an already existing target with the same characteristics */
  GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
  if (sel->from)
    {
      gchar *ser;
      GSList *list;

      g_assert (p->part->type == GDA_SQL_ANY_EXPR);
      ser = gda_sql_expr_serialize ((GdaSqlExpr*) p->part);
      for (list = sel->from->targets; list; list = list->next)
        {
          BuildTarget *bt = (BuildTarget*) list->data;
          GdaSqlSelectTarget *t = (GdaSqlSelectTarget*) list->data;
          gboolean idalias = FALSE;
          if (alias && t->as && !strcmp (t->as, alias))
            idalias = TRUE;
          else if (!alias && ! t->as)
            idalias = TRUE;

          gchar *tmp;
          tmp = gda_sql_expr_serialize (t->expr);
          if (! strcmp (ser, tmp))
            {
              if (idalias) {
                  g_free (tmp);
                  g_free (ser);
                  return bt->part_id;
              }
            }
          g_free (tmp);
        }
      g_free (ser);
    }

  BuildTarget *btarget;
  GdaSqlSelectTarget *target;
  btarget = g_new0 (BuildTarget, 1);
  GDA_SQL_ANY_PART (btarget)->type = GDA_SQL_ANY_SQL_SELECT_TARGET;
  GDA_SQL_ANY_PART (btarget)->parent = GDA_SQL_ANY_PART (sel->from);
  btarget->part_id = priv->next_assigned_id --;

  target = (GdaSqlSelectTarget*) btarget;
  target->expr = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (btarget));
  if (alias && *alias)
    target->as = g_strdup (alias);
  if (target->expr->value && g_value_get_string (target->expr->value))
    target->table_name = g_value_dup_string (target->expr->value);

  /* add target to sel->from. NOTE: @btarget is NOT added to the "repository" or GdaSqlAnyPart parts
  * like others */
  if (!sel->from)
    sel->from = gda_sql_select_from_new (GDA_SQL_ANY_PART (sel));
  sel->from->targets = g_slist_append (sel->from->targets, btarget);

  return btarget->part_id;
}


/**
 * gda_sql_builder_select_add_target:
 * @builder: a #GdaSqlBuilder object
 * @table_name: the name of the target table
 * @alias: (nullable): the alias to give to the target, or %NULL
 *
 * Adds a new target to a SELECT statement
 *
 * Returns: the ID of the new target, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_select_add_target (GdaSqlBuilder *builder, const gchar *table_name, const gchar *alias)
{
  GdaSqlBuilderId id;
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT)
    {
      g_warning (_("Wrong statement type"));
      return 0;
    }
  g_return_val_if_fail (table_name && *table_name, 0);

  id = gda_sql_builder_select_add_target_id (builder,
               gda_sql_builder_add_id (builder, table_name),
               alias);
  return id;
}

typedef struct {
  GdaSqlSelectJoin join; /* inheritance! */
  GdaSqlBuilderId part_id; /* copied from this part ID */
} BuilderJoin;

/**
 * gda_sql_builder_select_join_targets:
 * @builder: a #GdaSqlBuilder object
 * @left_target_id: the ID of the left target to use (not %0)
 * @right_target_id: the ID of the right target to use (not %0)
 * @join_type: the type of join
 * @join_expr: joining expression's ID, or %0
 *
 * Joins two targets in a SELECT statement, using the @join_type type of join.
 *
 * Note: if the target represented by @left_target_id is actually situated after (on the right) of
 * the target represented by @right_target_id, then the actual type of join may be switched from
 * %GDA_SQL_SELECT_JOIN_LEFT to %GDA_SQL_SELECT_JOIN_RIGHT or from %GDA_SQL_SELECT_JOIN_RIGHT to
 * %GDA_SQL_SELECT_JOIN_LEFT.
 *
 * Returns: the ID of the new join, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_select_join_targets (GdaSqlBuilder *builder,
                                     GdaSqlBuilderId left_target_id,
                                     GdaSqlBuilderId right_target_id,
                                     GdaSqlSelectJoinType join_type,
                                     GdaSqlBuilderId join_expr)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return 0;
  }

  /* determine join position */
  GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
  GSList *list;
  gint left_pos = -1, right_pos = -1;
  gint pos;
  for (pos = 0, list = sel->from ? sel->from->targets : NULL; list; pos++, list = list->next)
    {
      BuildTarget *btarget = (BuildTarget*) list->data;
      if (btarget->part_id == left_target_id)
        left_pos = pos;
      else if (btarget->part_id == right_target_id)
        right_pos = pos;
      if ((left_pos != -1) && (right_pos != -1))
        break;
    }

  if (left_pos == -1) {
    g_warning (_("Unknown left part target ID %u"), left_target_id);
    return 0;
  }

  if (right_pos == -1) {
    g_warning (_("Unknown right part target ID %u"), right_target_id);
    return 0;
  }

  if (left_pos > right_pos)
    {
      GdaSqlSelectJoinType jt;
      switch (join_type) {
        case GDA_SQL_SELECT_JOIN_LEFT:
          jt = GDA_SQL_SELECT_JOIN_RIGHT;
          break;
        case GDA_SQL_SELECT_JOIN_RIGHT:
          jt = GDA_SQL_SELECT_JOIN_LEFT;
          break;
        default:
          jt = join_type;
          break;
      }
      return gda_sql_builder_select_join_targets (builder, right_target_id,
                                                  left_target_id, jt, join_expr);
  }

  /* create join */
  BuilderJoin *bjoin;
  GdaSqlSelectJoin *join;

  bjoin = g_new0 (BuilderJoin, 1);
  GDA_SQL_ANY_PART (bjoin)->type = GDA_SQL_ANY_SQL_SELECT_JOIN;
  GDA_SQL_ANY_PART (bjoin)->parent = GDA_SQL_ANY_PART (sel->from);
  bjoin->part_id = priv->next_assigned_id --;
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
 * gda_sql_builder_join_add_field:
 * @builder: a #GdaSqlBuilder object
 * @join_id: the ID of the join to modify (not %0)
 * @field_name: the name of the field to use in the join condition (not %NULL)
 *
 * Alter a join in a SELECT statement to make its condition use equal field
 * values in the fields named @field_name in both tables, via the USING keyword.
 *
 * Since: 4.2
 */
void
gda_sql_builder_join_add_field (GdaSqlBuilder *builder, GdaSqlBuilderId join_id, const gchar *field_name)
{
  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  g_return_if_fail (field_name);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return;
  }

  /* determine join */
  GdaSqlStatementSelect *sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
  GdaSqlSelectJoin *join = NULL;
  GSList *list;
  for (list = sel->from ? sel->from->joins : NULL; list; list = list->next)
    {
      BuilderJoin *bjoin = (BuilderJoin*) list->data;
      if (bjoin->part_id == join_id)
        {
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
 * gda_sql_builder_select_order_by:
 * @builder: a #GdaSqlBuiler
 * @expr_id: the ID of the expression to use during sorting (not %0)
 * @asc: %TRUE for an ascending sorting
 * @collation_name: (nullable):  name of the collation to use when sorting, or %NULL
 *
 * Adds a new ORDER BY expression to a SELECT statement.
 *
 * Since: 4.2
 */
void
gda_sql_builder_select_order_by (GdaSqlBuilder *builder, GdaSqlBuilderId expr_id,
         gboolean asc, const gchar *collation_name)
{
  SqlPart *part;
  GdaSqlStatementSelect *sel;
  GdaSqlSelectOrder *sorder;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (expr_id > 0);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return;
  }

  part = get_part (builder, expr_id, GDA_SQL_ANY_EXPR);
  if (!part)
    return;
  sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;

  sorder = gda_sql_select_order_new (GDA_SQL_ANY_PART (sel));
  sorder->expr = (GdaSqlExpr*) use_part (part, GDA_SQL_ANY_PART (sorder));
  sorder->asc = asc;
  if (collation_name && *collation_name)
    sorder->collation_name = g_strdup (collation_name);
  sel->order_by = g_slist_append (sel->order_by, sorder);
}

/**
 * gda_sql_builder_select_set_distinct:
 * @builder: a #GdaSqlBuilder object
 * @distinct: set to %TRUE to have the DISTINCT requirement
 * @expr_id: the ID of the DISTINCT ON expression, or %0 if no expression is to be used. It is ignored
 *           if @distinct is %FALSE.
 *
 * Defines (if @distinct is %TRUE) or removes (if @distinct is %FALSE) a DISTINCT clause
 * for a SELECT statement.
 *
 * If @distinct is %TRUE, then the ID of an expression can be specified as the @expr_id argument:
 * if not %0, this is the expression used to apply the DISTINCT clause on (the resuting SQL
 * will then usually be "... DISTINCT ON &lt;expression&gt;...").
 *
 * Since: 4.2
 */
void
gda_sql_builder_select_set_distinct (GdaSqlBuilder *builder, gboolean distinct, GdaSqlBuilderId expr_id)
{
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  GdaSqlStatementSelect *sel;
  SqlPart *part = NULL;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return;
  }

  if (expr_id)
    {
      part = get_part (builder, expr_id, GDA_SQL_ANY_EXPR);
      if (!part)
        return;
    }

  sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
  if (sel->distinct_expr)
    {
      gda_sql_expr_free (sel->distinct_expr);
      sel->distinct_expr = NULL;
    }

  if (distinct && part)
    sel->distinct_expr = (GdaSqlExpr*) use_part (part, GDA_SQL_ANY_PART (sel));
  sel->distinct = distinct;
}

/**
 * gda_sql_builder_select_set_limit:
 * @builder: a #GdaSqlBuilder object
 * @limit_count_expr_id: the ID of the LIMIT expression, or %0
 * @limit_offset_expr_id: the ID of the OFFSET expression, or %0
 *
 * If @limit_count_expr_id is not %0, defines the maximum number of rows in the #GdaDataModel
 * resulting from the execution of the built statement. In this case, the offset from which the
 * rows must be collected can be defined by the @limit_offset_expr_id expression if not %0 (note that
 * this feature may not be supported by all the database providers).
 *
 * If @limit_count_expr_id is %0, then removes any LIMIT which may have been imposed by a previous
 * call to this method.
 *
 * Since: 4.2
 */
void
gda_sql_builder_select_set_limit (GdaSqlBuilder *builder,
                                  GdaSqlBuilderId limit_count_expr_id,
                                  GdaSqlBuilderId limit_offset_expr_id)
{
  GdaSqlStatementSelect *sel;
  SqlPart *part1 = NULL, *part2 = NULL;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return;
  }

  if (limit_count_expr_id)
    {
      part1 = get_part (builder, limit_count_expr_id, GDA_SQL_ANY_EXPR);
      if (!part1)
        return;
    }
  if (limit_offset_expr_id)
    {
      part2 = get_part (builder, limit_offset_expr_id, GDA_SQL_ANY_EXPR);
      if (!part2)
        return;
    }

  sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;

  if (sel->limit_count)
    {
      gda_sql_expr_free (sel->limit_count);
      sel->limit_count = NULL;
    }
  if (sel->limit_offset)
    {
      gda_sql_expr_free (sel->limit_offset);
      sel->limit_offset = NULL;
    }
  if (part1)
    sel->limit_count = (GdaSqlExpr*) use_part (part1, GDA_SQL_ANY_PART (sel));
  if (part2)
    sel->limit_offset = (GdaSqlExpr*) use_part (part2, GDA_SQL_ANY_PART (sel));
}

/**
 * gda_sql_builder_select_set_having:
 * @builder: a #GdaSqlBuilder object
 * @cond_id: the ID of the expression to set as HAVING condition, or 0 to unset any previous HAVING condition
 *
 * Valid only for: SELECT statements
 *
 * Sets the HAVING condition of the statement
 *
 * Since: 4.2
 */
void
gda_sql_builder_select_set_having (GdaSqlBuilder *builder, GdaSqlBuilderId cond_id)
{
  GdaSqlStatementSelect *sel;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return;
  }

  SqlPart *p = NULL;
  if (cond_id > 0) {
    p = get_part (builder, cond_id, GDA_SQL_ANY_EXPR);
    if (!p)
      return;
  }

  sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
  if (sel->having_cond)
    gda_sql_expr_free (sel->having_cond);
  sel->having_cond = (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (sel));
}

/**
 * gda_sql_builder_select_group_by:
 * @builder: a #GdaSqlBuilder object
 * @expr_id: the ID of the expression to set use in the GROUP BY clause, or 0 to unset any previous GROUP BY clause
 *
 * Valid only for: SELECT statements
 *
 * Adds the @expr_id expression to the GROUP BY clause's expressions list
 *
 * Since: 4.2
 */
void
gda_sql_builder_select_group_by (GdaSqlBuilder *builder, GdaSqlBuilderId expr_id)
{
  GdaSqlStatementSelect *sel;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);

  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_SELECT) {
    g_warning (_("Wrong statement type"));
    return;
  }

  SqlPart *p = NULL;
  if (expr_id > 0)
    {
      p = get_part (builder, expr_id, GDA_SQL_ANY_EXPR);
      if (!p)
        return;
    }

  sel = (GdaSqlStatementSelect*) priv->main_stmt->contents;
  if (p)
    sel->group_by = g_slist_append (sel->group_by,
                                    (GdaSqlExpr*) use_part (p, GDA_SQL_ANY_PART (sel)));
  else if (sel->group_by)
    {
      g_slist_free_full (sel->group_by, (GDestroyNotify) gda_sql_expr_free);
      sel->group_by = NULL;
    }
}

/**
 * gda_sql_builder_add_function:
 * @builder: a #GdaSqlBuilder object
 * @func_name: the functions's name
 * @...: a list, terminated with %0, of each function's argument's ID
 *
 * Builds a new expression which represents a function applied to some arguments
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_function (GdaSqlBuilder *builder, const gchar *func_name, ...)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (func_name && *func_name, 0);

  GdaSqlExpr *expr;
  GSList *list = NULL;
  va_list ap;
  SqlPart *part;
  GdaSqlBuilderId aid;

  expr = gda_sql_expr_new (NULL);
  expr->func = gda_sql_function_new (GDA_SQL_ANY_PART (expr));
  expr->func->function_name = g_strdup (func_name);

  va_start (ap, func_name);
  for (aid = va_arg (ap, GdaSqlBuilderId); aid; aid = va_arg (ap, GdaSqlBuilderId))
    {
      part = get_part (builder, aid, GDA_SQL_ANY_EXPR);
      if (!part)
        {
          expr->func->args_list = list;
          gda_sql_expr_free (expr);
          va_end (ap);
          return 0;
        }
      list = g_slist_prepend (list, use_part (part, GDA_SQL_ANY_PART (expr->func)));
    }
  va_end (ap);
  expr->func->args_list = g_slist_reverse (list);

  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_function_v: (rename-to gda_sql_builder_add_function)
 * @builder: a #GdaSqlBuilder object
 * @func_name: the functions's name
 * @args: (array length=args_size): an array of IDs representing the function's arguments
 * @args_size: @args's size
 *
 * Builds a new expression which represents a function applied to some arguments
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_function_v (GdaSqlBuilder *builder,
                                const gchar *func_name,
                                const GdaSqlBuilderId *args,
                                gint args_size)
{
  gint i;
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (func_name && *func_name, 0);

  GdaSqlExpr *expr;
  GSList *list = NULL;
  expr = gda_sql_expr_new (NULL);
  expr->func = gda_sql_function_new (GDA_SQL_ANY_PART (expr));
  expr->func->function_name = g_strdup (func_name);

  for (i = 0; i < args_size; i++)
    {
      SqlPart *part;
      part = get_part (builder, args[i], GDA_SQL_ANY_EXPR);
      if (!part)
        {
          expr->func->args_list = list;
          gda_sql_expr_free (expr);
          return 0;
        }
      list = g_slist_prepend (list, use_part (part, GDA_SQL_ANY_PART (expr->func)));
    }
  expr->func->args_list = g_slist_reverse (list);

  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_add_sub_select:
 * @builder: a #GdaSqlBuilder object
 * @sqlst: a pointer to a #GdaSqlStatement, which has to be a SELECT or compound SELECT. This will be copied.
 *
 * Adds an expression which is a subselect.
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_sub_select (GdaSqlBuilder *builder, GdaSqlStatement *sqlst)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (sqlst, 0);
  g_return_val_if_fail ((sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) ||
            (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND), 0);

  GdaSqlExpr *expr;
  expr = gda_sql_expr_new (NULL);

  switch (sqlst->stmt_type)
    {
    case GDA_SQL_STATEMENT_SELECT:
      expr->select = _gda_sql_statement_select_copy (sqlst->contents);
      break;
    case GDA_SQL_STATEMENT_COMPOUND:
      expr->select = _gda_sql_statement_compound_copy (sqlst->contents);
      break;
    default:
      g_assert_not_reached ();
    }

  GDA_SQL_ANY_PART (expr->select)->parent = GDA_SQL_ANY_PART (expr);

  return add_part (builder, (GdaSqlAnyPart *) expr);
}

/**
 * gda_sql_builder_compound_set_type:
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
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_COMPOUND) {
    g_warning (_("Wrong statement type"));
    return;
  }

  cstmt = (GdaSqlStatementCompound*) priv->main_stmt->contents;
  cstmt->compound_type = compound_type;
}

/**
 * gda_sql_builder_compound_add_sub_select:
 * @builder: a #GdaSqlBuilder object
 * @sqlst: a pointer to a #GdaSqlStatement, which has to be a SELECT or compound SELECT. This will be copied.
 *
 * Add a sub select to a COMPOUND statement
 *
 * Since: 4.2
 */
void
gda_sql_builder_compound_add_sub_select (GdaSqlBuilder *builder, GdaSqlStatement *sqlst)
{
  GdaSqlStatementCompound *cstmt;
  GdaSqlStatement *sub;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_COMPOUND) {
    g_warning (_("Wrong statement type"));
    return;
  }
  g_return_if_fail (sqlst);
  g_return_if_fail ((sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) ||
        (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND));

  cstmt = (GdaSqlStatementCompound*) priv->main_stmt->contents;
  sub = gda_sql_statement_copy (sqlst);

  cstmt->stmt_list = g_slist_append (cstmt->stmt_list, sub);
}


/**
 * gda_sql_builder_compound_add_sub_select_from_builder:
 * @builder: a #GdaSqlBuilder object
 * @subselect: a #GdaSqlBuilder, which has to be a SELECT or compound SELECT. This will be copied.
 *
 * Add a sub select to a COMPOUND statement
 *
 * Since: 4.2
 */
void
gda_sql_builder_compound_add_sub_select_from_builder (GdaSqlBuilder *builder, GdaSqlBuilder *subselect)
{
  GdaSqlStatementCompound *cstmt;
  GdaSqlStatement *sqlst;
  GdaSqlStatement *sub;

  g_return_if_fail (GDA_IS_SQL_BUILDER (builder));
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_if_fail (priv->main_stmt);
  g_return_if_fail (GDA_IS_SQL_BUILDER (subselect));
//  g_return_if_fail (subselect->priv->main_stmt);
  if (priv->main_stmt->stmt_type != GDA_SQL_STATEMENT_COMPOUND) {
    g_warning (_("Wrong statement type"));
    return;
  }
  sqlst = gda_sql_builder_get_sql_statement(subselect);
  g_return_if_fail (sqlst);
  g_return_if_fail ((sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) ||
                    (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND));

  cstmt = (GdaSqlStatementCompound*) priv->main_stmt->contents;
  sub = gda_sql_statement_copy (sqlst);

  cstmt->stmt_list = g_slist_append (cstmt->stmt_list, sub);
}

/**
 * gda_sql_builder_add_case:
 * @builder: a #GdaSqlBuilder object
 * @test_expr: the expression ID representing the test of the CASE, or %0
 * @else_expr: the expression ID representing the ELSE expression, or %0
 * @...: a list, terminated by a %0, of (WHEN expression ID, THEN expression ID) representing
 *       all the test cases
 *
 * Creates a new CASE ... WHEN ... THEN ... ELSE ... END expression.
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_case (GdaSqlBuilder *builder,
                          GdaSqlBuilderId test_expr,
                          GdaSqlBuilderId else_expr, ...)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

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
  GdaSqlBuilderId id1;
  va_start (ap, else_expr);
  for (id1 = va_arg (ap, GdaSqlBuilderId); id1; id1 = va_arg (ap, GdaSqlBuilderId))
    {
      GdaSqlBuilderId id2;
      SqlPart *pwhen, *pthen;
      id2 = va_arg (ap, GdaSqlBuilderId);
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
  return add_part (builder, (GdaSqlAnyPart *) expr);

 cleanups:
  va_end (ap);
  gda_sql_expr_free (expr);
  return 0;
}

/**
 * gda_sql_builder_add_case_v: (rename-to gda_sql_builder_add_case)
 * @builder: a #GdaSqlBuilder object
 * @test_expr: the expression ID representing the test of the CASE, or %0
 * @else_expr: the expression ID representing the ELSE expression, or %0
 * @when_array: (array length=args_size): an array containing each WHEN expression ID, having at least @args_size elements
 * @then_array: (array length=args_size): an array containing each THEN expression ID, having at least @args_size elements
 * @args_size: the size of @when_array and @then_array
 *
 * Creates a new CASE ... WHEN ... THEN ... ELSE ... END expression. The WHEN expression and the THEN
 * expression IDs are taken from the @when_array and @then_array at the same index, for each index inferior to
 * @args_size.
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_add_case_v (GdaSqlBuilder *builder,
                            GdaSqlBuilderId test_expr,
                            GdaSqlBuilderId else_expr,
                            const GdaSqlBuilderId *when_array,
                            const GdaSqlBuilderId *then_array,
                            gint args_size)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);

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
  for (i = 0; i < args_size; i++)
    {
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
  return add_part (builder, (GdaSqlAnyPart *) expr);

 cleanups:
  gda_sql_expr_free (expr);
  return 0;
}

/**
 * gda_sql_builder_export_expression:
 * @builder: a #GdaSqlBuilder object
 * @id: the ID of the expression to be exported, (must be a valid ID in @builder, not %0)
 *
 * Exports a part managed by @builder as a new #GdaSqlExpr, which can represent any expression
 * in a statement.
 *
 * Returns: a pointer to a new #GdaSqlExpr structure, free using gda_sql_expr_free() when not
 * needed anymore. If the part with @id as ID cannot be found, the returned value is %NULL.
 *
 * Since: 4.2
 */
GdaSqlExpr *
gda_sql_builder_export_expression (GdaSqlBuilder *builder, GdaSqlBuilderId id)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), NULL);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, NULL);

  SqlPart *part;
  part = get_part (builder, id, GDA_SQL_ANY_EXPR);
  if (! part)
    return NULL;
  g_return_val_if_fail (part->part->type == GDA_SQL_ANY_EXPR, NULL);
  return gda_sql_expr_copy ((GdaSqlExpr*) part->part);
}

/**
 * gda_sql_builder_import_expression:
 * @builder: a #GdaSqlBuilder object
 * @expr: a #GdaSqlExpr obtained using gda_sql_builder_export_expression()
 *
 * Imports the @expr into @builder.
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_import_expression (GdaSqlBuilder *builder, GdaSqlExpr *expr)
{
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (expr, 0);

  g_return_val_if_fail (GDA_SQL_ANY_PART (expr)->type == GDA_SQL_ANY_EXPR, 0);
  return add_part (builder, (GdaSqlAnyPart *) gda_sql_expr_copy (expr));
}


/**
 * gda_sql_builder_import_expression_from_builder:
 * @builder: a #GdaSqlBuilder object
 * @query: a #GdaSqlBuilder object to get expression from
 * @expr_id: a #GdaSqlBuilderId of the expression in @query
 *
 * Imports the an expression located in @query into @builder.
 *
 * Returns: the ID of the new expression, or %0 if there was an error
 *
 * Since: 4.2
 */
GdaSqlBuilderId
gda_sql_builder_import_expression_from_builder (GdaSqlBuilder *builder,
                                                GdaSqlBuilder *query,
                                                GdaSqlBuilderId expr_id)
{
  GdaSqlExpr *expr;

  g_return_val_if_fail (GDA_IS_SQL_BUILDER (builder), 0);
  GdaSqlBuilderPrivate *priv = gda_sql_builder_get_instance_private (builder);
  g_return_val_if_fail (priv->main_stmt, 0);
  g_return_val_if_fail (GDA_IS_SQL_BUILDER (query), 0);
  g_return_val_if_fail (expr_id, 0);

  expr = gda_sql_builder_export_expression (query, expr_id);
  g_return_val_if_fail (GDA_SQL_ANY_PART (expr)->type == GDA_SQL_ANY_EXPR, 0);
  return add_part (builder, (GdaSqlAnyPart *) gda_sql_expr_copy (expr));
}

