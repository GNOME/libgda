/* GDA Capi provider
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
#include "gda-capi.h"
#include "gda-capi-blob-op.h"

struct _GdaCapiBlobOpPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information describing a Blob in the C API */
};

static void gda_capi_blob_op_class_init (GdaCapiBlobOpClass *klass);
static void gda_capi_blob_op_init       (GdaCapiBlobOp *blob,
					 GdaCapiBlobOpClass *klass);
static void gda_capi_blob_op_finalize   (GObject *object);

static glong gda_capi_blob_op_get_length (GdaBlobOp *op);
static glong gda_capi_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_capi_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_capi_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaCapiBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_capi_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaCapiBlobOp),
			0,
			(GInstanceInitFunc) gda_capi_blob_op_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaCapiBlobOp", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_capi_blob_op_init (GdaCapiBlobOp *op,
			   GdaCapiBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_CAPI_BLOB_OP (op));

	op->priv = g_new0 (GdaCapiBlobOpPrivate, 1);

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_capi_blob_op_class_init (GdaCapiBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_capi_blob_op_finalize;
	blob_class->get_length = gda_capi_blob_op_get_length;
	blob_class->read = gda_capi_blob_op_read;
	blob_class->write = gda_capi_blob_op_write;
}

static void
gda_capi_blob_op_finalize (GObject * object)
{
	GdaCapiBlobOp *pgop = (GdaCapiBlobOp *) object;

	g_return_if_fail (GDA_IS_CAPI_BLOB_OP (pgop));

	/* free specific information */
	TO_IMPLEMENT;

	g_free (pgop->priv);
	pgop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_capi_blob_op_new (GdaConnection *cnc)
{
	GdaCapiBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_CAPI_BLOB_OP, NULL);
	pgop->priv->cnc = cnc;
	
	return GDA_BLOB_OP (pgop);
}

/*
 * Get length request
 */
static glong
gda_capi_blob_op_get_length (GdaBlobOp *op)
{
	GdaCapiBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CAPI_BLOB_OP (op), -1);
	pgop = GDA_CAPI_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);

	TO_IMPLEMENT;
	return -1;
}

/*
 * Blob read request
 */
static glong
gda_capi_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaCapiBlobOp *pgop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_CAPI_BLOB_OP (op), -1);
	pgop = GDA_CAPI_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
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
gda_capi_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaCapiBlobOp *pgop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_CAPI_BLOB_OP (op), -1);
	pgop = GDA_CAPI_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	/* write blob using bin->data and bin->binary_length */
	bin = (GdaBinary *) blob;
	TO_IMPLEMENT;

	return -1;
}
