/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 - 2012 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

typedef struct {
	GWeakRef     cnc;
} GdaPostgresHandlerBinPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaPostgresHandlerBin, gda_postgres_handler_bin, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaPostgresHandlerBin)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, gda_postgres_handler_bin_data_handler_init))

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
	
	object_class->dispose = gda_postgres_handler_bin_dispose;
}

static void
gda_postgres_handler_bin_init (GdaPostgresHandlerBin * hdl)
{
	/* Private structure */
  GdaPostgresHandlerBinPrivate *priv = gda_postgres_handler_bin_get_instance_private (hdl);
  g_weak_ref_init (&priv->cnc, NULL);
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
  GdaPostgresHandlerBinPrivate *priv = gda_postgres_handler_bin_get_instance_private (hdl);

  g_weak_ref_clear (&priv->cnc);

	/* for the parent class */
	G_OBJECT_CLASS (gda_postgres_handler_bin_parent_class)->dispose (object);
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
  GdaPostgresHandlerBinPrivate *priv = gda_postgres_handler_bin_get_instance_private (hdl);

  g_weak_ref_set (&priv->cnc, cnc);

	return (GdaDataHandler *) obj;
}


static gchar *
gda_postgres_handler_bin_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval;
	GdaPostgresHandlerBin *hdl;
	PostgresConnectionData *cdata = NULL;
  GdaConnection *cnc = NULL;

	g_return_val_if_fail (GDA_IS_POSTGRES_HANDLER_BIN (iface), NULL);
	
	hdl = (GdaPostgresHandlerBin*) (iface);
  GdaPostgresHandlerBinPrivate *priv = gda_postgres_handler_bin_get_instance_private (hdl);

  cnc = g_weak_ref_get (&priv->cnc);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
    g_object_unref (cnc);
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
