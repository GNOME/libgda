/* gda-data-handler.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gda-data-handler.h"

static void gda_data_handler_iface_init (gpointer g_class);

GType
gda_data_handler_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataHandlerIface),
			(GBaseInitFunc) gda_data_handler_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaDataHandler", &info, 0);
                g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	return type;
}


static void
gda_data_handler_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		initialized = TRUE;
	}
}

/**
 * gda_data_handler_get_sql_from_value
 * @dh: an object which implements the #GdaDataHandler interface
 * @value: the value to be converted to a string
 *
 * Creates a new string which is an SQL representation of the given value. If the value is NULL or
 * is of type GDA_TYPE_NULL, the returned string is NULL.
 *
 * Returns: the new string.
 */
gchar *
gda_data_handler_get_sql_from_value (GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	
	if (! value || gda_value_is_null (value))
		return NULL;

	/* Calling the real function with value != NULL and not of type GDA_TYPE_NULL */
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sql_from_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sql_from_value) (dh, value);
	
	return NULL;
}

/**
 * gda_data_handler_get_str_from_value
 * @dh: an object which implements the #GdaDataHandler interface
 * @value: the value to be converted to a string
 *
 * Creates a new string which is a "user friendly" representation of the given value
 * (in the users's locale, specially for the dates). If the value is 
 * NULL or is of type GDA_TYPE_NULL, the returned string is a copy of "" (empty string).
 *
 * Returns: the new string.
 */
gchar *
gda_data_handler_get_str_from_value (GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (! value || gda_value_is_null (value))
		return g_strdup ("");

	/* Calling the real function with value != NULL and not of type GDA_TYPE_NULL */
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_str_from_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_str_from_value) (dh, value);
	
	return NULL;
}

/**
 * gda_data_handler_get_value_from_sql
 * @dh: an object which implements the #GdaDataHandler interface
 * @sql:
 * @type: 
 *
 * Creates a new GValue which represents the SQL value given as argument. This is
 * the opposite of the function gda_data_handler_get_sql_from_value(). The type argument
 * is used to determine the real data type requested for the returned value.
 *
 * If the sql string is NULL, then the returned GValue is of type GDA_TYPE_NULL;
 * if the sql string does not correspond to a valid SQL string for the requested type, then
 * NULL is returned.
 *
 * Returns: the new GValue or NULL on error
 */
GValue *
gda_data_handler_get_value_from_sql (GdaDataHandler *dh, const gchar *sql, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (GDA_DATA_HANDLER (dh), type), NULL);

	if (!sql)
		return gda_value_new_null ();

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql) (dh, sql, type);
	
	return NULL;
}


/**
 * gda_data_handler_get_value_from_str
 * @dh: an object which implements the #GdaDataHandler interface
 * @str:
 * @type: 
 *
 * Creates a new GValue which represents the STR value given as argument. This is
 * the opposite of the function gda_data_handler_get_str_from_value(). The type argument
 * is used to determine the real data type requested for the returned value.
 *
 * If the str string is NULL, then the returned GValue is of type GDA_TYPE_NULL;
 * if the str string does not correspond to a valid STR string for the requested type, then
 * NULL is returned.
 *
 * Returns: the new GValue or NULL on error
 */
GValue *
gda_data_handler_get_value_from_str (GdaDataHandler *dh, const gchar *str, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (GDA_DATA_HANDLER (dh), type), NULL);

	if (!str)
		return gda_value_new_null ();

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_str)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_str) (dh, str, type);
	else {
		/* if the get_value_from_str() method is not implemented, then we try the
		   get_value_from_sql() method */
		if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql)
			return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql) (dh, str, type);
	}
	
	return NULL;
}


/**
 * gda_data_handler_get_sane_init_value
 * @dh: an object which implements the #GdaDataHandler interface
 * @type: 
 *
 * Creates a new GValue which holds a sane initial value to be used if no value is specifically
 * provided. For example for a simple string, this would return a new value containing the "" string.
 *
 * Returns: the new GValue, or %NULL if no such value can be created.
 */
GValue *
gda_data_handler_get_sane_init_value (GdaDataHandler *dh, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (GDA_DATA_HANDLER (dh), type), NULL);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sane_init_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sane_init_value) (dh, type);
	
	return NULL;
}

/**
 * gda_data_handler_get_nb_g_types
 * @dh: an object which implements the #GdaDataHandler interface
 *
 * Get the number of GType types the GdaDataHandler can handle correctly
 *
 * Returns: the number.
 */
guint
gda_data_handler_get_nb_g_types (GdaDataHandler *dh)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), 0);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_nb_g_types)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_nb_g_types) (dh);
	
	return 0;
}

/**
 * gda_data_handler_accepts_g_type
 * @dh: an object which implements the #GdaDataHandler interface
 * @type:
 *
 * Checks wether the GdaDataHandler is able to handle the gda type given as argument.
 *
 * Returns: TRUE if the gda type can be handled
 */
gboolean
gda_data_handler_accepts_g_type (GdaDataHandler *dh, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), FALSE);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->accepts_g_type)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->accepts_g_type) (dh, type);
	
	return FALSE;
}

/**
 * gda_data_handler_get_g_type_index
 * @dh: an object which implements the #GdaDataHandler interface
 * @index: 
 *
 * Get the GType handled by the GdaDataHandler, at the given position (starting at zero).
 *
 * Returns: the GType
 */
GType
gda_data_handler_get_g_type_index (GdaDataHandler *dh, guint index)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), G_TYPE_INVALID);
	g_return_val_if_fail (index < gda_data_handler_get_nb_g_types (dh),
			      G_TYPE_INVALID);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_g_type_index)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_g_type_index) (dh, index);
	
	return G_TYPE_INVALID;
}

/**
 * gda_data_handler_get_descr
 * @dh: an object which implements the #GdaDataHandler interface
 *
 * Get a short description of the GdaDataHandler
 *
 * Returns: the description
 */
const gchar *
gda_data_handler_get_descr (GdaDataHandler *dh)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_descr)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_descr) (dh);
	
	return NULL;
}
