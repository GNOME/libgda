/* GDA common library
 * Copyright (C) 1999-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Vivien Malerba <malerba@gnome-db.org>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_xml_document_h__)
#  define __gda_xml_document_h__

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

typedef struct _GdaXmlDocument      GdaXmlDocument;
typedef struct _GdaXmlDocumentClass GdaXmlDocumentClass;

#define GDA_TYPE_XML_DOCUMENT            (gda_xml_document_get_type())
#ifdef HAVE_GOBJECT
#define GDA_XML_DOCUMENT(obj)            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_DOCUMENT, GdaXmlDocument)
#define GDA_XML_DOCUMENT_CLASS(klass)    G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_DOCUMENT, GdaXmlDocumentClass)
#define GDA_IS_XML_DOCUMENT(obj)         G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_DOCUMENT)
#define GDA_IS_XML_DOCUMENT_CLASS(klass) GTK_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XML_DOCUMENT)
#else
#define GDA_XML_DOCUMENT(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_DOCUMENT, GdaXmlDocument)
#define GDA_XML_DOCUMENT_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_DOCUMENT, GdaXmlDocumentClass)
#define GDA_IS_XML_DOCUMENT(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_DOCUMENT)
#define GDA_IS_XML_DOCUMENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_DOCUMENT))
#endif

struct _GdaXmlDocument {
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

struct _GdaXmlDocumentClass
{
#ifdef HAVE_GOBJECT
	GObjectClass  parent_class;
	GObjectClass *parent;
#else
	GtkObjectClass parent_class;
#endif

	void (* warning) (GdaXmlDocument *q, const char *msg);
	void (* error)   (GdaXmlDocument *q, const char *msg);
};

#ifdef HAVE_GOBJECT
GType        gda_xml_document_get_type (void);
#else
GtkType      gda_xml_document_get_type (void);
#endif

GdaXmlDocument* gda_xml_document_new (const gchar *root_doc);
void            gda_xml_document_construct (GdaXmlDocument *xmlfile, const gchar *root_doc);
/*GdaXmlDocument* gda_xml_document_new_from_file (const gchar *filename);*/

/* output the structure */
gint   gda_xml_document_to_file (GdaXmlDocument *xmldoc, const gchar *filename);
gchar* gda_xml_document_stringify (GdaXmlDocument *xmldoc);

#if defined(__cplusplus)
}
#endif

#endif
