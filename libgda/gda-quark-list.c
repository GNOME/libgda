/* GDA Common Library
 * Copyright (C) 1998 - 2010 The GNOME Foundation.
 *
 * Authors:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-util.h>

struct _GdaQuarkList {
	GHashTable *hash_table;
};

GType gda_quark_list_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static ("GdaQuarkList",
			(GBoxedCopyFunc) gda_quark_list_copy,
			(GBoxedFreeFunc) gda_quark_list_free);
	return our_type;
}

/*
 * Private functions
 */

static void
copy_hash_pair (gpointer key, gpointer value, gpointer user_data)
{
	g_hash_table_insert ((GHashTable *) user_data,
			     g_strdup ((const char *) key),
			     g_strdup ((const char *) value));
}

/**
 * gda_quark_list_new:
 *
 * Creates a new #GdaQuarkList, which is a set of key->value pairs,
 * very similar to GLib's GHashTable, but with the only purpose to
 * make easier the parsing and creation of data source connection
 * strings.
 *
 * Returns: (transfer full): the newly created #GdaQuarkList.
 *
 * Free-function: gda_quark_list_free
 */
GdaQuarkList *
gda_quark_list_new (void)
{
	GdaQuarkList *qlist;

	qlist = g_new0 (GdaQuarkList, 1);
	qlist->hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	return qlist;
}

/**
 * gda_quark_list_new_from_string:
 * @string: a string.
 *
 * Creates a new #GdaQuarkList given a string.
 *
 * @string must be a semi-colon separated list of "&lt;key&gt;=&lt;value&gt;" strings (for example
 * "DB_NAME=notes;USERNAME=alfred"). Each key and value must respect the RFC 1738 recommendations: the
 * <constant>&lt;&gt;&quot;#%{}|\^~[]&apos;`;/?:@=&amp;</constant> and space characters are replaced by 
 * <constant>&quot;%%ab&quot;</constant> where
 * <constant>ab</constant> is the hexadecimal number corresponding to the character (for example the
 * "DB_NAME=notes;USERNAME=al%%20fred" string will specify a username as "al fred"). If this formalism
 * is not respected, then some unexpected results may occur.
 *
 * Returns: (transfer full): the newly created #GdaQuarkList.
 *
 * Free-function: gda_quark_list_free
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
 * gda_quark_list_clear:
 * @qlist: a #GdaQuarkList.
 *
 * Removes all strings in the given #GdaQuarkList.
 */
void
gda_quark_list_clear (GdaQuarkList *qlist)
{
	g_return_if_fail (qlist != NULL);
	
	g_hash_table_remove_all (qlist->hash_table);
}

/**
 * gda_quark_list_free:
 * @qlist: a #GdaQuarkList.
 *
 * Releases all memory occupied by the given #GdaQuarkList.
 */
void
gda_quark_list_free (GdaQuarkList *qlist)
{
	g_return_if_fail (qlist != NULL);

	g_hash_table_destroy (qlist->hash_table);

	g_free (qlist);
}


/**
 * gda_quark_list_copy
 * @qlist: quark_list to get a copy from.
 *
 * Creates a new #GdaQuarkList from an existing one.
 * 
 * Returns: a newly allocated #GdaQuarkList with a copy of the data in @qlist.
 */
GdaQuarkList *
gda_quark_list_copy (GdaQuarkList *qlist)
{
	GdaQuarkList *new_qlist;

	g_return_val_if_fail (qlist != NULL, NULL);
	
	new_qlist = gda_quark_list_new ();
	g_hash_table_foreach (qlist->hash_table,
			      copy_hash_pair,
			      new_qlist->hash_table);
	return new_qlist;
}

/**
 * gda_quark_list_add_from_string
 * @qlist: a #GdaQuarkList.
 * @string: a string.
 * @cleanup: whether to cleanup the previous content or not.
 *
 * @string must be a semi-colon separated list of "&lt;key&gt;=&lt;value&gt;" strings (for example
 * "DB_NAME=notes;USERNAME=alfred"). Each key and value must respect the RFC 1738 recommendations: the
 * <constant>&lt;&gt;&quot;#%{}|\^~[]&apos;`;/?:@=&amp;</constant> and space characters are replaced by 
 * <constant>&quot;%%ab&quot;</constant> where
 * <constant>ab</constant> is the hexadecimal number corresponding to the character (for example the
 * "DB_NAME=notes;USERNAME=al%%20fred" string will specify a username as "al fred"). If this formalism
 * is not respected, then some unexpected results may occur.
 *
 * Adds new key->value pairs from the given @string. If @cleanup is
 * set to %TRUE, the previous contents will be discarded before adding
 * the new pairs.
 */
void
gda_quark_list_add_from_string (GdaQuarkList *qlist, const gchar *string, gboolean cleanup)
{
	gchar **arr;

	g_return_if_fail (qlist != NULL);
	if (!string || !*string)
		return;

	if (cleanup)
		gda_quark_list_clear (qlist);

	arr = (gchar **) g_strsplit (string, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n] && (* (arr[n]))) {
			gchar **pair;

			pair = (gchar **) g_strsplit (arr[n], "=", 2);
			if (pair && pair[0]) {
				gchar *name = pair[0];
				gchar *value = pair[1];
				g_strstrip (name);
				gda_rfc1738_decode (name);
				g_strstrip (value);
				gda_rfc1738_decode (value);
				g_hash_table_insert (qlist->hash_table, 
						     (gpointer) name, (gpointer) value);
				g_free (pair);
			}
			else
				g_strfreev (pair);
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
	g_return_if_fail (qlist != NULL);
	g_return_if_fail (name != NULL);

	g_hash_table_remove (qlist->hash_table, name);
}

/**
 * gda_quark_list_foreach:
 * @qlist: a #GdaQuarkList structure.
 * @func: (scope call): the function to call for each key/value pair
 * @user_data: (closure): user data to pass to the function
 *
 * Calls the given function for each of the key/value pairs in @qlist. The function is passed the key and value 
 * of each pair, and the given user_data parameter. @qlist may not be modified while iterating over it.
 */
void
gda_quark_list_foreach (GdaQuarkList *qlist, GHFunc func, gpointer user_data)
{
	g_return_if_fail (qlist != NULL);

	g_hash_table_foreach (qlist->hash_table, func, user_data);
}
