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

#include <gda-report.h>
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
  object->queries = NULL;
  object->ReportHeader = NULL;
  object->PageHeaderList = NULL;
  object->DataHeader = NULL;
  object->DataList = NULL;
  object->DataFooter = NULL;
  object->PageFooterList = NULL;
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
//  object->queries = g_hash_table_new ((*GHashFunc) g_str_hash, (*GCompareFunc) g_str_equal);
  object->ReportHeader = xmlNewChild(root, NULL, "ReportHeader", NULL);
  object->PageHeaderList = xmlNewChild(root, NULL, "PageHeaderList", NULL);
  object->DataHeader = xmlNewChild(root, NULL, "DataHeader", NULL);
  object->DataList = xmlNewChild(root, NULL, "DataList", NULL);
  object->DataFooter = xmlNewChild(root, NULL, "DataFooter", NULL);
  object->PageFooterList = xmlNewChild(root, NULL, "PageFooterList", NULL);
  object->ReportFooter = xmlNewChild(root, NULL, "ReportFooter", NULL);;

  if (id) {
    object->id = g_strdup(id);
    xmlNewProp(root, "id", id);
  }
  /* DTD */
  GDA_XML_FILE(object)->dtd = xmlCreateIntSubset(GDA_XML_FILE(object)->doc, 
                                                 "report", NULL, 
                                                 GDA_REPORT_DTD_FILE);

  return object;
}

Gda_Report*  gda_report_new_from_file(const gchar *filename) {
Gda_Report* object;
gchar *str;

  object = GDA_REPORT(gtk_type_new(gda_report_get_type()));

  /* DTD already done while loading. */
  GDA_XML_FILE(object)->doc = xmlParseFile(filename);
  if (GDA_XML_FILE(object)->doc) {
    xmlNodePtr node;
    GDA_XML_FILE(object)->root = xmlDocGetRootElement(GDA_XML_FILE(object)->doc);
    node = GDA_XML_FILE(object)->root->childs;
    while (node) {
      if (!strcmp(node->name, "ReportHeader"))
        object->ReportHeader = node;
      if (!strcmp(node->name, "PageHeaderList"))
        object->PageHeaderList = node;
      if (!strcmp(node->name, "DataHeader"))
        object->DataHeader = node;
      if (!strcmp(node->name, "DataList"))
        object->DataList = node;
      if (!strcmp(node->name, "DataFooter"))
        object->DataFooter = node;
      if (!strcmp(node->name, "PageFooterList"))
        object->PageFooterList = node;
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
          if (!strcmp(node->name, "PageHeaderList"))
            object->PageHeaderList = node;
          if (!strcmp(node->name, "DataHeader"))
            object->DataHeader = node;
          if (!strcmp(node->name, "DataList"))
            object->DataList = node;
          if (!strcmp(node->name, "DataFooter"))
             object->DataFooter = node;
           if (!strcmp(node->name, "PageFooterList"))
            object->PageFooterList = node;
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


xmlNodePtr gda_report_add(Gda_Report *r, Gda_ReportTag location, ...) {
va_list ap;
gboolean error=FALSE;
xmlNodePtr node=NULL, nnode;
gchar *value;
Gda_Report *nr;
Gda_ReportTag nlocation;

  va_start(ap, location);
 
  if ((location == GDA_REPORT_REPORT_HEADER) || (location == GDA_REPORT_PAGE_HEADER_LIST) ||
      (location == GDA_REPORT_DATA_HEADER) || (location == GDA_REPORT_DATA_LIST) ||
      (location == GDA_REPORT_DATA_FOOTER) || (location == GDA_REPORT_PAGE_FOOTER_LIST) ||
      (location == GDA_REPORT_REPORT_FOOTER)) {
    nlocation = va_arg(ap, Gda_ReportTag);
    switch (nlocation) {
      case GDA_REPORT_REPORT_ELEMENT_LIST:
        node = xmlNewNode(NULL, "reportelementlist");
        break;
      case GDA_REPORT_REPORT_DATA:
        node = xmlNewNode(NULL, "reportdata");
        break;
      default:
        break;
    }

    /* where to put the new node ? */
    if (node)
      switch (location) {
        case GDA_REPORT_REPORT_HEADER:
          if (nlocation == GDA_REPORT_REPORT_ELEMENT_LIST)
            xmlAddChild(r->ReportHeader, node);
          else
            error = TRUE;
          break;
        case GDA_REPORT_PAGE_HEADER_LIST:
          if (nlocation == GDA_REPORT_REPORT_ELEMENT_LIST)
            xmlAddChild(r->PageHeaderList, node);
          else
            error = TRUE;
          break;
        case GDA_REPORT_DATA_HEADER:
          if (nlocation == GDA_REPORT_REPORT_ELEMENT_LIST)
            xmlAddChild(r->DataHeader, node);
          else
            error = TRUE;
          break;
        case GDA_REPORT_DATA_LIST:
          if (nlocation == GDA_REPORT_REPORT_DATA)
            xmlAddChild(r->DataList, node);
          else
            error = TRUE;
          break;
        case GDA_REPORT_DATA_FOOTER:
          if (nlocation == GDA_REPORT_REPORT_ELEMENT_LIST)
            xmlAddChild(r->DataFooter, node);
          else
            error = TRUE;
          break;
        case GDA_REPORT_PAGE_FOOTER_LIST:
          if (nlocation == GDA_REPORT_REPORT_ELEMENT_LIST)
            xmlAddChild(r->PageFooterList, node);
          else
            error = TRUE;
          break;
        case GDA_REPORT_REPORT_FOOTER:
          if (nlocation == GDA_REPORT_REPORT_ELEMENT_LIST)
            xmlAddChild(r->ReportFooter, node);
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
    switch (nlocation) {
      case GDA_REPORT_LINE:
        node = xmlNewNode(NULL, "line");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "x1", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "y1", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "x2", value);        
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "y2", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "linewidth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "lineheight", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "linecolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "linestyle", value);
        break;
      case GDA_REPORT_LABEL:
        node = xmlNewNode(NULL, "label");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "text", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "x", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "y", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "width", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "bgcolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fgcolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "bordercolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "borderwidth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "borderstyle", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontfamily", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontsize", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontweigth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontitalic", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "halignment", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "valignment", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "wordwrap", value);
        break;
      case GDA_REPORT_SPECIAL:
        node = xmlNewNode(NULL, "special");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "text", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "x", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "y", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "width", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "bgcolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fgcolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "bordercolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "borderwidth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "borderstyle", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontfamily", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontsize", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontweigth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontitalic", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "halignment", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "valignment", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "wordwrap", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "dateformat", value);
        break;
      case GDA_REPORT_REPFIELD:
        node = xmlNewNode(NULL, "repfield");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "query", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "value", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "x", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "y", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "width", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "bgcolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fgcolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "bordercolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "borderwidth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "borderstyle", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontfamily", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontsize", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontweigth", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "fontitalic", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "halignment", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "valignment", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "wordwrap", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "dateformat", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "precision", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "currency", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "negvaluecolor", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "commaseparator", value);
        break;
      case GDA_REPORT_PICTURE:
        node = xmlNewNode(NULL, "picture");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "x", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "y", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "width", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "size", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "aspecratio", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "format", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "source", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "data", value);
        break;
      case GDA_REPORT_PAGE_HEADER:
        node = xmlNewNode(NULL, "pageheader");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "positionfreq", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "pagefreq", value);
        break;
      case GDA_REPORT_PAGE_FOOTER:
        node = xmlNewNode(NULL, "pagefooter");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "positionfreq", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "pagefreq", value);
        break;
      case GDA_REPORT_DETAIL:
        node = xmlNewNode(NULL, "detail");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        break;
      case GDA_REPORT_GROUP_HEADER:
        node = xmlNewNode(NULL, "groupheader");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "newpage", value);
        break;
      case GDA_REPORT_GROUP_FOOTER:
        node = xmlNewNode(NULL, "groupfooter");
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "height", value);
        value = va_arg(ap, gchar * );
        xmlNewProp(node, "newpage", value);
        break;
      default:
        error = TRUE;
        break;
    }
  if (!error) {
	  nnode = va_arg(ap, xmlNodePtr);
	  if (nnode)
	    xmlAddChild(nnode, node);
	}
  
  va_end(ap);
  if (error) {
    if (node)
      xmlFreeNode(node);
    g_warning("Unknown requested operation or error in operation");
    return NULL;
  } else
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

void gda_report_set_id(Gda_Report *r, xmlNodePtr node, const gchar *value)
{
  xmlAttrPtr ptr=NULL;

  if (node)
    ptr = xmlSetProp(node, "id", (xmlChar *) value);
  if (ptr)
    xmlAddID(GDA_XML_FILE(r)->context, GDA_XML_FILE(r)->doc,
       (xmlChar *) value, ptr);
}

gchar* gda_report_get_attribute(xmlNodePtr node, const gchar *attname)
{
  if (node && attname && *attname)
    return xmlGetProp(node, (xmlChar *) attname);
  else
    return NULL;
}

Gda_ReportTag gda_report_get_tag(xmlNodePtr node) {
gboolean found=FALSE;
Gda_ReportTag retval=GDA_REPORT_UNKNOWN;

  if (!found && !strcmp(node->name, "reportelementlist")) {
    retval = GDA_REPORT_REPORT_ELEMENT_LIST;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "reportdata")) {
    retval = GDA_REPORT_REPORT_DATA;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "pageheader")) {
    retval = GDA_REPORT_PAGE_HEADER;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "pagefooter")) {
    retval = GDA_REPORT_PAGE_FOOTER;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "line")) {
    retval = GDA_REPORT_LINE;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "label")) {
    retval = GDA_REPORT_LABEL;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "special")) {
    retval = GDA_REPORT_SPECIAL;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "repfield")) {
    retval = GDA_REPORT_REPFIELD;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "picture")) {
    retval = GDA_REPORT_PICTURE;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "groupheader")) {
    retval = GDA_REPORT_GROUP_HEADER;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "detail")) {
    retval = GDA_REPORT_DETAIL;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "groupfooter")) {
    retval = GDA_REPORT_GROUP_FOOTER;
    found = TRUE;
  }

  /* main big tags */
  if (!found && !strcmp(node->name, "ReportHeader")) {
    retval = GDA_REPORT_REPORT_HEADER;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "PageHeaderList")) {
    retval = GDA_REPORT_PAGE_HEADER_LIST;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "DataHeader")) {
    retval = GDA_REPORT_DATA_HEADER;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "DataList")) {
    retval = GDA_REPORT_DATA_LIST;
    found = TRUE;
  }
    if (!found && !strcmp(node->name, "DataFooter")) {
    retval = GDA_REPORT_DATA_FOOTER;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "PageFooterList")) {
    retval = GDA_REPORT_PAGE_FOOTER_LIST;
    found = TRUE;
  }
  if (!found && !strcmp(node->name, "ReportFooter")) {
    retval = GDA_REPORT_REPORT_FOOTER;
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