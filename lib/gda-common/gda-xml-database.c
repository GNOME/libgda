/* GNOME DB library
 * Copyright (C) 1999, 2000 Rodrigo Moya
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

#include <gda-common.h>

#define PROPERTY_NAME "name"

static void gda_xml_database_init       (Gda_XmlDatabase *xmldb);
static void gda_xml_database_class_init (Gda_XmlDatabaseClass *klass);

static void gda_xml_database_destroy    (Gda_XmlDatabase *xmldb);

/*
 * Gda_XmlDatabase object signals
 */
enum
{
  GDA_XML_DATABASE_CHANGED,
  GDA_XML_DATABASE_LAST_SIGNAL
};

static gint xmldb_signals[GDA_XML_DATABASE_LAST_SIGNAL] = { 0, };

/*
 * Gda_XmlDatabase class interface
 */
static void
gda_xml_database_class_init (Gda_XmlDatabaseClass *klass)
{
  GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

  xmldb_signals[GDA_XML_DATABASE_CHANGED] =
    gtk_signal_new("changed",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(Gda_XmlDatabaseClass, changed),
		   gtk_signal_default_marshaller,
		   GTK_TYPE_NONE, 0);
  gtk_object_class_add_signals(object_class, xmldb_signals, GDA_XML_DATABASE_LAST_SIGNAL);

  klass->changed = NULL;
}

static void
gda_xml_database_init (Gda_XmlDatabase *xmldb)
{
  xmldb->tables = NULL;
  xmldb->views = NULL;
}

GtkType
gda_xml_database_get_type (void)
{
  static GtkType xml_database_type = 0;

  if (!xml_database_type)
    {
      GtkTypeInfo xml_database_info =
      {
        "Gda_XmlDatabase",
        sizeof (Gda_XmlDatabase),
        sizeof (Gda_XmlDatabaseClass),
        (GtkClassInitFunc) gda_xml_database_class_init,
        (GtkObjectInitFunc) gda_xml_database_init,
        (GtkArgSetFunc) NULL,
        (GtkArgSetFunc) NULL
      };
      xml_database_type = gtk_type_unique(gda_xml_file_get_type(),
                                          &xml_database_info);
    }
  return xml_database_type;
}

/**
 * gda_xml_database_new
 *
 * Creates a new #Gda_XmlDatabase object, which can be used to describe
 * a database which will then be loaded by a provider to create its
 * defined structure
 */
Gda_XmlDatabase *
gda_xml_database_new (void)
{
  Gda_XmlDatabase* xmldb;

  xmldb = GDA_XML_DATABASE(gtk_type_new(gda_xml_database_get_type()));
  gda_xml_file_construct(GDA_XML_FILE(xmldb), "database");

  /* initialize XML nodes */
  xmldb->tables = xmlNewChild(GDA_XML_FILE(xmldb)->root, NULL, "table", NULL);
  xmldb->views = xmlNewChild(GDA_XML_FILE(xmldb)->root, NULL, "view", NULL);

  return xmldb;
}

static void
gda_xml_database_destroy (Gda_XmlDatabase *xmldb)
{
  GtkObjectClass* parent_class;

  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));

  parent_class = gtk_type_class(gtk_object_get_type());
  if (parent_class && parent_class->destroy)
    parent_class->destroy(GTK_OBJECT(xmldb));
}

/**
 * gda_xml_database_free
 * @xmldb: XML database
 *
 * Destroys the given XML database
 */
void
gda_xml_database_free (Gda_XmlDatabase *xmldb)
{
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  gtk_object_destroy(GTK_OBJECT(xmldb));
}

/**
 * gda_xml_database_table_new
 * @xmldb: XML database
 * @tname: table name
 *
 * Add a new table description to the given XML database. If @tname already exists,
 * this function fails.
 */
xmlNodePtr
gda_xml_database_table_new (Gda_XmlDatabase *xmldb, gchar *tname)
{
  xmlNodePtr node;

  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(tname != NULL, NULL);

  /* check if table is already present */
  node = gda_xml_database_table_find(xmldb, tname);
  if (node)
    {
      g_warning(_("table %s already exists"), tname);
      return node;
    }

  node = xmlNewNode(NULL, "table");
  xmlNewProp(node, PROPERTY_NAME, tname);
  xmlAddChild(xmldb->tables, node);
  gda_xml_database_changed(xmldb);

  return node;
}

/**
 * gda_xml_database_table_find
 * @xmldb: XML database
 * @tname: table name
 *
 * Looks for the specified table in a XML database
 */
xmlNodePtr
gda_xml_database_table_find (Gda_XmlDatabase *xmldb, gchar *tname)
{
  xmlNodePtr node;

  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(tname != NULL, NULL);

  for (node = xmldb->tables->childs; node != NULL; node = node->next)
    {
      gchar* name = xmlGetProp(node, PROPERTY_NAME);
      if (name && !strcmp(name, tname)) return node;
    }
  return NULL; /* not found */
}

/**
 * gda_xml_database_table_add_field
 * @xmldb: XML database
 * @table: table
 * @fname: field name
 *
 * Add a new field to the given table
 */
xmlNodePtr
gda_xml_database_table_add_field (Gda_XmlDatabase *xmldb,
				  xmlNodePtr table,
				  const gchar *fname)
{
}

/**
 * gda_xml_database_changed
 * @xmldb: XML database
 *
 * Emit the "changed" signal for the given XML database
 */
void
gda_xml_database_changed (Gda_XmlDatabase *xmldb)
{
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  gtk_signal_emit(GTK_OBJECT(xmldb), xmldb_signals[GDA_XML_DATABASE_CHANGED]);
}
