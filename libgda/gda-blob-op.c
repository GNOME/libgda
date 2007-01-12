/* GDA Common Library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * Authors:
 *       Vivien Malerba <malerba@gnome-db.org>
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

/**
 * BLOB (Binary Large OBject) handling functions specific to each provider.
 */

#include "gda-blob-op.h"
#include "gda-value.h"

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(blob) (GDA_BLOB_OP_CLASS (G_OBJECT_GET_CLASS (blob)))
static void gda_blob_op_class_init (GdaBlobOpClass *klass);
static void gda_blob_op_init       (GdaBlobOp *provider, GdaBlobOpClass *klass);
static void gda_blob_op_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

GType
gda_blob_op_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
                        sizeof (GdaBlobOpClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_blob_op_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaBlobOp),
                        0,
                        (GInstanceInitFunc) gda_blob_op_init
                };

                type = g_type_register_static (PARENT_TYPE, "GdaBlobOp", &info, G_TYPE_FLAG_ABSTRACT);
        }
        return type;
}

static void
gda_blob_op_class_init (GdaBlobOpClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = gda_blob_op_finalize;
        klass->get_length = NULL;
        klass->read = NULL;
        klass->write = NULL;
}

static void
gda_blob_op_init (GdaBlobOp *provider, GdaBlobOpClass *klass)
{

}

static void
gda_blob_op_finalize (GObject *object)
{
	/* chain to parent class */
        parent_class->finalize (object);
}


/**
 * gda_blob_op_get_length
 * @op: an existing #GdaBlobOp
 *
 * Opens an existing BLOB. The BLOB must be initialized by
 * #gda_connection_create_blob or obtained from a #GValue.
 * FIXME: gda_connection_create_blob() no longer exists.
 * 
 * Returns: the length of the blob in bytes. In case of error, -1 is returned and the
 * provider should have added an error (a #GdaConnectionEvent) to the connection.
 */
glong
gda_blob_op_get_length (GdaBlobOp *op)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), -1);

	if (CLASS (op)->get_length != NULL)
		return CLASS (op)->get_length (op);
	else
		return -1;
}

/**
 * gda_blob_op_read
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob to read data to
 * @offset: offset to read from the start of the blob (starts at 0)
 * @size: maximum number of bytes to read.
 *
 * Reads a chunk of bytes from the BLOB into @blob.
 *
 * Returns: the number of bytes actually read. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
glong
gda_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), -1);

	if (CLASS (op)->read != NULL)
		return CLASS (op)->read (op, blob, offset, size);
	else
		return -1;
}

/**
 * gda_blob_op_read_all
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob to read data to
 *
 * Reads the whole contents of the blob manipulated by @op into @blob
 */
void
gda_blob_op_read_all (GdaBlobOp *op, GdaBlob *blob)
{
	glong len;
	g_return_if_fail (GDA_IS_BLOB_OP (op));
	g_return_if_fail (blob);

	len = gda_blob_op_get_length (blob->op);
	if (len != ((GdaBinary *)blob)->binary_length)
		gda_blob_op_read (blob->op, blob, 0, len);
}

/**
 * gda_blob_op_write
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob which contains the data to write
 * @offset: offset to write from the start of the blob (starts at 0)
 *
 * Writes a chunk of bytes from a @blob to the BLOB accessible through @op.
 *
 * Returns: the number of bytes written. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
glong
gda_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), -1);

	if (CLASS (op)->write != NULL)
		return CLASS (op)->write (op, blob, offset);
	else
		return -1;
}
