/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/valid.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_XML_DOCUMENT            (gda_xml_document_get_type())
#define GDA_XML_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_DOCUMENT, GdaXmlDocument))
#define GDA_XML_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_DOCUMENT, GdaXmlDocumentClass))
#define GDA_IS_XML_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_DOCUMENT))
#define GDA_IS_XML_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_XML_DOCUMENT))

typedef struct _GdaXmlDocument GdaXmlDocument;
typedef struct _GdaXmlDocumentClass GdaXmlDocumentClass;

struct _GdaXmlDocument {
	GObject object;

	xmlDocPtr doc;
	xmlDtdPtr dtd;
	xmlNodePtr root;
	xmlValidCtxtPtr context;
};

struct _GdaXmlDocumentClass {
	GObjectClass parent_class;

	void (*warning) (GdaXmlDocument * q, const char *msg);
	void (*error) (GdaXmlDocument * q, const char *msg);
};

GType           gda_xml_document_get_type (void);

GdaXmlDocument *gda_xml_document_new (const gchar *root_doc);
void            gda_xml_document_construct (GdaXmlDocument *xmlfile,
					    const gchar *root_doc);

gint            gda_xml_document_get_compress_mode (GdaXmlDocument *xmldoc);
void            gda_xml_document_set_compress_mode (GdaXmlDocument *xmldoc, gint mode);

gboolean        gda_xml_document_to_file (GdaXmlDocument *xmldoc,
					  const gchar *uri);
gchar          *gda_xml_document_stringify (GdaXmlDocument *xmldoc);

G_END_DECLS

#endif
