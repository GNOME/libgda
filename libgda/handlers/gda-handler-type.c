/*
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include "gda-handler-type.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gda-util.h>

struct _GdaHandlerType
{
	GObject   parent_instance;
};

static void data_handler_iface_init (GdaDataHandlerInterface *iface);

G_DEFINE_TYPE_EXTENDED (GdaHandlerType, gda_handler_type, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, data_handler_iface_init))

/**
 * gda_handler_type_new:
 *
 * Creates a data handler for Gda types
 *
 * Returns: (type GdaHandlerType) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_type_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_TYPE, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_type_get_sql_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	GTypeQuery tq;
	g_type_query (g_value_get_gtype (value), &tq);
	if (tq.type != 0) {
		const gchar *str;
		str = gda_g_type_to_string (g_value_get_gtype (value));
		retval = g_strdup_printf ("'%s'", str);
	}
	else
		retval = g_strdup ("NULL");	

	return retval;
}

static gchar *
gda_handler_type_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	GTypeQuery tq;
       
	g_type_query (g_value_get_gtype (value), &tq);
	if (tq.type != 0)
		retval = g_strdup (gda_g_type_to_string (g_value_get_gtype (value)));
	else
		retval = NULL;

	return retval;
}

static GValue *
gda_handler_type_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
	g_assert (sql);

	GValue *value = NULL;
	if (*sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			GType type;
			str[i-1] = 0;
			type = gda_g_type_from_string (str+1);
			g_free (str);
			if (type != G_TYPE_INVALID) {
				value = g_value_init (g_new0 (GValue, 1), G_TYPE_GTYPE);
				g_value_set_gtype (value, type);
			}
		}
	}
	else
		value = gda_value_new_null ();
	return value;
}

static GValue *
gda_handler_type_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
	g_assert (str);

	GValue *value = NULL;
	GType vtype;

	vtype = gda_g_type_from_string (str);
	if (vtype != G_TYPE_INVALID) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_GTYPE);
		g_value_set_gtype (value, vtype);
	}

	return value;
}

static gboolean
gda_handler_type_accepts_g_type (GdaDataHandler *iface, GType type)
{
	g_assert (iface);
	return type == G_TYPE_GTYPE ? TRUE : FALSE;
}

static const gchar *
gda_handler_type_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_HANDLER_TYPE (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}

static void
gda_handler_type_init (GdaHandlerType * hdl)
{
	g_object_set_data (G_OBJECT (hdl), "descr", "InternalType");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Gda type representation"));
}

static void
data_handler_iface_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_handler_type_get_sql_from_value;
	iface->get_str_from_value = gda_handler_type_get_str_from_value;
	iface->get_value_from_sql = gda_handler_type_get_value_from_sql;
	iface->get_value_from_str = gda_handler_type_get_value_from_str;
	iface->get_sane_init_value = NULL;
	iface->accepts_g_type = gda_handler_type_accepts_g_type;
	iface->get_descr = gda_handler_type_get_descr;
}


static void
gda_handler_type_class_init (GdaHandlerTypeClass * class)
{
	
}
