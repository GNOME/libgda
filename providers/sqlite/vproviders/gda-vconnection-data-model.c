/* 
 * GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-vconnection-data-model.h"
#include "gda-virtual-provider.h"
#include "../gda-sqlite.h"

#define PARENT_TYPE GDA_TYPE_VIRTUAL_CONNECTION

struct _GdaVconnectionDataModelPrivate {
	GSList *data_models;
};

/* properties */
enum
{
        PROP_0,
};

static void gda_vconnection_data_model_class_init (GdaVconnectionDataModelClass *klass);
static void gda_vconnection_data_model_init       (GdaVconnectionDataModel *prov, GdaVconnectionDataModelClass *klass);
static void gda_vconnection_data_model_dispose   (GObject *object);
static void gda_vconnection_data_model_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void gda_vconnection_data_model_get_property (GObject *object,
					       guint param_id,
					       GValue *value,
					       GParamSpec *pspec);
static GObjectClass  *parent_class = NULL;

/*
 * GdaVconnectionDataModel class implementation
 */
static void
gda_vconnection_data_model_class_init (GdaVconnectionDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_vconnection_data_model_dispose;

	/* Properties */
        object_class->set_property = gda_vconnection_data_model_set_property;
        object_class->get_property = gda_vconnection_data_model_get_property;
}

static void
gda_vconnection_data_model_init (GdaVconnectionDataModel *prov, GdaVconnectionDataModelClass *klass)
{
	prov->priv = g_new (GdaVconnectionDataModelPrivate, 1);
	prov->priv->data_models = NULL;
}

static void
gda_vconnection_data_model_dispose (GObject *object)
{
	GdaVconnectionDataModel *prov = (GdaVconnectionDataModel *) object;

	g_return_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (prov));

	/* free memory */
	if (prov->priv) {
		GSList *list;
		for (list = prov->priv->data_models; list; list = list->next) 
			g_object_unref (list->data);
		g_slist_free (prov->priv->data_models);

		g_free (prov->priv);
		prov->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

GType
gda_vconnection_data_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVconnectionDataModelClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_vconnection_data_model_class_init,
				NULL, NULL,
				sizeof (GdaVconnectionDataModel),
				0,
				(GInstanceInitFunc) gda_vconnection_data_model_init
			};
			
			type = g_type_register_static (PARENT_TYPE, "GdaVconnectionDataModel", &info, 0);
		}
	}

	return type;
}

static void
gda_vconnection_data_model_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
        GdaVconnectionDataModel *cnc;

        cnc = GDA_VCONNECTION_DATA_MODEL (object);
        if (cnc->priv) {
                switch (param_id) {
		default:
			break;
                }
        }
}

static void
gda_vconnection_data_model_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec)
{
        GdaVconnectionDataModel *cnc;

        cnc = GDA_VCONNECTION_DATA_MODEL (object);
        if (cnc->priv) {
		switch (param_id) {
		default:
			break;
		}
        }
}

/**
 * gda_vconnection_data_model_add
 */
gboolean
gda_vconnection_data_model_add (GdaVconnectionDataModel *cnc, 
				GdaDataModel *model, const gchar *table_name, GError **error)
{
	GdaVirtualProvider *prov;
	gchar *str;
	int rc;
	char *zErrMsg = NULL;
	gboolean retval = TRUE;
	SQLITEcnc *scnc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string (cnc, _("Invalid SQLite handle"));
		return FALSE;
	}

	prov = gda_connection_get_provider_obj (GDA_CONNECTION (cnc));
	cnc->adding = model;
	str = g_strdup_printf ("CREATE VIRTUAL TABLE %s USING %s ()", table_name, G_OBJECT_TYPE_NAME (prov));
	rc = sqlite3_exec (scnc->connection, str, NULL, 0, &zErrMsg);
	cnc->adding = NULL;
	g_free (str);
	if (rc != SQLITE_OK) {
		g_set_error (error, 0, 0,
			     g_strdup (zErrMsg));
		sqlite3_free (zErrMsg);
		retval = FALSE;
	}
	else {
		/* all ok => keep a reference on @model */
		cnc->priv->data_models = g_slist_prepend (cnc->priv->data_models, model);
		g_object_ref (model);
	}

	return retval;
}

/**
 * gda_vconnection_data_model_remove
 */
gboolean
gda_vconnection_data_model_remove (GdaVconnectionDataModel *cnc, GdaDataModel *model, GError **error)
{
	TO_IMPLEMENT;
	return FALSE;
}
