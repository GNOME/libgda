/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#include "gda-sqlite-handler-bin.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-util.h>

static void gda_sqlite_handler_bin_class_init (GdaSqliteHandlerBinClass * class);
static void gda_sqlite_handler_bin_init (GdaSqliteHandlerBin * wid);
static void gda_sqlite_handler_bin_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_sqlite_handler_bin_data_handler_init      (GdaDataHandlerInterface *iface);
static gchar       *gda_sqlite_handler_bin_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_sqlite_handler_bin_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_sqlite_handler_bin_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
								   GType type);
static GValue      *gda_sqlite_handler_bin_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
								   GType type);
static gboolean     gda_sqlite_handler_bin_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_sqlite_handler_bin_get_descr              (GdaDataHandler *dh);

G_DEFINE_TYPE_WITH_CODE (GdaSqliteHandlerBin, gda_sqlite_handler_bin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, gda_sqlite_handler_bin_data_handler_init))

static void
gda_sqlite_handler_bin_data_handler_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_sqlite_handler_bin_get_sql_from_value;
	iface->get_str_from_value = gda_sqlite_handler_bin_get_str_from_value;
	iface->get_value_from_sql = gda_sqlite_handler_bin_get_value_from_sql;
	iface->get_value_from_str = gda_sqlite_handler_bin_get_value_from_str;
	iface->get_sane_init_value = NULL;
	iface->accepts_g_type = gda_sqlite_handler_bin_accepts_g_type;
	iface->get_descr = gda_sqlite_handler_bin_get_descr;
}


static void
gda_sqlite_handler_bin_class_init (GdaSqliteHandlerBinClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	object_class->dispose = gda_sqlite_handler_bin_dispose;
}

static void
gda_sqlite_handler_bin_init (GdaSqliteHandlerBin * hdl)
{
	/* Private structure */
	g_object_set_data (G_OBJECT (hdl), "name", "SqliteBin");
	g_object_set_data (G_OBJECT (hdl), "descr", _("SQLite binary representation"));
}

static void
gda_sqlite_handler_bin_dispose (GObject   * object)
{
	/* for the parent class */
	G_OBJECT_CLASS (gda_sqlite_handler_bin_parent_class)->dispose (object);
}

/**
 * _gda_sqlite_handler_bin_new
 *
 * Creates a data handler for binary values
 *
 * Returns: the new object
 */
GdaDataHandler *
_gda_sqlite_handler_bin_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_SQLITE_HANDLER_BIN, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_sqlite_handler_bin_get_sql_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	GdaBinary *bin;
	gint i;

	bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
	retval = g_new0 (gchar, gda_binary_get_size (bin) * 2 + 4);
	retval [0] = 'x';
	retval [1] = '\'';
	for (i = 0; i < gda_binary_get_size (bin); i++) {
		guchar *ptr;

		ptr = gda_binary_get_data (bin) + i;
		if ((*ptr >> 4) <= 9)
			retval [2*i + 2] = (*ptr >> 4) + '0';
		else
			retval [2*i + 2] = (*ptr >> 4) + 'A' - 10;
		if ((*ptr & 0xF) <= 9)
			retval [2*i + 3] = (*ptr & 0xF) + '0';
		else
			retval [2*i + 3] = (*ptr & 0xF) + 'A' - 10;
	}
	retval [gda_binary_get_size (bin) * 2 + 2] = '\'';

	return retval;
}

static gchar *
gda_sqlite_handler_bin_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	GdaBinary *bin;
	gint i;

	bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
	retval = g_new0 (gchar, gda_binary_get_size (bin) * 2 + 1);
	for (i = 0; i < gda_binary_get_size (bin); i++) {
		guchar *ptr;

		ptr = gda_binary_get_data (bin) + i;
		if ((*ptr >> 4) <= 9)
			retval [2*i] = (*ptr >> 4) + '0';
		else
			retval [2*i] = (*ptr >> 4) + 'A' - 10;
		if ((*ptr & 0xF) <= 9)
			retval [2*i + 1] = (*ptr & 0xF) + '0';
		else
			retval [2*i + 1] = (*ptr & 0xF) + 'A' - 10;
	}

	return retval;
}

static int hex_to_int (int h) {
	if (h >= '0' && h <= '9') 
		return h - '0';
	else if (h >= 'a' && h <= 'f') 
		return h - 'a' + 10;
	else {
		if (h >= 'A' && h <= 'F')
			return h - 'A' + 10;
		else
			return 0;
	}
}

static GValue *
gda_sqlite_handler_bin_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
	g_assert (sql);
	GValue *value = NULL;

	if (*sql) {
		gint n = strlen (sql);
		if ((n >= 3) && 
		    ! ((n-3) % 2) && 
		    ((sql[0] == 'x') || (sql[0] == 'X')) &&
		    (sql[1] == '\'') &&
		    (sql[n] == '\'')) {
			GdaBinary *bin;
			
			bin = gda_binary_new ();
			if (n > 3) {
				gint i;
				guchar* buffer = g_new0 (guchar, (n-3)/2);
				for (i = 2; i < n-1; i += 2)
					buffer [i/2 - 1] = (hex_to_int (sql[i]) << 4) | hex_to_int (sql [i+1]);
				gda_binary_set_data (bin, buffer, n-3);
				g_free (buffer);
			}

			value = gda_value_new (GDA_TYPE_BINARY);
			gda_value_take_binary (value, bin);
		}
	}

	return value;
}

static GValue *
gda_sqlite_handler_bin_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
	g_assert (str);

	GValue *value = NULL;

	if (*str) {
		gint n = strlen (str);
		if (! (n % 2)) {
			GdaBinary *bin;

			bin = gda_binary_new ();
			guchar* buffer = NULL;
			if (n > 0) {
				gint i;
				buffer = g_new0 (guchar, n/2);
				for (i = 0; i < n; i += 2)
					buffer [i/2] = (hex_to_int (str[i]) << 4) | hex_to_int (str [i+1]);
			}
      gda_binary_set_data (bin, buffer, n);
			value = gda_value_new (GDA_TYPE_BINARY);
			gda_value_take_binary (value, bin);
		}
	}
	else {
		GdaBinary *bin;
		bin = gda_string_to_binary (str);
		value = gda_value_new (GDA_TYPE_BINARY);
		gda_value_take_binary (value, bin);
	}

	return value;
}

static gboolean
gda_sqlite_handler_bin_accepts_g_type (G_GNUC_UNUSED GdaDataHandler *iface, GType type)
{
	g_assert (iface);
	return type == GDA_TYPE_BINARY ? TRUE : FALSE;
}

static const gchar *
gda_sqlite_handler_bin_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}
