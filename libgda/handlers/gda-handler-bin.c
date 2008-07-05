/* gda-handler-bin.c
 *
 * Copyright (C) 2005 - 2008 Vivien Malerba
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
static guint        gda_handler_bin_get_nb_g_types       (GdaDataHandler *dh);
static GType        gda_handler_bin_get_g_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_bin_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_bin_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerBinPriv {
	gchar             *detailled_descr;
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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaHandlerBinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_bin_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerBin),
			0,
			(GInstanceInitFunc) gda_handler_bin_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_bin_data_handler_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaHandlerBin", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_static_mutex_unlock (&registering);
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
	iface->get_nb_g_types = gda_handler_bin_get_nb_g_types;
	iface->accepts_g_type = gda_handler_bin_accepts_g_type;
	iface->get_g_type_index = gda_handler_bin_get_g_type_index;
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
	hdl->priv->detailled_descr = _("Binary handler");
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
 * gda_handler_bin_new
 *
 * Creates a data handler for binary values
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_bin_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_BIN, NULL);

	return (GdaDataHandler *) obj;
}


static gchar *
gda_handler_bin_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
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
			blob = (GdaBlob*) gda_value_get_blob ((GValue *) value);
			if (blob->op)
				gda_blob_op_read_all (blob->op, blob);

			gchar *str, *str2;
			str = gda_binary_to_string ((GdaBinary *) blob, 0);
			str2 = gda_default_escape_string (str);
			g_free (str);
			retval = g_strdup_printf ("'%s'", str2);
			g_free (str2);
		}
	}
	else
		retval = g_strdup (NULL);

	return retval;
}

static gchar *
gda_handler_bin_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

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
	else
		retval = g_strdup (NULL);

	return retval;
}

static GValue *
gda_handler_bin_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	GdaHandlerBin *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (type == GDA_TYPE_BINARY) {
		if (sql && *sql) {
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
	}

	return value;
}

static GValue *
gda_handler_bin_get_value_from_str (GdaDataHandler *iface, const gchar *str, GType type)
{
	GdaHandlerBin *hdl;
	GValue *value = NULL;
	GdaBinary bin;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (type == GDA_TYPE_BINARY) {
		if (gda_string_to_binary (str, &bin)) {
			value = gda_value_new_binary (bin.data, bin.binary_length);
			g_free (bin.data);
		}
	}

	return value;
}

static guint
gda_handler_bin_get_nb_g_types (GdaDataHandler *iface)
{
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), 0);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_g_types;
}


static gboolean
gda_handler_bin_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerBin *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_g_types)) {
		if (hdl->priv->valid_g_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GType
gda_handler_bin_get_g_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), G_TYPE_INVALID);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, G_TYPE_INVALID);
	g_return_val_if_fail (index < hdl->priv->nb_g_types, G_TYPE_INVALID);

	return hdl->priv->valid_g_types[index];
}

static const gchar *
gda_handler_bin_get_descr (GdaDataHandler *iface)
{
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return g_object_get_data (G_OBJECT (hdl), "descr");
}
