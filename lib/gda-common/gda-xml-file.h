/* GNOME DB library
 * Copyright (C) 1999, 2000 Rodrigo Moya
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

#if !defined(__gda_xml_file_h__)
#  define __gda_xml_file_h__

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/valid.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_XmlFile      Gda_XmlFile;
typedef struct _Gda_XmlFileClass Gda_XmlFileClass;

#define GDA_TYPE_XML_FILE            (gda_xml_file_get_type())
#define GDA_XML_FILE(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_FILE, Gda_XmlFile)
#define GDA_XML_FILE_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_FILE, Gda_XmlFileClass)
#define GDA_IS_XML_FILE(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_FILE)
#define GDA_IS_XML_FILE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_FILE))

struct _Gda_XmlFile
{
  GtkObject       object;
  xmlDocPtr       doc;
  xmlDtdPtr       dtd;
  xmlNodePtr      root;
  xmlValidCtxtPtr context;
};

struct _Gda_XmlFileClass
{
  GtkObjectClass parent_class;

  void (* warning) (Gda_XmlFile *q, const char *msg);
  void (* error)   (Gda_XmlFile *q, const char *msg);
};

GtkType      gda_xml_file_get_type      (void);
Gda_XmlFile* gda_xml_file_new           (const gchar *root_doc);
/*Gda_XmlFile* gda_xml_file_new_from_file (const gchar *filename);*/

/* output the structure */
gint           gda_xml_file_to_file          (Gda_XmlFile *f, 
					      const gchar *filename);
gchar*         gda_xml_file_stringify        (Gda_XmlFile *f);

/* 
 * Used by objects inheriting this one. Do not call directely
 */
void         gda_xml_file_construct(Gda_XmlFile *xmlfile, 
				    const gchar *root_doc);

#if defined(__cplusplus)
}
#endif

#endif
