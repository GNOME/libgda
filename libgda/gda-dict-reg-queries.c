/* gda-dict-reg-queries.c
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
#include "gda-dict-private.h"
#include "gda-dict-reg-queries.h"
#include "gda-query.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"

#define XML_GROUP_TAG "gda_queries"
#define XML_ELEM_TAG "gda_query"

static gboolean queries_load_xml_tree (GdaDict *dict, xmlNodePtr queries, GError **error);
static gboolean queries_save_xml_tree (GdaDict *dict, xmlNodePtr queries, GError **error);
static GSList  *queries_get_objects  (GdaDict *dict);

typedef struct {
	GdaDictRegisterStruct main;
	guint                 serial;
} LocalRegisterStruct;
#define LOCAL_REG(x) ((LocalRegisterStruct*)(x))
#define MAIN_REG(x) ((GdaDictRegisterStruct*)(x))

GdaDictRegisterStruct *
gda_queries_get_register ()
{
	LocalRegisterStruct *reg = NULL;

	reg = g_new0 (LocalRegisterStruct, 1);
	MAIN_REG (reg)->type = GDA_TYPE_QUERY;
	MAIN_REG (reg)->xml_group_tag = XML_GROUP_TAG;
	MAIN_REG (reg)->free = NULL;
	MAIN_REG (reg)->dbms_sync_key = NULL;
	MAIN_REG (reg)->dbms_sync_descr = NULL;
	MAIN_REG (reg)->dbms_sync = NULL; /* no database sync possible */
	MAIN_REG (reg)->get_objects = queries_get_objects;
	MAIN_REG (reg)->load_xml_tree = queries_load_xml_tree;
	MAIN_REG (reg)->save_xml_tree = queries_save_xml_tree;
	MAIN_REG (reg)->sort = FALSE;

	MAIN_REG (reg)->all_objects = NULL;
	MAIN_REG (reg)->assumed_objects = NULL;
	reg->serial = 1;

	return MAIN_REG (reg);
}

guint
gda_queries_get_serial (GdaDictRegisterStruct *reg)
{
	return LOCAL_REG (reg)->serial++;
}

void
gda_queries_declare_serial (GdaDictRegisterStruct *reg, guint id)
{
	if (LOCAL_REG (reg)->serial <= id)
		LOCAL_REG (reg)->serial = id + 1;
}

static gboolean
queries_load_xml_tree (GdaDict *dict, xmlNodePtr queries, GError **error)
{
	xmlNodePtr qnode = queries->children;
	gboolean allok = TRUE;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_QUERY);
	g_assert (reg);

	while (qnode && allok) {
		if (!strcmp (qnode->name, XML_ELEM_TAG)) {
			GdaQuery *query;

			query = gda_query_new (dict);
			allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (query), qnode, error);
			if (allok)
				gda_dict_assume_object (dict, (GdaObject *) query);
			g_object_unref (G_OBJECT (query));
		}
		qnode = qnode->next;
	}

	if (allok) {
		GSList *list;
		for (list = reg->assumed_objects; list; list = list->next)
			gda_referer_activate (GDA_REFERER (list->data));
	}

	return allok;
}


static gboolean
queries_save_xml_tree (GdaDict *dict, xmlNodePtr group, GError **error)
{
	GSList *list;
	gboolean retval = TRUE;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_QUERY);
	g_assert (reg);
	
	for (list = reg->assumed_objects; list && retval; list = list->next) {
		if (!gda_query_get_parent_query (GDA_QUERY (list->data))) {
			/* We only add nodes for queries which do not have any parent query */
			xmlNodePtr qnode;
			
			qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
			if (qnode)
				xmlAddChild (group, qnode);
			else 
				/* error handling */
				retval = FALSE;
		}
	}

	return retval;
}

static GSList *
queries_get_objects (GdaDict *dict)
{
	GSList *list, *retval = NULL;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_QUERY);
	g_assert (reg);

	for (list = reg->assumed_objects; list; list = list->next)
		if (! gda_query_get_parent_query (GDA_QUERY (list->data)))
			retval = g_slist_append (retval, list->data);

	return retval;
}
