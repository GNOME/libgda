/* GDA Oracle provider
 * Copyright (C) 2008 The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <libgda/libgda.h>
#include "gda-oracle.h"
#include "gda-oracle-blob-op.h"

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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaOracleBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaOracleBlobOp),
			0,
			(GInstanceInitFunc) gda_oracle_blob_op_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaOracleBlobOp", &info, 0);
		g_static_mutex_unlock (&registering);
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
	blob_class->get_length = gda_oracle_blob_op_get_length;
	blob_class->read = gda_oracle_blob_op_read;
	blob_class->write = gda_oracle_blob_op_write;
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
gda_oracle_blob_op_new (GdaConnection *cnc)
{
	GdaOracleBlobOp *bop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	bop = g_object_new (GDA_TYPE_ORACLE_BLOB_OP, NULL);
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
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	bop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	bin = (GdaBinary *) blob;
	if (bin->data) 
		g_free (bin->data);
	bin->data = g_new0 (guchar, size);
	bin->binary_length = 0;

	/* fetch blob data using C API into bin->data, and set bin->binary_length */
	TO_IMPLEMENT;

	return bin->binary_length;
}

/*
 * Blob write request
 */
static glong
gda_oracle_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaOracleBlobOp *bop;
	GdaBinary *bin;
	glong nbwritten = -1;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	bop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	if (blob->op && (blob->op != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = g_new0 (GdaBlob, 1);
		tmpblob->op = blob->op;

		nbwritten = 0;

		for (nread = gda_blob_op_read (tmpblob->op, tmpblob, nbwritten, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (tmpblob->op, tmpblob, nbwritten, buf_size)) {
			glong tmp_written;

			tmp_written = -1; TO_IMPLEMENT;
			
			if (tmp_written < 0) {
				/* treat error */
				gda_blob_free ((gpointer) tmpblob);
				return -1;
			}
			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}
		gda_blob_free ((gpointer) tmpblob);
	}
	else {
		/* write blob using bin->data and bin->binary_length */
		bin = (GdaBinary *) blob;
		nbwritten = -1; TO_IMPLEMENT;
	}

	return nbwritten;
}
