/* gda-mysql-handler-boolean.c
 *
 * Copyright (C) 2010 Vivien Malerba
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

#include "gda-mysql-handler-boolean.h"
#include <glib/gi18n-lib.h>
#include <string.h>

static void gda_mysql_handler_boolean_class_init (GdaMysqlHandlerBooleanClass * class);
static void gda_mysql_handler_boolean_init (GdaMysqlHandlerBoolean * wid);
static void gda_mysql_handler_boolean_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_mysql_handler_boolean_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_mysql_handler_boolean_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_mysql_handler_boolean_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_mysql_handler_boolean_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
								      GType type);
static GValue      *gda_mysql_handler_boolean_get_value_from_str     (GdaDataHandler *iface, const gchar *str, 
								      GType type);

static GValue      *gda_mysql_handler_boolean_get_sane_init_value    (GdaDataHandler * dh, GType type);

static gboolean     gda_mysql_handler_boolean_accepts_g_type         (GdaDataHandler * dh, GType type);

static const gchar *gda_mysql_handler_boolean_get_descr              (GdaDataHandler *dh);

struct  _GdaMysqlHandlerBooleanPriv {
	gchar          *detailed_descr;
	guint           nb_g_types;
	GType          *valid_g_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_mysql_handler_boolean_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaMysqlHandlerBooleanClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_handler_boolean_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlHandlerBoolean),
			0,
			(GInstanceInitFunc) gda_mysql_handler_boolean_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_mysql_handler_boolean_data_handler_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaMysqlHandlerBoolean", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_mysql_handler_boolean_data_handler_init (GdaDataHandlerIface *iface)
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
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_mysql_handler_boolean_dispose;
}

static void
gda_mysql_handler_boolean_init (GdaMysqlHandlerBoolean *hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaMysqlHandlerBooleanPriv, 1);
	hdl->priv->detailed_descr = _("Boolean values handler");
	hdl->priv->nb_g_types = 1;
	hdl->priv->valid_g_types = g_new0 (GType, 1);
	hdl->priv->valid_g_types[0] = G_TYPE_BOOLEAN;

	g_object_set_data (G_OBJECT (hdl), "name", "MySQLBoolean");
	g_object_set_data (G_OBJECT (hdl), "descr", _("MySQL boolean representation"));
}

static void
gda_mysql_handler_boolean_dispose (GObject *object)
{
	GdaMysqlHandlerBoolean *hdl;

	g_return_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (object));

	hdl = GDA_MYSQL_HANDLER_BOOLEAN (object);

	if (hdl->priv) {
		g_free (hdl->priv->valid_g_types);
		hdl->priv->valid_g_types = NULL;

		g_free (hdl->priv);
		hdl->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
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
gda_mysql_handler_boolean_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaMysqlHandlerBoolean *hdl;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (g_value_get_boolean (value)) 
		retval = g_strdup ("1");
	else
		retval = g_strdup ("0");

	return retval;
}

static gchar *
gda_mysql_handler_boolean_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	GdaMysqlHandlerBoolean *hdl;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_mysql_handler_boolean_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GdaMysqlHandlerBoolean *hdl;
	GValue *value;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	if (*sql == '0')
		g_value_set_boolean (value, FALSE);
	else
		g_value_set_boolean (value, TRUE);
	return value;
}

static GValue *
gda_mysql_handler_boolean_get_value_from_str (GdaDataHandler *iface, const gchar *str, GType type)
{
	GdaMysqlHandlerBoolean *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	if (*str == '0')
		g_value_set_boolean (value, FALSE);
	else
		g_value_set_boolean (value, TRUE);

	return value;
}


static GValue *
gda_mysql_handler_boolean_get_sane_init_value (GdaDataHandler *iface, GType type)
{
	GdaMysqlHandlerBoolean *hdl;
	GValue *value;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	g_value_set_boolean (value, FALSE);

	return value;
}

static gboolean
gda_mysql_handler_boolean_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaMysqlHandlerBoolean *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_g_types)) {
		if (hdl->priv->valid_g_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static const gchar *
gda_mysql_handler_boolean_get_descr (GdaDataHandler *iface)
{
	GdaMysqlHandlerBoolean *hdl;

	g_return_val_if_fail (GDA_IS_MYSQL_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_MYSQL_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return g_object_get_data (G_OBJECT (hdl), "descr");
}
