/*
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc-desktop>
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

#ifndef __GDA_BLOB_OP_IMPL_H__
#define __GDA_BLOB_OP_IMPL_H__

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-value.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS


/**
 * GdaBlobOpFunctions:
 */
typedef struct {
	/* Virtual methods */
	glong    (* get_length) (GdaBlobOp *op);
	glong    (* read)       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
	glong    (* write)      (GdaBlobOp *op, GdaBlob *blob, glong offset);
	gboolean (* write_all)  (GdaBlobOp *op, GdaBlob *blob);
} GdaBlobOpFunctions;

#define GDA_BLOB_OP_FUNCTIONS(x) ((GdaBlobOpFunctions*)(x))

G_END_DECLS

#endif

