/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 - 2013 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_PSTMT_H__
#define __GDA_PSTMT_H__

#include <glib-object.h>
#include <libgda/gda-statement.h>

G_BEGIN_DECLS

#define GDA_TYPE_PSTMT            (gda_pstmt_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaPStmt, gda_pstmt, GDA, PSTMT, GObject)

/**
 * GdaPStmt:
 * @sql: actual SQL code used for this prepared statement, mem freed by GdaPStmt
 * @param_ids: (element-type utf8): list of parameters' IDs (as gchar *), mem freed by GdaPStmt
 * @ncols: number of types in array
 * @types: (array length=ncols) (element-type GLib.Type): array of ncols types
 * @tmpl_columns: (element-type Gda.Column): list of #GdaColumn objects which data models created from this prep. statement can copy
 *
 */

struct _GdaPStmtClass {
	GObjectClass  parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-pstmt
 * @short_description: Base class for prepared statement's
 * @title: GdaPstmt
 * @stability: Stable
 * @see_also:
 *
 * The #GdaPStmt represents the association between a #GdaStatement statement and a <emphasis>prepared statement</emphasis>
 * which is database dependent and is an in-memory representation of a statement. Using prepared statement has the
 * following advantages:
 * <itemizedlist>
 *   <listitem><para>the parsing of the SQL has to be done only once, which improves performances if the statement
 *	has to be executed more than once</para></listitem>
 *   <listitem><para>if a statement has been prepared, then it means it is syntactically correct and has been
 *	<emphasis>understood</emphasis> by the database's API</para></listitem>
 *   <listitem><para>it is possible to use variables in prepared statement which eliminates the risk
 *	of SQL code injection</para></listitem>
 * </itemizedlist>
 *
 * The #GdaPStmt is not intended to be instantiated, but subclassed by database provider's implementation.
 * Once created, the database provider's implementation can decide to associate (for future lookup) to
 * a #GdaStatement object in a connection using gda_connection_add_prepared_statement().
 *
 * The #GdaPStmt object can keep a reference to the #GdaStatement object (which can be set and get using
 * the gda_pstmt_set_gda_statement() and gda_pstmt_get_gda_statement()), however that reference
 * if a weak one (which means it will be lost if the #GdaStatement object is destroyed).
 */

void          gda_pstmt_set_gda_statement (GdaPStmt *pstmt, GdaStatement *stmt);
void          gda_pstmt_copy_contents     (GdaPStmt *src, GdaPStmt *dest);
GdaStatement *gda_pstmt_get_gda_statement (GdaPStmt *pstmt);


gchar         *gda_pstmt_get_sql          (GdaPStmt *ps);
void           gda_pstmt_set_sql          (GdaPStmt *ps, const gchar *sql);
GSList        *gda_pstmt_get_param_ids    (GdaPStmt *ps);
void           gda_pstmt_set_param_ids    (GdaPStmt *ps, GSList* params);
gint           gda_pstmt_get_ncols        (GdaPStmt *ps);
void           gda_pstmt_set_cols         (GdaPStmt *ps, gint ncols, GType *types);
GSList        *gda_pstmt_get_tmpl_columns (GdaPStmt *ps);
void           gda_pstmt_set_tmpl_columns (GdaPStmt *ps, GSList* columns);
GType         *gda_pstmt_get_types        (GdaPStmt *ps);

G_END_DECLS

#endif
