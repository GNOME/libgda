/*
 * Copyright (C) 2010 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2019 Daniel Espinsa <esodan@gmail.com>
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

#include "gda-mysql-handler-boolean.h"
#include <glib/gi18n-lib.h>
#include <string.h>

static void gda_mysql_handler_boolean_class_init (GdaMysqlHandlerBooleanClass * class);
static void gda_mysql_handler_boolean_init (GdaMysqlHandlerBoolean * wid);
static void gda_mysql_handler_boolean_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_mysql_handler_boolean_data_handler_init      (GdaDataHandlerInterface *iface);
static gchar       *gda_mysql_handler_boolean_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_mysql_handler_boolean_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_mysql_handler_boolean_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
								      GType type);
static GValue      *gda_mysql_handler_boolean_get_value_from_str     (GdaDataHandler *iface, const gchar *str, 
								      GType type);

static GValue      *gda_mysql_handler_boolean_get_sane_init_value    (GdaDataHandler * dh, GType type);

static gboolean     gda_mysql_handler_boolean_accepts_g_type         (GdaDataHandler * dh, GType type);

static const gchar *gda_mysql_handler_boolean_get_descr              (GdaDataHandler *dh);

typedef struct {
	gchar dummy;
} GdaMysqlHandlerBooleanPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaMysqlHandlerBoolean, gda_mysql_handler_boolean, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaMysqlHandlerBoolean)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, gda_mysql_handler_boolean_data_handler_init))

static void
gda_mysql_handler_boolean_data_handler_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_mysql_handler_boolean_get_sql_from_value;
	iface->get_str_from_value = gda_mysql_handler_boolean_get_str_from_value;
	iface->get_value_from_sql = gda_mysql_handler_boolean_get_value_from_sql;
	iface->get_value_from_str = gda_mysql_handler_boolean_get_value_from_str;
	iface->get_sane_init_value = gda_mysql_handler_boolean_get_sane_init_value;
	iface->accepts_g_type = gda_mysql_handler_boolean_accepts_g_type;
	iface->get_descr = gda_mysql_handler_boolean_get_descr;
}


static void
gda_mysql_handler_boolean_class_init (GdaMysqlHandlerBooleanClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	object_class->dispose = gda_mysql_handler_boolean_dispose;
}

static void
gda_mysql_handler_boolean_init (GdaMysqlHandlerBoolean *hdl)
{
	/* Private structure */
	g_object_set_data (G_OBJECT (hdl), "name", "MySQLBoolean");
	g_object_set_data (G_OBJECT (hdl), "descr", _("MySQL boolean representation"));
}

static void
gda_mysql_handler_boolean_dispose (GObject *object)
{
	/* for the parent class */
	G_OBJECT_CLASS (gda_mysql_handler_boolean_parent_class)->dispose (object);
}

/**
 * gda_mysql_handler_boolean_new
 *
 * Creates a data handler for booleans
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_mysql_handler_boolean_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_MYSQL_HANDLER_BOOLEAN, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_mysql_handler_boolean_get_sql_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);
	return g_strdup (g_value_get_boolean (value) ? "1" : "0");
}

static gchar *
gda_mysql_handler_boolean_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);
	return g_strdup (g_value_get_boolean (value) ? "1" : "0");
}

static GValue *
gda_mysql_handler_boolean_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
	g_assert (sql);

	GValue *value;
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	if (*sql == '0')
		g_value_set_boolean (value, FALSE);
	else
		g_value_set_boolean (value, TRUE);
	return value;
}

static GValue *
gda_mysql_handler_boolean_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
	g_assert (str);

	GValue *value;
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	if (*str == '0')
		g_value_set_boolean (value, FALSE);
	else
		g_value_set_boolean (value, TRUE);

	return value;
}


static GValue *
gda_mysql_handler_boolean_get_sane_init_value (G_GNUC_UNUSED GdaDataHandler *iface, G_GNUC_UNUSED GType type)
{
	GValue *value;

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	g_value_set_boolean (value, FALSE);

	return value;
}

static gboolean
gda_mysql_handler_boolean_accepts_g_type (G_GNUC_UNUSED GdaDataHandler *iface, GType type)
{
	g_assert (iface);
	return type == G_TYPE_BOOLEAN ? TRUE : FALSE;
}

static const gchar *
gda_mysql_handler_boolean_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}
