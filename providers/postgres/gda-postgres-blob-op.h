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

#ifndef __GDA_POSTGRES_BLOB_OP_H__
#define __GDA_POSTGRES_BLOB_OP_H__

#include <libgda/gda-value.h>
#include <libgda/gda-blob-op.h>

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_BLOB_OP            (gda_postgres_blob_op_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaPostgresBlobOp, gda_postgres_blob_op, GDA, POSTGRES_BLOB_OP, GdaBlobOp)

struct _GdaPostgresBlobOpClass {
	GdaBlobOpClass            parent_class;
};

GdaBlobOp    *gda_postgres_blob_op_new          (GdaConnection *cnc);
GdaBlobOp    *gda_postgres_blob_op_new_with_id  (GdaConnection *cnc, const gchar *sql_id);
gboolean      gda_postgres_blob_op_declare_blob (GdaPostgresBlobOp *pgop);
gchar        *gda_postgres_blob_op_get_id       (GdaPostgresBlobOp *pgop);
void          gda_postgres_blob_op_set_id       (GdaPostgresBlobOp *pgop, const gchar *sql_id);

G_END_DECLS

#endif

