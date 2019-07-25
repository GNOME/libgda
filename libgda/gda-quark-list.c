/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2006 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-quark-list"

#include <libgda/gda-quark-list.h>
#include <libgda/gda-util.h>
#include <string.h>

#ifdef USE_MLOCK
#include <sys/mman.h>
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#include <winbase.h>
#endif

#define RANDOM_BLOB_SIZE 1024
static gchar random_blob [RANDOM_BLOB_SIZE] = {0};
static void ensure_static_blob_filled (void);

typedef struct {
	guint  offset;/* offset in random_blob XOR from */
	gchar *pvalue; /* XORed value, not 0 terminated */
	gchar *cvalue; /* clear value, memory allocated with malloc() and mlock() */
} ProtectedValue;

static ProtectedValue *protected_value_new (gchar *cvalue);
static void protected_value_free (ProtectedValue *pvalue);
static void protected_value_xor (ProtectedValue *pvalue, gboolean to_clear);

struct _GdaQuarkList {
	GHashTable *hash_table;
	GHashTable *hash_protected;
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
ensure_static_blob_filled (void)
{
	if (random_blob [0] == 0) {
		guint i;
		for (i = 0; i < RANDOM_BLOB_SIZE; i++) {
			random_blob [i]	= g_random_int_range (1, 255);
			/*g_print ("%02x ", (guchar) random_blob [i]);*/
		}
#ifdef G_OS_WIN32
		VirtualLock (random_blob, sizeof (gchar) * RANDOM_BLOB_SIZE);
#else
#ifdef USE_MLOCK
		mlock (random_blob, sizeof (gchar) * RANDOM_BLOB_SIZE);
#endif
#endif
	}
}

static void
copy_hash_pair (gpointer key, gpointer value, gpointer user_data)
{
	g_hash_table_insert ((GHashTable *) user_data,
			     g_strdup ((const char *) key),
			     g_strdup ((const char *) value));
}

guint
protected_get_length (gchar *str, guint offset)
{
	gchar *ptr;
	guint i;
	ensure_static_blob_filled ();
	for (i = 0, ptr = str; i < RANDOM_BLOB_SIZE - offset - 1; i++, ptr++) {
		gchar c0, c1;
		c0 = *ptr;
		c1 = c0 ^ random_blob [offset + i];
		if (!c1)
			break;
	}
	return i;
}

static void
protected_value_xor (ProtectedValue *pvalue, gboolean to_clear)
{
	if (to_clear) {
		if (! pvalue->cvalue) {
			guint i, l;
			ensure_static_blob_filled ();
			l = protected_get_length (pvalue->pvalue, pvalue->offset);
			pvalue->cvalue = malloc (sizeof (gchar) * (l + 1));
			for (i = 0; i < l; i++)
				pvalue->cvalue [i] = pvalue->pvalue [i] ^ random_blob [pvalue->offset + i];
			pvalue->cvalue [l] = 0;
#ifdef G_OS_WIN32
			VirtualLock (pvalue->cvalue, sizeof (gchar) * (l + 1));
#else
#ifdef USE_MLOCK
			mlock (pvalue->cvalue, sizeof (gchar) * (l + 1));
#endif
#endif
			/*g_print ("Unmangled [%s]\n", pvalue->cvalue);*/
		}
	}
	else {
		if (pvalue->cvalue) {
			/*g_print ("Mangled [%s]\n", pvalue->cvalue);*/
			guint i;
			for (i = 0; ; i++) {
				gchar c;
				c = pvalue->cvalue[i];
				pvalue->cvalue[i] = g_random_int_range (1, 255);
				if (!c)
					break;
			}
#ifdef G_OS_WIN32
			VirtualUnlock (pvalue->cvalue, sizeof (gchar) * (i + 1));
#else
#ifdef USE_MLOCK
			munlock (pvalue->cvalue, sizeof (gchar) * (i + 1));
#endif
#endif
			free (pvalue->cvalue);
			pvalue->cvalue = NULL;
		}
	}
}

static void
copy_hash_pair_protected (gpointer key, gpointer value, gpointer user_data)
{
	ProtectedValue *p1, *p2;
	guint l;
	p1 = (ProtectedValue*) value;
	p2 = g_new0 (ProtectedValue, 1);
	l = protected_get_length (p1->pvalue, p1->offset);
	p2->pvalue = g_new (gchar, l + 1);
	memcpy (p2->pvalue, p1->pvalue, l + 1);
	p2->offset = p1->offset;
	p2->cvalue = NULL;
	g_hash_table_insert ((GHashTable *) user_data,
			     g_strdup ((const char *) key), p2);
}

static ProtectedValue *
protected_value_new (gchar *cvalue)
{
	ProtectedValue *pvalue;
	guint i, l;
	l = strlen (cvalue);
	if (l >= RANDOM_BLOB_SIZE) {
		g_warning ("Value too big to protect!");
		return NULL;
	}

	/*g_print ("Initially mangled [%s]\n", cvalue);*/
	ensure_static_blob_filled ();
	pvalue = g_new (ProtectedValue, 1);
	pvalue->offset = g_random_int_range (0, RANDOM_BLOB_SIZE - l - 2);
	pvalue->pvalue = g_new (gchar, l + 1);
	pvalue->cvalue = NULL;
	for (i = 0; i <= l; i++) {
		pvalue->pvalue [i] = cvalue [i] ^ random_blob [pvalue->offset + i];
		cvalue [i] = g_random_int_range (0, 255);
	}
	return pvalue;
}

static void
protected_value_free (ProtectedValue *pvalue)
{
	g_free (pvalue->pvalue);
	if (pvalue->cvalue)
		protected_value_xor (pvalue, FALSE);
	g_free (pvalue);
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
	qlist->hash_table = NULL;
	qlist->hash_protected = NULL;

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
	
	if (qlist->hash_table)
		g_hash_table_remove_all (qlist->hash_table);
	if (qlist->hash_protected)
		g_hash_table_remove_all (qlist->hash_protected);
}

/**
 * gda_quark_list_free:
 * @qlist: (nullable): a #GdaQuarkList, or %NULL
 *
 * Releases all memory occupied by the given #GdaQuarkList.
 */
void
gda_quark_list_free (GdaQuarkList *qlist)
{
	if (qlist) {
		if (qlist->hash_table)
			g_hash_table_destroy (qlist->hash_table);
		if (qlist->hash_protected)
			g_hash_table_destroy (qlist->hash_protected);
		g_free (qlist);
	}
}


/**
 * gda_quark_list_copy:
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
	if (qlist->hash_table) {
		new_qlist->hash_table = g_hash_table_new_full (g_str_hash,
							       g_str_equal,
							       g_free, g_free);
		g_hash_table_foreach (qlist->hash_table, copy_hash_pair, new_qlist->hash_table);
	}
	if (qlist->hash_protected) {
		new_qlist->hash_protected = g_hash_table_new_full (g_str_hash,
								   g_str_equal,
								   g_free,
								   (GDestroyNotify) protected_value_free);
		g_hash_table_foreach (qlist->hash_protected, copy_hash_pair_protected,
				      new_qlist->hash_protected);
	}
	return new_qlist;
}

static gboolean
name_is_protected (const gchar *name)
{
	if (!g_ascii_strncasecmp (name, "pass", 4) ||
	    !g_ascii_strncasecmp (name, "username", 8))
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_quark_list_add_from_string:
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
 * Some corner cases for any string part (delimited by the semi-colon):
 * <itemizedlist>
 *    <listitem><para>If it does not respect the "&lt;key&gt;=&lt;value&gt;" format then it will be ignored.</para></listitem>
 *    <listitem><para>Only the 1st equal character is used to separate the key from the value part (which means
 *       any other equal sign will be part of the value)</para></listitem>
 * </itemizedlist>
 *
 *
 * Adds new key->value pairs from the given @string. If @cleanup is
 * set to %TRUE, the previous contents will be discarded before adding
 * the new pairs.
 */
void
gda_quark_list_add_from_string (GdaQuarkList *qlist, const gchar *string, gboolean cleanup)
{
	g_return_if_fail (qlist != NULL);

	gchar **arr;

	if (!string || !*string)
		return;

	if (cleanup)
		gda_quark_list_clear (qlist);

	arr = g_strsplit (string, ";", -1);
	if (arr != NULL) {
		guint n;
		for (n = 0; arr[n] && (* (arr[n])); n++) {
			gchar **pair;
			gchar *tmp;
			for (tmp = arr[n]; *tmp; tmp++) {
				if (*tmp == '=')
					break;
			}
			if (!*tmp) {
				/* ignore this string since it does not contain the '=' char */
				continue;
			}

			pair = (gchar **) g_strsplit (arr[n], "=", 2);
			if (pair && pair[0]) {
				gchar *name = pair[0];
				gchar *value = pair[1];
				g_strstrip (name);
				gda_rfc1738_decode (name);
				if (value) {
					g_strstrip (value);
					gda_rfc1738_decode (value);
				}

				if (! name_is_protected (name)) {
					if (!qlist->hash_table)
						qlist->hash_table = g_hash_table_new_full (g_str_hash,
											   g_str_equal,
											   g_free, g_free);
					g_hash_table_insert (qlist->hash_table, 
							     (gpointer) name, (gpointer) value);
				}
				else {
					ProtectedValue *pvalue;
					pvalue = protected_value_new (value);
					if (pvalue) {
						if (!qlist->hash_protected)
							qlist->hash_protected = g_hash_table_new_full (g_str_hash,
												       g_str_equal,
												       g_free,
												       (GDestroyNotify) protected_value_free);
						g_hash_table_insert (qlist->hash_protected, 
								     (gpointer) name, (gpointer) pvalue);
						g_free (value); /* has been mangled */
					}
					else {
						if (!qlist->hash_table)
							qlist->hash_table = g_hash_table_new_full (g_str_hash,
												   g_str_equal,
												   g_free, g_free);
						g_hash_table_insert (qlist->hash_table, 
								     (gpointer) name, (gpointer) value);
					}
				}
				g_free (pair);
			}
			else
				g_strfreev (pair);
		}
		g_strfreev (arr);
	}
}

/**
 * gda_quark_list_find:
 * @qlist: a #GdaQuarkList.
 * @name: the name of the value to search for.
 *
 * Searches for the value identified by @name in the given #GdaQuarkList. For protected values
 * (authentification data), don't forget to call gda_quark_list_protect_values() when you
 * don't need them anymore (when needed again, they will be unmangled again).
 *
 * Returns: the value associated with the given key if found, or %NULL if not found.
 */
const gchar *
gda_quark_list_find (GdaQuarkList *qlist, const gchar *name)
{
	gchar *value = NULL;
	g_return_val_if_fail (qlist, NULL);
	g_return_val_if_fail (name, NULL);

	if (qlist->hash_table)
		value = g_hash_table_lookup (qlist->hash_table, name);
	if (value)
		return value;

	ProtectedValue *pvalue = NULL;
	if (qlist->hash_protected)
		pvalue = g_hash_table_lookup (qlist->hash_protected, name);
	if (pvalue) {
		if (! pvalue->cvalue)
			protected_value_xor (pvalue, TRUE);
		return pvalue->cvalue;
	}
	else
		return NULL;
}

/**
 * gda_quark_list_remove:
 * @qlist: a #GdaQuarkList structure.
 * @name: an entry name.
 *
 * Removes an entry from the #GdaQuarkList, given its name.
 */
void
gda_quark_list_remove (GdaQuarkList *qlist, const gchar *name)
{
	gboolean removed = FALSE;
	g_return_if_fail (qlist != NULL);
	g_return_if_fail (name != NULL);

	if (qlist->hash_table && g_hash_table_remove (qlist->hash_table, name))
		removed = TRUE;
	if (!removed && qlist->hash_protected)
		g_hash_table_remove (qlist->hash_protected, name);
}

typedef struct {
	gpointer user_data;
	GHFunc func;
} PFuncData;

static void
p_foreach (gchar *key, ProtectedValue *pvalue, PFuncData *data)
{
	if (pvalue->cvalue)
		data->func ((gpointer) key, (gpointer) pvalue->cvalue, data->user_data);
	else {
		protected_value_xor (pvalue, TRUE);
		data->func ((gpointer) key, (gpointer) pvalue->cvalue, data->user_data);
		protected_value_xor (pvalue, FALSE);
	}
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
	g_return_if_fail (qlist);

	if (qlist->hash_table)
		g_hash_table_foreach (qlist->hash_table, func, user_data);
	if (qlist->hash_protected) {
		PFuncData pdata;
		pdata.user_data = user_data;
		pdata.func = func;
		g_hash_table_foreach (qlist->hash_protected, (GHFunc) p_foreach, &pdata);
	}
}

static void
protect_foreach (G_GNUC_UNUSED gchar *key, ProtectedValue *pvalue, G_GNUC_UNUSED gpointer data)
{
	if (pvalue && pvalue->cvalue)
		protected_value_xor (pvalue, FALSE);
}

/**
 * gda_quark_list_protect_values:
 * @qlist: (nullable): a #GdaQuarkList, or %NULL
 *
 * Call this function to get rid of the clear version of all the values stored in @qlist. If @qlist is %NULL,
 * then this function does nothing.
 *
 * Since: 5.2.0
 */
void
gda_quark_list_protect_values (GdaQuarkList *qlist)
{
	if (qlist && qlist->hash_protected)
		g_hash_table_foreach (qlist->hash_protected, (GHFunc) protect_foreach, NULL);
}
