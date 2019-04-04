/*
 * Copyright (C) YEAR The GNOME Foundation
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

#ifndef __GDA_CAPI_BLOB_OP_H__
#define __GDA_CAPI_BLOB_OP_H__

#include <libgda/gda-blob-op.h>

G_BEGIN_DECLS

#define GDA_TYPE_CAPI_BLOB_OP            (gda_capi_blob_op_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaCapiBlobOp, gda_capi_blob_op, GDA, CAPI_BLOB_OP, GdaBlobOp)
struct _GdaCapiBlobOpClass {
	GdaBlobOpClass        parent_class;
};

GdaBlobOp    *gda_capi_blob_op_new          (GdaConnection *cnc);

/* TO_ADD: more convenient API to create a GdaBlobOp with some specific information as argument */

G_END_DECLS

#endif

