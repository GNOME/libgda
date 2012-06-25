/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DIR_BLOB_OP_H__
#define __GDA_DIR_BLOB_OP_H__

#ifndef __GDA_INTERNAL__
#error Do not include this file directly
#endif

#include <libgda/gda-value.h>
#include <libgda/gda-blob-op.h>

G_BEGIN_DECLS

#define GDA_TYPE_DIR_BLOB_OP            (_gda_dir_blob_op_get_type())
#define GDA_DIR_BLOB_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DIR_BLOB_OP, GdaDirBlobOp))
#define GDA_DIR_BLOB_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DIR_BLOB_OP, GdaDirBlobOpClass))
#define GDA_IS_DIR_BLOB_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DIR_BLOB_OP))
#define GDA_IS_DIR_BLOB_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_DIR_BLOB_OP))

typedef struct _GdaDirBlobOp        GdaDirBlobOp;
typedef struct _GdaDirBlobOpClass   GdaDirBlobOpClass;
typedef struct _GdaDirBlobOpPrivate GdaDirBlobOpPrivate;

struct _GdaDirBlobOp {
	GdaBlobOp                 parent;
	GdaDirBlobOpPrivate      *priv;
};

struct _GdaDirBlobOpClass {
	GdaBlobOpClass            parent_class;
};

GType         _gda_dir_blob_op_get_type     (void) G_GNUC_CONST;
GdaBlobOp    *_gda_dir_blob_op_new          (const gchar *complete_filename);
void          _gda_dir_blob_set_filename    (GdaDirBlobOp *blob, const gchar *complete_filename);
const gchar  *_gda_dir_blob_get_filename    (GdaDirBlobOp *blob);

G_END_DECLS

#endif

