/* GDA Common Library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * Authors:
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

#include <libgda/gda-quark-list.h>
#include <glib/ghash.h>
#include <glib/gmem.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>

struct _GdaQuarkList {
	GHashTable *hash_table;
};

/*
 * Private functions
 */

static void
free_hash_pair (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

/**
 * gda_quark_list_new
 *
 * Creates a new #GdaQuarkList, which is a set of key->value pairs,
 * very similar to GLib's GHashTable, but with the only purpose to
 * make easier the parsing and creation of data source connection
 * strings.
 *
 * Returns: the newly created #GdaQuarkList.
 */
GdaQuarkList *
gda_quark_list_new (void)
{
	GdaQuarkList *qlist;

	qlist = g_new0 (GdaQuarkList, 1);
	qlist->hash_table = g_hash_table_new (g_str_hash, g_str_equal);

	return qlist;
}

/**
 * gda_quark_list_new_from_string
 * @string: a connection string.
 *
 * Creates a new #GdaQuarkList given a connection string.
 *
 * Returns: the newly created #GdaQuarkList.
 */
GdaQuarkList *
gda_quark_list_new_from_string (const gchar *string)
{
	GdaQuarkList *qlist;

	qlist = gda_quark_list_new ();
	gda_quark_list_add_from_string (qlist, string, FALSE);

	return qlist;
}

/**
 * gda_quark_list_clear
 * @qlist: a #GdaQuarkList.
 *
 * Removes all strings in the given #GdaQuarkList.
 */
static void
gda_quark_list_clear(GdaQuarkList *qlist)
{
	g_return_if_fail (qlist != NULL);
	
	g_hash_table_foreach_remove (qlist->hash_table,
				     (GHRFunc) free_hash_pair,
				     g_free);
}

/**
 * gda_quark_list_free
 * @qlist: a #GdaQuarkList.
 *
 * Releases all memory occupied by the given #GdaQuarkList.
 */
void
gda_quark_list_free (GdaQuarkList *qlist)
{
	g_return_if_fail (qlist != NULL);

	gda_quark_list_clear(qlist);
	g_hash_table_destroy (qlist->hash_table);

	g_free (qlist);
}

/**
 * gda_quark_list_add_from_string
 * @qlist: a #GdaQuarkList.
 * @string: a connection string.
 * @cleanup: whether to cleanup the previous content or not.
 *
 * Adds new key->value pairs from the given @string. If @cleanup is
 * set to %TRUE, the previous contents will be discarded before adding
 * the new pairs.
 */
void
gda_quark_list_add_from_string (GdaQuarkList *qlist,
				const gchar *string,
				gboolean cleanup)
{
	gchar **arr;

	g_return_if_fail (qlist != NULL);
	g_return_if_fail (string != NULL);

	if (cleanup != FALSE)
		gda_quark_list_clear(qlist);

	arr = (gchar **) g_strsplit (string, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			gchar **pair;

			pair = (gchar **) g_strsplit (arr[n], "=", 2);
			if (pair) {
				gchar *name = pair[0];
				gchar *value = pair[1];
				g_hash_table_insert (qlist->hash_table,
						     (gpointer) g_strdup (name),
						     (gpointer) g_strdup (value));
				g_strfreev (pair);
			}
			n++;
		}
		g_strfreev (arr);
	}
}

/**
 * gda_quark_list_find
 * @qlist: a #GdaQuarkList.
 * @name: the name of the value to search for.
 *
 * Searches for the value identified by @name in the given #GdaQuarkList.
 *
 * Returns: the value associated with the given key if found, or %NULL
 * if not found.
 */
const gchar *
gda_quark_list_find (GdaQuarkList *qlist, const gchar *name)
{
	gchar *value;

	g_return_val_if_fail (qlist != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	value = g_hash_table_lookup (qlist->hash_table, name);
	return (const gchar *) value;
}

/**
 * gda_quark_list_remove
 * @qlist: a #GdaQuarkList structure.
 * @name: an entry name.
 *
 * Removes an entry from the #GdaQuarkList, given its name.
 */
void
gda_quark_list_remove (GdaQuarkList *qlist, const gchar *name)
{
	gpointer orig_key;
	gpointer orig_value;

	g_return_if_fail (qlist != NULL);
	g_return_if_fail (name != NULL);

	if (g_hash_table_lookup_extended (qlist->hash_table, name,
					  &orig_key, &orig_value)) {
		g_hash_table_remove (qlist->hash_table, name);
		g_free (orig_key);
		g_free (orig_value);
	}
}
