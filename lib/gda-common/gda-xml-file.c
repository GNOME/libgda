/* GDA library
 * Copyright (C) 1999, 2000 Rodrigo Moya
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
#include "gda-xml-file.h"

#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

enum
{
  GDA_XML_FILE_WARNING,
  GDA_XML_FILE_ERROR,
  GDA_XML_FILE_LAST_SIGNAL
};

static gint gda_xml_file_signals[GDA_XML_FILE_LAST_SIGNAL] = { 0, 0 };

#ifdef HAVE_GOBJECT
static void gda_xml_file_finalize (GObject *object);
#else
static void gda_xml_file_destroy    (GdaXmlFile *xmlfile);
#endif

/* errors handling */
static void (gda_xml_file_error_def) (void *ctx, const char *msg, ...);
static void (gda_xml_file_warn_def) (void *ctx, const char *msg, ...);

/*
 * GdaXmlFile object implementation
 */
#ifdef HAVE_GOBJECT
static void
gda_xml_file_class_init (GdaXmlFileClass *klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* FIXME: GObject signals are not yet implemented */
  object_class->finalize = &gda_xml_file_finalize;
  klass->parent = g_type_class_peek_parent (klass);
  klass->warning = NULL;
  klass->error = NULL;
}
#else
static void
gda_xml_file_class_init (GdaXmlFileClass *klass)
{
  GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

  gda_xml_file_signals[GDA_XML_FILE_WARNING] =
    gtk_signal_new ("warning",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GdaXmlFileClass, warning),
                    gtk_marshal_NONE__POINTER, GTK_TYPE_NONE, 1, 
		    GTK_TYPE_POINTER);

  gda_xml_file_signals[GDA_XML_FILE_ERROR] =
    gtk_signal_new ("error",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GdaXmlFileClass, error),
                    gtk_marshal_NONE__POINTER, GTK_TYPE_NONE, 1, 
		    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, gda_xml_file_signals, 
				GDA_XML_FILE_LAST_SIGNAL);

  klass->warning = NULL;
  klass->error = NULL;

  object_class->destroy = (void (*)(GtkObject *)) gda_xml_file_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_xml_file_init (GdaXmlFile *xmlfile, GdaXmlFileClass* klass)
#else
gda_xml_file_init (GdaXmlFile *xmlfile)
#endif
{
  /* might change in future versions of libxml */
  extern int xmlDoValidityCheckingDefaultValue;
  xmlDoValidityCheckingDefaultValue = 1;

  g_return_if_fail(GDA_IS_XML_FILE(xmlfile));

  xmlfile->doc = NULL;
  xmlfile->dtd = NULL;
  xmlfile->root = NULL;
  xmlfile->context = NULL;
}

#ifdef HAVE_GOBJECT
GType
gda_xml_file_get_type (void)
{
  static GType type = 0;
  if (!type)
    {
      GTypeInfo info =
      {
        sizeof (GdaXmlFileClass),                /* class_size */
        NULL,                                     /* base_init */
        NULL,                                     /* base_finalize */
        (GClassInitFunc) gda_xml_file_class_init, /* class_init */
        NULL,                                     /* class_finalize */
        NULL,                                     /* class_data */
        sizeof (GdaXmlFile),                     /* instance_size */
        0,                                        /* n_preallocs */
        (GInstanceInitFunc) gda_xml_file_init,    /* instance_init */
        NULL,                                     /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "GdaXmlFile", &info, 0);
    }
  return (type);
}
#else
GtkType
gda_xml_file_get_type (void)
{
  static guint gda_xml_file_type = 0;
  if (!gda_xml_file_type)
    {
      GtkTypeInfo gda_xml_file_info =
      {
        "GdaXmlFile",
        sizeof(GdaXmlFile),
        sizeof(GdaXmlFileClass),
        (GtkClassInitFunc) gda_xml_file_class_init,
        (GtkObjectInitFunc) gda_xml_file_init,
        (GtkArgSetFunc) 0,
        (GtkArgSetFunc) 0
      };
      gda_xml_file_type = gtk_type_unique(gtk_object_get_type(), &gda_xml_file_info);
    }
  return (gda_xml_file_type);
}
#endif

/**
 * gda_xml_file_new
 * @root_doc: root document new
 *
 * Create a new #GdaXmlFile object, with a root document of type @root_doc
 */
GdaXmlFile *
gda_xml_file_new (const gchar *root_doc)
{
  GdaXmlFile* xmlfile;

#ifdef HAVE_GOBJECT
  xmlfile = GDA_XML_FILE (g_object_new (GDA_TYPE_XML_FILE, NULL));
#else
  xmlfile = GDA_XML_FILE(gtk_type_new(gda_xml_file_get_type()));
#endif

  gda_xml_file_construct(xmlfile, root_doc);

  return xmlfile;
}

void gda_xml_file_construct(GdaXmlFile *xmlfile, const gchar *root_doc)
{
  /* initialize XML document */
  xmlfile->doc = xmlNewDoc("1.0");
  xmlfile->doc->root = xmlNewDocNode(xmlfile->doc, NULL, root_doc, NULL);
  xmlfile->root = xmlfile->doc->root;

  xmlfile->context = g_new0(xmlValidCtxt, 1);
  xmlfile->context->userData = xmlfile;
  xmlfile->context->error = gda_xml_file_error_def;
  xmlfile->context->warning = gda_xml_file_warn_def;
}

#ifdef HAVE_GOBJECT
static void
gda_xml_file_finalize (GObject *object)
{
  GdaXmlFile *xmlfile = GDA_XML_FILE (object);
  GdaXmlFileClass *klass =
    G_TYPE_INSTANCE_GET_CLASS (object, GDA_XML_FILE_CLASS, GdaXmlFileClass);

  xmlFreeDoc (xmlfile->doc);
  klass->parent->finalize (object);
}
#else
static void
gda_xml_file_destroy (GdaXmlFile *xmlfile)
{
  g_return_if_fail(GDA_IS_XML_FILE(xmlfile));

  xmlFreeDoc(xmlfile->doc);
}
#endif

/**
 * gda_xml_file_new_from_file
 * @filename: file name
 *
 * Load a #GdaXmlFile from the given @filename
 */
/* GdaXmlFile * */
/* gda_xml_file_new_from_file (const gchar *filename) */
/* { */
/*   GdaXmlFile* xmlfile; */

/*   xmlfile = GDA_XML_FILE(gtk_type_new(gda_xml_file_get_type())); */

   /* DTD already done while loading */ 
/*   xmlfile->doc = xmlParseFile(filename); */
/*   if (xmlfile->doc) */
/*     { */
/*       xmlfile->root = xmlDocGetRootElement(xmlfile->doc); */
/*     } */

/*   return xmlfile; */
/* } */

gchar* gda_xml_file_stringify(GdaXmlFile *f)
{
  xmlChar *str;
  gint i;

  xmlDocDumpMemory(f->doc, &str, &i);
  return str;
}



gint  gda_xml_file_to_file(GdaXmlFile *f, const gchar *filename)
{
  g_return_val_if_fail(GDA_IS_XML_FILE(f), -1);
  g_return_val_if_fail((filename != NULL), -1);

  return xmlSaveFile(filename, f->doc);
}

/* FIXME: signals in preparation for future use. Will work when I understand 
   how validation is done with libxml. */
static void (gda_xml_file_error_def) (void *ctx, const char *msg, ...)
{
  g_print("ERR SIG\n");
#ifndef HAVE_GOBJECT /* FIXME */
  gtk_signal_emit(GTK_OBJECT(((xmlValidCtxtPtr)ctx)->userData), 
		  gda_xml_file_signals[GDA_XML_FILE_ERROR], msg);
#endif
}

static void (gda_xml_file_warn_def) (void *ctx, const char *msg, ...)
{
  g_print("WARN SIG\n");
#ifndef HAVE_GOBJECT /* FIXME */
  gtk_signal_emit(GTK_OBJECT(((xmlValidCtxtPtr)ctx)->userData), 
		  gda_xml_file_signals[GDA_XML_FILE_ERROR], msg);
#endif
}

