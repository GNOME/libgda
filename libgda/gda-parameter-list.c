/* gda-parameter-list.c
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
#include <locale.h>
#include "gda-parameter-list.h"
#include "gda-query.h"
#include "gda-marshal.h"
#include "gda-data-model.h"
#include "gda-parameter.h"
#include "gda-entity-field.h"
#include "gda-referer.h"
#include "gda-entity.h"
#include "gda-entity-field.h"
#include "gda-dict-type.h"
#include "gda-query-field.h"
#include "gda-connection.h"
#include "gda-server-provider.h"
#include "gda-util.h"

/* 
 * Main static functions 
 */
static void gda_parameter_list_class_init (GdaParameterListClass * class);
static void gda_parameter_list_init (GdaParameterList * srv);
static void gda_parameter_list_dispose (GObject   * object);
static void gda_parameter_list_finalize (GObject   * object);

static void paramlist_remove_node (GdaParameterList *paramlist, GdaParameterListNode *node);
static void paramlist_remove_source (GdaParameterList *paramlist, GdaParameterListSource *source);

static void destroyed_param_cb (GdaParameter *param, GdaParameterList *dataset);
static void changed_param_cb (GdaParameter *param, GdaParameterList *dataset);
static void restrict_changed_param_cb (GdaParameter *param, GdaParameterList *dataset);

#ifdef GDA_DEBUG
static void gda_parameter_list_dump                     (GdaParameterList *paramlist, guint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	PARAM_CHANGED,
	PUBLIC_DATA_CHANGED,
	LAST_SIGNAL
};

static gint gda_parameter_list_signals[LAST_SIGNAL] = { 0, 0 };


/* private structure */
struct _GdaParameterListPrivate
{
	GHashTable *param_default_values;      /* key = param, value = new GdaValue */
	GHashTable *param_default_aliases;     /* key = param, value = alias parameter */
	GHashTable *aliases_default_param;     /* key = alias parameter, value = param */

	GHashTable *param_repl;                /* key = parameter, value = parameter used instead because of similarity */
};



/* module error */
GQuark gda_parameter_list_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_parameter_list_error");
	return quark;
}


GType
gda_parameter_list_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaParameterListClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_parameter_list_class_init,
			NULL,
			NULL,
			sizeof (GdaParameterList),
			0,
			(GInstanceInitFunc) gda_parameter_list_init
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaParameterList", &info, 0);
	}
	return type;
}


static void
gda_parameter_list_class_init (GdaParameterListClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_parameter_list_signals[PARAM_CHANGED] =
		g_signal_new ("param_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaParameterListClass, param_changed),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			      G_TYPE_OBJECT);
	gda_parameter_list_signals[PUBLIC_DATA_CHANGED] =
		g_signal_new ("public_data_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaParameterListClass, public_data_changed),
			      NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	class->param_changed = NULL;
	class->public_data_changed = NULL;

	object_class->dispose = gda_parameter_list_dispose;
	object_class->finalize = gda_parameter_list_finalize;

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_parameter_list_dump;
#endif

	/* class attributes */
	((GdaObjectClass *)class)->id_unique_enforced = FALSE;
}

static void
gda_parameter_list_init (GdaParameterList *paramlist)
{
	paramlist->priv = g_new0 (GdaParameterListPrivate, 1);
	paramlist->parameters = NULL;
	paramlist->nodes_list = NULL;
	paramlist->sources_list = NULL;
	paramlist->groups_list = NULL;

	paramlist->priv->param_default_values = g_hash_table_new_full (NULL, NULL, 
									       NULL, (GDestroyNotify) gda_value_free);
	paramlist->priv->param_default_aliases = g_hash_table_new (NULL, NULL);
	paramlist->priv->aliases_default_param = g_hash_table_new (NULL, NULL);

	paramlist->priv->param_repl = g_hash_table_new (NULL, NULL);
}

static void compute_public_data (GdaParameterList *paramlist);
static void gda_parameter_list_real_add_param (GdaParameterList *paramlist, GdaParameter *param);

/**
 * gda_parameter_list_new
 * @params: a list of #GdaParameter objects
 *
 * Creates a new #GdaParameterList object, and populates it with the list given as argument.
 * The list can then be freed as it gets copied. All the parameters in @params are referenced counted
 * and modified, so they should not be used anymore afterwards, and the @params list gets copied
 * (so it should be freed by the caller).
 *
 * Returns: a new #GdaParameterList object
 */
GdaParameterList *
gda_parameter_list_new (GSList *params)
{
	GObject *obj;
	GdaParameterList *paramlist;
	GSList *list;
	GdaDict *dict = NULL;

	/* get a valid GdaDict object */
	list = params;
	while (list) {
		if (!dict)
			dict = gda_object_get_dict (GDA_OBJECT (list->data));
		else
			if (dict != gda_object_get_dict (GDA_OBJECT (list->data)))
				g_warning ("Several parameters don't relate to the same GdaDict object");

		list = g_slist_next (list);
	}

	obj = g_object_new (GDA_TYPE_PARAMETER_LIST, "dict", dict, NULL);
        paramlist = GDA_PARAMETER_LIST (obj);

	/* add the parameters */
	while (params) {
		gda_parameter_list_real_add_param (paramlist, GDA_PARAMETER (params->data));
		params = g_slist_next (params);
	}
	compute_public_data (paramlist);

	return paramlist;
}

/**
 * gda_parameter_list_new_from_spec
 * @dict: a #GdaDict object, or %NULL
 * @xml_spec: a string
 * @error: a place to store the error, or %NULL
 *
 * Creates a new #GdaParameterList object from the @xml_spec
 * specifications
 *
 * Returns: a new object, or %NULL if an error occured
 */
GdaParameterList *
gda_parameter_list_new_from_spec (GdaDict *dict, const gchar *xml_spec, GError **error)
{
	GdaParameterList *plist = NULL;
	GSList *params = NULL, *sources = NULL;
	GSList *list;
	const gchar *lang;

	xmlDocPtr doc;
	xmlNodePtr root, cur;
	gboolean allok = TRUE;
	gchar *str;

	GdaServerProvider *prov = NULL;
        GdaConnection *cnc = NULL;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);

#ifdef HAVE_LC_MESSAGES
	lang = setlocale (LC_MESSAGES, NULL);
#else
	lang = setlocale (LC_CTYPE, NULL);
#endif

	if (dict)
		cnc = gda_dict_get_connection (dict);
        if (cnc)
                prov = gda_connection_get_provider_obj (cnc);

	/* string parsing */
	doc = xmlParseMemory (xml_spec, strlen (xml_spec));
	if (!doc)
		return NULL;
	root = xmlDocGetRootElement (doc);
	if (strcmp (root->name, "data-set-spec") != 0){
		g_set_error (error, GDA_PARAMETER_LIST_ERROR, GDA_PARAMETER_LIST_XML_SPEC_ERROR,
			     _("Spec's root node != 'data-set-spec': '%s'"), root->name);
		return NULL;
	}

	/* creating parameters */
	root = root->xmlChildrenNode;
	while (xmlNodeIsText (root)) 
		root = root->next; 

	if (strcmp (root->name, "parameters") != 0){
		g_set_error (error, GDA_PARAMETER_LIST_ERROR, GDA_PARAMETER_LIST_XML_SPEC_ERROR,
			     _("Missing node <parameters>: '%s'"), root->name);
		return NULL;
	}

	for (cur = root->xmlChildrenNode; cur && allok; cur = cur->next) {
		if (xmlNodeIsText (cur)) 
			continue;

		if (!strcmp (cur->name, "parameter")) {
			GdaParameter *param = NULL;
			GdaDictType *dtype = NULL;
			gchar *str, *id;
			gboolean dtype_created = FALSE;
			xmlChar *this_lang;

			/* don't care about entries for the wrong locale */
			this_lang = xmlGetProp (cur, "lang");
			if (this_lang && strncmp (this_lang, lang, strlen (this_lang))) {
				g_free (this_lang);
				continue;
			}

			/* find if there is already a param with the same ID */
			id = xmlGetProp (cur, "id");
			list = params;
			while (list && !param) {
				g_object_get (G_OBJECT (list->data), "string_id", &str, NULL);
				if (str && id && !strcmp (str, id))
					param = (GdaParameter *) list->data;
				g_free (str);
				list = g_slist_next (list);
			}

			if (param && !this_lang) {
				g_free (id);
				g_free (this_lang);
				continue;
			}
			g_free (this_lang);
			

			/* find data type and create GdaParameter */
			str = xmlGetProp(cur, "dbmstype");
			if (str) {
				dtype = gda_dict_get_data_type_by_name (ASSERT_DICT (dict), str);
				g_free (str);
			}
			if (!dtype) {
				str = xmlGetProp (cur, "gdatype");
				if (str) {
					GdaValueType gtype;

					gtype = gda_type_from_string (str);
					if (prov) {
						const gchar *deftype;

						deftype = gda_server_provider_get_default_dbms_type (prov, cnc, 
												     gtype);
						if (deftype)
							dtype = gda_dict_get_data_type_by_name (ASSERT_DICT (dict), 
												deftype);
					}	

					if (!dtype) {
						/* create a GdaDictType for that 'gda-type' */
						dtype = GDA_DICT_TYPE (gda_dict_type_new (ASSERT_DICT (dict)));
						gda_dict_type_set_sqlname (dtype, str);
						gda_dict_type_set_gda_type (dtype, gtype);
						gda_dict_declare_custom_data_type (ASSERT_DICT (dict), dtype);
						dtype_created = TRUE;
					}
					g_free (str);					
				}
			}

			if (!dtype) {
				str = xmlGetProp(cur, "name");
				
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, GDA_PARAMETER_LIST_XML_SPEC_ERROR,
					     _("Can't find a data type for parameter '%s'"), str ? str : "unnamed");
				g_free (str);

				allok = FALSE;
				continue;
			}

			if (!param) {
				param = GDA_PARAMETER (g_object_new (GDA_TYPE_PARAMETER, "dict", dict,
								     "gda_type", gda_dict_type_get_gda_type (dtype),
								     NULL));
				params = g_slist_append (params, param);
			}
			if (dtype_created)
				g_object_unref (dtype);
			
			/* set parameter's attributes */
			if (id) {
				g_object_set (G_OBJECT (param), "string_id", id, NULL);
				g_free (id);
			}

			str = xmlGetProp(cur, "name");
			if (str) {
				gda_object_set_name (GDA_OBJECT (param), str);
				g_free (str);
			}
			str = xmlGetProp(cur, "descr");
			if (str) {
				gda_object_set_description (GDA_OBJECT (param), str);
				g_free (str);
			}
			str = xmlGetProp(cur, "null-ok");
			if (str) {
				gda_parameter_set_not_null (param, (*str == 'T') || (*str == 't') ? FALSE : TRUE);
				g_free (str);
			}
			else
				gda_parameter_set_not_null (param, FALSE);
			str = xmlGetProp(cur, "plugin");
			if (str) {
				g_object_set (G_OBJECT (param), "handler_plugin", str, NULL);
				g_free (str);
			}

			str = xmlGetProp(cur, "source");
			if (str) 
				g_object_set_data (G_OBJECT (param), "source", str);
		}
	}

	/* Parameters' sources, not mandatory */
	root = root->next;
	while (xmlNodeIsText (root)) 
		root = root->next; 
	if (allok && root && !strcmp (root->name, "sources")){
		for (cur = root->xmlChildrenNode; (cur != NULL) && allok; cur = cur->next) {
			if (xmlNodeIsText (cur)) 
				continue;

			if (!strcmp (cur->name, "gda_array")) {
				GdaDataModel *model;

				str = xmlGetProp(cur, "name");
				model = gda_data_model_array_new_from_xml_node (cur, error);
				if (model) {
					sources = g_slist_prepend (sources, model);
					if (str)
						gda_object_set_name (GDA_OBJECT (model), str);
				}
				else 
					allok = FALSE; /* error has already been set */
				g_free (str);
			}
		}
	}
	
	/* affecting parameters' sources fields to each parameter */
	list = params;
	while (list && allok) {
		str = g_object_get_data (G_OBJECT (list->data), "source");
		if (str) {
			gchar *ptr1, *ptr2=NULL, *tok;
			
			ptr1 = strtok_r (str, ":", &tok);
			if (ptr1)
				ptr2 = strtok_r (NULL, ":", &tok);
			
			if (ptr1 && ptr2) {
				GSList *tmp = sources;
				GdaDataModel *model = NULL;
				while (tmp && !model) {
					if (!strcmp (gda_object_get_name (GDA_OBJECT (tmp->data)), ptr1))
						model = GDA_DATA_MODEL (tmp->data);
					tmp = g_slist_next (tmp);
				}

				if (model) {
					gint fno;

					fno = atoi (ptr2);
					if ((fno < 0) ||
					    (fno >= gda_data_model_get_n_columns (model))) {
						g_set_error (error, GDA_PARAMETER_LIST_ERROR, GDA_PARAMETER_LIST_XML_SPEC_ERROR,
							     _("Field number %d not found in source named '%s'"), fno, ptr1); 
						allok = FALSE;	
					}
					else {
						if (!gda_parameter_restrict_values (GDA_PARAMETER (list->data),
										    model, fno, NULL))
							allok = FALSE;
						else {
							/* rename the wrapper with the current param's name */
							g_object_set_data (G_OBJECT (model), "newname", 
									   (gchar *)gda_object_get_name (GDA_OBJECT (list->data)));
							g_object_set_data (G_OBJECT (model), "newdescr", 
									   (gchar *)gda_object_get_description (GDA_OBJECT (list->data)));
						}
					}
				}
				else {
					g_set_error (error, GDA_PARAMETER_LIST_ERROR, GDA_PARAMETER_LIST_XML_SPEC_ERROR,
						     _("Can't find parameter source named '%s'"), ptr1); 
					allok = FALSE;
				}
			}
			else {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, GDA_PARAMETER_LIST_XML_SPEC_ERROR,
					     _("Parameter source specification should be \"<source name>:<field pos>\": '%s'"), 
					     str);
				allok = FALSE;
			}

			g_object_set_data (G_OBJECT (list->data), "source", NULL);
			g_free (str);
		}
		list = g_slist_next (list);
	}

	/* setting prepared new names from sources (models) */
	list = sources;
	while (list) {
		str = g_object_get_data (G_OBJECT (list->data), "newname");
		if (str) {
			gda_object_set_name (GDA_OBJECT (list->data), str);
			g_object_set_data (G_OBJECT (list->data), "newname", NULL);
		}
		str = g_object_get_data (G_OBJECT (list->data), "newdescr");
		if (str) {
			gda_object_set_description (GDA_OBJECT (list->data), str);
			g_object_set_data (G_OBJECT (list->data), "newdescr", NULL);
		}
		list = g_slist_next (list);
	}

	/* parameters' values, constraints: TODO */
	
	/* GdaParameterList creation */
	if (allok)
		plist = gda_parameter_list_new (params);

	g_slist_foreach (params, (GFunc) g_object_unref, NULL);
	g_slist_free (params);
	g_slist_foreach (sources, (GFunc) g_object_unref, NULL);
	g_slist_free (sources);

	xmlFreeDoc(doc);

	return plist;
}

/**
 * gda_parameter_list_get_length
 * @paramlist: a #GdaParameterList object
 *
 * Get the number of #GdaParameter objects in @paramlist
 *
 * Returns:
 */
guint
gda_parameter_list_get_length (GdaParameterList *paramlist)
{
	g_return_val_if_fail (GDA_IS_PARAMETER_LIST (paramlist), 0);
	
	return g_slist_length (paramlist->parameters);
}

/**
 * gda_parameter_list_get_spec
 * @paramlist: a #GdaParameterList object
 *
 * Get the specification as an XML string. See the gda_parameter_list_new_from_spec()
 * form more information about the XML specification string format.
 *
 * Returns: a new string
 */
gchar *
gda_parameter_list_get_spec (GdaParameterList *paramlist)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *xmlbuff;
	int buffersize;
	GSList *list;

	g_return_val_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist), NULL);

	doc = xmlNewDoc ("1.0");
	g_return_val_if_fail (doc, NULL);
	root = xmlNewDocNode (doc, NULL, "data-set-spec", NULL);
	xmlDocSetRootElement (doc, root);

	/* parameters list */
	list = paramlist->parameters;
	while (list) {
		xmlNodePtr node;
		GdaParameter *param = GDA_PARAMETER (list->data);
		GdaDictType *dtype;
		gchar *str;
		const gchar *cstr;

		node = xmlNewTextChild (root, NULL, "parameter", NULL);
		g_object_get (G_OBJECT (param), "string_id", &str, NULL);
		if (str) {
			xmlSetProp (node, "id", str);
			g_free (str);
		}

		cstr = gda_object_get_name (GDA_OBJECT (param));
		if (cstr)
			xmlSetProp (node, "name", cstr);		
		cstr = gda_object_get_description (GDA_OBJECT (param));
		if (cstr)
			xmlSetProp (node, "descr", cstr);		

		/* dtype = gda_parameter_get_gda_type (param); */
/* 		if (dtype) { */
/* 			xmlSetProp (node, "dbmstype", gda_dict_type_get_sqlname (dtype)); */
/* 			xmlSetProp (node, "gdatype", gda_type_to_string (gda_dict_type_get_gda_type (dtype))); */
/* 		} */
		xmlSetProp (node, "gdatype", gda_type_to_string (gda_parameter_get_gda_type (param)));

		xmlSetProp (node, "null-ok", gda_parameter_get_not_null (param) ? "FALSE" : "TRUE");
		g_object_get (G_OBJECT (param), "handler_plugin", &str, NULL);
		if (str) {
			xmlSetProp (node, "plugin", str);
			g_free (str);
		}
		
		list = g_slist_next (list);
	}

	/* parameters' values, sources, constraints: TODO */

	xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);
	g_print ((gchar *) xmlbuff);
	
	xmlFreeDoc(doc);
	return (gchar *) xmlbuff;
}


static void
destroyed_param_cb (GdaParameter *param, GdaParameterList *paramlist)
{
	GdaParameterListNode *node;

	g_assert (g_slist_find (paramlist->parameters, param));
	g_signal_handlers_disconnect_by_func (G_OBJECT (param),
					      G_CALLBACK (destroyed_param_cb), paramlist);
	g_signal_handlers_disconnect_by_func (G_OBJECT (param),
					      G_CALLBACK (changed_param_cb), paramlist);
	g_signal_handlers_disconnect_by_func (G_OBJECT (param),
					      G_CALLBACK (restrict_changed_param_cb), paramlist);

	/* now destroy the GdaParameterListNode and the GdaParameterListSource if necessary */
	node = gda_parameter_list_find_node_for_param (paramlist, param);
	g_assert (node);
	if (node->source_model) {
		GdaParameterListSource *source;

		source = gda_parameter_list_find_source (paramlist, node->source_model);
		g_assert (source);
		g_assert (source->nodes);
		if (! source->nodes->next)
			paramlist_remove_source (paramlist, source);
	}
	paramlist_remove_node (paramlist, node);

	paramlist->parameters = g_slist_remove (paramlist->parameters, param);
	g_object_unref (G_OBJECT (param));
}

static void
restrict_changed_param_cb (GdaParameter *param, GdaParameterList *paramlist)
{
	compute_public_data (paramlist);
}

static void
changed_param_cb (GdaParameter *param, GdaParameterList *paramlist)
{
	/* signal the parameter change */
	gda_object_changed (GDA_OBJECT (paramlist));
#ifdef GDA_DEBUG_signal
	g_print (">> 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (paramlist), gda_parameter_list_signals[PARAM_CHANGED], 0, param);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'PARAM_CHANGED' from %s\n", __FUNCTION__);
#endif
}

static void
group_free (GdaParameterListGroup *group, gpointer data)
{
	g_slist_free (group->nodes);
	g_free (group);
}

static void
gda_parameter_list_dispose (GObject *object)
{
	GdaParameterList *paramlist;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_PARAMETER_LIST (object));

	paramlist = GDA_PARAMETER_LIST (object);
	if (paramlist->priv)
		gda_object_destroy_check (GDA_OBJECT (object));

	/* free the parameters list */
	while (paramlist->parameters)
		destroyed_param_cb (GDA_PARAMETER (paramlist->parameters->data), paramlist);	

	/* free the nodes if there are some */
	while (paramlist->nodes_list)
		paramlist_remove_node (paramlist, 
				       GDA_PARAMETER_LIST_NODE (paramlist->nodes_list->data));
	while (paramlist->sources_list)
		paramlist_remove_source (paramlist, 
					 GDA_PARAMETER_LIST_SOURCE (paramlist->sources_list->data));

	g_slist_foreach (paramlist->groups_list, (GFunc) group_free, NULL);
	g_slist_free (paramlist->groups_list);
	paramlist->groups_list = NULL;

	/* parent class */
	parent_class->dispose (object);
}

static void foreach_finalize_alias (GdaParameter *param, GdaParameter *alias, GdaParameterList *paramlist);
static void
gda_parameter_list_finalize (GObject *object)
{
	GdaParameterList *paramlist;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_PARAMETER_LIST (object));

	paramlist = GDA_PARAMETER_LIST (object);
	if (paramlist->priv) {
		g_hash_table_destroy (paramlist->priv->param_default_values);
		g_hash_table_foreach (paramlist->priv->param_default_aliases,
				      (GHFunc) foreach_finalize_alias, paramlist);
		g_hash_table_destroy (paramlist->priv->param_default_aliases);
		g_hash_table_destroy (paramlist->priv->aliases_default_param);
		g_hash_table_destroy (paramlist->priv->param_repl);

		g_free (paramlist->priv);
		paramlist->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void compute_shown_columns_index (GdaParameterListSource *source);
static void compute_ref_columns_index (GdaParameterListSource *source);

/*
 * Resets and computes paramlist->nodes, and if some nodes already exist, they are previously discarded
 */
static void 
compute_public_data (GdaParameterList *paramlist)
{
	GSList *list;
	GdaParameterListNode *node;
	GdaParameterListSource *source;
	GdaParameterListGroup *group;
	GHashTable *groups = g_hash_table_new (NULL, NULL); /* key = source model, 
							       value = GdaParameterListGroup */

	/*
	 * Get rid of all the previous structures
	 */
	while (paramlist->nodes_list)
		paramlist_remove_node (paramlist, GDA_PARAMETER_LIST_NODE (paramlist->nodes_list->data));
	while (paramlist->sources_list)
		paramlist_remove_source (paramlist, GDA_PARAMETER_LIST_SOURCE (paramlist->sources_list->data));

	g_slist_foreach (paramlist->groups_list, (GFunc) group_free, NULL);
	g_slist_free (paramlist->groups_list);
	paramlist->groups_list = NULL;

	/*
	 * Creation of the GdaParameterListNode structures
	 */
	list = paramlist->parameters;
	while (list) {
		node = g_new0 (GdaParameterListNode, 1);
		node->param = GDA_PARAMETER (list->data);
		gda_parameter_has_restrict_values (GDA_PARAMETER (list->data),
						   &(node->source_model), &(node->source_column));
		if (node->source_model)
			g_object_ref (node->source_model);
		
		paramlist->nodes_list = g_slist_append (paramlist->nodes_list, node);
		list = g_slist_next (list);
	}

	/*
	 * Creation of the GdaParameterListSource and GdaParameterListGroup structures 
	 */
	list = paramlist->nodes_list;
	while (list) {
		node = GDA_PARAMETER_LIST_NODE (list->data);
		
		/* source */
		source = NULL;
		if (node->source_model) {
			source = gda_parameter_list_find_source (paramlist, node->source_model);
			if (source) 
				source->nodes = g_slist_prepend (source->nodes, node);
			else {
				source = g_new0 (GdaParameterListSource, 1);
				source->data_model = node->source_model;
				source->nodes = g_slist_prepend (NULL, node);
				paramlist->sources_list = g_slist_prepend (paramlist->sources_list, source);
			}
		}

		/* group */
		group = NULL;
		if (node->source_model)
			group = g_hash_table_lookup (groups, node->source_model);
		if (group) 
			group->nodes = g_slist_append (group->nodes, node);
		else {
			group = g_new0 (GdaParameterListGroup, 1);
			group->nodes = g_slist_append (NULL, node);
			group->nodes_source = source;
			paramlist->groups_list = g_slist_append (paramlist->groups_list, group);
			if (node->source_model)
				g_hash_table_insert (groups, node->source_model, group);
		}		

		list = g_slist_next (list);
	}
	
	/*
	 * Filling information in all the GdaParameterListSource structures
	 */
	list = paramlist->sources_list;
	while (list) {
		compute_shown_columns_index (GDA_PARAMETER_LIST_SOURCE (list->data));
		compute_ref_columns_index (GDA_PARAMETER_LIST_SOURCE (list->data));
		list = g_slist_next (list);
	}

	g_hash_table_destroy (groups);

#ifdef GDA_DEBUG_signal
        g_print (">> 'PUBLIC_DATA_CHANGED' from %p\n", paramlist);
#endif
	g_signal_emit (paramlist, gda_parameter_list_signals[PUBLIC_DATA_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'PUBLIC_DATA_CHANGED' from %p\n", paramlist);
#endif
}

static void
compute_shown_columns_index (GdaParameterListSource *source)
{
        gint ncols, nparams;
        gint *mask = NULL, masksize = 0;

        nparams = g_slist_length (source->nodes);
        g_return_if_fail (nparams > 0);
        ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (source->data_model));
        g_return_if_fail (ncols > 0);

        if (ncols > nparams) {
                /* we only want columns which are not parameters */
                gint i, current = 0;

                masksize = ncols - nparams;
                mask = g_new0 (gint, masksize);
                for (i = 0; i < ncols ; i++) {
                        GSList *list = source->nodes;
                        gboolean found = FALSE;
                        while (list && !found) {
                                if (GDA_PARAMETER_LIST_NODE (list->data)->source_column == i)
                                        found = TRUE;
                                else
                                        list = g_slist_next (list);
                        }
                        if (!found) {
                                mask[current] = i;
                                current ++;
                        }
                }
                masksize = current;
        }
        else {
                /* we want all the columns */
                gint i;

                masksize = ncols;
                mask = g_new0 (gint, masksize);
                for (i=0; i<ncols; i++) {
                        mask[i] = i;
                }
        }

        source->shown_n_cols = masksize;
        source->shown_cols_index = mask;
}

static void
compute_ref_columns_index (GdaParameterListSource *source)
{
        gint ncols, nparams;
        gint *mask = NULL, masksize = 0;

        nparams = g_slist_length (source->nodes);
        g_return_if_fail (nparams > 0);
        ncols = gda_data_model_get_n_columns (GDA_DATA_MODEL (source->data_model));
        g_return_if_fail (ncols > 0);

        if (ncols > nparams) {
                /* we only want columns which are parameters */
                gint i, current = 0;

                masksize = ncols - nparams;
                mask = g_new0 (gint, masksize);
                for (i=0; i<ncols ; i++) {
                        GSList *list = source->nodes;
                        gboolean found = FALSE;
                        while (list && !found) {
                                if (GDA_PARAMETER_LIST_NODE (list->data)->source_column == i)
                                        found = TRUE;
                                else
                                        list = g_slist_next (list);
                        }
                        if (found) {
                                mask[current] = i;
                                current ++;
                        }
                }
                masksize = current;
        }
        else {
                /* we want all the columns */
                gint i;

                masksize = ncols;
                mask = g_new0 (gint, masksize);
                for (i=0; i<ncols; i++) {
                        mask[i] = i;
                }
        }

        source->ref_n_cols = masksize;
        source->ref_cols_index = mask;
}


/**
 * gda_parameter_list_add_param
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object
 *
 * Adds @param to the list of parameters managed within @paramlist.
 * WARNING: the paramlist may decide not to use the @param parameter, but to
 * modify another parameter already present within the paramlist. The publicly available
 * lists from the @paramlist object may also be changed in the process.
 */
void
gda_parameter_list_add_param (GdaParameterList *paramlist, GdaParameter *param)
{
	g_return_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist));
	g_return_if_fail (param && GDA_IS_PARAMETER (param));

	gda_parameter_list_real_add_param (paramlist, param);
	compute_public_data (paramlist);
}

static void
gda_parameter_list_real_add_param (GdaParameterList *paramlist, GdaParameter *param)
{
	GSList *params;
	GdaParameter *similar = NULL;
	GSList *param_dest_fields;

	if (g_slist_find (paramlist->parameters, param))
		return;

	/* 
	 * try to find a similar parameter in the paramlist->parameters:
	 * a parameter B is similar to a parameter A if the list of fields which use the value
	 * of A and the list of fields which use the value of B have at least one element in common.
	 */
	param_dest_fields = gda_parameter_get_param_users (param);
	params = paramlist->parameters;
	while (params && !similar) {
		GSList *list1;
		list1 = gda_parameter_get_param_users (GDA_PARAMETER (params->data));

		while (list1 && !similar) {
			if (g_slist_find (param_dest_fields, list1->data))
				similar = GDA_PARAMETER (params->data);
			list1 = g_slist_next (list1);
		}
		params = g_slist_next (params);
	}
	
	if (similar) {
#ifdef GDA_DEBUG_NO
		g_print ("Param %p and %p are similar, keeping %p only\n", similar, param, similar);
#endif
		g_hash_table_insert (paramlist->priv->param_repl, param, similar);
		/* don't use @param, but instead use similar and update the list of fields
		   similar is for */
		while (param_dest_fields) {
			gda_parameter_declare_param_user (similar, GDA_OBJECT (param_dest_fields->data));
			param_dest_fields = g_slist_next (param_dest_fields);
		}
	}
	else {
		/* really add @param to the paramlist */
		paramlist->parameters = g_slist_append (paramlist->parameters, param);
		gda_object_connect_destroy (param,
					       G_CALLBACK (destroyed_param_cb), paramlist);
		g_signal_connect (G_OBJECT (param), "changed",
				  G_CALLBACK (changed_param_cb), paramlist);
		g_signal_connect (G_OBJECT (param), "restrict_changed",
				  G_CALLBACK (restrict_changed_param_cb), paramlist);

		g_object_ref (G_OBJECT (param));
		gda_parameter_replace_param_users (param, paramlist->priv->param_repl);
	}
}

/**
 * gda_parameter_list_merge_paramlist_params
 * @paramlist: a #GdaParameterList object
 * @paramlist_to_merge: a #GdaParameterList object
 *
 * Add to @paramlist all the parameters of @paramlist_to_merge.
 */
void
gda_parameter_list_merge_paramlist_params (GdaParameterList *paramlist, GdaParameterList *paramlist_to_merge)
{
	GSList *params = paramlist_to_merge->parameters;
	g_return_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist));
	g_return_if_fail (paramlist_to_merge && GDA_IS_PARAMETER_LIST (paramlist_to_merge));

	while (params) {
		gda_parameter_list_real_add_param (paramlist, GDA_PARAMETER (params->data));
		params = g_slist_next (params);
	}
	compute_public_data (paramlist);
}

/**
 * gda_parameter_list_is_coherent
 * @paramlist: a #GdaParameterList object
 * @error:
 *
 * Checks that @paramlist has a coherent public data structure
 *
 * Returns: TRUE if @paramlist is coherent
 */
gboolean
gda_parameter_list_is_coherent (GdaParameterList *paramlist, GError **error)
{
	GSList *list;

	/* testing parameters list */
	list = paramlist->parameters;
	while (list) {
		if (!gda_parameter_list_find_node_for_param (paramlist, GDA_PARAMETER (list->data))) {
			g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
				     _("Missing GdaParameterListNode for param %p"), list->data);
			return FALSE;
		}
		list = g_slist_next (list);
	}

	/* testing GdaParameterListNode structures */
	list = paramlist->nodes_list;
	while (list) {
		GdaParameterListNode *node = GDA_PARAMETER_LIST_NODE (list->data);

		if (!node->param) {
			g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
				     _("GdaParameterListNode has a NULL param attribute"));
			return FALSE;
		}
			
		if (node->source_model) {
			GdaParameterListSource *source;
			GdaColumn *col;

			source = gda_parameter_list_find_source (paramlist, node->source_model);
			if (!source) {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
					     _("Missing GdaParameterListSource"));
				return FALSE;
			}
			if (!g_slist_find (source->nodes, node)) {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
					     _("GdaParameterListSource does not list a GdaParameterListNode as it should"));
				return FALSE;
			}

			col = gda_data_model_describe_column (node->source_model, node->source_column);
			if (!col) {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
					     _("GdaDataModel %p does not have a column %d"),
					     node->source_model, node->source_column);
				return FALSE;
			}
			if (gda_column_get_gda_type (col) != gda_parameter_get_gda_type (node->param)) {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
					     _("GdaParameter is restricted by a column of the wrong type: "
					       "%s (%s expected)"),
					     gda_type_to_string (gda_parameter_get_gda_type (node->param)),
					     gda_type_to_string (gda_column_get_gda_type (col)));
				return FALSE;
			}
		}

		list = g_slist_next (list);
	}

	/* testing GdaParameterListSource structures */
	list = paramlist->sources_list;
	while (list) {
		GSList *list2;
		GdaParameterListSource *source = GDA_PARAMETER_LIST_SOURCE (list->data);
		
		if (!source->data_model) {
			g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
				     _("GdaParameterListSource has a NULL data model"));
			return FALSE;
		}

		list2 = source->nodes;
		while (list2) {
			if (!g_slist_find (paramlist->nodes_list, list2->data)) {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
					     _("GdaParameterListSource references a GdaParameterListNode"
					       "not found in the nodes list"));
				return FALSE;
			}
			if (GDA_PARAMETER_LIST_NODE (list2->data)->source_model != source->data_model) {
				g_set_error (error, GDA_PARAMETER_LIST_ERROR, 0,
					     _("GdaParameterListSource references a GdaParameterListNode"
					       "which does not use the same data model"));
				return FALSE;
			}
			list2 = g_slist_next (list2);
		}
		list = g_slist_next (list);
	}
	
	return TRUE;
}

static void
paramlist_remove_node (GdaParameterList *paramlist, GdaParameterListNode *node)
{
	g_return_if_fail (g_slist_find (paramlist->nodes_list, node));

	if (node->source_model)
		g_object_unref (G_OBJECT (node->source_model));

	paramlist->nodes_list = g_slist_remove (paramlist->nodes_list, node);
	g_free (node);
}

static void
paramlist_remove_source (GdaParameterList *paramlist, GdaParameterListSource *source)
{
	g_return_if_fail (g_slist_find (paramlist->sources_list, source));

	if (source->nodes)
		g_slist_free (source->nodes);
	g_free (source->shown_cols_index);
	g_free (source->ref_cols_index);

	paramlist->sources_list = g_slist_remove (paramlist->sources_list, source);
	g_free (source);
}

/**
 * gda_parameter_list_is_valid
 * @paramlist: a #GdaParameterList object
 *
 * Tells if all the paramlist's parameters have valid data
 *
 * Returns: TRUE if the paramlist is valid
 */
gboolean
gda_parameter_list_is_valid (GdaParameterList *paramlist)
{
	GSList *params;
	gboolean retval = TRUE;

	g_return_val_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist), FALSE);
	g_return_val_if_fail (paramlist->priv, FALSE);

	params = paramlist->parameters;
	while (params && retval) {
		if (!gda_parameter_is_valid (GDA_PARAMETER (params->data))) 
			retval = FALSE;
		
#ifdef GDA_DEBUG_NO
		g_print ("== PARAM %p: valid= %d, value=%s\n", params->data, gda_parameter_is_valid (GDA_PARAMETER (params->data)),
			 gda_parameter_get_value (GDA_PARAMETER (params->data)) ?
			 gda_value_stringify (gda_parameter_get_value (GDA_PARAMETER (params->data))) : "Null");
#endif
		params = g_slist_next (params);
	}

	return retval;
}

/**
 * gda_parameter_list_find_param
 * @paramlist: a #GdaParameterList object
 * @param_name: the name of the requested parameter
 *
 * Finds a #GdaParameter using its name
 *
 * Returns: a #GdaParameter or %NULL
 */
GdaParameter *
gda_parameter_list_find_param (GdaParameterList *paramlist, const gchar *param_name)
{
	GdaParameter *param = NULL;
	GSList *list;
	gchar *string_id;
	gchar *pname;

	g_return_val_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist), NULL);
	g_return_val_if_fail (paramlist->priv, NULL);

	list = paramlist->parameters;
	while (list && !param) {
		pname = (gchar *) gda_object_get_name (GDA_OBJECT (list->data));
		if (pname && !strcmp (pname, param_name))
			param = GDA_PARAMETER (list->data);
		if (!param) {
			g_object_get (G_OBJECT (list->data), "string_id", &string_id, NULL);
			if (string_id && !strcmp (string_id, param_name))
				param = GDA_PARAMETER (list->data);
			g_free (string_id);
		}
		list = g_slist_next (list);
	}

	return param;
}

/**
 * gda_parameter_list_find_param_for_user
 * @paramlist: a #GdaParameterList object
 * @user: a #GdaObject object
 *
 * Finds a #GdaParameter which is to be used by @user
 *
 * Returns: a #GdaParameter or %NULL
 */
GdaParameter *
gda_parameter_list_find_param_for_user (GdaParameterList *paramlist, GdaObject *user)
{
	GdaParameter *param = NULL;
	GSList *list;

	g_return_val_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist), NULL);
	g_return_val_if_fail (paramlist->priv, NULL);

	list = paramlist->parameters;
	while (list && !param) {
		GSList *users = gda_parameter_get_param_users (GDA_PARAMETER (list->data));
		if (users &&  g_slist_find (users, user))
			param = GDA_PARAMETER (list->data);
		list = g_slist_next (list);
	}

	return param;
}

/**
 * gda_parameter_list_find_node_for_param
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object
 *
 * Finds a #GdaParameterListNode holding information for @param, don't modify the returned structure
 *
 * Returns: a #GdaParameterListNode or %NULL
 */
GdaParameterListNode *
gda_parameter_list_find_node_for_param (GdaParameterList *paramlist, GdaParameter *param)
{
	GdaParameterListNode *retval = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_PARAMETER_LIST (paramlist), NULL);
	g_return_val_if_fail (paramlist->priv, NULL);
	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (g_slist_find (paramlist->parameters, param), NULL);

	list = paramlist->nodes_list;
	while (list && !retval) {
		if (GDA_PARAMETER_LIST_NODE (list->data)->param == param)
			retval = GDA_PARAMETER_LIST_NODE (list->data);

		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_parameter_list_find_source_for_param
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object
 *
 * Finds a #GdaParameterListSource which contains the #GdaDataModel restricting the possible values of
 * @param, don't modify the returned structure.
 *
 * Returns: a #GdaParameterListSource or %NULL
 */
GdaParameterListSource *
gda_parameter_list_find_source_for_param (GdaParameterList *paramlist, GdaParameter *param)
{
	GdaParameterListNode *node;
	
	node = gda_parameter_list_find_node_for_param (paramlist, param);
	if (node && node->source_model)
		return gda_parameter_list_find_source (paramlist, node->source_model);
	else
		return NULL;
}

/**
 * gda_parameter_list_find_group_for_param
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object
 *
 * Finds a #GdaParameterListGroup which lists a  #GdaParameterListNode containing @param,
 * don't modify the returned structure.
 *
 * Returns: a #GdaParameterListGroup or %NULL
 */
GdaParameterListGroup *
gda_parameter_list_find_group_for_param (GdaParameterList *paramlist, GdaParameter *param)
{
	GdaParameterListGroup *retval = NULL;
	GSList *list, *sublist;

	g_return_val_if_fail (GDA_IS_PARAMETER_LIST (paramlist), NULL);
	g_return_val_if_fail (paramlist->priv, NULL);
	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (g_slist_find (paramlist->parameters, param), NULL);

	list = paramlist->groups_list;
	while (list && !retval) {
		sublist = GDA_PARAMETER_LIST_GROUP (list->data)->nodes;
		while (sublist && !retval) {
			if (GDA_PARAMETER_LIST_NODE (sublist->data)->param == param)
				retval = GDA_PARAMETER_LIST_GROUP (list->data);
			else
				sublist = g_slist_next (sublist);	
		}

		list = g_slist_next (list);
	}

	return retval;
}


/**
 * gda_parameter_list_find_source
 * @paramlist: a #GdaParameterList object
 * @model: a #GdaDataModel object
 *
 * Finds the #GdaParameterListSource structure used in @paramlist for which @model is a
 * the data model, don't modify the returned structure
 *
 * Returns: a #GdaParameterListSource pointer or %NULL.
 */
GdaParameterListSource *
gda_parameter_list_find_source (GdaParameterList *paramlist, GdaDataModel *model)
{
	GdaParameterListSource *retval = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_PARAMETER_LIST (paramlist), NULL);
	g_return_val_if_fail (paramlist->priv, NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	list = paramlist->sources_list;
	while (list && !retval) {
		if (GDA_PARAMETER_LIST_SOURCE (list->data)->data_model == model)
			retval = GDA_PARAMETER_LIST_SOURCE (list->data);

		list = g_slist_next (list);
	}

	return retval;
}


/**
 * gda_parameter_list_set_param_default_value
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object, managed by @paramlist
 * @value: a #GdaValue, of the same type as @param, or %NULL
 *
 * Stores @value in @paramlist to make it possible for @paramlist's users to find a default value
 * for @param when one is required, instead of %NULL.
 *
 * @paramlist only provides a storage functionnality, the way the value obtained with 
 * gda_parameter_list_get_param_default_value() is used is up to @paramlist's user.
 */
void
gda_parameter_list_set_param_default_value (GdaParameterList *paramlist, GdaParameter *param, const GdaValue *value)
{
	g_return_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist));
	g_return_if_fail (param && GDA_IS_PARAMETER (param));
	g_return_if_fail (g_slist_find (paramlist->parameters, param));

	if (value && ! gda_value_is_null ((GdaValue *) value)) {
		g_return_if_fail (gda_value_get_type ((GdaValue *) value) ==
				  gda_parameter_get_gda_type (param));
		g_hash_table_insert (paramlist->priv->param_default_values, param, 
				     gda_value_copy ((GdaValue *) value));
	}
	else
		g_hash_table_remove (paramlist->priv->param_default_values, param);
}

static void destroyed_alias_cb (GdaParameter *alias, GdaParameterList *paramlist);

/*
 * gda_parameter_list_set_param_default_alias
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object, managed by @paramlist
 * @alias: a #GdaParameter object, of the same type as @param, or %NULL
 *
 * Stores a reference to @alias in @paramlist to make it possible for @paramlist's users to find a default value
 * for @param when one is required, instead of %NULL.
 *
 * @paramlist only provides a storage functionnality, the way the value obtained with 
 * gda_parameter_list_get_param_default_value() is used is up to @paramlist's user.
 */
void
gda_parameter_list_set_param_default_alias (GdaParameterList *paramlist, GdaParameter *param, GdaParameter *alias)
{
	GdaParameter *oldalias;

	g_return_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist));
	g_return_if_fail (param && GDA_IS_PARAMETER (param));
	g_return_if_fail (g_slist_find (paramlist->parameters, param));

	/* remove the old alias if there was one */
	oldalias = g_hash_table_lookup (paramlist->priv->param_default_aliases, param);
	if (oldalias)
		destroyed_alias_cb (oldalias, paramlist);

	/* take care of new alias if there is one */
	if (alias) {
		g_return_if_fail (alias != param);
		g_return_if_fail (alias && GDA_IS_PARAMETER (alias));
		g_return_if_fail (gda_parameter_get_gda_type (param) == 
				  gda_parameter_get_gda_type (alias));
		g_hash_table_insert (paramlist->priv->param_default_aliases, param, alias);
		g_hash_table_insert (paramlist->priv->aliases_default_param, alias, param);
		gda_object_connect_destroy (alias, G_CALLBACK (destroyed_alias_cb), paramlist);
		g_object_ref (G_OBJECT (alias));
	}
}

static void
destroyed_alias_cb (GdaParameter *alias, GdaParameterList *paramlist)
{
	GdaParameter *param;

	g_signal_handlers_disconnect_by_func (G_OBJECT (alias),
					      G_CALLBACK (destroyed_alias_cb), paramlist);
	param = g_hash_table_lookup (paramlist->priv->aliases_default_param, alias);
	g_hash_table_remove (paramlist->priv->param_default_aliases, param);
	g_hash_table_remove (paramlist->priv->aliases_default_param, alias);
	g_object_unref (G_OBJECT (alias));
}

static void
foreach_finalize_alias (GdaParameter *param, GdaParameter *alias, GdaParameterList *paramlist)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (alias),
					      G_CALLBACK (destroyed_alias_cb), paramlist);
	g_object_unref (G_OBJECT (alias));
}

/**
 * gda_parameter_list_get_param_default_value
 * @paramlist: a #GdaParameterList object
 * @param: a #GdaParameter object, managed by @paramlist
 *
 * Retreives a prefered default value to be used by @paramlist's users when necessary.
 * The usage of such values is decided by @paramlist's users.
 * 
 * @paramlist only offers to store the value
 * using gda_parameter_list_set_param_default_value() or to store a #GdaParameter reference from which to get
 * a value using gda_parameter_list_set_param_default_alias().
 *
 * Returns: a #GdaValue, or %NULL.
 */
const GdaValue *
gda_parameter_list_get_param_default_value  (GdaParameterList *paramlist, GdaParameter *param)
{
	const GdaValue *value;
	g_return_val_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist), NULL);
	g_return_val_if_fail (paramlist->priv, NULL);
	g_return_val_if_fail (param && GDA_IS_PARAMETER (param), NULL);

	value = g_hash_table_lookup (paramlist->priv->param_default_values, param);
	if (value)
		return value;
	else {
		GdaParameter *alias;
		
		alias = g_hash_table_lookup (paramlist->priv->param_default_aliases, param);
		if (alias && gda_parameter_is_valid (alias))
			return gda_parameter_get_value (alias);
	}

	return NULL;
}

#ifdef GDA_DEBUG
static void
gda_parameter_list_dump (GdaParameterList *paramlist, guint offset)
{
	gchar *str;
        guint i;
	
	g_return_if_fail (paramlist && GDA_IS_PARAMETER_LIST (paramlist));

        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (paramlist->priv) {
		GError *error = NULL;
		GSList *list;
                g_print ("%s" D_COL_H1 "GdaParameterList" D_COL_NOR " %p (id=%s)",
                         str, paramlist, gda_object_get_id (GDA_OBJECT (paramlist)));
		if (! gda_parameter_list_is_coherent (paramlist, &error)) {
			g_print (" " D_COL_ERR "not coherent: " D_COL_NOR "%s", error->message);
			g_error_free (error);
		}
		g_print ("\n");

		g_print ("%s     ***** Parameters:\n", str);
		list = paramlist->parameters;
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}

		/* g_print ("\n%s     ***** Nodes:\n", str); */
/* 		list = paramlist->nodes; */
/* 		while (list) { */
/* 			GdaParameterListNode *node = GDA_PARAMETER_LIST_NODE (list->data); */
/* 			if (node->param)  */
/* 				g_print ("%s     * " D_COL_H1 "GdaParameterListNode %p" D_COL_NOR " for param %p\n",  */
/* 					 str, node, node->param); */
/* 			else { */
/* 				GSList *params = node->params; */
/* 				g_print ("%s     * " D_COL_H1 "GdaParameterListNode %p" D_COL_NOR "\n", str, node); */
/* 				gda_object_dump (GDA_OBJECT (node->data_for_param), offset+10); */
/* 				while (params) { */
/* 					g_print ("%s       -> param %p (%s)\n", str, params->data,  */
/* 						 gda_object_get_name (GDA_OBJECT (params->data))); */
/* 					params = g_slist_next (params); */
/* 				} */
/* 			} */
/* 			list = g_slist_next (list); */
/* 		} */
		TO_IMPLEMENT;
	}
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, paramlist);
}
#endif
 
