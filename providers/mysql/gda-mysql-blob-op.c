/* GDA Mysql provider
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
#include "gda-mysql.h"
#include "gda-mysql-blob-op.h"

struct _GdaMysqlBlobOpPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information describing a Blob in the C API */
};

static void gda_mysql_blob_op_class_init (GdaMysqlBlobOpClass *klass);
static void gda_mysql_blob_op_init       (GdaMysqlBlobOp *blob,
					 GdaMysqlBlobOpClass *klass);
static void gda_mysql_blob_op_finalize   (GObject *object);

static glong gda_mysql_blob_op_get_length (GdaBlobOp *op);
static glong gda_mysql_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_mysql_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_mysql_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaMysqlBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlBlobOp),
			0,
			(GInstanceInitFunc) gda_mysql_blob_op_init
		};
		type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaMysqlBlobOp", &info, 0);
	}
	return type;
}

static void
gda_mysql_blob_op_init (GdaMysqlBlobOp *op,
			   GdaMysqlBlobOpClass *klass)
{
	g_print ("*** %s\n", __func__);
	g_return_if_fail (GDA_IS_MYSQL_BLOB_OP (op));

	op->priv = g_new0 (GdaMysqlBlobOpPrivate, 1);

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_mysql_blob_op_class_init (GdaMysqlBlobOpClass *klass)
{
	g_print ("*** %s\n", __func__);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mysql_blob_op_finalize;
	blob_class->get_length = gda_mysql_blob_op_get_length;
	blob_class->read = gda_mysql_blob_op_read;
	blob_class->write = gda_mysql_blob_op_write;
}

static void
gda_mysql_blob_op_finalize (GObject * object)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlBlobOp *pgop = (GdaMysqlBlobOp *) object;

	g_return_if_fail (GDA_IS_MYSQL_BLOB_OP (pgop));

	/* free specific information */
	TO_IMPLEMENT;

	g_free (pgop->priv);
	pgop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_mysql_blob_op_new (GdaConnection *cnc)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_MYSQL_BLOB_OP, NULL);
	pgop->priv->cnc = cnc;
	
	return GDA_BLOB_OP (pgop);
}

/*
 * Get length request
 */
static glong
gda_mysql_blob_op_get_length (GdaBlobOp *op)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);

	TO_IMPLEMENT;
	return -1;
}

/*
 * Blob read request
 */
static glong
gda_mysql_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlBlobOp *pgop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	bin = (GdaBinary *) blob;
	if (bin->data) 
		g_free (bin->data);
	bin->data = g_new0 (gchar, size);
	bin->binary_length = 0;

	/* fetch blob data using C API into bin->data, and set bin->binary_length */
	TO_IMPLEMENT;

	return bin->binary_length;
}

/*
 * Blob write request
 */
static glong
gda_mysql_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlBlobOp *pgop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	/* write blob using bin->data and bin->binary_length */
	bin = (GdaBinary *) blob;
	TO_IMPLEMENT;

	return -1;
}
