/* GDA Common Library
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * Authors:
 *	 Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
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

#include <libgda/gda-blob.h>

/**
 * gda_blob_open
 */
gint 
gda_blob_open (gpointer connection, GdaBlob *blob, GdaBlobMode mode)
{
	g_return_val_if_fail (connection != NULL, -1);
	g_return_val_if_fail (blob->open != NULL, -1);

	return blob->open (connection, blob, mode);
}

/**
 * gda_blob_read
 */
gint 
gda_blob_read (gpointer connection, GdaBlob *blob, gint size, 
	       gpointer data, gint *read_length)
{
	g_return_val_if_fail (connection != NULL, -1);
	g_return_val_if_fail (blob->read != NULL, -1);

	return blob->read (connection, blob, size, data, read_length);
}

/**
 * gda_blob_write
 */
gint 
gda_blob_write (gpointer connection, GdaBlob *blob, gint size, 
		gconstpointer data, gint *written_length)
{
	g_return_val_if_fail (connection != NULL, -1);
	g_return_val_if_fail (blob->write != NULL, -1);

	return blob->write (connection, blob, size, data, written_length);
}

/**
 * gda_blob_lseek
 */
gint 
gda_blob_lseek (gpointer connection, GdaBlob *blob, gint offset, gint whence)
{
	g_return_val_if_fail (connection != NULL, -1);
	g_return_val_if_fail (blob->lseek != NULL, -1);

	return blob->lseek (connection, blob, offset, whence);
}

/**
 * gda_blob_close
 */
gint 
gda_blob_close (gpointer connection, GdaBlob *blob)
{
	g_return_val_if_fail (connection != NULL, -1);
	g_return_val_if_fail (blob->close != NULL, -1);

	return blob->close (connection, blob);
}

