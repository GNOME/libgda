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

#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-xml-database.h>
#include <string.h>

struct _GdaXmlDatabasePrivate {
	gchar *uri;
	gchar *name;
	gchar *user_version;
	gchar *version;
	GHashTable *tables;
	GHashTable *views;
	GHashTable *queries;
};

#define OBJECT_DATABASE     "database"
#define OBJECT_DATA         "data"
#define OBJECT_FIELD        "field"
#define OBJECT_QUERIES_NODE "queries"
#define OBJECT_ROW          "row"
#define OBJECT_TABLE        "table"
#define OBJECT_TABLES_NODE  "tables"
#define OBJECT_VIEW         "view"
#define OBJECT_VIEWS_NODE   "views"

#define PROPERTY_ALLOW_NULL   "isnull"
#define PROPERTY_AUTO         "auto_increment"
#define PROPERTY_CAPTION      "caption"
#define PROPERTY_GDATYPE      "gdatype"
#define PROPERTY_NAME         "name"
#define PROPERTY_OWNER        "owner"
#define PROPERTY_PRIMARY_KEY  "pkey"
#define PROPERTY_REFERENCES   "references"
#define PROPERTY_SCALE        "scale"
#define PROPERTY_SIZE         "size"
#define PROPERTY_UNIQUE_KEY   "unique"
#define PROPERTY_VERSION      "version"
#define PROPERTY_USER_VERSION "user_version"

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
process_queries_node (GdaXmlDatabase *xmldb, xmlNodePtr children)
{
}

static void
process_tables_node (GdaXmlDatabase *xmldb, xmlNodePtr children)
{
	xmlNodePtr node;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	for (node = children; node != NULL; node = node->next) {
		gda_xml_database_new_table_from_node (xmldb, node);
	}
}

static void
process_views_node (GdaXmlDatabase *xmldb, xmlNodePtr children)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (children != NULL);
}

/*
 * Callbacks
 */

static void
table_changed_cb (GdaDataModel *model, gpointer user_data)
{
	GdaXmlDatabase *xmldb = (GdaXmlDatabase *) user_data;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	gda_xml_database_changed (xmldb);
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
	xmldb->priv->name = NULL;
	xmldb->priv->user_version = NULL;
	xmldb->priv->version = NULL;
	xmldb->priv->tables = g_hash_table_new (g_str_hash, g_str_equal);
	xmldb->priv->views = g_hash_table_new (g_str_hash, g_str_equal);
	xmldb->priv->queries = g_hash_table_new (g_str_hash, g_str_equal);
}

static gboolean
remove_table_hash (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_signal_handlers_disconnect_by_func (G_OBJECT (value), table_changed_cb, user_data);
	g_object_unref (G_OBJECT (value));
	return TRUE;
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

	if (xmldb->priv->name) {
		g_free (xmldb->priv->name);
		xmldb->priv->name = NULL;
	}

	if (xmldb->priv->user_version) {
		g_free (xmldb->priv->user_version);
		xmldb->priv->user_version = NULL;
	}

	if (xmldb->priv->version) {
		g_free (xmldb->priv->version);
		xmldb->priv->version = NULL;
	}

	g_hash_table_foreach_remove (xmldb->priv->tables, remove_table_hash, xmldb);
	g_hash_table_destroy (xmldb->priv->tables);
	xmldb->priv->tables = NULL;

	g_hash_table_destroy (xmldb->priv->views);
	xmldb->priv->views = NULL;

	g_hash_table_destroy (xmldb->priv->queries);
	xmldb->priv->queries = NULL;

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
	xmldb->priv->uri = g_strdup (uri);

	/* parse the file */
	root = xmlDocGetRootElement (doc);
	if (strcmp (root->name, OBJECT_DATABASE)) {
		gda_log_error (_("Invalid XML database file '%s'"), uri);
		g_object_unref (G_OBJECT (xmldb));
		return NULL;
	}

	xmldb->priv->name = g_strdup (xmlGetProp (root, PROPERTY_NAME));
	xmldb->priv->user_version = g_strdup  (xmlGetProp (root, PROPERTY_USER_VERSION));
	xmldb->priv->version = g_strdup  (xmlGetProp (root, PROPERTY_VERSION));
	node = root->xmlChildrenNode;
	while (node) {
		xmlNodePtr children;

		children = node->xmlChildrenNode;

		/* more validation */
		if (!strcmp (node->name, OBJECT_TABLES_NODE))
			process_tables_node (xmldb, children);
		else if (!strcmp (node->name, OBJECT_VIEWS_NODE))
			process_views_node (xmldb, children);
		else if (!strcmp (node->name, OBJECT_QUERIES_NODE))
			process_queries_node (xmldb, children);
		else {
/*
			gda_log_error (_("Invalid XML database file '%s'"), uri);
			g_object_unref (G_OBJECT (xmldb));
			return NULL;
*/
		}

		node = node->next;
	}

	return xmldb;
}

/**
 * gda_xml_database_get_name
 * @xmldb: XML database.
 *
 * Return the name of the given XML database.
 *
 * Returns: the name of the database.
 */
const gchar *
gda_xml_database_get_name (GdaXmlDatabase *xmldb)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	return (const gchar *) xmldb->priv->name;
}

/**
 * gda_xml_database_set_name
 * @xmldb: XML database.
 * @name: new name for the database.
 *
 * Set the name of the given XML database object.
 */
void
gda_xml_database_set_name (GdaXmlDatabase *xmldb, const gchar *name)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	if (xmldb->priv->name)
		g_free (xmldb->priv->name);
	xmldb->priv->name = g_strdup (name);

	gda_xml_database_changed (xmldb);
}

/**
 * gda_xml_database_get_user_version
 * @xmldb: XML database.
 *
 * Get the user defined version of the given #GdaXmlDatabase object.
 *
 * Returns: the database version defined by the user.
 */
const gchar *
gda_xml_database_get_user_version (GdaXmlDatabase *xmldb)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	return (const gchar *) xmldb->priv->user_version;
}

/**
 * gda_xml_database_set_user_version
 * @xmldb: XML database.
 * @user_version: User defined Version string.
 *
 * Set the user defined version of the given XML database.
 */
void
gda_xml_database_set_user_version (GdaXmlDatabase *xmldb, const gchar *user_version)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));
	g_return_if_fail (user_version != NULL);

	if (xmldb->priv->user_version)
		g_free (xmldb->priv->user_version);
	xmldb->priv->user_version = g_strdup (user_version);

	gda_xml_database_changed (xmldb);
}

/**
 * gda_xml_database_get_version
 * @xmldb: XML database.
 *
 * Get the version of libgda used to create the #GdaXmlDatabase object.
 * This version is the one that was used for saving the XML file last 
 * time it was saved. This value can only be "get" as it is an internal
 * information related to the creation of the #GdaXmlDatabase object.
 * To get the user defined database version, use the function
 * gda_xml_database_get_user_version instead.
 *
 * Returns: the libgda version used to create the database.
 */
const gchar *
gda_xml_database_get_version (GdaXmlDatabase *xmldb)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	return (const gchar *) xmldb->priv->version;
}

/**
 * gda_xml_database_get_uri
 * @xmldb: XML database.
 *
 * Return the URI associated with the given XML database. This URI will
 * be used when saving the XML database (#gda_xml_database_save) and no
 * URI is given.
 *
 * Returns: the URI for the XML database.
 */
const gchar *
gda_xml_database_get_uri (GdaXmlDatabase *xmldb)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	return xmldb->priv->uri;
}

/**
 * gda_xml_database_set_uri
 * @xmldb: XML database.
 */
void
gda_xml_database_set_uri (GdaXmlDatabase *xmldb, const gchar *uri)
{
	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	if (xmldb->priv->uri)
		g_free (xmldb->priv->uri);
	xmldb->priv->uri = g_strdup (uri);
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
 * gda_xml_database_reload
 * @xmldb: XML database.
 *
 * Reload the given XML database from its original place, discarding
 * all changes that may have happened.
 */
void
gda_xml_database_reload (GdaXmlDatabase *xmldb)
{
	/* FIXME: implement */
}

/**
 * gda_xml_database_save
 * @xmldb: XML database.
 * @uri: URI to save the XML database to.
 *
 * Save the given XML database to disk.
 */
gboolean
gda_xml_database_save (GdaXmlDatabase *xmldb, const gchar *uri)
{
        gchar*xml;
	gboolean result;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), FALSE);

	xml = gda_xml_database_to_string (xmldb);
	if (xml) {
		result = gda_file_save (uri, xml, strlen (xml));
		g_free (xml);
	} else
		result = FALSE;

	return result;
}

/**
 * gda_xml_database_to_string
 * @xmldb: a #GdaXmlDatabase object.
 *
 * Get the given XML database contents as a XML string.
 *
 * Returns: the XML string representing the structure and contents of the
 * given #GdaXmlDatabase object. The returned value must be freed when no
 * longer needed.
 */
gchar *
gda_xml_database_to_string (GdaXmlDatabase *xmldb)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr tables_node = NULL;
	GList *list, *l;
	xmlChar *xml;
	gint size;
	gchar *retval;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);

	/* create the top node */
	doc = xmlNewDoc ("1.0");
	root = xmlNewDocNode (doc, NULL, OBJECT_DATABASE, NULL);
	xmlSetProp (root, PROPERTY_NAME, xmldb->priv->name);
	xmlSetProp (root, PROPERTY_USER_VERSION, xmldb->priv->user_version);
	xmlSetProp (root, PROPERTY_VERSION, VERSION);
	xmlDocSetRootElement (doc, root);

	/* add tables */
	list = gda_xml_database_get_tables (xmldb);
	for (l = list; l != NULL; l = l->next) {
		GdaTable *table = gda_xml_database_find_table (xmldb, l->data);
		xmlNodePtr node = gda_data_model_to_xml_node (GDA_DATA_MODEL (table), l->data);

		if (!node) {
			gda_log_error (_("Could not create a XML node from table %s"), l->data);
			xmlFreeDoc (doc);
			gda_xml_database_free_table_list (list);
			return NULL;
		}

		if (!tables_node)
			tables_node = xmlNewChild (root, NULL, OBJECT_TABLES_NODE, NULL);

		xmlAddChild (tables_node, node);
	}

	gda_xml_database_free_table_list (list);

	/* save to memory */
	xmlDocDumpMemory (doc, &xml, &size);
	xmlFreeDoc (doc);
	if (!xml) {
		gda_log_error (_("Could not dump XML file to memory"));
		return NULL;
	}

	retval = g_strdup (xml);
	free (xml);

	return retval;
}

static void
add_table_to_list (gpointer key, gpointer value, gpointer user_data)
{
	gchar *name = (gchar *) key;
	GList **list = (GList **) user_data;

	*list = g_list_append (*list, g_strdup (name));
}

/**
 * gda_xml_database_get_tables
 * @xmldb: XML database.
 *
 * Return a list of all table names present in the given database.
 * You must free the returned GList when you no longer need it, by
 * using the #gda_xml_database_free_table_list function.
 *
 * Returns: a GList of strings.
 */
GList *
gda_xml_database_get_tables (GdaXmlDatabase *xmldb)
{
	GList *list = NULL;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);

	g_hash_table_foreach (xmldb->priv->tables, (GHFunc) add_table_to_list, &list);
	return list;
}

/**
 * gda_xml_database_free_table_list
 * @list: list of tables, as returned by #gda_xml_database_get_tables.
 *
 * Free a GList of strings returned by #gda_xml_database_get_tables.
 */
void
gda_xml_database_free_table_list (GList *list)
{
	GList *l;

	for (l = list; l != NULL; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

/**
 * gda_xml_database_find_table
 * @xmldb: XML database.
 * @name: Name for the table to look for.
 *
 * Searches the given XML database for a table named @name, and
 * returns a pointer to it.
 *
 * Returns: a pointer to the table, or NULL if not found.
 */
GdaTable *
gda_xml_database_find_table (GdaXmlDatabase *xmldb, const gchar *name)
{
	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (xmldb->priv->tables, name);
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
	g_signal_connect (G_OBJECT (table), "changed", G_CALLBACK (table_changed_cb), xmldb);
	g_hash_table_insert (xmldb->priv->tables, g_strdup (name), table);
	gda_xml_database_changed (xmldb);

	return table;
}

/**
 * gda_xml_database_new_table_from_model
 * @xmldb: XML database.
 * @name: Name for the new table.
 * @model: Model to create the table from.
 * @add_data: Whether to add model's data or not.
 *
 * Create a new table in the given XML database from the given
 * #GdaDataModel.
 *
 * Returns: a pointer to the newly created in-memory table.
 */
GdaTable *
gda_xml_database_new_table_from_model (GdaXmlDatabase *xmldb,
				       const gchar *name,
				       const GdaDataModel *model,
				       gboolean add_data)
{
	GdaTable *table;

	g_return_val_if_fail (GDA_IS_XML_DATABASE (xmldb), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (g_hash_table_lookup (xmldb->priv->tables, name)) {
		gda_log_error (_("Table %s already exists"), name);
		return NULL;
	}

	table = gda_table_new_from_model (name, model, add_data);
	g_signal_connect (G_OBJECT (table), "changed", G_CALLBACK (table_changed_cb), xmldb);
	if (GDA_IS_TABLE (table)) {
		g_hash_table_insert (xmldb->priv->tables, g_strdup (name), table);
		gda_xml_database_changed (xmldb);
	}

	return table;
}

/**
 * gda_xml_database_new_table_from_node
 * @xmldb: XML Database.
 * @node: A XML node pointer.
 *
 * Add a table to the given XML database by parsing the given
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

	name = xmlGetProp (node, PROPERTY_NAME);
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
				fa, gda_type_from_string (xmlGetProp (children, PROPERTY_GDATYPE)));
			gda_field_attributes_set_allow_null (
				fa, atoi (xmlGetProp (children, PROPERTY_ALLOW_NULL)));
			gda_field_attributes_set_primary_key (
				fa, atoi (xmlGetProp (children, PROPERTY_PRIMARY_KEY)));
			gda_field_attributes_set_unique_key (
				fa, atoi (xmlGetProp (children, PROPERTY_UNIQUE_KEY)));
			gda_field_attributes_set_references (
				fa, xmlGetProp (children, PROPERTY_REFERENCES));
			gda_field_attributes_set_caption (
				fa, atoi (xmlGetProp (children, PROPERTY_CAPTION)));
			gda_field_attributes_set_auto_increment (
				fa, atoi (xmlGetProp (children, PROPERTY_AUTO)));

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

	/* add the data of the table */
	if (data) {
		if (!gda_data_model_add_data_from_xml_node (GDA_DATA_MODEL (table), data))
			g_warning (_("Could not add the data from the XML node"));
	}

	/* add the table to the XML database object */
	g_hash_table_insert (xmldb->priv->tables, g_strdup (name), table);
	g_signal_connect (G_OBJECT (table), "changed", G_CALLBACK (table_changed_cb), xmldb);
	gda_xml_database_changed (xmldb);

	return table;
}
