/* GDA Capi provider
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
#include "gda-capi-pstmt.h"

static void gda_capi_pstmt_class_init (GdaCapiPStmtClass *klass);
static void gda_capi_pstmt_init       (GdaCapiPStmt *pstmt, GdaCapiPStmtClass *klass);
static void gda_capi_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_capi_pstmt_get_type
 *
 * Returns: the #GType of GdaCapiPStmt.
 */
GType
gda_capi_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaCapiPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_capi_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaCapiPStmt),
			0,
			(GInstanceInitFunc) gda_capi_pstmt_init
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaCapiPStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_capi_pstmt_class_init (GdaCapiPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_capi_pstmt_finalize;
}

static void
gda_capi_pstmt_init (GdaCapiPStmt *pstmt, GdaCapiPStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	/* initialize specific parts of @pstmt */
	TO_IMPLEMENT;
}

static void
gda_capi_pstmt_finalize (GObject *object)
{
	GdaCapiPStmt *pstmt = (GdaCapiPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	TO_IMPLEMENT; /* free some specific parts of @pstmt */

	/* chain to parent class */
	parent_class->finalize (object);
}
