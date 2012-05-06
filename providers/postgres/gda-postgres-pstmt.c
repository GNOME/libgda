/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "gda-postgres-pstmt.h"
#include "gda-postgres-util.h"

static void gda_postgres_pstmt_class_init (GdaPostgresPStmtClass *klass);
static void gda_postgres_pstmt_init       (GdaPostgresPStmt *pstmt, GdaPostgresPStmtClass *klass);
static void gda_postgres_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_postgres_pstmt_get_type
 *
 * Returns: the #GType of GdaPostgresPStmt.
 */
GType
gda_postgres_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaPostgresPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresPStmt),
			0,
			(GInstanceInitFunc) gda_postgres_pstmt_init,
			0
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaPostgresPStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_postgres_pstmt_class_init (GdaPostgresPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_postgres_pstmt_finalize;
}

static void
gda_postgres_pstmt_init (GdaPostgresPStmt *pstmt, G_GNUC_UNUSED GdaPostgresPStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	pstmt->prep_name = NULL;
}

static void
gda_postgres_pstmt_finalize (GObject *object)
{
	GdaPostgresPStmt *pstmt = (GdaPostgresPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* deallocate statement */
	gchar *sql;
	PGresult *pg_res;

	sql = g_strdup_printf ("DEALLOCATE %s", pstmt->prep_name);
	pg_res = _gda_postgres_PQexec_wrap (pstmt->cnc, pstmt->pconn, sql);
	g_free (sql);
	if (pg_res) 
		PQclear (pg_res);
	
	/* free memory */
	g_free (pstmt->prep_name);

	/* chain to parent class */
	parent_class->finalize (object);
}

GdaPostgresPStmt *
gda_postgres_pstmt_new (GdaConnection *cnc, PGconn *pconn, const gchar *prep_name)
{
	GdaPostgresPStmt *pstmt;

	pstmt = (GdaPostgresPStmt *) g_object_new (GDA_TYPE_POSTGRES_PSTMT, NULL);
	pstmt->prep_name = g_strdup (prep_name);
	pstmt->cnc = cnc;
	pstmt->pconn = pconn;

	return pstmt;
}
