/* GNOME DB components libary
 * Copyright (C) 2000 Carlos Perelló Marín
 * Based on Vivien Malerba's gda-xml-query.h file
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

#if !defined(__gda_report_h__)
#define __gda_report_h__

#include <gnome.h>
#include "gda-xml-file.h"
#include "gda-xml-query.h"

BEGIN_GNOME_DECLS

#define GDA_REPORT(obj)            GTK_CHECK_CAST(obj, gda_report_get_type(), Gda_Report)
#define GDA_REPORT_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, gda_report_get_type(), Gda_ReportClass)
#define GDA_REPORT_IS_OBJECT(obj)  GTK_CHECK_TYPE(obj, gda_report_get_type())

typedef struct _Gda_Report      Gda_Report;
typedef struct _Gda_ReportClass Gda_ReportClass;

typedef enum {
  /* ident for the tags */
  GDA_REPORT_LINE,
  GDA_REPORT_LABEL,
  GDA_REPORT_SPECIAL,
  GDA_REPORT_CALCULATEDFIELD,
  GDA_REPORT_FIELD,
  GDA_REPORT_REPORTHEADER_TREE,
  GDA_REPORT_PAGEHEADER_TREE,
  GDA_REPORT_DETAIL_TREE,
  GDA_REPORT_PAGEFOOTER_TREE,
  GDA_REPORT_REPORTFOOTER_TREE,
  GDA_REPORT_UNKNOWN
} Gda_ReportTag;


struct _Gda_Report
{
  Gda_XmlFile              obj;

  gchar                   *id;
  xmlNodePtr               ReportHeader;
  xmlNodePtr               PageHeader;
  xmlNodePtr               Detail;
  xmlNodePtr               PageFooter;
  xmlNodePtr               ReportFooter;
};

struct _Gda_ReportClass
{
  Gda_XmlFileClass parent_class;
};

GtkType        gda_report_get_type         (void);
Gda_Report*    gda_report_new              (const gchar *id);
/* create and load from file */
Gda_Report*    gda_report_new_from_file    (const gchar *filename);
/* create from a node of a bigger XML doc */
Gda_Report*    gda_report_new_from_node    (const xmlNodePtr node);
/* destroy */
void           gda_report_destroy          (Gda_Report *q);


/* contents manipulation */
xmlNodePtr     gda_report_add              (Gda_Report *q, 
									Gda_ReportTag location, ...);
void           gda_report_node_add_child   (xmlNodePtr node, 
									xmlNodePtr child);
void           gda_report_set_attribute    (xmlNodePtr node, 
									const gchar *attname,
									const gchar *value);
void           gda_report_set_id           (Gda_Report *q,
									xmlNodePtr node, 
									const gchar *value);
gchar*         gda_report_get_attribute    (xmlNodePtr node, 
									const gchar *attname);
void           gda_report_set_node_value   (xmlNodePtr node, gchar *value);

Gda_XmlQueryTag gda_report_get_tag         (xmlNodePtr node);

xmlNodePtr     gda_report_find_tag         (xmlNodePtr parent, 
									Gda_ReportTag tag,
									xmlNodePtr last_child);
END_GNOME_DECLS

#endif