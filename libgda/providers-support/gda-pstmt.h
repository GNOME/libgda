/*
 * Copyright (C) 2008 - 2009 Vivien Malerba <malerba@gnome-db.org>
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
#define GDA_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_PSTMT, GdaPStmt))
#define GDA_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_PSTMT, GdaPStmtClass))
#define GDA_IS_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_PSTMT))
#define GDA_IS_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_PSTMT))

typedef struct _GdaPStmt        GdaPStmt;
typedef struct _GdaPStmtPrivate GdaPStmtPrivate;
typedef struct _GdaPStmtClass   GdaPStmtClass;

struct _GdaPStmt {
	GObject       object;

	GdaPStmtPrivate *priv;
	gchar        *sql; /* actual SQL code used for this prepared statement, mem freed by GdaPStmt */
        GSList       *param_ids; /* list of parameters' IDs (as gchar *), mem freed by GdaPStmt */

	/* meta data */
        gint          ncols;
        GType        *types; /* array of ncols types */
	GSList       *tmpl_columns; /* list of #GdaColumn objects which data models created from this prep. statement
				     * can copy */

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

struct _GdaPStmtClass {
	GObjectClass  parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType         gda_pstmt_get_type          (void) G_GNUC_CONST;
void          gda_pstmt_set_gda_statement (GdaPStmt *pstmt, GdaStatement *stmt);
void          gda_pstmt_copy_contents     (GdaPStmt *src, GdaPStmt *dest);
GdaStatement *gda_pstmt_get_gda_statement (GdaPStmt *pstmt);

G_END_DECLS

#endif
