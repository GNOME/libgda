/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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
 *
 * Loads a file, specified by the given @uri, and returns the file
 * contents as a string. The @uri argument can specify any transfer
 * method (file:, http:, ftp:, etc) supported by GnomeVFS on your
 * system.
 *
 * It is the caller's responsibility to free the returned value.
 *
 * Returns: the file contents as a newly-allocated string, or NULL
 * if there is an error.
 */
gchar *
gda_file_load (const gchar *uristr)
{
	GnomeVFSHandle *handle;
	GnomeVFSURI *uri;
	GnomeVFSResult result;
	GnomeVFSFileSize size;
	GString *str = NULL;
	gchar *retval = NULL;
	gchar buffer[8193];

	g_return_val_if_fail (uri != NULL, NULL);

	uri = gnome_vfs_uri_new (uristr);
	if (!uri)
		return FALSE;

	/* open the file */
	result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_READ);
	gnome_vfs_uri_unref (uri);
	if (result != GNOME_VFS_OK)
		return NULL;

	memset (buffer, 0, sizeof (buffer));
	result = gnome_vfs_read (handle, (gpointer) buffer, sizeof (buffer) - 1, &size);
	while (result == GNOME_VFS_OK) {
		if (!str)
			str = g_string_new (buffer);
		else
			str = g_string_append (str, buffer);

		memset (buffer, 0, sizeof (buffer));
		result = gnome_vfs_read (handle, (gpointer) buffer,
					 sizeof (buffer) - 1, &size);
	}

	if (result == GNOME_VFS_ERROR_EOF) {
		retval = str->str;
		g_string_free (str, FALSE);
	}
	else {
		retval = NULL;
		g_string_free (str, TRUE);
	}

	/* close file */
	gnome_vfs_close (handle);

	return retval;
}

/**
 * gda_file_save
 * @uristr: URI for the file to be saved.
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
gda_file_save (const gchar *uristr, const gchar *buffer, gint len)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult result;
	GnomeVFSFileSize size;
	GnomeVFSURI *uri;

	g_return_val_if_fail (uristr != NULL, FALSE);

	uri = gnome_vfs_uri_new (uristr);
	if (!uri)
		return FALSE;

	/* open the file */
	result = gnome_vfs_create_uri (&handle, uri, GNOME_VFS_OPEN_WRITE, FALSE,
				       S_IRUSR | S_IWUSR);
	if (result != GNOME_VFS_OK)
		return FALSE;

	result = gnome_vfs_write (handle, (gconstpointer) buffer, len, &size);
	gnome_vfs_close (handle);

	return result == GNOME_VFS_OK ? TRUE : FALSE;
}
