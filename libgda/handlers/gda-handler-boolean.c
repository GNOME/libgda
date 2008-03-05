/* gda-handler-boolean.c
 *
 * Copyright (C) 2003 - 2008 Vivien Malerba
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

#include "gda-handler-boolean.h"
#include <glib/gi18n-lib.h>
#include <string.h>

static void gda_handler_boolean_class_init (GdaHandlerBooleanClass * class);
static void gda_handler_boolean_init (GdaHandlerBoolean * wid);
static void gda_handler_boolean_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_boolean_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_boolean_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_handler_boolean_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_handler_boolean_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
								GType type);
static GValue      *gda_handler_boolean_get_value_from_str     (GdaDataHandler *iface, const gchar *str, 
								GType type);

static GValue      *gda_handler_boolean_get_sane_init_value    (GdaDataHandler * dh, GType type);

static guint        gda_handler_boolean_get_nb_g_types       (GdaDataHandler *dh);
static GType        gda_handler_boolean_get_g_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_boolean_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_boolean_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerBooleanPriv {
	gchar          *detailled_descr;
	guint           nb_g_types;
	GType          *valid_g_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_boolean_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaHandlerBooleanClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_boolean_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerBoolean),
			0,
			(GInstanceInitFunc) gda_handler_boolean_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_boolean_data_handler_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT, "GdaHandlerBoolean", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
	}
	return type;
}

static void
gda_handler_boolean_data_handler_init (GdaDataHandlerIface *iface)
{
	iface->get_sql_from_value = gda_handler_boolean_get_sql_from_value;
	iface->get_str_from_value = gda_handler_boolean_get_str_from_value;
	iface->get_value_from_sql = gda_handler_boolean_get_value_from_sql;
	iface->get_value_from_str = gda_handler_boolean_get_value_from_str;
	iface->get_sane_init_value = gda_handler_boolean_get_sane_init_value;
	iface->get_nb_g_types = gda_handler_boolean_get_nb_g_types;
	iface->accepts_g_type = gda_handler_boolean_accepts_g_type;
	iface->get_g_type_index = gda_handler_boolean_get_g_type_index;
	iface->get_descr = gda_handler_boolean_get_descr;
}


static void
gda_handler_boolean_class_init (GdaHandlerBooleanClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_handler_boolean_dispose;
}

static void
gda_handler_boolean_init (GdaHandlerBoolean *hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaHandlerBooleanPriv, 1);
	hdl->priv->detailled_descr = _("Boolean values handler");
	hdl->priv->nb_g_types = 1;
	hdl->priv->valid_g_types = g_new0 (GType, 1);
	hdl->priv->valid_g_types[0] = G_TYPE_BOOLEAN;

	g_object_set_data (G_OBJECT (hdl), "name", _("InternalBoolean"));
	g_object_set_data (G_OBJECT (hdl), "descr", _("Boolean representation"));
}

static void
gda_handler_boolean_dispose (GObject *object)
{
	GdaHandlerBoolean *hdl;

	g_return_if_fail (GDA_IS_HANDLER_BOOLEAN (object));

	hdl = GDA_HANDLER_BOOLEAN (object);

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
 * gda_handler_boolean_new
 *
 * Creates a data handler for booleans
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_boolean_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_BOOLEAN, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_boolean_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (g_value_get_boolean (value)) 
		retval = g_strdup ("TRUE");
	else
		retval = g_strdup ("FALSE");

	return retval;
}

static gchar *
gda_handler_boolean_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_handler_boolean_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GdaHandlerBoolean *hdl;
	GValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	if ((*sql == 't') || (*sql == 'T'))
		g_value_set_boolean (value, TRUE);
	else
		g_value_set_boolean (value, FALSE);
	return value;
}

static GValue *
gda_handler_boolean_get_value_from_str (GdaDataHandler *iface, const gchar *str, GType type)
{
	GdaHandlerBoolean *hdl;
	GValue *value = NULL;
	gchar *lcstr;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	lcstr = g_utf8_strdown (str, -1);
	if (!strcmp (lcstr, "true") || (*lcstr == 't'))
		g_value_set_boolean (value, TRUE);
	if (!value && (!strcmp (lcstr, "FALSE") || (*lcstr == 'f')))
		g_value_set_boolean (value, FALSE);
	g_free (lcstr);

	if (! G_IS_VALUE (value)) {
		g_value_set_boolean (value, TRUE);
		lcstr = gda_value_stringify (value);
		if (strcmp (str, lcstr))
			g_value_set_boolean (value, FALSE);
	}

	return value;
}


static GValue *
gda_handler_boolean_get_sane_init_value (GdaDataHandler *iface, GType type)
{
	GdaHandlerBoolean *hdl;
	GValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_BOOLEAN);
	g_value_set_boolean (value, FALSE);

	return value;
}

static guint
gda_handler_boolean_get_nb_g_types (GdaDataHandler *iface)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), 0);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_g_types;
}


static gboolean
gda_handler_boolean_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerBoolean *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_g_types)) {
		if (hdl->priv->valid_g_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GType
gda_handler_boolean_get_g_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), G_TYPE_INVALID);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, G_TYPE_INVALID);
	g_return_val_if_fail (index < hdl->priv->nb_g_types, G_TYPE_INVALID);

	return hdl->priv->valid_g_types[index];
}

static const gchar *
gda_handler_boolean_get_descr (GdaDataHandler *iface)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return g_object_get_data (G_OBJECT (hdl), "descr");
}
