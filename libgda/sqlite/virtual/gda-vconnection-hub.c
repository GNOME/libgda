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
#include "gda-vconnection-hub.h"
#include "gda-virtual-provider.h"
#include <libgda/gda-dict.h>
#include <libgda/gda-dict-database.h>
#include <libgda/gda-dict-table.h>

typedef struct {
	GdaVconnectionHub *hub;
	GdaConnection     *cnc;
	GdaDict           *dict;
	gchar             *ns; /* can be NULL in one case only among all the HubConnection structs */
} HubConnection;

static void hub_connection_free (HubConnection *hc);
static gboolean attach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc, GError **error);
static void detach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc);

struct _GdaVconnectionHubPrivate {
	GSList *hub_connections; /* list of HubConnection structures */
};

/* properties */
enum
{
        PROP_0,
};

static void gda_vconnection_hub_class_init (GdaVconnectionHubClass *klass);
static void gda_vconnection_hub_init       (GdaVconnectionHub *cnc, GdaVconnectionHubClass *klass);
static void gda_vconnection_hub_dispose   (GObject *object);
static void gda_vconnection_hub_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_vconnection_hub_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);
static GObjectClass  *parent_class = NULL;

static HubConnection *get_hub_cnc_by_ns (GdaVconnectionHub *hub, const gchar *ns);
static HubConnection *get_hub_cnc_by_cnc (GdaVconnectionHub *hub, GdaConnection *cnc);

static gchar *get_complete_table_name (HubConnection *hc, GdaDictTable *table);

/*
 * GdaVconnectionHub class implementation
 */
static void
gda_vconnection_hub_class_init (GdaVconnectionHubClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_vconnection_hub_dispose;

	/* Properties */
        object_class->set_property = gda_vconnection_hub_set_property;
        object_class->get_property = gda_vconnection_hub_get_property;
}

static void
gda_vconnection_hub_init (GdaVconnectionHub *cnc, GdaVconnectionHubClass *klass)
{
	cnc->priv = g_new (GdaVconnectionHubPrivate, 1);
	cnc->priv->hub_connections = NULL;
}

static void
gda_vconnection_hub_dispose (GObject *object)
{
	GdaVconnectionHub *cnc = (GdaVconnectionHub *) object;

	g_return_if_fail (GDA_IS_VCONNECTION_HUB (cnc));

	/* free memory */
	if (cnc->priv) {
		gda_connection_close_no_warning ((GdaConnection *) cnc);
		g_assert (!cnc->priv->hub_connections);

		g_free (cnc->priv);
		cnc->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

GType
gda_vconnection_hub_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVconnectionHubClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_vconnection_hub_class_init,
				NULL, NULL,
				sizeof (GdaVconnectionHub),
				0,
				(GInstanceInitFunc) gda_vconnection_hub_init
			};
			
			type = g_type_register_static (GDA_TYPE_VCONNECTION_DATA_MODEL, "GdaVconnectionHub", &info, 0);
		}
	}

	return type;
}

static void
gda_vconnection_hub_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
        GdaVconnectionHub *cnc;

        cnc = GDA_VCONNECTION_HUB (object);
        if (cnc->priv) {
                switch (param_id) {
		default:
			break;
                }
        }
}

static void
gda_vconnection_hub_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec)
{
        GdaVconnectionHub *cnc;

        cnc = GDA_VCONNECTION_HUB (object);
        if (cnc->priv) {
		switch (param_id) {
		default:
			break;
		}
        }
}

/**
 * gda_vconnection_hub_add
 * @hub: a #GdaVconnectionHub connection
 * @cnc: a #GdaConnection
 * @ns: a namespace, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Make all the tables of @cnc appear as tables (of the same name) in the @hub connection.
 * If the @ns is not %NULL, then within @hub, the tables will be accessible using the '@ns.@table_name'
 * notation.
 *
 * Within any instance of @hub, there can be only one added connection where @ns is %NULL.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_hub_add (GdaVconnectionHub *hub, 
			 GdaConnection *cnc, const gchar *ns, GError **error)
{
	HubConnection *hc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (hub), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* check for constraints */
	hc = get_hub_cnc_by_ns (hub, ns);
	if (hc && (hc->cnc != cnc)) {
		g_set_error (error, 0, 0,
			     _("Namespace must be specified"));
		return FALSE;
	}

	if (hc)
		return TRUE;

	if (!gda_connection_is_opened (cnc)) {
		g_set_error (error, 0, 0,
			     _("Connection is closed"));
		return FALSE;
	}

	/* actually adding @cnc */
	hc = g_new (HubConnection, 1);
	hc->hub = hub;
	hc->cnc = cnc;
	g_object_ref (cnc);
	hc->dict = NULL;
	hc->ns = ns ? g_strdup (ns) : NULL;
	
	if (!attach_hub_connection (hub, hc, error)) {
		hub_connection_free (hc);
		return FALSE;
	}

	return TRUE;
}

/**
 * gda_vconnection_hub_remove
 * @hub: a #GdaVconnectionHub connection
 * @cnc: a #GdaConnection
 * @error: a place to store errors, or %NULL
 *
 * Remove all the tables in @hub representing @cnc's tables.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_hub_remove (GdaVconnectionHub *hub, GdaConnection *cnc, GError **error)
{
	HubConnection *hc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (hub), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	hc = get_hub_cnc_by_cnc (hub, cnc);

	if (!hc) {
		g_set_error (error, 0, 0,
			     _("Connection was not represented in hub"));
		return FALSE;
	}

	/* clean the hub->priv->hub_connections list */
	detach_hub_connection (hub, hc);
	return TRUE;
}

static  HubConnection*
get_hub_cnc_by_ns (GdaVconnectionHub *hub, const gchar *ns)
{
	GSList *list;
	for (list = hub->priv->hub_connections; list; list = list->next) {
		if ((!ns && !((HubConnection*) list->data)->ns)|| 
		    (ns && ((HubConnection*) list->data)->ns && !strcmp (((HubConnection*) list->data)->ns, ns)))
			return (HubConnection*) list->data;
	}
	return NULL;
}

static HubConnection *
get_hub_cnc_by_cnc (GdaVconnectionHub *hub, GdaConnection *cnc)
{
	GSList *list;
	for (list = hub->priv->hub_connections; list; list = list->next) {
		if (((HubConnection*) list->data)->cnc == cnc)
			return (HubConnection*) list->data;
	}
	return NULL;
}

/**
 * gda_vconnection_hub_get_connection
 * @hub: a #GdaVconnectionHub connection
 * @ns: a name space, or %NULL
 *
 * Find the #GdaConnection object in @hub associated to the @ns name space
 *
 * Returns: the #GdaConnection, or %NULL if no connection is associated to @ns
 */
GdaConnection *
gda_vconnection_hub_get_connection (GdaVconnectionHub *hub, const gchar *ns)
{
	HubConnection *hc;
	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (hub), NULL);
	g_return_val_if_fail (hub->priv, NULL);

	hc = get_hub_cnc_by_ns (hub, ns);
	if (hc)
		return hc->cnc;
	else
		return NULL;
}

/**
 * gda_vconnection_hub_foreach
 * @hub: a #GdaVconnectionHub connection
 * @func: a #GdaVConnectionDataModelFunc function pointer
 * @data: data to pass to @cunc calls
 *
 * Call @func for each #GdaConnection represented in @hub.
 */
void
gda_vconnection_hub_foreach (GdaVconnectionHub *hub, 
			     GdaVConnectionHubFunc func, gpointer data)
{
	GSList *list, *next;
	g_return_if_fail (GDA_IS_VCONNECTION_HUB (hub));
	g_return_if_fail (hub->priv);

	if (!func)
		return;

	list = hub->priv->hub_connections;
	while (list) {
		HubConnection *hc = (HubConnection*) list->data;
		next = list->next;
		func (hc->cnc, hc->ns, data);
		list = next;
	}
}

static void table_added_cb (GdaDictDatabase *gdadictdatabase, GdaDictTable *table, HubConnection *hc);
static void table_removed_cb (GdaDictDatabase *gdadictdatabase, GdaDictTable *table, HubConnection *hc);
static void table_updated_cb (GdaDictDatabase *gdadictdatabase, GdaDictTable *table, HubConnection *hc);

typedef struct {
	GdaVconnectionDataModelSpec spec;
	GdaDictTable *table;
	HubConnection *hc;
} LocalSpec;

static GList *
dict_table_create_columns_func (GdaVconnectionDataModelSpec *spec)
{
	gint i, ncols;
	GList *columns = NULL;
	GdaDictTable *table = GDA_DICT_TABLE (((LocalSpec*)spec)->table);
	ncols = gda_entity_get_n_fields (GDA_ENTITY (table));
	for (i = 0; i < ncols; i++) {
		GdaEntityField *field = gda_entity_get_field_by_index ((GdaEntity *) table, i);
		GdaColumn *col;
		col = gda_column_new ();
		gda_column_set_name (col, gda_entity_field_get_name (field));
		gda_column_set_g_type (col, gda_entity_field_get_g_type (field));
		gda_column_set_dbms_type (col, gda_dict_type_get_sqlname (gda_entity_field_get_dict_type (field)));
		columns = g_list_prepend (columns, col);
	}
	return g_list_reverse (columns);
}
static GdaDataModel *
dict_table_create_model_func (GdaVconnectionDataModelSpec *spec)
{
	GdaDataModel *model;
	GdaQuery *query;
	const gchar *table_name;
	gchar *tmp;
	LocalSpec *lspec = (LocalSpec *) spec;
	
	table_name = gda_object_get_name ((GdaObject*) lspec->table);
	tmp = g_strdup_printf ("SELECT * FROM %s", table_name);
	query = gda_query_new_from_sql (lspec->hc->dict, tmp, NULL);
	g_free (tmp);
	model = gda_data_model_query_new (query);
	g_object_unref (query);
	gda_data_model_query_compute_modification_queries (GDA_DATA_MODEL_QUERY (model), table_name, 
							   GDA_DATA_MODEL_QUERY_OPTION_USE_ALL_FIELDS_IF_NO_PK, NULL);

	return model;
}

static gboolean
attach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc, GError **error)
{
	GdaDictDatabase *db;
	GSList *tables, *list;
	gchar *tmp;
	
	/* make sure there is a GdaDict */
	if (!hc->dict) {
		hc->dict = gda_dict_new ();
		gda_dict_set_connection (hc->dict, hc->cnc);
		if (!gda_dict_update_dbms_meta_data (hc->dict, GDA_TYPE_DICT_TABLE, NULL, error))
			return FALSE;
	}

	/* add a :memory: database */
	if (hc->ns) {
		GdaCommand *command;
		GList *list;
		tmp = g_strdup_printf ("ATTACH ':memory:' AS %s", hc->ns);
		command = gda_command_new (tmp, GDA_COMMAND_TYPE_SQL, 0);
		list = gda_connection_execute_command (GDA_CONNECTION (hub), command, NULL, error);
		g_free (tmp);
		if (!list)
			return FALSE;
		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);
	}

	/* add virtual tables */
	db = gda_dict_get_database (hc->dict);
	tables = gda_dict_database_get_tables (db);
	for (list = tables; list; list = list->next) {
		LocalSpec *lspec;
		lspec = g_new0 (LocalSpec, 1);
		GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->data_model = NULL;
		GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_columns_func = (GdaVconnectionDataModelCreateColumnsFunc) dict_table_create_columns_func;
		GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_model_func = (GdaVconnectionDataModelCreateModelFunc) dict_table_create_model_func;
		lspec->table = GDA_DICT_TABLE (list->data);
		lspec->hc = hc;
		tmp = get_complete_table_name (hc, GDA_DICT_TABLE (list->data));
		if (!gda_vconnection_data_model_add (GDA_VCONNECTION_DATA_MODEL (hub), (GdaVconnectionDataModelSpec*) lspec, 
							  g_free, tmp, error)) {
			g_free (tmp);
			return FALSE;
		}
		g_free (tmp);		
	}
	g_slist_free (tables);

	/* monitor changes in dictionary */
	g_signal_connect (db, "table-added",
			  G_CALLBACK (table_added_cb), hc);
	g_signal_connect (db, "table-removed",
			  G_CALLBACK (table_removed_cb), hc);
	g_signal_connect (db, "table-updated",
			  G_CALLBACK (table_updated_cb), hc);

	hub->priv->hub_connections = g_slist_append (hub->priv->hub_connections, hc);
	return TRUE;
}

static gchar *
get_complete_table_name (HubConnection *hc, GdaDictTable *table)
{
	if (hc->ns)
		return g_strdup_printf ("%s.%s", hc->ns, gda_object_get_name ((GdaObject *) table));
	else
		return g_strdup (gda_object_get_name ((GdaObject *) table));
}

static void
table_added_cb (GdaDictDatabase *gdadictdatabase, GdaDictTable *table, HubConnection *hc)
{
	LocalSpec *lspec;
	GError *error = NULL;
	gchar *tmp;

	lspec = g_new0 (LocalSpec, 1);
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->data_model = NULL;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_columns_func = (GdaVconnectionDataModelCreateColumnsFunc) dict_table_create_columns_func;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_model_func = (GdaVconnectionDataModelCreateModelFunc) dict_table_create_model_func;
	lspec->table = table;
	lspec->hc = hc;
	tmp = get_complete_table_name (hc, table);
	if (!gda_vconnection_data_model_add (GDA_VCONNECTION_DATA_MODEL (hc->hub), (GdaVconnectionDataModelSpec*) lspec, 
					     g_free, tmp, &error)) {
		gda_connection_add_event_string (GDA_CONNECTION (hc->hub), _("Could not add virtual table %s: %s"), 
						 tmp, error && error->message ? error->message : _("No detail"));
		g_error_free (error);
	}
	g_free (tmp);
}

static void
table_removed_cb (GdaDictDatabase *gdadictdatabase, GdaDictTable *table, HubConnection *hc)
{
	gchar *name;

	name = get_complete_table_name (hc, table);
	gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (hc->hub), name, NULL);
	g_free (name);
}

static void
table_updated_cb (GdaDictDatabase *gdadictdatabase, GdaDictTable *table, HubConnection *hc)
{
	table_removed_cb (gdadictdatabase, table, hc);
	table_added_cb (gdadictdatabase, table, hc);
}

static void
detach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc)
{
	GdaDictDatabase *db;
	GSList *tables, *list;
	gchar *tmp;

	/* unmonitor changes in dictionary */
	db = gda_dict_get_database (hc->dict);
	g_signal_handlers_disconnect_by_func (db, G_CALLBACK (table_added_cb), hc);
	g_signal_handlers_disconnect_by_func (db, G_CALLBACK (table_removed_cb), hc);
	g_signal_handlers_disconnect_by_func (db, G_CALLBACK (table_updated_cb), hc);

	/* remove virtual tables */
	tables = gda_dict_database_get_tables (db);
	for (list = tables; list; list = list->next) {
		tmp = get_complete_table_name (hc, GDA_DICT_TABLE (list->data));
		gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (hub), tmp, NULL);
		g_free (tmp);
	}
	g_slist_free (tables);

	/* remove the :memory: database */
	if (hc->ns) {
		GdaCommand *command;
		GList *list;
		tmp = g_strdup_printf ("DETACH %s", hc->ns);
		command = gda_command_new (tmp, GDA_COMMAND_TYPE_SQL, 0);
		list = gda_connection_execute_command (GDA_CONNECTION (hub), command, NULL, NULL);
		g_free (tmp);
		if (list) {
			g_list_foreach (list, (GFunc) g_object_unref, NULL);
			g_list_free (list);
		}
	}	

	hub->priv->hub_connections = g_slist_remove (hub->priv->hub_connections, hc);
	hub_connection_free (hc);
}

static void
hub_connection_free (HubConnection *hc)
{
	if (hc->dict)
		g_object_unref (hc->dict);
	g_object_unref (hc->cnc);
	g_free (hc->ns);
	g_free (hc);
}
