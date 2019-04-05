/*
 * Copyright (C) 2008 - 2016 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 - 2014 Murray Cumming <murrayc@murrayc.com>
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
#include "gda-mysql.h"
#include "gda-mysql-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include <libgda/gda-debug-macros.h>


static void gda_mysql_blob_op_class_init (GdaMysqlBlobOpClass *klass);
static void gda_mysql_blob_op_init       (GdaMysqlBlobOp *blob);
static void gda_mysql_blob_op_dispose   (GObject *object);

static glong gda_mysql_blob_op_get_length (GdaBlobOp *op);
static glong gda_mysql_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_mysql_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);


typedef struct {
	GdaConnection  *cnc;
	/* TO_ADD: specific information describing a Blob in the C API */
} GdaMysqlBlobOpPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaMysqlBlobOp, gda_mysql_blob_op, GDA_TYPE_BLOB_OP)


static void
gda_mysql_blob_op_init (GdaMysqlBlobOp       *op)
{
	g_return_if_fail (GDA_IS_MYSQL_BLOB_OP (op));
  GdaMysqlBlobOpPrivate *priv = gda_mysql_blob_op_get_instance_private (op);
  priv->cnc = NULL;

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_mysql_blob_op_class_init (GdaMysqlBlobOpClass  *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	object_class->dispose = gda_mysql_blob_op_dispose;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_mysql_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_mysql_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_mysql_blob_op_write;
}

static void
gda_mysql_blob_op_dispose (GObject  *object)
{
  GdaMysqlBlobOpPrivate *priv = gda_mysql_blob_op_get_instance_private (GDA_MYSQL_BLOB_OP (object));
	if (priv->cnc != NULL) {
    g_object_unref (priv->cnc);
    priv->cnc = NULL;
  }

	G_OBJECT_CLASS (gda_mysql_blob_op_parent_class)->finalize (object);
}

GdaBlobOp *
gda_mysql_blob_op_new (GdaConnection  *cnc)
{
	GdaMysqlBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_MYSQL_BLOB_OP, "connection", cnc, NULL);

  GdaMysqlBlobOpPrivate *priv = gda_mysql_blob_op_get_instance_private (pgop);

	priv->cnc = g_object_ref (cnc);
	
	return GDA_BLOB_OP (pgop);
}

/*
 * Get length request
 */
static glong
gda_mysql_blob_op_get_length (GdaBlobOp  *op)
{
	GdaMysqlBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
  GdaMysqlBlobOpPrivate *priv = gda_mysql_blob_op_get_instance_private (pgop);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	TO_IMPLEMENT;
	return -1;
}

/*
 * Blob read request
 */
static glong
gda_mysql_blob_op_read (GdaBlobOp  *op,
			GdaBlob    *blob,
			glong       offset,
			glong       size)
{
	GdaMysqlBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
  GdaMysqlBlobOpPrivate *priv = gda_mysql_blob_op_get_instance_private (pgop);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
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
gda_mysql_blob_op_write (GdaBlobOp  *op,
			 GdaBlob    *blob,
			 G_GNUC_UNUSED glong       offset)
{
	GdaMysqlBlobOp *pgop;
	/* GdaBinary *bin; */

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
  GdaMysqlBlobOpPrivate *priv = gda_mysql_blob_op_get_instance_private (pgop);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	/* write blob using bin->data and bin->binary_length */
	/* bin = (GdaBinary *) blob; */
	TO_IMPLEMENT;

	return -1;
}
