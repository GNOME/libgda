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

#ifndef __GDA_CAPI_PSTMT_H__
#define __GDA_CAPI_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-capi.h"

G_BEGIN_DECLS

#define GDA_TYPE_CAPI_PSTMT            (gda_capi_pstmt_get_type())
#define GDA_CAPI_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CAPI_PSTMT, GdaCapiPStmt))
#define GDA_CAPI_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CAPI_PSTMT, GdaCapiPStmtClass))
#define GDA_IS_CAPI_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CAPI_PSTMT))
#define GDA_IS_CAPI_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CAPI_PSTMT))

typedef struct _GdaCapiPStmt        GdaCapiPStmt;
typedef struct _GdaCapiPStmtClass   GdaCapiPStmtClass;

struct _GdaCapiPStmt {
	GdaPStmt        object;

	/* TO_ADD: this structure holds any information necessary to reference a prepared statement, usually a connection
         * handle from the C or C++ API
         */
};

struct _GdaCapiPStmtClass {
	GdaPStmtClass  parent_class;
};

GType         gda_capi_pstmt_get_type  (void) G_GNUC_CONST;
/* TO_ADD: helper function to create a GdaCapiPStmt such as gda_capi_pstmt_new() with some specific arguments */

G_END_DECLS

#endif
