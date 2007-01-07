/* GDA Common Library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * Authors:
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

#ifndef __GDA_BLOB_OP_H__
#define __GDA_BLOB_OP_H__

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-value.h>

G_BEGIN_DECLS

#define GDA_TYPE_BLOB_OP            (gda_blob_op_get_type())
#define GDA_BLOB_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_BLOB_OP, GdaBlobOp))
#define GDA_BLOB_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_BLOB_OP, GdaBlobOpClass))
#define GDA_IS_BLOB_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_BLOB_OP))
#define GDA_IS_BLOB_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_BLOB_OP))

struct _GdaBlobOp {
	GObject object;
};

struct _GdaBlobOpClass {
	GObjectClass parent_class;

	/* Virtual methods */
	glong  (* get_length) (GdaBlobOp *op);
	glong  (* read)       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
	glong  (* write)      (GdaBlobOp *op, GdaBlob *blob, glong offset);
};

GType    gda_blob_op_get_type  (void);

glong    gda_blob_op_get_length (GdaBlobOp *op);
glong    gda_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
glong    gda_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

G_END_DECLS

#endif

