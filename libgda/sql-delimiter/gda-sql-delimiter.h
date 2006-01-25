/* gda-sql-delimiter.h
 *
 * Copyright (C) 2004 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef _GDA_DELIMITER_SQL_PARSER_H
#define _GDA_DELIMITER_SQL_PARSER_H

#include <glib.h>
#define YY_NO_UNISTD_H


typedef struct GdaDelimiterStatement        GdaDelimiterStatement;
typedef struct GdaDelimiterExpr             GdaDelimiterExpr;
typedef struct GdaDelimiterParamSpec        GdaDelimiterParamSpec;

/*
 * Type of parsed SQL
 */
typedef enum
{
	GDA_DELIMITER_SQL_SELECT,
	GDA_DELIMITER_SQL_INSERT,
	GDA_DELIMITER_SQL_DELETE,
	GDA_DELIMITER_SQL_UPDATE,
	GDA_DELIMITER_UNKNOWN
}
GdaDelimiterStatementType;


/*
 * Structure to hold a SQL statement
 */
struct GdaDelimiterStatement
{
	GdaDelimiterStatementType  type;
	char                      *full_query;
	GList                     *expr_list;   /* list of GdaDelimiterExpr structures */
	GList                     *params_specs;/* list of GList of GdaDelimiterParamSpec structures */
};

/*
 * Structure to hold a single statement
 */
struct GdaDelimiterExpr
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
	GDA_DELIMITER_PARAM_NULLOK
} GdaDelimiterParamSpecType;

/*
 * Structure to hold one parameter specification
 */
struct GdaDelimiterParamSpec
{
	GdaDelimiterParamSpecType  type;
	char            *content;
};
#define GDA_DELIMITER_PARAM_SPEC(x) ((GdaDelimiterParamSpec *)(x))

void  gda_delimiter_display (GdaDelimiterStatement * statement);
int   gda_delimiter_destroy (GdaDelimiterStatement * statement);

GdaDelimiterStatement *gda_delimiter_parse                (const char *sql_text);
GdaDelimiterStatement *gda_delimiter_parse_with_error     (const char *sql_text, GError ** error);
GdaDelimiterStatement *gda_delimiter_no_parse             (const char *sql_text);

GdaDelimiterStatement *gda_delimiter_parse_copy_statement (GdaDelimiterStatement * statement);

#endif
