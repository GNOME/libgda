/* GDA common libary
 * Copyright (C) 2000 Vivien Malerba
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

/*                    FIXME FIXME
 * Do some more work to make sure the right xmlValidCtxtPtr is uqed when 
 * the dociment is validated (when loading from a file, or when using
 * xmlValidateElement()
 */

#include "config.h"
#include "gda-xml-query.h"

#define GDA_XML_QUERY_DTD_FILE DTDINSTALLDIR"/gda-xml-query.dtd"

/*
 * XmlQuery object signals
 */
enum
{
  GDA_XML_QUERY_LAST_SIGNAL
};

static gint gda_xml_query_signals[GDA_XML_QUERY_LAST_SIGNAL] = { 0 };

#ifdef HAVE_GOBJECT
static void gda_xml_query_finalize (GObject *object);
#endif

/*
 * XmlQuery object interface
 */
#ifdef HAVE_GOBJECT
static void
gda_xml_query_class_init (Gda_XmlQueryClass *klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = &gda_xml_query_finalize;
  klass->parent = g_type_class_peek_parent (klass);
}
#else
static void
gda_xml_query_class_init (Gda_XmlQueryClass *klass)
{
  GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

  object_class->destroy = (void (*)(GtkObject *)) gda_xml_query_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_xml_query_init (Gda_XmlQuery *object, Gda_XmlQueryClass *klass)
#else
gda_xml_query_init (Gda_XmlQuery *object)
#endif
{
  g_return_if_fail(GDA_XML_QUERY_IS_OBJECT(object));

  object->id = NULL;
  object->target = NULL;
  object->sources = NULL;
  object->values = NULL;
  object->qual = NULL;
  object->sql_txt = NULL;
}

#ifdef HAVE_GOBJECT
GType
gda_xml_query_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo info =
      {
       sizeof (Gda_XmlQueryClass),                /* class_size */
        NULL,                                      /* base_init */
        NULL,                                      /* base_finalize */
       (GClassInitFunc) gda_xml_query_class_init, /* class_init */
        NULL,                                      /* class_finalize */
        NULL,                                      /* class_data */
       sizeof (Gda_XmlQuery),                     /* instance_size */
        0,                                         /* n_preallocs */
       (GInstanceInitFunc) gda_xml_query_init,    /* instance_init */
        NULL,                                      /* value_table */
      };
      type = g_type_register_static (GDA_TYPE_XML_FILE, "Gda_XmlQuery", &info);
    }
  return type;
}
#else
GtkType
gda_xml_query_get_type (void)
{
  static GtkType gda_xml_query_object_type = 0;

  if (!gda_xml_query_object_type)
    {
      GtkTypeInfo gda_xml_query_object_info =
      {
	"Gda_XmlQuery",
	sizeof (Gda_XmlQuery),
	sizeof (Gda_XmlQueryClass),
	(GtkClassInitFunc) gda_xml_query_class_init,
	(GtkObjectInitFunc) gda_xml_query_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL
      };
      gda_xml_query_object_type = gtk_type_unique (gda_xml_file_get_type (),
					       &gda_xml_query_object_info);
    }
  return gda_xml_query_object_type;
}
#endif

/**
 * gda_xml_query_new
 */
Gda_XmlQuery *
gda_xml_query_new (const gchar *id, Gda_XmlQueryOperation op)
{
  Gda_XmlQuery* object;
  xmlNodePtr root;
  Gda_XmlFile* xmlfile;

#ifdef HAVE_GOBJECT
  object = GDA_XML_QUERY (g_object_new (GDA_TYPE_XML_QUERY, NULL));
#else
  object = GDA_XML_QUERY(gtk_type_new(gda_xml_query_get_type()));
#endif

  xmlfile = GDA_XML_FILE(object);

  /* object->doc = xmlNewDoc("1.0"); */
/*   object->doc->root = xmlNewDocNode(object->doc, NULL, "query", NULL); */
/*   object->root = object->doc->root; */
  gda_xml_file_construct(xmlfile, "query");

  root = xmlfile->root;
  object->target = xmlNewChild(root, NULL, "target", NULL);
  object->sources = xmlNewChild(root, NULL, "sources", NULL);
  object->values = xmlNewChild(root, NULL, "values", NULL);
  object->qual = xmlNewChild(root, NULL, "qual", NULL);

  switch (op)
    {
    case GDA_XML_QUERY_SELECT:
      xmlNewProp(root, "op", "SELECT");
      break;
    case GDA_XML_QUERY_INSERT:
      xmlNewProp(root, "op", "INSERT");
      break;
    case GDA_XML_QUERY_UPDATE:
      xmlNewProp(root, "op", "UPDATE");
      break;
    case GDA_XML_QUERY_DELETE:
      xmlNewProp(root, "op", "DELETE");
      break;
    }
  object->op = op;

  if (id)
    {
      object->id = g_strdup(id);
      xmlNewProp(root, "id", id);
    }
  /* DTD */
  GDA_XML_FILE(object)->dtd = xmlCreateIntSubset(GDA_XML_FILE(object)->doc, 
						 "query", NULL, 
						 GDA_XML_QUERY_DTD_FILE);

  return object;
}

Gda_XmlQuery*  gda_xml_query_new_from_file(const gchar *filename)
{
  Gda_XmlQuery* object;
  gchar *str;

#ifdef HAVE_GOBJECT
  object = GDA_XML_QUERY (g_object_new (GDA_TYPE_XML_QUERY, NULL));
#else
  object = GDA_XML_QUERY(gtk_type_new(gda_xml_query_get_type()));
#endif

  /* DTD already done while loading. */
  GDA_XML_FILE(object)->doc = xmlParseFile(filename);
  if (GDA_XML_FILE(object)->doc)
    {
      xmlNodePtr node;
      GDA_XML_FILE(object)->root = 
	xmlDocGetRootElement(GDA_XML_FILE(object)->doc);
      node = GDA_XML_FILE(object)->root->childs;
      while (node)
	{
	  if (!strcmp(node->name, "target"))
	    object->target = node;
	  if (!strcmp(node->name, "sources"))
	    object->sources = node;
	  if (!strcmp(node->name, "values"))
	    object->values = node;
	  if (!strcmp(node->name, "qual"))
	    object->qual = node;
	  node = node->next;
	}
    }

  /* kind of operation */
  str = gda_xml_query_get_attribute(GDA_XML_FILE(object)->root, "op");
  if (str)
    {
      if(!strcmp(str, "SELECT"))
	object->op = GDA_XML_QUERY_SELECT;
      if(!strcmp(str, "INSERT"))
	object->op = GDA_XML_QUERY_INSERT;
      if(!strcmp(str, "UPDATE"))
	object->op = GDA_XML_QUERY_UPDATE;
      if(!strcmp(str, "DELETE"))
	object->op = GDA_XML_QUERY_DELETE;
      g_free(str);
    }

  /* DTD */
  GDA_XML_FILE(object)->dtd = xmlCreateIntSubset(GDA_XML_FILE(object)->doc, 
						 "query", NULL, 
						 GDA_XML_QUERY_DTD_FILE);

  return object;
}

Gda_XmlQuery*  gda_xml_query_new_from_node(const xmlNodePtr node)
{
  Gda_XmlQuery* object;
  Gda_XmlFile *fobject;
  xmlDocPtr doc;
  xmlChar *str;
  gint i;

#ifdef HAVE_GOBJECT
  object = GDA_XML_QUERY (g_object_new (GDA_TYPE_XML_QUERY, NULL));
#else
  object = GDA_XML_QUERY(gtk_type_new(gda_xml_query_get_type()));
#endif
  
  doc = xmlNewDoc("1.0");
  xmlCreateIntSubset(doc, "query", NULL, GDA_XML_QUERY_DTD_FILE);
  doc->root = xmlCopyNode(node, TRUE);
  xmlDocDumpMemory(doc, &str, &i);
  xmlFreeDoc(doc);

  /* DTD already done while loading. */
  fobject = GDA_XML_FILE(object);
  fobject->doc = xmlParseDoc(str);
  g_free(str);
  if (fobject->doc)
    {
      xmlNodePtr node;
      fobject->root = xmlDocGetRootElement(fobject->doc);
      node = fobject->root->childs;
      while (node)
	{
	  if (!strcmp(node->name, "target"))
	    object->target = node;
	  if (!strcmp(node->name, "sources"))
	    object->sources = node;
	  if (!strcmp(node->name, "values"))
	    object->values = node;
	  if (!strcmp(node->name, "qual"))
	    object->qual = node;
	  node = node->next;
	}
    }

  /* kind of operation */
  str = gda_xml_query_get_attribute(fobject->root, "op");
  if (str)
    {
      if(!strcmp(str, "SELECT"))
	object->op = GDA_XML_QUERY_SELECT;
      if(!strcmp(str, "INSERT"))
	object->op = GDA_XML_QUERY_INSERT;
      if(!strcmp(str, "UPDATE"))
	object->op = GDA_XML_QUERY_UPDATE;
      if(!strcmp(str, "DELETE"))
	object->op = GDA_XML_QUERY_DELETE;
      g_free(str);
    }

  /* DTD */
  fobject->dtd = xmlCreateIntSubset(fobject->doc, "query", NULL, 
				    GDA_XML_QUERY_DTD_FILE);
  

  return object;
}

#ifdef HAVE_GOBJECT
void
gda_xml_query_destroy (Gda_XmlQuery *object)
{
  g_return_if_fail (GDA_XML_QUERY_IS_OBJECT (object));
  g_object_unref (G_OBJECT (object));
}

void
gda_xml_query_finalize (GObject *object)
{
  Gda_XmlQuery *query = GDA_XML_QUERY (object);
  Gda_XmlQueryClass *klass =
    G_TYPE_INSTANCE_GET_CLASS (object, GDA_XML_QUERY_CLASS, Gda_XmlQueryClass);

  if (query->id)
    g_free(query->id);
  klass->parent->finalize (object);
}
#else
void
gda_xml_query_destroy (Gda_XmlQuery *object)
{
  GtkObject *parent_class = NULL;
  
  parent_class = gtk_type_class (gtk_object_get_type());
  g_return_if_fail(GDA_XML_QUERY_IS_OBJECT(object));

  if (object->id)
    g_free(object->id);

  /* for the parent class to free what's remaining */
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (GTK_OBJECT(object));
}
#endif

xmlNodePtr gda_xml_query_add(Gda_XmlQuery *q, Gda_XmlQueryTag op, ...)
{
  va_list ap;
  gboolean error=FALSE;
  xmlNodePtr node=NULL, nnode;
  gchar *name;
  Gda_XmlQuery *nq;
  Gda_XmlQueryTag nop;

  va_start(ap, op);

  if ((op == GDA_XML_QUERY_TARGET_TREE) || (op == GDA_XML_QUERY_SOURCES_TREE) ||
      (op == GDA_XML_QUERY_VALUES_TREE))
    {
      nop = va_arg(ap, Gda_XmlQueryTag);
      switch (nop)
	{
	case GDA_XML_QUERY_TABLE:
	  node = xmlNewNode(NULL, "table");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_XML_QUERY_VIEW:
	  node = xmlNewNode(NULL, "view");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_XML_QUERY_CONST:
	  node = xmlNewNode(NULL, "const");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_XML_QUERY_FIELD:
	  node = xmlNewNode(NULL, "field");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "source", name);
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_XML_QUERY_ALLFIELDS:
	  node = xmlNewNode(NULL, "allfields");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "source", name);
	  break;
	case GDA_XML_QUERY_FUNC:
	  node = xmlNewNode(NULL, "func");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_XML_QUERY_AGGREGATE:
	  node = xmlNewNode(NULL, "aggregate");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_XML_QUERY_QUERY:
	  /* the node is not created here but copied from the given query!!! */
	  nq = va_arg(ap, Gda_XmlQuery *);
	  node = xmlCopyNode(GDA_XML_FILE(nq)->root, 1);
	  break;
	default:
	  break;
	}

      /* where to put the new node ? */
      if (node)
	switch (op)
	  {
	  case GDA_XML_QUERY_TARGET_TREE:
	    if ((nop == GDA_XML_QUERY_TABLE) || (nop == GDA_XML_QUERY_VIEW))
	      xmlAddChild(q->target, node);
	    else
	      error = TRUE;
	    break;
	    
	  case GDA_XML_QUERY_SOURCES_TREE:
	    if ((nop == GDA_XML_QUERY_TABLE) || (nop == GDA_XML_QUERY_VIEW))
	      xmlAddChild(q->sources, node);
	    else
	      error = TRUE;
	    break;
	    
	  case GDA_XML_QUERY_VALUES_TREE:
	    if ((nop == GDA_XML_QUERY_CONST) || (nop == GDA_XML_QUERY_QUERY) ||
		(nop == GDA_XML_QUERY_FIELD) || (nop == GDA_XML_QUERY_FUNC) ||
		(nop == GDA_XML_QUERY_ALLFIELDS) ||
		(nop == GDA_XML_QUERY_AGGREGATE))
	      xmlAddChild(q->values, node);
	    else
	      error = TRUE;
	    break;
	    
	  default: 
	    error = TRUE;
	    break;
	  }
      else
	error = TRUE;
    }
  else /* other kind of tags */
    {
      switch (op)
	{
	case GDA_XML_QUERY_AND:
	  node = xmlNewNode(NULL, "and");
	  break;
	case GDA_XML_QUERY_OR:
	  node = xmlNewNode(NULL, "or");
	  break;
	case GDA_XML_QUERY_NOT:
	  node = xmlNewNode(NULL, "not");
	  break;
	case GDA_XML_QUERY_EQ:
	  node = xmlNewNode(NULL, "eq");
	  break;
	case GDA_XML_QUERY_NONEQ:
	  node = xmlNewNode(NULL, "noneq");
	  break;
	case GDA_XML_QUERY_INF:
	  node = xmlNewNode(NULL, "inf");
	  break;
	case GDA_XML_QUERY_INFEQ:
	  node = xmlNewNode(NULL, "infeq");
	  break;
	case GDA_XML_QUERY_SUP:
	  node = xmlNewNode(NULL, "sup");
	  break;
	case GDA_XML_QUERY_SUPEQ:
	  node = xmlNewNode(NULL, "supeq");
	  break;
	case GDA_XML_QUERY_NULL:
	  node = xmlNewNode(NULL, "null");
	  break;
	case GDA_XML_QUERY_LIKE:
	  node = xmlNewNode(NULL, "like");
	  break;
	case GDA_XML_QUERY_CONTAINS:
	  node = xmlNewNode(NULL, "contains");
	  break;
	case GDA_XML_QUERY_TABLE:
	  node = xmlNewNode(NULL, "table");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_XML_QUERY_VIEW:
	  node = xmlNewNode(NULL, "view");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_XML_QUERY_CONST:
	  node = xmlNewNode(NULL, "const");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_XML_QUERY_FIELD:
	  node = xmlNewNode(NULL, "field");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "source", name);
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_XML_QUERY_ALLFIELDS:
	  node = xmlNewNode(NULL, "field");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "source", name);
	  break;
	case GDA_XML_QUERY_FUNC:
	  node = xmlNewNode(NULL, "func");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_XML_QUERY_AGGREGATE:
	  node = xmlNewNode(NULL, "aggregate");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_XML_QUERY_QUERY:
	  /* the node is copied from the other Gda_XmlQuery */
	  nq = va_arg(ap, Gda_XmlQuery *);
	  node = xmlCopyNode(GDA_XML_FILE(nq)->root, 1);
	  break;
	default:
	  error = TRUE;
	}
      if (!error)
	{
	  nnode = va_arg(ap, xmlNodePtr);
	  if (nnode)
	    xmlAddChild(nnode, node);
	}
    }
  
  va_end(ap);
  if (error)
    {
      if (node)
	xmlFreeNode(node);
      g_warning("Unknown requested operation or error in operation");
      return NULL;
    }
  else
    return node;
}

void gda_xml_query_set_node_value(xmlNodePtr node, gchar *value)
{
  if (node)
    xmlNodeSetContent(node, value); 
}

void gda_xml_query_node_add_child(xmlNodePtr node, xmlNodePtr child)
{
  xmlAddChild(node, child);
}

void gda_xml_query_set_attribute(xmlNodePtr node, const gchar *attname,
			     const gchar *value)
{
  if (node && attname && *attname)
    xmlSetProp(node, (xmlChar *) attname, (xmlChar *) value);
}

void gda_xml_query_set_id(Gda_XmlQuery *q, xmlNodePtr node, const gchar *value)
{
  xmlAttrPtr ptr=NULL;

  if (node)
    ptr = xmlSetProp(node, "id", (xmlChar *) value);
  if (ptr)
    xmlAddID(GDA_XML_FILE(q)->context, GDA_XML_FILE(q)->doc,
	     (xmlChar *) value, ptr);
}

gchar* gda_xml_query_get_attribute(xmlNodePtr node, const gchar *attname)
{
  if (node && attname && *attname)
    return xmlGetProp(node, (xmlChar *) attname);
  else
    return NULL;
}

Gda_XmlQueryTag gda_xml_query_get_tag(xmlNodePtr node)
{
  gboolean found=FALSE;
  Gda_XmlQueryTag retval=GDA_XML_QUERY_UNKNOWN;


  if (!found && !strcmp(node->name, "table"))
    {
      retval = GDA_XML_QUERY_TABLE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "view"))
    {
      retval = GDA_XML_QUERY_VIEW;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "const"))
    {
      retval = GDA_XML_QUERY_CONST;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "field"))
    {
      retval = GDA_XML_QUERY_FIELD;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "allfields"))
    {
      retval = GDA_XML_QUERY_ALLFIELDS;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "func"))
    {
      retval = GDA_XML_QUERY_FUNC;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "aggregate"))
    {
      retval = GDA_XML_QUERY_AGGREGATE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "and"))
    {
      retval = GDA_XML_QUERY_AND;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "or"))
    {
      retval = GDA_XML_QUERY_OR;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "not"))
    {
      retval = GDA_XML_QUERY_NOT;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "eq"))
    {
      retval = GDA_XML_QUERY_EQ;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "noneq"))
    {
      retval = GDA_XML_QUERY_NONEQ;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "inf"))
    {
      retval = GDA_XML_QUERY_INF;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "infeq"))
    {
      retval = GDA_XML_QUERY_INFEQ;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "sup"))
    {
      retval = GDA_XML_QUERY_SUP;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "supeq"))
    {
      retval = GDA_XML_QUERY_SUPEQ;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "null"))
    {
      retval = GDA_XML_QUERY_NULL;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "like"))
    {
      retval = GDA_XML_QUERY_LIKE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "contains"))
    {
      retval = GDA_XML_QUERY_CONTAINS;
      found = TRUE;
    }

  /* main big tags */
  if (!found && !strcmp(node->name, "target"))
    {
      retval = GDA_XML_QUERY_TARGET_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "sources"))
    {
      retval = GDA_XML_QUERY_SOURCES_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "values"))
    {
      retval = GDA_XML_QUERY_VALUES_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "qual"))
    {
      retval = GDA_XML_QUERY_QUAL_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "query"))
    {
      retval = GDA_XML_QUERY_QUERY;
      found = TRUE;
    }

  return retval;
}

xmlNodePtr gda_xml_query_find_tag (xmlNodePtr parent, 
				   Gda_XmlQueryTag tag,
				   xmlNodePtr last_child)
{
  xmlNodePtr retval=NULL, childs;

  if (last_child)
    childs = last_child->next;
  else
    childs = parent->childs;

  while (childs && !retval)
    {
      if (gda_xml_query_get_tag(childs) == tag)
	retval = childs;
      childs = childs->next;
    }
 
  return retval;
}

/* Node rendering */
gchar *render_node(Gda_XmlQuery *q, xmlNodePtr node, gboolean with_print)
{
  Gda_XmlQueryTag tag;
  gchar *tagstr=NULL, *print=NULL, *ref, *tmp;
  xmlAttrPtr idatt;
  xmlNodePtr refnode;
  gchar *retval=NULL;
  GString *fs;

  if (with_print)
    print = gda_xml_query_get_attribute(node, "printname");
  tag =  gda_xml_query_get_tag(node);
  switch (tag) /* maybe put this as a separate tags rendering engine */
    {
    case GDA_XML_QUERY_TABLE:
    case GDA_XML_QUERY_VIEW:
      tmp =  gda_xml_query_get_attribute(node, "alias");
      if (tmp)
	{
	  tagstr = g_strdup_printf("%s %s", xmlNodeGetContent(node), tmp);
	  g_free(tmp);
	}
      else
	tagstr = g_strdup(xmlNodeGetContent(node));
      break;
    case GDA_XML_QUERY_CONST:
      tagstr = g_strdup(xmlNodeGetContent(node));
      break;
    case GDA_XML_QUERY_FIELD:
      ref = gda_xml_query_get_attribute(node, "source");
      idatt = NULL;
      if (ref)
	{
	  idatt = xmlGetID(GDA_XML_FILE(q)->doc, ref);
	  g_free(ref);
	}
      if (idatt)
	{
	  gchar *alias, *name;
	  refnode = idatt->node;
	  alias = gda_xml_query_get_attribute(refnode, "alias");
	  if (!alias)
	    alias = g_strdup(xmlNodeGetContent(refnode));
	  if (alias)
	    {
	      name = gda_xml_query_get_attribute(node, "name");
	      if (name)
		{
		  tagstr= g_strdup_printf("%s.%s", alias, name);
		  g_free(name);
		}
	      else
		tagstr = NULL;
	      g_free(alias);
	    }
	  else
	    tagstr = NULL;
	}
      break;
    case GDA_XML_QUERY_ALLFIELDS:
      ref = gda_xml_query_get_attribute(node, "source");
      idatt = NULL;
      if (ref)
	{
	  idatt = xmlGetID(GDA_XML_FILE(q)->doc, ref);
	  g_free(ref);
	}
      if (idatt)
	{
	  gchar *alias;
	  refnode = idatt->node;
	  alias = gda_xml_query_get_attribute(refnode, "alias");
	  if (!alias)
	    alias = g_strdup(xmlNodeGetContent(refnode));
	  if (alias)
	    {
	      tagstr= g_strdup_printf("%s.*", alias);
	      g_free(alias);
	    }
	}
      break;
    case GDA_XML_QUERY_AGGREGATE:
    case GDA_XML_QUERY_FUNC:
      tmp = gda_xml_query_get_attribute(node, "name");
      if (tmp)
	{
	  xmlNodePtr children;
	  gboolean start = TRUE, error = FALSE;
	  fs = g_string_new(tmp);
	  g_free(tmp);
	  g_string_append(fs, "(");
	  children = node->childs;
	  while (children)
	    {
	      gchar *str;
	      if (start)
		start = FALSE;
	      else
		g_string_append(fs, ", ");
	      str = render_node(q, children, FALSE);
	      if (str)
		{
		  g_string_append(fs, str);
		  g_free(str);
		}
	      else
		{
		  children = NULL;
		  error = TRUE;
		}
	      children = children->next;
	    }
	  g_string_append(fs, ")");
	  tagstr = fs->str;
	  g_string_free(fs, FALSE);
	  if (error)
	    {
	      g_free(tagstr);
	      tagstr = NULL;
	    }
	}
      else
	tagstr = NULL;
      break;
    case GDA_XML_QUERY_QUERY:
      {
	Gda_XmlQuery *q2;

	q2 = gda_xml_query_new_from_node(node);
	tagstr = gda_xml_file_stringify(GDA_XML_FILE(q2));
	g_free(tagstr);
	tagstr = gda_xml_query_get_standard_sql(q2);
	gda_xml_query_destroy(q2);
      }
      break;
    case GDA_XML_QUERY_NULL:
    case GDA_XML_QUERY_NOT:
      refnode = node->childs;
      tmp = render_node(q, refnode, FALSE);
      if (tmp)
	{
	  if (tag == GDA_XML_QUERY_NULL)
	    tagstr = g_strdup_printf("%s is null", tmp);
	  else
	    tagstr = g_strdup_printf("NOT %s", tmp);
	  g_free(tmp);
	}
      break;
    case GDA_XML_QUERY_EQ:
    case GDA_XML_QUERY_NONEQ:
    case GDA_XML_QUERY_INF:
    case GDA_XML_QUERY_INFEQ:
    case GDA_XML_QUERY_SUP:
    case GDA_XML_QUERY_SUPEQ:
    case GDA_XML_QUERY_LIKE:
    case GDA_XML_QUERY_CONTAINS:
      {
	gchar *larg, *rarg, *tmp;

	tmp = render_node(q, node->childs, FALSE);
	if (gda_xml_query_get_tag(node->childs) == GDA_XML_QUERY_QUERY)
	  {
	    larg = g_strdup_printf("(%s)", tmp);
	    g_free(tmp);
	  }
	else
	  larg = tmp;
	tmp = render_node(q, node->childs->next, FALSE);
	if (gda_xml_query_get_tag(node->childs->next) == GDA_XML_QUERY_QUERY)
	  {
	    rarg = g_strdup_printf("(%s)", tmp);
	    g_free(tmp);
	  }
	else
	  rarg = tmp;
	if (larg && rarg)
	  {
	    switch (tag)
	      {
	      case GDA_XML_QUERY_EQ:
		tagstr = g_strdup_printf("%s=%s", larg, rarg);
		break;
	      case GDA_XML_QUERY_NONEQ:
		tagstr = g_strdup_printf("%s!=%s", larg, rarg);
		break;
	      case GDA_XML_QUERY_INF:
		tagstr = g_strdup_printf("%s<%s", larg, rarg);
		break;
	      case GDA_XML_QUERY_INFEQ:
		tagstr = g_strdup_printf("%s<=%s", larg, rarg);
		break;
	      case GDA_XML_QUERY_SUP:
		tagstr = g_strdup_printf("%s>%s", larg, rarg);
		break;
	      case GDA_XML_QUERY_SUPEQ:
		tagstr = g_strdup_printf("%s>=%s", larg, rarg);
		break;
	      case GDA_XML_QUERY_LIKE:
		tagstr = g_strdup_printf("%s LIKE %s", larg, rarg);
		break;
	      case GDA_XML_QUERY_CONTAINS:
		tagstr = g_strdup_printf("%s CONTAINS %s", larg, rarg);
		break;
	      default:
		break;
	      }
	    g_free(larg);
	    g_free(rarg);
	  }
      }
      break;
    case GDA_XML_QUERY_AND:
    case GDA_XML_QUERY_OR:
      {
	gint i=0;
	gboolean error = FALSE;
	
	refnode = node->childs;
	fs = g_string_new("");
	while (refnode && !error)
	  {
	    tmp = render_node(q, refnode, FALSE);
	    if (tmp)
	      {
		if (i>0)
		  {
		    if (tag == GDA_XML_QUERY_AND)
		      g_string_append(fs, " AND ");
		    else
		      g_string_append(fs, " OR ");
		  }
		g_string_append(fs, tmp);
		i++;
		g_free(tmp);
	      }
	    else
	      error = TRUE;
	    refnode = refnode->next;
	  }
	if (i>1) /* then put some parentheses */
	  {
	    g_string_append(fs, ")");
	    g_string_prepend(fs, "(");
	  }
	if (!error)
	  {
	    tagstr = fs->str;
	    g_string_free(fs, FALSE);
	  }
	else
	  g_string_free(fs, TRUE);
      }
      break;
    default:
      break;
    }

  if (tagstr)
    {
      if (print)
	{
	  retval = g_strdup_printf("%s AS %s", tagstr, print);
	  g_free(print);
	  print = NULL;
	}
      else
	retval = g_strdup_printf("%s", tagstr);
    }
  else
    {
      if (print)
	g_free(print);
      retval = NULL;
    }

  /*g_print("RETVAL: %s\n", retval);*/
  return retval;
}

/* Standard SQL rendering */
gchar *gda_xml_query_get_standard_sql(Gda_XmlQuery *q)
{
  GString *qstr;
  gchar *str;
  xmlNodePtr node;
  gboolean start, error = FALSE;

  str = gda_xml_query_get_attribute(GDA_XML_FILE(q)->root, "op");
  qstr = g_string_new(str);
  switch (q->op)
    {
    case GDA_XML_QUERY_SELECT:
      node = q->values->childs;
      start = TRUE;
      while (node)
	{	  
	  gchar *str;
	  if (start)
	    {
	      start = FALSE;
	      g_string_append(qstr, " ");
	    }
	  else
	    g_string_append(qstr, ", ");
	  str = render_node(q, node, TRUE); /* rendering that node */
	  if (str)
	    {
	      g_string_append(qstr, str);
	      g_free(str);
	    }
	  else
	    error = TRUE;
	  node = node->next;
	}
      /* FROM */
      node = q->sources->childs;
      start = TRUE;
      while (node)
	{	  
	  gchar *str;
	  if (start)
	    {
	      start = FALSE;
	      g_string_append(qstr, " FROM ");
	    }
	  else
	    g_string_append(qstr, ", ");
	  str = render_node(q, node, FALSE); /* rendering that node */
	  if (str)
	    {
	      g_string_append(qstr, str);
	      g_free(str);
	    }
	  else
	    error = TRUE;
	  node = node->next;
	}
      /* WHERE */
      node = q->qual->childs;
      start = TRUE;
      while (node)
	{
	  gchar *str;
	  if (!gda_xml_query_find_tag(node, GDA_XML_QUERY_AGGREGATE, NULL))
	    {
	      if (start)
		{
		  g_string_append(qstr, " WHERE ");
		  start = FALSE;
		}
	      else
		g_string_append(qstr, " AND ");
	      str = render_node(q, node, FALSE); /* rendering that node */
	      if (str)
		{
		  g_string_append(qstr, str);
		  g_free(str);
		}
	      else
		error = TRUE;
	    }
	  node = node->next;
	}
      /* GROUP BY */
      node = q->values->childs;
      start = TRUE;
      while (node)
	{	  
	  gchar *str;
	  str = gda_xml_query_get_attribute(node, "group");
	  if (str)
	    {
	      if (!strcmp(str, "yes"))
	      g_free(str);
	      if (start)
		{
		  start = FALSE;
		  g_string_append(qstr, " GROUP BY ");
		}
	      else
		g_string_append(qstr, ", ");
	      str = gda_xml_query_get_attribute(node, "printname");
	      if (!str)
		str = gda_xml_query_get_attribute(node, "name");
	      if (str)
		{
		  g_string_append(qstr, str);
		  g_free(str);
		}
	      else
		error = TRUE;
	    }
	  node = node->next;
	}
      /* HAVING is when there is a condition on an aggregate. FIXME if it
	 is wrong */
      node = q->qual->childs;
      start = TRUE;
      while (node)
	{
	  gchar *str;
	  if (gda_xml_query_find_tag(node, GDA_XML_QUERY_AGGREGATE, NULL))
	    {
	      if (start)
		{
		  g_string_append(qstr, " HAVING ");
		  start = FALSE;
		}
	      else
		g_string_append(qstr, " AND ");
	      str = render_node(q, node, FALSE); /* rendering that node */
	      if (str)
		{
		  g_string_append(qstr, str);
		  g_free(str);
		}
	      else
		error = TRUE;
	    }
	  node = node->next;
	}      
      break;
    case GDA_XML_QUERY_INSERT:
    case GDA_XML_QUERY_DELETE:
    case GDA_XML_QUERY_UPDATE:
      break;
    }
  

  /* return the value */
  str = qstr->str;
  if (error)
    {
      g_string_free(qstr, TRUE);
      str = NULL;
    }
  else
    g_string_free(qstr, FALSE);
  return str;
}
