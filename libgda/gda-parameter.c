/* GDA library
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

#include <glib/ghash.h>
#include <glib/gmem.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <libgda/gda-parameter.h>

struct _GdaParameterList {
	GHashTable *hash;
};

/*
 * Private functions
 */

static void
free_hash_param (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gda_parameter_free ((GdaParameter *) value);
}

/**
 * gda_parameter_new_from_value
 * @name: name for the parameter being created.
 * @value: a #GdaValue for this parameter.
 *
 * Creates a new #GdaParameter object, which is usually used
 * with #GdaParameterList.
 *
 * Returns: the newly created #GdaParameter.
 */
GdaParameter *
gda_parameter_new_from_value (const gchar *name, GdaValue *value)
{
	GdaParameter *param;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	param = g_new0 (GdaParameter, 1);
	param->name = g_strdup (name);
	param->value = gda_value_copy (value);

	return param;
}

/**
 * gda_parameter_new_boolean
 * @name: name for the parameter being created.
 * @value: a boolean value.
 *
 * Creates a new #GdaParameter from a gboolean value.
 *
 * Returns: the newly created #GdaParameter.
 */
GdaParameter *
gda_parameter_new_boolean (const gchar *name, gboolean value)
{
	GdaParameter *param;

	g_return_val_if_fail (name != NULL, NULL);

	param = g_new0 (GdaParameter, 1);
	param->name = g_strdup (name);
	param->value = gda_value_new_boolean (value);

	return param;
}

/**
 * gda_parameter_new_double
 * @name: name for the parameter being created.
 * @value: a gdouble value.
 *
 * Creates a new #GdaParameter from a gdouble value.
 *
 * Returns: the newly created #GdaParameter.
 */
GdaParameter *
gda_parameter_new_double (const gchar *name, gdouble value)
{
	GdaParameter *param;

	g_return_val_if_fail (name != NULL, NULL);

	param = g_new0 (GdaParameter, 1);
	param->name = g_strdup (name);
	param->value = gda_value_new_double (value);

	return param;
}

/**
 * gda_parameter_new_gobject
 * @name: name for the parameter being created.
 * @value: a GObject value.
 *
 * Creates a new #GdaParameter from a GObject.
 *
 * Returns: the newly created #GdaParameter.
 */
GdaParameter *
gda_parameter_new_gobject (const gchar *name, const GObject *value)
{
	GdaParameter *param;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	param = g_new0 (GdaParameter, 1);
	param->name = g_strdup (name);
	param->value = gda_value_new_gobject (value);

	return param;
}
 
/**
 * gda_parameter_new_string
 * @name: name for the parameter being created.
 * @value: string value.
 *
 * Creates a new #GdaParameter from a string.
 *
 * Returns: the newly created #GdaParameter.
 */
GdaParameter *
gda_parameter_new_string (const gchar *name, const gchar *value)
{
	GdaParameter *param;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	param = g_new0 (GdaParameter, 1);
	param->name = g_strdup (name);
	param->value = gda_value_new_string (value);

	return param;
}

/**
 * gda_parameter_free
 * @param: the #GdaParameter to be freed.
 *
 * Releases all memory occupied by the given #GdaParameter.
 */
void
gda_parameter_free (GdaParameter *param)
{
	g_return_if_fail (param != NULL);

	g_free (param->name);
	gda_value_free (param->value);
	g_free (param);
}

/**
 * gda_parameter_get_name
 * @param: a #GdaParameter object.
 *
 * Returns: the name of the given #GdaParameter.
 */
const gchar *
gda_parameter_get_name (GdaParameter *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	return (const gchar *) param->name;
}

/**
 * gda_parameter_set_name
 * @param: a #GdaParameter.
 * @name: new name for the parameter.
 *
 * Sets the name of the given #GdaParameter.
 */
void
gda_parameter_set_name (GdaParameter *param, const gchar *name)
{
	g_return_if_fail (param != NULL);
	g_return_if_fail (name != NULL);

	if (param->name != NULL)
		g_free (param->name);
	param->name = g_strdup (name);
}

/**
 * gda_parameter_get_value
 * @param: a #GdaParameter.
 *
 * Returns: the value stored in the given @param.
 */
const GdaValue *
gda_parameter_get_value (GdaParameter *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	return param->value;
}

/**
 * gda_parameter_set_value
 * @param: a #GdaParameter.
 * @value: a #GdaValue.
 *
 * Stores the given @value in the given @param.
 */
void
gda_parameter_set_value (GdaParameter *param, GdaValue *value)
{
	/* FIXME */
}

/**
 * gda_parameter_list_new
 *
 * Creates a new #GdaParameterList.
 *
 * Returns: the newly created parameter list.
 */
GdaParameterList *
gda_parameter_list_new (void)
{
	GdaParameterList *plist;

	plist = g_new0 (GdaParameterList, 1);
	plist->hash = g_hash_table_new (g_str_hash, g_str_equal);

	return plist;
}

/**
 * gda_parameter_list_free
 * @plist: a #GdaParameterList.
 *
 * Releases all memory occupied by the given #GdaParameterList.
 */
void
gda_parameter_list_free (GdaParameterList *plist)
{
	g_return_if_fail (plist != NULL);

	g_hash_table_foreach (plist->hash, free_hash_param, NULL);
	g_hash_table_destroy (plist->hash);

	g_free (plist);
}

/**
 * gda_parameter_list_add_parameter
 * @plist: a #GdaParameterList.
 * @param: the #GdaParameter to be added to the list.
 *
 * Adds a new parameter to the given #GdaParameterList. Note that @param is, 
 * when calling this function, is owned by the #GdaParameterList, so the 
 * caller should just forget about it and not try to free the parameter once 
 * it's been added to the #GdaParameterList.
 */
void
gda_parameter_list_add_parameter (GdaParameterList *plist, GdaParameter *param)
{
	gpointer orig_key;
	gpointer orig_value;
	const gchar *name;

	g_return_if_fail (plist != NULL);
	g_return_if_fail (param != NULL);

	name = gda_parameter_get_name (param);

	/* first look for the key in our list */
	if (g_hash_table_lookup_extended (plist->hash, name, &orig_key, &orig_value)) {
		g_hash_table_remove (plist->hash, name);
		g_free (orig_key);
		gda_parameter_free ((GdaParameter *) orig_value);
	}

	/* add the parameter to the list */
	g_hash_table_insert (plist->hash, g_strdup (name), param);
}

static void
get_names_cb (gpointer key, gpointer value, gpointer user_data)
{
	GList **list = (GList **) user_data;
	*list = g_list_append (*list, key);
}

/**
 * gda_parameter_list_get_names
 * @plist: a #GdaParameterList.
 *
 * Gets the names of all parameters in the parameter list.
 *
 * Returns: a GList containing the names of the parameters. After
 * using it, you should free this list by calling g_list_free.
 */
GList *
gda_parameter_list_get_names (GdaParameterList *plist)
{
	GList *list = NULL;

	g_return_val_if_fail (plist != NULL, NULL);

	g_hash_table_foreach (plist->hash, get_names_cb, &list);
	return list;
}

/**
 * gda_parameter_list_find
 * @plist: a #GdaParameterList.
 * @name: name of the parameter to search for.
 *
 * Gets a #GdaParameter from the parameter list given its name.
 *
 * Returns: the #GdaParameter identified by @name, if found, or %NULL
 * if not found.
 */
GdaParameter *
gda_parameter_list_find (GdaParameterList *plist, const gchar *name)
{
	g_return_val_if_fail (plist != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (plist->hash, name);
}

/**
 * gda_parameter_list_clear
 * @plist: a #GdaParameterList.
 *
 * Clears the parameter list. This means removing all #GdaParameter's currently
 * being stored in the parameter list. After calling this function,
 * the parameter list is empty.
 */
void
gda_parameter_list_clear (GdaParameterList *plist)
{
	g_return_if_fail (plist != NULL);
	g_hash_table_foreach_remove (plist->hash, (GHRFunc) free_hash_param, NULL);
}

/**
 * gda_parameter_list_get_length
 * @plist: a #GdaParameterList.
 *
 * Returns: the number of parameters stored in the given parameter list.
 */
guint
gda_parameter_list_get_length (GdaParameterList *plist)
{
	return plist ? g_hash_table_size (plist->hash) : 0;
}
