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

#define OBJECT_DATABASE "database"
#define OBJECT_TABLE    "table"
#define OBJECT_FIELD    "field"
#define OBJECT_VIEW     "view"

#define PROPERTY_NAME   "name"
#define PROPERTY_OWNER  "owner"

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
  gda_xml_file_construct(GDA_XML_FILE(xmldb), OBJECT_DATABASE);

  /* initialize XML nodes */
  xmldb->tables = xmlNewChild(GDA_XML_FILE(xmldb)->root, NULL, "tables", NULL);
  xmldb->views = xmlNewChild(GDA_XML_FILE(xmldb)->root, NULL, "views", NULL);

  return xmldb;
}

/**
 * gda_xml_database_new_from_file
 */
Gda_XmlDatabase *
gda_xml_database_new_from_file (const gchar *filename)
{
  Gda_XmlDatabase* xmldb;
  
  xmldb = GDA_XML_DATABASE(gtk_type_new(gda_xml_database_get_type()));
  GDA_XML_FILE(xmldb)->doc = xmlParseFile(filename);
  if (GDA_XML_FILE(xmldb)->doc)
    {
      xmlNodePtr node;
      GDA_XML_FILE(xmldb)->root = xmlDocGetRootElement(GDA_XML_FILE(xmldb)->doc);
      node = GDA_XML_FILE(xmldb)->root->childs;
      while (node)
        {
        	if (!strcmp(node->name, "tables"))
        	  xmldb->tables = node;
        	else if (!strcmp(node->name, "views"))
        	  xmldb->views = node;
        	node = node->next;
        }
    }
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
 * gda_xml_database_save
 */
void
gda_xml_database_save (Gda_XmlDatabase *xmldb, const gchar *filename)
{
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  
  if (!xmlSaveFile(filename, GDA_XML_FILE(xmldb)->doc))
    {
    }
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

  node = xmlNewNode(NULL, OBJECT_TABLE);
  xmlNewProp(node, PROPERTY_NAME, tname);
  xmlAddChild(xmldb->tables, node);
  gda_xml_database_changed(xmldb);

  return node;
}

/**
 * gda_xml_database_table_remove
 * @xmldb: XML database
 * @tname: name of the table
 */
void
gda_xml_database_table_remove (Gda_XmlDatabase *xmldb, const gchar *tname)
{
  xmlNodePtr table;
  
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  g_return_if_fail(tname != NULL);
  
  table = gda_xml_database_table_find(xmldb, tname);
  if (table)
    {
      xmlUnlinkNode(table);
      xmlFreeNode(table);
      gda_xml_database_changed(xmldb);
    }
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
 * gda_xml_database_table_get_name
 * @xmldb: XML database
 * @table: table
 */
const gchar *
gda_xml_database_table_get_name (Gda_XmlDatabase *xmldb, xmlNodePtr table)
{
  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(table != NULL, NULL);
  return xmlGetProp(table, PROPERTY_NAME);
}

/**
 * gda_xml_database_table_set_name
 * @xmldb: XML database
 * @table: table
 * @tname: new name
 */
void
gda_xml_database_table_set_name (Gda_XmlDatabase *xmldb, xmlNodePtr table, const gchar *tname)
{
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  g_return_if_fail(table != NULL);
  g_return_if_fail(tname != NULL);
  
  if (!gda_xml_database_table_find(xmldb, tname))
    {
      xmlSetProp(table, PROPERTY_NAME, tname);
      gda_xml_database_changed(xmldb);
    }
}

/**
 * gda_xml_database_table_get_owner
 */
const gchar *
gda_xml_database_table_get_owner (Gda_XmlDatabase *xmldb, xmlNodePtr table)
{
  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(table != NULL, NULL);
  return xmlGetProp(table, PROPERTY_OWNER);
}

/**
 * gda_xml_database_table_set_owner
 */
void
gda_xml_database_table_set_owner (Gda_XmlDatabase *xmldb, xmlNodePtr table, const gchar *owner)
{
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  g_return_if_fail(table != NULL);
  
  if (owner)
    {
      xmlSetProp(table, PROPERTY_OWNER, owner);
    }
  gda_xml_database_changed(xmldb);
}

/**
 * gda_xml_database_field_count
 */
gint
gda_xml_database_field_count (Gda_XmlDatabase *xmldb, xmlNodePtr table)
{
  xmlNodePtr xmlnode;
  gint       cnt = 0;
  
  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), -1);
  g_return_val_if_fail(table != NULL, -1);
  
  for (xmlnode = table->childs; xmlnode != NULL; xmlnode = xmlnode->next)
    {
      cnt++;
    }
  return cnt;
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
  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(table != NULL, NULL);
  
  /* check if 'table' is really a TABLE */
  if (table->parent == xmldb->tables)
    {
      xmlNodePtr field = gda_xml_database_table_find_field(xmldb, table, fname);
      if (!field)
        {
          field = xmlNewNode(NULL, OBJECT_FIELD);
          xmlNewProp(field, PROPERTY_NAME, fname);
          xmlAddChild(table, field);
          gda_xml_database_changed(xmldb);
        }
    }
  else g_warning(_("%p is not a valid table"), table);
  return NULL;
}

/**
 * gda_xml_database_table_remove_field
 */
void
gda_xml_database_table_remove_field (Gda_XmlDatabase *xmldb, xmlNodePtr table, const gchar *fname)
{
  xmlNodePtr field;
  
  g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  g_return_if_fail(table != NULL);
  g_return_if_fail(fname != NULL);
  
  field = gda_xml_database_table_find_field(xmldb, table, fname);
  if (field)
    {
      xmlUnlinkNode(field);
      xmlFreeNode(field);
      gda_xml_database_changed(xmldb);
    }
}

/**
 * gda_xml_database_table_get_field
 */
xmlNodePtr
gda_xml_database_table_get_field (Gda_XmlDatabase *xmldb, xmlNodePtr table, gint pos)
{
  gint       cnt = 0;
  xmlNodePtr xmlnode;
  
  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(table != NULL, NULL);
  
  for (xmlnode = table->childs; xmlnode != NULL; xmlnode = xmlnode->next)
    {
      if (!strcmp(xmlnode->name, OBJECT_FIELD))
        {
          if (cnt == pos) return xmlnode;
          cnt++; /* increment field count */
        }
    }
  return NULL;
}

/**
 * gda_xml_database_table_find_field
 * @xmldb: XML database
 * @table: table
 * @fname: field name
 *
 * Look for the given field in the given table
 */
xmlNodePtr
gda_xml_database_table_find_field (Gda_XmlDatabase *xmldb, xmlNodePtr table, const gchar *fname)
{
  xmlNodePtr node;

  g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
  g_return_val_if_fail(table != NULL, NULL);
  g_return_val_if_fail(fname != NULL, NULL);

  for (node = table->childs; node != NULL; node = node->next)
    {
      if (!strcmp(node->name, OBJECT_FIELD))
        {
          gchar* name = xmlGetProp(node, PROPERTY_NAME);
          if (name && !strcmp(name, fname)) return node;
        }
    }
  return NULL; /* not found */
}

/**
 * gda_xml_database_field_get_name
 * @xmldb: XML database
 * @field: XML field node
 *
 * Return the name of the given field
 */
const gchar *
gda_xml_database_field_get_name (Gda_XmlDatabase *xmldb, xmlNodePtr field)
{
  g_return_val_if_fail(field != NULL, NULL);
  return xmlGetProp(field, PROPERTY_NAME);
}

/**
 * gda_xml_database_field_set_name
 */
void
gda_xml_database_field_set_name (Gda_XmlDatabase *xmldb, xmlNodePtr field, const gchar *name)
{
  g_return_if_fail(field != NULL);
  g_return_if_fail(name != NULL);
  
  if (!gda_xml_database_table_find_field(xmldb, field->parent, name))
    {
      xmlSetProp(field, PROPERTY_NAME, name);
      gda_xml_database_changed(xmldb);
    }
}
