/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include "gda-data-handler.h"
#include "handlers/gda-handler-bin.h"
#include "handlers/gda-handler-boolean.h"
#include "handlers/gda-handler-numerical.h"
#include "handlers/gda-handler-string.h"
#include "handlers/gda-handler-time.h"
#include "handlers/gda-handler-type.h"

static GRecMutex init_rmutex;
#define MUTEX_LOCK() g_rec_mutex_lock(&init_rmutex)
#define MUTEX_UNLOCK() g_rec_mutex_unlock(&init_rmutex)
static void gda_data_handler_iface_init (gpointer g_class);

GType
gda_data_handler_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDataHandlerIface),
			(GBaseInitFunc) gda_data_handler_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL,
			0
		};

		MUTEX_LOCK();
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "GdaDataHandler", &info, 0);
			g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
		}
		MUTEX_UNLOCK();
	}
	return type;
}


static void
gda_data_handler_iface_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	MUTEX_LOCK();
	if (! initialized) {
		initialized = TRUE;
	}
	MUTEX_UNLOCK();
}

static gboolean
_accepts_g_type (GdaDataHandler *dh, GType type)
{
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->accepts_g_type)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->accepts_g_type) (dh, type);
	else
		return FALSE;
}

/**
 * gda_data_handler_get_sql_from_value:
 * @dh: an object which implements the #GdaDataHandler interface
 * @value: (allow-none): the value to be converted to a string, or %NULL
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

	if (! value || gda_value_is_null (value))
		return g_strdup ("NULL");

	g_return_val_if_fail (_accepts_g_type (dh, G_VALUE_TYPE (value)), NULL);

	/* Calling the real function with value != NULL and not of type GDA_TYPE_NULL */
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sql_from_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_sql_from_value) (dh, value);
	
	return NULL;
}

/**
 * gda_data_handler_get_str_from_value:
 * @dh: an object which implements the #GdaDataHandler interface
 * @value: (allow-none): the value to be converted to a string, or %NULL
 *
 * Creates a new string which is a "user friendly" representation of the given value
 * (in the user's locale, specially for the dates). If the value is 
 * NULL or is of type GDA_TYPE_NULL, the returned string is a copy of "" (empty string).
 *
 * Returns: (transfer full): the new string, or %NULL if an error occurred
 */
gchar *
gda_data_handler_get_str_from_value (GdaDataHandler *dh, const GValue *value)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);

	if (! value || gda_value_is_null (value))
		return g_strdup ("");

	g_return_val_if_fail (_accepts_g_type (dh, G_VALUE_TYPE (value)), NULL);

	/* Calling the real function with value != NULL and not of type GDA_TYPE_NULL */
	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_str_from_value)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_str_from_value) (dh, value);
	
	return NULL;
}

/**
 * gda_data_handler_get_value_from_sql:
 * @dh: an object which implements the #GdaDataHandler interface
 * @sql: (allow-none): an SQL string, or %NULL
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
	g_return_val_if_fail (_accepts_g_type (dh, type), NULL);

	if (!sql)
		return gda_value_new_null ();

	if (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql)
		return (GDA_DATA_HANDLER_GET_IFACE (dh)->get_value_from_sql) (dh, sql, type);
	
	return NULL;
}


/**
 * gda_data_handler_get_value_from_str:
 * @dh: an object which implements the #GdaDataHandler interface
 * @str: (allow-none): a string or %NULL
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
 * Returns: (transfer full): the new #GValue or %NULL on error
 */
GValue *
gda_data_handler_get_value_from_str (GdaDataHandler *dh, const gchar *str, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (_accepts_g_type (dh, type), NULL);

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
 * Returns: (transfer full): the new #GValue, or %NULL if no such value can be created.
 */
GValue *
gda_data_handler_get_sane_init_value (GdaDataHandler *dh, GType type)
{
	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (_accepts_g_type (dh, type), NULL);

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
	return _accepts_g_type (dh, type);
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

static guint
gtype_hash (gconstpointer key)
{
        return GPOINTER_TO_UINT (key);
}

static gboolean
gtype_equal (gconstpointer a, gconstpointer b)
{
        return (GType) a == (GType) b ? TRUE : FALSE;
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
 * Returns: (transfer none): a #GdaDataHandler which must not be modified or destroyed.
 *
 * Since: 4.2.3
 */
GdaDataHandler *
gda_data_handler_get_default (GType for_type)
{
	static GMutex mutex;
	static GHashTable *hash = NULL;
	GdaDataHandler *dh;

	g_mutex_lock (&mutex);
	if (!hash) {
		hash = g_hash_table_new_full (gtype_hash, gtype_equal,
					      NULL, (GDestroyNotify) g_object_unref);

                g_hash_table_insert (hash, (gpointer) G_TYPE_INT64, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UINT64, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_BINARY, gda_handler_bin_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_BLOB, gda_handler_bin_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_BOOLEAN, gda_handler_boolean_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DATE, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DOUBLE, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_INT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_NUMERIC, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_FLOAT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_SHORT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_USHORT, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_STRING, gda_handler_string_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIME, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) GDA_TYPE_TIMESTAMP, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_DATE_TIME, gda_handler_time_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_CHAR, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UCHAR, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_ULONG, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_LONG, gda_handler_numerical_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_GTYPE, gda_handler_type_new ());
                g_hash_table_insert (hash, (gpointer) G_TYPE_UINT, gda_handler_numerical_new ());
	}
	g_mutex_unlock (&mutex);

	dh = g_hash_table_lookup (hash, (gpointer) for_type);
	return dh;
}
