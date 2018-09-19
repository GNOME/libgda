/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 - 2012 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include "gda-postgres-handler-bin.h"
#include "gda-postgres.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-util.h>

static void gda_postgres_handler_bin_class_init (GdaPostgresHandlerBinClass * class);
static void gda_postgres_handler_bin_init (GdaPostgresHandlerBin * wid);
static void gda_postgres_handler_bin_dispose (GObject *object);


/* GdaDataHandler interface */
static void         gda_postgres_handler_bin_data_handler_init      (GdaDataHandlerInterface *iface);
static gchar       *gda_postgres_handler_bin_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_postgres_handler_bin_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_postgres_handler_bin_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
								     GType type);
static GValue      *gda_postgres_handler_bin_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
								     GType type);
static gboolean     gda_postgres_handler_bin_accepts_g_type         (GdaDataHandler * dh, GType type);

static const gchar *gda_postgres_handler_bin_get_descr              (GdaDataHandler *dh);

struct  _GdaPostgresHandlerBinPriv {
	GdaConnection     *cnc;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_postgres_handler_bin_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaPostgresHandlerBinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_handler_bin_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresHandlerBin),
			0,
			(GInstanceInitFunc) gda_postgres_handler_bin_init,
			NULL
		};	

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_postgres_handler_bin_data_handler_init,
			NULL,
			NULL
		};

		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaPostgresHandlerBin", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_postgres_handler_bin_data_handler_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_postgres_handler_bin_get_sql_from_value;
	iface->get_str_from_value = gda_postgres_handler_bin_get_str_from_value;
	iface->get_value_from_sql = gda_postgres_handler_bin_get_value_from_sql;
	iface->get_value_from_str = gda_postgres_handler_bin_get_value_from_str;
	iface->get_sane_init_value = NULL;
	iface->accepts_g_type = gda_postgres_handler_bin_accepts_g_type;
	iface->get_descr = gda_postgres_handler_bin_get_descr;
}


static void
gda_postgres_handler_bin_class_init (GdaPostgresHandlerBinClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_postgres_handler_bin_dispose;
}

static void
gda_postgres_handler_bin_init (GdaPostgresHandlerBin * hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaPostgresHandlerBinPriv, 1);
	g_object_set_data (G_OBJECT (hdl), "name", _("PostgresqlBin"));
	g_object_set_data (G_OBJECT (hdl), "descr", _("PostgreSQL binary representation"));
}

static void
gda_postgres_handler_bin_dispose (GObject   * object)
{
	GdaPostgresHandlerBin *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_POSTGRES_HANDLER_BIN (object));

	hdl = GDA_POSTGRES_HANDLER_BIN (object);

	if (hdl->priv) {
		if (hdl->priv->cnc)
			g_object_remove_weak_pointer (G_OBJECT (hdl->priv->cnc), (gpointer) &(hdl->priv->cnc));

		g_free (hdl->priv);
		hdl->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

/**
 * gda_postgres_handler_bin_new
 *
 * Creates a data handler for binary values
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_postgres_handler_bin_new (GdaConnection *cnc)
{
	GObject *obj;
	GdaPostgresHandlerBin *hdl;

	obj = g_object_new (GDA_TYPE_POSTGRES_HANDLER_BIN, NULL);
	hdl = (GdaPostgresHandlerBin*) obj;

	if (cnc) {
		hdl->priv->cnc = cnc;
		g_object_add_weak_pointer (G_OBJECT (cnc), (gpointer) &(hdl->priv->cnc));
	}

	return (GdaDataHandler *) obj;
}


static gchar *
gda_postgres_handler_bin_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	GdaPostgresHandlerBin *hdl;
	PostgresConnectionData *cdata = NULL;

	g_return_val_if_fail (GDA_IS_POSTGRES_HANDLER_BIN (iface), NULL);
	
	hdl = (GdaPostgresHandlerBin*) (iface);

	if (hdl->priv->cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (hdl->priv->cnc), NULL);
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (hdl->priv->cnc, NULL);
	}

	GdaBinary *data = (GdaBinary *) gda_value_get_binary ((GValue *) value);
	if (data) {
		gchar *tmp;
		size_t retlength;
		if (0 && cdata) {
			/* FIXME: use this call but it's only defined for Postgres >= 8.1 */
			/*tmp = PQescapeByteaConn (cdata->pconn, data, length, &retlength);*/
		}
		else
			tmp = (gchar *) PQescapeBytea (gda_binary_get_data (data), 
						       gda_binary_get_size (data), &retlength);
		if (tmp) {
			retval = g_strdup_printf ("'%s'", tmp);
			PQfreemem (tmp);
		}
		else {
			g_warning (_("Insufficient memory to convert binary buffer to string"));
			return NULL;
		}
	}
	else
		retval = g_strdup ("NULL");
	
	return retval;
}

static gchar *
gda_postgres_handler_bin_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	retval = gda_binary_to_string (gda_value_get_binary ((GValue *) value), 0);
	return retval;
}

static GValue *
gda_postgres_handler_bin_get_value_from_sql (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *sql, GType type)
{
	g_assert (sql);

	GValue *value = NULL;
	if (*sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			guchar *unstr;
			size_t retlength;

			str[i-1] = 0;
			unstr = PQunescapeBytea ((guchar*) (str+1), &retlength);
			if (unstr) {
				value = gda_value_new_binary (unstr, retlength);
				PQfreemem (unstr);
			}
			else
				g_warning (_("Insufficient memory to convert string to binary buffer"));

			g_free (str);
		}
	}

	return value;
}

static GValue *
gda_postgres_handler_bin_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, GType type)
{
	g_assert (str);

	GValue *value = NULL;
	GdaBinary *bin;
	bin = gda_string_to_binary (str);
	if (bin) {
		value = gda_value_new (GDA_TYPE_BINARY);
		gda_value_take_binary (value, bin);
	}

	return value;
}

static gboolean
gda_postgres_handler_bin_accepts_g_type (GdaDataHandler *iface, GType type)
{
	g_assert (iface);
	return type == GDA_TYPE_BINARY ? TRUE : FALSE;
}

static const gchar *
gda_postgres_handler_bin_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_HANDLER_BIN (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}
