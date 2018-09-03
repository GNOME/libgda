/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <string.h>
#define __GDA_INTERNAL__
#include "dir-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>

typedef struct  {
	gchar *complete_filename;
} GdaDirBlobOpPrivate;
#define gda_dir_blob_op_get_instance_private(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, GDA_TYPE_DIR_BLOB_OP, GdaDirBlobOpPrivate)

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
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaDirBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dir_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaDirBlobOp),
			0,
			(GInstanceInitFunc) gda_dir_blob_op_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaDirBlobOp", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_dir_blob_op_init (GdaDirBlobOp *op,
		      G_GNUC_UNUSED GdaDirBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_DIR_BLOB_OP (op));
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (op);

	priv->complete_filename = NULL;
}

static void
gda_dir_blob_op_class_init (GdaDirBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);
	g_type_class_add_private (object_class, sizeof (GdaDirBlobOpPrivate));

	parent_class = g_type_class_peek_parent (klass);

	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_dir_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_dir_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_dir_blob_op_write;
}

/**
 * _gda_dir_blob_op_new
 */
GdaBlobOp *
_gda_dir_blob_op_new (const gchar *complete_filename)
{
	GdaDirBlobOp *pgop;

	g_return_val_if_fail (complete_filename, NULL);

	pgop = g_object_new (GDA_TYPE_DIR_BLOB_OP, NULL);
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (pgop);
	priv->complete_filename = g_strdup (complete_filename);
	
	return GDA_BLOB_OP (pgop);
}

/**
 * _gda_dir_blob_set_filename
 */
void
_gda_dir_blob_set_filename (GdaDirBlobOp *blob, const gchar *complete_filename)
{
	g_return_if_fail (GDA_IS_DIR_BLOB_OP (blob));
	g_return_if_fail (complete_filename);
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (blob);

	g_free (priv->complete_filename);
	priv->complete_filename = g_strdup (complete_filename);
}

/**
 * _gda_dir_blob_get_filename
 */
const gchar *
_gda_dir_blob_get_filename (GdaDirBlobOp *blob)
{
	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (blob), NULL);
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (blob);

	return priv->complete_filename;
}

/*
 * Virtual functions
 */
static glong
gda_dir_blob_op_get_length (GdaBlobOp *op)
{
	GdaDirBlobOp *dirop;
	struct stat filestat;

	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (op), -1);
	dirop = GDA_DIR_BLOB_OP (op);
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (dirop);

	if (! g_stat (priv->complete_filename, &filestat))
		return filestat.st_size;
	return -1;
}

static glong
gda_dir_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaDirBlobOp *dirop;
	GdaBinary *bin;
	FILE *file;
	size_t nread;
	guchar *buffer;

	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (op), -1);
	dirop = GDA_DIR_BLOB_OP (op);
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (dirop);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	/* open file */
	file = fopen (priv->complete_filename, "rb"); /* Flawfinder: ignore */
	if (!file)
		return -1;
	
	/* go to offset */
	if (fseek (file, offset, SEEK_SET) != 0) {
		fclose (file);
		return -1;
	}
	
	bin = gda_blob_get_binary (blob);
	gda_binary_reset_data (bin);
	buffer = g_new0 (guchar, size);
	nread = fread ((char *) (buffer), 1, size, file);
	gda_binary_take_data (bin, buffer, nread);
	fclose (file);

	return nread;
}

static glong
gda_dir_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaDirBlobOp *dirop;
	GdaBinary *bin;
	FILE *file;
	glong nbwritten;

	g_return_val_if_fail (GDA_IS_DIR_BLOB_OP (op), -1);
	dirop = GDA_DIR_BLOB_OP (op);
	GdaDirBlobOpPrivate *priv = gda_dir_blob_op_get_instance_private (blob);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	/* open file */
	file = fopen (priv->complete_filename, "w+b"); /* Flawfinder: ignore */
	if (!file)
		return -1;
	
	/* go to offset */
	if (offset > 0) {
		if (fseek (file, offset, SEEK_SET) != 0) {
			fclose (file);
			return -1;
		}
	}

	if (gda_blob_get_op (blob) && (gda_blob_get_op (blob) != op)) {
		/* use data through blob->op */
#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = gda_blob_new ();
		gda_blob_set_op (tmpblob, gda_blob_get_op (blob));

		nbwritten = 0;

		for (nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, 0, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, nbwritten, buf_size)) {
			GdaBinary *bin = gda_blob_get_binary (tmpblob);
			glong tmp_written;
			tmp_written = fwrite ((char *) gda_binary_get_data (bin), sizeof (guchar), gda_binary_get_size (bin), file);
			if (tmp_written < gda_binary_get_size (bin)) {
				/* error writing stream */
				fclose (file);
				gda_blob_free (tmpblob);
				return -1;
			}
			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}

		fclose (file);
		gda_blob_free (tmpblob);
	}
	else {
		bin = (GdaBinary *) blob;
		nbwritten = fwrite ((char *) (gda_binary_get_data (bin)), 1, gda_binary_get_size (bin), file);
		fclose (file);
	}

	return (nbwritten >= 0) ? nbwritten : -1;
}
