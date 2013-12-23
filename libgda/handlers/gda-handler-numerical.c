/*
 * Copyright (C) 2006 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 - 2011 Murray Cumming <murrayc@murrayc.com>
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

static gboolean     gda_handler_numerical_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_numerical_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerNumericalPriv {
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
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaHandlerNumericalClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_numerical_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerNumerical),
			0,
			(GInstanceInitFunc) gda_handler_numerical_init,
			NULL
		};

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_numerical_data_handler_init,
			NULL,
			NULL
		};

		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaHandlerNumerical", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_mutex_unlock (&registering);
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
	iface->accepts_g_type = gda_handler_numerical_accepts_g_type;
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
	hdl->priv->nb_g_types = 13;
	hdl->priv->valid_g_types = g_new0 (GType, hdl->priv->nb_g_types);
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
	hdl->priv->valid_g_types[11] = G_TYPE_ULONG;
	hdl->priv->valid_g_types[12] = G_TYPE_LONG;

	g_object_set_data (G_OBJECT (hdl), "name", "InternalNumerical");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Numerical representation"));
}

static void
gda_handler_numerical_dispose (GObject *object)
{
	GdaHandlerNumerical *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_NUMERICAL (object));

	hdl = GDA_HANDLER_NUMERICAL (object);

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
 * gda_handler_numerical_new:
 *
 * Creates a data handler for numerical values
 *
 * Returns: (transfer full): the new object
 */
GdaDataHandler *
gda_handler_numerical_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_NUMERICAL, NULL);

	return (GdaDataHandler *) obj;
}


/* Interface implementation */
static gchar *
gda_handler_numerical_get_sql_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	retval = gda_value_stringify ((GValue *) value);

	if (!retval)
		retval = g_strdup ("0");

	return retval;
}

static gchar *
gda_handler_numerical_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);
	return gda_value_stringify ((GValue *) value);
}

/*
 * if @c_locale is %TRUE, then @str is expected to be in the "C" locale, and if it is %FALSE,
 * then @str is expected to be in the current (user defined) locale
 */
static GValue *
_gda_handler_numerical_get_value_from_str_locale (const gchar *str, GType type, gboolean c_locale)
{
	g_assert (str);
	GValue *value = NULL;
	long long int llint;
	char *endptr = NULL;

	if (c_locale)
		llint = g_ascii_strtoull (str, &endptr, 10);
	else
		llint = strtoll (str, &endptr, 10);

	if (type == G_TYPE_INT64) {
		if (!*endptr && (llint >= G_MININT64) && (llint <= G_MAXINT64)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_INT64);
			g_value_set_int64 (value, (gint64) llint);
		}
	}
	else if (type == G_TYPE_DOUBLE) {
		gdouble dble;
		if (c_locale)
			dble = g_ascii_strtod (str, &endptr);
		else
			dble = g_strtod (str, &endptr);
		if (!*endptr) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_DOUBLE);
			g_value_set_double (value, dble);
		}
	}
	else if (type == G_TYPE_FLOAT) {
		gfloat flt;
		if (c_locale)
			flt = g_ascii_strtod (str, &endptr);
		else
			flt = strtof (str, &endptr);
		if (!*endptr) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_FLOAT);
			g_value_set_float (value, flt);
		}
	}
	else if (type == G_TYPE_INT) {
		if (!*endptr && (llint >= G_MININT) && (llint <= G_MAXINT)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_INT);
			g_value_set_int (value, (gint) llint);
		}
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric *numeric;
		const gchar *p = str;
		gboolean ok = TRUE;
		gboolean hasdot = FALSE;

		numeric = gda_numeric_new ();
		gda_numeric_set_precision (numeric, 0);
		gda_numeric_set_width (numeric, 0);
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
						gda_numeric_set_precision (numeric,
									   gda_numeric_get_precision (numeric) + 1);
					gda_numeric_set_width (numeric,
							       gda_numeric_get_width (numeric) + 1);
				}
			}
			p++;
		}
		if (ok) {
			gdouble d;
			char *end = NULL;
			if (c_locale)
				d = g_ascii_strtod (str, &end);
			else
				d = strtod (str, &end);
			if (! *end) {
				value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_NUMERIC);
				gda_numeric_set_double (numeric, d);
				gda_value_set_numeric (value, numeric);
			}
		}
		gda_numeric_free (numeric);
	}
	else if (type == GDA_TYPE_SHORT) {
		if (!*endptr && (llint >= G_MINSHORT) && (llint <= G_MAXSHORT)) {
			value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_SHORT);
			gda_value_set_short (value, (gshort) llint);
		}
	}
	else if (type == G_TYPE_CHAR) {
		if (!*endptr && (llint >= G_MININT8) && (llint <= G_MAXINT8)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_CHAR);
			g_value_set_schar (value, (gchar) llint);
		}
	}
	else if (type == G_TYPE_UINT64) {
		if (!*endptr && (llint >= 0) && (llint <= G_MAXINT64)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_UINT64);
			g_value_set_uint64 (value, (guint64) llint);
		}
		else {
			unsigned long long int lluint;
			lluint = strtoull (str, &endptr, 10);
			if (!*endptr && (lluint <= G_MAXUINT64)) {
				value = g_value_init (g_new0 (GValue, 1), G_TYPE_UINT64);
				g_value_set_uint64 (value, (guint64) lluint);
			}
		}
	}
	else if (type == GDA_TYPE_USHORT) {
		if (!*endptr && (llint >= 0) && (llint <= G_MAXUSHORT)) {
			value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_USHORT);
			gda_value_set_ushort (value, (gushort) llint);
		}
	}
	else if (type == G_TYPE_UCHAR) {
		if (!*endptr && (llint >= 0) && (llint <= G_MAXUINT8)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_UCHAR);
			g_value_set_uchar (value, (guchar) llint);
		}
	}
	else if (type == G_TYPE_UINT) {
		if (!*endptr && (llint >= 0) && (llint <= G_MAXUINT)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_UINT);
			g_value_set_uint (value, (guint) llint);
		}
	}
	else if (type == G_TYPE_ULONG) {
		if (!*endptr && (llint >= 0) && ((gulong) llint <= G_MAXULONG)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_ULONG);
			g_value_set_ulong (value, (gulong) llint);
		}
		else {
			unsigned long long int lluint;
			lluint = strtoull (str, &endptr, 10);
			if (!*endptr && (lluint <= G_MAXULONG)) {
				value = g_value_init (g_new0 (GValue, 1), G_TYPE_ULONG);
				g_value_set_ulong (value, (gulong) lluint);
			}
		}
	}
	else if (type == G_TYPE_LONG) {
		if (!*endptr && (llint >= G_MINLONG) && (llint <= G_MAXLONG)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_LONG);
			g_value_set_long (value, (glong) llint);
		}
	}
	else
		g_assert_not_reached ();

	return value;
}

static GValue *
gda_handler_numerical_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, GType type)
{
	return _gda_handler_numerical_get_value_from_str_locale (sql, type, TRUE);
}

static GValue *
gda_handler_numerical_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, GType type)
{
	return _gda_handler_numerical_get_value_from_str_locale (str, type, FALSE);
}

static GValue *
gda_handler_numerical_get_sane_init_value (G_GNUC_UNUSED GdaDataHandler *iface, GType type)
{
	GValue *value;
	value = gda_handler_numerical_get_value_from_sql (iface, "", type);

	return value;
}

static gboolean
gda_handler_numerical_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerNumerical *hdl;
	guint i = 0;

	g_assert (iface);
	hdl = (GdaHandlerNumerical*) (iface);
	for (i = 0; i < hdl->priv->nb_g_types; i++) {
		if (hdl->priv->valid_g_types [i] == type)
			return TRUE;
	}

	return FALSE;
}

static const gchar *
gda_handler_numerical_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_HANDLER_NUMERICAL (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}
