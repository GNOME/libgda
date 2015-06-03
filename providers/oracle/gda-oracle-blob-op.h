/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_ORACLE_BLOB_OP_H__
#define __GDA_ORACLE_BLOB_OP_H__

#include <libgda/gda-blob-op.h>

G_BEGIN_DECLS

#define GDA_TYPE_ORACLE_BLOB_OP            (gda_oracle_blob_op_get_type())
#define GDA_ORACLE_BLOB_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_BLOB_OP, GdaOracleBlobOp))
#define GDA_ORACLE_BLOB_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_BLOB_OP, GdaOracleBlobOpClass))
#define GDA_IS_ORACLE_BLOB_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ORACLE_BLOB_OP))
#define GDA_IS_ORACLE_BLOB_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ORACLE_BLOB_OP))

typedef struct _GdaOracleBlobOp        GdaOracleBlobOp;
typedef struct _GdaOracleBlobOpClass   GdaOracleBlobOpClass;
typedef struct _GdaOracleBlobOpPrivate GdaOracleBlobOpPrivate;

struct _GdaOracleBlobOp {
	GdaBlobOp             parent;
	GdaOracleBlobOpPrivate *priv;
};

struct _GdaOracleBlobOpClass {
	GdaBlobOpClass        parent_class;
};

GType         gda_oracle_blob_op_get_type     (void) G_GNUC_CONST;
GdaBlobOp    *gda_oracle_blob_op_new          (GdaConnection *cnc, OCILobLocator *lobloc);

G_END_DECLS

#endif

