/* gda-statement.h
 *
 * Copyright (C) 2007 - 2008 Vivien Malerba
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


#ifndef _GDA_STATEMENT_H_
#define _GDA_STATEMENT_H_

#include <glib-object.h>
#include <sql-parser/gda-statement-struct.h>

G_BEGIN_DECLS

#define GDA_TYPE_STATEMENT          (gda_statement_get_type())
#define GDA_STATEMENT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_statement_get_type(), GdaStatement)
#define GDA_STATEMENT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_statement_get_type (), GdaStatementClass)
#define GDA_IS_STATEMENT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_statement_get_type ())

/* error reporting */
extern GQuark gda_statement_error_quark (void);
#define GDA_STATEMENT_ERROR gda_statement_error_quark ()

typedef enum
{
	GDA_STATEMENT_PARSE_ERROR,
	GDA_STATEMENT_SYNTAX_ERROR,
	GDA_STATEMENT_NO_CNC_ERROR,
	GDA_STATEMENT_CNC_CLOSED_ERROR,
	GDA_STATEMENT_EXEC_ERROR,
	GDA_STATEMENT_PARAM_TYPE_ERROR,
	GDA_STATEMENT_PARAM_ERROR
} GdaStatementError;

typedef enum {
	GDA_STATEMENT_MODEL_RANDOM_ACCESS   = 1 << 0,
	GDA_STATEMENT_MODEL_CURSOR_FORWARD  = 1 << 1,
	GDA_STATEMENT_MODEL_CURSOR_BACKWARD = 1 << 2,
	GDA_STATEMENT_MODEL_CURSOR          = GDA_STATEMENT_MODEL_CURSOR_FORWARD | GDA_STATEMENT_MODEL_CURSOR_BACKWARD,
	GDA_STATEMENT_MODEL_ALLOW_NOPARAM   = 1 << 3
} GdaStatementModelUsage;

typedef enum {
        GDA_STATEMENT_SQL_PRETTY             = 1 << 0,
        GDA_STATEMENT_SQL_PARAMS_LONG        = 1 << 1,
        GDA_STATEMENT_SQL_PARAMS_SHORT       = 1 << 2,
        GDA_STATEMENT_SQL_PARAMS_AS_COLON    = 1 << 3,
        GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR   = 1 << 4,
        GDA_STATEMENT_SQL_PARAMS_AS_QMARK    = 1 << 5,
        GDA_STATEMENT_SQL_PARAMS_AS_UQMARK   = 1 << 6
} GdaStatementSqlFlag;

/* struct for the object's data */
struct _GdaStatement
{
	GObject              object;
	GdaStatementPrivate *priv;
};

/* struct for the object's class */
struct _GdaStatementClass
{
	GObjectClass         parent_class;

	/* signals */
	void   (*checked)   (GdaStatement *stmt, GdaConnection *cnc, gboolean checked);
	void   (*reset)     (GdaStatement *stmt);
};

GType               gda_statement_get_type               (void) G_GNUC_CONST;
GdaStatement       *gda_statement_new                    (void);
GdaStatement       *gda_statement_copy                   (GdaStatement *orig);

gchar              *gda_statement_serialize              (GdaStatement *stmt);

gboolean            gda_statement_get_parameters         (GdaStatement *stmt, GdaSet **out_params, GError **error);
#define             gda_statement_to_sql(stmt,params,error) gda_statement_to_sql_extended ((stmt), NULL, (params), GDA_STATEMENT_SQL_PARAMS_SHORT, NULL, (error))
gchar              *gda_statement_to_sql_extended        (GdaStatement *stmt, GdaConnection *cnc, 
							  GdaSet *params, GdaStatementSqlFlag flags, 
							  GSList **params_used, GError **error);

GdaSqlStatementType gda_statement_get_statement_type     (GdaStatement *stmt);
gboolean            gda_statement_is_useless             (GdaStatement *stmt);
gboolean            gda_statement_check_structure        (GdaStatement *stmt, GError **error);
gboolean            gda_statement_check_validity         (GdaStatement *stmt, GdaConnection *cnc, GError **error);
gboolean            gda_statement_normalize              (GdaStatement *stmt, GdaConnection *cnc, GError **error);

G_END_DECLS

#endif
