/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
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

#include "gda-handler-string.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/gda-server-provider.h>

struct _GdaHandlerString
{
  GObject            parent_instance;

  GdaServerProvider *prov;
  GdaConnection     *cnc;
};

static void data_handler_iface_init (GdaDataHandlerInterface *iface);

G_DEFINE_TYPE_EXTENDED (GdaHandlerString, gda_handler_string, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, data_handler_iface_init))

/**
 * gda_handler_string_new:
 *
 * Creates a data handler for strings
 *
 * Returns: (type GdaHandlerString) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_string_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_STRING, NULL);

	return (GdaDataHandler *) obj;
}

/**
 * gda_handler_string_new_with_provider:
 * @prov: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 *
 * Creates a data handler for strings, which will use some specific methods implemented
 * by the @prov object (possibly also @cnc).
 *
 * Returns: (type GdaHandlerString) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_string_new_with_provider (GdaServerProvider *prov, GdaConnection *cnc)
{
	GObject *obj;
	GdaHandlerString *dh;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	obj = g_object_new (GDA_TYPE_HANDLER_STRING, NULL);
	dh = (GdaHandlerString*) obj;

	dh->prov = prov;
	if (cnc)
		dh->cnc = cnc;

	g_object_add_weak_pointer (G_OBJECT (prov), (gpointer) &(dh->prov));
	if (cnc)
		g_object_add_weak_pointer (G_OBJECT (cnc), (gpointer) &(dh->cnc));

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_string_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *str, *retval;
	GdaHandlerString *hdl;

	g_return_val_if_fail (GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = (GdaHandlerString*) (iface);

	str = gda_value_stringify ((GValue *) value);
	if (str) {
		gchar *str2;
		if (hdl->prov) 
			str2 = gda_server_provider_escape_string (hdl->prov, hdl->cnc, str);
		else 
			str2 = gda_default_escape_string (str);
		retval = g_strdup_printf ("'%s'", str2);
		g_free (str2);
		g_free (str);
	}
	else
		retval = g_strdup ("NULL");

	return retval;
}

static gchar *
gda_handler_string_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);
	return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_handler_string_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
	g_assert (sql);

	GdaHandlerString *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = (GdaHandlerString*) (iface);

	if (*sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			gchar *unstr;

			str[i-1] = 0;
			if (hdl->prov)
				unstr = gda_server_provider_unescape_string (hdl->prov, hdl->cnc, str+1);
			else
				unstr = gda_default_unescape_string (str+1);
			if (unstr) {
				value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
				g_value_take_string (value, unstr);
			}
			g_free (str);
		}
	}
	else
		value = gda_value_new_null ();

	return value;
}

static GValue *
gda_handler_string_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
	g_assert (str);

	GValue *value;
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
	g_value_set_string (value, str);
	return value;
}

static GValue *
gda_handler_string_get_sane_init_value (G_GNUC_UNUSED GdaDataHandler *iface, G_GNUC_UNUSED GType type)
{
	GValue *value;

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
	g_value_set_string (value, "");
	return value;
}

static gboolean
gda_handler_string_accepts_g_type (GdaDataHandler *iface, GType type)
{
	g_assert (iface);
	return type == G_TYPE_STRING ? TRUE : FALSE;
}

static const gchar *
gda_handler_string_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_HANDLER_STRING (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}

static void
gda_handler_string_init (GdaHandlerString * hdl)
{
	/* Private structure */
	g_object_set_data (G_OBJECT (hdl), "name", "InternalString");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Strings representation"));
}

static void
gda_handler_string_dispose (GObject   *object)
{
	GdaHandlerString *hdl = (GdaHandlerString *)object;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_STRING (object));

	hdl = GDA_HANDLER_STRING (object);

	if (hdl->prov)
		g_object_remove_weak_pointer (G_OBJECT (hdl->prov), (gpointer) &(hdl->prov));
	if (hdl->cnc)
		g_object_remove_weak_pointer (G_OBJECT (hdl->cnc), (gpointer) &(hdl->cnc));

	/* for the parent class */
	G_OBJECT_CLASS (gda_handler_string_parent_class)->dispose (object);
}

static void
data_handler_iface_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_handler_string_get_sql_from_value;
	iface->get_str_from_value = gda_handler_string_get_str_from_value;
	iface->get_value_from_sql = gda_handler_string_get_value_from_sql;
	iface->get_value_from_str = gda_handler_string_get_value_from_str;
	iface->get_sane_init_value = gda_handler_string_get_sane_init_value;
	iface->accepts_g_type = gda_handler_string_accepts_g_type;
	iface->get_descr = gda_handler_string_get_descr;
}

static void
gda_handler_string_class_init (GdaHandlerStringClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gda_handler_string_dispose;
}
