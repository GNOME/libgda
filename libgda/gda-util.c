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
 * gda_type_to_string
 * @type: Type to convert from.
 *
 * Return the string representing the given #GdaType.
 *
 * Returns: the name of the type.
 */
const gchar *
gda_type_to_string (GdaType type)
{
	switch (type) {
	case GDA_TYPE_NULL : return "null";
	case GDA_TYPE_BIGINT : return "bigint";
	case GDA_TYPE_BINARY : return "binary";
	case GDA_TYPE_BOOLEAN : return "boolean";
	case GDA_TYPE_DATE : return "date";
	case GDA_TYPE_DOUBLE : return "double";
	case GDA_TYPE_GEOMETRIC_POINT : return "point";
	case GDA_TYPE_INTEGER : return "integer";
	case GDA_TYPE_LIST : return "list";
	case GDA_TYPE_NUMERIC : return "numeric";
	case GDA_TYPE_SINGLE : return "single";
	case GDA_TYPE_SMALLINT : return "smallint";
	case GDA_TYPE_STRING : return "string";
	case GDA_TYPE_TIME : return "time";
	case GDA_TYPE_TIMESTAMP : return "timestamp";
	case GDA_TYPE_TINYINT : return "tinyint";
	}

	return "string";
}

/**
 * gda_type_from_string
 */
GdaType
gda_type_from_string (const gchar *str)
{
	g_return_val_if_fail (str != NULL, GDA_TYPE_UNKNOWN);

	if (!g_strcasecmp (str, "null")) return GDA_TYPE_NULL;
	else if (!g_strcasecmp (str, "bigint")) return GDA_TYPE_BIGINT;
	else if (!g_strcasecmp (str, "binary")) return GDA_TYPE_BINARY;
	else if (!g_strcasecmp (str, "boolean")) return GDA_TYPE_BOOLEAN;
	else if (!g_strcasecmp (str, "date")) return GDA_TYPE_DATE;
	else if (!g_strcasecmp (str, "double")) return GDA_TYPE_DOUBLE;
	else if (!g_strcasecmp (str, "point")) return GDA_TYPE_GEOMETRIC_POINT;
	else if (!g_strcasecmp (str, "integer")) return GDA_TYPE_INTEGER;
	else if (!g_strcasecmp (str, "list")) return GDA_TYPE_LIST;
	else if (!g_strcasecmp (str, "numeric")) return GDA_TYPE_NUMERIC;
	else if (!g_strcasecmp (str, "single")) return GDA_TYPE_SINGLE;
	else if (!g_strcasecmp (str, "smallint")) return GDA_TYPE_SMALLINT;
	else if (!g_strcasecmp (str, "string")) return GDA_TYPE_STRING;
	else if (!g_strcasecmp (str, "time")) return GDA_TYPE_TIME;
	else if (!g_strcasecmp (str, "timestamp")) return GDA_TYPE_TIMESTAMP;
	else if (!g_strcasecmp (str, "tinyint")) return GDA_TYPE_TINYINT;

	return GDA_TYPE_UNKNOWN;
}

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
