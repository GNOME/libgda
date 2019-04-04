/*
 * Copyright (C) YEAR The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-capi-pstmt.h"
#include <libgda/gda-debug-macros.h>

static void gda_capi_pstmt_class_init (GdaCapiPStmtClass *klass);
static void gda_capi_pstmt_init       (GdaCapiPStmt *pstmt);
static void gda_capi_pstmt_dispose    (GObject *object);

typedef struct {
  int dummy;
	/* TO_ADD: this structure holds any information necessary to reference a prepared statement, usually a connection
         * handle from the C or C++ API
         */
} GdaCapiPStmtPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaCapiPStmt, gda_capi_pstmt, GDA_TYPE_PSTMT)

static void 
gda_capi_pstmt_class_init (GdaCapiPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual functions */
	object_class->dispose = gda_capi_pstmt_dispose;
}

static void
gda_capi_pstmt_init (GdaCapiPStmt *pstmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	/* initialize specific parts of @pstmt */
	TO_IMPLEMENT;
}

static void
gda_capi_pstmt_dispose (GObject *object)
{
	GdaCapiPStmt *pstmt = (GdaCapiPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	TO_IMPLEMENT; /* free some specific parts of @pstmt */

	/* chain to parent class */
	G_OBJECT_CLASS (gda_capi_pstmt_parent_class)->dispose (object);
}
