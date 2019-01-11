/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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


#ifndef __GDA_SQL_PARSER_H_
#define __GDA_SQL_PARSER_H_

#include <glib-object.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-batch.h>
#include <libgda/sql-parser/gda-statement-struct.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>

G_BEGIN_DECLS

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

/* interface with the Lemon parser */
typedef struct _GdaSqlParserIface
{
	GdaSqlParser    *parser;
	GdaSqlStatement *parsed_statement;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
} GdaSqlParserIface;

#define GDA_TYPE_SQL_PARSER          (gda_sql_parser_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaSqlParser, gda_sql_parser, GDA, SQL_PARSER, GObject)
/* struct for the object's class */
struct _GdaSqlParserClass
{
	GObjectClass         parent_class;

	/* virtual methods and data for sub classed parsers */
	void *(*delim_alloc) (void *(*f) (size_t s));
	void (*delim_free) (void  *d, void(*f) (void *d));
	void (*delim_trace) (void *d, char *s);
	void (*delim_parse) (void *d, int i, GValue *v, GdaSqlParserIface *iface);
	gint *delim_tokens_trans;
	
	void *(*parser_alloc) (void *(*f)(size_t s));
	void (*parser_free) (void *p, void(*f)(void *p));
	void (*parser_trace) (void *p, char *s);
	void (*parser_parse) (void *p, int i, GValue *v, GdaSqlParserIface *iface);
	gint *parser_tokens_trans;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-sql-parser
 * @short_description: SQL parser
 * @title: GdaSqlParser
 * @stability: Stable
 * @see_also: #GdaSqlBuilder, #GdaSqlStatement and #GdaStatement
 *
 * The #GdaSqlParser is an object dedicated to creating #GdaStatement and #GdaBatch objects from SQL strings. The actual contents
 * of the parsed statements is represented as #GdaSqlStatement structures (which can be obtained from any #GdaStatement through the
 * "structure" property).
 *
 * #GdaSqlParser parsers can be created by calling gda_server_provider_create_parser() for a provider adapted SQL parser, or using
 * gda_sql_parser_new() for a general purpose SQL parser.
 *
 * The #GdaSqlParser can either work in "parse" mode where it will try to parse the SQL string, or in "delimiter" mode where it will
 * only attempt at delimiting SQL statements in a string which may contain several SQL statements (usually separated by a semi column).
 * If operating in "parser" mode, and the parser can't correctly parse the string, then it will switch to the "delimiter" mode
 * for the next statement in the string to parse (and create a GDA_SQL_STATEMENT_UNKNOWN statement).
 *
 * The #GdaSqlParser object parses and analyzes SQL statements and reports the following statement types:
 * <itemizedlist>
 * <listitem><para>SELECT (and COMPOUND select), 
 *     INSERT, UPDATE and DELETE SQL statements should be completely parsed. 
 * </para></listitem>
 * <listitem><para>Transaction related statements (corresponding to the BEGIN, COMMIT, ROLLBACK,
 * SAVEPOINT, ROLLBACK SAVEPOINT and DELETE SAVEPOINT) are parsed and a minimalist structure is created to 
 * extract some information (that structure is not enough per-se to re-create the complete SQL statement).
 * </para></listitem>
 * <listitem><para>Any other type of SQL statement (CREATE TABLE, ...) creates a #GdaStatement of type 
 *     GDA_SQL_STATEMENT_UNKNOWN, and it only able to locate place holders (variables) and end of statement
 *     marks.</para></listitem>
 * </itemizedlist>
 *
 * NOTE: Any SQL of a type which should be parsed which but which creates a #GdaStatement of type GDA_SQL_STATEMENT_UNKNOWN
 * (check with gda_statement_get_statement_type()) should be reported as a bug.
 *
 * The #GdaSqlParser object recognizes place holders (variables), which can later be queried and valued using
 * gda_statement_get_parameters(). The following syntax are recognized (other syntaxes might be 
 * recognized for specific database providers if the #GdaSqlParser is created using gda_server_provider_create_parser()
 * but for portability reasons it's better to avoid them):
 * <itemizedlist>
 * <listitem><para><programlisting>##NAME[::TYPE[::NULL]]]</programlisting>: 
 *     for a variable named NAME with the optional type TYPE (which can be a GType
 *     name or a custom database type name), and with the optional "::NULL" to instruct that the variable can
 *     be NULL.
 * </para></listitem>
 * <listitem><para>
 *  <programlisting>## /&ast; name:NAME [type:TYPE] [nullok:[TRUE|FALSE]] [descr:DESCR] &ast;/</programlisting>
 *     for a variable named NAME with the optional type TYPE (which can be a GType
 *     name or a custom database type name), with the optional "nullok" attribute and an optional
 *     description DESCR. Note that the NAME, TYPE and DESCR literals here must be quoted (simple or double quotes) if
 *     they include non alphanumeric characters, and that there must always be at least a space between the 
 *     <![CDATA[##]]> and the opening and closing comments (C style).
 * </para></listitem>
 * </itemizedlist>
 * Note that the type string must be a type recognized by the
 * <link linkend="gda-g-type-from-string">gda_g_type_from_string()</link> function (all valid GType names
 * plus a few synonyms). Examples of correct place holders definitions are:
 * <programlisting>
 *## /&ast; name:"+0" type:gchararray &ast;/
 *## /&ast; name:'-5' type:string &ast;/
 *## /&ast;name:myvar type:gint descr:ToBeDefined nullok:FALSE&ast;/
 *## /&ast;name:myvar type:int descr:"A long description"&ast;/
 *##+0::gchararray
 *##-5::timestamp
 *</programlisting>
 *
 * Also note that variables should not be used when an SQL identifier is expected. For example the following
 * examples <emphasis>should be avoided</emphasis> because they may not work properly (depending on the database being used):
 *<programlisting>
 *SELECT * FROM ##tablename::string;
 *DELETE FROM mytable WHERE ##tcol::string = 5;
 *ALTER GROUP mygroup ADD USER ##name::gchararray;
 *</programlisting>
 *
 * The #GdaSqlParser object internally uses a LEMON generated parser (the same as the one used by SQLite).
 *
 * The #GdaSqlParser object implements its own locking mechanism so it is thread-safe.
 */

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
