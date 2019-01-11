/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_INSERT_H_
#define _GDA_STATEMENT_STRUCT_INSERT_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>

G_BEGIN_DECLS

/*
 * Structure definition
 */
/**
 * GdaSqlStatementInsert:
 * @any: inheritance structure
 * @on_conflict: conflict resolution clause if there is one (such as "OR REPLACE")
 * @table: name of the table to which data is inserted
 * @fields_list: list of #GdaSqlField fields which are valued for insertion
 * @values_list: list of list of #GdaSqlExpr expressions (this is a list of list, not a simple list)
 * @select: a #GdaSqlStatementSelect or #GdaSqlStatementCompound structure representing the values to insert
 *
 * The statement is an INSERT statement, any kind of INSERT statement can be represented using this structure 
 * (if this is not the case
 * then report a bug).
 * <mediaobject>
 *   <imageobject role="html">
 *     <imagedata fileref="stmt-insert1.png" format="PNG"/>
 *   </imageobject>
 *   <caption>
 *     <para>
 *	Example of a #GdaSqlStatement having a #GdaSqlStatementInsert as its contents with 2 lists of values
 *	to insert.
 *     </para>
 *   </caption>
 * </mediaobject>
 * <mediaobject>
 *   <imageobject role="html">
 *     <imagedata fileref="stmt-insert2.png" format="PNG"/>
 *   </imageobject>
 *   <caption>
 *     <para>
 *	Another example of a #GdaSqlStatement having a #GdaSqlStatementInsert as its contents, using a SELECT
 *	to express the values to insert.
 *     </para>
 *   </caption>
 * </mediaobject>
 */
struct _GdaSqlStatementInsert {
	GdaSqlAnyPart           any;
	gchar                  *on_conflict; /* conflict resolution clause */
	GdaSqlTable            *table;
	GSList                 *fields_list; /* list of GdaSqlField structures */
	GSList                 *values_list; /* list of list of GdaSqlExpr */
	GdaSqlAnyPart          *select; /* SELECT OR COMPOUND statements: GdaSqlStatementSelect or GdaSqlStatementCompound */

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
GdaSqlStatementContentsInfo *_gda_sql_statement_insert_get_infos (void);

/*
 * Functions used by the parser
 */
void gda_sql_statement_insert_take_table_name (GdaSqlStatement *stmt, GValue *value);
void gda_sql_statement_insert_take_on_conflict (GdaSqlStatement *stmt, GValue *value);
void gda_sql_statement_insert_take_fields_list (GdaSqlStatement *stmt, GSList *list);
void gda_sql_statement_insert_take_1_values_list (GdaSqlStatement *stmt, GSList *list);
void gda_sql_statement_insert_take_extra_values_list (GdaSqlStatement *stmt, GSList *list);

void gda_sql_statement_insert_take_select (GdaSqlStatement *stmt, GdaSqlStatement *select);

G_END_DECLS

#endif
