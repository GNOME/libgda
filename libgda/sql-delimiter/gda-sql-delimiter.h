/* gda-sql-delimiter.h
 *
 * Copyright (C) 2004 - 2006 Vivien Malerba
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


#ifndef _GDA_DELIMITER_SQL_PARSER_H
#define _GDA_DELIMITER_SQL_PARSER_H

#include <glib.h>
#define YY_NO_UNISTD_H


typedef struct _GdaDelimiterStatement       GdaDelimiterStatement;
typedef struct _GdaDelimiterExpr            GdaDelimiterExpr;
typedef struct _GdaDelimiterParamSpec       GdaDelimiterParamSpec;

/*
 * Type of parsed SQL
 */
typedef enum
{
	GDA_DELIMITER_SQL_SELECT,
	GDA_DELIMITER_SQL_INSERT,
	GDA_DELIMITER_SQL_DELETE,
	GDA_DELIMITER_SQL_UPDATE,
	GDA_DELIMITER_SQL_BEGIN,
	GDA_DELIMITER_SQL_COMMIT,
	GDA_DELIMITER_MULTIPLE,
	GDA_DELIMITER_UNKNOWN
}
GdaDelimiterStatementType;


/*
 * Structure to hold a SQL statement
 */
struct _GdaDelimiterStatement
{
	GdaDelimiterStatementType  type;
	GList                     *expr_list;   /* list of GdaDelimiterExpr structures */
	GList                     *params_specs;/* list of GList of GdaDelimiterParamSpec structures */
};

/*
 * Structure to hold a single statement
 */
struct _GdaDelimiterExpr
{
	GList         *pspec_list; /* list of GdaDelimiterParamSpec structures */
	gchar         *sql_text;
};
#define GDA_DELIMITER_SQL_EXPR(x) ((GdaDelimiterExpr*)(x))

/*
 * Different types of parameter specifications
 */
typedef enum
{
	GDA_DELIMITER_PARAM_NAME,
	GDA_DELIMITER_PARAM_DESCR,
	GDA_DELIMITER_PARAM_TYPE,
	GDA_DELIMITER_PARAM_ISPARAM,
	GDA_DELIMITER_PARAM_NULLOK,
	GDA_DELIMITER_PARAM_DEFAULT
} GdaDelimiterParamSpecType;

/*
 * Structure to hold one parameter specification
 */
struct _GdaDelimiterParamSpec
{
	GdaDelimiterParamSpecType  type;
	char                      *content;
};
#define GDA_DELIMITER_PARAM_SPEC(x) ((GdaDelimiterParamSpec *)(x))

void                   gda_delimiter_display              (GdaDelimiterStatement *statement);
gchar                 *gda_delimiter_to_string            (GdaDelimiterStatement *statement);
int                    gda_delimiter_destroy              (GdaDelimiterStatement *statement);

GList                 *gda_delimiter_parse                (const char *sql_text);
GList                 *gda_delimiter_parse_with_error     (const char *sql_text, GError **error);
gchar                **gda_delimiter_split_sql            (const char *sql_text);

void                   gda_delimiter_free_list            (GList *statements);
GdaDelimiterStatement *gda_delimiter_concat_list          (GList *statements);

GdaDelimiterStatement *gda_delimiter_parse_copy_statement (GdaDelimiterStatement *statement, GHashTable *repl);

#endif
