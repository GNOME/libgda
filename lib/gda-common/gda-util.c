/* GDA Common Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include <stdio.h>

#include "config.h"
#include "gda-util.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough. */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/* function called by g_hash_table_foreach to add items to a GList */
static void
add_key_to_list (gpointer key, gpointer value, gpointer user_data)
{
	GList **list = (GList **) user_data;

	*list = g_list_append (*list, key);
}

/**
 * gda_util_hash_to_list
 * @hash_table: a hash table
 *
 * Convert a #GHashTable into a #GList. It only adds to the list the
 * hash table's keys, not the data associated with it. Another thing
 * this function assumes is that the keys are strings (that is,
 * zero-delimited sequence of characters)
 *
 * Returns: the newly created #GList
 */
GList *
gda_util_hash_to_list (GHashTable * hash_table)
{
	GList *list = NULL;

	g_return_val_if_fail (hash_table != NULL, NULL);

	g_hash_table_foreach (hash_table, (GHFunc) add_key_to_list, &list);
	return list;
}

/**
 * gda_util_destroy_hash_pair
 */
gboolean
gda_util_destroy_hash_pair (gchar * key, gpointer value, GFreeFunc free_func)
{
	g_free (key);
	if ((free_func != NULL) && (value != NULL))
		free_func (value);
	return TRUE;
}

/**
 * gda_util_load_file
 * @filename: file name
 *
 * Load a file from disk, and return its contents as a newly-allocated
 * string. You must then free it yourself when no longer needed
 */
gchar *
gda_util_load_file (const gchar * filename)
{
	GString *str;
	FILE *fp;
	gchar *ret;
	gchar buffer[2049];

	g_return_val_if_fail (filename != NULL, NULL);

	if ((fp = fopen (filename, "r"))) {
		str = g_string_new ("");
		while (!feof (fp)) {
			memset (buffer, 0, sizeof (buffer));
			fread ((void *) buffer, sizeof (buffer) - 1, 1, fp);
			str = g_string_append (str, buffer);
		}
		fclose (fp);
		ret = g_strdup (str->str);
		g_string_free (str, TRUE);
		return ret;
	}
	else
		g_warning (_("Could not open file %s"), filename);
	return NULL;
}

/**
 * gda_util_save_file
 * @filename: file name
 * @text: file contents
 *
 * Save the given text into a file
 */
gboolean
gda_util_save_file (const gchar * filename, const gchar * text)
{
	FILE *fp;

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (text != NULL, FALSE);

	if ((fp = fopen (filename, "w"))) {
		fwrite ((void *) text, strlen (text), 1, fp);
		fclose (fp);
		return TRUE;
	}
	else
		g_warning (_("Could not create file %s"), filename);
	return FALSE;
}
