/* GDA Common Library
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * Authors:
 *	 Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
 *	 Gonzalo Paniagua Javier (gonzalo@gnome-db.org)
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

/**
 * gda_blob_open
 * @blob: a #GdaBlob structure obtained from a #GdaValue or allocated by the
 * user when he/she wants to create a new #GdaBlob.
 * @mode: see #GdaBlobMode.
 *
 * Opens an existing BLOB. The BLOB must be initialized by
 * #gda_connection_create_blob or obtained from a #GdaValue.
 * 
 * Returns: 0 if everything's ok. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
gint 
gda_blob_open (GdaBlob *blob, GdaBlobMode mode)
{
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->open != NULL, -1);

	return blob->open (blob, mode);
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
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->read != NULL, -1);

	return blob->read (blob, buf, size, bytes_read);
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
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->write != NULL, -1);

	return blob->write (blob, buf, size, bytes_written);
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
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->lseek != NULL, -1);

	return blob->lseek (blob, offset, whence);
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
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->close != NULL, -1);

	return blob->close (blob);
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
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->remove != NULL, -1);

	return blob->remove (blob);
}

/**
 * gda_blob_free_data
 * @blob: a valid #GdaBlob.
 *
 * Let the provider free any internal data stored in @blob. The user
 * is still responsible for deallocating @blob itself.
 *
 */
void 
gda_blob_free_data (GdaBlob *blob)
{
	g_return_val_if_fail (blob != NULL, -1);
	g_return_if_fail (blob->free_data != NULL);

	return blob->free_data (blob);
}

