/*
 * Copyright (C) 2006 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include "gda-handler-boolean.h"
#include <glib/gi18n-lib.h>
#include <string.h>

struct _GdaHandlerBoolean
{
	GObject   parent_instance;
};

static void data_handler_iface_init (GdaDataHandlerInterface *iface);

G_DEFINE_TYPE_EXTENDED (GdaHandlerBoolean, gda_handler_boolean, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, data_handler_iface_init))

/**
 * gda_handler_boolean_new:
 *
 * Creates a data handler for booleans
 *
 * Returns: (type GdaHandlerBoolean) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_boolean_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_BOOLEAN, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_boolean_get_sql_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	if (g_value_get_boolean (value)) 
		retval = g_strdup ("TRUE");
	else
		retval = g_strdup ("FALSE");

	return retval;
}

static gchar *
gda_handler_boolean_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);
	return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_handler_boolean_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
	g_assert (sql);

	GValue *value;
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	if ((*sql == 't') || (*sql == 'T'))
		g_value_set_boolean (value, TRUE);
	else
		g_value_set_boolean (value, FALSE);

	return value;
}

static GValue *
gda_handler_boolean_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
	g_assert (str);

	GValue *value;
	gchar *lcstr;
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	lcstr = g_utf8_strdown (str, -1);
	if (!strcmp (lcstr, "true") || (*lcstr == 't'))
		g_value_set_boolean (value, TRUE);
	g_free (lcstr);

	return value;
}


static GValue *
gda_handler_boolean_get_sane_init_value (G_GNUC_UNUSED GdaDataHandler *iface, G_GNUC_UNUSED GType type)
{
	GValue *value;

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	g_value_set_boolean (value, FALSE);

	return value;
}

static gboolean
gda_handler_boolean_accepts_g_type (G_GNUC_UNUSED GdaDataHandler *iface, GType type)
{
	g_assert (iface);
	return type == G_TYPE_BOOLEAN ? TRUE : FALSE;
}

static const gchar *
gda_handler_boolean_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}

static void
gda_handler_boolean_init (GdaHandlerBoolean *hdl)
{
	g_object_set_data (G_OBJECT (hdl), "name", "InternalBoolean");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Boolean representation"));
}

static void
gda_handler_boolean_dispose (GObject *object)
{
	g_return_if_fail (GDA_IS_HANDLER_BOOLEAN (object));

	/* for the parent class */
	G_OBJECT_CLASS (gda_handler_boolean_parent_class)->dispose (object);
}

static void
data_handler_iface_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_handler_boolean_get_sql_from_value;
	iface->get_str_from_value = gda_handler_boolean_get_str_from_value;
	iface->get_value_from_sql = gda_handler_boolean_get_value_from_sql;
	iface->get_value_from_str = gda_handler_boolean_get_value_from_str;
	iface->get_sane_init_value = gda_handler_boolean_get_sane_init_value;
	iface->accepts_g_type = gda_handler_boolean_accepts_g_type;
	iface->get_descr = gda_handler_boolean_get_descr;
}

static void
gda_handler_boolean_class_init (GdaHandlerBooleanClass * class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gda_handler_boolean_dispose;
}
