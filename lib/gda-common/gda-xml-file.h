/* GDA library
 * Copyright (C) 1999, 2000 Rodrigo Moya
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

#if !defined(__gda_xml_file_h__)
#  define __gda_xml_file_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif

#include <tree.h>
#include <parser.h>
#include <valid.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _GdaXmlFile      GdaXmlFile;
typedef struct _GdaXmlFileClass GdaXmlFileClass;

#define GDA_TYPE_XML_FILE            (gda_xml_file_get_type())
#ifdef HAVE_GOBJECT
#  define GDA_XML_FILE(obj) \
          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_FILE, GdaXmlFile)
#  define GDA_XML_FILE_CLASS(klass) \
          G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_FILE, GdaXmlFileClass)
#  define GDA_IS_XML_FILE(obj) \
          G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_FILE)
#  define GDA_IS_XML_FILE_CLASS(klass) \
          GTK_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XML_FILE)
#else
#  define GDA_XML_FILE(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_FILE, GdaXmlFile)
#  define GDA_XML_FILE_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_FILE, GdaXmlFileClass)
#  define GDA_IS_XML_FILE(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_FILE)
#  define GDA_IS_XML_FILE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_FILE))
#endif

struct _GdaXmlFile
{
#ifdef HAVE_GOBJECT
  GObject         object;
#else
  GtkObject       object;
#endif
  xmlDocPtr       doc;
  xmlDtdPtr       dtd;
  xmlNodePtr      root;
  xmlValidCtxtPtr context;
};

struct _GdaXmlFileClass
{
#ifdef HAVE_GOBJECT
  GObjectClass  parent_class;
  GObjectClass *parent;
#else
  GtkObjectClass parent_class;
#endif

  void (* warning) (GdaXmlFile *q, const char *msg);
  void (* error)   (GdaXmlFile *q, const char *msg);
};

#ifdef HAVE_GOBJECT
GType        gda_xml_file_get_type      (void);
#else
GtkType      gda_xml_file_get_type      (void);
#endif

GdaXmlFile* gda_xml_file_new           (const gchar *root_doc);
/*GdaXmlFile* gda_xml_file_new_from_file (const gchar *filename);*/

/* output the structure */
gint           gda_xml_file_to_file          (GdaXmlFile *f, 
					      const gchar *filename);
gchar*         gda_xml_file_stringify        (GdaXmlFile *f);

/* 
 * Used by objects inheriting this one. Do not call directely
 */
void         gda_xml_file_construct(GdaXmlFile *xmlfile, 
				    const gchar *root_doc);

#if defined(__cplusplus)
}
#endif

#endif
