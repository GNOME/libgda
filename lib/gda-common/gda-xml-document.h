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
#include <gtk/gtkobject.h>
#include <gda-common-defs.h>
#include <tree.h>
#include <parser.h>
#include <valid.h>

G_BEGIN_DECLS

typedef struct _GdaXmlDocument GdaXmlDocument;
typedef struct _GdaXmlDocumentClass GdaXmlDocumentClass;

#define GDA_TYPE_XML_DOCUMENT            (gda_xml_document_get_type())
#define GDA_XML_DOCUMENT(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XML_DOCUMENT, GdaXmlDocument)
#define GDA_XML_DOCUMENT_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XML_DOCUMENT, GdaXmlDocumentClass)
#define GDA_IS_XML_DOCUMENT(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XML_DOCUMENT)
#define GDA_IS_XML_DOCUMENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_DOCUMENT))

struct _GdaXmlDocument {
	GtkObject object;

	xmlDocPtr doc;
	xmlDtdPtr dtd;
	xmlNodePtr root;
	xmlValidCtxtPtr context;
};

struct _GdaXmlDocumentClass {
	GtkObjectClass parent_class;

	void (*warning) (GdaXmlDocument * q, const char *msg);
	void (*error) (GdaXmlDocument * q, const char *msg);
};

GtkType         gda_xml_document_get_type (void);

GdaXmlDocument *gda_xml_document_new (const gchar * root_doc);
void            gda_xml_document_construct (GdaXmlDocument * xmlfile,
					    const gchar * root_doc);
/*GdaXmlDocument* gda_xml_document_new_from_file (const gchar *filename);*/

gint            gda_xml_document_get_compress_mode (GdaXmlDocument *xmldoc);
void            gda_xml_document_set_compress_mode (GdaXmlDocument *xmldoc, gint mode);

gint            gda_xml_document_to_file (GdaXmlDocument * xmldoc,
					  const gchar * filename);
gchar          *gda_xml_document_stringify (GdaXmlDocument * xmldoc);

G_END_DECLS

#endif
