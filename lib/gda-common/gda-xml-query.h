/* GNOME DB components libary
 * Copyright (C) 2000 Vivien Malerba
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_xml_query_h__)
#define __gda_xml_query_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtk.h>
#endif

#include <gda-xml-file.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef HAVE_GOBJECT
#  define GDA_TYPE_XML_QUERY (gda_xml_query_get_type ())
#  define GDA_XML_QUERY(obj) \
          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_QUERY, Gda_XmlQuery)
#  define GDA_XML_QUERY_CLASS(klass) \
          G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_QUERY, Gda_XmlQueryClass)
#  define GDA_XML_QUERY_IS_OBJECT(obj) \
          G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_QUERY)
#else
#  define GDA_XML_QUERY(obj)            GTK_CHECK_CAST(obj, gda_xml_query_get_type(), Gda_XmlQuery)
#  define GDA_XML_QUERY_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, gda_xml_query_get_type(), Gda_XmlQueryClass)
#  define GDA_XML_QUERY_IS_OBJECT(obj)  GTK_CHECK_TYPE(obj, gda_xml_query_get_type())
#endif

typedef struct _Gda_XmlQuery      Gda_XmlQuery;
typedef struct _Gda_XmlQueryClass Gda_XmlQueryClass;

typedef enum {
  /* ident for the tags */
  GDA_XML_QUERY_TABLE,
  GDA_XML_QUERY_VIEW,
  GDA_XML_QUERY_CONST,
  GDA_XML_QUERY_QUERY,
  GDA_XML_QUERY_FIELD,
  GDA_XML_QUERY_ALLFIELDS,
  GDA_XML_QUERY_FUNC,
  GDA_XML_QUERY_AGGREGATE,
  GDA_XML_QUERY_AND,
  GDA_XML_QUERY_OR,
  GDA_XML_QUERY_NOT,
  GDA_XML_QUERY_EQ,
  GDA_XML_QUERY_NONEQ,
  GDA_XML_QUERY_INF,
  GDA_XML_QUERY_INFEQ,
  GDA_XML_QUERY_SUP,
  GDA_XML_QUERY_SUPEQ,
  GDA_XML_QUERY_NULL,
  GDA_XML_QUERY_LIKE,
  GDA_XML_QUERY_CONTAINS,
  GDA_XML_QUERY_TARGET_TREE,
  GDA_XML_QUERY_SOURCES_TREE,
  GDA_XML_QUERY_VALUES_TREE,
  GDA_XML_QUERY_QUAL_TREE,
  GDA_XML_QUERY_UNKNOWN
} Gda_XmlQueryTag;

typedef enum {
  /* possible type of queries */
  GDA_XML_QUERY_SELECT,
  GDA_XML_QUERY_INSERT,
  GDA_XML_QUERY_UPDATE,
  GDA_XML_QUERY_DELETE
} Gda_XmlQueryOperation;

struct _Gda_XmlQuery
{
  Gda_XmlFile              obj;

  gchar                   *id;
  Gda_XmlQueryOperation    op; /* SELECT, ... */
  xmlNodePtr               target;
  xmlNodePtr               sources;
  xmlNodePtr               values;
  xmlNodePtr               qual;
  gchar                   *sql_txt;
};

struct _Gda_XmlQueryClass
{
  Gda_XmlFileClass parent_class;
#ifdef HAVE_GOBJECT
  GObjectClass    *parent;
#endif
};

#ifdef HAVE_GOBJECT
GType          gda_xml_query_get_type         (void);
#else
GtkType        gda_xml_query_get_type         (void);
#endif

Gda_XmlQuery*  gda_xml_query_new              (const gchar *id, 
					       Gda_XmlQueryOperation op);
/* create and load from file */
Gda_XmlQuery*  gda_xml_query_new_from_file    (const gchar *filename);
/* create from a node of a bigger XML doc */
Gda_XmlQuery*  gda_xml_query_new_from_node    (const xmlNodePtr node);
/* destroy */
void           gda_xml_query_destroy          (Gda_XmlQuery *q);


/* contents manipulation */
xmlNodePtr     gda_xml_query_add              (Gda_XmlQuery *q, 
					       Gda_XmlQueryTag op, ...);
void           gda_xml_query_node_add_child   (xmlNodePtr node, 
					       xmlNodePtr child);
void           gda_xml_query_set_attribute    (xmlNodePtr node, 
					       const gchar *attname,
					       const gchar *value);
void           gda_xml_query_set_id           (Gda_XmlQuery *q,
					       xmlNodePtr node, 
					       const gchar *value);
gchar*         gda_xml_query_get_attribute    (xmlNodePtr node, 
					       const gchar *attname);
void           gda_xml_query_set_node_value   (xmlNodePtr node, gchar *value);

Gda_XmlQueryTag gda_xml_query_get_tag         (xmlNodePtr node);

xmlNodePtr     gda_xml_query_find_tag         (xmlNodePtr parent, 
					       Gda_XmlQueryTag tag,
					       xmlNodePtr last_child);

/* simple SQL rendering implementation. Every DBMS should implement its own */
gchar*         gda_xml_query_get_standard_sql (Gda_XmlQuery *q);

#if defined(__cplusplus)
}
#endif

#endif

/* usage for gda_xml_query_add(), accepted combinations are:
   GDA_XML_QUERY_TARGET_TREE, GDA_XML_QUERY_TABLE, "table name"
                            , GDA_XML_QUERY_VIEW, "view name"
   GDA_XML_QUERY_SOURCES_TREE, GDA_XML_QUERY_TABLE, "table name"
                             , GDA_XML_QUERY_VIEW, "view name"
   GDA_XML_QUERY_VALUES_TREE, GDA_XML_QUERY_CONST, "const value"
                            , GDA_XML_QUERY_QUERY, Gda_XmlQuery *q
			    , GDA_XML_QUERY_FIELD, "source id", "field name"
			    , GDA_XML_QUERY_ALLFIELDS, "source id"
			    , GDA_XML_QUERY_FUNC, "function name"
			    , GDA_XML_QUERY_AGGREGATE, "aggregate name"

   GDA_XML_QUERY_AND, xmlNodePtr parent
   GDA_XML_QUERY_OR, xmlNodePtr parent
   GDA_XML_QUERY_NOT, xmlNodePtr parent
   GDA_XML_QUERY_EQ, xmlNodePtr parent
   GDA_XML_QUERY_NONEQ, xmlNodePtr parent
   GDA_XML_QUERY_INF, xmlNodePtr parent
   GDA_XML_QUERY_INFEQ, xmlNodePtr parent
   GDA_XML_QUERY_SUP, xmlNodePtr parent
   GDA_XML_QUERY_SUPEQ, xmlNodePtr parent
   GDA_XML_QUERY_NULL, xmlNodePtr parent
   GDA_XML_QUERY_LIKE, xmlNodePtr parent
   GDA_XML_QUERY_CONTAINS, xmlNodePtr parent
   
   GDA_XML_QUERY_CONST, "const value", xmlNodePtr parent
   GDA_XML_QUERY_QUERY, Gda_XmlQuery *q, xmlNodePtr parent
   GDA_XML_QUERY_FIELD, "source id", "field name", xmlNodePtr parent
   GDA_XML_QUERY_ALLFIELDS, "source id", xmlNodePtr parent
   GDA_XML_QUERY_FUNC, "function name", xmlNodePtr parent
   GDA_XML_QUERY_AGGREGATE, "agg name", xmlNodePtr parent
   GDA_XML_QUERY_TABLE, "table name", xmlNodePtr parent
   GDA_XML_QUERY_VIEW, "view name", xmlNodePtr parent
*/
