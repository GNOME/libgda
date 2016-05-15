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
#include "gda-oracle.h"
#include "gda-oracle-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include <libgda/gda-debug-macros.h>

struct _GdaOracleBlobOpPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information describing a Blob in the C API */
};

static void gda_oracle_blob_op_class_init (GdaOracleBlobOpClass *klass);
static void gda_oracle_blob_op_init       (GdaOracleBlobOp *blob,
					 GdaOracleBlobOpClass *klass);
static void gda_oracle_blob_op_finalize   (GObject *object);

static glong gda_oracle_blob_op_get_length (GdaBlobOp *op);
static glong gda_oracle_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_oracle_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_oracle_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaOracleBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaOracleBlobOp),
			0,
			(GInstanceInitFunc) gda_oracle_blob_op_init,
			NULL
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaOracleBlobOp", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_oracle_blob_op_init (GdaOracleBlobOp *op,
			   GdaOracleBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_ORACLE_BLOB_OP (op));

	op->priv = g_new0 (GdaOracleBlobOpPrivate, 1);

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_oracle_blob_op_class_init (GdaOracleBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_oracle_blob_op_finalize;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_oracle_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_oracle_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_oracle_blob_op_write;
}

static void
gda_oracle_blob_op_finalize (GObject * object)
{
	GdaOracleBlobOp *bop = (GdaOracleBlobOp *) object;

	g_return_if_fail (GDA_IS_ORACLE_BLOB_OP (bop));

	/* free specific information */
	TO_IMPLEMENT;

	g_free (bop->priv);
	bop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_oracle_blob_op_new (GdaConnection *cnc, OCILobLocator *lobloc)
{
	GdaOracleBlobOp *bop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	bop = g_object_new (GDA_TYPE_ORACLE_BLOB_OP, "connection", cnc, NULL);
	bop->priv->cnc = cnc;
	
	return GDA_BLOB_OP (bop);
}

/*
 * Get length request
 */
static glong
gda_oracle_blob_op_get_length (GdaBlobOp *op)
{
	GdaOracleBlobOp *bop;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	bop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);

	TO_IMPLEMENT;
	return -1;
}

/*
 * Blob read request
 */
static glong
gda_oracle_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaOracleBlobOp *bop;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	bop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);
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
gda_oracle_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaOracleBlobOp *bop;
	glong nbwritten = -1;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	bop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	GdaBlobOp *blob_op;
	blob_op = gda_blob_get_op (blob);
	if (blob_op && (blob_op != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = gda_blob_new ();
		gda_blob_set_op (tmpblob, blob_op);

		nbwritten = 0;

		for (nread = gda_blob_op_read (blob_op, tmpblob, nbwritten, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (blob_op, tmpblob, nbwritten, buf_size)) {
			glong tmp_written;

			tmp_written = -1; TO_IMPLEMENT;
			
			if (tmp_written < 0) {
				/* treat error */
				gda_blob_free (tmpblob);
				return -1;
			}
			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}
		gda_blob_free (tmpblob);
	}
	else {
		/* write blob using bin->data and bin->binary_length */
		GdaBinary *bin;
		bin = (GdaBinary *) blob;
		nbwritten = -1; TO_IMPLEMENT;
	}

	return nbwritten;
}
