/* gda-handler-string.c
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

#include "gda-handler-string.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>

static void gda_handler_string_class_init (GdaHandlerStringClass * class);
static void gda_handler_string_init (GdaHandlerString * wid);
static void gda_handler_string_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_string_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_string_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_handler_string_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_handler_string_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
							       GType type);
static GValue      *gda_handler_string_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
							       GType type);
static GValue      *gda_handler_string_get_sane_init_value    (GdaDataHandler * dh, GType type);

static guint        gda_handler_string_get_nb_gda_types       (GdaDataHandler *dh);
static GType        gda_handler_string_get_gda_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_string_accepts_gda_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_string_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerStringPriv {
	gchar          *detailled_descr;
	guint           nb_gda_types;
	GType          *valid_gda_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_string_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaHandlerStringClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_string_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerString),
			0,
			(GInstanceInitFunc) gda_handler_string_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_string_data_handler_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaHandlerString", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
	}
	return type;
}

static void
gda_handler_string_data_handler_init (GdaDataHandlerIface *iface)
{
	iface->get_sql_from_value = gda_handler_string_get_sql_from_value;
	iface->get_str_from_value = gda_handler_string_get_str_from_value;
	iface->get_value_from_sql = gda_handler_string_get_value_from_sql;
	iface->get_value_from_str = gda_handler_string_get_value_from_str;
	iface->get_sane_init_value = gda_handler_string_get_sane_init_value;
	iface->get_nb_gda_types = gda_handler_string_get_nb_gda_types;
	iface->accepts_gda_type = gda_handler_string_accepts_gda_type;
	iface->get_gda_type_index = gda_handler_string_get_gda_type_index;
	iface->get_descr = gda_handler_string_get_descr;
}


static void
gda_handler_string_class_init (GdaHandlerStringClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_handler_string_dispose;
}

static void
gda_handler_string_init (GdaHandlerString * hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaHandlerStringPriv, 1);
	hdl->priv->detailled_descr = _("Strings handler");
	hdl->priv->nb_gda_types = 1;
	hdl->priv->valid_gda_types = g_new0 (GType, 1);
	hdl->priv->valid_gda_types[0] = G_TYPE_STRING;

	gda_object_set_name (GDA_OBJECT (hdl), _("InternalString"));
	gda_object_set_description (GDA_OBJECT (hdl), _("Strings representation"));
}

static void
gda_handler_string_dispose (GObject   * object)
{
	GdaHandlerString *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_STRING (object));

	hdl = GDA_HANDLER_STRING (object);

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
 * gda_handler_string_new
  *
 * Creates a data handler for strings
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_string_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_STRING, "dict", NULL, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_string_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *str, *str2, *retval;
	GdaHandlerString *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	str = gda_value_stringify ((GValue *) value);
	if (str) {
		str2 = gda_default_escape_chars (str);
		retval = g_strdup_printf ("'%s'", str2);
		g_free (str2);
		g_free (str);
	}
	else
		retval = g_strdup ("''");

	return retval;
}

static gchar *
gda_handler_string_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	GdaHandlerString *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_handler_string_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GdaHandlerString *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (sql && *sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			gchar *unstr;

			str[i-1] = 0;
			unstr = gda_default_unescape_chars (str+1);
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
gda_handler_string_get_value_from_str (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GdaHandlerString *hdl;
	GValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
	g_value_set_string (value, sql);
	return value;
}

static GValue *
gda_handler_string_get_sane_init_value (GdaDataHandler *iface, GType type)
{
	GdaHandlerString *hdl;
	GValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
	g_value_set_string (value, "");

	return value;
}

static guint
gda_handler_string_get_nb_gda_types (GdaDataHandler *iface)
{
	GdaHandlerString *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), 0);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_gda_types;
}


static gboolean
gda_handler_string_accepts_gda_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerString *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_gda_types)) {
		if (hdl->priv->valid_gda_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GType
gda_handler_string_get_gda_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerString *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), G_TYPE_INVALID);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, G_TYPE_INVALID);
	g_return_val_if_fail (index < hdl->priv->nb_gda_types, G_TYPE_INVALID);

	return hdl->priv->valid_gda_types[index];
}

static const gchar *
gda_handler_string_get_descr (GdaDataHandler *iface)
{
	GdaHandlerString *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_STRING (iface), NULL);
	hdl = GDA_HANDLER_STRING (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_object_get_description (GDA_OBJECT (hdl));
}
