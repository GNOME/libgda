/* GDA common library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include "gda-parameter.h"

struct _GdaParameterList {
	GHashTable *hash;
};

/*
 * Private functions
 */

static void
free_hash_param (gpointer key, gpointer value, gpointer user_data)
{
}

/**
 * gda_parameter_new
 * @name: name for the parameter being created.
 *
 * Create a new #GdaParameter object, which is usually used
 * with #GdaParameterList.
 *
 * Returns: the newly created #GdaParameter.
 */
GdaParameter *
gda_parameter_new (const gchar *name)
{
	GdaParameter *param;

	param = GNOME_Database_Parameter__alloc ();
	if (name)
		gda_parameter_set_name (param, name);
	CORBA_any_set_release (&param->value, TRUE);

	return param;
}

/**
 * gda_parameter_free
 * @param: the #GdaParameter to be freed.
 */
void
gda_parameter_free (GdaParameter *param)
{
	g_return_if_fail (param != NULL);
	CORBA_free (param);
}

/**
 * gda_parameter_get_name
 * @param: a #GdaParameter object.
 *
 * Returns the name of the given #GdaParameter.
 */
const gchar *
gda_parameter_get_name (GdaParameter *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	return (const gchar *) param->name;
}

/**
 * gda_parameter_set_name
 */
void
gda_parameter_set_name (GdaParameter *param, const gchar *name)
{
	g_return_if_fail (param != NULL);
	g_return_if_fail (name != NULL);

	if (param->name != NULL)
		CORBA_free (param->name);
	param->name = CORBA_string_dup (name);
}

/**
 * gda_parameter_get_value
 */
GdaValue *
gda_parameter_get_value (GdaParameter *param)
{
	g_return_val_if_fail (param != NULL, NULL);
	return &param->value;
}

/**
 * gda_parameter_set_value
 */
void
gda_parameter_set_value (GdaParameter *param, GdaValue *value)
{
	/* FIXME */
}

/**
 * gda_parameter_list_new
 *
 * Create a new #GdaParameterList.
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
 */
void
gda_parameter_list_add_parameter (GdaParameterList *plist, GdaParameter *param)
{
	g_return_if_fail (plist != NULL);
}
