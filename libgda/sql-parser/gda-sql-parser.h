/* gda-sql-parser.h
 *
 * Copyright (C) 2007 Vivien Malerba
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


#ifndef __GDA_SQL_PARSER_H_
#define __GDA_SQL_PARSER_H_

#include <glib-object.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-batch.h>
#include <sql-parser/gda-statement-struct.h>
#include <sql-parser/gda-statement-struct-util.h>

G_BEGIN_DECLS

#define GDA_TYPE_SQL_PARSER          (gda_sql_parser_get_type())
#define GDA_SQL_PARSER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_sql_parser_get_type(), GdaSqlParser)
#define GDA_SQL_PARSER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_sql_parser_get_type (), GdaSqlParserClass)
#define GDA_IS_SQL_PARSER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_sql_parser_get_type ())

/* error reporting */
extern GQuark gda_sql_parser_error_quark (void);
#define GDA_SQL_PARSER_ERROR gda_sql_parser_error_quark ()

typedef enum
{
	GDA_SQL_PARSER_SYNTAX_ERROR,
	GDA_SQL_PARSER_OVERFLOW_ERROR,
	GDA_SQL_PARSER_EMPTY_SQL_ERROR
} GdaSqlParserError;

typedef enum {
        GDA_SQL_PARSER_MODE_PARSE,
        GDA_SQL_PARSER_MODE_DELIMIT
} GdaSqlParserMode;

typedef enum {
        GDA_SQL_PARSER_FLAVOUR_STANDARD   = 0,
        GDA_SQL_PARSER_FLAVOUR_SQLITE     = 1,
        GDA_SQL_PARSER_FLAVOUR_MYSQL      = 2,
        GDA_SQL_PARSER_FLAVOUR_ORACLE     = 3,
        GDA_SQL_PARSER_FLAVOUR_POSTGRESQL = 4
} GdaSqlParserFlavour;

/* struct for the object's data */
struct _GdaSqlParser
{
	GObject              object;
	GdaSqlParserPrivate *priv;
};

/* interface with the Lemon parser */
typedef struct _GdaSqlParserIface
{
	GdaSqlParser    *parser;
	GdaSqlStatement *parsed_statement;
} GdaSqlParserIface;

/* struct for the object's class */
struct _GdaSqlParserClass
{
	GObjectClass         parent_class;

	/* virtual methods and data for sub classed parsers */
	void *(*delim_alloc) (void*(*)(size_t));
	void (*delim_free) (void*, void(*)(void*));
	void (*delim_trace) (void*, char *);
	void (*delim_parse) (void*, int, GValue *, GdaSqlParserIface *);
	gint *delim_tokens_trans;
	
	void *(*parser_alloc) (void*(*)(size_t));
	void (*parser_free) (void*, void(*)(void*));
	void (*parser_trace) (void*, char *);
	void (*parser_parse) (void*, int, GValue *, GdaSqlParserIface *);	
	gint *parser_tokens_trans;
};

GType               gda_sql_parser_get_type               (void) G_GNUC_CONST;
GdaSqlParser       *gda_sql_parser_new                    (void);

GdaStatement       *gda_sql_parser_parse_string           (GdaSqlParser *parser, 
							   const gchar *sql, const gchar **remain,
							   GError **error);
GdaBatch           *gda_sql_parser_parse_string_as_batch  (GdaSqlParser *parser, 
							   const gchar *sql, const gchar **remain,
							   GError **error);
GdaBatch           *gda_sql_parser_parse_file_as_batch    (GdaSqlParser *parser, 
							   const gchar *filename, GError **error);

/* private API */
void                gda_sql_parser_set_syntax_error      (GdaSqlParser *parser);
void                gda_sql_parser_set_overflow_error    (GdaSqlParser *parser);

G_END_DECLS

#endif
