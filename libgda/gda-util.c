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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gfileutils.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <libsql/sql_parser.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>

/**
 * gda_type_to_string
 * @type: Type to convert from.
 *
 * Return the string representing the given #GdaType.
 *
 * Returns: the name of the type.
 */
const gchar *
gda_type_to_string (GdaValueType type)
{
	switch (type) {
	case GDA_VALUE_TYPE_NULL : return "null";
	case GDA_VALUE_TYPE_BIGINT : return "bigint";
	case GDA_VALUE_TYPE_BINARY : return "binary";
	case GDA_VALUE_TYPE_BOOLEAN : return "boolean";
	case GDA_VALUE_TYPE_DATE : return "date";
	case GDA_VALUE_TYPE_DOUBLE : return "double";
	case GDA_VALUE_TYPE_GEOMETRIC_POINT : return "point";
	case GDA_VALUE_TYPE_INTEGER : return "integer";
	case GDA_VALUE_TYPE_LIST : return "list";
	case GDA_VALUE_TYPE_NUMERIC : return "numeric";
	case GDA_VALUE_TYPE_SINGLE : return "single";
	case GDA_VALUE_TYPE_SMALLINT : return "smallint";
	case GDA_VALUE_TYPE_STRING : return "string";
	case GDA_VALUE_TYPE_TIME : return "time";
	case GDA_VALUE_TYPE_TIMESTAMP : return "timestamp";
	case GDA_VALUE_TYPE_TINYINT : return "tinyint";
	default:
	}

	return "string";
}

/**
 * gda_type_from_string
 */
GdaValueType
gda_type_from_string (const gchar *str)
{
	g_return_val_if_fail (str != NULL, GDA_VALUE_TYPE_UNKNOWN);

	if (!g_strcasecmp (str, "null")) return GDA_VALUE_TYPE_NULL;
	else if (!g_strcasecmp (str, "bigint")) return GDA_VALUE_TYPE_BIGINT;
	else if (!g_strcasecmp (str, "binary")) return GDA_VALUE_TYPE_BINARY;
	else if (!g_strcasecmp (str, "boolean")) return GDA_VALUE_TYPE_BOOLEAN;
	else if (!g_strcasecmp (str, "date")) return GDA_VALUE_TYPE_DATE;
	else if (!g_strcasecmp (str, "double")) return GDA_VALUE_TYPE_DOUBLE;
	else if (!g_strcasecmp (str, "point")) return GDA_VALUE_TYPE_GEOMETRIC_POINT;
	else if (!g_strcasecmp (str, "integer")) return GDA_VALUE_TYPE_INTEGER;
	else if (!g_strcasecmp (str, "list")) return GDA_VALUE_TYPE_LIST;
	else if (!g_strcasecmp (str, "numeric")) return GDA_VALUE_TYPE_NUMERIC;
	else if (!g_strcasecmp (str, "single")) return GDA_VALUE_TYPE_SINGLE;
	else if (!g_strcasecmp (str, "smallint")) return GDA_VALUE_TYPE_SMALLINT;
	else if (!g_strcasecmp (str, "string")) return GDA_VALUE_TYPE_STRING;
	else if (!g_strcasecmp (str, "time")) return GDA_VALUE_TYPE_TIME;
	else if (!g_strcasecmp (str, "timestamp")) return GDA_VALUE_TYPE_TIMESTAMP;
	else if (!g_strcasecmp (str, "tinyint")) return GDA_VALUE_TYPE_TINYINT;

	return GDA_VALUE_TYPE_UNKNOWN;
}

/* function called by g_hash_table_foreach to add items to a GList */
static void
add_string_key_to_list (gpointer key, gpointer value, gpointer user_data)
{
        GList **list = (GList **) user_data;

        *list = g_list_append (*list, g_strdup (key));
}

/**
 * gda_string_hash_to_list
 */
GList *
gda_string_hash_to_list (GHashTable *hash_table)
{
	GList *list = NULL;

        g_return_val_if_fail (hash_table != NULL, NULL);

        g_hash_table_foreach (hash_table, (GHFunc) add_string_key_to_list, &list);
        return list;
}

/**
 * gda_sql_replace_placeholders
 * @sql: A SQL command containing placeholders for values.
 * @params: List of values for the placeholders.
 *
 * Replaces the placeholders (:name) in the given SQL command with
 * the values from the #GdaParameterList specified as the @params
 * argument.
 *
 * Returns: the SQL string with all placeholders replaced, or NULL
 * on error. On success, the returned string must be freed by the caller
 * when no longer needed.
 */
gchar *
gda_sql_replace_placeholders (const gchar *sql, GdaParameterList *params)
{
	GString *str;
	sql_statement *sql_stmt;

	g_return_val_if_fail (sql != NULL, NULL);

	/* parse the string */
	sql_stmt = sql_parse (sql);
	if (!sql_stmt) {
		gda_log_error (_("Could not parse SQL command '%s'"), sql);
		return NULL;
	}

	/* FIXME */

	return NULL;
}

/**
 * gda_file_load
 * @filename: path for the file to be loaded.
 *
 * Loads a file, specified by the given @uri, and returns the file
 * contents as a string.
 *
 * It is the caller's responsibility to free the returned value.
 *
 * Returns: the file contents as a newly-allocated string, or NULL
 * if there is an error.
 */
gchar *
gda_file_load (const gchar *filename)
{
	gchar *retval = NULL;
	gsize length = 0;
	GError *error = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (g_file_get_contents (filename, &retval, &length, &error))
		return retval;

	gda_log_error (_("Error while reading %s: %s"), filename, error->message);
	g_error_free (error);

	return NULL;
}

/**
 * gda_file_save
 * @filename: path for the file to be saved.
 * @buffer: contents of the file.
 * @len: size of @buffer.
 *
 * Saves a chunk of data into a file.
 *
 * Returns: TRUE if successful, FALSE on error.
 */
gboolean
gda_file_save (const gchar *filename, const gchar *buffer, gint len)
{
	gint fd;
	gint res;
	
	g_return_val_if_fail (filename != NULL, FALSE);

	fd = open (filename, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		gda_log_error (_("Could not create file %s"), filename);
		return FALSE;
	}

	res = write (fd, (const void *) buffer, len);
	close (fd);

	return res == -1 ? FALSE : TRUE;
}
