/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
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
 */
GdaQuarkList *
gda_quark_list_new_from_string (const gchar * string)
{
	GdaQuarkList *qlist;

	qlist = gda_quark_list_new ();
	gda_quark_list_add_from_string (qlist, string, FALSE);

	return qlist;
}

/**
 * gda_quark_list_free
 */
void
gda_quark_list_free (GdaQuarkList * qlist)
{
	g_return_if_fail (qlist != NULL);

	g_hash_table_foreach_remove (qlist->hash_table,
				     (GHRFunc) free_hash_pair,
				     g_free);
	g_hash_table_destroy (qlist->hash_table);

	g_free (qlist);
}

/**
 * gda_quark_list_add_from_string
 */
void
gda_quark_list_add_from_string (GdaQuarkList * qlist,
				const gchar * string, gboolean cleanup)
{
	gchar **arr;

	g_return_if_fail (qlist != NULL);
	g_return_if_fail (string != NULL);

	/* FIXME: remove all strings if cleanup != FALSE */

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
 */
const gchar *
gda_quark_list_find (GdaQuarkList * qlist, const gchar * name)
{
	gchar *value;

	g_return_val_if_fail (qlist != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	value = g_hash_table_lookup (qlist->hash_table, name);
	return (const gchar *) value;
}
