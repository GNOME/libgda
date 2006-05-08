/* GDA Common Library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * Authors:
 *	 Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
 *	 Gonzalo Paniagua Javier (gonzalo@gnome-db.org)
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
 * BLOB (Binary Large OBject) handling functions.
 */

#include "gda-blob.h"
#include "gda-value.h"

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(blob) (GDA_BLOB_CLASS (G_OBJECT_GET_CLASS (blob)))
static void gda_blob_class_init (GdaBlobClass *klass);
static void gda_blob_init       (GdaBlob *provider, GdaBlobClass *klass);
static void gda_blob_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

static void 
blob_to_string (const GValue *src, GValue *dest) 
{
	GdaBlob *gdablob;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BLOB (src));
	
	gdablob = (GdaBlob *) gda_value_get_blob ((GValue *) src);
	
	g_value_set_string (dest, gda_blob_get_sql_id (gdablob));
}

GType
gda_blob_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
                        sizeof (GdaBlobClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_blob_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaBlob),
                        0,
                        (GInstanceInitFunc) gda_blob_init
                };

                type = g_type_register_static (PARENT_TYPE, "GdaBlob", &info, G_TYPE_FLAG_ABSTRACT);

		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 blob_to_string);

        }
        return type;
}

static void
gda_blob_class_init (GdaBlobClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = gda_blob_finalize;
        klass->open = NULL;
        klass->read = NULL;
        klass->write = NULL;
        klass->lseek = NULL;
        klass->close = NULL;
        klass->remove = NULL;
        klass->get_sql_id = NULL;
}

static void
gda_blob_init (GdaBlob *provider, GdaBlobClass *klass)
{

}

static void
gda_blob_finalize (GObject *object)
{
	/* chain to parent class */
        parent_class->finalize (object);
}


/**
 * gda_blob_open
 * @blob: an existing #GdaBlob
 * @mode: see #GdaBlobMode.
 *
 * Opens an existing BLOB. The BLOB must be initialized by
 * #gda_connection_create_blob or obtained from a #GValue.
 * 
 * Returns: 0 if everything's ok. In case of error, -1 is returned and the
 * provider should have added an error (a #GdaConnectionEvent) to the connection.
 */
gint 
gda_blob_open (GdaBlob *blob, GdaBlobMode mode)
{
	g_return_val_if_fail (blob && GDA_IS_BLOB (blob), -1);

	if (CLASS (blob)->open != NULL)
		return CLASS (blob)->open (blob, mode);
	else
		return -1;
}

/**
 * gda_blob_read
 * @blob: a #GdaBlob which is opened with the flag GDA_BLOB_MODE_READ set.
 * @buf: buffer to read the data into.
 * @size: maximum number of bytes to read.
 * @bytes_read: on return it will point to the number of bytes actually read.
 *
 * Reads a chunk of bytes from the BLOB into a user-provided location.
 *
 * Returns: 0 if everything's ok. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
gint
gda_blob_read (GdaBlob *blob, gpointer buf, gint size, gint *bytes_read)
{
	g_return_val_if_fail (blob && GDA_IS_BLOB (blob), -1);

	if (CLASS (blob)->read != NULL)
		return CLASS (blob)->read (blob, buf, size, bytes_read);
	else
		return -1;
}

/**
 * gda_blob_write
 * @blob: a #GdaBlob which is opened with the flag GDA_BLOB_MODE_WRITE set.
 * @buf: buffer to write the data from.
 * @size: maximum number of bytes to read.
 * @bytes_written: on return it will point to the number of bytes actually written.
 *
 * Writes a chunk of bytes from a user-provided location to the BLOB.
 *
 * Returns: 0 if everything's ok. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
gint
gda_blob_write (GdaBlob *blob, gpointer buf, gint size, gint *bytes_written)
{
	g_return_val_if_fail (blob && GDA_IS_BLOB (blob), -1);

	if (CLASS (blob)->write != NULL)
		return CLASS (blob)->write (blob, buf, size, bytes_written);
	else
		return -1;
}

/**
 * gda_blob_lseek
 * @blob: a opened #GdaBlob.
 * @offset: offset added to the position specified by @whence.
 * @whence: SEEK_SET, SEEK_CUR or SEEK_END with the same meaning as in fseek(3).
 *
 * Sets the blob read/write position.
 *
 * Returns: the current position in the blob or < 0 in case of error. In case
 * of error the provider should have added an error to the connection.
 */
gint 
gda_blob_lseek (GdaBlob *blob, gint offset, gint whence)
{
	g_return_val_if_fail (blob && GDA_IS_BLOB (blob), -1);

	if (CLASS (blob)->lseek != NULL)
		return CLASS (blob)->lseek (blob, offset, whence);
	else
		return -1;
}

/**
 * gda_blob_close
 * @blob: a opened #GdaBlob.
 *
 * Closes the BLOB. After calling this function, @blob should no longer be used.
 *
 * Returns: 0 if everything's ok. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
gint 
gda_blob_close (GdaBlob *blob)
{
	g_return_val_if_fail (blob && GDA_IS_BLOB (blob), -1);

	if (CLASS (blob)->close != NULL)
		return CLASS (blob)->close (blob);
	else
		return -1;
}

/**
 * gda_blob_remove
 * @blob: a valid #GdaBlob.
 *
 * Removes the BLOB from the database. After calling this function, @blob
 * should no longer be used.
 *
 * Returns: 0 if everything's ok. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
gint 
gda_blob_remove (GdaBlob *blob)
{
	g_return_val_if_fail (blob && GDA_IS_BLOB (blob), -1);

	if (CLASS (blob)->remove != NULL)
		return CLASS (blob)->remove (blob);
	else
		return -1;
}

/**
 * gda_blob_get_sql_id
 * @blob: a valid #GdaBlob.
 *
 * Returns a string representation of a @blob's SQL ID.
 */
gchar*
gda_blob_get_sql_id (GdaBlob *blob) 
{
	g_return_val_if_fail(blob && GDA_IS_BLOB (blob), NULL);

	if (CLASS (blob)->get_sql_id != NULL)
		return CLASS (blob)->get_sql_id (blob);
	else
		return NULL;
}
