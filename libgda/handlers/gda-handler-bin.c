/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#include "gda-handler-bin.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-util.h>
#include <libgda/gda-blob-op.h>

static void gda_handler_bin_class_init (GdaHandlerBinClass * class);
static void gda_handler_bin_init (GdaHandlerBin * wid);
static void gda_handler_bin_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_bin_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_bin_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_handler_bin_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_handler_bin_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
							    GType type);
static GValue      *gda_handler_bin_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
							    GType type);
static gboolean     gda_handler_bin_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_bin_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerBinPriv {
	guint              nb_g_types;
	GType             *valid_g_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_bin_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaHandlerBinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_bin_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerBin),
			0,
			(GInstanceInitFunc) gda_handler_bin_init,
			NULL
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_bin_data_handler_init,
			NULL,
			NULL
		};

		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaHandlerBin", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_handler_bin_data_handler_init (GdaDataHandlerIface *iface)
{
	iface->get_sql_from_value = gda_handler_bin_get_sql_from_value;
	iface->get_str_from_value = gda_handler_bin_get_str_from_value;
	iface->get_value_from_sql = gda_handler_bin_get_value_from_sql;
	iface->get_value_from_str = gda_handler_bin_get_value_from_str;
	iface->get_sane_init_value = NULL;
	iface->accepts_g_type = gda_handler_bin_accepts_g_type;
	iface->get_descr = gda_handler_bin_get_descr;
}


static void
gda_handler_bin_class_init (GdaHandlerBinClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_handler_bin_dispose;
}

static void
gda_handler_bin_init (GdaHandlerBin * hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaHandlerBinPriv, 1);
	hdl->priv->nb_g_types = 2;
	hdl->priv->valid_g_types = g_new0 (GType, hdl->priv->nb_g_types);
	hdl->priv->valid_g_types[0] = GDA_TYPE_BINARY;
	hdl->priv->valid_g_types[1] = GDA_TYPE_BLOB;

	g_object_set_data (G_OBJECT (hdl), "name", "InternalBin");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Binary representation"));
}

static void
gda_handler_bin_dispose (GObject   * object)
{
	GdaHandlerBin *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_BIN (object));

	hdl = GDA_HANDLER_BIN (object);

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
 * gda_handler_bin_new:
 *
 * Creates a data handler for binary values
 *
 * Returns: (transfer full): the new object
 */
GdaDataHandler *
gda_handler_bin_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_BIN, NULL);

	return (GdaDataHandler *) obj;
}


static gchar *
gda_handler_bin_get_sql_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	if (gda_value_isa ((GValue *) value, GDA_TYPE_BINARY)) {
		gchar *str, *str2;
		str = gda_binary_to_string (gda_value_get_binary ((GValue *) value), 0);
		str2 = gda_default_escape_string (str);
		g_free (str);
		retval = g_strdup_printf ("'%s'", str2);
		g_free (str2);
	}
	else {
		GdaBlob *blob;
		GdaBinary *bin;
		blob = (GdaBlob*) gda_value_get_blob ((GValue *) value);
		bin = (GdaBinary *) blob;
		if (blob->op &&
		    (bin->binary_length != gda_blob_op_get_length (blob->op)))
			gda_blob_op_read_all (blob->op, blob);

		gchar *str, *str2;
		str = gda_binary_to_string (bin, 0);
		str2 = gda_default_escape_string (str);
		g_free (str);
		retval = g_strdup_printf ("'%s'", str2);
		g_free (str2);
	}

	return retval;
}

static gchar *
gda_handler_bin_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval = NULL;
	if (value) {
		if (gda_value_isa ((GValue *) value, GDA_TYPE_BINARY)) 
			retval = gda_binary_to_string (gda_value_get_binary ((GValue *) value), 0);
		else {
			GdaBlob *blob;
			blob = (GdaBlob*) gda_value_get_blob ((GValue *) value);
			if (blob->op)
				gda_blob_op_read_all (blob->op, blob);
			retval = gda_binary_to_string ((GdaBinary *) blob, 0);
		}
	}

	return retval;
}

static GValue *
gda_handler_bin_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, GType type)
{
	g_assert (sql);

	GValue *value = NULL;
	if (*sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			gchar *unstr;

			str[i-1] = 0;
			unstr = gda_default_unescape_string (str+1);
			if (unstr) {
				value = gda_handler_bin_get_value_from_str (iface, unstr, type);
				g_free (unstr);
			}
			g_free (str);
		}
	}

	return value;
}

static GValue *
gda_handler_bin_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, GType type)
{
	g_assert (str);

	GValue *value = NULL;
	if (type == GDA_TYPE_BINARY) {
		GdaBinary *bin;
		bin = gda_string_to_binary (str);
		if (bin) {
			value = gda_value_new (GDA_TYPE_BINARY);
			gda_value_take_binary (value, bin);
		}
	}
	else {
		GdaBlob *blob;
		blob = gda_string_to_blob (str);
		if (blob) {
			value = gda_value_new (GDA_TYPE_BLOB);
			gda_value_take_blob (value, blob);
		}
	}

	return value;
}

static gboolean
gda_handler_bin_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerBin *hdl;
	guint i;

	g_assert (iface);
	hdl = (GdaHandlerBin*) (iface);

	for (i = 0; i < hdl->priv->nb_g_types; i++) {
		if (hdl->priv->valid_g_types [i] == type)
			return TRUE;
	}

	return FALSE;
}

static const gchar *
gda_handler_bin_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_HANDLER_BIN (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}
