/* gda-handler-bin.c
 *
 * Copyright (C) 2005 Vivien Malerba
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

#include "gda-handler-bin.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>

static void gda_handler_bin_class_init (GdaHandlerBinClass * class);
static void gda_handler_bin_init (GdaHandlerBin * wid);
static void gda_handler_bin_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_bin_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_bin_get_sql_from_value     (GdaDataHandler *dh, const GdaValue *value);
static gchar       *gda_handler_bin_get_str_from_value     (GdaDataHandler *dh, const GdaValue *value);
static GdaValue    *gda_handler_bin_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
							    GdaValueType type);
static GdaValue    *gda_handler_bin_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
							    GdaValueType type);
static guint        gda_handler_bin_get_nb_gda_types       (GdaDataHandler *dh);
static GdaValueType gda_handler_bin_get_gda_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_bin_accepts_gda_type       (GdaDataHandler * dh, GdaValueType type);

static const gchar *gda_handler_bin_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerBinPriv {
	gchar             *detailled_descr;
	guint              nb_gda_types;
	GdaValueType      *valid_gda_types;

	GdaServerProvider *prov;
	GdaConnection     *cnc;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_bin_get_type (void)
{
	static GType type = 0;

	if (!type) {
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

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaHandlerBin", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
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
	iface->get_nb_gda_types = gda_handler_bin_get_nb_gda_types;
	iface->accepts_gda_type = gda_handler_bin_accepts_gda_type;
	iface->get_gda_type_index = gda_handler_bin_get_gda_type_index;
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
	hdl->priv->nb_gda_types = 2;
	hdl->priv->valid_gda_types = g_new0 (GdaValueType, 2);
	hdl->priv->valid_gda_types[0] = GDA_VALUE_TYPE_BINARY;
	hdl->priv->valid_gda_types[1] = GDA_VALUE_TYPE_BLOB;

	gda_object_set_name (GDA_OBJECT (hdl), _("InternalBin"));
	gda_object_set_description (GDA_OBJECT (hdl), _("Binary representation"));
}

static void
gda_handler_bin_dispose (GObject   * object)
{
	GdaHandlerBin *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_BIN (object));

	hdl = GDA_HANDLER_BIN (object);

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

	obj = g_object_new (GDA_TYPE_HANDLER_BIN, "dict", NULL, NULL);

	return (GdaDataHandler *) obj;
}

/**
 * gda_handler_bin_new_with_prov
 *
 * Creates a data handler for binary values
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_bin_new_with_prov (GdaServerProvider *prov, GdaConnection *cnc)
{
	GdaHandlerBin *dh;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	dh = GDA_HANDLER_BIN (gda_handler_bin_new ());
	
	dh->priv->prov = prov;
	dh->priv->cnc = cnc;

	g_object_add_weak_pointer (G_OBJECT (prov), &(dh->priv->prov));
	g_object_add_weak_pointer (G_OBJECT (cnc), &(dh->priv->cnc));

	return dh;
}

static GdaBinary *
string_to_bin (const gchar *string)
{
	GdaBinary *bin;
	gchar *sptr;
	guchar *bptr;

	bin = g_new0 (GdaBinary, 1);
	bin->data = g_new (guchar, strlen (string)*4+1);
	bptr = bin->data;
	sptr = string;
	while (*sptr) {
		if (*sptr == '\\') {
			sptr++;
			if (*sptr && *(sptr+1) && *(sptr+2)) {
				*bptr = (*sptr - '0') * 100 +
					(*(sptr+1) - '0') * 10 +
					(*(sptr+2) - '0');
				bptr++;
				sptr += 3;
			}
			else {
				g_free (bin->data);
				g_free (bin);
				return NULL;
			}
		}
		else {
			*bptr = *sptr;
			bptr++;
			sptr++;
		}
		bin->binary_length ++;
	}

	return bin;
}

static gchar *
bin_to_string (const GdaBinary *bin)
{
	GString *string;
	gchar *retval;
	guchar *ptr;
	glong offset;

	string = g_string_new (NULL);
	
	for (ptr = bin->data, offset = 0; offset < bin->binary_length; offset++, ptr++) {
		if (((*ptr >= 'a') && (*ptr <= 'z')) ||
		    ((*ptr >= 'A') && (*ptr <= 'Z')) ||
		    ((*ptr >= '0') && (*ptr <= '9')) ||
		    (*ptr == ' '))
			g_string_append_c (string, *ptr);
		else 
			g_string_append_printf (string, "\%3d", *ptr);
	}

	retval = string->str;
	g_string_free (string, FALSE);
	return retval;
}

static gchar *
gda_handler_bin_get_sql_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	gchar *retval;
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
		if (gda_value_isa (value, GDA_VALUE_TYPE_BLOB)) {
			gchar *sql_id;
			GdaBlob *blob;

			blob = gda_value_get_blob (value);
			sql_id = gda_blob_get_sql_id (blob);
			if (sql_id)
				retval = sql_id;
			else {
				/* copy the blob in memory and translate it */
				TO_IMPLEMENT;
				retval = NULL;
			}
		}
		else {
			gchar *str;
			str = bin_to_string (gda_value_get_binary (value));
			retval = g_strdup_printf ("'%s'", str);
			g_free (str);
		}
	}
	else
		retval = g_strdup (NULL);

	return retval;
}

static gchar *
gda_handler_bin_get_str_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	gchar *retval;
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
		if (gda_value_isa (value, GDA_VALUE_TYPE_BLOB)) {
			gchar *sql_id;
			GdaBlob *blob;
			
			blob = gda_value_get_blob (value);
			sql_id = gda_blob_get_sql_id (blob);
			if (sql_id)
				retval = sql_id;
			else {
				/* copy the blob in memory and translate it */
				TO_IMPLEMENT;
				retval = NULL;
			}
		}
		else
			retval = bin_to_string (gda_value_get_binary (value));
	}
	else
		retval = g_strdup (NULL);

	return retval;
}

static GdaValue *
gda_handler_bin_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GdaValueType type)
{
	GdaHandlerBin *hdl;
	GdaValue *value = NULL;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (sql && *sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			GdaBinary *bin;
			str[i-1] = 0;
			value = gda_handler_bin_get_value_from_str (iface, str+1, type);
			g_free (str);
		}
	}

	return value;
}

static GdaValue *
gda_handler_bin_get_value_from_str (GdaDataHandler *iface, const gchar *str, GdaValueType type)
{
	GdaHandlerBin *hdl;
	GdaValue *value = NULL;
	GdaBinary *bin;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	switch (type) {
	case GDA_VALUE_TYPE_BINARY:
		bin = string_to_bin (str+1);
		if (bin) {
			value = gda_value_new_binary (bin->data, bin->binary_length);
			g_free (bin->data);
			g_free (bin);
		}
		break;

	case GDA_VALUE_TYPE_BLOB: {
		GdaBlob *blob = NULL;
		
		if (hdl->priv->prov) 
			blob = gda_server_provider_fetch_blob_by_id (hdl->priv->prov,
								     hdl->priv->cnc,
								     str+1);
		if (blob) {
			value = gda_value_new_blob (blob);
			g_object_unref (blob);
		}
		break;
	}
	default:
		g_assert_not_reached ();
	}

	return value;
}

static guint
gda_handler_bin_get_nb_gda_types (GdaDataHandler *iface)
{
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), 0);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_gda_types;
}


static gboolean
gda_handler_bin_accepts_gda_type (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerBin *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), FALSE);
	g_return_val_if_fail (type != GDA_VALUE_TYPE_UNKNOWN, FALSE);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_gda_types)) {
		if (hdl->priv->valid_gda_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GdaValueType
gda_handler_bin_get_gda_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), GDA_VALUE_TYPE_UNKNOWN);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, GDA_VALUE_TYPE_UNKNOWN);
	g_return_val_if_fail (index < hdl->priv->nb_gda_types, GDA_VALUE_TYPE_UNKNOWN);

	return hdl->priv->valid_gda_types[index];
}

static const gchar *
gda_handler_bin_get_descr (GdaDataHandler *iface)
{
	GdaHandlerBin *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_BIN (iface), NULL);
	hdl = GDA_HANDLER_BIN (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_object_get_description (GDA_OBJECT (hdl));
}
