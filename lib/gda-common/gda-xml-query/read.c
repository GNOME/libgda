/* XmlQuery: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "xml-query.h"
#include "utils.h"
#include "read.h"

typedef XmlQueryItem * (Constructor)(gchar *tag, gchar *sqlfmt, gchar *sqlop);


typedef struct _ClassInfo ClassInfo;
struct _ClassInfo {
    gchar        *class;
    Constructor  *constr;
};


typedef struct _ItemInfo ItemInfo;
struct _ItemInfo {
    gchar *tag;
    gchar *class;
};


typedef XmlQueryItem * (NodeFunc)(void);
typedef void           (AttrFunc)(XmlQueryItem *, gchar *);



static GHashTable *nodeTable = NULL;


static void
init_node_table(void);

static XmlQueryItem *
proc_node(xmlNode *node, XmlQueryItem *parent);

static void
proc_attr(xmlAttr *attr, XmlQueryItem *item);


/*
 * xmlDocPtr xmlParseMemory(char *buffer, int size);
 * xmlDocPtr xmlParseFile(const char *filename);
 */

XmlQueryItem *
proc_file(gchar *filename)
{
    xmlDoc *doc;
    XmlQueryItem *query;

    if (nodeTable == NULL)
        init_node_table();

    xmlKeepBlanksDefault(0);
    xmlLoadExtDtdDefaultValue = 1;
    xmlDoValidityCheckingDefaultValue = 1;
    doc = xmlParseFile(filename);
    if (doc == NULL) return NULL;

    query = proc_node(xmlDocGetRootElement(doc),NULL);

    xmlFreeDoc(doc);

    return query;
}


XmlQueryItem *
proc_node(xmlNode *node, XmlQueryItem *parent)
{
    XmlQueryItem   *item;
    NodeFunc       *nodefunc;
    gchar          *nname = (gchar *)(node->name);;

    nodefunc = g_hash_table_lookup(nodeTable,nname);

    if (nodefunc == NULL) {
	g_warning("unknown node %s\n",nname);
	return NULL;
    }

    item = nodefunc();
    if (parent != NULL)
        xml_query_item_add(parent,item);

    if (node->properties)
        proc_attr(node->properties,item);
    if (node->children)
        proc_node(node->children,item);
    if (node->next)
        proc_node(node->next,parent);

    return item;
}


void proc_attr(xmlAttr *attr, XmlQueryItem *item)
{
    gint   atype;
    gchar *aname = (gchar *)attr->name;
    gchar *avalue = (gchar *)attr->children->content;

    xml_query_item_set_attrib(item,aname,avalue);

    atype = attr->atype;

    switch (atype) {
        case XML_ATTRIBUTE_ID:
            xml_query_item_add_id(item,avalue);
        break;
        case XML_ATTRIBUTE_IDREF:
            xml_query_item_add_ref(item,avalue);
        break;
        default:
            /* do nothing */ ;
    }

    if (attr->next) {
        proc_attr(attr->next,item);
    }    
}




static void
init_node_table(void)
{
    nodeTable = g_hash_table_new(g_str_hash,g_str_equal);

    g_hash_table_insert(nodeTable,"field",      xml_query_field_new);
    g_hash_table_insert(nodeTable,"const",      xml_query_const_new);
    g_hash_table_insert(nodeTable,"valueref",   xml_query_valueref_new);
    g_hash_table_insert(nodeTable,"column",     xml_query_column_new);

    g_hash_table_insert(nodeTable,"union",      xml_query_bin_new_union);
    g_hash_table_insert(nodeTable,"unionall",   xml_query_bin_new_unionall);
    g_hash_table_insert(nodeTable,"intersect",  xml_query_bin_new_intersect);
    g_hash_table_insert(nodeTable,"minus",      xml_query_bin_new_minus);
    g_hash_table_insert(nodeTable,"not",        xml_query_bin_new_not);
    g_hash_table_insert(nodeTable,"exists",     xml_query_bin_new_exists);
    g_hash_table_insert(nodeTable,"null",       xml_query_bin_new_null);
    g_hash_table_insert(nodeTable,"where",      xml_query_bin_new_where);
    g_hash_table_insert(nodeTable,"having",     xml_query_bin_new_having);
    g_hash_table_insert(nodeTable,"on",         xml_query_bin_new_on);

    g_hash_table_insert(nodeTable,"func",       xml_query_func_new);
    g_hash_table_insert(nodeTable,"value",      xml_query_value_new);
    g_hash_table_insert(nodeTable,"query",      xml_query_query_new);
    g_hash_table_insert(nodeTable,"join",       xml_query_join_new);
    g_hash_table_insert(nodeTable,"target",     xml_query_target_new);

    g_hash_table_insert(nodeTable,"eq",         xml_query_dual_new_eq);
    g_hash_table_insert(nodeTable,"ne",         xml_query_dual_new_ne);
    g_hash_table_insert(nodeTable,"lt",         xml_query_dual_new_lt);
    g_hash_table_insert(nodeTable,"le",         xml_query_dual_new_le);
    g_hash_table_insert(nodeTable,"gt",         xml_query_dual_new_gt);
    g_hash_table_insert(nodeTable,"ge",         xml_query_dual_new_ge);
    g_hash_table_insert(nodeTable,"like",       xml_query_dual_new_like);
    g_hash_table_insert(nodeTable,"in",         xml_query_dual_new_in);
    g_hash_table_insert(nodeTable,"set",        xml_query_dual_new_set);

    g_hash_table_insert(nodeTable,"and",        xml_query_list_new_and);
    g_hash_table_insert(nodeTable,"or",         xml_query_list_new_or);
    g_hash_table_insert(nodeTable,"order",      xml_query_list_new_order);
    g_hash_table_insert(nodeTable,"dest",       xml_query_list_new_dest);
    g_hash_table_insert(nodeTable,"arglist",    xml_query_list_new_arglist);
    g_hash_table_insert(nodeTable,"setlist",    xml_query_list_new_setlist);
    g_hash_table_insert(nodeTable,"sourcelist", xml_query_list_new_sourcelist);
    g_hash_table_insert(nodeTable,"targetlist", xml_query_list_new_targetlist);
    g_hash_table_insert(nodeTable,"valuelist",  xml_query_list_new_valuelist);
    g_hash_table_insert(nodeTable,"joinlist",   xml_query_list_new_joinlist);
    g_hash_table_insert(nodeTable,"group",      xml_query_list_new_group);

    g_hash_table_insert(nodeTable,"select",     xml_query_select_new);
    g_hash_table_insert(nodeTable,"insert",     xml_query_insert_new);
    g_hash_table_insert(nodeTable,"delete",     xml_query_delete_new);
    g_hash_table_insert(nodeTable,"update",     xml_query_update_new);

}    




