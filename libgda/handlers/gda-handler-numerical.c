/* gda-handler-numerical.c
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

#include "gda-handler-numerical.h"
#include <locale.h>
#include <glib/gi18n-lib.h>

static void gda_handler_numerical_class_init (GdaHandlerNumericalClass * class);
static void gda_handler_numerical_init (GdaHandlerNumerical * wid);
static void gda_handler_numerical_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_numerical_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_numerical_get_sql_from_value     (GdaDataHandler *dh, const GdaValue *value);
static gchar       *gda_handler_numerical_get_str_from_value     (GdaDataHandler *dh, const GdaValue *value);
static GdaValue    *gda_handler_numerical_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql,
								       GdaValueType type);
static GdaValue    *gda_handler_numerical_get_value_from_str     (GdaDataHandler *dh, const gchar *str,
								       GdaValueType type);
static GdaValue    *gda_handler_numerical_get_sane_init_value    (GdaDataHandler * dh, GdaValueType type);

static guint        gda_handler_numerical_get_nb_gda_types       (GdaDataHandler *dh);
static GdaValueType gda_handler_numerical_get_gda_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_numerical_accepts_gda_type       (GdaDataHandler * dh, GdaValueType type);

static const gchar *gda_handler_numerical_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerNumericalPriv {
	gchar          *detailled_descr;
	guint           nb_gda_types;
	GdaValueType   *valid_gda_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_numerical_get_type (void)
{
	static GType type = 0;

	if (!type) {
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
	iface->get_nb_gda_types = gda_handler_numerical_get_nb_gda_types;
	iface->accepts_gda_type = gda_handler_numerical_accepts_gda_type;
	iface->get_gda_type_index = gda_handler_numerical_get_gda_type_index;
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
	hdl->priv->nb_gda_types = 11;
	hdl->priv->valid_gda_types = g_new0 (GdaValueType, 11);
	hdl->priv->valid_gda_types[0] = GDA_VALUE_TYPE_BIGINT;
	hdl->priv->valid_gda_types[1] = GDA_VALUE_TYPE_DOUBLE;
	hdl->priv->valid_gda_types[2] = GDA_VALUE_TYPE_INTEGER;
        hdl->priv->valid_gda_types[3] = GDA_VALUE_TYPE_NUMERIC;
        hdl->priv->valid_gda_types[4] = GDA_VALUE_TYPE_SINGLE;
        hdl->priv->valid_gda_types[5] = GDA_VALUE_TYPE_SMALLINT;
        hdl->priv->valid_gda_types[6] = GDA_VALUE_TYPE_TINYINT;
	hdl->priv->valid_gda_types[7] = GDA_VALUE_TYPE_BIGUINT;
        hdl->priv->valid_gda_types[8] = GDA_VALUE_TYPE_SMALLUINT;
        hdl->priv->valid_gda_types[9] = GDA_VALUE_TYPE_TINYUINT;
	hdl->priv->valid_gda_types[10] = GDA_VALUE_TYPE_UINTEGER;

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

		g_free (hdl->priv->valid_gda_types);
		hdl->priv->valid_gda_types = NULL;

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
gda_handler_numerical_get_sql_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	gchar *str, *retval;
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	setlocale (LC_NUMERIC, "C");
	str = gda_value_stringify (value);
	setlocale (LC_NUMERIC, "");
	if (str) 
		retval = str;
	else
		retval = g_strdup ("0");

	return retval;
}

static gchar *
gda_handler_numerical_get_str_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_value_stringify (value);
}

static GdaValue *
gda_handler_numerical_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GdaValueType type)
{
	GdaValue *value;

	setlocale (LC_NUMERIC, "C");
	value = gda_handler_numerical_get_value_from_str (iface, sql, type);
	setlocale (LC_NUMERIC, "");

	return value;
}

static GdaValue *
gda_handler_numerical_get_value_from_str (GdaDataHandler *iface, const gchar *str, GdaValueType type)
{
	GdaHandlerNumerical *hdl;
	GdaValue *value = NULL;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	switch (type) {
	case GDA_VALUE_TYPE_BIGINT:
		value = gda_value_new_bigint (atoll (str));
		break;
	case GDA_VALUE_TYPE_DOUBLE:
		value = gda_value_new_double (atof (str));
		break;
	case GDA_VALUE_TYPE_INTEGER:
		value = gda_value_new_integer (atoi (str));
		break;
	case GDA_VALUE_TYPE_NUMERIC: {
		GdaNumeric numeric;
		gchar *p = str;
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
			numeric.number = str;
			value = gda_value_new_numeric (&numeric);
		}
		break;
	}
	case GDA_VALUE_TYPE_SINGLE:
		value = gda_value_new_single (atof (str));
		break;
	case GDA_VALUE_TYPE_SMALLINT:
		value = gda_value_new_smallint (atoi (str));
		break;
	case GDA_VALUE_TYPE_TINYINT:
		value = gda_value_new_tinyint (atoi (str));
		break;
	case GDA_VALUE_TYPE_BIGUINT:
		value = gda_value_new_biguint (strtoull (str, NULL, 10));
		break;
	case GDA_VALUE_TYPE_SMALLUINT:
		value = gda_value_new_smalluint (atoi (str));
		break;
        case GDA_VALUE_TYPE_TINYUINT:
		value = gda_value_new_tinyuint (atoi (str));
		break;
	case GDA_VALUE_TYPE_UINTEGER:
		value = gda_value_new_uinteger (strtoul (str, NULL, 10));		
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return value;
}


static GdaValue *
gda_handler_numerical_get_sane_init_value (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerNumerical *hdl;
	GdaValue *value;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	value = gda_handler_numerical_get_value_from_sql (iface, "", type);
	return value;
}


static guint
gda_handler_numerical_get_nb_gda_types (GdaDataHandler *iface)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), 0);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_gda_types;
}


static gboolean
gda_handler_numerical_accepts_gda_type (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerNumerical *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), FALSE);
	g_return_val_if_fail (type != GDA_VALUE_TYPE_UNKNOWN, FALSE);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_gda_types)) {
		if (hdl->priv->valid_gda_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GdaValueType
gda_handler_numerical_get_gda_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerNumerical *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_NUMERICAL (iface), GDA_VALUE_TYPE_UNKNOWN);
	hdl = GDA_HANDLER_NUMERICAL (iface);
	g_return_val_if_fail (hdl->priv, GDA_VALUE_TYPE_UNKNOWN);
	g_return_val_if_fail (index < hdl->priv->nb_gda_types, GDA_VALUE_TYPE_UNKNOWN);

	return hdl->priv->valid_gda_types[index];
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
