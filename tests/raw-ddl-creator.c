/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-server-operation.h"
#include "raw-ddl-creator.h"
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-meta-struct-private.h>
#include <libgda/gda-config.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>

/*
 * Main static functions
 */
static void raw_ddl_creator_class_init (RawDDLCreatorClass *klass);
static void raw_ddl_creator_init (RawDDLCreator *creator);
static void raw_ddl_creator_dispose (GObject *object);

static void raw_ddl_creator_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void raw_ddl_creator_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static gboolean load_customization (RawDDLCreator *ddlc, const gchar *xml_spec_file, GError **error);
static GdaServerOperation *prepare_dbo_server_operation (RawDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc,
							 GdaMetaDbObject *dbo, GError **error);

typedef struct {
	gchar  *prov;
	gchar  *path;
	gchar  *expr;
} ProviderSpecificKey;

typedef struct {
	gchar  *repl;
} ProviderSpecificValue;

static guint ProviderSpecific_hash (gconstpointer key);
static gboolean ProviderSpecific_equal (gconstpointer a, gconstpointer b);
static void ProviderSpecific_key_free (ProviderSpecificKey *key);
static void ProviderSpecific_value_free (ProviderSpecificValue *value);

typedef struct {
	GdaConnection *cnc;
	GdaMetaStruct *d_mstruct;

	GHashTable    *provider_specifics; /* key = a ProviderSpecificKey , value = a ProviderSpecificValue */
	GValue *catalog;
	GValue *schema;
	gchar *quoted_catalog;
	gchar *quoted_schema;
} RawDDLCreatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (RawDDLCreator, raw_ddl_creator, G_TYPE_OBJECT)

/* properties */
enum {
	PROP_0,
	PROP_CNC_OBJECT,
	PROP_CATALOG,
	PROP_SCHEMA
};

/* module error */
GQuark raw_ddl_creator_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("raw_ddl_creator_error");
	return quark;
}

static guint
ProviderSpecific_hash (gconstpointer key)
{
	ProviderSpecificKey *pkey = (ProviderSpecificKey*) key;
	return g_str_hash (pkey->path) + g_str_hash (pkey->prov) +
		(pkey->expr ? g_str_hash (pkey->expr) : 0);
}

static gboolean
ProviderSpecific_equal (gconstpointer a, gconstpointer b)
{
	ProviderSpecificKey *pa, *pb;
	pa = (ProviderSpecificKey*) a;
	pb = (ProviderSpecificKey*) b;

	if (strcmp (pa->prov, pb->prov) ||
	    (pa->expr && !pb->expr) ||
	    (pb->expr && !pa->expr) ||
	    (pa->expr && pb->expr && strcmp (pa->expr, pb->expr)) ||
	    strcmp (pa->path, pb->path))
		return FALSE;
	else
		return TRUE;
}

static void
ProviderSpecific_key_free (ProviderSpecificKey *key)
{
	g_free (key->prov);
	g_free (key->path);
	g_free (key->expr);
	g_free (key);
}

static void
ProviderSpecific_value_free (ProviderSpecificValue *value)
{
	g_free (value->repl);
	g_free (value);
}


static void
raw_ddl_creator_class_init (RawDDLCreatorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* Properties */
	object_class->set_property = raw_ddl_creator_set_property;
	object_class->get_property = raw_ddl_creator_get_property;
	g_object_class_install_property (object_class, PROP_CNC_OBJECT,
		g_param_spec_object ("cnc", NULL, _ ("Connection object"), GDA_TYPE_CONNECTION,
		(G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_CATALOG,
		g_param_spec_string ("catalog", NULL, _ ("Catalog in which the database objects will be created"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_SCHEMA,
		g_param_spec_string ("schema", NULL, _ ("Schema in which the database objects will be created"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));

	object_class->dispose = raw_ddl_creator_dispose;

}


static void
raw_ddl_creator_init (RawDDLCreator *creator)
{
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (creator);
	priv->cnc = NULL;
	priv->d_mstruct = NULL;
	priv->provider_specifics = g_hash_table_new_full (ProviderSpecific_hash, ProviderSpecific_equal,
								   (GDestroyNotify) ProviderSpecific_key_free,
								   (GDestroyNotify) ProviderSpecific_value_free);
	priv->catalog = NULL;
	priv->schema = NULL;
	priv->quoted_catalog = NULL;
	priv->quoted_schema = NULL;
}

static void
raw_ddl_creator_dispose (GObject *object)
{
	RawDDLCreator *creator;

	g_return_if_fail (RAW_IS_DDL_CREATOR (object));

	creator = RAW_DDL_CREATOR (object);
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (creator);

		if (priv->catalog)
			gda_value_free (priv->catalog);
		if (priv->schema)
			gda_value_free (priv->schema);
		g_free (priv->quoted_catalog);
		g_free (priv->quoted_schema);

		if (priv->cnc) {
			g_object_unref (priv->cnc);
			priv->cnc = NULL;
		}

		if (priv->d_mstruct) {
			g_object_unref (priv->d_mstruct);
			priv->d_mstruct = NULL;
		}

	/* parent class */
	G_OBJECT_CLASS (raw_ddl_creator_parent_class)->dispose (object);
}


static void
raw_ddl_creator_set_property (GObject *object,
			      guint param_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	RawDDLCreator *creator;

	creator = RAW_DDL_CREATOR (object);

  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (creator);

		switch (param_id) {
		case PROP_CNC_OBJECT:
			if (priv->cnc)
				g_object_unref (priv->cnc);

			priv->cnc = g_value_get_object (value);
			if (priv->cnc)
				g_object_ref (priv->cnc);
			break;
		case PROP_CATALOG:
			if (priv->catalog)
				gda_value_free (priv->catalog);
			priv->catalog = NULL;
			g_free (priv->quoted_catalog);
			priv->quoted_catalog = NULL;
			if (g_value_get_string (value) && *g_value_get_string (value)) {
				priv->catalog = gda_value_copy (value);
				if (!priv->cnc)
					g_warning ("The \"catalog\" property should be set after the \"cnc\" one");
				priv->quoted_catalog =
					gda_sql_identifier_quote (g_value_get_string (value), priv->cnc, NULL,
								  FALSE, FALSE);
			}
			break;
		case PROP_SCHEMA:
			if (priv->schema)
				gda_value_free (priv->schema);
			priv->schema = NULL;
			g_free (priv->quoted_schema);
			priv->quoted_schema = NULL;
			if (g_value_get_string (value) && *g_value_get_string (value)) {
				priv->schema = gda_value_copy (value);
				if (!priv->cnc)
					g_warning ("The \"schema\" property should be set after the \"cnc\" one");
				priv->quoted_schema =
					gda_sql_identifier_quote (g_value_get_string (value), priv->cnc, NULL,
								  FALSE, FALSE);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
}

static void
raw_ddl_creator_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	RawDDLCreator *creator;
	creator = RAW_DDL_CREATOR (object);
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (creator);

		switch (param_id) {
		case PROP_CNC_OBJECT:
			g_value_set_object (value, (GObject *) priv->cnc);
			break;
		case PROP_CATALOG:
			if (priv->catalog)
				g_value_set_string (value, g_value_get_string (priv->catalog));
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_SCHEMA:
			if (priv->schema)
				g_value_set_string (value, g_value_get_string (priv->schema));
			else
				g_value_set_string (value, NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
}

/**
 * raw_ddl_creator_new
 * Create a new #RawDDLCreator object
 *
 * Returns: the newly created object
 */
RawDDLCreator *
raw_ddl_creator_new (void)
{
	return (RawDDLCreator *) g_object_new (RAW_TYPE_DDL_CREATOR, NULL);
}

/**
 * raw_ddl_creator_set_dest_from_file
 * @ddlc: a #RawDDLCreator object
 * @xml_spec_file: a file name
 * @error: a place to store errors, or %NULL
 *
 * Sets the destination structure
 *
 * Returns: TRUE if no error occurred
 */
gboolean
raw_ddl_creator_set_dest_from_file (RawDDLCreator *ddlc, const gchar *xml_spec_file, GError **error)
{
	g_return_val_if_fail (RAW_IS_DDL_CREATOR (ddlc), FALSE);
	g_return_val_if_fail (xml_spec_file && *xml_spec_file, FALSE);
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (ddlc);

	if (priv->d_mstruct)
		g_object_unref (priv->d_mstruct);

	priv->d_mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, NULL);

	if (!gda_meta_struct_load_from_xml_file (priv->d_mstruct, NULL, NULL, xml_spec_file, error) ||
	    !load_customization (ddlc, xml_spec_file, error)) {
		g_object_unref (priv->d_mstruct);
		priv->d_mstruct = NULL;
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean
load_customization (RawDDLCreator *ddlc, const gchar *xml_spec_file, GError **error)
{
	xmlNodePtr node;
	xmlDocPtr doc;
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (ddlc);

	/* load information schema's structure XML file */
	doc = xmlParseFile (xml_spec_file);
	if (!doc) {
		g_set_error (error, RAW_DDL_CREATOR_ERROR, RAW_DDL_CREATOR_SPECFILE_NOT_FOUND_ERROR,
			     "Could not load file '%s'", xml_spec_file);
		return FALSE;
	}

	node = xmlDocGetRootElement (doc);
	if (!node || strcmp ((gchar *) node->name, "schema")) {
		g_set_error (error, RAW_DDL_CREATOR_ERROR, RAW_DDL_CREATOR_INCORRECT_SCHEMA_ERROR,
			     "Root node of file '%s' should be <schema>.", xml_spec_file);
		xmlFreeDoc (doc);
		return FALSE;
	}

	/* walk through the xmlDoc */
	for (node = node->children; node; node = node->next) {
		/* <specifics> tag to allow for provider specific transformations */
		if (!strcmp ((gchar *) node->name, "specifics")) {
			xmlNodePtr snode;
			for (snode = node->children; snode; snode = snode->next) {
				if (strcmp ((gchar *) snode->name, "provider"))
					continue;
				xmlChar *pname;
				pname = xmlGetProp (snode, BAD_CAST "name");
				if (!pname) {
					g_warning ("<provider> section ignored because no provider name specified");
					continue; /* ignore this section */
				}

				xmlNodePtr rnode;
				for (rnode = snode->children; rnode; rnode = rnode->next) {
					if (!strcmp ((gchar *) rnode->name, "replace") ||
					    !strcmp ((gchar *) rnode->name, "ignore")) {
						xmlChar *context, *tmp;
						context = xmlGetProp (rnode, BAD_CAST "context");
						if (!context) {
							g_warning ("<%s> section ignored because no context specified",
								   snode->name);
							continue;
						}
						ProviderSpecificKey *key = g_new0 (ProviderSpecificKey, 1);
						ProviderSpecificValue *val = g_new0 (ProviderSpecificValue, 1);
						key->prov = g_strdup ((gchar *) pname);
						key->path = g_strdup ((gchar *) context);
						xmlFree (context);
						tmp = xmlGetProp (rnode, BAD_CAST "expr");
						if (tmp) {
							key->expr = g_strdup ((gchar *) tmp);
							xmlFree (tmp);
						}
						tmp = xmlGetProp (rnode, BAD_CAST "replace_with");
						if (tmp) {
							val->repl = g_strdup ((gchar *) tmp);
							xmlFree (tmp);
						}
						g_hash_table_insert (priv->provider_specifics, key, val);
						/*g_print ("RULE: %s, %s, %s => %s\n", key->prov,
						  key->path, key->expr, val->repl);*/
					}
				}
				xmlFree (pname);
			}
		}
	}
	xmlFreeDoc (doc);

	return TRUE;
}

static GdaServerOperation *create_server_operation_for_table (RawDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc,
							      GdaMetaDbObject *dbobj, GError **error);
static GdaServerOperation *create_server_operation_for_view (RawDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc,
							     GdaMetaDbObject *dbobj, GError **error);

static GdaServerOperation *
prepare_dbo_server_operation (RawDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, GdaMetaDbObject *dbo, GError **error)
{
	GdaServerOperation *op = NULL;
	switch (dbo->obj_type) {
	case GDA_META_DB_UNKNOWN:
		g_set_error (error, RAW_DDL_CREATOR_ERROR, RAW_DDL_CREATOR_INCORRECT_SCHEMA_ERROR,
			     "Unknown database object '%s'", dbo->obj_full_name);
		break;

	case GDA_META_DB_TABLE:
		op = create_server_operation_for_table (ddlc, prov, cnc, dbo, error);
		break;

	case GDA_META_DB_VIEW:
		op = create_server_operation_for_view (ddlc, prov, cnc, dbo, error);
		break;
	default:
		TO_IMPLEMENT;
		break;
	}

	return op;
}

static const gchar *
provider_specific_match (GHashTable *specific_hash, GdaServerProvider *prov, const gchar *expr, const gchar *path)
{
	ProviderSpecificKey spec;
	ProviderSpecificValue *val;

	spec.prov = (gchar *) gda_server_provider_get_name (prov);
	spec.path = (gchar *) path;
	spec.expr = (gchar *) expr;
	val = g_hash_table_lookup (specific_hash, &spec);
	/*g_print ("RULESEARCH %s, %s, %s => %s\n", spec.prov, spec.path, spec.expr, val ? val->repl : "-no rule found-");*/
	if (val)
		return val->repl;
	else
		return expr;
}

typedef struct {
	GdaServerOperation *op;
	gint index;
	GError **error;
	gboolean allok;
} FData;

static GdaServerOperation *
create_server_operation_for_table (RawDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc,
				   GdaMetaDbObject *dbobj, GError **error)
{
	GdaServerOperation *op;
	GSList *list;
	gint index;
	const gchar *repl;
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (ddlc);

	op = gda_server_provider_create_operation (prov, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
	if (!op)
		return NULL;
	if (! gda_server_operation_set_value_at (op, dbobj->obj_name, error, "/TABLE_DEF_P/TABLE_NAME"))
		goto onerror;

	/* columns */
	for (index = 0, list = GDA_META_TABLE (dbobj)->columns; list; list = list->next, index++) {
		GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
		if (! gda_server_operation_set_value_at (op, tcol->column_name, error,
							 "/FIELDS_A/@COLUMN_NAME/%d", index))
			goto onerror;
		repl = provider_specific_match (priv->provider_specifics, prov, tcol->column_type ? tcol->column_type : "string",
						"/FIELDS_A/@COLUMN_TYPE");
		if (! gda_server_operation_set_value_at (op, repl ? repl : "string", error,
							 "/FIELDS_A/@COLUMN_TYPE/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, NULL, error,
							 "/FIELDS_A/@COLUMN_SIZE/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, tcol->nullok ? "FALSE" : "TRUE", error,
							 "/FIELDS_A/@COLUMN_NNUL/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, "FALSE", error,
							 "/FIELDS_A/@COLUMN_AUTOINC/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, "FALSE", error,
							 "/FIELDS_A/@COLUMN_UNIQUE/%d", index))
			goto onerror;
		FData fdata;
		fdata.op = op;
		fdata.index = index;
		fdata.error = error;
		fdata.allok = TRUE;

		if (tcol->auto_incement) {
			fdata.allok = gda_server_operation_set_value_at (fdata.op, "TRUE", fdata.error,
								  "/FIELDS_A/@COLUMN_AUTOINC/%d",
								  fdata.index);
		}

		if (!fdata.allok)
			goto onerror;

		repl = provider_specific_match (priv->provider_specifics, prov, "dummy", "/FIELDS_A/@COLUMN_PKEY");
		if (repl) {
			if (! gda_server_operation_set_value_at (op, tcol->pkey ? "TRUE" : "FALSE", error,
								 "/FIELDS_A/@COLUMN_PKEY/%d", index))
				goto onerror;
		}
		else {
			if (! gda_server_operation_set_value_at (op, "FALSE", error, "/FIELDS_A/@COLUMN_PKEY/%d", index))
				goto onerror;
		}
	}

	/* foreign keys */
	gint fkindex;
	for (fkindex = 0, list = GDA_META_TABLE (dbobj)->fk_list; list; fkindex++, list = list->next) {
		GdaMetaTableForeignKey *mfkey = GDA_META_TABLE_FOREIGN_KEY (list->data);
		gint col;
		if (! gda_server_operation_set_value_at (op, mfkey->depend_on->obj_full_name, error, "/FKEY_S/%d/FKEY_REF_TABLE",
							 fkindex))
			goto onerror;
		for (col = 0; col < mfkey->cols_nb; col++) {
			if (! gda_server_operation_set_value_at (op, mfkey->fk_names_array[col], error,
								 "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d",
								 fkindex, col))
				goto onerror;
			if (! gda_server_operation_set_value_at (op, mfkey->ref_pk_names_array[col], error,
								 "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
								 fkindex, col))
				goto onerror;
		}
	}

#ifdef GDA_DEBUG
	{
		xmlNodePtr node;
		xmlBufferPtr buffer;
		node = gda_server_operation_save_data_to_xml (op, NULL);
		buffer = xmlBufferCreate ();
		xmlNodeDump (buffer, NULL, node, 5, 1);
		xmlFreeNode (node);
		xmlBufferDump (stdout, buffer);
		xmlBufferFree (buffer);
		g_print ("\n");
	}
#endif

	return op;
 onerror:
	g_object_unref (op);
	return NULL;
}

static GdaServerOperation *
create_server_operation_for_view (G_GNUC_UNUSED RawDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc,
				  GdaMetaDbObject *dbobj, GError **error)
{
	GdaServerOperation *op;

	op = gda_server_provider_create_operation (prov, cnc, GDA_SERVER_OPERATION_CREATE_VIEW, NULL, error);
	if (!op)
		return NULL;
	if (! gda_server_operation_set_value_at (op, dbobj->obj_name, error, "/VIEW_DEF_P/VIEW_NAME"))
		goto onerror;
	if (! gda_server_operation_set_value_at (op, GDA_META_VIEW (dbobj)->view_def, error, "/VIEW_DEF_P/VIEW_DEF"))
		goto onerror;

	return op;
 onerror:
	g_object_unref (op);
	return NULL;
}

/**
 * raw_ddl_creator_set_connection
 * @ddlc: a #RawDDLCreator object
 * @cnc: (allow-none): a #GdaConnection object or %NULL
 *
 *
 */
void
raw_ddl_creator_set_connection (RawDDLCreator *ddlc, GdaConnection *cnc)
{
	g_return_if_fail (RAW_IS_DDL_CREATOR (ddlc));
	g_return_if_fail (!cnc || GDA_IS_CONNECTION (cnc));

	g_object_set (G_OBJECT (ddlc), "cnc", cnc, NULL);
}

/**
 * raw_ddl_creator_get_sql
 * @ddlc: a #RawDDLCreator object
 * @error: a place to store errors, or %NULL
 *
 * Get the SQL code which would be executed to create the database objects
 *
 * Returns: a new string if no error occurred, or %NULL
 */
gchar *
raw_ddl_creator_get_sql (RawDDLCreator *ddlc, GError **error)
{
	GString *string;
	gchar *sql;
	g_return_val_if_fail (RAW_IS_DDL_CREATOR (ddlc), NULL);
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (ddlc);

	if (!priv->cnc) {
		g_set_error (error, RAW_DDL_CREATOR_ERROR, RAW_DDL_CREATOR_NO_CONNECTION_ERROR,
			     "%s", "No connection specified");
		return NULL;
	}

	/* render operations to SQL */
	GdaServerProvider *prov = gda_connection_get_provider (priv->cnc);
	GSList *objlist, *list;
	objlist = gda_meta_struct_get_all_db_objects (priv->d_mstruct);

	string = g_string_new ("");
	for (list = objlist; list; list = list->next) {
		GdaServerOperation *op;
		op = prepare_dbo_server_operation (ddlc, prov, NULL, GDA_META_DB_OBJECT (list->data), error);
		if (!op) {
			g_string_free (string, TRUE);
			g_assert (*error != NULL);
			return NULL;
		}
		else {
			sql = gda_server_provider_render_operation (prov, priv->cnc, op, error);
			if (!sql) {
				g_string_free (string, TRUE);
				return NULL;
			}
			else {
				g_string_append_printf (string, "--\n-- Database object: %s\n--\n",
							GDA_META_DB_OBJECT (list->data)->obj_full_name);
				g_string_append (string, sql);
				g_string_append (string, ";\n\n");
				g_free (sql);
			}
		}
		g_object_unref (op);
	}

	g_print ("SQL: %s\n", string->str);
	return g_string_free (string, FALSE);
}

/**
 * raw_ddl_creator_execute
 * @ddlc: a #RawDDLCreator object
 * @error: a place to store errors, or %NULL
 *
 *
 */
gboolean
raw_ddl_creator_execute (RawDDLCreator *ddlc, GError **error)
{
	g_return_val_if_fail (RAW_IS_DDL_CREATOR (ddlc), FALSE);
  RawDDLCreatorPrivate *priv = raw_ddl_creator_get_instance_private (ddlc);

	if (!priv->cnc) {
		g_set_error (error, RAW_DDL_CREATOR_ERROR, RAW_DDL_CREATOR_NO_CONNECTION_ERROR,
			     "%s", "No connection specified");
		return FALSE;
	}
	if (!priv->d_mstruct) {
		g_set_error (error, RAW_DDL_CREATOR_ERROR, RAW_DDL_CREATOR_NO_CONNECTION_ERROR,
			     "%s", "No destination database objects specified");
		return FALSE;
	}

	/* begin transaction */
	if (!gda_connection_begin_transaction (priv->cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN,
					       error))
		return FALSE;

	/* execute operations */
	GdaServerProvider *prov = gda_connection_get_provider (priv->cnc);
	GSList *objlist, *list;
	objlist = gda_meta_struct_get_all_db_objects (priv->d_mstruct);
	for (list = objlist; list; list = list->next) {
		GdaServerOperation *op;
		op = prepare_dbo_server_operation (ddlc, prov, NULL, GDA_META_DB_OBJECT (list->data), error);
		if (!op)
			goto onerror;
		else {
			if (!gda_server_provider_perform_operation (prov, priv->cnc, op, error)) {
				g_object_unref (op);
				goto onerror;
			}
		}
		g_object_unref (op);
	}

	if (!gda_connection_commit_transaction (priv->cnc, NULL, error))
		goto onerror;

	return TRUE;

 onerror:
	gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
	return FALSE;
}

