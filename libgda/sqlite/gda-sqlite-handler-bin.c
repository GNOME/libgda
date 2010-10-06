/* gda-sqlite-handler-bin.c
 *
 * Copyright (C) 2006 - 2009 Vivien Malerba
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
static void         gda_sqlite_handler_bin_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_sqlite_handler_bin_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_sqlite_handler_bin_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_sqlite_handler_bin_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
							    GType type);
static GValue      *gda_sqlite_handler_bin_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
							    GType type);
static gboolean     gda_sqlite_handler_bin_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_sqlite_handler_bin_get_descr              (GdaDataHandler *dh);

struct  _GdaSqliteHandlerBinPriv {
	gchar             *detailed_descr;
	guint              nb_g_types;
	GType             *valid_g_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
_gda_sqlite_handler_bin_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaSqliteHandlerBinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sqlite_handler_bin_class_init,
			NULL,
			NULL,
			sizeof (GdaSqliteHandlerBin),
			0,
			(GInstanceInitFunc) gda_sqlite_handler_bin_init,
			0
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_sqlite_handler_bin_data_handler_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, CLASS_PREFIX "HandlerBin", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_sqlite_handler_bin_data_handler_init (GdaDataHandlerIface *iface)
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
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_sqlite_handler_bin_dispose;
}

static void
gda_sqlite_handler_bin_init (GdaSqliteHandlerBin * hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaSqliteHandlerBinPriv, 1);
	hdl->priv->detailed_descr = _("SQlite binary handler");
	hdl->priv->nb_g_types = 1;
	hdl->priv->valid_g_types = g_new0 (GType, hdl->priv->nb_g_types);
	hdl->priv->valid_g_types[0] = GDA_TYPE_BINARY;

	g_object_set_data (G_OBJECT (hdl), "name", "SqliteBin");
	g_object_set_data (G_OBJECT (hdl), "descr", _("SQlite binary representation"));
}

static void
gda_sqlite_handler_bin_dispose (GObject   * object)
{
	GdaSqliteHandlerBin *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_SQLITE_HANDLER_BIN (object));

	hdl = GDA_SQLITE_HANDLER_BIN (object);

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
gda_sqlite_handler_bin_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaSqliteHandlerBin *hdl;

	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), NULL);
	hdl = GDA_SQLITE_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
		GdaBinary *bin;
		gint i;
		g_return_val_if_fail (gda_value_isa ((GValue *) value, GDA_TYPE_BINARY), NULL);

		bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
		retval = g_new0 (gchar, bin->binary_length * 2 + 4);
		retval [0] = 'x';
		retval [1] = '\'';
		for (i = 0; i < bin->binary_length; i++) {
			guchar *ptr;

			ptr = bin->data + i;
			if ((*ptr >> 4) <= 9)
				retval [2*i + 2] = (*ptr >> 4) + '0';
			else
				retval [2*i + 2] = (*ptr >> 4) + 'A' - 10;
			if ((*ptr & 0xF) <= 9)
				retval [2*i + 3] = (*ptr & 0xF) + '0';
			else
				retval [2*i + 3] = (*ptr & 0xF) + 'A' - 10;
		}
		retval [bin->binary_length * 2 + 2] = '\'';
	}
	else
		retval = g_strdup ("NULL");

	return retval;
}

static gchar *
gda_sqlite_handler_bin_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaSqliteHandlerBin *hdl;

	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), NULL);
	hdl = GDA_SQLITE_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
		GdaBinary *bin;
		gint i;
		g_return_val_if_fail (gda_value_isa ((GValue *) value, GDA_TYPE_BINARY), NULL);

		bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
		retval = g_new0 (gchar, bin->binary_length * 2 + 1);
		for (i = 0; i < bin->binary_length; i++) {
			guchar *ptr;

			ptr = bin->data + i;
			if ((*ptr >> 4) <= 9)
				retval [2*i] = (*ptr >> 4) + '0';
			else
				retval [2*i] = (*ptr >> 4) + 'A' - 10;
			if ((*ptr & 0xF) <= 9)
				retval [2*i + 1] = (*ptr & 0xF) + '0';
			else
				retval [2*i + 1] = (*ptr & 0xF) + 'A' - 10;
		}
	}
	else
		retval = g_strdup ("");

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
gda_sqlite_handler_bin_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GdaSqliteHandlerBin *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), NULL);
	hdl = GDA_SQLITE_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if ((type == GDA_TYPE_BINARY) && sql && *sql) {
		gint n = strlen (sql);
		if ((n >= 3) && 
		    ! ((n-3) % 2) && 
		    ((sql[0] == 'x') || (sql[0] == 'X')) &&
		    (sql[1] == '\'') &&
		    (sql[n] == '\'')) {
			GdaBinary *bin;
			
			bin = g_new0 (GdaBinary, 1);
			if (n > 3) {
				gint i;
				bin->data = g_new0 (guchar, (n-3)/2);
				for (i = 2; i < n-1; i += 2)
					bin->data [i/2 - 1] = (hex_to_int (sql[i]) << 4) | hex_to_int (sql [i+1]);
				bin->binary_length = n-3;
			}

			value = gda_value_new (GDA_TYPE_BINARY);
			gda_value_take_binary (value, bin);
		}
	}
	else
		g_assert_not_reached ();

	return value;
}

static GValue *
gda_sqlite_handler_bin_get_value_from_str (GdaDataHandler *iface, const gchar *str, GType type)
{
	GdaSqliteHandlerBin *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), NULL);
	hdl = GDA_SQLITE_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (type == GDA_TYPE_BINARY) {
		if (str && *str) {
			gint n = strlen (str);
			if (! (n % 2)) {
				GdaBinary *bin;
				
				bin = g_new0 (GdaBinary, 1);
				if (n > 0) {
					gint i;
					bin->data = g_new0 (guchar, n/2);
					for (i = 0; i < n; i += 2)
						bin->data [i/2] = (hex_to_int (str[i]) << 4) | hex_to_int (str [i+1]);
					bin->binary_length = n;
				}
				
				value = gda_value_new (GDA_TYPE_BINARY);
				gda_value_take_binary (value, bin);
			}
		}
		else {
			if (str) {
				GdaBinary *bin;
				
				bin = g_new0 (GdaBinary, 1);
				value = gda_value_new (GDA_TYPE_BINARY);
				gda_value_take_binary (value, bin);
			}
			else 
				value = gda_value_new_null ();
		}
	}
	else
		g_assert_not_reached ();

	return value;
}

static gboolean
gda_sqlite_handler_bin_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaSqliteHandlerBin *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_SQLITE_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_g_types)) {
		if (hdl->priv->valid_g_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static const gchar *
gda_sqlite_handler_bin_get_descr (GdaDataHandler *iface)
{
	GdaSqliteHandlerBin *hdl;

	g_return_val_if_fail (GDA_IS_SQLITE_HANDLER_BIN (iface), NULL);
	hdl = GDA_SQLITE_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return g_object_get_data (G_OBJECT (hdl), "descr");
}
