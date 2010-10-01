/* gda-handler-type.c
 *
 * Copyright (C) 2005 - 2009 Vivien Malerba
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

#include "gda-handler-type.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gda-util.h>

static void gda_handler_type_class_init (GdaHandlerTypeClass * class);
static void gda_handler_type_init (GdaHandlerType * wid);
static void gda_handler_type_dispose (GObject   * object);


/* GdaDataHandler interface */
static void         gda_handler_type_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_type_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
static gchar       *gda_handler_type_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
static GValue      *gda_handler_type_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, 
							     GType type);
static GValue      *gda_handler_type_get_value_from_str     (GdaDataHandler *dh, const gchar *sql, 
							     GType type);
static gboolean     gda_handler_type_accepts_g_type       (GdaDataHandler * dh, GType type);

static const gchar *gda_handler_type_get_descr              (GdaDataHandler *dh);

struct  _GdaHandlerTypePriv {
	gchar          *detailed_descr;
	guint           nb_g_types;
	GType          *valid_g_types;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_type_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaHandlerTypeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_type_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerType),
			0,
			(GInstanceInitFunc) gda_handler_type_init,
			0
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_type_data_handler_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaHandlerType", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_handler_type_data_handler_init (GdaDataHandlerIface *iface)
{
	iface->get_sql_from_value = gda_handler_type_get_sql_from_value;
	iface->get_str_from_value = gda_handler_type_get_str_from_value;
	iface->get_value_from_sql = gda_handler_type_get_value_from_sql;
	iface->get_value_from_str = gda_handler_type_get_value_from_str;
	iface->get_sane_init_value = NULL;
	iface->accepts_g_type = gda_handler_type_accepts_g_type;
	iface->get_descr = gda_handler_type_get_descr;
}


static void
gda_handler_type_class_init (GdaHandlerTypeClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_handler_type_dispose;
}

static void
gda_handler_type_init (GdaHandlerType * hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaHandlerTypePriv, 1);
	hdl->priv->detailed_descr = _("Gda type handler");
	hdl->priv->nb_g_types = 1;
	hdl->priv->valid_g_types = g_new0 (GType, 1);
	hdl->priv->valid_g_types[0] = G_TYPE_GTYPE;

	g_object_set_data (G_OBJECT (hdl), "descr", "InternalType");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Gda type representation"));
}

static void
gda_handler_type_dispose (GObject   * object)
{
	GdaHandlerType *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_TYPE (object));

	hdl = GDA_HANDLER_TYPE (object);

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
 * gda_handler_type_new
 *
 * Creates a data handler for Gda types
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_type_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_TYPE, NULL);

	return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_type_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaHandlerType *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TYPE (iface), NULL);
	hdl = GDA_HANDLER_TYPE (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
		GTypeQuery tq;
		g_type_query (g_value_get_gtype (value), &tq);
		if (tq.type != 0) {
			const gchar *str;
			str = gda_g_type_to_string (g_value_get_gtype (value));
			retval = g_strdup_printf ("'%s'", str);
		}
		else
			retval = g_strdup ("NULL");	
	}
	else
		retval = g_strdup ("NULL");

	return retval;
}

static gchar *
gda_handler_type_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	gchar *retval;
	GdaHandlerType *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TYPE (iface), NULL);
	hdl = GDA_HANDLER_TYPE (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (value) {
		GTypeQuery tq;
       
		g_type_query (g_value_get_gtype (value), &tq);
		if (tq.type != 0)
			retval = g_strdup (gda_g_type_to_string (g_value_get_gtype (value)));
		else
			retval = g_strdup (NULL); /* FIXME: return NULL instead because of the error? */
	}
	else
		retval = g_strdup (NULL);

	return retval;
}

static GValue *
gda_handler_type_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
	GdaHandlerType *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TYPE (iface), NULL);
	hdl = GDA_HANDLER_TYPE (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	if (sql && *sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			GType type;
			str[i-1] = 0;
			type = gda_g_type_from_string (str+1);
			g_free (str);
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_GTYPE);
			g_value_set_gtype (value, type);
		}
	}
	else
		value = gda_value_new_null ();
	return value;
}

static GValue *
gda_handler_type_get_value_from_str (GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
	GdaHandlerType *hdl;
	GValue *value;
	GType vtype;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TYPE (iface), NULL);
	hdl = GDA_HANDLER_TYPE (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	vtype = gda_g_type_from_string (str);
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_GTYPE);
	g_value_set_gtype (value, vtype);

	return value;
}

static gboolean
gda_handler_type_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerType *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TYPE (iface), FALSE);
	g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
	hdl = GDA_HANDLER_TYPE (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_g_types)) {
		if (hdl->priv->valid_g_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static const gchar *
gda_handler_type_get_descr (GdaDataHandler *iface)
{
	GdaHandlerType *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TYPE (iface), NULL);
	hdl = GDA_HANDLER_TYPE (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return g_object_get_data (G_OBJECT (hdl), "descr");
}
