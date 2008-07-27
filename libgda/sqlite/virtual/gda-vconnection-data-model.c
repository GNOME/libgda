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
#include "gda-vconnection-data-model-private.h"
#include "gda-virtual-provider.h"
#include <libgda/gda-connection-private.h>
#include "../gda-sqlite.h"

struct _GdaVconnectionDataModelPrivate {
	GSList *table_data_list; /* list of GdaVConnectionTableData structures */
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
	cnc->priv->table_data_list = NULL;

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
		g_assert (!cnc->priv->table_data_list);

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

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
			
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VIRTUAL_CONNECTION, "GdaVconnectionDataModel", &info, 0);
		g_static_mutex_unlock (&registering);
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

static void
spec_destroy_func (GdaVconnectionDataModelSpec *spec)
{
	g_object_unref (spec->data_model);
	g_free (spec);
}

/**
 * gda_vconnection_data_model_add_model
 * @cnc: a #GdaVconnectionDataModel connection
 * @model: a #GdaDataModel
 * @table_name: the name of the table
 * @error: a place to store errors, or %NULL
 *
 * Make @model appear as a table named @table_name in the @cnc connection (as if a
 * "CREATE TABLE..." statement was executed, except that the data contained within @model
 * is actually used when @table_name's contents is read or written).
 *
 * For a more general approach, see the gda_vconnection_data_model_add() method.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_add_model (GdaVconnectionDataModel *cnc, 
				      GdaDataModel *model, const gchar *table_name, GError **error)
{
	GdaVconnectionDataModelSpec *spec;
	gboolean retval;

	spec = g_new0 (GdaVconnectionDataModelSpec, 1);
	spec->data_model = model;
	g_object_ref (model);
	retval = gda_vconnection_data_model_add (cnc, spec, (GDestroyNotify) spec_destroy_func, table_name, error);

	return retval;
}

/**
 * gda_vconnection_data_model_add
 * @cnc: a #GdaVconnectionDataModel connection
 * @spec: a #GdaVconnectionDataModelSpec structure
 * @spec_free_func: function to call when freeing @spec, or %NULL
 * @table_name: the name of the table
 * @error: a place to store errors, or %NULL
 *
 * Create a new virtual table named @table_name in @cnc. The contents of that new table
 * is dictated by what's in @spec.
 *
 * If there is just one #GdaDataModel to make appear as a table
 * then the gda_vconnection_data_model_add_model() method is easier to use.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_add (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelSpec *spec, 
				GDestroyNotify spec_free_func, const gchar *table_name, GError **error)
{
	GdaVirtualProvider *prov;
	gchar *str;
	int rc;
	char *zErrMsg = NULL;
	gboolean retval = TRUE;
	SqliteConnectionData *scnc;

	static gint counter = 0;

	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);
	g_return_val_if_fail (spec, FALSE);
	g_return_val_if_fail (spec->data_model || (spec->create_columns_func && spec->create_model_func), FALSE);
	if (spec->data_model)
		g_return_val_if_fail (GDA_IS_DATA_MODEL (spec->data_model), FALSE);

	scnc = (SqliteConnectionData*) gda_connection_internal_get_provider_data ((GdaConnection *) cnc);
	if (!scnc) 
		return FALSE;

	/* create a new GdaVConnectionTableData structure for this virtual table */
	GdaVConnectionTableData *td;
	td = g_new0 (GdaVConnectionTableData, 1);
	td->spec = spec;
	td->spec_free_func = spec_free_func;
	td->table_name = g_strdup (table_name);
	td->unique_name = g_strdup_printf ("Spec%d", counter++);
	cnc->priv->table_data_list = g_slist_append (cnc->priv->table_data_list, td);

	/* actually create the virtual table in @cnc */
	prov = (GdaVirtualProvider *) gda_connection_get_provider_obj (GDA_CONNECTION (cnc));
	str = g_strdup_printf ("CREATE VIRTUAL TABLE %s USING %s ('%s')", table_name, G_OBJECT_TYPE_NAME (prov), td->unique_name);
	rc = sqlite3_exec (scnc->connection, str, NULL, 0, &zErrMsg);
	g_free (str);
	if (rc != SQLITE_OK) {
		g_set_error (error, 0, 0, g_strdup (zErrMsg));
		sqlite3_free (zErrMsg);
		gda_vconnection_data_model_table_data_free (td);
		cnc->priv->table_data_list = g_slist_remove (cnc->priv->table_data_list, td);
		retval = FALSE;
	}
	/*
	else 
		g_print ("Virtual connection: added table %s (model = %p)\n", td->table_name, td->spec->data_model);
	*/


	return retval;
}


/**
 * gda_vconnection_data_model_remove
 * @cnc: a #GdaVconnectionDataModel connection
 * @table_name: the name of the table to remove from @cnc
 * @error: a place to store errors, or %NULL
 *
 * Remove the table named @table_name in the @cnc connection (as if a "DROP TABLE..."
 * statement was executed, except that no data gets destroyed as the associated data model remains the same).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_remove (GdaVconnectionDataModel *cnc, const gchar *table_name, GError **error)
{
	GdaVConnectionTableData *td;

	GdaVirtualProvider *prov;
	gchar *str;
	int rc;
	char *zErrMsg = NULL;
	gboolean retval = TRUE;
	SqliteConnectionData *scnc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	scnc = (SqliteConnectionData*) gda_connection_internal_get_provider_data ((GdaConnection *) cnc);
	if (!scnc) 
		return FALSE;

	td = gda_vconnection_get_table_data_by_name (cnc, table_name);
	if (!td) {
		g_set_error (error, 0, 0,
			     _("Table to remove not found"));
		return FALSE;
	}
	
	prov = (GdaVirtualProvider *) gda_connection_get_provider_obj (GDA_CONNECTION (cnc));
	str = g_strdup_printf ("DROP TABLE %s", td->table_name);
	rc = sqlite3_exec (scnc->connection, str, NULL, 0, &zErrMsg);
	g_free (str);

	if (rc != SQLITE_OK) {
		g_set_error (error, 0, 0, g_strdup (zErrMsg));
		sqlite3_free (zErrMsg);
		return FALSE;
	}
	else {
		/* clean the cnc->priv->table_data_list list */
		cnc->priv->table_data_list = g_slist_remove (cnc->priv->table_data_list, td);
		/*g_print ("Virtual connection: removed table %s (%p)\n", td->table_name, td->spec->data_model);*/
		gda_vconnection_data_model_table_data_free (td);
		return TRUE;
	}

	return retval;
}

/**
 * gda_vconnection_data_model_get_model
 * @cnc: a #GdaVconnectionDataModel connection
 * @table_name: a table name within @cnc
 *
 * Find the #GdaDataModel object representing the @table_name table in @cnc
 *
 * Returns: the #GdaDataModel, or %NULL if no table named @table_name exists
 */
GdaDataModel *
gda_vconnection_data_model_get_model (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GdaVConnectionTableData *td;
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!table_name || !(*table_name))
		return NULL;

	td = gda_vconnection_get_table_data_by_name (cnc, table_name);
	if (td)
		return td->spec->data_model;
	else
		return NULL;
}

/**
 * gda_vconnection_data_model_get_table_name
 * @cnc: a #GdaVconnectionDataModel connection
 * @model: a #GdaDataModel representing a table within @cnc
 *
 * Find the name of the table associated to @model in @cnc
 *
 * Returns: the table name, or %NULL if not found
 */
const gchar *
gda_vconnection_data_model_get_table_name (GdaVconnectionDataModel *cnc, GdaDataModel *model)
{
	GdaVConnectionTableData *td;
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!model)
		return NULL;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	td = gda_vconnection_get_table_data_by_model (cnc, model);
	if (td)
		return td->table_name;
	else
		return NULL;
}

/**
 * gda_vconnection_data_model_foreach
 * @cnc: a #GdaVconnectionDataModel connection
 * @func: a #GdaVConnectionDataModelFunc function pointer
 * @data: data to pass to @cunc calls
 *
 * Call @func for each table in @cnc. 
 *
 * Warning: @func will be called for any table present in @cnc even if no data
 * model represents the contents of the table (which means the 1st argument of @func
 * may be %NULL)
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

	list = cnc->priv->table_data_list;
	while (list) {
		GdaVConnectionTableData *td = (GdaVConnectionTableData*) list->data;
		next = list->next;
		func (td->spec->data_model, td->table_name, data);
		list = next;
	}
}

/* 
 * private 
 */
GdaVConnectionTableData *
gda_vconnection_get_table_data_by_name (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GSList *list;
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		if (!strcmp (((GdaVConnectionTableData*) list->data)->table_name, table_name))
			return (GdaVConnectionTableData*) list->data;
	}
	return NULL;
}

GdaVConnectionTableData *
gda_vconnection_get_table_data_by_unique_name (GdaVconnectionDataModel *cnc, const gchar *unique_name)
{
	GSList *list;
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		if (!strcmp (((GdaVConnectionTableData*) list->data)->unique_name, unique_name))
			return (GdaVConnectionTableData*) list->data;
	}
	return NULL;
}

GdaVConnectionTableData *
gda_vconnection_get_table_data_by_model (GdaVconnectionDataModel *cnc, GdaDataModel *model)
{
	GSList *list;
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		if (((GdaVConnectionTableData*) list->data)->real_model == model)
			return (GdaVConnectionTableData*) list->data;
	}
	return NULL;
}

void
gda_vconnection_data_model_table_data_free (GdaVConnectionTableData *td)
{
	if (td->real_model)
		g_object_unref (td->real_model);
	g_free (td->table_name);
	g_free (td->unique_name);
	if (td->spec_free_func)
		td->spec_free_func (td->spec);
	g_free (td);
}
