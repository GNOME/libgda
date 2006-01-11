/* gda-handler-boolean.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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

#include "gda-handler-boolean.h"
#include <glib/gi18n-lib.h>
#include <string.h>

static void gda_handler_boolean_class_init (GdaHandlerBooleanClass * class);
static void gda_handler_boolean_init (GdaHandlerBoolean * wid);
static void gda_handler_boolean_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_boolean_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_boolean_get_sql_from_value     (GdaDataHandler *dh, const GdaValue *value);
static gchar       *gda_handler_boolean_get_str_from_value     (GdaDataHandler *dh, const GdaValue *value);
static GdaValue    *gda_handler_boolean_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
								     GdaValueType type);
static GdaValue    *gda_handler_boolean_get_value_from_str     (GdaDataHandler *iface, const gchar *str, 
								     GdaValueType type);

static GdaValue    *gda_handler_boolean_get_sane_init_value    (GdaDataHandler * dh, GdaValueType type);

static guint        gda_handler_boolean_get_nb_gda_types       (GdaDataHandler *dh);
static GdaValueType gda_handler_boolean_get_gda_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_boolean_accepts_gda_type       (GdaDataHandler * dh, GdaValueType type);

static const gchar *gda_handler_boolean_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerBooleanPriv {
	gchar          *detailled_descr;
	guint           nb_gda_types;
	GdaValueType   *valid_gda_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_boolean_get_type (void)
{
	static GType type = 0;

	if (!type) {
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

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaHandlerBoolean", &info, 0);
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
	iface->get_nb_gda_types = gda_handler_boolean_get_nb_gda_types;
	iface->accepts_gda_type = gda_handler_boolean_accepts_gda_type;
	iface->get_gda_type_index = gda_handler_boolean_get_gda_type_index;
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
	hdl->priv->nb_gda_types = 1;
	hdl->priv->valid_gda_types = g_new0 (GdaValueType, 1);
	hdl->priv->valid_gda_types[0] = GDA_VALUE_TYPE_BOOLEAN;

	gda_object_set_name (GDA_OBJECT (hdl), _("InternalBoolean"));
	gda_object_set_description (GDA_OBJECT (hdl), _("Booleans representation"));
}

static void
gda_handler_boolean_dispose (GObject *object)
{
	GdaHandlerBoolean *hdl;

	g_return_if_fail (GDA_IS_HANDLER_BOOLEAN (object));

	hdl = GDA_HANDLER_BOOLEAN (object);

	if (hdl->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		g_free (hdl->priv->valid_gda_types);
		hdl->priv->valid_gda_types = NULL;

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

	obj = g_object_new (GDA_TYPE_HANDLER_BOOLEAN, "dict", NULL, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_boolean_get_sql_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	gchar *retval;
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (gda_value_get_boolean (value)) 
		retval = g_strdup ("TRUE");
	else
		retval = g_strdup ("FALSE");

	return retval;
}

static gchar *
gda_handler_boolean_get_str_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_value_stringify (value);
}

static GdaValue *
gda_handler_boolean_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GdaValueType type)
{
	GdaHandlerBoolean *hdl;
	GdaValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if ((*sql == 't') || (*sql == 'T'))
		value = gda_value_new_boolean (TRUE);
	else
		value = gda_value_new_boolean (FALSE);
	return value;
}

static GdaValue *
gda_handler_boolean_get_value_from_str (GdaDataHandler *iface, const gchar *str, GdaValueType type)
{
	GdaHandlerBoolean *hdl;
	GdaValue *value = NULL;
	gchar *lcstr;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	lcstr = g_utf8_strdown (str, -1);
	if (!strcmp (lcstr, "true") || (*lcstr == 't'))
		value = gda_value_new_boolean (TRUE);
	if (!value && (!strcmp (lcstr, "FALSE") || (*lcstr == 'f')))
		value = gda_value_new_boolean (FALSE);
	g_free (lcstr);

	if (!value) {
		value = gda_value_new_boolean (TRUE);
		lcstr = gda_value_stringify (value);
		if (strcmp (str, lcstr)) {
			gda_value_free (value);
			value = gda_value_new_boolean (FALSE);
		}
	}

	return value;
}


static GdaValue *
gda_handler_boolean_get_sane_init_value (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerBoolean *hdl;
	GdaValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = gda_value_new_boolean (FALSE);
	return value;
}

static guint
gda_handler_boolean_get_nb_gda_types (GdaDataHandler *iface)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), 0);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_gda_types;
}


static gboolean
gda_handler_boolean_accepts_gda_type (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerBoolean *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), FALSE);
	g_return_val_if_fail (type != GDA_VALUE_TYPE_UNKNOWN, FALSE);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_gda_types)) {
		if (hdl->priv->valid_gda_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GdaValueType
gda_handler_boolean_get_gda_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), GDA_VALUE_TYPE_UNKNOWN);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, GDA_VALUE_TYPE_UNKNOWN);
	g_return_val_if_fail (index < hdl->priv->nb_gda_types, GDA_VALUE_TYPE_UNKNOWN);

	return hdl->priv->valid_gda_types[index];
}

static const gchar *
gda_handler_boolean_get_descr (GdaDataHandler *iface)
{
	GdaHandlerBoolean *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BOOLEAN (iface), NULL);
	hdl = GDA_HANDLER_BOOLEAN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_object_get_description (GDA_OBJECT (hdl));
}
