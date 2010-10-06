/* GDA Jdbc provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-jdbc-pstmt.h"

static void gda_jdbc_pstmt_class_init (GdaJdbcPStmtClass *klass);
static void gda_jdbc_pstmt_init       (GdaJdbcPStmt *pstmt, GdaJdbcPStmtClass *klass);
static void gda_jdbc_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_jdbc_pstmt_get_type
 *
 * Returns: the #GType of GdaJdbcPStmt.
 */
GType
gda_jdbc_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaJdbcPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_jdbc_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaJdbcPStmt),
			0,
			(GInstanceInitFunc) gda_jdbc_pstmt_init,
			0
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaJdbcPStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_jdbc_pstmt_class_init (GdaJdbcPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_jdbc_pstmt_finalize;
}

static void
gda_jdbc_pstmt_init (GdaJdbcPStmt *pstmt, G_GNUC_UNUSED GdaJdbcPStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	pstmt->pstmt_obj = NULL;
}

static void
gda_jdbc_pstmt_finalize (GObject *object)
{
	GdaJdbcPStmt *pstmt = (GdaJdbcPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	if (pstmt->pstmt_obj)
		gda_value_free (pstmt->pstmt_obj);

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gda_jdbc_pstmt_new 
 *
 * @pstmt_obj may be NULL for a direct SQL statement or meta data
 */
GdaJdbcPStmt *
gda_jdbc_pstmt_new (GValue *pstmt_obj)
{
	GdaJdbcPStmt *pstmt;
	pstmt = g_object_new (GDA_TYPE_JDBC_PSTMT, NULL);
	pstmt->pstmt_obj = pstmt_obj;

	return pstmt;
}
