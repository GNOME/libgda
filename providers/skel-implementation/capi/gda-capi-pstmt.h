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
G_DECLARE_DERIVABLE_TYPE(GdaCapiPStmt, gda_capi_pstmt, GDA, CAPI_PSTMT, GdaPStmt)
struct _GdaCapiPStmtClass {
	GdaPStmtClass  parent_class;
};

/* TO_ADD: helper function to create a GdaCapiPStmt such as gda_capi_pstmt_new() with some specific arguments */

G_END_DECLS

#endif
