/* gda-sql-builder.h
 *
 * Copyright (C) 2009 Vivien Malerba
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

#ifndef __GDA_SQL_BUILDER_H_
#define __GDA_SQL_BUILDER_H_

#include <glib-object.h>
#include <sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

#define GDA_TYPE_SQL_BUILDER          (gda_sql_builder_get_type())
#define GDA_SQL_BUILDER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_sql_builder_get_type(), GdaSqlBuilder)
#define GDA_SQL_BUILDER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_sql_builder_get_type (), GdaSqlBuilderClass)
#define GDA_IS_SQL_BUILDER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_sql_builder_get_type ())

typedef struct _GdaSqlBuilder        GdaSqlBuilder;
typedef struct _GdaSqlBuilderClass   GdaSqlBuilderClass;
typedef struct _GdaSqlBuilderPrivate GdaSqlBuilderPrivate;

/* error reporting */
extern GQuark gda_sql_builder_error_quark (void);
#define GDA_SQL_BUILDER_ERROR gda_sql_builder_error_quark ()

typedef enum {
	GDA_SQL_BUILDER_WRONG_TYPE_ERROR,
	GDA_SQL_BUILDER_MISUSE_ERROR
} GdaSqlBuilderError;


/* struct for the object's data */
struct _GdaSqlBuilder
{
	GObject               object;
	GdaSqlBuilderPrivate  *priv;
};

/* struct for the object's class */
struct _GdaSqlBuilderClass
{
	GObjectClass              parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType             gda_sql_builder_get_type       (void) G_GNUC_CONST;
GdaSqlBuilder    *gda_sql_builder_new            (GdaSqlStatementType stmt_type);
GdaStatement     *gda_sql_builder_get_statement (GdaSqlBuilder *builder, GError **error);
GdaSqlStatement  *gda_sql_builder_get_sql_statement (GdaSqlBuilder *builder, gboolean copy_it);

/* Expression API */
guint              gda_sql_builder_literal (GdaSqlBuilder *builder, guint id, const gchar *string);
guint              gda_sql_builder_expr (GdaSqlBuilder *builder, guint id, GdaDataHandler *dh, GType type, ...);
guint              gda_sql_builder_param (GdaSqlBuilder *builder, guint id, const gchar *param_name, GType type, gboolean nullok);

guint              gda_sql_builder_cond1 (GdaSqlBuilder *builder, guint id, GdaSqlOperatorType op, guint op1);
guint              gda_sql_builder_cond2 (GdaSqlBuilder *builder, guint id, GdaSqlOperatorType op, guint op1, guint op2);
guint              gda_sql_builder_cond3 (GdaSqlBuilder *builder, guint id, GdaSqlOperatorType op, guint op1, guint op2, guint op3);

/* General Statement API */
void              gda_sql_builder_set_table (GdaSqlBuilder *builder, const gchar *table_name);
void              gda_sql_builder_set_where (GdaSqlBuilder *builder, guint cond_id);

void              gda_sql_builder_add_field_value (GdaSqlBuilder *builder, const gchar *field_name, guint value_id);

G_END_DECLS

#endif
