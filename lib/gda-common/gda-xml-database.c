/* GDA common library
 * Copyright (C) 1999-2001 The Free Software Foundation
 *
 * AUTHORS:
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
#include "gda-xml-database.h"
#include <stdlib.h>
#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define OBJECT_DATABASE    "database"
#define OBJECT_DATA        "data"
#define OBJECT_FIELD       "field"
#define OBJECT_ROW         "row"
#define OBJECT_TABLE       "table"
#define OBJECT_TABLES_NODE "tables"
#define OBJECT_VIEW        "view"
#define OBJECT_VIEWS_NODE  "views"

#define PROPERTY_GDATYPE "gdatype"
#define PROPERTY_NAME    "name"
#define PROPERTY_OWNER   "owner"
#define PROPERTY_SCALE   "scale"
#define PROPERTY_SIZE    "size"

/*
 * GdaXmlDatabase object signals
 */
enum {
	GDA_XML_DATABASE_CHANGED,
	GDA_XML_DATABASE_LAST_SIGNAL
};

static gint xmldb_signals[GDA_XML_DATABASE_LAST_SIGNAL] = { 0, };

/*
 * GdaXmlDatabase class interface
 */
#ifdef HAVE_GOBJECT
static void
gda_xml_database_class_init (GdaXmlDatabaseClass *klass, gpointer data)
{
	/* FIXME: GObject signals are not yet implemented */
	klass->changed = NULL;
}
#else
static void
gda_xml_database_class_init (GdaXmlDatabaseClass *klass)
{
	GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

	xmldb_signals[GDA_XML_DATABASE_CHANGED] =
		gtk_signal_new("changed",
			       GTK_RUN_FIRST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GdaXmlDatabaseClass, changed),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals(object_class, xmldb_signals, GDA_XML_DATABASE_LAST_SIGNAL);

	klass->changed = NULL;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_xml_database_init (GdaXmlDatabase *xmldb, GdaXmlDatabaseClass *klass)
#else
gda_xml_database_init (GdaXmlDatabase *xmldb)
#endif
{
	xmldb->tables = NULL;
	xmldb->views = NULL;
}

#ifdef HAVE_GOBJECT
GType
gda_xml_database_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GdaXmlDatabaseClass),
			NULL,
			NULL,
			(GClassInitFunc) gda_xml_database_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlDatabase),
			0,
			(GInstanceInitFunc) gda_xml_database_init,
			NULL,
		};
		type = g_type_register_static (GDA_TYPE_XML_DOCUMENT, "GdaXmlDatabase",
					       &info, 0);
	}
	return type;
}
#else
GtkType
gda_xml_database_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaXmlDatabase",
			sizeof (GdaXmlDatabase),
			sizeof (GdaXmlDatabaseClass),
			(GtkClassInitFunc) gda_xml_database_class_init,
			(GtkObjectInitFunc) gda_xml_database_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL
		};
		type = gtk_type_unique(gda_xml_document_get_type(), &info);
	}
	return type;
}
#endif

/**
 * gda_xml_database_new
 *
 * Creates a new #GdaXmlDatabase object, which can be used to describe
 * a database which will then be loaded by a provider to create its
 * defined structure
 */
GdaXmlDatabase *
gda_xml_database_new (void)
{
	GdaXmlDatabase* xmldb;

#ifdef HAVE_GOBJECT
	xmldb = GDA_XML_DATABASE (g_object_new (GDA_TYPE_XML_DATABASE, NULL));
#else
	xmldb = GDA_XML_DATABASE(gtk_type_new(gda_xml_database_get_type()));
#endif

	gda_xml_document_construct(GDA_XML_DOCUMENT(xmldb), OBJECT_DATABASE);

	/* initialize XML nodes */
	xmldb->tables = xmlNewChild(GDA_XML_DOCUMENT(xmldb)->root, NULL, "tables", NULL);
	xmldb->views = xmlNewChild(GDA_XML_DOCUMENT(xmldb)->root, NULL, "views", NULL);

	return xmldb;
}

/**
 * gda_xml_database_new_from_file
 */
GdaXmlDatabase *
gda_xml_database_new_from_file (const gchar *filename)
{
	GdaXmlDatabase* xmldb;
  
#ifdef HAVE_GOBJECT
	xmldb = GDA_XML_DATABASE (g_object_new (GDA_TYPE_XML_DATABASE, NULL));
#else
	xmldb = GDA_XML_DATABASE(gtk_type_new(gda_xml_database_get_type()));
#endif

	GDA_XML_DOCUMENT(xmldb)->doc = xmlParseFile(filename);
	if (GDA_XML_DOCUMENT(xmldb)->doc) {
		xmlNodePtr node;
		GDA_XML_DOCUMENT(xmldb)->root = xmlDocGetRootElement(GDA_XML_DOCUMENT(xmldb)->doc);
		node = GDA_XML_DOCUMENT(xmldb)->root->xmlChildrenNode;
		while (node) {
			if (!strcmp(node->name, OBJECT_TABLES_NODE))
				xmldb->tables = node;
			else if (!strcmp(node->name, OBJECT_VIEWS_NODE))
				xmldb->views = node;
			node = node->next;
		}
	}
	return xmldb;
}

/**
 * gda_xml_database_free
 * @xmldb: XML database
 *
 * Destroys the given XML database
 */
void
gda_xml_database_free (GdaXmlDatabase *xmldb)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (xmldb));
#else
	gtk_object_destroy (GTK_OBJECT (xmldb));
#endif
}

/**
 * gda_xml_database_save
 */
void
gda_xml_database_save (GdaXmlDatabase *xmldb, const gchar *filename)
{
	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  
	if (!xmlSaveFile(filename, GDA_XML_DOCUMENT(xmldb)->doc)) {
	}
}

/**
 * gda_xml_database_changed
 * @xmldb: XML database
 *
 * Emit the "changed" signal for the given XML database
 */
void
gda_xml_database_changed (GdaXmlDatabase *xmldb)
{
	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
  
#ifndef HAVE_GOBJECT  /* FIXME: signals without GTK */
	gtk_signal_emit(GTK_OBJECT(xmldb), xmldb_signals[GDA_XML_DATABASE_CHANGED]);
#endif
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
gda_xml_database_table_new (GdaXmlDatabase *xmldb, gchar *tname)
{
	xmlNodePtr node;

	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(tname != NULL, NULL);

	/* check if table is already present */
	node = gda_xml_database_table_find(xmldb, tname);
	if (node) {
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
gda_xml_database_table_remove (GdaXmlDatabase *xmldb, const gchar *tname)
{
	xmlNodePtr table;

	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
	g_return_if_fail(tname != NULL);
  
	table = gda_xml_database_table_find(xmldb, tname);
	if (table) {
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
gda_xml_database_table_find (GdaXmlDatabase *xmldb, const gchar *tname)
{
	xmlNodePtr node;

	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(tname != NULL, NULL);

	for (node = xmldb->tables->xmlChildrenNode; node != NULL; node = node->next) {
		gchar* name = xmlGetProp(node, PROPERTY_NAME);
		if (name && !strcmp(name, tname))
			return node;
	}
	return NULL; /* not found */
}

/**
 * gda_xml_database_table_get_name
 * @xmldb: XML database
 * @table: table
 */
const gchar *
gda_xml_database_table_get_name (GdaXmlDatabase *xmldb, xmlNodePtr table)
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
gda_xml_database_table_set_name (GdaXmlDatabase *xmldb, xmlNodePtr table, const gchar *tname)
{
	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
	g_return_if_fail(table != NULL);
	g_return_if_fail(tname != NULL);
  
	if (!gda_xml_database_table_find(xmldb, tname)) {
		xmlSetProp(table, PROPERTY_NAME, tname);
		gda_xml_database_changed(xmldb);
	}
}

/**
 * gda_xml_database_table_get_owner
 */
const gchar *
gda_xml_database_table_get_owner (GdaXmlDatabase *xmldb, xmlNodePtr table)
{
	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(table != NULL, NULL);
	return xmlGetProp(table, PROPERTY_OWNER);
}

/**
 * gda_xml_database_table_set_owner
 */
void
gda_xml_database_table_set_owner (GdaXmlDatabase *xmldb, xmlNodePtr table, const gchar *owner)
{
	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
	g_return_if_fail(table != NULL);
  
	if (owner)
		xmlSetProp(table, PROPERTY_OWNER, owner);
	gda_xml_database_changed(xmldb);
}

/**
 * gda_xml_database_table_field_count
 */
gint
gda_xml_database_table_field_count (GdaXmlDatabase *xmldb, xmlNodePtr table)
{
	xmlNodePtr xmlnode;
	gint       cnt = 0;
  
	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), -1);
	g_return_val_if_fail(table != NULL, -1);
  
	for (xmlnode = table->xmlChildrenNode; xmlnode != NULL; xmlnode = xmlnode->next) {
		if (!strcmp(xmlnode->name, OBJECT_FIELD)) cnt++;
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
gda_xml_database_table_add_field (GdaXmlDatabase *xmldb,
                                  xmlNodePtr table,
                                  const gchar *fname)
{
	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(table != NULL, NULL);
  
	/* check if 'table' is really a TABLE */
	if (table->parent == xmldb->tables) {
		xmlNodePtr field = gda_xml_database_table_find_field(xmldb, table, fname);
		if (!field) {
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
gda_xml_database_table_remove_field (GdaXmlDatabase *xmldb, xmlNodePtr table, const gchar *fname)
{
	xmlNodePtr field;

	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
	g_return_if_fail(table != NULL);
	g_return_if_fail(fname != NULL);

	field = gda_xml_database_table_find_field(xmldb, table, fname);
	if (field) {
		xmlUnlinkNode(field);
		xmlFreeNode(field);
		gda_xml_database_changed(xmldb);
	}
}

/**
 * gda_xml_database_table_get_field
 */
xmlNodePtr
gda_xml_database_table_get_field (GdaXmlDatabase *xmldb, xmlNodePtr table, gint pos)
{
	gint       cnt = 0;
	xmlNodePtr xmlnode;

	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(table != NULL, NULL);

	for (xmlnode = table->xmlChildrenNode; xmlnode != NULL; xmlnode = xmlnode->next) {
		if (!strcmp(xmlnode->name, OBJECT_FIELD)) {
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
gda_xml_database_table_find_field (GdaXmlDatabase *xmldb, xmlNodePtr table, const gchar *fname)
{
	xmlNodePtr node;

	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(table != NULL, NULL);
	g_return_val_if_fail(fname != NULL, NULL);

	for (node = table->xmlChildrenNode; node != NULL; node = node->next) {
		if (!strcmp(node->name, OBJECT_FIELD)) {
			gchar* name = xmlGetProp(node, PROPERTY_NAME);
			if (name && !strcmp(name, fname))
				return node;
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
gda_xml_database_field_get_name (GdaXmlDatabase *xmldb, xmlNodePtr field)
{
	g_return_val_if_fail(field != NULL, NULL);
	return xmlGetProp(field, PROPERTY_NAME);
}

/**
 * gda_xml_database_field_set_name
 */
void
gda_xml_database_field_set_name (GdaXmlDatabase *xmldb, xmlNodePtr field, const gchar *name)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(name != NULL);
  
	if (!gda_xml_database_table_find_field(xmldb, field->parent, name)) {
		xmlSetProp(field, PROPERTY_NAME, name);
		gda_xml_database_changed(xmldb);
	}
}

/**
 * gda_xml_database_field_get_gdatype
 */
const gchar *
gda_xml_database_field_get_gdatype (GdaXmlDatabase *xmldb, xmlNodePtr field)
{
	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), NULL);
	g_return_val_if_fail(field != NULL, NULL);
	return xmlGetProp(field, PROPERTY_GDATYPE);
}

/**
 * gda_xml_database_field_set_gdatype
 */
void
gda_xml_database_field_set_gdatype (GdaXmlDatabase *xmldb, xmlNodePtr field, const gchar *type)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(type != NULL);

	xmlSetProp(field, PROPERTY_GDATYPE, type);
	gda_xml_database_changed(xmldb);
}

/**
 * gda_xml_database_field_get_size
 */
gint
gda_xml_database_field_get_size (GdaXmlDatabase *xmldb, xmlNodePtr field)
{
	gchar* size;

	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), -1);
	g_return_val_if_fail(field != NULL, -1);

	size = xmlGetProp(field, PROPERTY_SIZE);
	return size ? atoi(size) : -1;
}

/**
 * gda_xml_database_field_set_size
 */
void
gda_xml_database_field_set_size (GdaXmlDatabase *xmldb, xmlNodePtr field, gint size)
{
	gchar* str;
  
	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
	g_return_if_fail(field != NULL);

	str = g_strdup_printf("%d", size);
	xmlSetProp(field, PROPERTY_SIZE, str);
	g_free((gpointer) str);
	gda_xml_database_changed(xmldb);
}

/**
 * gda_xml_database_field_get_scale
 */
gint
gda_xml_database_field_get_scale (GdaXmlDatabase *xmldb, xmlNodePtr field)
{
	gchar* scale;

	g_return_val_if_fail(GDA_IS_XML_DATABASE(xmldb), -1);
	g_return_val_if_fail(field != NULL, -1);

	scale = xmlGetProp(field, PROPERTY_SCALE);
	return scale ? atoi(scale) : -1;
}

/**
 * gda_xml_database_field_set_scale
 */
void
gda_xml_database_field_set_scale (GdaXmlDatabase *xmldb, xmlNodePtr field, gint scale)
{
	gchar* str;

	g_return_if_fail(GDA_IS_XML_DATABASE(xmldb));
	g_return_if_fail(field != NULL);

	str = g_strdup_printf("%d", scale);
	xmlSetProp(field, PROPERTY_SCALE, str);
	g_free((gpointer) str);
	gda_xml_database_changed(xmldb);
}
