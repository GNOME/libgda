/* gda-dict.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-dict.h"
#include "gda-marshal.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <libgda/gda-object.h>
#include "gda-connection.h"
#include "gda-data-handler.h"
#include "gda-dict-database.h"
#include "gda-dict-table.h"
#include "gda-dict-type.h"
#include "gda-dict-aggregate.h"
#include "gda-dict-function.h"
#include "gda-xml-storage.h"
#include "gda-query.h"
#include "gda-referer.h"
#include "gda-entity.h"
#include "graph/gda-graph.h"
#include "graph/gda-graph-query.h"
#include <libgda/gda-util.h>
#include "gda-server-provider.h"

#include "handlers/gda-handler-boolean.h"
#include "handlers/gda-handler-numerical.h"
#include "handlers/gda-handler-string.h"
#include "handlers/gda-handler-time.h"

#if defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED) && \
    defined(LIBXML_OUTPUT_ENABLED)
#define XML_ID_TEST 
#endif

extern xmlDtdPtr gda_dict_dtd;

static gboolean 
LC_NAMES (GdaDict *dict) 
{
	GdaConnection *cnc;
	GdaServerProviderInfo *sinfo = NULL;
	
	cnc = gda_dict_get_connection (dict);
        if (cnc)
                sinfo = gda_connection_get_infos (cnc);

	return (sinfo && sinfo->is_case_insensitive);

}

/* 
 * Main static functions 
 */
static void gda_dict_class_init (GdaDictClass * class);
static void gda_dict_init (GdaDict * srv);
static void gda_dict_dispose (GObject   * object);
static void gda_dict_finalize (GObject   * object);

static void gda_dict_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec);
static void gda_dict_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec);

static gboolean gda_dict_load_data_types (GdaDict *dict, xmlNodePtr types, GError **error);
static gboolean gda_dict_load_procedures (GdaDict *dict, xmlNodePtr procs, GError **error);
static gboolean gda_dict_load_aggregates (GdaDict *dict, xmlNodePtr aggs, GError **error);
static gboolean gda_dict_load_queries (GdaDict *dict, xmlNodePtr queries, GError **error);
static gboolean gda_dict_load_graphs (GdaDict *dict, xmlNodePtr graphs, GError **error);

static void query_destroyed_cb (GdaQuery *query, GdaDict *dict);
static void query_weak_ref_notify (GdaDict *dict, GdaQuery *query);
static void updated_query_cb (GdaQuery *query, GdaDict *dict);
static void dsn_changed_cb (GdaConnection *cnc, GdaDict *dict);


/* FIXME : rename functions */
static void destroyed_data_type_cb (GdaDictType *dt, GdaDict *dict);
static void destroyed_function_cb (GdaDictFunction *func, GdaDict *dict);
static void destroyed_aggregate_cb (GdaDictAggregate *agg, GdaDict *dict);

static void updated_data_type_cb (GdaDictType *dt, GdaDict *dict);
static void updated_function_cb (GdaDictFunction *func, GdaDict *dict);
static void updated_aggregate_cb (GdaDictAggregate *agg, GdaDict *dict);

static void graph_destroyed_cb (GdaGraph *graph, GdaDict *dict);
static void graph_weak_ref_notify (GdaDict *dict, GdaGraph *graph);
static void updated_graph_cb (GdaGraph *graph, GdaDict *dict);

static void dict_changed (GdaDict *dict, gpointer data);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* default handlers for all dictionaries, NULL until it has been initialised */
static GdaDataHandler **default_dict_handlers = NULL;

/* signals */
enum
{
	DATA_TYPE_ADDED,
        DATA_TYPE_REMOVED,
        DATA_TYPE_UPDATED,
        FUNCTION_ADDED,
        FUNCTION_REMOVED,
        FUNCTION_UPDATED,
        AGGREGATE_ADDED,
        AGGREGATE_REMOVED,
        AGGREGATE_UPDATED,
        DATA_UPDATE_STARTED,
        UPDATE_PROGRESS,
        DATA_UPDATE_FINISHED,
	QUERY_ADDED,
	QUERY_REMOVED,
	QUERY_UPDATED,
	GRAPH_ADDED,
	GRAPH_REMOVED,
	GRAPH_UPDATED,
	CHANGED,
	LAST_SIGNAL
};

static gint gda_dict_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_SERIAL_QUERY,
	PROP_SERIAL_GRAPH,
	PROP_WITH_FUNCTIONS,
	PROP_DSN,
	PROP_USERNAME
};

/* NOTE about referencing data type objects:
 *
 * There are 2 kinds of data types: the ones returned by the GdaConnection schema data types list and
 * the ones added by gda_dict_declare_custom_data_type().
 * 
 * Data types of the first category are:
 * -> listed in priv->data_types
 * -> referenced (using g_object_ref())
 * -> not listed in priv->custom_types
 *
 * Data types of the 2nd category (custom ones) are:
 * -> listed in priv->data_types
 * -> NOT referenced
 * -> listed in priv->custom_types
 *
 * When a database sync occurs, if a data type which was a custom data type is found, then
 * it becomes a data type of the 1st category.
 */

struct _GdaDictPrivate
{
	GdaDictDatabase   *database;
	GdaConnection     *cnc;
	gchar             *xml_filename;
	guint              serial_query; /* counter */
	guint              serial_graph; /* counter */

	gboolean           with_functions;

	gchar             *dsn;
	gchar             *user;

	/* DBMS update related information */
	gboolean           update_in_progress;
	gboolean           stop_update; /* TRUE if a DBMS data update must be stopped */

	/* DBMS objects */
	GSList            *users;
        GSList            *data_types;
	GSList            *custom_types; /* types here also listed in @data_types, not refed in this list */
        GSList            *functions;
        GSList            *aggregates;

	GSList            *assumed_queries;   /* list of GdaQuery objects */
	GSList            *all_queries;

	GSList            *assumed_graphs;   /* list of GdaGraph objects */
	GSList            *all_graphs;

	/* Hash tables for fast retreival of any object */
	GHashTable        *object_ids; /* key = string ID, value = GdaObject for that ID */
};

/* module error */
GQuark gda_dict_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_error");
	return quark;
}


GType
gda_dict_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDictClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_class_init,
			NULL,
			NULL,
			sizeof (GdaDict),
			0,
			(GInstanceInitFunc) gda_dict_init
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GdaDict", &info, 0);
	}
	return type;
}

static void
gda_dict_class_init (GdaDictClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_dict_signals[DATA_TYPE_ADDED] =
                g_signal_new ("data_type_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, data_type_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[DATA_TYPE_REMOVED] =
                g_signal_new ("data_type_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, data_type_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[DATA_TYPE_UPDATED] =
                g_signal_new ("data_type_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, data_type_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[FUNCTION_ADDED] =
                g_signal_new ("function_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, function_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_dict_signals[FUNCTION_REMOVED] =
                g_signal_new ("function_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, function_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[FUNCTION_UPDATED] =
                g_signal_new ("function_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, function_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[AGGREGATE_ADDED] =
                g_signal_new ("aggregate_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, aggregate_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[AGGREGATE_REMOVED] =
                g_signal_new ("aggregate_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, aggregate_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_dict_signals[AGGREGATE_UPDATED] =
                g_signal_new ("aggregate_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, aggregate_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_signals[DATA_UPDATE_STARTED] =
                g_signal_new ("data_update_started",
                              G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, data_update_started),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        gda_dict_signals[UPDATE_PROGRESS] =
                g_signal_new ("update_progress",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, update_progress),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER_UINT_UINT,
                              G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT);
        gda_dict_signals[DATA_UPDATE_FINISHED] =
                g_signal_new ("data_update_finished",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictClass, data_update_finished),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_dict_signals[QUERY_ADDED] =
		g_signal_new ("query_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, query_added),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[QUERY_REMOVED] =
		g_signal_new ("query_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, query_removed),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[QUERY_UPDATED] =
		g_signal_new ("query_updated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, query_updated),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[GRAPH_ADDED] =
		g_signal_new ("graph_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, graph_added),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[GRAPH_REMOVED] =
		g_signal_new ("graph_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, graph_removed),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[GRAPH_UPDATED] =
		g_signal_new ("graph_updated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, graph_updated),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	class->query_added = (void (*) (GdaDict *, GdaQuery *)) dict_changed;
	class->query_removed = (void (*) (GdaDict *, GdaQuery *)) dict_changed;
	class->query_updated = (void (*) (GdaDict *, GdaQuery *)) dict_changed;
	class->graph_added = (void (*) (GdaDict *, GdaGraph *)) dict_changed;
	class->graph_removed = (void (*) (GdaDict *, GdaGraph *)) dict_changed;
	class->graph_updated = (void (*) (GdaDict *, GdaGraph *)) dict_changed;
	class->changed = NULL;

	/* Properties */
	object_class->set_property = gda_dict_set_property;
	object_class->get_property = gda_dict_get_property;
	g_object_class_install_property (object_class, PROP_SERIAL_QUERY,
					 g_param_spec_uint ("query_serial", NULL, NULL, 
							    1, G_MAXUINT, 1, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_SERIAL_GRAPH,
					 g_param_spec_uint ("graph_serial", NULL, NULL, 
							    1, G_MAXUINT, 1, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_WITH_FUNCTIONS,
					 g_param_spec_boolean ("with_functions", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DSN,
					 g_param_spec_string ("dsn", _("DSN of the connection to use"), NULL, NULL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_USERNAME,
					 g_param_spec_string ("username", _("Username for the connection to use"), 
							      NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->dispose = gda_dict_dispose;
	object_class->finalize = gda_dict_finalize;
}

/* data is unused */
static void
dict_changed (GdaDict *dict, gpointer data)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'CHANGED' from %s()\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (dict), gda_dict_signals[CHANGED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CHANGED' from %s()\n", __FUNCTION__);
#endif	
}

static void
gda_dict_init (GdaDict * dict)
{
	dict->priv = g_new0 (GdaDictPrivate, 1);
	dict->priv->serial_query = 1;
	dict->priv->serial_graph = 1;
	dict->priv->assumed_queries = NULL;
	dict->priv->all_queries = NULL;
	dict->priv->assumed_graphs = NULL;
	dict->priv->all_graphs = NULL;
	dict->priv->database = NULL;
	dict->priv->cnc = NULL;
	dict->priv->xml_filename = NULL;
	dict->priv->dsn = NULL;
	dict->priv->user = NULL;
	dict->priv->object_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

/**
 * gda_dict_new
 *
 * Create a new #GdaDict object.
 *
 * Returns: the newly created object.
 */
GObject   *
gda_dict_new ()
{
	GObject   *obj;
	GdaDict *dict;

	obj = g_object_new (GDA_TYPE_DICT, NULL);
	dict = GDA_DICT (obj);

	/* Server and database objects creation */
	dict->priv->cnc = NULL;
	dict->priv->database = GDA_DICT_DATABASE (gda_dict_database_new (dict));

	return obj;
}


static void
gda_dict_dispose (GObject   * object)
{
	GdaDict *dict;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT (object));

	dict = GDA_DICT (object);
	if (dict->priv) {
		GSList *list;

		/* graphs */
		list = dict->priv->all_graphs;
		while (list) {
			g_object_weak_unref (G_OBJECT (list->data), (GWeakNotify) graph_weak_ref_notify, dict);
			
			list = g_slist_next (list);
		}

		if (dict->priv->all_graphs) {
			g_slist_free (dict->priv->all_graphs);
			dict->priv->all_graphs = NULL;
		}

		while (dict->priv->assumed_graphs)
			gda_object_destroy (GDA_OBJECT (dict->priv->assumed_graphs->data));

		/* queries */
		while (dict->priv->assumed_queries)
			gda_object_destroy (GDA_OBJECT (dict->priv->assumed_queries->data));
		
		list = dict->priv->all_queries;
		while (list) {
			g_object_weak_unref (G_OBJECT (list->data), (GWeakNotify) query_weak_ref_notify, dict);
			
			gda_object_destroy (GDA_OBJECT (list->data));
			list = g_slist_next (list);
		}

		if (dict->priv->all_queries) {
			g_slist_free (dict->priv->all_queries);
			dict->priv->all_queries = NULL;
		}

		/* database */
		if (dict->priv->database) {
			g_object_unref (G_OBJECT (dict->priv->database));
			dict->priv->database = NULL;
		}

		/* functions */
		while (dict->priv->functions)
			gda_object_destroy (GDA_OBJECT (dict->priv->functions->data));

		/* aggregates */
		while (dict->priv->aggregates)
			gda_object_destroy (GDA_OBJECT (dict->priv->aggregates->data));

		/* types */
		while (dict->priv->data_types)
			gda_object_destroy (GDA_OBJECT (dict->priv->data_types->data));

		if (dict->priv->custom_types) {
			g_slist_free (dict->priv->custom_types);
			dict->priv->custom_types = NULL;
		}
		
		/* connection */
		if (dict->priv->cnc) {
			g_signal_handlers_disconnect_by_func (dict->priv->cnc,
							      G_CALLBACK (dsn_changed_cb), dict);
			g_object_unref (G_OBJECT (dict->priv->cnc));
			dict->priv->cnc = NULL;
		}
		g_free (dict->priv->dsn);
		g_free (dict->priv->user);

	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_finalize (GObject   * object)
{
	GdaDict *dict;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT (object));

	dict = GDA_DICT (object);
	if (dict->priv) {
		if (dict->priv->xml_filename) {
			g_free (dict->priv->xml_filename);
			dict->priv->xml_filename = NULL;
		}

		g_hash_table_destroy (dict->priv->object_ids);

		g_free (dict->priv);
		dict->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void 
gda_dict_set_property (GObject *object,
		      guint param_id,
		      const GValue *value,
		      GParamSpec *pspec)
{
	GdaDict *gda_dict;

	gda_dict = GDA_DICT (object);
	if (gda_dict->priv) {
		switch (param_id) {
		case PROP_SERIAL_QUERY:
		case PROP_SERIAL_GRAPH:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		case PROP_WITH_FUNCTIONS:
			gda_dict->priv->with_functions = g_value_get_boolean (value);
			break;
		case PROP_DSN:
			g_free (gda_dict->priv->dsn);
			gda_dict->priv->dsn = NULL;
			if (g_value_get_string (value))
				gda_dict->priv->dsn = g_strdup (g_value_get_string (value));
			break;
		case PROP_USERNAME:
			g_free (gda_dict->priv->user);
			gda_dict->priv->user = NULL;
			if (g_value_get_string (value))
				gda_dict->priv->user = g_strdup (g_value_get_string (value));
			break;
		}
	}
}

static void
gda_dict_get_property (GObject *object,
		       guint param_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	GdaDict *gda_dict;
	gda_dict = GDA_DICT (object);
	
	if (gda_dict->priv) {
		switch (param_id) {
		case PROP_SERIAL_QUERY:
			g_value_set_uint (value, gda_dict->priv->serial_query++);
			break;
		case PROP_SERIAL_GRAPH:
			g_value_set_uint (value, gda_dict->priv->serial_graph++);
			break;
		case PROP_WITH_FUNCTIONS:
			g_value_set_boolean (value, gda_dict->priv->with_functions);
			break;
		case PROP_DSN:
			g_value_set_string (value, gda_dict->priv->dsn);
			break;
		case PROP_USERNAME:
			g_value_set_string (value, gda_dict->priv->user);
			break;
		}	
	}
}

/**
 * gda_dict_declare_object_string_id_change
 * @dict:
 * @obj:
 * @oldid:
 *
 * Internal function, not to be used directly.
 */
void
gda_dict_declare_object_string_id_change (GdaDict *dict, GdaObject *obj, const gchar *oldid)
{
	const gchar *strid;
	GdaObject *exobj;

	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (GDA_IS_OBJECT (obj));
	g_return_if_fail (gda_object_get_dict (obj) == dict);

	if (oldid) {
		exobj = g_hash_table_lookup (dict->priv->object_ids, oldid);
		if (!exobj) 
			g_warning ("Objects 'old ID not registered");
		if (exobj != obj)
			g_warning ("Objects 'old ID does not belong to object");
		else {
			g_hash_table_remove (dict->priv->object_ids, oldid);
			/* g_print ("- %s <-> %p\n", oldid, obj); */
		}
	}
	
	strid = gda_object_get_id (obj);
	if (!strid || !(*strid))
		/* stop here if no Id */
		return;

	exobj = g_hash_table_lookup (dict->priv->object_ids, strid);
	if (exobj) {
		if (exobj != obj) {
			g_warning ("Object ID collision");
			return;
		}
		g_hash_table_remove (dict->priv->object_ids, strid);
	}

	g_hash_table_insert (dict->priv->object_ids, g_strdup (strid), obj);
	/* g_print ("+ %s <-> %p\n", strid, obj); */

	/* special treatments */
	if (GDA_IS_GRAPH (obj)) {
		guint id;

		id = atoi (strid + 2);
		if (id >= dict->priv->serial_graph) 
			dict->priv->serial_graph = id + 1;
	}

	if (GDA_IS_QUERY (obj)) {
		/*g_print ("\t%u / %u\n", gda_query_object_get_int_id (GDA_QUERY_OBJECT (obj)),
		  dict->priv->serial_query);*/
		if (gda_query_object_get_int_id (GDA_QUERY_OBJECT (obj)) >= dict->priv->serial_query) {
			dict->priv->serial_query = gda_query_object_get_int_id (GDA_QUERY_OBJECT (obj)) + 1;
			/* g_print ("Dict %p's serial_query is now %u\n", dict, dict->priv->serial_query); */
		}
	}
}

/**
 * gda_dict_get_object_by_string_id
 * @dict: a #GdaDict object
 * @strid: a string
 *
 * Fetch a pointer to the #GdaObject which has the @strid string ID.
 * 
 * Returns: the corresponding #GdaObject, or %NULL if none found
 */
GdaObject *
gda_dict_get_object_by_string_id (GdaDict *dict, const gchar *strid)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (strid && *strid, NULL);

	return g_hash_table_lookup (dict->priv->object_ids, strid);
}


#ifdef XML_ID_TEST
static void xml_id_update_tree_recurs (xmlDocPtr doc, xmlNodePtr node, GHashTable *keys);

/*
 * Goes through all the @doc tree and replaces ID and IDREF with the new naming scheme.
 */
static void
xml_id_update_tree (xmlDocPtr doc)
{
	GHashTable *keys;
	xmlNodePtr node;

	g_assert (node);
	keys = g_hash_table_new_full (g_str_hash, g_str_equal, xmlFree, g_free);

	node = xmlDocGetRootElement (doc);
	xml_id_update_tree_recurs (doc, node, keys);
	g_hash_table_destroy (keys);
}

static void
xml_id_update_tree_recurs (xmlDocPtr doc, xmlNodePtr node, GHashTable *keys)
{
	static gchar* upd_nodes[] = {"GNOME_DB_DATATYPE",
				     "GNOME_DB_FUNCTION",
				     "GNOME_DB_AGGREGATE",
				     "GDA_DICT_TABLE",
				     "GDA_ENTITY_FIELD"};
	static gchar* upd_prefix[] = {"DT",
				      "PR",
				      "AG",
				      "TV",
				      "FI"};
	gint i;
	xmlAttrPtr attr;
	xmlNodePtr child;

	/* update this node'id if necessary */
	for (i = 0; i < 5; i++) {
		if (!strcmp (node->name, upd_nodes [i])) {
			gchar *oid, *id, *name;
			
			name = xmlGetProp (node, "name");
			oid = xmlGetProp (node, "id");
			g_assert (name && oid);

			switch (i) {
			default:
				id = utility_build_encoded_id (upd_prefix [i], name);
				break;
			case 4: {
				/* GDA_ENTITY_FIELD has a composed ID */
				gchar *tableid, *tmp;
				xmlNodePtr parent = node->parent;

				g_assert (parent);
				tableid = xmlGetProp (parent, "id");
				tmp = utility_build_encoded_id (upd_prefix [i], name);
				id = g_strconcat (tableid, ":", tmp, NULL);
				g_free (tmp);
				xmlFree (tableid);
				break;
			}
			case 1:
			case 2: 
				/* GNOME_DB_FUNCTION and GNOME_DB_AGGREGATE encode their DBMS id */
				id = utility_build_encoded_id (upd_prefix [i], oid+2);
				break;
			}

			xmlSetProp (node, "id", id);
			/*g_print ("UPDATE ID %s: \tname=%s, oldid=%s, id=%s\n", upd_nodes [i], name, oid, id);*/

			g_hash_table_insert (keys, oid, id); /* don't free oid and id as they will be when hash is destroyed */
			xmlFree (name);
			break;
		}
	}

	/* update any reference in this node */
	attr = node->properties;
	while (attr) {
		gchar *oid, *id;
		
		oid = xmlGetProp (node, attr->name);
		g_assert (oid);
		id = g_hash_table_lookup (keys, oid);
		if (id) {
			xmlSetProp (node, attr->name, id);
			/*g_print ("replace ref %s: \tattr=%s, oldref=%s, newref=%s\n", attr->name, node->name, oid, id);*/
		}
		xmlFree (oid);
		attr = attr->next;
	}

	/* update children */
	child = node->children;
	while (child) {
		xml_id_update_tree_recurs (doc, child, keys);
		child = child->next;
	}
}

#endif

static void xml_validity_error_func (void *ctx, const char *msg, ...);

/**
 * gda_dict_load_xml_file
 * @dict: a #GdaDict object
 * @xmlfile: the name of the file to which the XML will be written to
 * @error: location to store error, or %NULL
 *
 * Loads an XML file which respects the Libgda DTD, and creates all the necessary
 * objects that are defined within the XML file. During the creation of the other
 * objects, all the normal signals are emitted.
 *
 * If the GdaDict object already has some contents, then it is first of all
 * destroyed (to return its state as when it was first created).
 *
 * If an error occurs during loading then the GdaDict object is left as empty
 * as when it is first created.
 *
 * Returns: TRUE if loading was successfull and FALSE otherwise.
 */
gboolean
gda_dict_load_xml_file (GdaDict *dict, const gchar *xmlfile, GError **error)
{
	xmlDocPtr doc;
	xmlNodePtr node, subnode;
	gchar *str;
	xmlDtdPtr old_dtd = NULL;

	g_return_val_if_fail (dict && GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);
	g_return_val_if_fail (xmlfile && *xmlfile, FALSE);

	if (! g_file_test (xmlfile, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
			     GDA_DICT_ERROR,
			     GDA_DICT_LOAD_FILE_NOT_EXIST_ERROR,
			     "File '%s' does not exist", xmlfile);
		return FALSE;
	}

	doc = xmlParseFile (xmlfile);

	if (doc) {
		/* doc validation */
		xmlValidCtxtPtr validc;

		validc = g_new0 (xmlValidCtxt, 1);
		validc->userData = dict;
		validc->error = xml_validity_error_func;
		validc->warning = NULL; 
		xmlDoValidityCheckingDefaultValue = 1;

		/* replace the DTD with ours */
		old_dtd = doc->intSubset;
		doc->intSubset = gda_dict_dtd;

		if (! xmlValidateDocument (validc, doc)) {
			gchar *str;

			xmlFreeDoc (doc);
			g_free (validc);
			str = g_object_get_data (G_OBJECT (dict), "xmlerror");
			if (str) {
				g_set_error (error,
					     GDA_DICT_ERROR,
					     GDA_DICT_FILE_LOAD_ERROR,
					     "File '%s' does not conform to DTD:\n%s", xmlfile, str);
				g_free (str);
				g_object_set_data (G_OBJECT (dict), "xmlerror", NULL);
			}
			else 
				g_set_error (error,
					     GDA_DICT_ERROR,
					     GDA_DICT_FILE_LOAD_ERROR,
					     "File '%s' does not conform to DTD", xmlfile);

			return FALSE;
		}
		g_free (validc);
	}
	else {
		g_set_error (error,
			     GDA_DICT_ERROR,
			     GDA_DICT_FILE_LOAD_ERROR,
			     "Can't load file '%s'", xmlfile);
		return FALSE;
	}

	/* doc is now OK */
	node = xmlDocGetRootElement (doc);
	if (strcmp (node->name, "gda_dict")) {
		g_set_error (error,
			     GDA_DICT_ERROR,
			     GDA_DICT_FILE_LOAD_ERROR,
			     "XML file '%s' does not have any <gda_dict> node", xmlfile);
		return FALSE;
	}
	subnode = node->children;
	
	if (!subnode) {
		g_set_error (error,
			     GDA_DICT_ERROR,
			     GDA_DICT_FILE_LOAD_ERROR,
			     "XML file '%s': <gda_dict> does not have any children",
			     xmlfile);
		return FALSE;
	}

#ifdef XML_ID_TEST
	{
		/* test if we need to update the XML ID of the nodes in the tree */
		xmlXPathContextPtr xpathCtx; 

		xpathCtx = xmlXPathNewContext(doc);
		if (xpathCtx) {
			xmlXPathObjectPtr xpathObj; 

			xpathObj = xmlXPathEvalExpression("//gda_datatype", xpathCtx);
			if (xpathObj) {
				xmlNodeSetPtr nodes;
				xmlNodePtr node = NULL;

				nodes = xpathObj->nodesetval;
				if (nodes && (nodes->nodeNr > 0))
					node = nodes->nodeTab [0];
					
				if (node) {
					gchar *id, *name;

					id = xmlGetProp (node, "id");
					name = xmlGetProp (node, "name");
					
					if (id && name) {
						gchar *tmp;

						tmp = utility_build_encoded_id ("DT", name);
						if (strcmp (tmp, id)) {
							g_print ("Updating XML IDs and IDREFs of this dictionary...\n");
							xml_id_update_tree (doc);
						}
						g_free (tmp);
					}
					
					if (id)
						xmlFree (id);
					if (name)
						xmlFree (name);
				}

				xmlXPathFreeObject (xpathObj);
			}
			xmlXPathFreeContext(xpathCtx);
		}
	}
#endif

	/* Dictionary attributes */
	str = xmlGetProp (node, "with_functions");
        if (str) {
                dict->priv->with_functions = (*str && (*str == 't')) ? TRUE : FALSE;
                g_free (str);
        }

	while (subnode) {
		gboolean done = FALSE;
		/* while (xmlNodeIsText (subnode)) { */
/* 			subnode = subnode->next; */
/* 			continue; */
/* 		} */
		
		/* Datasource */
		if (!strcmp (subnode->name, "gda_dsn_info")) {
			g_free (dict->priv->dsn);			
			g_free (dict->priv->user);

			dict->priv->dsn = xmlGetProp (subnode, "dsn");
			dict->priv->user = xmlGetProp (subnode, "user");
			done = TRUE;
		}

		/* GdaDictType objects */
		if (!done && !strcmp (subnode->name, "gda_dict_types")) {
			if (!gda_dict_load_data_types (dict, subnode, error))
				return FALSE;
			done = TRUE;
		}

		/* GdaDictFunction objects */
		if (!done && !strcmp (subnode->name, "gda_dict_procedures")) {
			if (!gda_dict_load_procedures (dict, subnode, error))
				return FALSE;
			done = TRUE;
		}

		/* GdaDictAggregate objects */
		if (!done && !strcmp (subnode->name, "gda_dict_aggregates")) {
			if (!gda_dict_load_aggregates (dict, subnode, error))
				return FALSE;
			done = TRUE;
		}

		/* GdaDictDatabase object */
		if (!done && !strcmp (subnode->name, "gda_dict_database")) {
			if (!gda_xml_storage_load_from_xml (GDA_XML_STORAGE (dict->priv->database), subnode, error))
				return FALSE;
			done = TRUE;
		}

		/* GdaQuery objects */
		if (!done && !strcmp (subnode->name, "gda_queries")) {
			if (!gda_dict_load_queries (dict, subnode, error))
				return FALSE;
			done = TRUE;
		}

		/* GdaGraph objects */
		if (!done && !strcmp (subnode->name, "gda_graphs")) {
			if (!gda_dict_load_graphs (dict, subnode, error))
				return FALSE;
			done = TRUE;
		}

		subnode = subnode->next;
	}
	doc->intSubset = old_dtd;
	xmlFreeDoc (doc);

	return TRUE;
}

static gboolean
gda_dict_load_data_types (GdaDict *dict, xmlNodePtr types, GError **error)
{
	xmlNodePtr qnode = types->children;
	gboolean allok = TRUE;
	gchar *prop;

	while (qnode && allok) {
		if (!strcmp (qnode->name, "gda_dict_type")) {
			gboolean load = TRUE; /* don't load custom data types */
			prop = xmlGetProp (qnode, "custom");
			if (prop) {
				if (*prop == 't')
					load = FALSE;
				g_free (prop);
			}
			if (load) {
				GdaDictType *type;
			
				type = GDA_DICT_TYPE (gda_dict_type_new (dict));
				allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (type), qnode, error);
				if (allok) {
					g_object_ref (G_OBJECT (type));
					dict->priv->data_types = g_slist_append (dict->priv->data_types, type);
					
					gda_object_connect_destroy (type, G_CALLBACK (destroyed_data_type_cb), dict);
					g_signal_connect (G_OBJECT (type), "changed",
							  G_CALLBACK (updated_data_type_cb), dict);
#ifdef GDA_DEBUG_signal
					g_print (">> 'DATA_TYPE_ADDED' from %s\n", __FUNCTION__);
#endif
					g_signal_emit (G_OBJECT (dict), gda_dict_signals[DATA_TYPE_ADDED],
						       0, type);
#ifdef GDA_DEBUG_signal
					g_print ("<< 'DATA_TYPE_ADDED' from %s\n", __FUNCTION__);
#endif
				}
				
				g_object_unref (G_OBJECT (type));
			}
		}
		qnode = qnode->next;
	}

	return allok;
}

static void
destroyed_data_type_cb (GdaDictType *dt, GdaDict *dict)
{
        g_return_if_fail (g_slist_find (dict->priv->data_types, dt));

        dict->priv->data_types = g_slist_remove (dict->priv->data_types, dt);

        g_signal_handlers_disconnect_by_func (G_OBJECT (dt),
                                              G_CALLBACK (destroyed_data_type_cb), dict);
        g_signal_handlers_disconnect_by_func (G_OBJECT (dt),
                                              G_CALLBACK (updated_data_type_cb), dict);

#ifdef GDA_DEBUG_signal
        g_print (">> 'DATA_TYPE_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (dict), "data_type_removed", dt);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'DATA_TYPE_REMOVED' from %s\n", __FUNCTION__);
#endif

        if (! g_slist_find (dict->priv->custom_types, dt))
                g_object_unref (G_OBJECT (dt));
        else
                dict->priv->custom_types = g_slist_remove (dict->priv->custom_types, dt);
}

static void
updated_data_type_cb (GdaDictType *dt, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'DATA_TYPE_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (dict), "data_type_updated", dt);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'DATA_TYPE_UPDATED' from %s\n", __FUNCTION__);
#endif
}

static gboolean
gda_dict_load_procedures (GdaDict *dict, xmlNodePtr procs, GError **error)
{
	xmlNodePtr qnode = procs->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp (qnode->name, "gda_dict_function")) {
			GdaDictFunction *func;

			func = GDA_DICT_FUNCTION (gda_dict_function_new (dict));
			allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (func), qnode, error);
			if (allok) {
				g_object_ref (G_OBJECT (func));
				dict->priv->functions = g_slist_append (dict->priv->functions, func);
				
				gda_object_connect_destroy (func, G_CALLBACK (destroyed_function_cb), dict);
				g_signal_connect (G_OBJECT (func), "changed",
						  G_CALLBACK (updated_function_cb), dict);
#ifdef GDA_DEBUG_signal
				g_print (">> 'FUNCTION_ADDED' from %s\n", __FUNCTION__);
#endif
				g_signal_emit_by_name (G_OBJECT (dict), "function_added", func);
#ifdef GDA_DEBUG_signal
				g_print ("<< 'FUNCTION_ADDED' from %s\n", __FUNCTION__);
#endif
			}

			g_object_unref (G_OBJECT (func));
		}
		qnode = qnode->next;
	}

	return allok;
}

static void
destroyed_function_cb (GdaDictFunction *func, GdaDict *dict)
{
        g_return_if_fail (g_slist_find (dict->priv->functions, func));
        dict->priv->functions = g_slist_remove (dict->priv->functions, func);

        g_signal_handlers_disconnect_by_func (G_OBJECT (func),
                                              G_CALLBACK (destroyed_function_cb), dict);
        g_signal_handlers_disconnect_by_func (G_OBJECT (func),
                                              G_CALLBACK (updated_function_cb), dict);

#ifdef GDA_DEBUG_signal
        g_print (">> 'FUNCTION_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (dict), "function_removed", func);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'FUNCTION_REMOVED' from %s\n", __FUNCTION__);
#endif

        g_object_unref (G_OBJECT (func));
}

static void
updated_function_cb (GdaDictFunction *func, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'FUNCTION_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (dict), "function_updated", func);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'FUNCTION_UPDATED' from %s\n", __FUNCTION__);
#endif
}

static gboolean
gda_dict_load_aggregates (GdaDict *dict, xmlNodePtr aggs, GError **error)
{
	xmlNodePtr qnode = aggs->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp (qnode->name, "gda_dict_aggregate")) {
			GdaDictAggregate *agg;

			agg = GDA_DICT_AGGREGATE (gda_dict_aggregate_new (dict));
			allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (agg), qnode, error);
			if (allok) {
				g_object_ref (agg);
				dict->priv->aggregates = g_slist_append (dict->priv->aggregates, agg);
				gda_object_connect_destroy (agg, G_CALLBACK (destroyed_aggregate_cb), dict);
				g_signal_connect (G_OBJECT (agg), "changed",
						  G_CALLBACK (updated_aggregate_cb), dict);
#ifdef GDA_DEBUG_signal
				g_print (">> 'AGGREGATE_ADDED' from %s\n", __FUNCTION__);
#endif
				g_signal_emit_by_name (G_OBJECT (dict), "aggregate_added", agg);
#ifdef GDA_DEBUG_signal
				g_print ("<< 'AGGREGATE_ADDED' from %s\n", __FUNCTION__);
#endif

			}

			g_object_unref (G_OBJECT (agg));
		}
		qnode = qnode->next;
	}

	return allok;
}

static void
destroyed_aggregate_cb (GdaDictAggregate *agg, GdaDict *dict)
{
        g_return_if_fail (g_slist_find (dict->priv->aggregates, agg));
        dict->priv->aggregates = g_slist_remove (dict->priv->aggregates, agg);

        g_signal_handlers_disconnect_by_func (G_OBJECT (agg),
                                              G_CALLBACK (destroyed_aggregate_cb), dict);
        g_signal_handlers_disconnect_by_func (G_OBJECT (agg),
                                              G_CALLBACK (updated_aggregate_cb), dict);

#ifdef GDA_DEBUG_signal
        g_print (">> 'AGGREGATE_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (dict), "aggregate_removed", agg);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'AGGREGATE_REMOVED' from %s\n", __FUNCTION__);
#endif
        g_object_unref (G_OBJECT (agg));
}

static void
updated_aggregate_cb (GdaDictAggregate *agg, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
        g_print (">> 'AGGREGATE_UPDATED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit_by_name (G_OBJECT (dict), "aggregate_updated", agg);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'AGGREGATE_UPDATED' from %s\n", __FUNCTION__);
#endif
}

static gboolean
gda_dict_load_queries (GdaDict *dict, xmlNodePtr queries, GError **error)
{
	xmlNodePtr qnode = queries->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp (qnode->name, "gda_query")) {
			GdaQuery *query;

			query = GDA_QUERY (gda_query_new (dict));
			allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (query), qnode, error);
			gda_dict_assume_query (dict, query);
			g_object_unref (G_OBJECT (query));
		}
		qnode = qnode->next;
	}

	if (allok) {
		GSList *list = dict->priv->assumed_queries;
		while (list) {
			gda_referer_activate (GDA_REFERER (list->data));
			list= g_slist_next (list);
		}
	}

	return allok;
}

static gboolean
gda_dict_load_graphs (GdaDict *dict, xmlNodePtr graphs, GError **error)
{
	xmlNodePtr qnode = graphs->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp (qnode->name, "gda_graph")) {
			GdaGraph *graph = NULL;
			gchar *prop;

			prop = xmlGetProp (qnode, "type");
			if (prop) {
				gchar *oprop = xmlGetProp (qnode, "object");
				if (!oprop && (*prop == 'Q')) {
					allok = FALSE;
					g_set_error (error,
						     GDA_DICT_ERROR,
						     GDA_DICT_FILE_LOAD_ERROR,
						     _("gda_graph of type 'Q' must have an 'object' attribute"));
				}
				
				if (allok && (*prop == 'Q')) {
					GdaQuery *query = gda_dict_get_query_by_xml_id (dict, oprop);
					if (!query) {
						allok = FALSE;
						g_set_error (error,
							     GDA_DICT_ERROR,
							     GDA_DICT_FILE_LOAD_ERROR,
							     _("gda_graph of type 'Q' must have valid 'object' attribute"));
					}
					else 
						graph = GDA_GRAPH (gda_graph_query_new (query));
				}
				g_free (oprop);
			}
			g_free (prop);

			if (allok) {
				if (!graph)
					graph = GDA_GRAPH (gda_graph_new (dict, GDA_GRAPH_DB_RELATIONS)); /* type is checked next line */
				allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (graph), qnode, error);
				gda_dict_assume_graph (dict, graph);
				g_object_unref (G_OBJECT (graph));
			}
		}
		qnode = qnode->next;
	}

	if (0 && allok) { /* the GdaGraph does not implement the GdaReferer interface for now */
		GSList *list = dict->priv->assumed_graphs;
		while (list) {
			gda_referer_activate (GDA_REFERER (list->data));
			list= g_slist_next (list);
		}
	}

	return allok;
}

/*
 * function called when an error occurred during the document validation
 */
static void
xml_validity_error_func (void *ctx, const char *msg, ...)
{
        xmlValidCtxtPtr pctxt;
        va_list args;
        gchar *str, *str2, *newerr;
	GdaDict *dict;

	pctxt = (xmlValidCtxtPtr) ctx;
	/* FIXME: it looks like libxml does set ctx to userData... */
	/*dict = GDA_DICT (pctxt->userData);*/
	dict = GDA_DICT (pctxt);
	str2 = g_object_get_data (G_OBJECT (dict), "xmlerror");

        va_start (args, msg);
        str = g_strdup_vprintf (msg, args);
        va_end (args);
	
	if (str2) {
		newerr = g_strdup_printf ("%s\n%s", str2, str);
		g_free (str2);
	}
	else
		newerr = g_strdup (str);
        g_free (str);
	g_object_set_data (G_OBJECT (dict), "xmlerror", newerr);
}


/**
 * gda_dict_save_xml_file
 * @dict: a #GdaDict object
 * @xmlfile: the name of the file to which the XML will be written to
 * @error: location to store error, or %NULL
 *
 * Saves the contents of a GdaDict object to a file which is given as argument.
 *
 * Returns: TRUE if saving was successfull and FALSE otherwise.
 */
gboolean
gda_dict_save_xml_file (GdaDict *dict, const gchar *xmlfile, GError **error)
{
	gboolean retval = TRUE;
	xmlDocPtr doc;
#define LIBGDA_DICT_DTD_FILE DTDINSTALLDIR"/libgda-dict.dtd"

	g_return_val_if_fail (dict && GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);
		
	doc = xmlNewDoc ("1.0");
	if (doc) {
		xmlNodePtr topnode, node;

		/* DTD insertion */
                xmlCreateIntSubset(doc, "gda_dict", NULL, "libgda-dict.dtd"/*LIBGDA_DICT_DTD_FILE*/);

		/* Top node */
		topnode = xmlNewDocNode (doc, NULL, "gda_dict", NULL);
		xmlDocSetRootElement (doc, topnode);

		/* Dictionary attributes */
		xmlSetProp (topnode, "with_functions", dict->priv->with_functions ? "t" : "f");

		/* DSN information */
		if (dict->priv->dsn) {
			node = xmlNewChild (topnode, NULL, "gda_dsn_info", NULL);
			xmlSetProp (node, "dsn", dict->priv->dsn);
			xmlSetProp (node, "user", dict->priv->user ? dict->priv->user : "");
		}

		/* GdaDictType objects */
		if (retval) {
			GSList *list;
			node = xmlNewChild (topnode, NULL, "gda_dict_types", NULL);
			list = dict->priv->data_types;
			while (list) {
				xmlNodePtr qnode;
				
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
				if (qnode) {
					xmlAddChild (node, qnode);
					if (g_slist_find (dict->priv->custom_types, list->data))
						xmlSetProp (qnode, "custom", "t");
				}
				else 
					/* error handling */
					retval = FALSE;

				list = g_slist_next(list);
			}
		}

		/* GdaDictFunction objects */
		if (retval) {
			GSList *list;
			node = xmlNewChild (topnode, NULL, "gda_dict_procedures", NULL);
			list = dict->priv->functions;
			while (list) {
				xmlNodePtr qnode;
				
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
				if (qnode)
					xmlAddChild (node, qnode);
				else 
					/* error handling */
					retval = FALSE;

				list = g_slist_next(list);
			}
		}

		/* GdaDictAggregate objects */
		if (retval) {
			GSList *list;
			node = xmlNewChild (topnode, NULL, "gda_dict_aggregates", NULL);
			list = dict->priv->aggregates;
			while (list) {
				xmlNodePtr qnode;
				
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
				if (qnode)
					xmlAddChild (node, qnode);
				else 
					/* error handling */
					retval = FALSE;

				list = g_slist_next(list);
			}
		}
		
		/* GdaDictDatabase */
		if (retval) {
			node = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (dict->priv->database), error);
			if (node)
				xmlAddChild (topnode, node);
			else 
				/* error handling */
				retval = FALSE;
		}
		
		/* GdaQuery objects */
		if (retval) {
			GSList *list;
			node = xmlNewChild (topnode, NULL, "gda_queries", NULL);
			list = dict->priv->assumed_queries;
			while (list) {
				if (!gda_query_get_parent_query (GDA_QUERY (list->data))) {
					/* We only add nodes for queries which do not have any parent query */
					xmlNodePtr qnode;
					
					qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
					if (qnode)
						xmlAddChild (node, qnode);
					else 
						/* error handling */
						retval = FALSE;
				}
				list = g_slist_next(list);
			}
		}

		/* GdaGraph objects */
		if (retval) {
			GSList *list;
			node = xmlNewChild (topnode, NULL, "gda_graphs", NULL);
			list = dict->priv->assumed_graphs;
			while (list) {
				xmlNodePtr qnode;
				
				qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
				if (qnode)
					xmlAddChild (node, qnode);
				else 
					/* error handling */
					retval = FALSE;

				list = g_slist_next(list);
			}
		}

		/* actual writing to file */
		if (retval) {
			gint i;
			i = xmlSaveFormatFile (xmlfile, doc, TRUE);
			if (i == -1) {
				/* error handling */
				g_set_error (error, GDA_DICT_ERROR, GDA_DICT_FILE_SAVE_ERROR,
					     _("Error writing XML file %s"), xmlfile);
				retval = FALSE;
			}
		}

		/* getting rid of the doc */
		xmlFreeDoc (doc);
	}
	else {
		/* error handling */
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_FILE_SAVE_ERROR,
			     _("Can't allocate memory for XML structure."));
		retval = FALSE;
	}

	return retval;
}


/**
 * gda_dict_declare_query
 * @dict: a #GdaDict object
 * @query: a #GdaQuery object
 *
 * Declares the existence of a new query to @dict. All the #GdaQuery objects MUST
 * be declared to the corresponding #GdaDict object for the library to work correctly.
 * Once @query has been declared, @dict does not hold any reference to @query. If @dict
 * must hold such a reference, then use gda_dict_assume_query().
 *
 * This functions is called automatically from each gda_query_new* function, and it should not be necessary
 * to call it except for classes extending the #GdaQuery class.
 */
void
gda_dict_declare_query (GdaDict *dict, GdaQuery *query)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (query && GDA_IS_QUERY (query));
	
	/* we don't take any reference on query */
	if (g_slist_find (dict->priv->all_queries, query))
		return;	
	else {
		dict->priv->all_queries = g_slist_append (dict->priv->all_queries, query);
		g_object_weak_ref (G_OBJECT (query), (GWeakNotify) query_weak_ref_notify, dict);
	}
}

static void
query_weak_ref_notify (GdaDict *dict, GdaQuery *query)
{
	dict->priv->all_queries = g_slist_remove (dict->priv->all_queries, query);
}

static void
updated_query_cb (GdaQuery *query, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'QUERY_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (dict), "query_updated", query);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'QUERY_UPDATED' from %s\n", __FUNCTION__);
#endif	
}

/**
 * gda_dict_assume_query
 * @dict: a #GdaDict object
 * @query: a #GdaQuery object
 *
 * Force @dict to manage @query: it will get a reference to it.
 */
void
gda_dict_assume_query (GdaDict *dict, GdaQuery *query)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (query && GDA_IS_QUERY (query));
	
	if (g_slist_find (dict->priv->assumed_queries, query)) {
		g_warning ("GdaQuery %p already assumed!", query);
		return;
	}

	gda_dict_declare_query (dict, query);

	dict->priv->assumed_queries = g_slist_append (dict->priv->assumed_queries, query);
	g_object_ref (G_OBJECT (query));
	gda_object_connect_destroy (query,
				 G_CALLBACK (query_destroyed_cb), dict);
	g_signal_connect (G_OBJECT (query), "changed",
			  G_CALLBACK (updated_query_cb), dict);

#ifdef GDA_DEBUG_signal
	g_print (">> 'QUERY_ADDED' from gda_dict_assume_query\n");
#endif
	g_signal_emit (G_OBJECT (dict), gda_dict_signals[QUERY_ADDED], 0, query);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'QUERY_ADDED' from gda_dict_assume_query\n");
#endif
}


/* called when a GdaQuery is "destroyed" */
static void 
query_destroyed_cb (GdaQuery *query, GdaDict *dict)
{
	gda_dict_unassume_query (dict, query);
}


/**
 * gda_dict_unassume_query
 * @dict: a #GdaDict object
 * @query: a #GdaQuery object
 *
 * Forces @dict to lose a reference it has on @query
 */
void
gda_dict_unassume_query (GdaDict *dict, GdaQuery *query)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);

	if (g_slist_find (dict->priv->assumed_queries, query)) {
		dict->priv->assumed_queries = g_slist_remove (dict->priv->assumed_queries, query);
		g_signal_handlers_disconnect_by_func (G_OBJECT (query),
						      G_CALLBACK (query_destroyed_cb), dict);
		g_signal_handlers_disconnect_by_func (G_OBJECT (query),
						      G_CALLBACK (updated_query_cb), dict);
#ifdef GDA_DEBUG_signal
		g_print (">> 'QUERY_REMOVED' from gda_dict_unassume_query\n");
#endif
		g_signal_emit (G_OBJECT (dict), gda_dict_signals[QUERY_REMOVED], 0, query);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'QUERY_REMOVED' from gda_dict_unassume_query\n");
#endif
		g_object_unref (G_OBJECT (query));
	}
}


/**
 * gda_dict_get_queries
 * @dict: a #GdaDict object
 *
 * Get a list of all the non interdependant queries managed by @dict
 * (only queries with no parent query are listed)
 *
 * Returns: a new list of #GdaQuery objects
 */
GSList *
gda_dict_get_queries (GdaDict *dict)
{
	GSList *list, *retval = NULL;

	g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	list = dict->priv->assumed_queries;
	while (list) {
		if (! gda_query_get_parent_query (GDA_QUERY (list->data)))
			retval = g_slist_append (retval, list->data);
		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_dict_get_query_by_xml_id
 * @dict: a #GdaDict object
 * @xml_id: the XML Id of the query being searched
 *
 * Find a #GdaQuery object from its XML Id
 *
 * Returns: the #GdaQuery object, or NULL if not found
 */
GdaQuery *
gda_dict_get_query_by_xml_id (GdaDict *dict, const gchar *xml_id)
{
	GdaQuery *query = NULL;

	GSList *list;
        gchar *str;

        g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
        g_return_val_if_fail (dict->priv, NULL);

        list = dict->priv->all_queries;
        while (list && !query) {
                str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
                if (!strcmp (str, xml_id))
                        query = GDA_QUERY (list->data);
                g_free (str);
                list = g_slist_next (list);
        }

        return query;
}

/**
 * gda_dict_declare_graph
 * @dict: a #GdaDict object
 * @graph: a #GdaGraph object
 *
 * Declares the existence of a new graph to @dict. All the #GdaGraph objects MUST
 * be declared to the corresponding #GdaDict object for the library to work correctly.
 * Once @graph has been declared, @dict does not hold any reference to @graph. If @dict
 * must hold such a reference, then use gda_dict_assume_graph().
 *
 * This functions is called automatically from each gda_graph_new* function, and it should not be necessary
 * to call it except for classes extending the #GdaGraph class.
 */
void
gda_dict_declare_graph (GdaDict *dict, GdaGraph *graph)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (graph && GDA_IS_GRAPH (graph));
	
	/* we don't take any reference on graph */
	if (g_slist_find (dict->priv->all_graphs, graph))
		return;	
	else {
		dict->priv->all_graphs = g_slist_append (dict->priv->all_graphs, graph);
		g_object_weak_ref (G_OBJECT (graph), (GWeakNotify) graph_weak_ref_notify, dict);
	}
}

static void
graph_weak_ref_notify (GdaDict *dict, GdaGraph *graph)
{
	dict->priv->all_graphs = g_slist_remove (dict->priv->all_graphs, graph);
}

static void
updated_graph_cb (GdaGraph *graph, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'GRAPH_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (dict), "graph_updated", graph);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'GRAPH_UPDATED' from %s\n", __FUNCTION__);
#endif
}



/**
 * gda_dict_assume_graph
 * @dict: a #GdaDict object
 * @graph: a #GdaGraph object
 *
 * Force @dict to manage @graph: it will get a reference to it.
 */
void
gda_dict_assume_graph (GdaDict *dict, GdaGraph *graph)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (graph && GDA_IS_GRAPH (graph));
	
	if (g_slist_find (dict->priv->assumed_graphs, graph)) {
		g_warning ("GdaGraph %p already assumed!", graph);
		return;
	}

	gda_dict_declare_graph (dict, graph);

	dict->priv->assumed_graphs = g_slist_append (dict->priv->assumed_graphs, graph);
	g_object_ref (G_OBJECT (graph));
	gda_object_connect_destroy (graph, 
				 G_CALLBACK (graph_destroyed_cb), dict);
	g_signal_connect (G_OBJECT (graph), "changed",
			  G_CALLBACK (updated_graph_cb), dict);

#ifdef GDA_DEBUG_signal
	g_print (">> 'GRAPH_ADDED' from gda_dict_assume_graph\n");
#endif
	g_signal_emit (G_OBJECT (dict), gda_dict_signals[GRAPH_ADDED], 0, graph);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'GRAPH_ADDED' from gda_dict_assume_graph\n");
#endif
}


/* called when a GdaGraph is "destroyed" */
static void 
graph_destroyed_cb (GdaGraph *graph, GdaDict *dict)
{
	gda_dict_unassume_graph (dict, graph);
}


/**
 * gda_dict_unassume_graph
 * @dict: a #GdaDict object
 * @graph: a #GdaGraph object
 *
 * Forces @dict to lose a reference it has on @graph
 */
void
gda_dict_unassume_graph (GdaDict *dict, GdaGraph *graph)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);

	if (g_slist_find (dict->priv->assumed_graphs, graph)) {
		dict->priv->assumed_graphs = g_slist_remove (dict->priv->assumed_graphs, graph);
		g_signal_handlers_disconnect_by_func (G_OBJECT (graph),
						      G_CALLBACK (graph_destroyed_cb), dict);
		g_signal_handlers_disconnect_by_func (G_OBJECT (graph),
						      G_CALLBACK (updated_graph_cb), dict);
#ifdef GDA_DEBUG_signal
		g_print (">> 'GRAPH_REMOVED' from gda_dict_unassume_graph\n");
#endif
		g_signal_emit (G_OBJECT (dict), gda_dict_signals[GRAPH_REMOVED], 0, graph);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'GRAPH_REMOVED' from gda_dict_unassume_graph\n");
#endif
		g_object_unref (G_OBJECT (graph));
	}
}


/**
 * gda_dict_get_graphs
 * @dict: a #GdaDict object
 * @type_of_graphs: the requested type of graphs
 *
 * Get a list of the graphs managed by @dict, which are of the
 * requested type.
 *
 * Returns: a new list of #GdaGraph objects
 */
GSList *
gda_dict_get_graphs (GdaDict *dict, GdaGraphType type_of_graphs)
{
	g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	if (dict->priv->assumed_graphs) {
		GSList *list, *retval = NULL;

		list = dict->priv->assumed_graphs;
		while (list) {
			if (gda_graph_get_graph_type (GDA_GRAPH (list->data)) == type_of_graphs)
				retval = g_slist_prepend (retval, list->data);
			list = g_slist_next (list);
		}
		return g_slist_reverse (retval);
	}
	else
		return NULL;
}

/**
 * gda_dict_get_graph_by_xml_id
 * @dict: a #GdaDict object
 * @xml_id: the XML Id of the graph being searched
 *
 * Find a #GdaGraph object from its XML Id
 *
 * Returns: the #GdaGraph object, or NULL if not found
 */
GdaGraph *
gda_dict_get_graph_by_xml_id (GdaDict *dict, const gchar *xml_id)
{
	GdaGraph *graph = NULL;

	GSList *list;
        gchar *str;

        g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
        g_return_val_if_fail (dict->priv, NULL);

        list = dict->priv->all_graphs;
        while (list && !graph) {
                str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
                if (!strcmp (str, xml_id))
                        graph = GDA_GRAPH (list->data);
                g_free (str);
                list = g_slist_next (list);
        }

        return graph;
}

/**
 * gda_dict_get_graph_for_object
 * @dict: a #GdaDict object
 * @obj: a #Gobject object
 *
 * Find a #GdaGraph object guiven the object it is related to.
 *
 * Returns: the #GdaGraph object, or NULL if not found
 */
GdaGraph *
gda_dict_get_graph_for_object (GdaDict *dict, GObject *obj)
{
	GdaGraph *graph = NULL;

	GSList *list;
	GObject *ref_obj;

        g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
        g_return_val_if_fail (dict->priv, NULL);

        list = dict->priv->all_graphs;
        while (list && !graph) {
		g_object_get (G_OBJECT (list->data), "ref_object", &ref_obj, NULL);
		if (ref_obj == obj)
                        graph = GDA_GRAPH (list->data);
                list = g_slist_next (list);
        }

        return graph;
}


/**
 * gda_dict_get_connection
 * @dict: a #GdaDict object
 *
 * Fetch a pointer to the GdaConnection used by the GdaDict object.
 *
 * Returns: a pointer to the GdaConnection, if one has been assigned to @dict
 */
GdaConnection *
gda_dict_get_connection (GdaDict *dict)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	return dict->priv->cnc;
}

static void
dsn_changed_cb (GdaConnection *cnc, GdaDict *dict)
{
	const gchar *cstr;

	g_assert (cnc == dict->priv->cnc);

	g_free (dict->priv->dsn);
	dict->priv->dsn = g_strdup ((gchar *) gda_connection_get_dsn (cnc));

        cstr = gda_dict_get_xml_filename (dict);
        if (!cstr) {
                gchar *str;

                str = gda_dict_compute_xml_filename (dict, dict->priv->dsn, NULL, NULL);
                if (str) {
                        gda_dict_set_xml_filename (dict, str);
                        g_free (str);
                }
        }
}

/**
 * gda_dict_set_connection
 * @dict: a #GdaDict object
 * @cnc: a #GdaConnection object
 * 
 * Sets the associated connection to @dict. This connection is then used when the dictionary
 * synchronises itself.
 */
void
gda_dict_set_connection (GdaDict *dict, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	if (cnc)
		g_return_if_fail (GDA_IS_CONNECTION (cnc));

	if (dict->priv->cnc) {
		g_object_unref (G_OBJECT (dict->priv->cnc));
		g_signal_handlers_disconnect_by_func (dict->priv->cnc,
						      G_CALLBACK (dsn_changed_cb), dict);
		dict->priv->cnc = NULL;
	}
	if (cnc) {
		g_object_ref (cnc);
		dict->priv->cnc = cnc;

		g_free (dict->priv->user);
		dict->priv->user = g_strdup ((gchar *) gda_connection_get_username (dict->priv->cnc));

		g_signal_connect (G_OBJECT (dict->priv->cnc), "dsn_changed",
				  G_CALLBACK (dsn_changed_cb), dict);
		dsn_changed_cb (cnc, dict);
	}
}


/**
 * gda_dict_get_database
 * @dict: a #GdaDict object
 *
 * Fetch a pointer to the GdaDictDatabase used by the GdaDict object.
 *
 * Returns: a pointer to the GdaDictDatabase
 */
GdaDictDatabase *
gda_dict_get_database (GdaDict *dict)
{
	g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	return dict->priv->database;
}

#ifdef GDA_DEBUG
/**
 * gda_dict_dump
 * @dict: a #GdaDict object
 *
 * Dumps the whole dictionary managed by the GdaDict object
 */
void
gda_dict_dump (GdaDict *dict)
{
	GSList *list;

	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);

	g_print ("\n----------------- DUMPING START -----------------\n");
	g_print (D_COL_H1 "GdaDict %p\n" D_COL_NOR, dict);
	if (dict->priv->cnc) 
		g_print ("Connection: %p\n", dict->priv->cnc);

	if (dict->priv->database)
		gda_object_dump (GDA_OBJECT (dict->priv->database), 0);

	list = dict->priv->assumed_queries;
	if (list)
		g_print ("Queries:\n");
	else
		g_print ("No Query defined\n");
	while (list) {
		if (!gda_query_get_parent_query (GDA_QUERY (list->data)))
			gda_object_dump (GDA_OBJECT (list->data), 0);
		list = g_slist_next (list);
	}

	list = dict->priv->assumed_graphs;
	if (list)
		g_print ("Graphs:\n");
	else
		g_print ("No Graph defined\n");
	while (list) {
		gda_object_dump (GDA_OBJECT (list->data), 0);
		list = g_slist_next (list);
	}

	g_print ("----------------- DUMPING END -----------------\n\n");
}
#endif

static gboolean dict_data_type_update_list (GdaDict *dict, GError **error);
static gboolean dict_functions_update_list (GdaDict *dict, GError **error);
static gboolean dict_aggregates_update_list (GdaDict *dict, GError **error);

/**
 * gda_dict_update_dbms_data
 * @dict: a #GdaDict object
 * @error: location to store error, or %NULL
 *
 * Updates the list of data types, functions, tables, etc from the database,
 * which means that the @dict object uses an opened connection to the DBMS.
 * Use gda_dict_set_connection() to set a #GdaConnection to @dict.
 *
 * Returns: TRUE if no error
 */
gboolean
gda_dict_update_dbms_data (GdaDict *dict, GError **error)
{
	gboolean retval = TRUE;

	g_return_val_if_fail (GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);
	
	if (!dict->priv->cnc) {
		g_set_error (error, GDA_DICT_ERROR,
			     GDA_DICT_META_DATA_UPDATE_ERROR,
			     _("No connection associated to the dictionary"));
		return FALSE;
	}
	
	if (!gda_connection_is_opened (dict->priv->cnc)) {
		g_set_error (error, GDA_DICT_ERROR,
			     GDA_DICT_META_DATA_UPDATE_ERROR,
			     _("Connection is closed"));
		return FALSE;
	}

	if (dict->priv->update_in_progress) {
                g_set_error (error, GDA_DICT_ERROR, 
			     GDA_DICT_META_DATA_UPDATE_ERROR,
                             _("Update already started!"));
                return FALSE;
        }

	/* start the update */
	dict->priv->update_in_progress = TRUE;
        dict->priv->stop_update = FALSE;

#ifdef GDA_DEBUG_signal
        g_print (">> 'DATA_UPDATE_STARTED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (dict), gda_dict_signals[DATA_UPDATE_STARTED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'DATA_UPDATE_STARTED' from %s\n", __FUNCTION__);
#endif
	
	/* data types, functions and aggregates */
	retval = dict_data_type_update_list (dict, error);
        if (retval && dict->priv->with_functions && !dict->priv->stop_update)
                retval = dict_functions_update_list (dict, error);
        if (retval && dict->priv->with_functions && !dict->priv->stop_update)
                retval = dict_aggregates_update_list (dict, error);
	
	/* tables, fields, etc */
	retval = gda_dict_database_update_dbms_data (dict->priv->database, error);

#ifdef GDA_DEBUG_signal
        g_print (">> 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (dict), gda_dict_signals[DATA_UPDATE_FINISHED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif

        dict->priv->update_in_progress = FALSE;
        if (dict->priv->stop_update) {
                g_set_error (error, GDA_DICT_ERROR, 
			     GDA_DICT_META_DATA_UPDATE_USER_STOPPED,
                             _("Update stopped!"));
                return FALSE;
        }

	return retval;
}

static gboolean
dict_data_type_update_list (GdaDict *dict, GError **error)
{
	GSList *dtl = dict->priv->data_types;
	GdaDictType *dt;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *updated_dt = NULL;
	gboolean has_synonyms;

	/* no check here as the checks are supposed to have taken place once before calling this function */

	/* here we get the complete list of types, and for each type, update or
	   create the entry in the list if not yet there. */
	rs = gda_connection_get_schema (GDA_CONNECTION (dict->priv->cnc),
					GDA_CONNECTION_SCHEMA_TYPES, NULL, NULL);

	if (!rs) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_DATATYPE_ERROR,
			     _("Can't get list of data types"));
		return FALSE;
	}


	if (!utility_check_data_model (rs, 4, 
				       GDA_VALUE_TYPE_STRING, 
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_TYPE)) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_DATATYPE_ERROR,
			     _("Schema for list of data types is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}
	has_synonyms = utility_check_data_model (rs, 5, 
						 GDA_VALUE_TYPE_STRING, 
						 GDA_VALUE_TYPE_STRING,
						 GDA_VALUE_TYPE_STRING,
						 GDA_VALUE_TYPE_TYPE,
						 GDA_VALUE_TYPE_STRING);

	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !dict->priv->stop_update) {
		const GdaValue *value;
		gboolean newdt = FALSE;

		value = gda_data_model_get_value_at (rs, 0, now);
		str = gda_value_stringify ((GdaValue *) value);
		dt = gda_dict_get_data_type_by_name (dict, str);
		if (!dt) {
			gint i = 0;
			GSList *list;
			gboolean found = FALSE;

			/* type name */
			dt = GDA_DICT_TYPE (gda_dict_type_new (dict));
			gda_dict_type_set_sqlname (dt, str);
			newdt = TRUE;
			
			/* finding the right position for the data type */
			list = dict->priv->data_types;
			while (list && !found) {
				if (strcmp (str, 
					    gda_dict_type_get_sqlname (GDA_DICT_TYPE (list->data))) < 0)
					found = TRUE;
				else
					i++;
				list = g_slist_next (list);
			}

			dict->priv->data_types = g_slist_insert (dict->priv->data_types, dt, i);
			/* REM: we don't connect the "destroyed" and "changed" signals from the new GdaDictType right
			   now because otherwise the GdaConnection object will forward the "changed" signal 
			   (into "data_type_updated") before we have had the chance to emit the "data_type_added" signal */
		}
		g_free (str);
		
		updated_dt = g_slist_append (updated_dt, dt);

		/* FIXME: number of params */
		/*dt->numparams = 0;*/

		/* description */
		value = gda_data_model_get_value_at (rs, 2, now);
		if (value && !gda_value_is_null ((GdaValue *) value) && 
		    gda_value_get_string((GdaValue *) value) && 
		    (* gda_value_get_string((GdaValue *) value))) {
			str = gda_value_stringify ((GdaValue *) value);
			gda_object_set_description (GDA_OBJECT (dt), str);
			g_free (str);
		}
		else 
			gda_object_set_description (GDA_OBJECT (dt), NULL);

		/* owner */
		value = gda_data_model_get_value_at (rs, 1, now);
		if (value && !gda_value_is_null ((GdaValue *) value) && 
		    gda_value_get_string((GdaValue *) value) && 
		    (* gda_value_get_string((GdaValue *) value))) {
			str = gda_value_stringify ((GdaValue *) value);
			gda_object_set_owner (GDA_OBJECT (dt), str);
			g_free (str);
		}
		else
			gda_object_set_owner (GDA_OBJECT (dt), NULL);
				
		/* gda_type */
		value = gda_data_model_get_value_at (rs, 3, now);
		if (value && !gda_value_is_null ((GdaValue *) value)) 
			gda_dict_type_set_gda_type (dt, gda_value_get_gdatype ((GdaValue *) value));
		
		/* data type synomyms */
		gda_dict_type_clear_synonyms (dt);
		if (has_synonyms) {
			value = gda_data_model_get_value_at (rs, 4, now);
			if (value && !gda_value_is_null ((GdaValue *) value) && 
			    gda_value_get_string ((GdaValue *) value) && 
			    (* gda_value_get_string((GdaValue *) value))) {
				gchar *tok, *buf;

				str = gda_value_stringify ((GdaValue *) value);
				tok = strtok_r (str, ",", &buf);
				if (tok) {
					if (*tok) 
						gda_dict_type_add_synonym (dt, tok);
					tok = strtok_r (NULL, ",", &buf);
					while (tok) {
						if (*tok) 
							gda_dict_type_add_synonym (dt, tok);

						tok = strtok_r (NULL, ",", &buf);				
					}
				}
				g_free (str);
			}
		}

		/* signal if the DT is new */
		if (newdt) {
			gda_object_connect_destroy (dt,
						 G_CALLBACK (destroyed_data_type_cb), dict);
			g_signal_connect (G_OBJECT (dt), "changed",
					  G_CALLBACK (updated_data_type_cb), dict);
#ifdef GDA_DEBUG_signal
			g_print (">> 'DATA_TYPE_ADDED' from %s\n", __FUNCTION__);
#endif
			g_signal_emit (G_OBJECT (dict), gda_dict_signals[DATA_TYPE_ADDED], 0, dt);
#ifdef GDA_DEBUG_signal
			g_print ("<< 'DATA_TYPE_ADDED' from %s\n", __FUNCTION__);
#endif
		}

		g_signal_emit_by_name (G_OBJECT (dict), "update_progress", "DATA_TYPES",
				       now, total);
		now++;
	}
	
	g_object_unref (G_OBJECT (rs));
	
	/* if a data type has been updated, make sure it's not anymore in the custom
	 * data types list */
	dtl = dict->priv->custom_types;
	while (dtl) {
		if (g_slist_find (updated_dt, dtl->data)) {
			GSList *tmp = g_slist_next (dtl);

			g_object_ref (dtl->data);
			dict->priv->custom_types = g_slist_delete_link (dict->priv->custom_types, dtl);
			dtl = tmp;
		}
		else
			dtl = g_slist_next (dtl);
	}

	/* remove the data types not existing anymore and not in the custom list */
	dtl = dict->priv->data_types;
	while (dtl) {
		if (!g_slist_find (updated_dt, dtl->data) && 
		    !g_slist_find (dict->priv->custom_types, dtl->data)) {
			gda_object_destroy (GDA_OBJECT (dtl->data));
			dtl = dict->priv->data_types;
		}
		else
			dtl = g_slist_next (dtl);
	}
	g_slist_free (updated_dt);
	
	g_signal_emit_by_name (G_OBJECT (dict), "update_progress", NULL, 0, 0);
	
	return TRUE;
}

static GdaDictFunction *gda_dict_get_function_by_name_arg_real (GdaDict *dict, GSList *functions, 
								const gchar *funcname, const GSList *argtypes);

static gboolean
dict_functions_update_list (GdaDict *dict, GError **error)
{
	GdaDictFunction *func;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *list, *updated_fn = NULL, *todelete_fn = NULL;
	GSList *original_functions;
	gboolean insert;
	gint current_position = 0;

	/* no check here as the checks are supposed to have taken place once before calling this function */

	/* here we get the complete list of functions, and for each function, update or
	   create the entry in the list if not yet there. */
	rs = gda_connection_get_schema (GDA_CONNECTION (dict->priv->cnc),
					GDA_CONNECTION_SCHEMA_PROCEDURES, NULL, NULL);

	if (!rs) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_FUNCTIONS_ERROR,
			     _("Can't get list of functions"));
		return FALSE;
	}


	if (!utility_check_data_model (rs, 8, 
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_INTEGER,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING)) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_FUNCTIONS_ERROR,
			     _("Schema for list of functions is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}

	original_functions = gda_dict_get_functions (dict);
	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !dict->priv->stop_update) {
		GdaDictType *rettype = NULL; /* return type for the function */
		GSList *dtl = NULL;     /* list of params for the function */
		const GdaValue *value;
		gchar *ptr;
		gchar *tok;
		
		insert = TRUE;

		/* fetch return type */
		value = gda_data_model_get_value_at (rs, 4, now);
		str = gda_value_stringify ((GdaValue *) value);
		if (*str != '-') {
			rettype = gda_dict_get_data_type_by_name (dict, str);
			if (!rettype)
				insert = FALSE;
		}
		else 
			insert = FALSE;
		g_free (str);

		/* fetch argument types */
		value = gda_data_model_get_value_at (rs, 6, now);
		str = gda_value_stringify ((GdaValue *) value);
		if (str) {
			ptr = strtok_r (str, ",", &tok);
			while (ptr && *ptr) {
				GdaDictType *indt;

				if (*ptr == '-')
					dtl = g_slist_append (dtl, NULL); /* any data type will do */
				else {
					indt = gda_dict_get_data_type_by_name (dict, ptr);
					if (indt)
						dtl = g_slist_append (dtl, indt);
					else 
						insert = FALSE;
				}
				ptr = strtok_r (NULL, ",", &tok);
			}
			g_free (str);
		}

		/* fetch a func if there is already one with the same id */
		value = gda_data_model_get_value_at (rs, 1, now);
		str = gda_value_stringify ((GdaValue *) value);
		func = gda_dict_get_function_by_dbms_id (dict, str);
		g_free (str);

		if (!func) {
			/* try to find the function using its name, return type and argument type, 
			   and not its DBMS id, this is usefull if the DBMS has changed and the
			   DBMS id have changed */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GdaValue *) value);
			func = gda_dict_get_function_by_name_arg_real (dict, original_functions, str, dtl);
			g_free (str);

			if (func && (gda_dict_function_get_ret_type (func) != rettype))
				func = NULL;
		}

		if (!insert) {
			if (func)
				/* If no insertion, then func MUST be updated */
				todelete_fn = g_slist_append (todelete_fn, func);
			func = NULL;
		}
		else {
			if (func) {
				/* does the function we found have the same rettype and params 
				   as the one we have now? */
				gboolean isequal = TRUE;
				GSList *hlist;
				
				list = (GSList *) gda_dict_function_get_arg_types (func);
				hlist = dtl;
				while (list && hlist && isequal) {
					if (list->data != hlist->data)
						isequal = FALSE;
					list = g_slist_next (list);
					hlist = g_slist_next (hlist);
				}
				if (isequal && (gda_dict_function_get_ret_type (func) != rettype)) 
					isequal = FALSE;
				
				if (isequal) {
					updated_fn = g_slist_append (updated_fn, func);
					current_position = g_slist_index (dict->priv->functions, func) + 1;
					insert = FALSE;
				}
				else {
					todelete_fn = g_slist_append (todelete_fn, func);
					func = NULL;
				}
			}

			if (!func) {
				/* creating new ServerFunction object */
				func = GDA_DICT_FUNCTION (gda_dict_function_new (dict));
				gda_dict_function_set_ret_type (func, rettype);
				gda_dict_function_set_arg_types (func, dtl);

				/* mark function as updated */
				updated_fn = g_slist_append (updated_fn, func);
			}
		}
	
		if (dtl)
			g_slist_free (dtl);
		
		/* function's attributes update */
		if (func) {
			/* unique id */
			value = gda_data_model_get_value_at (rs, 1, now);
			str = gda_value_stringify ((GdaValue *) value);
			gda_dict_function_set_dbms_id (func, str);
			g_free (str);

			/* description */
			value = gda_data_model_get_value_at (rs, 3, now);
			if (value && !gda_value_is_null ((GdaValue *) value) && 
			    (* gda_value_get_string((GdaValue *) value))) {
				str = gda_value_stringify ((GdaValue *) value);
				gda_object_set_description (GDA_OBJECT (func), str);
				g_free (str);
			}
			
			/* sqlname */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GdaValue *) value);
			gda_dict_function_set_sqlname (func, str);
			g_free (str);
			
			/* owner */
			value = gda_data_model_get_value_at (rs, 2, now);
			if (value && !gda_value_is_null ((GdaValue *) value) && 
			    (* gda_value_get_string((GdaValue *) value))) {
				str = gda_value_stringify ((GdaValue *) value);
				gda_object_set_owner (GDA_OBJECT (func), str);
				g_free (str);
			}
			else
				gda_object_set_owner (GDA_OBJECT (func), NULL);
		}

		/* Real insertion */
		if (insert) {
			/* insertion in the list: finding where to insert the function */
			dict->priv->functions = g_slist_insert (dict->priv->functions, func, current_position++);
			gda_object_connect_destroy (func,
						 G_CALLBACK (destroyed_function_cb), dict);
			g_signal_connect (G_OBJECT (func), "changed",
					  G_CALLBACK (updated_function_cb), dict);

#ifdef GDA_DEBUG_signal
			g_print (">> 'FUNCTION_ADDED' from %s\n", __FUNCTION__);
#endif
			g_signal_emit_by_name (G_OBJECT (dict), "function_added", func);
#ifdef GDA_DEBUG_signal
			g_print ("<< 'FUNCTION_ADDED' from %s\n", __FUNCTION__);
#endif
		}

		g_signal_emit_by_name (G_OBJECT (dict), "update_progress", "FUNCTIONS", now, total);
		now ++;
	}

	g_object_unref (G_OBJECT (rs));
	if (original_functions)
		g_slist_free (original_functions);

	/* cleanup for the functions which do not exist anymore */
        list = dict->priv->functions;
        while (list && !dict->priv->stop_update) {
		if (!g_slist_find (updated_fn, list->data)) 
			todelete_fn = g_slist_append (todelete_fn, list->data);
		list = g_slist_next (list);
        }
	g_slist_free (updated_fn);

	list = todelete_fn;
	while (list) {
		gda_object_destroy (GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	g_slist_free (todelete_fn);
	
	g_signal_emit_by_name (G_OBJECT (dict), "update_progress", NULL, 0, 0);
	
	return TRUE;
}

static GdaDictAggregate *
gda_dict_get_aggregate_by_name_arg_real (GdaDict *dict, GSList *aggregates, const gchar *aggname, 
					 GdaDictType *argtype);

static gboolean
dict_aggregates_update_list (GdaDict *dict, GError **error)
{
	GdaDictAggregate *agg;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *list, *updated_aggs = NULL, *todelete_aggs = NULL;
	GSList *original_aggregates;
	gboolean insert;
	gint current_position = 0;

	/* no check here as the checks are supposed to have taken place once before calling this function */

	/* here we get the complete list of aggregates, and for each aggregate, update or
	   create the entry in the list if not yet there. */
	rs = gda_connection_get_schema (GDA_CONNECTION (dict->priv->cnc),
					GDA_CONNECTION_SCHEMA_AGGREGATES, NULL, NULL);

	if (!rs) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_AGGREGATES_ERROR,
			     _("Can't get list of aggregates"));
		return FALSE;
	}


	if (!utility_check_data_model (rs, 7, 
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING)) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_AGGREGATES_ERROR,
			     _("Schema for list of aggregates is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}

	original_aggregates = gda_dict_get_aggregates (dict);
	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !dict->priv->stop_update) {
		GdaDictType *outdt = NULL; /* return type for the aggregate */
		GdaDictType *indt = NULL;  /* argument for the aggregate */
		const GdaValue *value;
		
		insert = TRUE;

		/* fetch return type */
		value = gda_data_model_get_value_at (rs, 4, now);
		str = gda_value_stringify ((GdaValue *) value);
		if (*str != '-') {
			outdt = gda_dict_get_data_type_by_name (dict, str);
			if (!outdt)
				insert = FALSE;
		}
		else 
			insert = FALSE;
		g_free (str);

		/* fetch argument type */
		value = gda_data_model_get_value_at (rs, 5, now);
		str = gda_value_stringify ((GdaValue *) value);
		if (str) {
			if (*str != '-') {
				indt = gda_dict_get_data_type_by_name (dict, str);
				if (!indt)
					insert = FALSE;
			}
			g_free (str);
		}

		/* fetch a agg if there is already one with the same id */
		value = gda_data_model_get_value_at (rs, 1, now);
		str = gda_value_stringify ((GdaValue *) value);
		agg = gda_dict_get_aggregate_by_dbms_id (dict, str);
		g_free (str);

		if (!agg) {
			/* try to find the aggregate using its name, return type and argument type, 
			   and not its DBMS id, this is usefull if the DBMS has changed and the
			   DBMS id have changed */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GdaValue *) value);
			agg = gda_dict_get_aggregate_by_name_arg_real (dict, original_aggregates, str, indt);
			g_free (str);

			if (agg && (gda_dict_aggregate_get_ret_type (agg) != outdt))
				agg = NULL;
		}

		if (!insert) 
			agg = NULL;
		else {
			if (agg) {
				/* does the aggregate we found have the same outdt and indt
				   as the one we have now? */
				gboolean isequal = TRUE;
				if (gda_dict_aggregate_get_arg_type (agg) != indt)
					isequal = FALSE;
				if (isequal && (gda_dict_aggregate_get_ret_type (agg) != outdt)) 
					isequal = FALSE;
				
				if (isequal) {
					updated_aggs = g_slist_append (updated_aggs, agg);
					current_position = g_slist_index (dict->priv->aggregates, agg) + 1;
					insert = FALSE;
				}
				else 
					agg = NULL;
			}

			if (!agg) {
				/* creating new ServerAggregate object */
				agg = GDA_DICT_AGGREGATE (gda_dict_aggregate_new (dict));
				gda_dict_aggregate_set_ret_type (agg, outdt);
				gda_dict_aggregate_set_arg_type (agg, indt);
				
				/* mark aggregate as updated */
				updated_aggs = g_slist_append (updated_aggs, agg);
			}
		}
	
		/* aggregate's attributes update */
		if (agg) {
			/* unique id */
			value = gda_data_model_get_value_at (rs, 1, now);
			str = gda_value_stringify ((GdaValue *) value);
			gda_dict_aggregate_set_dbms_id (agg, str);
			g_free (str);

			/* description */
			value = gda_data_model_get_value_at (rs, 3, now);
			if (value && !gda_value_is_null ((GdaValue *) value) && 
			    (* gda_value_get_string((GdaValue *) value))) {
				str = gda_value_stringify ((GdaValue *) value);
				gda_object_set_description (GDA_OBJECT (agg), str);
				g_free (str);
			}
			
			/* sqlname */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GdaValue *) value);
			gda_dict_aggregate_set_sqlname (agg, str);
			g_free (str);
			
			/* owner */
			value = gda_data_model_get_value_at (rs, 2, now);
			if (value && !gda_value_is_null ((GdaValue *) value) && 
			    (* gda_value_get_string((GdaValue *) value))) {
				str = gda_value_stringify ((GdaValue *) value);
				gda_object_set_owner (GDA_OBJECT (agg), str);
				g_free (str);
			}
			else
				gda_object_set_owner (GDA_OBJECT (agg), NULL);
		}

		/* Real insertion */
		if (insert) {
			/* insertion in the list: finding where to insert the aggregate */
			dict->priv->aggregates = g_slist_insert (dict->priv->aggregates, agg, current_position++);
			gda_object_connect_destroy (agg,
						 G_CALLBACK (destroyed_aggregate_cb), dict);
			g_signal_connect (G_OBJECT (agg), "changed",
					  G_CALLBACK (updated_aggregate_cb), dict);

#ifdef GDA_DEBUG_signal
			g_print (">> 'AGGREGATE_ADDED' from %s\n", __FUNCTION__);
#endif
			g_signal_emit_by_name (G_OBJECT (dict), "aggregate_added", agg);
#ifdef GDA_DEBUG_signal
			g_print ("<< 'AGGREGATE_ADDED' from %s\n", __FUNCTION__);
#endif
		}

		g_signal_emit_by_name (G_OBJECT (dict), "update_progress", "AGGREGATES", now, total);
		now ++;
	}

	g_object_unref (G_OBJECT (rs));
	if (original_aggregates)
		g_slist_free (original_aggregates);

	/* cleanup for the aggregates which do not exist anymore */
        list = dict->priv->aggregates;
        while (list && !dict->priv->stop_update) {
		if (!g_slist_find (updated_aggs, list->data)) 
			todelete_aggs = g_slist_prepend (todelete_aggs, list->data);
		list = g_slist_next (list);
        }
	g_slist_free (updated_aggs);

	list = todelete_aggs;
	while (list) {
		gda_object_destroy (GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	g_slist_free (todelete_aggs);
		
	g_signal_emit_by_name (G_OBJECT (dict), "update_progress", NULL, 0, 0);
	
	return TRUE;
}

/**
 * gda_dict_stop_update_dbms_data
 * @dict: a #GdaDict object
 *
 * When the dictionary updates its internal lists of DBMS objects, a call to this function will 
 * stop that update process. It has no effect when the dictionary is not updating its DBMS data.
 */
void
gda_dict_stop_update_dbms_data (GdaDict *dict)
{
	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);

	dict->priv->stop_update = TRUE;
}

/**
 * gda_dict_compute_xml_filename
 * @dict: a #GdaDict object
 * @datasource: a data source, or %NULL
 * @app_id: an extra identification, or %NULL
 * @error: location to store error, or %NULL
 *
 * Get the prefered filename which represents the data dictionary associated to the @datasource data source.
 * Using the returned value in conjunction with gda_dict_load_xml_file() and gda_dict_save_xml_file() has
 * the advantage of letting the library handle file naming onventions.
 *
 * if @datasource is %NULL, and a #GdaConnection object has been assigned to @dict, then the returned
 * string will take into account the data source used by that connection.
 *
 * The @app_id argument allows to give an extra identification to the request, when some special features
 * must be saved but not interfere with the default dictionary.
 *
 * Returns: a new string
 */
gchar *
gda_dict_compute_xml_filename (GdaDict *dict, const gchar *datasource, const gchar *app_id, GError **error)
{
	gchar *str;
	gboolean with_error = FALSE;

#define LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S ".libgda"

	g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	if (!datasource) {
		if (dict->priv->cnc)
			datasource = gda_connection_get_dsn (dict->priv->cnc);
		else {
			g_warning ("datasource != NULL failed");
			return NULL;
		}
	}

	if (!app_id)
		str = g_strdup_printf ("%s%sDICT_%s_default.xml", g_get_home_dir (), LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S,
				       datasource);
	else
		str = g_strdup_printf ("%s%sDICT_%s_%s.xml", g_get_home_dir (), LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S,
				       datasource, app_id);

	/* create an empty file with that name */
	if (!g_file_test (str, G_FILE_TEST_EXISTS)) {
		gchar *dirpath;
		FILE *fp;
		
		dirpath = g_strdup_printf ("%s%s", g_get_home_dir (), LIBGDA_USER_CONFIG_DIR);
		if (!g_file_test (dirpath, G_FILE_TEST_IS_DIR)){
			if (mkdir (dirpath, 0700)) {
				g_set_error (error,
					     GDA_DICT_ERROR,
					     GDA_DICT_FILE_LOAD_ERROR,
					     _("Error creating directory %s"), dirpath);
				with_error = TRUE;
			}
		}
		g_free (dirpath);

		/* fp = fopen (str, "wt"); */
/* 		if (fp == NULL) { */
/* 			g_set_error (error, */
/* 				     GDA_DICT_ERROR, */
/* 				     GDA_DICT_FILE_LOAD_ERROR, */
/* 				     _("Unable to create the dictionary file %s"), str); */
/* 			with_error = TRUE; */
/* 		} */
/* 		else */
/* 			fclose (fp); */
	}

	if (with_error) {
		g_free (str);
		str = NULL;
	}

	return str;
}


/**
 * gda_dict_set_xml_filename
 * @dict: a #GdaDict object
 * @xmlfile: a file name
 *
 * Sets the filename @dict will use when gda_dict_save_xml() and gda_dict_load_xml() are called.
 */
void
gda_dict_set_xml_filename (GdaDict *dict, const gchar *xmlfile)
{
	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);

	if (dict->priv->xml_filename) {
		g_free (dict->priv->xml_filename);
		dict->priv->xml_filename = NULL;
	}
	
	if (xmlfile)
		dict->priv->xml_filename = g_strdup (xmlfile);
}

/**
 * gda_dict_get_xml_filename
 * @dict: a #GdaDict object
 *
 * Get the filename @dict will use when gda_dict_save_xml() and gda_dict_load_xml() are called.
 *
 * Returns: the filename, or %NULL if none have been set.
 */
const gchar *
gda_dict_get_xml_filename (GdaDict *dict)
{
	g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	return dict->priv->xml_filename;
}

/**
 * gda_dict_load
 * @dict: a #GdaDict object
 * @error: location to store error, or %NULL
 * 
 * Loads an XML file which respects the Libgda DTD, and creates all the necessary
 * objects that are defined within the XML file. During the creation of the other
 * objects, all the normal signals are emitted.
 *
 * If the GdaDict object already has some contents, then it is first of all
 * destroyed (to return its state as when it was first created).
 *
 * If an error occurs during loading then the GdaDict object is left as empty
 * as when it is first created.
 *
 * The file loaded is the one specified using gda_dict_set_xml_filename()
 *
 * Returns: TRUE if loading was successfull and FALSE otherwise.
 */
gboolean
gda_dict_load (GdaDict *dict, GError **error)
{
	g_return_val_if_fail (dict && GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);

	return gda_dict_load_xml_file (dict, dict->priv->xml_filename, error);
}

/**
 * gda_dict_save
 * @dict: a #GdaDict object
 * @error: location to store error, or %NULL
 *
 * Saves the contents of a GdaDict object to a file which is specified using the
 * gda_dict_set_xml_filename() method.
 *
 * Returns: TRUE if saving was successfull and FALSE otherwise.
 */
gboolean
gda_dict_save (GdaDict *dict, GError **error)
{
	g_return_val_if_fail (dict && GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);

	return gda_dict_save_xml_file (dict, dict->priv->xml_filename, error);
}


/**
 * gda_dict_get_entities_fk_constraints
 * @dict: a #GdaDict object
 * @entity1: an object implementing the #GdaEntity interface
 * @entity2: an object implementing the #GdaEntity interface
 * @entity1_has_fk: TRUE if the returned constraints are the one for which @entity1 contains the foreign key
 *
 * Get a list of all the constraints which represent a foreign constrains, between
 * @entity1 and @entity2. If @entity1 and @entity2 are #GdaDictTable objects, then the
 * constraints are the ones from the database.
 *
 * Constraints are represented as #GdaDictConstraint objects.
 *
 * Returns: a new list of the constraints
 */
GSList *
gda_dict_get_entities_fk_constraints (GdaDict *dict, GdaEntity *entity1, GdaEntity *entity2,
				     gboolean entity1_has_fk)
{
	GSList *retval = NULL;

	g_return_val_if_fail (dict && GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (entity1 && GDA_IS_ENTITY (entity1), NULL);
	g_return_val_if_fail (entity2 && GDA_IS_ENTITY (entity2), NULL);
	if (entity1 == entity2)
		return NULL;

	if (GDA_IS_DICT_TABLE (entity1)) {
		if (GDA_IS_DICT_TABLE (entity2)) 
			retval = gda_dict_database_get_tables_fk_constraints (dict->priv->database, 
									GDA_DICT_TABLE (entity1), GDA_DICT_TABLE (entity2),
									entity1_has_fk);
	}

	return retval;
}

/**
 * gda_dict_get_data_types
 * @dict: a #GdaDict object
 *
 * Get the list of data types;
 *
 * Returns: the list (the caller must free the list after usage)
 */
GSList *
gda_dict_get_data_types (GdaDict *dict)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	if (dict->priv->data_types)
		return g_slist_copy (dict->priv->data_types);
	else
		return NULL;
}

/**
 * gda_dict_get_data_type_by_name
 * @dict: a #GdaDict object
 * @type_name: the name of the requested data type
 *
 * Find a data type from its DBMS name or from one of its synonyms if it has some.
 *
 * Returns: the data type or %NULL if it cannot be found
 */
GdaDictType  *
gda_dict_get_data_type_by_name (GdaDict *dict, const gchar *type_name)
{
	GSList *list;
	GdaDictType *datatype = NULL;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (type_name && *type_name, NULL);
	
	/* compare the data types names */
	list = dict->priv->data_types;
	while (list && !datatype) {
		if (!strcmp (gda_dict_type_get_sqlname (GDA_DICT_TYPE (list->data)),
			     type_name))
			datatype = GDA_DICT_TYPE (list->data);
		list = g_slist_next (list);
	}

	/* if not found, then compare with the synonyms */
	list = dict->priv->data_types;
	while (list && !datatype) {
		const GSList *synlist = gda_dict_type_get_synonyms (GDA_DICT_TYPE (list->data));
		while (synlist && !datatype) {
			if (!strcmp ((gchar *) (synlist->data), type_name))
				datatype = GDA_DICT_TYPE (list->data);
			synlist = g_slist_next (synlist);
		}
		list = g_slist_next (list);
	}

	return datatype;
}

/**
 * gda_dict_add_data_type_real 
 * @dict:
 * @datatype:
 *
 * Adds a data type to @dict
 *
 * Reserved for internal usage only.
 */
void
gda_dict_add_data_type_real (GdaDict *dict, GdaDictType *datatype)
{
	gint i = 0;
	GSList *list;
	gboolean found = FALSE;
	const gchar *str;

	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (GDA_IS_DICT_TYPE (datatype));
	str = gda_dict_type_get_sqlname (datatype);
	g_return_if_fail (! gda_dict_get_data_type_by_name (dict, str));

	/* finding the right position for the data type */
	list = dict->priv->data_types;
	while (list && !found) {
		if (strcmp (str,
			    gda_dict_type_get_sqlname (GDA_DICT_TYPE (list->data))) < 0)
			found = TRUE;
		else
			i++;
		list = g_slist_next (list);
	}
	dict->priv->data_types = g_slist_insert (dict->priv->data_types, datatype, i);
	g_object_ref (datatype);

	/* signals */
	gda_object_connect_destroy (datatype, G_CALLBACK (destroyed_data_type_cb), dict);
	g_signal_connect (G_OBJECT (datatype), "changed",
			  G_CALLBACK (updated_data_type_cb), dict);
#ifdef GDA_DEBUG_signal
	g_print (">> 'DATA_TYPE_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (dict), gda_dict_signals[DATA_TYPE_ADDED], 0, datatype);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'DATA_TYPE_ADDED' from %s\n", __FUNCTION__);
#endif
}

/**
 * gda_dict_get_data_type_by_xml_id
 * @dict: a #GdaDict object
 * @xml_id: the XML identifier of the data type to be found
 *
 * To find a #GdaDictType using its XML id.
 *
 * Returns: the data type or %NULL if it cannot be found
 */
GdaDictType  *
gda_dict_get_data_type_by_xml_id (GdaDict *dict, const gchar *xml_id)
{
	GSList *list;
	GdaDictType *datatype = NULL;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);
	
	list = dict->priv->data_types;
	while (list && !datatype) {
		gchar *id = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
		if (!strcmp (id, xml_id))
			datatype = GDA_DICT_TYPE (list->data);
		g_free (id);
		list = g_slist_next (list);
	}

	return datatype;
}

/**
 * gda_dict_declare_custom_data_type
 * @dict: a #GdaDict object
 * @type: a #GdaDictType object
 *
 * Forces @dict to consider @type as a new data type even though that data type
 * is not declared by the database to which @dict can be connected to.
 *
 * Returns: TRUE if the data type does not already exist, and FALSE if it does.
 */
gboolean
gda_dict_declare_custom_data_type (GdaDict *dict, GdaDictType *type)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);
	g_return_val_if_fail (type && GDA_IS_DICT_TYPE (type), FALSE);

	if (gda_dict_get_data_type_by_name (dict, gda_dict_type_get_sqlname (type)))
		return FALSE;
	else {
		gda_dict_add_data_type_real (dict, type);
		dict->priv->custom_types = g_slist_append (dict->priv->custom_types, type);
	}

	return TRUE;
}

/**
 * gda_dict_get_functions
 * @dict: a #GdaDict object
 *
 * To get the complete list of functions
 *
 * Returns: the allocated list of functions
 */
GSList *
gda_dict_get_functions (GdaDict *dict)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	if (dict->priv->functions)
		return g_slist_copy (dict->priv->functions);
	else
		return NULL;
}


/**
 * gda_dict_get_functions_by_name
 * @dict: a #GdaDict object
 * @funcname: name of the function
 *
 * To get the list of DBMS functions which match the given name.
 *
 * Returns: the allocated list of functions
 */
GSList *
gda_dict_get_functions_by_name (GdaDict *dict, const gchar *funcname)
{
	GSList *retval = NULL, *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (funcname && *funcname, NULL);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (funcname, -1);
	else
		cmpstr = (gchar *) funcname;

	list = dict->priv->functions;
	while (list) {
		if (LC_NAMES (dict)) {
			cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
			if (!strcmp (cmpstr2, cmpstr))
				retval = g_slist_prepend (retval, list->data);
			g_free (cmpstr2);
		}
		else
			if (!strcmp (gda_object_get_name (GDA_OBJECT (list->data)), cmpstr))
				retval = g_slist_prepend (retval, list->data);
		list = g_slist_next (list);
	}

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return retval;
}

static GdaDictFunction *
gda_dict_get_function_by_name_arg_real (GdaDict *dict, GSList *functions, const gchar *funcname, const GSList *argtypes)
{
	GdaDictFunction *func = NULL; /* prefectely matching function */
	GdaDictFunction *anytypefunc = NULL; /* matching because it accepts any data type for one of its args */
	GdaDictFunction *gdatypefunc = NULL; /* matching because its arg type is the same gda type as requested for one of its args */
	GSList *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	GdaServerProviderInfo *sinfo = NULL;
	GdaConnection *cnc;

	cnc = gda_dict_get_connection (dict);
        if (cnc)
                sinfo = gda_connection_get_infos (cnc);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (funcname, -1);
	else
		cmpstr = (gchar *) funcname;

	list = functions;
	while (list && !func) {
		gboolean argsok = TRUE;
		gboolean func_any_type = FALSE;
		gboolean func_gda_type = FALSE;
		GSList *funcargs, *list2;
		GdaDictFunction *tmp = NULL;

		/* arguments comparison */
		list2 = (GSList *) argtypes;
		funcargs = (GSList *) gda_dict_function_get_arg_types (GDA_DICT_FUNCTION (list->data));
		while (funcargs && list2 && argsok) {
			gboolean tmpok = FALSE;

			if (funcargs->data == list2->data)
				tmpok = TRUE;
			else {
				if (!funcargs->data) {
					tmpok = TRUE;
					func_any_type = TRUE;
				}
				else {
					if (funcargs->data && list2->data &&
					    sinfo && sinfo->implicit_data_types_casts &&
					    (gda_dict_type_get_gda_type (funcargs->data) == 
					     gda_dict_type_get_gda_type (list2->data))) {
						tmpok = TRUE;
						func_gda_type = TRUE;
					}
				}
			}
			
			if (!tmpok)
				argsok = FALSE;
			funcargs = g_slist_next (funcargs);
			list2 = g_slist_next (list2);
		}
		if (list2 || funcargs)
			argsok = FALSE;

		/* names comparison */
		if (argsok) {
			if (LC_NAMES (dict)) {
				cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
				if (!strcmp (cmpstr2, cmpstr))
					tmp = GDA_DICT_FUNCTION (list->data);
				g_free (cmpstr2);
			}
			else
				if (!strcmp (cmpstr, gda_object_get_name (GDA_OBJECT (list->data))))
					tmp = GDA_DICT_FUNCTION (list->data);
		}

		if (tmp) {
			if (func_any_type)
				anytypefunc = tmp;
			else {
				if (func_gda_type)
					gdatypefunc = tmp;
				else
					func = tmp;
			}
		}
		
		list = g_slist_next (list);
	}

	if (!func && gdatypefunc)
		func = gdatypefunc;
	if (!func && anytypefunc)
		func = anytypefunc;

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return func;
}

/**
 * gda_dict_get_function_by_name_arg
 * @dict: a #GdaDict object
 * @funcname: name of the function
 * @argtypes: a list of #GdaDictType objects
 *
 * To find a DBMS function which is uniquely identified by its name and the type(s)
 * of its argument(s).
 *
 * About the functions accepting any data type for one of their argument: if the corresponding data type in
 * @argtypes is not %NULL, then such a function will be a candidate, and if the corresponding data type in
 * @argtypes is %NULL, then only such a function will be a candidate.
 *
 * Returns: The function or NULL if not found
 */
GdaDictFunction *
gda_dict_get_function_by_name_arg (GdaDict *dict, const gchar *funcname, const GSList *argtypes)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (funcname && *funcname, NULL);

	return gda_dict_get_function_by_name_arg_real (dict, dict->priv->functions, funcname, argtypes);
}

/**
 * gda_dict_get_function_by_dbms_id
 * @dict: a #GdaDict object
 * @dbms_id: 
 *
 * To find a DBMS functions which is uniquely identified by its DBMS identifier
 *
 * Returns: The function or NULL if not found
 */
GdaDictFunction *
gda_dict_get_function_by_dbms_id (GdaDict *dict, const gchar *dbms_id)
{
	GdaDictFunction *func = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (dbms_id && *dbms_id, NULL);

	list = dict->priv->functions;
	while (list && !func) {
		gchar *str = gda_dict_function_get_dbms_id (GDA_DICT_FUNCTION (list->data));
		if (!str || ! *str) {
			str = (gchar *) gda_dict_function_get_sqlname (GDA_DICT_FUNCTION (list->data));
			g_error ("Function %p (%s) has no dbms_id", list->data, str);
		}
		if (str && !strcmp (dbms_id, str))
			func = GDA_DICT_FUNCTION (list->data);
		g_free (str);
		list = g_slist_next (list);
	}

	return func;
}

/**
 * gda_dict_get_function_by_xml_id
 * @dict: a #GdaDict object
 * @xml_id: 
 *
 * To find a DBMS functions which is uniquely identified by its XML identifier
 *
 * Returns: The function or NULL if not found
 */
GdaDictFunction *
gda_dict_get_function_by_xml_id (GdaDict *dict, const gchar *xml_id)
{
	GdaDictFunction *func = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	list = dict->priv->functions;
	while (list && !func) {
		const gchar *str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
		if (!strcmp (xml_id, str))
			func = GDA_DICT_FUNCTION (list->data);
		list = g_slist_next (list);
	}

	return func;
}



/**
 * gda_dict_get_aggregates
 * @dict: a #GdaDict object
 *
 * To get the complete list of aggregates
 *
 * Returns: the allocated list of aggregates
 */
GSList *
gda_dict_get_aggregates (GdaDict *dict)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	if (dict->priv->functions)
		return g_slist_copy (dict->priv->aggregates);
	else
		return NULL;
}

/**
 * gda_dict_get_aggregates_by_name
 * @dict: a #GdaDict object
 * @aggname: the name of the aggregate
 *
 * To get the list of DBMS aggregates which match the given name.
 *
 * Returns: the allocated list of aggregates
 */
GSList *
gda_dict_get_aggregates_by_name (GdaDict *dict, const gchar *aggname)
{
	GSList *retval = NULL, *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (aggname && *aggname, NULL);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (aggname, -1);
	else
		cmpstr = (gchar *) aggname;

	list = dict->priv->aggregates;
	while (list) {
		if (LC_NAMES (dict)) {
			cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
			if (!strcmp (cmpstr2, cmpstr))
				retval = g_slist_prepend (retval, list->data);
			g_free (cmpstr2);
		}
		else
			if (!strcmp (gda_object_get_name (GDA_OBJECT (list->data)), cmpstr))
				retval = g_slist_prepend (retval, list->data);
		list = g_slist_next (list);
	}

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return retval;;
}

static GdaDictAggregate *
gda_dict_get_aggregate_by_name_arg_real (GdaDict *dict, GSList *aggregates, const gchar *aggname, 
					 GdaDictType *argtype)
{
	GdaDictAggregate *agg = NULL; /* prefectely matching aggregate */
	GdaDictAggregate *anytypeagg = NULL; /* matching because it accepts any data type */
	GdaDictAggregate *gdatypeagg = NULL; /* matching because its arg type is the same gda type as requested */
	GSList *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	GdaServerProviderInfo *sinfo = NULL;
	GdaConnection *cnc;

	cnc = gda_dict_get_connection (dict);
        if (cnc)
                sinfo = gda_connection_get_infos (cnc);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (aggname, -1);
	else
		cmpstr = (gchar *) aggname;

	list = aggregates;
	while (list && !agg) {
		GdaDictType *testdt = gda_dict_aggregate_get_arg_type (GDA_DICT_AGGREGATE (list->data));
		GdaDictAggregate *tmp = NULL;
		gint mode = 0;

		if (argtype == testdt) {
			tmp = GDA_DICT_AGGREGATE (list->data);
			mode = 1;
		}
		else {
			if (!testdt) {
				tmp = GDA_DICT_AGGREGATE (list->data);
				mode = 2;
			}
			else {
				if (argtype && testdt &&
				    sinfo && sinfo->implicit_data_types_casts &&
				    (gda_dict_type_get_gda_type (testdt) == 
				     gda_dict_type_get_gda_type (argtype))) {
					tmp = GDA_DICT_AGGREGATE (list->data);
					mode = 3;
				}
			}
		}

		if (tmp) {
			if (LC_NAMES (dict)) {
				cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
				if (strcmp (cmpstr2, cmpstr))
					tmp = NULL;
				g_free (cmpstr2);
			}
			else
				if (strcmp (cmpstr, gda_object_get_name (GDA_OBJECT (list->data))))
					tmp = NULL;
		}

		if (tmp) {
			switch (mode) {
			case 1:
				agg = tmp;
				break;
			case 2:
				anytypeagg = tmp;
				break;
			case 3:
				gdatypeagg = tmp;
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
		
		list = g_slist_next (list);
	}

	if (!agg && gdatypeagg)
		agg = gdatypeagg;

	if (!agg && anytypeagg)
		agg = anytypeagg;

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return agg;	
}

/**
 * gda_dict_get_aggregate_by_name_arg
 * @dict: a #GdaDict object
 * @aggname: the name of the aggregate
 * @argtype: the type of argument or %NULL
 *
 * To find a DBMS aggregate which is uniquely identified by its name and the type
 * of its argument.
 *
 * About the aggregates accepting any data type for their argument: if @argtype is not %NULL
 * then such an aggregate will be a candidate, and if @argtype is %NULL
 * then only such an aggregate will be a candidate.
 *
 * If several aggregates are found, then the aggregate completely matching will be returned, or
 * an aggregate where the argument type has the same GDA typa as the @argtype, or lastly an
 * aggregate accepting any data type as argument.
 *
 * Returns: The aggregate or NULL if not found
 */
GdaDictAggregate *
gda_dict_get_aggregate_by_name_arg (GdaDict *dict, const gchar *aggname, GdaDictType *argtype)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (aggname && *aggname, NULL);
	if (argtype)
		g_return_val_if_fail (GDA_IS_DICT_TYPE (argtype), NULL);

	return gda_dict_get_aggregate_by_name_arg_real (dict, dict->priv->aggregates, aggname, argtype);
}


/**
 * gda_dict_get_aggregate_by_dbms_id
 * @dict: a #GdaDict object
 * @dbms_id: 
 *
 * To find a DBMS functions which is uniquely identified by its name and the type
 * of its argument.
 *
 * Returns: The aggregate or NULL if not found
 */
GdaDictAggregate *
gda_dict_get_aggregate_by_dbms_id (GdaDict *dict, const gchar *dbms_id)
{
	GdaDictAggregate *agg = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (dbms_id && *dbms_id, NULL);

	list = dict->priv->aggregates;
	while (list && !agg) {
		gchar *str;

		str = gda_dict_aggregate_get_dbms_id (GDA_DICT_AGGREGATE (list->data));
		if (!strcmp (dbms_id, str))
			agg = GDA_DICT_AGGREGATE (list->data);
		g_free (str);
		list = g_slist_next (list);
	}

	return agg;
}

/**
 * gda_dict_get_aggregate_by_xml_id
 * @dict: a #GdaDict object
 * @xml_id: 
 *
 * To find a DBMS aggregates which is uniquely identified by its XML identifier
 *
 * Returns: The aggregate or NULL if not found
 */
GdaDictAggregate *
gda_dict_get_aggregate_by_xml_id (GdaDict *dict, const gchar *xml_id)
{
	GdaDictAggregate *agg = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	list = dict->priv->aggregates;
	while (list && !agg) {
		const gchar *str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
		if (!strcmp (xml_id, str))
			agg = GDA_DICT_AGGREGATE (list->data);
		list = g_slist_next (list);
	}

	return agg;
}

/**
 * gda_dict_get_default_handler
 * @dict : a #GdaDict object
 * @for_type: a #GdaValueType type
 *
 * Obtain a pointer to a #GdaDataHandler which can manage
 * #GdaValue values of type @for_type
 *
 * The returned pointer is %NULL if @for_type is GDA_VALUE_TYPE_NULL, 
 * or if there is no default data handler available.
 *
 * Returns: a #GdaDataHandler
 */
GdaDataHandler* 
gda_dict_get_default_handler (GdaDict *dict, GdaValueType for_type)
{
	if (!default_dict_handlers) {
		default_dict_handlers = g_new0 (GdaDataHandler*, GDA_VALUE_TYPE_UNKNOWN);

		/* FIXME: there MUST not be any NULL value below, except for GDA_VALUE_TYPE_NULL */
		default_dict_handlers [GDA_VALUE_TYPE_NULL] = NULL;
		default_dict_handlers [GDA_VALUE_TYPE_BIGINT] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_BIGUINT] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_BINARY] = gda_handler_bin_new ();
		default_dict_handlers [GDA_VALUE_TYPE_BLOB] = gda_handler_bin_new ();
		default_dict_handlers [GDA_VALUE_TYPE_BOOLEAN] = gda_handler_boolean_new ();
		default_dict_handlers [GDA_VALUE_TYPE_DATE] = gda_handler_time_new_no_locale ();
		default_dict_handlers [GDA_VALUE_TYPE_DOUBLE] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_GEOMETRIC_POINT] = NULL;
		default_dict_handlers [GDA_VALUE_TYPE_GOBJECT] = NULL;
		default_dict_handlers [GDA_VALUE_TYPE_INTEGER] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_LIST] = NULL;
		default_dict_handlers [GDA_VALUE_TYPE_MONEY] = NULL;
		default_dict_handlers [GDA_VALUE_TYPE_NUMERIC] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_SINGLE] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_SMALLINT] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_SMALLUINT] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_STRING] = gda_handler_string_new ();
		default_dict_handlers [GDA_VALUE_TYPE_TIME] = gda_handler_time_new_no_locale ();
		default_dict_handlers [GDA_VALUE_TYPE_TIMESTAMP] = gda_handler_time_new_no_locale ();
		default_dict_handlers [GDA_VALUE_TYPE_TINYINT] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_TINYUINT] = gda_handler_numerical_new ();
		default_dict_handlers [GDA_VALUE_TYPE_TYPE] = gda_handler_type_new ();
		default_dict_handlers [GDA_VALUE_TYPE_UINTEGER] = gda_handler_numerical_new ();		
	}

	return default_dict_handlers [for_type];
}
