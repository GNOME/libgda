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

typedef struct {
	GdaDataModel *model;
	gchar        *table_name;
} TableModel;

struct _GdaVconnectionDataModelPrivate {
	GSList *table_models; /* list of TableModel structures */
};

/* properties */
enum
{
        PROP_0,
};

static void gda_vconnection_data_model_class_init (GdaVconnectionDataModelClass *klass);
static void gda_vconnection_data_model_init       (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelClass *klass);
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

static TableModel *get_table_model_by_name (GdaVconnectionDataModel *cnc, const gchar *table_name);
static TableModel *get_table_model_by_model (GdaVconnectionDataModel *cnc, GdaDataModel *model);

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
gda_vconnection_data_model_init (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelClass *klass)
{
	cnc->priv = g_new (GdaVconnectionDataModelPrivate, 1);
	cnc->priv->table_models = NULL;

	g_object_set (G_OBJECT (cnc), "cnc_string", "_IS_VIRTUAL=TRUE", NULL);
}

static void
gda_vconnection_data_model_dispose (GObject *object)
{
	GdaVconnectionDataModel *cnc = (GdaVconnectionDataModel *) object;

	g_return_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc));

	/* free memory */
	if (cnc->priv) {
		gda_connection_close_no_warning ((GdaConnection *) cnc);
		g_assert (!cnc->priv->table_models);

		g_free (cnc->priv);
		cnc->priv = NULL;
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
 * @cnc: a #GdaVconnectionDataModel connection
 * @model: a #GdaDataModel
 * @table_name: the name of the table
 * @error: a place to store errors, or %NULL
 *
 * Make @model appear as a table named @table_name in the @cnc connection (as if a
 * "CREATE TABLE..." statement was executed, except that the data contained within @model
 * is actually used when @table_name's contents is read or written).
 *
 * Returns: TRUE if no error occurred
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
		gda_connection_add_event_string ((GdaConnection *) cnc, _("Invalid SQLite handle"));
		return FALSE;
	}

	prov = (GdaVirtualProvider *) gda_connection_get_provider_obj (GDA_CONNECTION (cnc));
	cnc->adding = model;
	str = g_strdup_printf ("CREATE VIRTUAL TABLE %s USING %s ()", table_name, G_OBJECT_TYPE_NAME (prov));
	rc = sqlite3_exec (scnc->connection, str, NULL, 0, &zErrMsg);
	cnc->adding = NULL;
	g_free (str);
	if (rc != SQLITE_OK) {
		g_set_error (error, 0, 0, g_strdup (zErrMsg));
		sqlite3_free (zErrMsg);
		retval = FALSE;
	}
	else {
		/* all ok => keep a reference on @model */
		TableModel *tm;

		tm = g_new (TableModel, 1);
		tm->model = model;
		g_object_ref (model);
		tm->table_name = g_strdup (table_name);
		cnc->priv->table_models = g_slist_append (cnc->priv->table_models, tm);
	}

	return retval;
}

/**
 * gda_vconnection_data_model_remove
 * @cnc: a #GdaVconnectionDataModel connection
 * @model: a #GdaDataModel
 * @error: a place to store errors, or %NULL
 *
 * Remove the table representing @model in the @cnc connection (as if a "DROP TABLE..."
 * statement was executed, except that no data gets destroyed as @model remains the same).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_remove (GdaVconnectionDataModel *cnc, GdaDataModel *model, GError **error)
{
	TableModel *tm;

	GdaVirtualProvider *prov;
	gchar *str;
	int rc;
	char *zErrMsg = NULL;
	gboolean retval = TRUE;
	SQLITEcnc *scnc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	if (!scnc) {
		gda_connection_add_event_string ((GdaConnection *) cnc, _("Invalid SQLite handle"));
		return FALSE;
	}

	tm = get_table_model_by_model (cnc, model);
	if (!tm) {
		g_set_error (error, 0, 0,
			     _("Data model to remove does not represent a table"));
		return FALSE;
	}

	prov = (GdaVirtualProvider *) gda_connection_get_provider_obj (GDA_CONNECTION (cnc));
	str = g_strdup_printf ("DROP TABLE %s", tm->table_name);
	rc = sqlite3_exec (scnc->connection, str, NULL, 0, &zErrMsg);
	g_free (str);
	if (rc != SQLITE_OK) {
		g_set_error (error, 0, 0, g_strdup (zErrMsg));
		sqlite3_free (zErrMsg);
		return FALSE;
	}
	else {
		/* clean the cnc->priv->table_models list */
		cnc->priv->table_models = g_slist_remove (cnc->priv->table_models, tm);
		g_object_unref (tm->model);
		g_free (tm->table_name);
		g_free (tm);
		return TRUE;
	}

	return retval;
}

static TableModel *
get_table_model_by_name (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GSList *list;
	for (list = cnc->priv->table_models; list; list = list->next) {
		if (!strcmp (((TableModel*) list->data)->table_name, table_name))
			return (TableModel*) list->data;
	}
	return NULL;
}

static TableModel *
get_table_model_by_model (GdaVconnectionDataModel *cnc, GdaDataModel *model)
{
	GSList *list;
	for (list = cnc->priv->table_models; list; list = list->next) {
		if (((TableModel*) list->data)->model == model)
			return (TableModel*) list->data;
	}
	return NULL;
}

/**
 * gda_vconnection_data_model_get_model
 * @cnc: a #GdaVconnectionDataModel connection
 * @table_name: a table name within @cnc
 *
 * Find the #GdaDataModel object representing the @table_name table in @cnc
 *
 * Returns: the #GdaDataModel, or %NULL if not table named @table_name exists
 */
GdaDataModel *
gda_vconnection_data_model_get_model (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	TableModel *tm;
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!table_name || !(*table_name))
		return NULL;

	tm = get_table_model_by_name (cnc, table_name);
	if (tm)
		return tm->model;
	else
		return NULL;
}

/**
 * gda_vconnection_data_model_foreach
 * @cnc: a #GdaVconnectionDataModel connection
 * @func: a #GdaVConnectionDataModelFunc function pointer
 * @data: data to pass to @cunc calls
 *
 * Call @func for each #GdaDataModel represented as a table in @cnc.
 */
void
gda_vconnection_data_model_foreach (GdaVconnectionDataModel *cnc, 
				    GdaVConnectionDataModelFunc func, gpointer data)
{
	GSList *list, *next;
	g_return_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc));
	g_return_if_fail (cnc->priv);

	if (!func)
		return;

	list = cnc->priv->table_models;
	while (list) {
		TableModel *tm = (TableModel*) list->data;
		next = list->next;
		func (tm->model, tm->table_name, data);
		list = next;
	}
}
