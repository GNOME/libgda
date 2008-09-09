/* 
 * GDA common library
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
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
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-util.h>
#include <libgda/gda-data-select.h>

typedef struct {
	GdaVconnectionHub *hub;
	GdaConnection     *cnc;
	gchar             *ns; /* can be NULL in one case only among all the HubConnection structs */
} HubConnection;

static void hub_connection_free (HubConnection *hc);
static gboolean attach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc, GError **error);
static void detach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc);

static GdaSqlParser *internal_parser;

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

	/* static objects */
	internal_parser = gda_sql_parser_new ();
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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
			
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VCONNECTION_DATA_MODEL, "GdaVconnectionHub", &info, 0);
		g_static_mutex_unlock (&registering);
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

static void meta_changed_cb (GdaMetaStore *store, GSList *changes, HubConnection *hc);

typedef struct {
	GdaVconnectionDataModelSpec spec;
	GValue *table_name;
	HubConnection *hc;
} LocalSpec;

static void local_spec_free (LocalSpec *spec)
{
	gda_value_free (spec->table_name);
	g_free (spec);
}

static GList *
dict_table_create_columns_func (GdaVconnectionDataModelSpec *spec, GError **error)
{
	LocalSpec *lspec = (LocalSpec *) spec;
	gint i, nrows;
	GList *columns = NULL;
	GdaDataModel *model;

	model = gda_connection_get_meta_store_data (lspec->hc->cnc, 
						    GDA_CONNECTION_META_FIELDS, NULL, 1, "name", lspec->table_name);
	if (!model)
		return NULL;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		GdaColumn *col;
		const GValue *v0, *v1, *v2;

		v0 = gda_data_model_get_value_at (model, 0, i, error);
		v1 = gda_data_model_get_value_at (model, 1, i, error);
		v2 = gda_data_model_get_value_at (model, 2, i, error);

		if (!v0 || !v1 || !v2) {
			if (columns) {
				g_list_foreach (columns, (GFunc) g_object_unref, NULL);
				g_list_free (columns);
				columns = NULL;
			}
			break;
		}

		col = gda_column_new ();
		gda_column_set_name (col, g_value_get_string (v0));
		gda_column_set_g_type (col, gda_g_type_from_string (g_value_get_string (v2)));
		gda_column_set_dbms_type (col, g_value_get_string (v1));
		columns = g_list_prepend (columns, col);
	}
	g_object_unref (model);

	return g_list_reverse (columns);
}

static GdaDataModel *
dict_table_create_model_func (GdaVconnectionDataModelSpec *spec)
{
	GdaDataModel *model;
	GdaStatement *stmt;
	gchar *tmp;
	LocalSpec *lspec = (LocalSpec *) spec;
	
	tmp = g_strdup_printf ("SELECT * FROM %s", g_value_get_string (lspec->table_name));
	stmt = gda_sql_parser_parse_string (internal_parser, tmp, NULL, NULL);
	g_free (tmp);
	model = gda_connection_statement_execute_select (lspec->hc->cnc, stmt, NULL, NULL);
	g_object_unref (stmt);
	if (model) 
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), NULL);

	return model;
}
static gboolean table_add (HubConnection *hc, const GValue *table_name, GError **error);
static void table_remove (HubConnection *hc, const GValue *table_name);
static gchar *get_complete_table_name (HubConnection *hc, const GValue *table_name);

static gboolean
attach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc, GError **error)
{
	gchar *tmp;
	GdaMetaStore *store;
	GdaMetaContext context;
	
	g_object_get (G_OBJECT (hc->cnc), "meta-store", &store, NULL);
	g_assert (store);

	/* make sure the meta store is up to date */
	context.table_name = "_tables";
	context.size = 0;
	if (!gda_connection_update_meta_store (hc->cnc, &context, error))
		return FALSE;

	/* add a :memory: database */
	if (hc->ns) {
		GdaStatement *stmt;
		tmp = g_strdup_printf ("ATTACH ':memory:' AS %s", hc->ns);
		stmt = gda_sql_parser_parse_string (internal_parser, tmp, NULL, NULL);
		g_free (tmp);
		g_assert (stmt);
		if (gda_connection_statement_execute_non_select (GDA_CONNECTION (hub), stmt, NULL, NULL, error) == -1) {
			g_object_unref (stmt);
			return FALSE;
		}
		g_object_unref (stmt);
	}

	/* add virtual tables */
	GdaDataModel *model;
	gint i, nrows;
	model = gda_connection_get_meta_store_data (hc->cnc, GDA_CONNECTION_META_TABLES, error, 0);
	if (!model)
		return FALSE;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cv = gda_data_model_get_value_at (model, 0, i, error);
		if (!cv || !table_add (hc, cv, error)) {
			g_object_unref (model);
			return FALSE;
		}
	}
	g_object_unref (model);

	/* monitor changes */
	g_signal_connect (store, "meta-changed", G_CALLBACK (meta_changed_cb), hc);

	hub->priv->hub_connections = g_slist_append (hub->priv->hub_connections, hc);
	return TRUE;
}

static gchar *
get_complete_table_name (HubConnection *hc, const GValue *table_name)
{
	if (hc->ns)
		return g_strdup_printf ("%s.%s", hc->ns, g_value_get_string (table_name));
	else
		return g_strdup (g_value_get_string (table_name));
}

static void
meta_changed_cb (GdaMetaStore *store, GSList *changes, HubConnection *hc)
{
	GSList *list;
	for (list = changes; list; list = list->next) {
		GdaMetaStoreChange *ch = (GdaMetaStoreChange*) list->data;
		GValue *tsn, *tn;
			
		/* we are only intsrested in changes occuring in the "_tables" table */
		if (!strcmp (ch->table_name, "_tables")) {
			switch (ch->c_type) {
			case GDA_META_STORE_ADD: {
				/* we only want tables where table_short_name = table_name */
				tsn = g_hash_table_lookup (ch->keys, "+6");
				tn = g_hash_table_lookup (ch->keys, "+2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_add (hc, tn, NULL);
				break;
			}
			case GDA_META_STORE_REMOVE: {
				/* we only want tables where table_short_name = table_name */
				tsn = g_hash_table_lookup (ch->keys, "-6");
				tn = g_hash_table_lookup (ch->keys, "-2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_remove (hc, tn);
				break;
			}
			case GDA_META_STORE_MODIFY: {
				/* we only want tables where table_short_name = table_name */
				tsn = g_hash_table_lookup (ch->keys, "-6");
				tn = g_hash_table_lookup (ch->keys, "-2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_remove (hc, tn);
				tsn = g_hash_table_lookup (ch->keys, "+6");
				tn = g_hash_table_lookup (ch->keys, "+2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_add (hc, tn, NULL);
				break;
			}
			}
		}
		else if (!strcmp (ch->table_name, "_columns")) {
			TO_IMPLEMENT;
		}
	}
}

static gboolean
table_add (HubConnection *hc, const GValue *table_name, GError **error)
{
	LocalSpec *lspec;
	gchar *tmp;

	lspec = g_new0 (LocalSpec, 1);
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->data_model = NULL;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_columns_func = (GdaVconnectionDataModelCreateColumnsFunc) dict_table_create_columns_func;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_model_func = (GdaVconnectionDataModelCreateModelFunc) dict_table_create_model_func;
	lspec->table_name = gda_value_copy (table_name);
	lspec->hc = hc;
	tmp = get_complete_table_name (hc, lspec->table_name);
	if (!gda_vconnection_data_model_add (GDA_VCONNECTION_DATA_MODEL (hc->hub), (GdaVconnectionDataModelSpec*) lspec, 
					     (GDestroyNotify) local_spec_free, tmp, error)) {
		g_free (tmp);
		return FALSE;
	}
	g_free (tmp);
	return TRUE;
}

static void
table_remove (HubConnection *hc, const GValue *table_name)
{
	gchar *name;

	name = get_complete_table_name (hc, table_name);
	gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (hc->hub), name, NULL);
	g_free (name);
}

static void
detach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc)
{
	GdaMetaStore *store;
	GdaDataModel *model;
	gint i, nrows;

	/* un-monitor changes */
	g_object_get (G_OBJECT (hc->cnc), "meta-store", &store, NULL);
	g_assert (store);
	g_signal_handlers_disconnect_by_func (store, G_CALLBACK (meta_changed_cb), hc);

	/* remove virtual tables */
	model = gda_connection_get_meta_store_data (hc->cnc, GDA_CONNECTION_META_TABLES, NULL, 0);
	if (!model)
		return;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cv = gda_data_model_get_value_at (model, 0, i, NULL);
		if (cv)
			table_remove (hc, cv);
	}
	g_object_unref (model);

	/* remove the :memory: database */
	if (hc->ns) {
		GdaStatement *stmt;
		gchar *tmp;
		tmp = g_strdup_printf ("DETACH %s", hc->ns);
		stmt = gda_sql_parser_parse_string (internal_parser, tmp, NULL, NULL);
		g_free (tmp);
		g_assert (stmt);
		gda_connection_statement_execute_non_select (GDA_CONNECTION (hub), stmt, NULL, NULL, NULL);
		g_object_unref (stmt);
	}	

	hub->priv->hub_connections = g_slist_remove (hub->priv->hub_connections, hc);
	hub_connection_free (hc);
}

static void
hub_connection_free (HubConnection *hc)
{
	g_object_unref (hc->cnc);
	g_free (hc->ns);
	g_free (hc);
}
