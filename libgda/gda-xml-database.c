/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#include <config.h>
#include <libgda/gda-xml-database.h>
#include <libgda/gda-util.h>
#include <libgda/gda-log.h>
#include <bonobo/bonobo-i18n.h>

struct _GdaXmlDatabasePrivate {
	xmlNodePtr tables;
};

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

static void gda_xml_database_class_init (GdaXmlDatabaseClass *klass);
static void gda_xml_database_init       (GdaXmlDatabase *xmldb, GdaXmlDatabaseClass *klass);
static void gda_xml_database_finalize   (GObject *object);

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
static void
gda_xml_database_class_init (GdaXmlDatabaseClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	xmldb_signals[GDA_XML_DATABASE_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaXmlDatabaseClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = gda_xml_database_finalize;
	klass->changed = NULL;
}

static void
gda_xml_database_init (GdaXmlDatabase *xmldb, GdaXmlDatabaseClass *klass)
{
	xmldb->priv = g_new (GdaXmlDatabasePrivate, 1);
	xmldb->priv->tables = NULL;
}

static void
gda_xml_database_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaXmlDatabase *xmldb = (GdaXmlDatabase *) object;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	/* free memory */
	g_free (xmldb->priv);
	xmldb->priv = NULL;

	parent_class = G_OBJECT_CLASS (g_type_class_peek (GDA_TYPE_XML_DOCUMENT));
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

GType
gda_xml_database_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlDatabaseClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xml_database_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlDatabase),
			0,
			(GInstanceInitFunc) gda_xml_database_init
		};
		type = g_type_register_static (GDA_TYPE_XML_DOCUMENT,
					       "GdaXmlDatabase",
					       &info, 0);
	}
	return type;
}

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
	GdaXmlDatabase *xmldb;

	xmldb = GDA_XML_DATABASE (g_object_new (GDA_TYPE_XML_DATABASE, NULL));
	gda_xml_document_construct (GDA_XML_DOCUMENT (xmldb), OBJECT_DATABASE);

	return xmldb;
}

/**
 * gda_xml_database_new_from_uri
 */
GdaXmlDatabase *
gda_xml_database_new_from_uri (const gchar *uri)
{
	GdaXmlDatabase *xmldb;
	gchar *body;
	xmlNodePtr node;

	g_return_val_if_fail (uri != NULL, NULL);

	/* load the file from the given URI */
	body = gda_file_load (uri);
	if (!body) {
		gda_log_error (_("Could not load file at %s"), uri);
		return NULL;
	}

	/* parse the loaded XML file */
	xmldb = GDA_XML_DATABASE (g_object_new (GDA_TYPE_XML_DATABASE, NULL));

	GDA_XML_DOCUMENT (xmldb)->doc = xmlParseMemory (body, strlen (body));
	g_free (body);

	if (!GDA_XML_DOCUMENT (xmldb)->doc) {
		gda_log_error (_("Could not parse file at %s"), uri);
		g_object_unref (G_OBJECT (xmldb));
		return NULL;
	}

	GDA_XML_DOCUMENT (xmldb)->root = xmlDocGetRootElement (GDA_XML_DOCUMENT (xmldb)->doc);
	node = GDA_XML_DOCUMENT (xmldb)->root->xmlChildrenNode;
	while (node) {
		if (!strcmp (node->name, OBJECT_TABLES_NODE)) {
			if (xmldb->priv->tables != NULL) {
				gda_log_error (_("Bad formed document at %s"), uri);
				g_object_unref (G_OBJECT (xmldb));
				return NULL;
			}
			xmldb->priv->tables = node;
		}
		node = node->next;
	}

	return xmldb;
}

/**
 * gda_xml_database_changed
 * @xmldb: XML database
 *
 * Emit the "changed" signal for the given XML database
 */
void
gda_xml_database_changed (GdaXmlDatabase * xmldb)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_signal_emit (G_OBJECT (xmldb),
		       xmldb_signals[GDA_XML_DATABASE_CHANGED],
		       0);
}

/**
 * gda_xml_database_get_tables
 * @xmldb: XML database
 *
 * Returns a list with the names of all the tables that are present
 * in the given #GdaXmlDatabase object
 */
GList *
gda_xml_database_get_tables (GdaXmlDatabase *xmldb)
{
	xmlNodePtr node;
	GList *list = NULL;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (xmldb->priv->tables != NULL, NULL);

	for (node = xmldb->priv->tables->xmlChildrenNode;
	     node != NULL;
	     node = node->next) {
		gchar *name;

		name = xmlGetProp (node, PROPERTY_NAME);
		if (name)
			list = g_list_append (list, g_strdup (name));
	}

	return list;
}

/**
 * gda_xml_database_table_new
 * @xmldb: XML database
 * @tname: table name
 *
 * Add a new table description to the given XML database. If @tname already exists,
 * this function fails.
 */
GdaXmlDatabaseTable *
gda_xml_database_table_new (GdaXmlDatabase *xmldb, const gchar *name)
{
	GdaXmlDatabaseTable *table;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	/* check if table is already present */
	table = gda_xml_database_table_find (xmldb, name);
	if (table) {
		g_warning (_("table %s already exists"), name);
		return table;
	}

	/* create the TABLES node if it's not present */
	if (xmldb->priv->tables == NULL) {
		xmldb->priv->tables = xmlNewNode (NULL, OBJECT_TABLES_NODE);
		xmlAddChild (GDA_XML_DOCUMENT (xmldb)->root, xmldb->priv->tables);
	}

	table = xmlNewNode (NULL, OBJECT_TABLE);
	xmlNewProp (table, PROPERTY_NAME, name);
	xmlAddChild (xmldb->priv->tables, table);
	gda_xml_database_changed (xmldb);

	return table;
}

/**
 * gda_xml_database_table_remove
 * @xmldb: XML database
 * @table: the table to be removed
 */
void
gda_xml_database_table_remove (GdaXmlDatabase * xmldb, GdaXmlDatabaseTable *table)
{
	GdaXmlDatabaseTable *tmp_table;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (table != NULL);

	tmp_table = gda_xml_database_table_find (xmldb,
						 xmlGetProp (table, PROPERTY_NAME));
	if (tmp_table) {
		xmlUnlinkNode (table);
		xmlFreeNode (table);
		gda_xml_database_changed (xmldb);
	}
}

/**
 * gda_xml_database_table_find
 * @xmldb: XML database
 * @tname: table name
 *
 * Looks for the specified table in a XML database
 */
GdaXmlDatabaseTable *
gda_xml_database_table_find (GdaXmlDatabase *xmldb, const gchar *name)
{
	xmlNodePtr node;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (xmldb->priv != NULL, NULL);
	g_return_val_if_fail (xmldb->priv->tables, NULL);

	for (node = xmldb->priv->tables->xmlChildrenNode;
	     node != NULL;
	     node = node->next) {
		gchar *tmp = xmlGetProp (node, PROPERTY_NAME);
		if (name && !strcmp (name, tmp))
			return node;
	}

	return NULL;		/* not found */
}

/**
 * gda_xml_database_table_get_name
 * @xmldb: XML database
 * @table: table
 */
const gchar *
gda_xml_database_table_get_name (GdaXmlDatabase * xmldb, GdaXmlDatabaseTable *table)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	return xmlGetProp (table, PROPERTY_NAME);
}

/**
 * gda_xml_database_table_set_name
 * @xmldb: XML database
 * @table: table
 * @tname: new name
 */
void
gda_xml_database_table_set_name (GdaXmlDatabase *xmldb,
				 GdaXmlDatabaseTable *table,
				 const gchar *name)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (table != NULL);
	g_return_if_fail (name != NULL);

	if (!gda_xml_database_table_find (xmldb, name)) {
		xmlSetProp (table, PROPERTY_NAME, name);
		gda_xml_database_changed (xmldb);
	}
}

/**
 * gda_xml_database_table_get_owner
 */
const gchar *
gda_xml_database_table_get_owner (GdaXmlDatabase * xmldb,
				  GdaXmlDatabaseTable *table)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	return xmlGetProp (table, PROPERTY_OWNER);
}

/**
 * gda_xml_database_table_set_owner
 */
void
gda_xml_database_table_set_owner (GdaXmlDatabase * xmldb,
				  GdaXmlDatabaseTable *table,
				  const gchar * owner)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (table != NULL);

	if (owner)
		xmlSetProp (table, PROPERTY_OWNER, owner);
	gda_xml_database_changed (xmldb);
}

/**
 * gda_xml_database_table_field_count
 */
gint
gda_xml_database_table_field_count (GdaXmlDatabase * xmldb,
				    GdaXmlDatabaseTable *table)
{
	xmlNodePtr xmlnode;
	gint cnt = 0;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), -1);
	g_return_val_if_fail (table != NULL, -1);

	for (xmlnode = table->xmlChildrenNode; xmlnode != NULL;
	     xmlnode = xmlnode->next) {
		if (!strcmp (xmlnode->name, OBJECT_FIELD))
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
GdaXmlDatabaseField *
gda_xml_database_table_add_field (GdaXmlDatabase *xmldb,
				  GdaXmlDatabaseTable *table,
				  const gchar *fname)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (fname != NULL, NULL);

	/* check if 'table' is really a TABLE */
	if (table->parent == xmldb->priv->tables) {
		xmlNodePtr field = gda_xml_database_table_find_field (xmldb,
								      table,
								      fname);
		if (!field) {
			field = xmlNewNode (NULL, OBJECT_FIELD);
			xmlNewProp (field, PROPERTY_NAME, fname);
			xmlAddChild (table, field);
			gda_xml_database_changed (xmldb);
		}
	}
	else
		g_warning (_("%p is not a valid table"), table);

	return NULL;
}

/**
 * gda_xml_database_table_remove_field
 */
void
gda_xml_database_table_remove_field (GdaXmlDatabase * xmldb,
				     GdaXmlDatabaseTable *table,
				     const gchar * fname)
{
	xmlNodePtr field;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (table != NULL);
	g_return_if_fail (fname != NULL);

	field = gda_xml_database_table_find_field (xmldb, table, fname);
	if (field) {
		xmlUnlinkNode (field);
		xmlFreeNode (field);
		gda_xml_database_changed (xmldb);
	}
}

/**
 * gda_xml_database_table_get_field
 */
GdaXmlDatabaseField *
gda_xml_database_table_get_field (GdaXmlDatabase *xmldb,
				  GdaXmlDatabaseTable *table,
				  gint pos)
{
	gint cnt = 0;
	xmlNodePtr xmlnode;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (table != NULL, NULL);

	for (xmlnode = table->xmlChildrenNode; xmlnode != NULL;
	     xmlnode = xmlnode->next) {
		if (!strcmp (xmlnode->name, OBJECT_FIELD)) {
			if (cnt == pos)
				return xmlnode;
			cnt++;	/* increment field count */
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
GdaXmlDatabaseField *
gda_xml_database_table_find_field (GdaXmlDatabase * xmldb,
				   GdaXmlDatabaseTable *table,
				   const gchar * fname)
{
	xmlNodePtr node;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (fname != NULL, NULL);

	for (node = table->xmlChildrenNode; node != NULL; node = node->next) {
		if (!strcmp (node->name, OBJECT_FIELD)) {
			gchar *name = xmlGetProp (node, PROPERTY_NAME);
			if (name && !strcmp (name, fname))
				return node;
		}
	}

	return NULL;		/* not found */
}

/**
 * gda_xml_database_field_get_name
 * @xmldb: XML database
 * @field: XML field node
 *
 * Return the name of the given field
 */
const gchar *
gda_xml_database_field_get_name (GdaXmlDatabase * xmldb, GdaXmlDatabaseField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return xmlGetProp (field, PROPERTY_NAME);
}

/**
 * gda_xml_database_field_set_name
 */
void
gda_xml_database_field_set_name (GdaXmlDatabase * xmldb,
				 GdaXmlDatabaseField *field,
				 const gchar * name)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (name != NULL);

	if (!gda_xml_database_table_find_field (xmldb, field->parent, name)) {
		xmlSetProp (field, PROPERTY_NAME, name);
		gda_xml_database_changed (xmldb);
	}
}

/**
 * gda_xml_database_field_get_gdatype
 */
const gchar *
gda_xml_database_field_get_gdatype (GdaXmlDatabase * xmldb,
				    GdaXmlDatabaseField *field)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (field != NULL, NULL);
	return xmlGetProp (field, PROPERTY_GDATYPE);
}

/**
 * gda_xml_database_field_set_gdatype
 */
void
gda_xml_database_field_set_gdatype (GdaXmlDatabase * xmldb,
				    GdaXmlDatabaseField *field,
				    const gchar * type)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (type != NULL);

	xmlSetProp (field, PROPERTY_GDATYPE, type);
	gda_xml_database_changed (xmldb);
}

/**
 * gda_xml_database_field_get_size
 */
gint
gda_xml_database_field_get_size (GdaXmlDatabase * xmldb,
				 GdaXmlDatabaseField *field)
{
	gchar *size;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), -1);
	g_return_val_if_fail (field != NULL, -1);

	size = xmlGetProp (field, PROPERTY_SIZE);
	return size ? atoi (size) : -1;
}

/**
 * gda_xml_database_field_set_size
 */
void
gda_xml_database_field_set_size (GdaXmlDatabase * xmldb,
				 GdaXmlDatabaseField *field,
				 gint size)
{
	gchar *str;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (field != NULL);

	str = g_strdup_printf ("%d", size);
	xmlSetProp (field, PROPERTY_SIZE, str);
	g_free ((gpointer) str);
	gda_xml_database_changed (xmldb);
}

/**
 * gda_xml_database_field_get_scale
 */
gint
gda_xml_database_field_get_scale (GdaXmlDatabase * xmldb,
				  GdaXmlDatabaseField *field)
{
	gchar *scale;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), -1);
	g_return_val_if_fail (field != NULL, -1);

	scale = xmlGetProp (field, PROPERTY_SCALE);
	return scale ? atoi (scale) : -1;
}

/**
 * gda_xml_database_field_set_scale
 */
void
gda_xml_database_field_set_scale (GdaXmlDatabase * xmldb,
				  GdaXmlDatabaseField *field,
				  gint scale)
{
	gchar *str;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (field != NULL);

	str = g_strdup_printf ("%d", scale);
	xmlSetProp (field, PROPERTY_SCALE, str);
	g_free ((gpointer) str);
	gda_xml_database_changed (xmldb);
}
