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

#include "config.h"
#include "gda-xml-document.h"
#include <glib-object.h>

static void gda_xml_document_class_init (GdaXmlDocumentClass *klass);
static void gda_xml_document_init       (GdaXmlDocument *xmldoc, GdaXmlDocumentClass *klass);
static void gda_xml_document_finalize   (GObject * object);

/* errors handling */
static void (gda_xml_document_error_def) (void *ctx, const char *msg, ...);
static void (gda_xml_document_warn_def) (void *ctx, const char *msg, ...);

enum {
	GDA_XML_DOCUMENT_WARNING,
	GDA_XML_DOCUMENT_ERROR,
	GDA_XML_DOCUMENT_LAST_SIGNAL
};

static gint gda_xml_document_signals[GDA_XML_DOCUMENT_LAST_SIGNAL] = { 0, };
static GObjectClass *parent_class = NULL;

/*
 * GdaXmlDocument object implementation
 */
static void
gda_xml_document_class_init (GdaXmlDocumentClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_xml_document_signals[GDA_XML_DOCUMENT_WARNING] =
		g_signal_new ("warning",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaXmlDocumentClass, warning),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	gda_xml_document_signals[GDA_XML_DOCUMENT_ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaXmlDocumentClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	klass->warning = NULL;
	klass->error = NULL;

	object_class->finalize = gda_xml_document_finalize;
}

static void
gda_xml_document_init (GdaXmlDocument *xmldoc, GdaXmlDocumentClass *klass)
{
	/* might change in future versions of libxml */
	extern int xmlDoValidityCheckingDefaultValue;
	xmlDoValidityCheckingDefaultValue = 1;

	g_return_if_fail (GDA_IS_XML_DOCUMENT (xmldoc));

	xmldoc->doc = NULL;
	xmldoc->dtd = NULL;
	xmldoc->root = NULL;
	xmldoc->context = NULL;
}

static void
gda_xml_document_finalize (GObject *object)
{
	GdaXmlDocument *xmldoc = (GdaXmlDocument *) object;

	g_return_if_fail (GDA_IS_XML_DOCUMENT (xmldoc));

	xmlFreeDoc (xmldoc->doc);
	xmldoc->doc = NULL;

	parent_class->finalize (object);
}

GType
gda_xml_document_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlDocumentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xml_document_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlDocument),
			0,
			(GInstanceInitFunc) gda_xml_document_init
		};
		type = g_type_register_static (G_TYPE_OBJECT,
					       "GdaXmlDocument",
					       &info, 0);
	}
	return type;
}

/**
 * gda_xml_document_new
 * @root_doc: root document new
 *
 * Create a new #GdaXmlDocument object, with a root document of type @root_doc
 */
GdaXmlDocument *
gda_xml_document_new (const gchar * root_doc)
{
	GdaXmlDocument *xmldoc;

	xmldoc = GDA_XML_DOCUMENT (g_object_new (GDA_TYPE_XML_DOCUMENT, NULL));
	gda_xml_document_construct (xmldoc, root_doc);

	return xmldoc;
}

void
gda_xml_document_construct (GdaXmlDocument * xmldoc, const gchar * root_doc)
{
	/* initialize XML document */
	xmldoc->doc = xmlNewDoc ("1.0");
	xmldoc->root = xmlNewDocNode (xmldoc->doc, NULL, root_doc, NULL);
	xmlDocSetRootElement (xmldoc->doc, xmldoc->root);

	xmldoc->context = g_new0 (xmlValidCtxt, 1);
	xmldoc->context->userData = xmldoc;
	xmldoc->context->error = gda_xml_document_error_def;
	xmldoc->context->warning = gda_xml_document_warn_def;
}

/**
 * gda_xml_document_get_compress_mode
 * @xmldoc: a #GdaXmlDocument object
 *
 * Returns the compression mode being used by the given
 * XML document
 */
gint
gda_xml_document_get_compress_mode (GdaXmlDocument *xmldoc)
{
	g_return_val_if_fail (GDA_IS_XML_DOCUMENT (xmldoc), -1);
	return xmlGetDocCompressMode (xmldoc->doc);
}

/**
 * gda_xml_document_set_compress_mode
 */
void
gda_xml_document_set_compress_mode (GdaXmlDocument *xmldoc, gint mode)
{
	g_return_if_fail (GDA_IS_XML_DOCUMENT (xmldoc));
	xmlSetDocCompressMode (xmldoc->doc, mode);
}

/**
 * gda_xml_document_to_file
 * @xmldoc: a #GdaXmlDocument object.
 * @uri: URI of the resulting file.
 *
 * Saves the given #GdaXmlDocument into a disk file. That is, it
 * translates the in-memory document structure, transforms it to
 * XML and saves, in the given file, the resulting XML output.
 *
 * Returns: TRUE if successful, FALSE on error.
 */
gboolean
gda_xml_document_to_file (GdaXmlDocument *xmldoc, const gchar *uri)
{
	gchar *body;
	gboolean rc;

	g_return_val_if_fail (GDA_IS_XML_DOCUMENT (xmldoc), FALSE);
	g_return_val_if_fail ((uri != NULL), FALSE);

	body = gda_xml_document_stringify (xmldoc);
	rc = gda_file_save (uri, body, strlen (body));
	g_free (body);

	return rc;
}

/**
 * gda_xml_document_stringify
 */
gchar *
gda_xml_document_stringify (GdaXmlDocument *xmldoc)
{
	xmlChar *str;
	gint i;

	g_return_val_if_fail (GDA_IS_XML_DOCUMENT (xmldoc), NULL);

	xmlDocDumpMemory (xmldoc->doc, &str, &i);
	return str;
}

/* FIXME: signals in preparation for future use. Will work when I understand 
   how validation is done with libxml. */
static void (gda_xml_document_error_def) (void *ctx, const char *msg, ...)
{
	g_print ("ERR SIG\n");
	g_signal_emit (G_OBJECT (((xmlValidCtxtPtr) ctx)->userData),
		       gda_xml_document_signals[GDA_XML_DOCUMENT_ERROR],
		       0, msg);
}

static void (gda_xml_document_warn_def) (void *ctx, const char *msg, ...)
{
	g_print ("WARN SIG\n");
	g_signal_emit (G_OBJECT (((xmlValidCtxtPtr) ctx)->userData),
		       gda_xml_document_signals[GDA_XML_DOCUMENT_ERROR],
		       0, msg);
}
