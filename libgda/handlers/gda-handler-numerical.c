/* gda-handler-numerical.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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

#include "gda-handler-numerical.h"
#include <locale.h>
#include <glib/gi18n-lib.h>

static void gda_handler_numerical_class_init (GdaHandlerNumericalClass * class);
static void gda_handler_numerical_init (GdaHandlerNumerical * wid);
static void gda_handler_numerical_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_numerical_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_numerical_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_handler_numerical_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_handler_numerical_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql,
								  GType type);
static GValue      *gda_handler_numerical_get_value_from_str     (GdaDataHandler *dh, const gchar *str,
								  GType type);
static GValue      *gda_handler_numerical_get_sane_init_value    (GdaDataHandler * dh, GType type);

static guint        gda_handler_numerical_get_nb_g_types       (GdaDataHandler *dh);
static GType        gda_handler_numerical_get_g_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_numerical_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_numerical_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerNumericalPriv {
	gchar          *detailled_descr;
	guint           nb_g_types;
	GType          *valid_g_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_numerical_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaHandlerNumericalClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_numerical_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerNumerical),
			0,
			(GInstanceInitFunc) gda_handler_numerical_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_numerical_data_handler_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaHandlerNumerical", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
	}
	return type;
}

static void
gda_handler_numerical_data_handler_init (GdaDataHandlerIface *iface)
{
	iface->get_sql_from_value = gda_handler_numerical_get_sql_from_value;
	iface->get_str_from_value = gda_handler_numerical_get_str_from_value;
	iface->get_value_from_sql = gda_handler_numerical_get_value_from_sql;
	iface->get_value_from_str = gda_handler_numerical_get_value_from_str;
	iface->get_sane_init_value = gda_handler_numerical_get_sane_init_value;
	iface->get_nb_g_types = gda_handler_numerical_get_nb_g_types;
	iface->accepts_g_type = gda_handler_numerical_accepts_g_type;
	iface->get_g_type_index = gda_handler_numerical_get_g_type_index;
	iface->get_descr = gda_handler_numerical_get_descr;
}


static void
gda_handler_numerical_class_init (GdaHandlerNumericalClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_handler_numerical_dispose;
}

static void
gda_handler_numerical_init (GdaHandlerNumerical * hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaHandlerNumericalPriv, 1);
	hdl->priv->detailled_descr = "";
	hdl->priv->nb_g_types = 11;
	hdl->priv->valid_g_types = g_new0 (GType, 11);
	hdl->priv->valid_g_types[0] = G_TYPE_INT64;
	hdl->priv->valid_g_types[1] = G_TYPE_DOUBLE;
	hdl->priv->valid_g_types[2] = G_TYPE_INT;
        hdl->priv->valid_g_types[3] = GDA_TYPE_NUMERIC;
        hdl->priv->valid_g_types[4] = G_TYPE_FLOAT;
        hdl->priv->valid_g_types[5] = GDA_TYPE_SHORT;
        hdl->priv->valid_g_types[6] = G_TYPE_CHAR;
	hdl->priv->valid_g_types[7] = G_TYPE_UINT64;
        hdl->priv->valid_g_types[8] = GDA_TYPE_USHORT;
        hdl->priv->valid_g_types[9] = G_TYPE_UCHAR;
	hdl->priv->valid_g_types[10] = G_TYPE_UINT;

	gda_object_set_name (GDA_OBJECT (hdl), _("InternalNumerical"));
	gda_object_set_description (GDA_OBJECT (hdl), _("Numerical representation"));
}

static void
gda_handler_numerical_dispose (GObject *object)
{
	GdaHandlerNumerical *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_NUMERICAL (object));

	hdl = GDA_HANDLER_NUMERICAL (object);

	if (hdl->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		g_free (hdl->priv->valid_g_types);
		hdl->priv->valid_g_types = NULL;

		g_free (hdl->priv);
		hdl->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

/**
 * gda_handler_numerical_new
 *
 * Creates a data handler for numerical values
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_numerical_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_NUMERICAL, "dict", NULL, NULL);

	return (GdaDataHandler *) obj;
}


/* Interface implementation */
static gchar *
gda_handler_numerical_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *str, *retval;
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	setlocale (LC_NUMERIC, "C");
	str = gda_value_stringify ((GValue *) value);
	setlocale (LC_NUMERIC, "");
	if (str) 
		retval = str;
	else
		retval = g_strdup ("0");

	return retval;
}

static gchar *
gda_handler_numerical_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_handler_numerical_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GValue *value;

	setlocale (LC_NUMERIC, "C");
	value = gda_handler_numerical_get_value_from_str (iface, sql, type);
	setlocale (LC_NUMERIC, "");

	return value;
}

static GValue *
gda_handler_numerical_get_value_from_str (GdaDataHandler *iface, const gchar *str, GType type)
{
	GdaHandlerNumerical *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (type == G_TYPE_INT64) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_INT64);
		g_value_set_int64 (value, atoll (str));
	}
	else if (type == G_TYPE_DOUBLE) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_DOUBLE);
		g_value_set_double (value, atof (str));
	}
	else if (type == G_TYPE_INT) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_INT);
		g_value_set_int (value, atoi (str));
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric numeric;
		const gchar *p = str;
		gboolean ok = TRUE;
		gboolean hasdot = FALSE;
		
		numeric.precision = 0;
		numeric.width = 0;
		while (p && *p && ok) {
			if ((*p == '.') || (*p == ',')) {
				if (hasdot)
					ok = FALSE;
				else
					hasdot = TRUE;
			}
			else {
				if (!g_ascii_isdigit (*p))
					ok = FALSE;
				else {
					if (hasdot)
						numeric.precision++;
					numeric.width++;
				}
			}
			p++;
		}
		if (ok) {
			numeric.number = (gchar *) str;
			value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_NUMERIC);
			gda_value_set_numeric (value, &numeric);
		}
	}
	else if (type == G_TYPE_FLOAT) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_FLOAT);
		g_value_set_float (value, atof (str));
	}
	else if (type == GDA_TYPE_SHORT) {
		value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_SHORT);
		gda_value_set_short (value, atoi (str));
	}
	else if (type == G_TYPE_CHAR) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_CHAR);
		g_value_set_char (value, atoi (str));
	}
	else if (type == G_TYPE_UINT64) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_UINT64);
		g_value_set_uint64 (value, strtoull (str, NULL, 10));
	}
	else if (type == GDA_TYPE_USHORT) {
		value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_USHORT);
		gda_value_set_ushort (value, atoi (str));
	}
	else if (type == G_TYPE_UCHAR) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_UCHAR);
		g_value_set_uchar (value, atoi (str));
	}
	else if (type == G_TYPE_UINT) {
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_UINT);
		g_value_set_uint (value, strtoul (str, NULL, 10));
	}
	else
		g_assert_not_reached ();

	return value;
}


static GValue *
gda_handler_numerical_get_sane_init_value (GdaDataHandler *iface, GType type)
{
	GdaHandlerNumerical *hdl;
	GValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = gda_handler_numerical_get_value_from_sql (iface, "", type);
	return value;
}


static guint
gda_handler_numerical_get_nb_g_types (GdaDataHandler *iface)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), 0);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_g_types;
}


static gboolean
gda_handler_numerical_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerNumerical *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_g_types)) {
		if (hdl->priv->valid_g_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GType
gda_handler_numerical_get_g_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), G_TYPE_INVALID);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, G_TYPE_INVALID);
	g_return_val_if_fail (index < hdl->priv->nb_g_types, G_TYPE_INVALID);

	return hdl->priv->valid_g_types[index];
}

static const gchar *
gda_handler_numerical_get_descr (GdaDataHandler *iface)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_object_get_description (GDA_OBJECT (hdl));
}
