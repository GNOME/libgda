/* GNOME DB components libary
 * Copyright (C) 2000 Carlos Perelló Marín
 * Based on Vivien Malerba's gda-xml-query.c file
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

/*                    FIXME FIXME
 * Do some more work to make sure the right xmlValidCtxtPtr is uqed when 
 * the dociment is validated (when loading from a file, or when using
 * xmlValidateElement()
 */

#include "gda-report.h"
#define GDA_REPORT_DTD_FILE DTDINSTALLDIR"/gda-report.dtd"

static void gda_report_class_init (Gda_ReportClass *klass);
static void gda_report_init       (Gda_Report *object);

/*
 * Report object signals
 */
enum
{
  GDA_REPORT_LAST_SIGNAL
};

static gint gda_report_signals[GDA_REPORT_LAST_SIGNAL] = { 0 };

/*
 * Report object interface
 */
static void
gda_report_class_init (Gda_ReportClass *klass)
{
  GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

  object_class->destroy = (void (*)(GtkObject *)) gda_report_destroy;
}

static void
gda_report_init (Gda_Report *object)
{
  g_return_if_fail(GDA_REPORT_IS_OBJECT(object));

  object->id = NULL;
  object->ReportHeader = NULL;
  object->PageHeader = NULL;
  object->Detail = NULL;
  object->PageFooter = NULL;
  object->ReportFooter = NULL;
}

GtkType
gda_report_get_type (void)
{
  static GtkType gda_report_object_type = 0;

  if (!gda_report_object_type)
    {
      GtkTypeInfo gda_report_object_info =
      {
	"Gda_Report",
	sizeof (Gda_Report),
	sizeof (Gda_ReportClass),
	(GtkClassInitFunc) gda_report_class_init,
	(GtkObjectInitFunc) gda_report_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL
      };
      gda_report_object_type = gtk_type_unique (gda_xml_file_get_type (),
					       &gda_report_object_info);
    }
  return gda_report_object_type;
}

/**
 * gda_report_new
 */
Gda_Report *
gda_report_new (const gchar *id)
{
  Gda_Report* object;
  xmlNodePtr root;
  Gda_XmlFile* xmlfile;

  object = GDA_REPORT(gtk_type_new(gda_report_get_type()));
  xmlfile = GDA_XML_FILE(object);

  gda_xml_file_construct(xmlfile, "report");

  root = xmlfile->root;
  object->ReportHeader = xmlNewChild(root, NULL, "ReportHeader", NULL);
  object->PageHeader = xmlNewChild(root, NULL, "PageHeader", NULL);;
  object->Detail = xmlNewChild(root, NULL, "Detail", NULL);;
  object->PageFooter = xmlNewChild(root, NULL, "PageFooter", NULL);;
  object->ReportFooter = xmlNewChild(root, NULL, "ReportFooter", NULL);;

  if (id)
    {
      object->id = g_strdup(id);
      xmlNewProp(root, "id", id);
    }
  /* DTD */
  GDA_XML_FILE(object)->dtd = xmlCreateIntSubset(GDA_XML_FILE(object)->doc, 
						 "report", NULL, 
						 GDA_REPORT_DTD_FILE);

  return object;
}

Gda_Report*  gda_report_new_from_file(const gchar *filename)
{
  Gda_Report* object;
  gchar *str;

  object = GDA_REPORT(gtk_type_new(gda_report_get_type()));

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
	  if (!strcmp(node->name, "ReportHeader"))
	    object->ReportHeader = node;
	  if (!strcmp(node->name, "PageHeader"))
	    object->PageHeader = node;
	  if (!strcmp(node->name, "Detail"))
	    object->Detail = node;
	  if (!strcmp(node->name, "PageFooter"))
	    object->PageFooter = node;
	  if (!strcmp(node->name, "ReportFooter"))
	    object->ReportFooter = node;
	  node = node->next;
	}
    }

  /* DTD */
  GDA_XML_FILE(object)->dtd = xmlCreateIntSubset(GDA_XML_FILE(object)->doc, 
						 "report", NULL, 
						 GDA_REPORT_DTD_FILE);

  return object;
}

Gda_Report*  gda_report_new_from_node(const xmlNodePtr node)
{
  Gda_Report* object;
  Gda_XmlFile *fobject;
  xmlDocPtr doc;
  xmlChar *str;
  gint i;

  object = GDA_REPORT(gtk_type_new(gda_report_get_type()));
  
  doc = xmlNewDoc("1.0");
  xmlCreateIntSubset(doc, "report", NULL, GDA_REPORT_DTD_FILE);
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
	  if (!strcmp(node->name, "ReportHeader"))
	    object->ReportHeader = node;
	  if (!strcmp(node->name, "PageHeader"))
	    object->PageHeader = node;
	  if (!strcmp(node->name, "Detail"))
	    object->Detail = node;
	  if (!strcmp(node->name, "PageFooter"))
	    object->PageFooter = node;
	  if (!strcmp(node->name, "ReportFooter"))
	    object->ReportFooter = node;
	  node = node->next;
	}
    }

  /* DTD */
  fobject->dtd = xmlCreateIntSubset(fobject->doc, "report", NULL, 
				    GDA_REPORT_DTD_FILE);
  

  return object;
}

void
gda_report_destroy (Gda_Report *object)
{
  GtkObject *parent_class = NULL;
  
  parent_class = gtk_type_class (gtk_object_get_type());
  g_return_if_fail(GDA_REPORT_IS_OBJECT(object));

  if (object->id)
    g_free(object->id);

  /* for the parent class to free what's remaining */
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (GTK_OBJECT(object));
}


xmlNodePtr gda_report_add(Gda_Report *q, Gda_ReportTag location, ...)
{
  va_list ap;
  gboolean error=FALSE;
  xmlNodePtr node=NULL, nnode;
  gchar *name;
  Gda_Report *nq;
  Gda_ReportTag nop;

  va_start(ap, location);

  if ((location == GDA_REPORT_REPORTHEADER_TREE) || (location == GDA_REPORT_PAGEHEADER_TREE) ||
      (location == GDA_REPORT_DETAIL_TREE) || (location == GDA_REPORT_PAGEFOOTER_TREE) || (location == GDA_REPORT_REPORTFOOTER_TREE))
    {
      nop = va_arg(ap, Gda_ReportTag);
	/* All the instructions to construct the node aren't updated,
	 they are from gda-xml-quey files. They are wrong at this context */
      switch (nop)
	{
	case GDA_REPORT_LINE:
	  node = xmlNewNode(NULL, "line");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_REPORT_LABEL:
	  node = xmlNewNode(NULL, "label");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_REPORT_SPECIAL:
	  node = xmlNewNode(NULL, "special");
	  name = va_arg(ap, gchar *);
	  xmlNodeSetContent(node, name);
	  break;
	case GDA_REPORT_CALCULATEDFIELD:
	  node = xmlNewNode(NULL, "calculatedfield");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "source", name);
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "name", name);
	  break;
	case GDA_REPORT_FIELD:
	  node = xmlNewNode(NULL, "field");
	  name = va_arg(ap, gchar *);
	  xmlNewProp(node, "source", name);
	  break;
	default:
	  break;
	}

      /* where to put the new node ? */
      if (node)
	switch (location)
	  {
	  case GDA_REPORT_REPORTHEADER_TREE:
	    if ((nop == GDA_REPORT_LINE) || (nop == GDA_REPORT_LABEL) ||
	        (nop == GDA_REPORT_SPECIAL) || (nop == GDA_REPORT_CALCULATEDFIELD))
	      xmlAddChild(q->ReportHeader, node);
	    else
	      error = TRUE;
	    break;
	    
	  case GDA_REPORT_PAGEHEADER_TREE:
	    if ((nop == GDA_REPORT_LINE) || (nop == GDA_REPORT_LABEL) ||
	        (nop == GDA_REPORT_SPECIAL) || (nop == GDA_REPORT_CALCULATEDFIELD))
	      xmlAddChild(q->PageHeader, node);
	    else
	      error = TRUE;
	    break;
	    
	  case GDA_REPORT_DETAIL_TREE:
	    if ((nop == GDA_REPORT_LINE) || (nop == GDA_REPORT_LABEL) || (nop == GDA_REPORT_SPECIAL)
		|| (nop == GDA_REPORT_CALCULATEDFIELD) || (nop == GDA_REPORT_FIELD))
	      xmlAddChild(q->Detail, node);
	    else
	      error = TRUE;
	    break;
	    
	  case GDA_REPORT_PAGEFOOTER_TREE:
	    if ((nop == GDA_REPORT_LINE) || (nop == GDA_REPORT_LABEL) ||
	        (nop == GDA_REPORT_SPECIAL) || (nop == GDA_REPORT_CALCULATEDFIELD))
	      xmlAddChild(q->PageFooter, node);
	    else
	      error = TRUE;
	    break;

	  case GDA_REPORT_REPORTFOOTER_TREE:
	    if ((nop == GDA_REPORT_LINE) || (nop == GDA_REPORT_LABEL) ||
	        (nop == GDA_REPORT_SPECIAL) || (nop == GDA_REPORT_CALCULATEDFIELD))
	      xmlAddChild(q->ReportFooter, node);
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
    error = TRUE;
  
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

void gda_report_set_node_value(xmlNodePtr node, gchar *value)
{
  if (node)
    xmlNodeSetContent(node, value); 
}

void gda_report_node_add_child(xmlNodePtr node, xmlNodePtr child)
{
  xmlAddChild(node, child);
}

void gda_report_set_attribute(xmlNodePtr node, const gchar *attname,
					  const gchar *value)
{
  if (node && attname && *attname)
    xmlSetProp(node, (xmlChar *) attname, (xmlChar *) value);
}

void gda_report_set_id(Gda_Report *q, xmlNodePtr node, const gchar *value)
{
  xmlAttrPtr ptr=NULL;

  if (node)
    ptr = xmlSetProp(node, "id", (xmlChar *) value);
  if (ptr)
    xmlAddID(GDA_XML_FILE(q)->context, GDA_XML_FILE(q)->doc,
	     (xmlChar *) value, ptr);
}

gchar* gda_report_get_attribute(xmlNodePtr node, const gchar *attname)
{
  if (node && attname && *attname)
    return xmlGetProp(node, (xmlChar *) attname);
  else
    return NULL;
}

Gda_ReportTag gda_report_get_tag(xmlNodePtr node)
{
  gboolean found=FALSE;
  Gda_ReportTag retval=GDA_REPORT_UNKNOWN;


  if (!found && !strcmp(node->name, "line"))
    {
      retval = GDA_REPORT_LINE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "label"))
    {
      retval = GDA_REPORT_LABEL;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "special"))
    {
      retval = GDA_REPORT_SPECIAL;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "calculatedfield"))
    {
      retval = GDA_REPORT_CALCULATEDFIELD;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "field"))
    {
      retval = GDA_REPORT_FIELD;
      found = TRUE;
    }

  /* main big tags */
  if (!found && !strcmp(node->name, "ReportHeader"))
    {
      retval = GDA_REPORT_REPORTHEADER_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "PageHeader"))
    {
      retval = GDA_REPORT_PAGEHEADER_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "Detail"))
    {
      retval = GDA_REPORT_DETAIL_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "PageFooter"))
    {
      retval = GDA_REPORT_PAGEFOOTER_TREE;
      found = TRUE;
    }
  if (!found && !strcmp(node->name, "ReportFooter"))
    {
      retval = GDA_REPORT_REPORTFOOTER_TREE;
      found = TRUE;
    }

  return retval;
}

xmlNodePtr gda_report_find_tag (xmlNodePtr parent, 
						Gda_ReportTag tag,
						xmlNodePtr last_child)
{
  xmlNodePtr retval=NULL, childs;

  if (last_child)
    childs = last_child->next;
  else
    childs = parent->childs;

  while (childs && !retval)
    {
      if (gda_report_get_tag(childs) == tag)
	retval = childs;
      childs = childs->next;
    }
 
  return retval;
}