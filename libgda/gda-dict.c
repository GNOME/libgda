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
#include <glib/gstdio.h>
#include "gda-dict.h"
#include "gda-dict-private.h"
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
#include <libgda/gda-util.h>
#include "gda-server-provider.h"

#include "handlers/gda-handler-boolean.h"
#include "handlers/gda-handler-numerical.h"
#include "handlers/gda-handler-string.h"
#include "handlers/gda-handler-time.h"

#include "gda-dict-reg-queries.h"
#include "gda-dict-reg-types.h"
#include "gda-dict-reg-aggregates.h"
#include "gda-dict-reg-functions.h"

#if defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED) && \
    defined(LIBXML_OUTPUT_ENABLED)
#define XML_ID_TEST 
#endif

extern xmlDtdPtr gda_dict_dtd;

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

static void reg_object_weak_ref_notify (GdaDict *dict, GdaObject *object);

static void dsn_changed_cb (GdaConnection *cnc, GdaDict *dict);


static void dict_changed (GdaDict *dict, gpointer data);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* default handlers for all dictionaries, NULL until it has been initialised */
static GHashTable *default_dict_handlers = NULL; /* key = GType, value = GdaDataHandler obj */

/* signals */
enum
{
	OBJECT_ADDED,
	OBJECT_REMOVED,
	OBJECT_UPDATED,
	OBJECT_ACT_CHANGED,

        DATA_UPDATE_STARTED,
        UPDATE_PROGRESS,
        DATA_UPDATE_FINISHED,

	CHANGED,
	LAST_SIGNAL
};

static gint gda_dict_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_DSN,
	PROP_USERNAME
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

	gda_dict_signals[OBJECT_ADDED] =
		g_signal_new ("object_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, object_added),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[OBJECT_REMOVED] =
		g_signal_new ("object_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, object_removed),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	gda_dict_signals[OBJECT_UPDATED] =
		g_signal_new ("object_updated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, object_updated),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	gda_dict_signals[OBJECT_ACT_CHANGED] =
		g_signal_new ("object_act_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, object_act_changed),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER, G_TYPE_NONE,
			      1, G_TYPE_POINTER);

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

	gda_dict_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDictClass, changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE,
			      0);
	class->object_added = (void (*) (GdaDict *, GdaObject *)) dict_changed;
	class->object_removed = (void (*) (GdaDict *, GdaObject *)) dict_changed;
	class->object_updated = (void (*) (GdaDict *, GdaObject *)) dict_changed;
	class->object_act_changed = (void (*) (GdaDict *, GdaObject *)) dict_changed;

	class->changed = NULL;

	/* Properties */
	object_class->set_property = gda_dict_set_property;
	object_class->get_property = gda_dict_get_property;
	g_object_class_install_property (object_class, PROP_DSN,
					 g_param_spec_string ("dsn", _("DSN of the connection to use"), NULL, NULL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_USERNAME,
					 g_param_spec_string ("username", _("Username for the connection to use"), 
							      NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->dispose = gda_dict_dispose;
	object_class->finalize = gda_dict_finalize;

	/* class variable */
	class->class_registry_list = NULL;
	class->class_registry_list = g_slist_append (class->class_registry_list, 
						     gda_types_get_register);
	class->class_registry_list = g_slist_append (class->class_registry_list, 
						     gda_queries_get_register);
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

gboolean
uint_equal (gconstpointer  v1, gconstpointer  v2)
{
	return GPOINTER_TO_UINT (v1) == GPOINTER_TO_UINT (v2);
}
guint
uint_hash (gconstpointer  v)
{
	return GPOINTER_TO_UINT (v);
}

static void
gda_dict_init (GdaDict * dict)
{
	dict->priv = g_new0 (GdaDictPrivate, 1);
	dict->priv->database = NULL;
	dict->priv->cnc = NULL;
	dict->priv->xml_filename = NULL;
	dict->priv->dsn = NULL;
	dict->priv->user = NULL;
	dict->priv->object_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	dict->priv->registry_list = NULL;
	dict->priv->registry = g_hash_table_new (uint_hash, uint_equal);
	dict->priv->registry_xml_groups = g_hash_table_new (g_str_hash, g_str_equal);
	dict->priv->objects_as_hash = g_hash_table_new (NULL, NULL);
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
	GObject *obj;
	GdaDict *dict;
	GSList *list;

	obj = g_object_new (GDA_TYPE_DICT, NULL);
	dict = GDA_DICT (obj);

	/* Server and database objects creation */
	dict->priv->cnc = NULL;
	dict->priv->database = GDA_DICT_DATABASE (gda_dict_database_new (dict));

	/* register basic functionality */
	list = GDA_DICT_CLASS (G_OBJECT_GET_CLASS (obj))->class_registry_list;
	for (; list; list = list->next) 
		gda_dict_register_object_type (dict, ((GdaDictRegFunc)(list->data)) ());

	return obj;
}

/**
 * gda_dict_extend_with_functions
 * @dict: a #GdaDict object
 *
 * Make @dict handle functions and aggregates
 */
void
gda_dict_extend_with_functions (GdaDict *dict)
{
	GdaDictRegisterStruct *reg;

	g_return_if_fail (GDA_IS_DICT (dict));

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	if (!reg)
		gda_dict_register_object_type (dict, gda_functions_get_register ());
	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	if (!reg)
		gda_dict_register_object_type (dict, gda_aggregates_get_register ());
}

static void
registry_hash_foreach_cb (guint key, GdaDictRegisterStruct *reg, GdaDict *dict)
{
	GSList *list, *copy;

	/* assumed objects list */
	copy = g_slist_copy (reg->assumed_objects);
	for (list = copy; list; list = list->next)
		gda_object_destroy (GDA_OBJECT (list->data));
	g_slist_free (copy);
	g_assert (! reg->assumed_objects);

	/* all objects list */
	copy = g_slist_copy (reg->all_objects);
	for (list = copy; list; list = list->next) {
		g_object_weak_unref (G_OBJECT (list->data), (GWeakNotify) reg_object_weak_ref_notify, dict);
		gda_object_destroy (GDA_OBJECT (list->data));
	}
	g_slist_free (copy);
	g_slist_free (reg->all_objects);
	reg->all_objects = NULL;

	/* clean @reg */
	if (reg->free)
		(reg->free)(dict, reg);
	else
		g_free (reg);
}

static void
gda_dict_dispose (GObject   * object)
{
	GdaDict *dict;

	g_return_if_fail (GDA_IS_DICT (object));

	dict = GDA_DICT (object);
	if (dict->priv) {
		/* generic registered objects */
		if (dict->priv->registry) {
			g_hash_table_foreach (dict->priv->registry, (GHFunc) registry_hash_foreach_cb, dict);
			g_hash_table_destroy (dict->priv->registry);
			dict->priv->registry = NULL;
		}

		if (dict->priv->objects_as_hash) {
			g_hash_table_destroy (dict->priv->objects_as_hash);
			dict->priv->objects_as_hash = NULL;
		}

		if (dict->priv->registry_xml_groups) {
			g_hash_table_destroy (dict->priv->registry_xml_groups);
			dict->priv->registry_xml_groups = NULL;
		}

		if (dict->priv->registry_list) {
			g_slist_free (dict->priv->registry_list);
			dict->priv->registry_list = NULL;
		}


		/* database */
		if (dict->priv->database) {
			g_object_unref (G_OBJECT (dict->priv->database));
			dict->priv->database = NULL;
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
			xmlChar *oid, *id, *name;
			
			name = xmlGetProp (node, BAD_CAST "name");
			oid = xmlGetProp (node, BAD_CAST "id");
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
		int xmlcheck;

		validc = g_new0 (xmlValidCtxt, 1);
		validc->userData = dict;
		validc->error = xml_validity_error_func;
		validc->warning = NULL; 

		xmlcheck = xmlDoValidityCheckingDefaultValue;
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

			xmlDoValidityCheckingDefaultValue = xmlcheck;
			return FALSE;
		}
		xmlDoValidityCheckingDefaultValue = xmlcheck;
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

	while (subnode) {
		gboolean done = FALSE;
		/* Datasource */
		if (!strcmp (subnode->name, "gda_dsn_info")) {
			g_free (dict->priv->dsn);			
			g_free (dict->priv->user);

			dict->priv->dsn = xmlGetProp (subnode, "dsn");
			dict->priv->user = xmlGetProp (subnode, "user");
			done = TRUE;
		}

		/* GdaDictDatabase object */
		if (!done && !strcmp (subnode->name, "gda_dict_database")) {
			if (!gda_xml_storage_load_from_xml (GDA_XML_STORAGE (dict->priv->database), subnode, error))
				return FALSE;
			done = TRUE;
		}

		/* If a <gda-dict-aggregates> or a <gda-dict-procedures> is found, then make GdaDict handle functions */
		if (!done && (!strcmp (subnode->name, "gda_dict_aggregates") ||
			      !strcmp (subnode->name, "gda_dict_procedures")))
			gda_dict_extend_with_functions (dict);

		/* generic objects */
		if (!done) {
			GdaDictRegisterStruct *reg = NULL;

			reg = g_hash_table_lookup (dict->priv->registry_xml_groups, subnode->name);
			if (reg) {
				if (!reg->load_xml_tree)
					g_warning (_("Could not load XML data for %s (no registered load function)"), 
						   subnode->name);
				else
					if (!(reg->load_xml_tree) (dict, subnode, error))
						return FALSE;
			}
		}


		subnode = subnode->next;
	}
	doc->intSubset = old_dtd;
	xmlFreeDoc (doc);

	return TRUE;
}


/*
 * function called when an error occurred during the document validation
 */
static void
xml_validity_error_func (void *ctx, const char *msg, ...)
{
        va_list args;
        gchar *str, *str2, *newerr;
	GdaDict *dict;

	dict = GDA_DICT (ctx);
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

		/* DSN information */
		if (dict->priv->dsn) {
			node = xmlNewChild (topnode, NULL, "gda_dsn_info", NULL);
			xmlSetProp (node, "dsn", dict->priv->dsn);
			xmlSetProp (node, "user", dict->priv->user ? dict->priv->user : "");
		}

		/* GdaDictType objects */
		if (retval) {
			GdaDictRegisterStruct *reg;

			reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_TYPE);
			node = xmlNewChild (topnode, NULL, reg->xml_group_tag, NULL);
			if ((reg->save_xml_tree) (dict, node, error))
				xmlAddChild (topnode, node);
			else 
				/* error handling */
				retval = FALSE;
		}

		/* GdaDictFunction objects */
		if (retval) {
			GdaDictRegisterStruct *reg;

			reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
			if (reg) {
				node = xmlNewChild (topnode, NULL, reg->xml_group_tag, NULL);
				if ((reg->save_xml_tree) (dict, node, error))
					xmlAddChild (topnode, node);
				else 
					/* error handling */
					retval = FALSE;
			}
		}
		/* GdaDictAggregate objects */
		if (retval) {
			GdaDictRegisterStruct *reg;

			reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
			if (reg) {
				node = xmlNewChild (topnode, NULL, reg->xml_group_tag, NULL);
				if ((reg->save_xml_tree) (dict, node, error))
					xmlAddChild (topnode, node);
				else 
					/* error handling */
					retval = FALSE;
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
		
		/* generic objects */
		if (retval) {
			GSList *list;
			GdaDictRegisterStruct *reg;

			for (list = dict->priv->registry_list; list && retval; list = list->next) {
				reg = (GdaDictRegisterStruct *) (list->data);
				if ((reg->type == GDA_TYPE_DICT_TYPE) ||
				    (reg->type == GDA_TYPE_DICT_AGGREGATE) ||
				    (reg->type == GDA_TYPE_DICT_FUNCTION))
					continue;
				
				if (!reg->save_xml_tree || !reg->xml_group_tag) {
					if (reg->xml_group_tag)
						g_warning (_("Could not save XML data for %s (no registered save function)"), 
							   reg->xml_group_tag);
				}
				else {
					xmlNodePtr node;
					node = xmlNewChild (topnode, NULL, reg->xml_group_tag, NULL);
					if ((reg->save_xml_tree) (dict, node, error))
						xmlAddChild (topnode, node);
					else
						retval = FALSE;
				}
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
	if (gda_connection_get_dsn (cnc))
		dict->priv->dsn = g_strdup ((gchar *) gda_connection_get_dsn (cnc));
	else
		dict->priv->dsn = NULL;

        cstr = gda_dict_get_xml_filename (dict);
        if (!cstr && dict->priv->dsn) {
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
 * synchronises itself, and when manipulating data (the gda_dict_get_handler() method).
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

	/* FIXME: registered objects are not dumped */

	g_return_if_fail (dict && GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);

	g_print ("\n----------------- DUMPING START -----------------\n");
	g_print (D_COL_H1 "GdaDict %p\n" D_COL_NOR, dict);
	if (dict->priv->cnc) 
		g_print ("Connection: %p\n", dict->priv->cnc);

	if (dict->priv->database)
		gda_object_dump (GDA_OBJECT (dict->priv->database), 0);

	while (list) {
		gda_object_dump (GDA_OBJECT (list->data), 0);
		list = g_slist_next (list);
	}

	g_print ("----------------- DUMPING END -----------------\n\n");
}
#endif

/**
 * gda_dict_update_dbms_data
 * @dict: a #GdaDict object
 * @limit_to_type: limit the DBMS update to this type, or 0 for no limit
 * @limit_obj_name: limit the DBMS update to objects of this name, or %NULL for no limit
 * @error: location to store error, or %NULL
 *
 * Updates the list of data types, functions, tables, etc from the database,
 * which means that the @dict object uses an opened connection to the DBMS.
 * Use gda_dict_set_connection() to set a #GdaConnection to @dict.
 *
 * Returns: TRUE if no error
 */
gboolean
gda_dict_update_dbms_data (GdaDict *dict, GType limit_to_type, const gchar *limit_obj_name, GError **error)
{
	gboolean retval = TRUE;
	GdaDictRegisterStruct *reg;

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
	
	/* types */
	if (!dict->priv->stop_update && 
	    (!limit_to_type || (limit_to_type == GDA_TYPE_DICT_TYPE))) {
		reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_TYPE);
		g_assert (reg);
		retval = (reg->dbms_sync) (dict, limit_obj_name, error);
	}

	/* functions */
	if (!dict->priv->stop_update && 
	    (!limit_to_type || (limit_to_type == GDA_TYPE_DICT_FUNCTION)) &&
	    gda_connection_supports_feature (dict->priv->cnc, GDA_CONNECTION_FEATURE_PROCEDURES)) {
		reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
		if (reg) 
			retval = (reg->dbms_sync) (dict, limit_obj_name, error);
	}

	/* aggregates */
	if (!dict->priv->stop_update && 
	    (!limit_to_type || (limit_to_type == GDA_TYPE_DICT_AGGREGATE)) && 
	    gda_connection_supports_feature (dict->priv->cnc, GDA_CONNECTION_FEATURE_AGGREGATES)) {
		reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
		if (reg) 
			retval = (reg->dbms_sync) (dict, limit_obj_name, error);
	}
	
	/* tables, fields, etc */
	if (!dict->priv->stop_update && 
	    retval &&
	    (!limit_to_type || 
	     (limit_to_type == GDA_TYPE_DICT_TABLE) ||
	     (limit_to_type == GDA_TYPE_DICT_CONSTRAINT)))
		retval = gda_dict_database_update_dbms_data (dict->priv->database, limit_to_type, limit_obj_name, error);

	/* generic registered objects */
	if (!dict->priv->stop_update && retval) {
		GSList *list;
		GdaDictRegisterStruct *reg;

		for (list = dict->priv->registry_list; !dict->priv->stop_update && list && retval; list = list->next) {
			reg = (GdaDictRegisterStruct *) (list->data);
			if ((reg->type == GDA_TYPE_DICT_TYPE) ||
			    (reg->type == GDA_TYPE_DICT_AGGREGATE) ||
			    (reg->type == GDA_TYPE_DICT_FUNCTION))
				continue;
			
			if (reg->dbms_sync) {
				if (!(reg->dbms_sync) (dict, limit_obj_name, error))
					retval = FALSE;
			}
		}
	}

#ifdef GDA_DEBUG_signal
        g_print (">> 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (dict), gda_dict_signals[DATA_UPDATE_FINISHED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif

        dict->priv->update_in_progress = FALSE;
        if (error && !*error && dict->priv->stop_update) {
                g_set_error (error, GDA_DICT_ERROR, 
			     GDA_DICT_META_DATA_UPDATE_USER_STOPPED,
                             _("Update stopped!"));
                return FALSE;
        }

	return retval;
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
	if (!datasource)
		if (dict->priv->cnc)
			datasource = gda_connection_get_dsn (dict->priv->cnc);

	if (!datasource) {
		g_warning ("datasource != NULL failed");
		return NULL;
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
		/* FILE *fp; */
		
		dirpath = g_strdup_printf ("%s%s", g_get_home_dir (), LIBGDA_USER_CONFIG_DIR);
		if (!g_file_test (dirpath, G_FILE_TEST_IS_DIR)){
			if (g_mkdir (dirpath, 0700)) {
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
 * gda_dict_get_handler
 * @dict : a #GdaDict object
 * @for_type: a #GType type
 *
 * Obtain a pointer to a #GdaDataHandler which can convert #GValue values of type @for_type.
 *
 * Unlike the gda_dict_get_default_handler() method, this method asks the provider (for
 * the connection assigned to @dict using gda_dict_set_connection()) if there is any.
 *
 * It fallbacks to the same data handler as
 * gda_dict_get_default_handler() if no connection has been assigned, or if the assigned'd provider
 * offers no data handler for that type.
 *
 * The returned pointer is %NULL if there is no data handler available for the @for_type type.
 *
 * Returns: a #GdaDataHandler
 */
GdaDataHandler *
gda_dict_get_handler (GdaDict *dict, GType for_type)
{
	GdaDataHandler *handler = NULL;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	if (dict->priv->cnc) {
		GdaServerProvider *prov;

		prov = gda_connection_get_provider_obj (dict->priv->cnc);
		if (prov)
			handler = gda_server_provider_get_data_handler_gtype (prov, dict->priv->cnc, for_type);
	}

	if (handler)
		return handler;
	else
		return gda_dict_get_default_handler (dict, for_type);
}

static guint
gtype_hash (gconstpointer key)
{
	return (guint) key;
}

static gboolean 
gtype_equal (gconstpointer a, gconstpointer b)
{
	return (GType) a == (GType) b ? TRUE : FALSE;
}

/**
 * gda_dict_get_default_handler
 * @dict : a #GdaDict object
 * @for_type: a #GType type
 *
 * Obtain a pointer to a #GdaDataHandler which can manage
 * #GValue values of type @for_type
 *
 * The returned pointer is %NULL if there is no default data handler available for the @for_type data type
 *
 * Returns: a #GdaDataHandler
 */
GdaDataHandler* 
gda_dict_get_default_handler (GdaDict *dict, GType for_type)
{
	if (!default_dict_handlers) {
		default_dict_handlers = g_hash_table_new_full (gtype_hash, gtype_equal, 
							       NULL, (GDestroyNotify) g_object_unref);

		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_INT64, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_UINT64, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_BINARY, gda_handler_bin_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_BLOB, gda_handler_bin_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_BOOLEAN, gda_handler_boolean_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_DATE, gda_handler_time_new_no_locale ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_DOUBLE, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_INT, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_NUMERIC, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_FLOAT, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_SHORT, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_USHORT, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_STRING, gda_handler_string_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_TIME, gda_handler_time_new_no_locale ());
		g_hash_table_insert (default_dict_handlers, (gpointer) GDA_TYPE_TIMESTAMP, gda_handler_time_new_no_locale ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_CHAR, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_UCHAR, gda_handler_numerical_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_ULONG, gda_handler_type_new ());
		g_hash_table_insert (default_dict_handlers, (gpointer) G_TYPE_UINT, gda_handler_numerical_new ());		
	}

	return g_hash_table_lookup (default_dict_handlers, (gpointer) for_type);
}


/* 
 * Genertic objects lists 
 */

/**
 * gda_dict_get_object_type_registration
 * @dict: a #GdaDict object
 * @type: e #Gtype
 *
 * Get a pointer to the #GdaDictRegisterStruct structure for the @type type of
 * objects.
 *
 * Returns: the #GdaDictRegisterStruct pointer, or %NULL if @type is not registered
 */
GdaDictRegisterStruct *
gda_dict_get_object_type_registration (GdaDict *dict, GType type)
{
	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	return g_hash_table_lookup (dict->priv->registry, GUINT_TO_POINTER (type));
}

/**
 * gda_dict_register_object_type
 * @dict: a #GdaDict object
 * @reg: a #GdaDictRegisterStruct structure
 *
 * Make @dict manage objects of type @reg->type.
 */
void
gda_dict_register_object_type (GdaDict *dict, GdaDictRegisterStruct *reg)
{
	GdaDictRegisterStruct *preg;

	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (reg);

	preg = gda_dict_get_object_type_registration (dict, reg->type);
	if (preg) {
		if (preg != reg)
			g_warning (_("Cannot register object type %s a second time"), g_type_name (reg->type));
	}

	if ((reg->load_xml_tree || reg->save_xml_tree) && !reg->xml_group_tag) {
		g_warning (_("Cannot register object type %s: no XML group tag specified"), g_type_name (reg->type));
		return;
	}

	g_hash_table_insert (dict->priv->registry, GUINT_TO_POINTER (reg->type), reg);
	if (reg->xml_group_tag)
		g_hash_table_insert (dict->priv->registry_xml_groups, (gchar *) reg->xml_group_tag, reg);
	
	dict->priv->registry_list = g_slist_append (dict->priv->registry_list, reg);

	/* g_print ("GdaDict %p registered %p for %s (%u)\n", dict, reg, g_type_name (reg->type), (guint ) reg->type); */
}

static void
reg_object_weak_ref_notify (GdaDict *dict, GdaObject *object)
{
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, G_OBJECT_TYPE (object));
	if (!reg) {
		GType as_type;

		as_type = GPOINTER_TO_UINT (g_hash_table_lookup (dict->priv->objects_as_hash, object));
		if (as_type)
			reg = gda_dict_get_object_type_registration (dict, as_type);
	}
	g_assert (reg);
	reg->all_objects = g_slist_remove (reg->all_objects, object);

	g_hash_table_remove (dict->priv->objects_as_hash, object);
}

/**
 * gda_dict_declare_object
 * @dict: a #GdaDict object
 * @object: a #GdaObject object
 *
 * Declares the existence of a new object to @dict: @dict knows about @object but does not
 * hold any reference to it. If @dict must hold such a reference, then use gda_dict_assume_object().
 */
void
gda_dict_declare_object (GdaDict *dict, GdaObject *object)
{
	g_return_if_fail (G_IS_OBJECT (object));
	gda_dict_declare_object_as (dict, object, G_OBJECT_TYPE (object));
}

/**
 * gda_dict_declare_object_as
 * @dict: a #GdaDict object
 * @object: a #GdaObject object
 * @as_type: type parent type of @object to take into account
 *
 * Same as gda_dict_declare_object() but forces to use the @as_type type instead of
 * @object's realtype
 */
void
gda_dict_declare_object_as (GdaDict *dict, GdaObject *object, GType as_type)
{
	GdaDictRegisterStruct *reg;

	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (GDA_IS_OBJECT (object));
	
	reg = gda_dict_get_object_type_registration (dict, as_type);
	if (!reg) {
		g_warning (_("Trying to declare an object when object class %s is not registered in the dictionary"),
			   g_type_name (as_type));
		return;
	}
	
	if (g_slist_find (reg->all_objects, object))
		return;	
	
	reg->all_objects = g_slist_prepend (reg->all_objects, object);
	g_object_weak_ref (G_OBJECT (object), (GWeakNotify) reg_object_weak_ref_notify, dict);
	if (G_OBJECT_TYPE (object) != as_type)
		g_hash_table_insert (dict->priv->objects_as_hash, object, GUINT_TO_POINTER (as_type));
}

static void 
destroyed_object_cb (GdaObject *object, GdaDict *dict)
{
	gda_dict_unassume_object (dict, object);
}

static void
updated_object_cb (GdaObject *object, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'OBJECT_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (dict), "object_updated", object);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'OBJECT_UPDATED' from %s\n", __FUNCTION__);
#endif	
}

static void
activated_object_cb (GdaReferer *object, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'OBJECT_ACT_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (dict), "object_act_changed", object);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'OBJECT_ACT_CHANGED' from %s\n", __FUNCTION__);
#endif	
}

static void
deactivated_object_cb (GdaReferer *object, GdaDict *dict)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'OBJECT_ACT_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (dict), "object_act_changed", object);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'OBJECT_ACT_CHANGED' from %s\n", __FUNCTION__);
#endif	
}

/**
 * gda_dict_assume_object
 * @dict: a #GdaDict object
 * @object: a #GdaObject object
 *
 * Declares the existence of a new object to @dict, and force @dict to hold a reference to @object so it is not
 * destroyed.
 */
void
gda_dict_assume_object (GdaDict *dict, GdaObject *object)
{
	g_return_if_fail (G_IS_OBJECT (object));

	gda_dict_assume_object_as (dict, object, G_OBJECT_TYPE (object));
}

/**
 * gda_dict_assume_object_as
 * @dict: a #GdaDict object
 * @object: a #GdaObject object
 * @as_type: parent type of @object to take into account
 *
 * Same as gda_dict_assume_object() but forces to use the @as_type type instead of
 * @object's realtype
 */
void
gda_dict_assume_object_as (GdaDict *dict, GdaObject *object, GType as_type)
{
	GdaDictRegisterStruct *reg;

	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (GDA_IS_OBJECT (object));

	gda_dict_declare_object_as (dict, object, as_type);
	reg = gda_dict_get_object_type_registration (dict, as_type);
	if (!reg) {
		g_warning (_("Trying to make an object assumed when object class %s is not registered in the dictionary"),
			   g_type_name (as_type));
		return;
	}

	if (g_slist_find (reg->assumed_objects, object)) {
		g_warning ("GdaObject of type %s (%p) already assumed!", G_OBJECT_TYPE_NAME (object), object);
		return;	
	}
	else {
		gint i = -1;
		GdaDictRegisterStruct *reg;

		reg = gda_dict_get_object_type_registration (dict, as_type);
		if (!reg) {
			g_warning (_("Trying to assume an object when object class is not registered in the dictionary"));
			return;
		}
		if (reg->sort) {
			GSList *slist = reg->assumed_objects;
			gboolean found = FALSE;
			const gchar *oname;
			i = 0;
			oname = gda_object_get_name (object);
			while (slist && !found) {
				if (strcmp (oname, gda_object_get_name (slist->data)) <= 0)
					found = TRUE;
				else
					i++;
				slist = slist->next;
			}
		}

		g_object_ref (G_OBJECT (object));

		gda_object_connect_destroy (object, G_CALLBACK (destroyed_object_cb), dict);
		g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (updated_object_cb), dict);
		if (GDA_IS_REFERER (object)) {
			g_signal_connect (G_OBJECT (object), "activated", G_CALLBACK (activated_object_cb), dict);
			g_signal_connect (G_OBJECT (object), "deactivated", G_CALLBACK (deactivated_object_cb), dict);
		}
		
		reg->assumed_objects = g_slist_insert (reg->assumed_objects, object, i);
		if (G_OBJECT_TYPE (object) != as_type)
			g_hash_table_insert (dict->priv->objects_as_hash, object, GUINT_TO_POINTER (as_type));
		
#ifdef GDA_DEBUG_signal
		g_print (">> 'OBJECT_ADDED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (dict), gda_dict_signals[OBJECT_ADDED], 0, object);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'OBJECT_ADDED' from %s\n", __FUNCTION__);
#endif
	}
}

/**
 * gda_dict_unassume_object
 * @dict: a #GdaDict object
 * @object: a #GdaObject object
 *
 * Makes @dict release its reference on @object.
 */
void
gda_dict_unassume_object (GdaDict *dict, GdaObject *object)
{
	GdaDictRegisterStruct *reg;

	g_return_if_fail (GDA_IS_DICT (dict));
	g_return_if_fail (dict->priv);
	g_return_if_fail (GDA_IS_OBJECT (object));

	reg = gda_dict_get_object_type_registration (dict, G_OBJECT_TYPE (object));
	if (!reg) {
		GType as_type;

		as_type = GPOINTER_TO_UINT (g_hash_table_lookup (dict->priv->objects_as_hash, object));
		if (as_type)
			reg = gda_dict_get_object_type_registration (dict, as_type);
		if (!reg) {
			g_warning (_("Trying to make an object unassumed when object class %s is not registered in the dictionary"),
				   G_OBJECT_TYPE_NAME (object));
			return;
		}
	}

	if (g_slist_find (reg->assumed_objects, object)) {
		reg->assumed_objects = g_slist_remove (reg->assumed_objects, object);

		g_signal_handlers_disconnect_by_func (G_OBJECT (object),
						      G_CALLBACK (destroyed_object_cb), dict);
		g_signal_handlers_disconnect_by_func (G_OBJECT (object),
						      G_CALLBACK (updated_object_cb), dict);
		if (GDA_IS_REFERER (object)) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (object), G_CALLBACK (activated_object_cb), dict);
			g_signal_handlers_disconnect_by_func (G_OBJECT (object), G_CALLBACK (deactivated_object_cb), dict);
		}

#ifdef GDA_DEBUG_signal
		g_print (">> 'OBJECT_REMOVED' from %s\n", __FUNTCION__);
#endif
		g_signal_emit (G_OBJECT (dict), gda_dict_signals[OBJECT_REMOVED], 0, object);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'OBJECT_REMOVED' from %s\n", __FUNTCION__);
#endif
		g_object_unref (G_OBJECT (object));
	}
}

/**
 * gda_dict_object_is_assumed
 * @dict: a #GdaDict object
 * @object: a #GdaObject object
 *
 * Tests if @object is assumed by @dict
 *
 * Returns: TRUE if @object is assumed by @dict
 */
gboolean
gda_dict_object_is_assumed (GdaDict *dict, GdaObject *object)
{
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), FALSE);
	g_return_val_if_fail (dict->priv, FALSE);
	g_return_val_if_fail (GDA_IS_OBJECT (object), FALSE);

	reg = gda_dict_get_object_type_registration (dict, G_OBJECT_TYPE (object));
	if (!reg) {
		GType as_type;

		as_type = GPOINTER_TO_UINT (g_hash_table_lookup (dict->priv->objects_as_hash, object));
		if (as_type)
			reg = gda_dict_get_object_type_registration (dict, as_type);
		if (reg) {
			g_warning (_("Trying to make if an object is assumed when object class %s is not registered in the dictionary"),
				   G_OBJECT_TYPE_NAME (object));
			return FALSE;
		}
	}

	if (g_slist_find (reg->assumed_objects, object))
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_dict_get_objects
 * @dict: a #GdaDict object
 * @type: a #Gtype type of object
 *
 * Returns: a new list of all the objects of type @type managed by @dict.
 */
GSList *
gda_dict_get_objects (GdaDict *dict, GType type)
{
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	reg = gda_dict_get_object_type_registration (dict, type);
	if (!reg) {
		g_warning (_("Trying to get a list of objects when object class %s is not registered in the dictionary"),
			   g_type_name (type));
		return NULL;
	}
	if (reg->get_objects)
		return (reg->get_objects) (dict);
	else {
		if (reg->assumed_objects)
			return g_slist_copy (reg->assumed_objects);
		else
			return NULL;
	}
}

/**
 * gda_dict_get_object_by_name
 * @dict: a #GdaDict object
 * @type: a #Gtype type of object
 * @name: 
 *
 * Tries to find an object from its name, among the objects managed by @dict of type @type.
 *
 * Returns: a pointer to the requested object, or %NULL if the object was not found
 */
GdaObject *
gda_dict_get_object_by_name (GdaDict *dict, GType type, const gchar *name)
{
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);

	reg = gda_dict_get_object_type_registration (dict, type);
	if (!reg) {
		g_warning (_("Trying to get an object by name when object class %s is not registered in the dictionary"),
			   g_type_name (type));
		return NULL;
	}
	if (reg->get_by_name)
		return (reg->get_by_name) (dict, name);
	else {
		GSList *list;
		GdaObject *obj = NULL;

		list = reg->assumed_objects;
		for (; list && !obj; list = list->next) {
			if (!strcmp (gda_object_get_name (list->data), name))
				obj = (GdaObject*) list->data;
		}
		return obj;
	}
}

/**
 * gda_dict_get_object_by_xml_id
 * @dict: a #GdaDict object
 * @type: a #Gtype type of object
 * @xml_id:
 *
 * For the objects which implement the #GdaXmlStorage interface, this function allows to find an object from
 * its XML Id. The object is looked from the managed objects of type @type, and also from the list
 * of all declared objects.
 *
 * Returns: a pointer to the requested object, or %NULL if the object was not found
 */
GdaObject *
gda_dict_get_object_by_xml_id (GdaDict *dict, GType type, const gchar *xml_id)
{
	GdaObject *object = NULL;
	GSList *list;
        gchar *str;
	GdaDictRegisterStruct *reg;

        g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
        g_return_val_if_fail (dict->priv, NULL);

	reg = gda_dict_get_object_type_registration (dict, type);
	if (!reg) {
		g_warning (_("Trying to get an object by its XML Id when object class %s is not registered in the dictionary"),
			   g_type_name (type));
		return NULL;
	}

        list = reg->all_objects;
        while (list && !object) {
		if (! GDA_IS_XML_STORAGE (list->data)) {
			g_warning (_("Trying to get an object from its XML Id when object class does not implement GdaXmlStorage"));
			return NULL;
		}
                str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
                if (!strcmp (str, xml_id))
                        object = GDA_OBJECT (list->data);
                g_free (str);
                list = g_slist_next (list);
        }

        return object;
}

/**
 * gda_dict_class_always_register
 * @func: a #GdaDictRegFunc function
 *
 * Make sure all new GdaDict object will use @func to register
 * a type of object
 */
void
gda_dict_class_always_register (GdaDictRegFunc func)
{
	GdaDictClass *class;

	g_return_if_fail (func);

	class = GDA_DICT_CLASS (g_type_class_ref (GDA_TYPE_DICT));
	g_assert (class);
	if (! g_slist_find (class->class_registry_list, func))
		class->class_registry_list = g_slist_append (class->class_registry_list, func);
}
