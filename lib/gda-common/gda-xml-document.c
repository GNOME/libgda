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

#include "config.h"
#include "gda-xml-document.h"
#include <gtk/gtksignal.h>

static void gda_xml_document_destroy (GtkObject * object);

/* errors handling */
static void (gda_xml_document_error_def) (void *ctx, const char *msg, ...);
static void (gda_xml_document_warn_def) (void *ctx, const char *msg, ...);
enum
{
	GDA_XML_DOCUMENT_WARNING,
	GDA_XML_DOCUMENT_ERROR,
	GDA_XML_DOCUMENT_LAST_SIGNAL
};

static gint gda_xml_document_signals[GDA_XML_DOCUMENT_LAST_SIGNAL] = { 0, 0 };

/*
 * GdaXmlDocument object implementation
 */
static void
gda_xml_document_class_init (GdaXmlDocumentClass * klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	gda_xml_document_signals[GDA_XML_DOCUMENT_WARNING] =
		gtk_signal_new ("warning",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaXmlDocumentClass,
						   warning),
				gtk_marshal_NONE__POINTER, GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	gda_xml_document_signals[GDA_XML_DOCUMENT_ERROR] =
		gtk_signal_new ("error", GTK_RUN_FIRST, object_class->type,
				GTK_SIGNAL_OFFSET (GdaXmlDocumentClass,
						   error),
				gtk_marshal_NONE__POINTER, GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, gda_xml_document_signals,
				      GDA_XML_DOCUMENT_LAST_SIGNAL);

	klass->warning = NULL;
	klass->error = NULL;

	object_class->destroy =
		(void (*)(GtkObject *)) gda_xml_document_destroy;
}

static void
gda_xml_document_init (GdaXmlDocument * xmldoc)
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

GtkType
gda_xml_document_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaXmlDocument",
			sizeof (GdaXmlDocument),
			sizeof (GdaXmlDocumentClass),
			(GtkClassInitFunc) gda_xml_document_class_init,
			(GtkObjectInitFunc) gda_xml_document_init,
			(GtkArgSetFunc) 0,
			(GtkArgSetFunc) 0
		};
		type = gtk_type_unique (gtk_object_get_type (), &info);
	}
	return (type);
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

	xmldoc = GDA_XML_DOCUMENT (gtk_type_new (GDA_TYPE_XML_DOCUMENT));
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

static void
gda_xml_document_destroy (GtkObject * object)
{
	GtkObjectClass *parent_class;
	GdaXmlDocument *xmldoc = (GdaXmlDocument *) object;

	g_return_if_fail (GDA_IS_XML_DOCUMENT (xmldoc));

	xmlFreeDoc (xmldoc->doc);
	xmldoc->doc = NULL;

	parent_class = gtk_type_class (gtk_object_get_type ());
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}

/**
 * gda_xml_document_new_from_file
 * @filename: file name
 *
 * Load a #GdaXmlDocument from the given @filename
 */
/* GdaXmlDocument * */
/* gda_xml_document_new_from_file (const gchar *filename) */
/* { */
/*   GdaXmlDocument* xmldocument; */

/*   xmldocument = GDA_XML_DOCUMENT(gtk_type_new(gda_xml_document_get_type())); */

   /* DTD already done while loading */
/*   xmldocument->doc = xmlParseFile(filename); */
/*   if (xmldocument->doc) */
/*     { */
/*       xmldocument->root = xmlDocGetRootElement(xmldocument->doc); */
/*     } */

/*   return xmldocument; */
/* } */

gchar *
gda_xml_document_stringify (GdaXmlDocument * xmldoc)
{
	xmlChar *str;
	gint i;

	g_return_val_if_fail (GDA_IS_XML_DOCUMENT (xmldoc), NULL);

	xmlDocDumpMemory (xmldoc->doc, &str, &i);
	return str;
}

gint
gda_xml_document_to_file (GdaXmlDocument * xmldoc, const gchar * filename)
{
	g_return_val_if_fail (GDA_IS_XML_DOCUMENT (xmldoc), -1);
	g_return_val_if_fail ((filename != NULL), -1);

	return xmlSaveFile (filename, xmldoc->doc);
}

/* FIXME: signals in preparation for future use. Will work when I understand 
   how validation is done with libxml. */
static void (gda_xml_document_error_def) (void *ctx, const char *msg, ...)
{
	g_print ("ERR SIG\n");
	gtk_signal_emit (GTK_OBJECT (((xmlValidCtxtPtr) ctx)->userData),
			 gda_xml_document_signals[GDA_XML_DOCUMENT_ERROR],
			 msg);
}

static void (gda_xml_document_warn_def) (void *ctx, const char *msg, ...)
{
	g_print ("WARN SIG\n");
	gtk_signal_emit (GTK_OBJECT (((xmlValidCtxtPtr) ctx)->userData),
			 gda_xml_document_signals[GDA_XML_DOCUMENT_ERROR],
			 msg);
}
