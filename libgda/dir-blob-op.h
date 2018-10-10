/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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


/* error reporting */
extern GQuark gda_dir_blob_op_error_quark (void);
#define GDA_DIR_BLOB_OP_ERROR gda_dir_blob_op_error_quark ()

typedef enum {
	GDA_DIR_BLOB_OP_GENERAL_ERROR
} GdaDirBlobOpError;


#define GDA_TYPE_DIR_BLOB_OP            (gda_dir_blob_op_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaDirBlobOp, gda_dir_blob_op, GDA, DIR_BLOB_OP, GdaBlobOp)

struct _GdaDirBlobOpClass {
	GdaBlobOpClass            parent_class;
};

GdaBlobOp    *_gda_dir_blob_op_new          (const gchar *complete_filename);
void          _gda_dir_blob_set_filename    (GdaDirBlobOp *blob, const gchar *complete_filename);
const gchar  *_gda_dir_blob_get_filename    (GdaDirBlobOp *blob);

G_END_DECLS

#endif

