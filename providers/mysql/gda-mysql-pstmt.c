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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-mysql-pstmt.h"

static void
gda_mysql_pstmt_class_init (GdaMysqlPStmtClass  *klass);
static void
gda_mysql_pstmt_init (GdaMysqlPStmt       *pstmt,
		      GdaMysqlPStmtClass  *klass);
static void
gda_mysql_pstmt_finalize (GObject  *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_mysql_pstmt_get_type
 *
 * Returns: the #GType of GdaMysqlPStmt.
 */
GType
gda_mysql_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaMysqlPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlPStmt),
			0,
			(GInstanceInitFunc) gda_mysql_pstmt_init,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaMysqlPStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_mysql_pstmt_class_init (GdaMysqlPStmtClass  *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_mysql_pstmt_finalize;
}

static void
gda_mysql_pstmt_init (GdaMysqlPStmt       *pstmt,
		      G_GNUC_UNUSED GdaMysqlPStmtClass  *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	/* initialize specific parts of @pstmt */
	// TO_IMPLEMENT;
	pstmt->mysql_bind_result = NULL;
}

static void
gda_mysql_pstmt_finalize (GObject  *object)
{
	GdaMysqlPStmt *pstmt = (GdaMysqlPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	if (pstmt->mysql_stmt)
		mysql_stmt_close (pstmt->mysql_stmt);

	gint i;
	for (i = 0; i < ((GdaPStmt *) pstmt)->ncols; ++i) {
		g_free (pstmt->mysql_bind_result[i].buffer);
		g_free (pstmt->mysql_bind_result[i].is_null);
		g_free (pstmt->mysql_bind_result[i].length);
	}
	g_free (pstmt->mysql_bind_result);
	pstmt->mysql_bind_result = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/*
 * Steals @mysql_stmt
 */
GdaMysqlPStmt *
gda_mysql_pstmt_new (GdaConnection  *cnc,
		     MYSQL          *mysql,
		     MYSQL_STMT     *mysql_stmt)
{
	GdaMysqlPStmt *ps = (GdaMysqlPStmt *) g_object_new (GDA_TYPE_MYSQL_PSTMT,
							    NULL);
	ps->cnc = cnc;
	ps->mysql = mysql;
	ps->mysql_stmt = mysql_stmt;
	ps->stmt_used = FALSE;

	return ps;
}

