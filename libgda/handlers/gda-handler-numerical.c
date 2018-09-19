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

struct _GdaHandlerNumerical
{
	GObject   parent_instance;

	guint           nb_g_types;
	GType          *valid_g_types;
};

static void data_handler_iface_init (GdaDataHandlerInterface *iface);

G_DEFINE_TYPE_EXTENDED (GdaHandlerNumerical, gda_handler_numerical, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, data_handler_iface_init))

/**
 * gda_handler_numerical_new:
 *
 * Creates a data handler for numerical values
 *
 * Returns: (type GdaHandlerNumerical) (transfer full): the new object
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
	for (i = 0; i < hdl->nb_g_types; i++) {
		if (hdl->valid_g_types [i] == type)
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

static void
gda_handler_numerical_init (GdaHandlerNumerical * hdl)
{
	/* Private structure */
	hdl->nb_g_types = 13;
	hdl->valid_g_types = g_new0 (GType, hdl->nb_g_types);
	hdl->valid_g_types[0] = G_TYPE_INT64;
	hdl->valid_g_types[1] = G_TYPE_DOUBLE;
	hdl->valid_g_types[2] = G_TYPE_INT;
        hdl->valid_g_types[3] = GDA_TYPE_NUMERIC;
        hdl->valid_g_types[4] = G_TYPE_FLOAT;
        hdl->valid_g_types[5] = GDA_TYPE_SHORT;
        hdl->valid_g_types[6] = G_TYPE_CHAR;
	hdl->valid_g_types[7] = G_TYPE_UINT64;
        hdl->valid_g_types[8] = GDA_TYPE_USHORT;
        hdl->valid_g_types[9] = G_TYPE_UCHAR;
	hdl->valid_g_types[10] = G_TYPE_UINT;
	hdl->valid_g_types[11] = G_TYPE_ULONG;
	hdl->valid_g_types[12] = G_TYPE_LONG;

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

	g_clear_pointer (&hdl->valid_g_types, g_free);

	/* for the parent class */
	G_OBJECT_CLASS (gda_handler_numerical_parent_class)->dispose (object);
}

static void
data_handler_iface_init (GdaDataHandlerInterface *iface)
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
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gda_handler_numerical_dispose;
}
