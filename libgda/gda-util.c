/* GDA common library
 * Copyright (C) 1999-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-util.h>
#include <libgnomevfs/gnome-vfs.h>

/**
 * gda_file_load
 * @uri: URI for the file to be loaded.
 */
gchar *
gda_file_load (const gchar *uri)
{
}

/**
 * gda_file_save
 * @uri: URI for the file to be saved.
 * @buffer: contents of the file.
 * @len: size of @buffer.
 *
 * Saves a chunk of data into a file. As libgda uses GnomeVFS for
 * all its file access, this function allows you to save both
 * to local files and to any other target supported by GnomeVFS.
 *
 * Returns: TRUE if successful, FALSE on error.
 */
gboolean
gda_file_save (const gchar *uri, const gchar *buffer, gint len)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult result;
	GnomeVFSFileSize size;

	g_return_val_if_fail (uri != NULL, FALSE);

	/* open the file */
	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_WRITE);
	if (result != GNOME_VFS_OK)
		return FALSE;

	result = gnome_vfs_write (handle, (gconstpointer) buffer, len, &size);
	gnome_vfs_close (handle);

	return result == GNOME_VFS_OK ? TRUE : FALSE;
}
