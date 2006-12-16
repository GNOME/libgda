/* gda-dict-reg-graphs.c
 *
 * Copyright (C) 2006 Vivien Malerba
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
#include "gda-dict-private.h"
#include "gda-dict-reg-graphs.h"
#include "graph/gda-graph.h"
#include "graph/gda-graph-query.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"
#include "gda-query.h"

#define XML_GROUP_TAG "gda_graphs"
#define XML_ELEM_TAG "gda_graph"

static gboolean graphs_load_xml_tree (GdaDict *dict, xmlNodePtr graphs, GError **error);
static gboolean graphs_save_xml_tree (GdaDict *dict, xmlNodePtr graphs, GError **error);

typedef struct {
	GdaDictRegisterStruct main;
	guint                 serial;
} LocalRegisterStruct;
#define LOCAL_REG(x) ((LocalRegisterStruct*)(x))
#define MAIN_REG(x) ((GdaDictRegisterStruct*)(x))

GdaDictRegisterStruct *
gda_graphs_get_register ()
{
	LocalRegisterStruct *reg = NULL;

	reg = g_new0 (LocalRegisterStruct, 1);
	MAIN_REG (reg)->type = GDA_TYPE_GRAPH;
	MAIN_REG (reg)->xml_group_tag = XML_GROUP_TAG;
	MAIN_REG (reg)->free = NULL;
	MAIN_REG (reg)->dbms_sync_key = NULL;
        MAIN_REG (reg)->dbms_sync_descr = NULL;
	MAIN_REG (reg)->dbms_sync = NULL; /* no database sync possible */
	MAIN_REG (reg)->get_objects = NULL;
	MAIN_REG (reg)->load_xml_tree = graphs_load_xml_tree;
	MAIN_REG (reg)->save_xml_tree = graphs_save_xml_tree;
	MAIN_REG (reg)->sort = FALSE;

	MAIN_REG (reg)->all_objects = NULL;
	MAIN_REG (reg)->assumed_objects = NULL;
	reg->serial = 1;

	return MAIN_REG (reg);
}

guint
gda_graphs_get_serial (GdaDictRegisterStruct *reg)
{
	return LOCAL_REG (reg)->serial++;
}

void
gda_graphs_declare_serial (GdaDictRegisterStruct *reg, guint id)
{
	if (LOCAL_REG (reg)->serial <= id)
		LOCAL_REG (reg)->serial = id + 1;
}

static gboolean
graphs_load_xml_tree (GdaDict *dict, xmlNodePtr graphs, GError **error)
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
					GdaQuery *query;

					query = (GdaQuery *) gda_dict_get_object_by_xml_id (dict, GDA_TYPE_QUERY, oprop);
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
					graph = GDA_GRAPH (gda_graph_new (dict, GDA_GRAPH_DB_RELATIONS));
				allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (graph), qnode, error);
				gda_dict_assume_object_as (dict, (GdaObject *) graph, GDA_TYPE_GRAPH);
				g_object_unref (G_OBJECT (graph));
			}
		}
		qnode = qnode->next;
	}

	return allok;
}


static gboolean
graphs_save_xml_tree (GdaDict *dict, xmlNodePtr group, GError **error)
{
	GSList *list;
	gboolean retval = TRUE;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_GRAPH);
	g_assert (reg);

	for (list = reg->assumed_objects; list && retval; list = list->next) {
		xmlNodePtr qnode;
		
		qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
		if (qnode)
			xmlAddChild (group, qnode);
		else 
			/* error handling */
			retval = FALSE;
	}

	return retval;
}


/**
 * gda_graphs_get_with_type
 * @dict: a #GdaDict object
 * @type_of_graphs: the requested type of graphs
 *
 * Get a list of the graphs managed by @dict, which are of the
 * requested type.
 *
 * Returns: a new list of #GdaGraph objects
 */
GSList *
gda_graphs_get_with_type (GdaDict *dict, GdaGraphType type_of_graphs)
{
	GSList *list;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_GRAPH);
	g_assert (reg);

	list = reg->assumed_objects;
	if (list) {
		GSList *retval = NULL;

		for (; list; list = list->next)
			if (gda_graph_get_graph_type (GDA_GRAPH (list->data)) == type_of_graphs)
				retval = g_slist_prepend (retval, list->data);
		
		return g_slist_reverse (retval);
	}
	else
		return NULL;
}

/**
 * gda_graphs_get_graph_for_object
 * @dict: a #GdaDict object
 * @obj: a #Gobject object
 *
 * Find a #GdaGraph object guiven the object it is related to.
 *
 * Returns: the #GdaGraph object, or NULL if not found
 */
GdaGraph *
gda_graphs_get_graph_for_object (GdaDict *dict, GObject *obj)
{
	GdaGraph *graph = NULL;

	GSList *list;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_GRAPH);
	g_assert (reg);

        list = reg->assumed_objects;
        while (list && !graph) {
		GObject *ref_obj;
		g_object_get (G_OBJECT (list->data), "ref_object", &ref_obj, NULL);
		if (ref_obj == obj)
                        graph = GDA_GRAPH (list->data);
		if (ref_obj)
			g_object_unref (ref_obj);
                list = g_slist_next (list);
        }

        return graph;
}
