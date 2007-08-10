/* GdaDataModelDir Blob operation
 * Copyright (C) 2007 The GNOME Foundation
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

#ifndef __GDA_DIR_BLOB_OP_H__
#define __GDA_DIR_BLOB_OP_H__

#ifndef __GDA_INTERNAL__
#error Do not include this file directly
#endif

#include <libgda/gda-value.h>
#include <libgda/gda-blob-op.h>

G_BEGIN_DECLS

#define GDA_TYPE_DIR_BLOB_OP            (gda_dir_blob_op_get_type())
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

GType         gda_dir_blob_op_get_type     (void) G_GNUC_CONST;
GdaBlobOp    *gda_dir_blob_op_new          (const gchar *complete_filename);
void          gda_dir_blob_set_filename    (GdaDirBlobOp *blob, const gchar *complete_filename);
const gchar  *gda_dir_blob_get_filename    (GdaDirBlobOp *blob);

G_END_DECLS

#endif

