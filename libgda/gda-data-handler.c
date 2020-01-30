/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-data-handler"

#include "gda-data-handler.h"
#include "handlers/gda-handler-bin.h"
#include "handlers/gda-handler-boolean.h"
#include "handlers/gda-handler-numerical.h"
#include "handlers/gda-handler-string.h"
#include "handlers/gda-handler-text.h"
#include "handlers/gda-handler-time.h"
#include "handlers/gda-handler-type.h"

G_DEFINE_INTERFACE (GdaDataHandler, gda_data_handler, G_TYPE_OBJECT)

char *gda_data_handler_get_sql_from_value_default (GdaDataHandler *dh, const GValue *value);

static void
gda_data_handler_default_init (GdaDataHandlerInterface *iface) {
	iface->get_sql_from_value = gda_data_handler_get_sql_from_value_default;
}

char *
gda_data_handler_get_sql_from_value_default (GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (! value || gda_value_is_null (value))
		return g_strdup ("NULL");

	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, G_VALUE_TYPE (value)), NULL);
	return NULL;
}
/**
 * gda_data_handler_get_sql_from_value:
 * @dh: an object which implements the #GdaDataHandler interface
 * @value: (nullable): the value to be converted to a string, or %NULL
 *
 * Creates a new string which is an SQL representation of the given value, the returned string
 * can be used directly in an SQL statement. For example if @value is a G_TYPE_STRING, then
 * the returned string will be correctly quoted. Note however that it is a better practice
 * to use variables in statements instead of value literals, see
 * the <link linkend="GdaSqlParser.description">GdaSqlParser</link> for more information.
 *
 * If the value is NULL or is of type GDA_TYPE_NULL,
 * or is a G_TYPE_STRING and g_value_get_string() returns %NULL, the returned string is "NULL".
 *
 * Returns: (transfer full): the new string, or %NULL if an error occurred
 */
gchar *
gda_data_handler_get_sql_from_value (GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sql_from_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sql_from_value) (dh, value);
	
	return NULL;
}


/**
 * gda_data_handler_get_str_from_value:
 * @dh: an object which implements the #GdaDataHandler interface
 * @value: (nullable): the value to be converted to a string, or %NULL
 *
 * Creates a new string which is a "user friendly" representation of the given value
 * (in the user's locale, specially for the dates). If the value is 
 * NULL or is of type GDA_TYPE_NULL, the returned string is a copy of "" (empty string).
 *
 * Note: the returned value will be in the current locale representation.
 *
 * Returns: (transfer full): the new string, or %NULL if an error occurred
 */
gchar *
gda_data_handler_get_str_from_value (GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (! value || gda_value_is_null (value))
		return g_strdup ("");

	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, G_VALUE_TYPE (value)), NULL);

	/* Calling the real function with value != NULL and not of type GDA_TYPE_NULL */
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_str_from_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_str_from_value) (dh, value);
	
	return NULL;
}

/**
 * gda_data_handler_get_value_from_sql:
 * @dh: an object which implements the #GdaDataHandler interface
 * @sql: (nullable) (transfer none): an SQL string, or %NULL
 * @type: a GType
 *
 * Creates a new GValue which represents the SQL value given as argument. This is
 * the opposite of the function gda_data_handler_get_sql_from_value(). The type argument
 * is used to determine the real data type requested for the returned value.
 *
 * If the @sql string is %NULL, then the returned GValue is of type GDA_TYPE_NULL;
 * if the @sql string does not correspond to a valid SQL string for the requested type, then
 * the %NULL is returned.
 *
 * Returns: (transfer full): the new #GValue or %NULL on error
 */
GValue *
gda_data_handler_get_value_from_sql (GdaDataHandler *dh, const gchar *sql, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	if (!sql)
		return gda_value_new_null ();

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql) (dh, sql, type);
	
	return NULL;
}


/**
 * gda_data_handler_get_value_from_str:
 * @dh: an object which implements the #GdaDataHandler interface
 * @str: (nullable) (transfer none): a string or %NULL
 * @type: a GType
 *
 * Creates a new GValue which represents the @str value given as argument. This is
 * the opposite of the function gda_data_handler_get_str_from_value(). The type argument
 * is used to determine the real data type requested for the returned value.
 *
 * If the @str string is %NULL, then the returned GValue is of type GDA_TYPE_NULL;
 * if the @str string does not correspond to a valid string for the requested type, then
 * %NULL is returned.
 *
 * Note: the @str string must be in the current locale representation
 *
 * Returns: (transfer full): the new #GValue or %NULL on error
 */
GValue *
gda_data_handler_get_value_from_str (GdaDataHandler *dh, const gchar *str, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

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
 * gda_data_handler_get_sane_init_value:
 * @dh: an object which implements the #GdaDataHandler interface
 * @type: a #GType
 *
 * Creates a new GValue which holds a sane initial value to be used if no value is specifically
 * provided. For example for a simple string, this would return a new value containing the "" string.
 *
 * Returns: (nullable) (transfer full): the new #GValue, or %NULL if no such value can be created.
 */
GValue *
gda_data_handler_get_sane_init_value (GdaDataHandler *dh, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sane_init_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sane_init_value) (dh, type);
	
	return NULL;
}

/**
 * gda_data_handler_accepts_g_type:
 * @dh: an object which implements the #GdaDataHandler interface
 * @type: a #GType
 *
 * Checks wether the GdaDataHandler is able to handle the gda type given as argument.
 *
 * Returns: %TRUE if the gda type can be handled
 */
gboolean
gda_data_handler_accepts_g_type (GdaDataHandler *dh, GType type)
{
	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), FALSE);
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->accepts_g_type)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->accepts_g_type) (dh, type);

	return FALSE;
}

/**
 * gda_data_handler_get_descr:
 * @dh: an object which implements the #GdaDataHandler interface
 *
 * Get a short description of the GdaDataHandler
 *
 * Returns: (transfer none): the description
 */
const gchar *
gda_data_handler_get_descr (GdaDataHandler *dh)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_descr)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_descr) (dh);
	
	return NULL;
}

/**
 * gda_data_handler_get_default:
 * @for_type: a #GType type
 *
 * Obtain a pointer to a #GdaDataHandler which can manage #GValue values of type @for_type. The returned
 * data handler will be adapted to use the current locale information (for example dates will be formatted
 * taking into account the locale).
 *
 * The returned pointer is %NULL if there is no default data handler available for the @for_type data type
 *
 * Returns: (transfer full): a #GdaDataHandler which must be destroyed using g_object_unref()
 *
 * Since: 4.2.3
 */
GdaDataHandler *
gda_data_handler_get_default (GType for_type)
{
	GdaDataHandler *dh = NULL;

  if (for_type == G_TYPE_INT64) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_UINT64) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == GDA_TYPE_BINARY) {
    dh = gda_handler_bin_new ();
  } else if (for_type == GDA_TYPE_BLOB) {
    dh = gda_handler_bin_new ();
  } else if (for_type == G_TYPE_BOOLEAN) {
    dh = gda_handler_boolean_new ();
  } else if (for_type == G_TYPE_DATE) {
    dh = gda_handler_time_new ();
  } else if (for_type == G_TYPE_DOUBLE) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_INT) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == GDA_TYPE_NUMERIC) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_FLOAT) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == GDA_TYPE_SHORT) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == GDA_TYPE_USHORT) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_STRING) {
    dh = gda_handler_string_new ();
  } else if (for_type == GDA_TYPE_TEXT) {
    dh = gda_handler_text_new ();
  } else if (for_type == GDA_TYPE_TIME) {
    dh = gda_handler_time_new ();
  } else if (for_type == G_TYPE_DATE_TIME) {
    dh = gda_handler_time_new ();
  } else if (for_type == G_TYPE_CHAR) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_UCHAR) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_ULONG) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_LONG) {
    dh = gda_handler_numerical_new ();
  } else if (for_type == G_TYPE_GTYPE) {
    dh = gda_handler_type_new ();
  } else if (for_type == G_TYPE_UINT) {
    dh = gda_handler_numerical_new ();
  }
	return dh;
}
