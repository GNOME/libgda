/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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


#ifndef _GDA_STATEMENT_H_
#define _GDA_STATEMENT_H_

#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gda_statement_error_quark (void);
#define GDA_STATEMENT_ERROR gda_statement_error_quark ()

/**
 * GdaStatementError:
 * @GDA_STATEMENT_PARSE_ERROR: 
 * @GDA_STATEMENT_SYNTAX_ERROR: 
 * @GDA_STATEMENT_NO_CNC_ERROR: 
 * @GDA_STATEMENT_CNC_CLOSED_ERROR: 
 * @GDA_STATEMENT_EXEC_ERROR: 
 * @GDA_STATEMENT_PARAM_TYPE_ERROR: 
 * @GDA_STATEMENT_PARAM_ERROR: 
 */
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

/**
 * GdaStatementModelUsage:
 * @GDA_STATEMENT_MODEL_RANDOM_ACCESS: access to the data model will be random (usually this will result in a data model completely stored in memory)
 * @GDA_STATEMENT_MODEL_CURSOR_FORWARD: access to the data model will be done using a cursor moving forward
 * @GDA_STATEMENT_MODEL_CURSOR_BACKWARD: access to the data model will be done using a cursor moving backward
 * @GDA_STATEMENT_MODEL_CURSOR: access to the data model will be done using a cursor (moving both forward and backward)
 * @GDA_STATEMENT_MODEL_ALLOW_NOPARAM: specifies that the data model should be executed even if some parameters required to execute it are invalid (in this case the data model will have no row, and will automatically be re-run when the missing parameters are once again valid)
 * @GDA_STATEMENT_MODEL_OFFLINE: specifies that the data model's contents will be fully loaded into the client side (the memory of the process using Libgda), not requiring the server any more to access the data (the default behaviour is to access the server any time data is to be read, and data is cached in memory). This flag is useful only if used in conjunction with the GDA_STATEMENT_MODEL_RANDOM_ACCESS flag (otherwise an error will be returned).
 *
 * These flags specify how the #GdaDataModel returned when executing a #GdaStatement will be used
 */
typedef enum {
	GDA_STATEMENT_MODEL_RANDOM_ACCESS   = 1 << 0,
	GDA_STATEMENT_MODEL_CURSOR_FORWARD  = 1 << 1,
	GDA_STATEMENT_MODEL_CURSOR_BACKWARD = 1 << 2,
	GDA_STATEMENT_MODEL_CURSOR          = GDA_STATEMENT_MODEL_CURSOR_FORWARD | GDA_STATEMENT_MODEL_CURSOR_BACKWARD,
	GDA_STATEMENT_MODEL_ALLOW_NOPARAM   = 1 << 3,
	GDA_STATEMENT_MODEL_OFFLINE         = 1 << 4
} GdaStatementModelUsage;

/**
 * GdaStatementSqlFlag:
 * @GDA_STATEMENT_SQL_PARAMS_AS_VALUES: rendering will replace parameters with their values
 * @GDA_STATEMENT_SQL_PRETTY: rendering will include newlines and indentation to make it easy to read
 * @GDA_STATEMENT_SQL_PARAMS_LONG: parameters will be rendered using the "/&ast; name:&lt;param_name&gt; ... &ast;/" syntax
 * @GDA_STATEMENT_SQL_PARAMS_SHORT: parameters will be rendered using the "##&lt;param_name&gt;..." syntax
 * @GDA_STATEMENT_SQL_PARAMS_AS_COLON: parameters will be rendered using the ":&lt;param_name&gt;" syntax
 * @GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR: parameters will be rendered using the "$&lt;param_number&gt;" syntax where parameters are numbered starting from 1
 * @GDA_STATEMENT_SQL_PARAMS_AS_QMARK: parameters will be rendered using the "?&lt;param_number&gt;" syntax where parameters are numbered starting from 1
 * @GDA_STATEMENT_SQL_PARAMS_AS_UQMARK: parameters will be rendered using the "?" syntax
 * @GDA_STATEMENT_SQL_TIMEZONE_TO_GMT: time and timestamp with a timezone information are converted to GMT before rendering, and rendering does not show the timezone information
 *
 * Specifies rendering options
 */
typedef enum {
	GDA_STATEMENT_SQL_PARAMS_AS_VALUES   = 0,
        GDA_STATEMENT_SQL_PRETTY             = 1 << 0,
        GDA_STATEMENT_SQL_PARAMS_LONG        = 1 << 1,
        GDA_STATEMENT_SQL_PARAMS_SHORT       = 1 << 2,
        GDA_STATEMENT_SQL_PARAMS_AS_COLON    = 1 << 3,
        GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR   = 1 << 4,
        GDA_STATEMENT_SQL_PARAMS_AS_QMARK    = 1 << 5,
        GDA_STATEMENT_SQL_PARAMS_AS_UQMARK   = 1 << 6,
        GDA_STATEMENT_SQL_TIMEZONE_TO_GMT    = 1 << 7
} GdaStatementSqlFlag;


#define GDA_TYPE_STATEMENT          (gda_statement_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaStatement, gda_statement, GDA, STATEMENT, GObject)

/* struct for the object's class */
struct _GdaStatementClass
{
	GObjectClass         parent_class;

	/* signals */
	void   (*checked)   (GdaStatement *stmt, GdaConnection *cnc, gboolean checked);
	void   (*reset)     (GdaStatement *stmt);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-statement
 * @short_description: Single SQL statement
 * @title: GdaStatement
 * @stability: Stable
 * @see_also: #GdaBatch
 *
 * The #GdaStatement represents a single SQL statement (multiple statements can be grouped in a #GdaBatch object).
 *
 *  A #GdaStatement can either be built by describing its constituing parts using a #GdaSqlBuilder object,
 *  or from an SQL statement using a #GdaSqlParser object.
 *
 *  A #GdaConnection can use a #GdaStatement to:
 *  <itemizedlist>
 *    <listitem><para>prepare it for a future execution, the preparation step involves converting the #GdaStatement
 *	object into a structure used by the database's own API, see gda_connection_statement_prepare()</para></listitem>
 *    <listitem><para>execute it using gda_connection_statement_execute_select() if it is known that the statement is a
 *	selection statement, gda_connection_statement_execute_non_select() if it is not a selection statement, or
 *	gda_connection_statement_execute() when the type of expected result is unknown.</para></listitem>
 *  </itemizedlist>
 *  Note that it is possible to use the same #GdaStatement object at the same time with several #GdaConnection objects.
 */

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
