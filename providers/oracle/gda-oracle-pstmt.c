/* GDA Oracle provider
 * Copyright (C) 2009 The GNOME Foundation.
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
#include "gda-oracle-pstmt.h"
#include "gda-oracle-util.h"

static void gda_oracle_pstmt_class_init (GdaOraclePStmtClass *klass);
static void gda_oracle_pstmt_init       (GdaOraclePStmt *pstmt, GdaOraclePStmtClass *klass);
static void gda_oracle_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_oracle_pstmt_get_type
 *
 * Returns: the #GType of GdaOraclePStmt.
 */
GType
gda_oracle_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaOraclePStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaOraclePStmt),
			0,
			(GInstanceInitFunc) gda_oracle_pstmt_init
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaOraclePStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_oracle_pstmt_class_init (GdaOraclePStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_oracle_pstmt_finalize;
}

static void
gda_oracle_pstmt_init (GdaOraclePStmt *pstmt, GdaOraclePStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	/* initialize specific parts of @pstmt */
	pstmt->hstmt = NULL;
	pstmt->ora_values = NULL;
}

static void
gda_oracle_pstmt_finalize (GObject *object)
{
	GdaOraclePStmt *pstmt = (GdaOraclePStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	OCIStmtRelease ((dvoid *) pstmt->hstmt, NULL, NULL, 0, OCI_STRLS_CACHE_DELETE);
	if (pstmt->ora_values) {
		g_list_foreach (pstmt->ora_values, (GFunc) _gda_oracle_value_free, NULL);
		g_list_free (pstmt->ora_values);
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

GdaOraclePStmt *
gda_oracle_pstmt_new (OCIStmt *hstmt)
{
	GdaOraclePStmt *pstmt;

        pstmt = (GdaOraclePStmt *) g_object_new (GDA_TYPE_ORACLE_PSTMT, NULL);
        pstmt->hstmt = hstmt;

        return pstmt;

}
