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
	gchar *uri;
	GHashTable *tables;
	GHashTable *views;
};

#define OBJECT_DATABASE    "database"
#define OBJECT_DATA        "data"
#define OBJECT_FIELD       "field"
#define OBJECT_ROW         "row"
#define OBJECT_TABLE       "table"
#define OBJECT_TABLES_NODE "tables"
#define OBJECT_VIEW        "view"
#define OBJECT_VIEWS_NODE  "views"

#define PROPERTY_ALLOW_NULL "isnull"
#define PROPERTY_GDATYPE    "gdatype"
#define PROPERTY_NAME       "name"
#define PROPERTY_OWNER      "owner"
#define PROPERTY_SCALE      "scale"
#define PROPERTY_SIZE       "size"

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
static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
process_tables_node (GdaXmlDatabase *xmldb, xmlNodePtr children)
{
	xmlNodePtr node;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	for (node = children; node != NULL; node = node->next)
		gda_xml_database_new_table_from_node (xmldb, node);
}

static void
process_views_node (GdaXmlDatabase *xmldb, xmlNodePtr children)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (children != NULL);
}

/*
 * GdaXmlDatabase class interface
 */
static void
gda_xml_database_class_init (GdaXmlDatabaseClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	/* allocate private structure */
	xmldb->priv = g_new0 (GdaXmlDatabasePrivate, 1);
	xmldb->priv->uri = NULL;
	xmldb->priv->tables = g_hash_table_new (g_str_hash, g_str_equal);
	xmldb->priv->views = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
remove_table_hash (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_object_unref (G_OBJECT (value));
}

static void
gda_xml_database_finalize (GObject *object)
{
	GdaXmlDatabase *xmldb = (GdaXmlDatabase *) object;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	/* free memory */
	if (xmldb->priv->uri) {
		g_free (xmldb->priv->uri);
		xmldb->priv->uri = NULL;
	}

	g_hash_table_foreach_remove (xmldb->priv->tables, remove_table_hash, NULL);
	g_hash_table_destroy (xmldb->priv->tables);
	xmldb->priv->tables = NULL;

	g_hash_table_destroy (xmldb->priv->views);
	xmldb->priv->views = NULL;

	g_free (xmldb->priv);
	xmldb->priv = NULL;

	/* chain to parent class */
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
		type = g_type_register_static (G_TYPE_OBJECT,
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

	xmldb = g_object_new (GDA_TYPE_XML_DATABASE, NULL);
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
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;

	g_return_val_if_fail (uri != NULL, NULL);

	/* load the file from the given URI */
	body = gda_file_load (uri);
	if (!body) {
		gda_log_error (_("Could not load file at %s"), uri);
		return NULL;
	}

	/* parse the loaded XML file */
	doc = xmlParseMemory (body, strlen (body));
	g_free (body);

	if (!doc) {
		gda_log_error (_("Could not parse file at %s"), uri);
		return NULL;
	}

	xmldb = g_object_new (GDA_TYPE_XML_DATABASE, NULL);

	/* parse the file */
	root = xmlDocGetRootElement (doc);
	if (strcmp (root->name, OBJECT_DATABASE)) {
		gda_log_error (_("Invalid XML database file '%s'"), uri);
		g_object_unref (G_OBJECT (xmldb));
		return NULL;
	}

	node = root->xmlChildrenNode;
	while (node) {
		xmlNodePtr children;

		children = node->xmlChildrenNode;

		/* more validation */
		if (!strcmp (node->name, OBJECT_TABLES_NODE))
			process_tables_node (xmldb, children);
		else if (!strcmp (node->name, OBJECT_VIEWS_NODE))
			process_views_node (xmldb, children);
		else {
			gda_log_error (_("Invalid XML database file '%s'"), uri);
			g_object_unref (G_OBJECT (xmldb));
			return NULL;
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
 * gda_xml_database_new_table
 * @xmldb: XML database.
 * @name: Name for the new table.
 *
 * Create a new empty table in the given XML database.
 *
 * Returns: a pointer to the newly created in-memory table.
 */
GdaTable *
gda_xml_database_new_table (GdaXmlDatabase *xmldb, const gchar *name)
{
	GdaTable *table;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	if (g_hash_table_lookup (xmldb->priv->tables, name)) {
		gda_log_error (_("Table %s already exists"), name);
		return NULL;
	}

	table = gda_table_new (name);
	g_hash_table_insert (xmldb->priv->tables, g_strdup (name), table);
	gda_xml_database_changed (xmldb);

	return table;
}

/**
 * gda_xml_database_new_table_from_node
 * @xmldb: XML Database.
 * @node: A XML node pointer.
 *
 * Add a table to the given XML database by parsong the given
 * XML node pointer, which usually is obtained from an
 * already loaded xmlDocPtr.
 *
 * Returns: a pointer to the newly created in-memory table.
 */
GdaTable *
gda_xml_database_new_table_from_node (GdaXmlDatabase *xmldb, xmlNodePtr node)
{
	GdaTable *table;
	gchar *name;
	xmlNodePtr children;
	xmlNodePtr data = NULL;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (node != NULL, NULL);

	if (strcmp (node->name, OBJECT_TABLE)) {
		gda_log_error (_("Invalid node"));
		return NULL;
	}

	table = gda_table_new (name);
	if (!table) {
		gda_log_error (_("Table %s already exists"), name);
		return NULL;
	}

	/* parse the node */
	for (children = node->xmlChildrenNode; children != NULL; children = children->next) {
		if (!strcmp (children->name, OBJECT_FIELD)) {
			GdaFieldAttributes *fa;

			fa = gda_field_attributes_new ();
			gda_field_attributes_set_defined_size (
				fa, atoi (xmlGetProp (children, PROPERTY_SIZE)));
			gda_field_attributes_set_name (
				fa, xmlGetProp (children, PROPERTY_NAME));
			gda_field_attributes_set_scale (
				fa, atoi (xmlGetProp (children, PROPERTY_SCALE)));
			gda_field_attributes_set_gdatype (
				fa, atoi (xmlGetProp (children, PROPERTY_GDATYPE)));
			gda_field_attributes_set_allow_null (
				fa, atoi (xmlGetProp (children, PROPERTY_ALLOW_NULL)));

			gda_table_add_field (table, fa);
		}
		else if (!strcmp (children->name, OBJECT_DATA)) {
			if (data) {
				gda_log_error (_("Duplicated <data> node for table %s"), name);
				g_object_unref (G_OBJECT (table));
				return NULL;
			}
			data = children;
		}
		else {
			gda_log_error (_("Invalid XML node"));
			g_object_unref (G_OBJECT (table));
			return NULL;
		}
	}

	/* FIXME: add the data of the table */

	/* add the table to the XML database object */
	g_hash_table_insert (xmldb->priv->tables, g_strdup (name), table);
	gda_xml_database_changed (xmldb);

	return table;
}
