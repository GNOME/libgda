/* GDA Mysql provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
		static const GTypeInfo info = {
			sizeof (GdaMysqlPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlPStmt),
			0,
			(GInstanceInitFunc) gda_mysql_pstmt_init
		};

		type = g_type_register_static (GDA_TYPE_PSTMT, "GdaMysqlPStmt", &info, 0);
	}
	return type;
}

static void 
gda_mysql_pstmt_class_init (GdaMysqlPStmtClass  *klass)
{
	g_print ("*** %s\n", __func__);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_mysql_pstmt_finalize;
}

static void
gda_mysql_pstmt_init (GdaMysqlPStmt       *pstmt,
		      GdaMysqlPStmtClass  *klass)
{
	g_print ("*** %s\n", __func__);
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	/* initialize specific parts of @pstmt */
	// TO_IMPLEMENT;
}

static void
gda_mysql_pstmt_finalize (GObject  *object)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlPStmt *pstmt = (GdaMysqlPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	// TO_IMPLEMENT; /* free some specific parts of @pstmt */

	/* chain to parent class */
	parent_class->finalize (object);
}


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

	return ps;
}

