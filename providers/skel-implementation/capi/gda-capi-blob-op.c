/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <libgda/libgda.h>
#include "gda-capi.h"
#include "gda-capi-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include <libgda/gda-debug-macros.h>


static void gda_capi_blob_op_class_init (GdaCapiBlobOpClass *klass);
static void gda_capi_blob_op_init       (GdaCapiBlobOp *blob);
static void gda_capi_blob_op_dispose   (GObject *object);

static glong gda_capi_blob_op_get_length (GdaBlobOp *op);
static glong gda_capi_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_capi_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);


typedef struct {
	GdaConnection *cnc;
	/* TO_ADD: specific information describing a Blob in the C API */
} GdaCapiBlobOpPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaCapiBlobOp, gda_capi_blob_op, GDA_TYPE_BLOB_OP)

static void
gda_capi_blob_op_init (GdaCapiBlobOp *op)
{
	g_return_if_fail (GDA_IS_CAPI_BLOB_OP (op));

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_capi_blob_op_class_init (GdaCapiBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);


	object_class->dispose = gda_capi_blob_op_dispose;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_capi_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_capi_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_capi_blob_op_write;
}

static void
gda_capi_blob_op_dispose (GObject * object)
{
	/* free specific information */
	TO_IMPLEMENT;
	G_OBJECT_CLASS (gda_capi_blob_op_parent_class)->dispose (object);
}

GdaBlobOp *
gda_capi_blob_op_new (GdaConnection *cnc)
{
	GdaCapiBlobOp *bop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	bop = g_object_new (GDA_TYPE_CAPI_BLOB_OP, "connection", cnc, NULL);

  GdaCapiBlobOpPrivate *priv = gda_capi_blob_op_get_instance_private (bop);

	priv->cnc = cnc;
	
	return GDA_BLOB_OP (bop);
}

/*
 * Get length request
 */
static glong
gda_capi_blob_op_get_length (GdaBlobOp *op)
{
	GdaCapiBlobOp *bop;

	g_return_val_if_fail (GDA_IS_CAPI_BLOB_OP (op), -1);
	bop = GDA_CAPI_BLOB_OP (op);
  GdaCapiBlobOpPrivate *priv = gda_capi_blob_op_get_instance_private (bop);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	TO_IMPLEMENT;
	return -1;
}

/*
 * Blob read request
 */
static glong
gda_capi_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaCapiBlobOp *bop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_CAPI_BLOB_OP (op), -1);
	bop = GDA_CAPI_BLOB_OP (op);
  GdaCapiBlobOpPrivate *priv = gda_capi_blob_op_get_instance_private (bop);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	bin = gda_blob_get_binary (blob);
	if (gda_binary_get_data (bin))
	  gda_binary_reset_data (bin); 
		g_free (gda_binary_get_data (bin));
	guchar* buffer = g_new0 (guchar, size);
	gda_binary_set_data (bin, buffer, size);
	g_free (buffer);

	/* fetch blob data using C API into bin->data, and set bin->binary_length */
	TO_IMPLEMENT;

	return gda_binary_get_size (bin);
}

/*
 * Blob write request
 */
static glong
gda_capi_blob_op_write (GdaBlobOp *op, GdaBlob *blob, G_GNUC_UNUSED glong offset)
{
	GdaCapiBlobOp *bop;
	GdaBinary *bin = NULL;
	glong nbwritten = -1;

	g_return_val_if_fail (GDA_IS_CAPI_BLOB_OP (op), -1);
	bop = GDA_CAPI_BLOB_OP (op);
  GdaCapiBlobOpPrivate *priv = gda_capi_blob_op_get_instance_private (bop);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	if (gda_blob_get_op (blob) && (gda_blob_get_op (blob) != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = gda_blob_new ();
		gda_blob_set_op (tmpblob, gda_blob_get_op (blob));

		nbwritten = 0;

		for (nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, nbwritten, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, nbwritten, buf_size)) {
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
		bin = gda_blob_get_binary (blob);
    		g_warning("bin not used. length=%ld", gda_binary_get_size (bin)); /* Avoids a compiler warning. */        
		nbwritten = -1; TO_IMPLEMENT;
	}

	return nbwritten;
}
