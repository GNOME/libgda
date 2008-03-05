/* GdaDataModelDir Blob operation
 * Copyright (C) 2007 The GNOME Foundation
 *
 * AUTHORS:
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#define __GDA_INTERNAL__
#include "dir-blob-op.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>

struct _GdaDirBlobOpPrivate {
	gchar *complete_filename;
};

static void gda_dir_blob_op_class_init (GdaDirBlobOpClass *klass);
static void gda_dir_blob_op_init       (GdaDirBlobOp *blob,
					  GdaDirBlobOpClass *klass);
static void gda_dir_blob_op_finalize   (GObject *object);

static glong gda_dir_blob_op_get_length (GdaBlobOp *op);
static glong gda_dir_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_dir_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_dir_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDirBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dir_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaDirBlobOp),
			0,
			(GInstanceInitFunc) gda_dir_blob_op_init
		};
		type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaDirBlobOp", &info, 0);
	}
	return type;
}

static void
gda_dir_blob_op_init (GdaDirBlobOp *op,
			   GdaDirBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_DIR_BLOB_OP (op));

	op->priv = g_new0 (GdaDirBlobOpPrivate, 1);
	op->priv->complete_filename = NULL;
}

static void
gda_dir_blob_op_class_init (GdaDirBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_dir_blob_op_finalize;
	blob_class->get_length = gda_dir_blob_op_get_length;
	blob_class->read = gda_dir_blob_op_read;
	blob_class->write = gda_dir_blob_op_write;
}


static void
gda_dir_blob_op_finalize (GObject * object)
{
	GdaDirBlobOp *pgop = (GdaDirBlobOp *) object;

	g_return_if_fail (GDA_IS_DIR_BLOB_OP (pgop));

	g_free (pgop->priv->complete_filename);
	g_free (pgop->priv);
	pgop->priv = NULL;

	parent_class->finalize (object);
}

/**
 * gda_dir_blob_op_new
 */
GdaBlobOp *
gda_dir_blob_op_new (const gchar *complete_filename)
{
	GdaDirBlobOp *pgop;

	g_return_val_if_fail (complete_filename, NULL);

	pgop = g_object_new (GDA_TYPE_DIR_BLOB_OP, NULL);
	pgop->priv->complete_filename = g_strdup (complete_filename);
	
	return GDA_BLOB_OP (pgop);
}

/**
 * gda_dir_blob_set_filename
 */
void
gda_dir_blob_set_filename (GdaDirBlobOp *blob, const gchar *complete_filename)
{
	g_return_if_fail (GDA_IS_DIR_BLOB_OP (blob));
	g_return_if_fail (blob->priv);
	g_return_if_fail (complete_filename);

	g_free (blob->priv->complete_filename);
	blob->priv->complete_filename = g_strdup (complete_filename);
}

/**
 * gda_dir_blob_get_filename
 */
const gchar *
gda_dir_blob_get_filename (GdaDirBlobOp *blob)
{
	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (blob), NULL);
	g_return_val_if_fail (blob->priv, NULL);

	return blob->priv->complete_filename;
}

/*
 * Virtual functions
 */
static glong
gda_dir_blob_op_get_length (GdaBlobOp *op)
{
	GdaDirBlobOp *pgop;
	struct stat filestat;

	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (op), -1);
	pgop = GDA_DIR_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);

	if (! g_stat (pgop->priv->complete_filename, &filestat)) 
		return filestat.st_size;
	else
		return -1;
}

static glong
gda_dir_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaDirBlobOp *pgop;
	GdaBinary *bin;
	FILE *file;
	size_t nread;

	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (op), -1);
	pgop = GDA_DIR_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	/* open file */
	file = fopen (pgop->priv->complete_filename, "r");
	if (!file)
		return -1;
	
	/* go to offset */
	if (offset > 0) {
		if (fseek (file, offset, SEEK_SET) != 0) {
			fclose (file);
			return -1;
		}
	}
	
	bin = (GdaBinary *) blob;
	if (bin->data) {
		g_free (bin->data);
		bin->data = NULL;
	}
	bin->data = g_new0 (guchar, size);
	nread = fread ((char *) (bin->data), 1, size, file);
	bin->binary_length = (nread >= 0) ? nread : 0;
	fclose (file);

	return nread;
}

static glong
gda_dir_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaDirBlobOp *pgop;
	GdaBinary *bin;
	FILE *file;
	glong nbwritten;

	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (op), -1);
	pgop = GDA_DIR_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	/* open file */
	file = fopen (pgop->priv->complete_filename, "w+");
	if (!file)
		return -1;
	
	/* go to offset */
	if (offset > 0) {
		if (fseek (file, offset, SEEK_SET) != 0) {
			fclose (file);
			return -1;
		}
	}
	
	bin = (GdaBinary *) blob;
	nbwritten = fwrite ((char *) (bin->data), 1, bin->binary_length, file);
	fclose (file);

	return (nbwritten >= 0) ? nbwritten : -1;
}
