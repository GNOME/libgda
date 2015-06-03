/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

struct _GdaMysqlBlobOpPrivate {
	GdaConnection  *cnc;
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
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaMysqlBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlBlobOp),
			0,
			(GInstanceInitFunc) gda_mysql_blob_op_init,
			NULL
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaMysqlBlobOp", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_mysql_blob_op_init (GdaMysqlBlobOp       *op,
			G_GNUC_UNUSED GdaMysqlBlobOpClass  *klass)
{
	g_return_if_fail (GDA_IS_MYSQL_BLOB_OP (op));

	op->priv = g_new0 (GdaMysqlBlobOpPrivate, 1);

	/* initialize specific structure */
	TO_IMPLEMENT;
}

static void
gda_mysql_blob_op_class_init (GdaMysqlBlobOpClass  *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mysql_blob_op_finalize;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_mysql_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_mysql_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_mysql_blob_op_write;
}

static void
gda_mysql_blob_op_finalize (GObject  *object)
{
	GdaMysqlBlobOp *pgop = (GdaMysqlBlobOp *) object;

	g_return_if_fail (GDA_IS_MYSQL_BLOB_OP (pgop));

	/* free specific information */
	TO_IMPLEMENT;

	g_free (pgop->priv);
	pgop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_mysql_blob_op_new (GdaConnection  *cnc)
{
	GdaMysqlBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_MYSQL_BLOB_OP, "connection", cnc, NULL);
	pgop->priv->cnc = cnc;
	
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
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);

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
gda_mysql_blob_op_write (GdaBlobOp  *op,
			 GdaBlob    *blob,
			 G_GNUC_UNUSED glong       offset)
{
	GdaMysqlBlobOp *pgop;
	/* GdaBinary *bin; */

	g_return_val_if_fail (GDA_IS_MYSQL_BLOB_OP (op), -1);
	pgop = GDA_MYSQL_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	/* write blob using bin->data and bin->binary_length */
	/* bin = (GdaBinary *) blob; */
	TO_IMPLEMENT;

	return -1;
}
