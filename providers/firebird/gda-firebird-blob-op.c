/*
 * Copyright (C) 2007 - 2016 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <libgda/libgda.h>
#include "gda-firebird.h"
#include "gda-firebird-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include <libgda/gda-debug-macros.h>

struct _GdaFirebirdBlobOpPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information describing a Blob in the C API */
};

static void gda_firebird_blob_op_class_init (GdaFirebirdBlobOpClass *klass);
static void gda_firebird_blob_op_init       (GdaFirebirdBlobOp *blob,
					 GdaFirebirdBlobOpClass *klass);
static void gda_firebird_blob_op_finalize   (GObject *object);

static glong gda_firebird_blob_op_get_length (GdaBlobOp *op);
static glong gda_firebird_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_firebird_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_firebird_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaFirebirdBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdBlobOp),
			0,
			(GInstanceInitFunc) gda_firebird_blob_op_init
		};
		g_mutex_lock (&registering);
		if (type == 0) {
#ifdef FIREBIRD_EMBED
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaFirebirdBlobOpEmbed", &info, 0);
#else
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaFirebirdBlobOp", &info, 0);
#endif
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_firebird_blob_op_init (GdaFirebirdBlobOp *op,
			   GdaFirebirdBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op));

	op->priv = g_new0 (GdaFirebirdBlobOpPrivate, 1);

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_firebird_blob_op_class_init (GdaFirebirdBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_firebird_blob_op_finalize;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_firebird_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_firebird_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_firebird_blob_op_write;
}

static void
gda_firebird_blob_op_finalize (GObject * object)
{
	GdaFirebirdBlobOp *pgop = (GdaFirebirdBlobOp *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_BLOB_OP (pgop));

	/* free specific information */
	TO_IMPLEMENT;

	g_free (pgop->priv);
	pgop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_firebird_blob_op_new (GdaConnection *cnc)
{
	GdaFirebirdBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_FIREBIRD_BLOB_OP, "connection", cnc, NULL);
	pgop->priv->cnc = cnc;
	
	return GDA_BLOB_OP (pgop);
}

/*
 * Get length request
 */
static glong
gda_firebird_blob_op_get_length (GdaBlobOp *op)
{
	GdaFirebirdBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op), -1);
	pgop = GDA_FIREBIRD_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);

	TO_IMPLEMENT;
	return -1;
}

/*
 * Blob read request
 */
static glong
gda_firebird_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaFirebirdBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op), -1);
	pgop = GDA_FIREBIRD_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	/* fetch blob data using C API into bin->data, and set bin->binary_length */
	TO_IMPLEMENT;

	return -1;
}

/*
 * Blob write request
 */
static glong
gda_firebird_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaFirebirdBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op), -1);
	pgop = GDA_FIREBIRD_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	/* write blob using bin->data and bin->binary_length */
	TO_IMPLEMENT;

	return -1;
}
