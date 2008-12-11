/* gda-ddl-creator.c
 *
 * Copyright (C) 2008 Vivien Malerba
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gda-ddl-creator.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-meta-struct-private.h>
#include <libgda/gda-config.h>

#include <libgda/sql-parser/gda-statement-struct-util.h>

/*
 * Main static functions
 */
static void gda_ddl_creator_class_init (GdaDDLCreatorClass *klass);
static void gda_ddl_creator_init (GdaDDLCreator *creator);
static void gda_ddl_creator_dispose (GObject *object);
static void gda_ddl_creator_finalize (GObject *object);

static void gda_ddl_creator_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_ddl_creator_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static gboolean load_customization (GdaDDLCreator *ddlc, const gchar *xml_spec_file, GError **error);
static GdaServerOperation *prepare_dbo_server_operation (GdaDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, 
							 GdaMetaDbObject *dbo, GError **error);
static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;

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

struct _GdaDDLCreatorPrivate {
	GdaConnection *cnc;
	GdaMetaStruct *d_mstruct;

	GHashTable    *provider_specifics; /* key = a ProviderSpecificKey , value = a ProviderSpecificValue */
	GValue *catalog;
	GValue *schema;
	gchar *quoted_catalog;
	gchar *quoted_schema;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;


/* properties */
enum {
	PROP_0,
	PROP_CNC_OBJECT,
	PROP_CATALOG,
	PROP_SCHEMA
};

/* module error */
GQuark gda_ddl_creator_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_ddl_creator_error");
	return quark;
}

GType
gda_ddl_creator_get_type (void) {
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDDLCreatorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_ddl_creator_class_init,
			NULL,
			NULL,
			sizeof (GdaDDLCreator),
			0,
			(GInstanceInitFunc) gda_ddl_creator_init
		};
		
		g_static_rec_mutex_lock (&init_mutex);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDDLCreator", &info, 0);
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
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
gda_ddl_creator_class_init (GdaDDLCreatorClass *klass) 
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_static_rec_mutex_lock (&init_mutex);
	parent_class = g_type_class_peek_parent (klass);
		
	/* Properties */
	object_class->set_property = gda_ddl_creator_set_property;
	object_class->get_property = gda_ddl_creator_get_property;
	g_object_class_install_property (object_class, PROP_CNC_OBJECT,
		g_param_spec_object ("cnc", NULL, _ ("Connection object"), GDA_TYPE_CONNECTION,
		(G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_CATALOG,
		g_param_spec_string ("catalog", NULL, _ ("Catalog in which the database objects will be created"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_SCHEMA,
		g_param_spec_string ("schema", NULL, _ ("Schema in which the database objects will be created"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	
	object_class->dispose = gda_ddl_creator_dispose;
	object_class->finalize = gda_ddl_creator_finalize;
	
	g_static_rec_mutex_unlock (&init_mutex);
}


static void
gda_ddl_creator_init (GdaDDLCreator *creator) 
{
	creator->priv = g_new0 (GdaDDLCreatorPrivate, 1);
	creator->priv->cnc = NULL;
	creator->priv->d_mstruct = NULL;
	creator->priv->provider_specifics = g_hash_table_new_full (ProviderSpecific_hash, ProviderSpecific_equal,
								   (GDestroyNotify) ProviderSpecific_key_free,
								   (GDestroyNotify) ProviderSpecific_value_free);
	creator->priv->catalog = NULL;
	creator->priv->schema = NULL;
	creator->priv->quoted_catalog = NULL;
	creator->priv->quoted_schema = NULL;
}

static void
gda_ddl_creator_dispose (GObject *object) 
{
	GdaDDLCreator *creator;
	
	g_return_if_fail (GDA_IS_DDL_CREATOR (object));
	
	creator = GDA_DDL_CREATOR (object);
	if (creator->priv) {
		if (creator->priv->catalog)
			gda_value_free (creator->priv->catalog);
		if (creator->priv->schema)
			gda_value_free (creator->priv->schema);
		g_free (creator->priv->quoted_catalog);
		g_free (creator->priv->quoted_schema);

		if (creator->priv->cnc) {
			g_object_unref (creator->priv->cnc);
			creator->priv->cnc = NULL;
		}

		if (creator->priv->d_mstruct) {
			g_object_unref (creator->priv->d_mstruct);
			creator->priv->d_mstruct = NULL;
		}
	}
	
	/* parent class */
	parent_class->dispose (object);
}

static void
gda_ddl_creator_finalize (GObject *object)
{
	GdaDDLCreator *creator;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DDL_CREATOR (object));
	
	creator = GDA_DDL_CREATOR (object);
	if (creator->priv) {
		if (creator->priv->provider_specifics)
			g_hash_table_destroy (creator->priv->provider_specifics);
		g_free (creator->priv);
		creator->priv = NULL;
	}
	
	/* parent class */
	parent_class->finalize (object);
}

static void
gda_ddl_creator_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec) 
{
	GdaDDLCreator *creator;
	
	creator = GDA_DDL_CREATOR (object);
	if (creator->priv) {
		switch (param_id) {
		case PROP_CNC_OBJECT:
			if (creator->priv->cnc)
				g_object_unref (creator->priv->cnc);

			creator->priv->cnc = g_value_get_object (value);
			if (creator->priv->cnc)
				g_object_ref (creator->priv->cnc);
			break;
		case PROP_CATALOG:
			if (creator->priv->catalog)
				gda_value_free (creator->priv->catalog);
			creator->priv->catalog = NULL;
			g_free (creator->priv->quoted_catalog);
			creator->priv->quoted_catalog = NULL;
			if (g_value_get_string (value) && *g_value_get_string (value)) {
				creator->priv->catalog = gda_value_copy (value);
				if (gda_sql_identifier_needs_quotes (g_value_get_string (value))) 
					creator->priv->quoted_catalog = gda_sql_identifier_add_quotes (g_value_get_string (value));
				else
					creator->priv->quoted_catalog = g_value_dup_string (value);
			}
			break;
		case PROP_SCHEMA:
			if (creator->priv->schema)
				gda_value_free (creator->priv->schema);
			creator->priv->schema = NULL;
			g_free (creator->priv->quoted_schema);
			creator->priv->quoted_schema = NULL;
			if (g_value_get_string (value) && *g_value_get_string (value)) {
				creator->priv->schema = gda_value_copy (value);
				if (gda_sql_identifier_needs_quotes (g_value_get_string (value))) 
					creator->priv->quoted_schema = gda_sql_identifier_add_quotes (g_value_get_string (value));
				else
					creator->priv->quoted_schema = g_value_dup_string (value);
			}
			break;
		}
	}
}

static void
gda_ddl_creator_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec) 
{
	GdaDDLCreator *creator;
	creator = GDA_DDL_CREATOR (object);
	
	if (creator->priv) {
		switch (param_id) {
		case PROP_CNC_OBJECT:
			g_value_set_object (value, (GObject *) creator->priv->cnc);
			break;
		case PROP_CATALOG:
			if (creator->priv->catalog)
				g_value_set_string (value, g_value_get_string (creator->priv->catalog));
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_SCHEMA:
			if (creator->priv->schema)
				g_value_set_string (value, g_value_get_string (creator->priv->schema));
			else
				g_value_set_string (value, NULL);
			break;
		}
	}
}

/**
 * gda_ddl_creator_new
 * Create a new #GdaDDLCreator object
 *
 * Returns: the newly created object
 */
GdaDDLCreator *
gda_ddl_creator_new (void) 
{
	return (GdaDDLCreator *) g_object_new (GDA_TYPE_DDL_CREATOR, NULL);
}

/**
 * gda_ddl_creator_set_dest_from_file
 * @ddlc: a #GdaDDLCreator object
 * @xml_spec_file: a file name
 * @error: a place to store errors, or %NULL
 *
 * Sets the destination structure
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_ddl_creator_set_dest_from_file (GdaDDLCreator *ddlc, const gchar *xml_spec_file, GError **error)
{
	g_return_val_if_fail (GDA_IS_DDL_CREATOR (ddlc), FALSE);
	g_return_val_if_fail (xml_spec_file && *xml_spec_file, FALSE);

	if (ddlc->priv->d_mstruct)
		g_object_unref (ddlc->priv->d_mstruct);

	ddlc->priv->d_mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, NULL);

	if (!gda_meta_struct_load_from_xml_file (ddlc->priv->d_mstruct, NULL, NULL, xml_spec_file, error) ||
	    !load_customization (ddlc, xml_spec_file, error)) {
		g_object_unref (ddlc->priv->d_mstruct);
		ddlc->priv->d_mstruct = NULL;
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean
load_customization (GdaDDLCreator *ddlc, const gchar *xml_spec_file, GError **error)
{
	xmlNodePtr node;
	xmlDocPtr doc;

	/* load information schema's structure XML file */
	doc = xmlParseFile (xml_spec_file);
	if (!doc) {
		g_set_error (error, GDA_DDL_CREATOR_ERROR, GDA_DDL_CREATOR_SPECFILE_NOT_FOUND_ERROR,
			     _("Could not load file '%s'"), xml_spec_file);
		return FALSE;
	}
	
	node = xmlDocGetRootElement (doc);
	if (!node || strcmp ((gchar *) node->name, "schema")) {
		g_set_error (error, GDA_DDL_CREATOR_ERROR, GDA_DDL_CREATOR_INCORRECT_SCHEMA_ERROR,
			     _("Root node of file '%s' should be <schema>."), xml_spec_file);
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
						g_hash_table_insert (ddlc->priv->provider_specifics, key, val);
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

static GdaServerOperation *create_server_operation_for_table (GdaDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, 
							      GdaMetaDbObject *dbobj, GError **error);
static GdaServerOperation *create_server_operation_for_view (GdaDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, 
							     GdaMetaDbObject *dbobj, GError **error);

static GdaServerOperation *
prepare_dbo_server_operation (GdaDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, GdaMetaDbObject *dbo, GError **error)
{
	GdaServerOperation *op = NULL;
	switch (dbo->obj_type) {
	case GDA_META_DB_UNKNOWN:
		g_set_error (error, GDA_DDL_CREATOR_ERROR, GDA_DDL_CREATOR_INCORRECT_SCHEMA_ERROR,
			     _("Unknown database object '%s'"), dbo->obj_full_name);
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
static void
meta_table_column_foreach_attribute_func (const gchar *att_name, const GValue *value, FData *fdata)
{
	if (!fdata->allok)
		return;
	if (!strcmp (att_name, GDA_ATTRIBUTE_AUTO_INCREMENT) && 
	    (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN) && 
	    g_value_get_boolean (value)) {
		fdata->allok = gda_server_operation_set_value_at (fdata->op, "TRUE", fdata->error,
								  "/FIELDS_A/@COLUMN_AUTOINC/%d", 
								  fdata->index);
	}
}

static GdaServerOperation *
create_server_operation_for_table (GdaDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, 
				   GdaMetaDbObject *dbobj, GError **error)
{
	GdaServerOperation *op;
	GSList *list;
	gint index;
	const gchar *repl;

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
		repl = provider_specific_match (ddlc->priv->provider_specifics, prov, tcol->column_type ? tcol->column_type : "string",
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
		gda_meta_table_column_foreach_attribute (tcol, (GdaAttributesManagerFunc) meta_table_column_foreach_attribute_func,
							 &fdata);
		if (!fdata.allok)
			goto onerror;

		repl = provider_specific_match (ddlc->priv->provider_specifics, prov, "dummy", "/FIELDS_A/@COLUMN_PKEY");
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

#ifdef GDA_DEBUG_NO
	{
		xmlNodePtr node;
		xmlBufferPtr buffer;
		node = gda_server_operation_save_data_to_xml (op, NULL);
		buffer = xmlBufferCreate ();
		xmlNodeDump (buffer, NULL, node, 5, 1);
		xmlFreeNode (node);
		xmlBufferDump (stdout, buffer);
		xmlBufferFree (buffer);
	}
#endif

	return op;
 onerror:
	g_object_unref (op);
	return NULL;
}

static GdaServerOperation *
create_server_operation_for_view (GdaDDLCreator *ddlc, GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaMetaDbObject *dbobj, GError **error)
{
	GdaServerOperation *op;

	op = gda_server_provider_create_operation (prov, cnc, dbobj->obj_type, NULL, error);
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
 * gda_ddl_creator_set_connection
 * @ddlc: a #GdaDDLCreator object
 * @cnc: a #GdaConnection object or %NULL
 *
 *
 */
void
gda_ddl_creator_set_connection (GdaDDLCreator *ddlc, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_DDL_CREATOR (ddlc));
	g_return_if_fail (!cnc || GDA_IS_CONNECTION (cnc));

	g_object_set (G_OBJECT (ddlc), "cnc", cnc, NULL);
}

/**
 * gda_ddl_creator_get_sql
 * @ddlc: a #GdaDDLCreator object
 * @error: a place to store errors, or %NULL
 *
 * Get the SQL code which would be executed to create the database objects
 *
 * Returns: a new string if no error occurred, or %NULL
 */
gchar *
gda_ddl_creator_get_sql (GdaDDLCreator *ddlc, GError **error)
{
	GString *string;
	gchar *sql;
	g_return_val_if_fail (GDA_IS_DDL_CREATOR (ddlc), NULL);
	g_return_val_if_fail (ddlc->priv, NULL);
	if (!ddlc->priv->cnc) {
		g_set_error (error, GDA_DDL_CREATOR_ERROR, GDA_DDL_CREATOR_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	/* render operations to SQL */
	GdaServerProvider *prov = gda_connection_get_provider (ddlc->priv->cnc);
	GSList *objlist, *list;
	objlist = gda_meta_struct_get_all_db_objects (ddlc->priv->d_mstruct);

	string = g_string_new ("");
	for (list = objlist; list; list = list->next) {
		GdaServerOperation *op;
		op = prepare_dbo_server_operation (ddlc, prov, NULL, GDA_META_DB_OBJECT (list->data), error);
		if (!op) {
			g_string_free (string, TRUE);
			return NULL;
		}
		else {
			sql = gda_server_provider_render_operation (prov, ddlc->priv->cnc, op, error);
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

	sql = string->str;
	g_string_free (string, FALSE);
	return sql;
}

/**
 * gda_ddl_creator_execute
 * @ddlc: a #GdaDDLCreator object
 * @error: a place to store errors, or %NULL
 *
 * 
 */
gboolean
gda_ddl_creator_execute (GdaDDLCreator *ddlc, GError **error)
{
	g_return_val_if_fail (GDA_IS_DDL_CREATOR (ddlc), FALSE);
	g_return_val_if_fail (ddlc->priv, FALSE);
	if (!ddlc->priv->cnc) {
		g_set_error (error, GDA_DDL_CREATOR_ERROR, GDA_DDL_CREATOR_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return FALSE;
	}
	if (!ddlc->priv->d_mstruct) {
		g_set_error (error, GDA_DDL_CREATOR_ERROR, GDA_DDL_CREATOR_NO_CONNECTION_ERROR,
			     "%s", _("No destination database objects specified"));
		return FALSE;
	}

	/* begin transaction */
	if (!gda_connection_begin_transaction (ddlc->priv->cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN,
					       error))
		return FALSE;

	/* execute operations */
	GdaServerProvider *prov = gda_connection_get_provider (ddlc->priv->cnc);
	GSList *objlist, *list;
	objlist = gda_meta_struct_get_all_db_objects (ddlc->priv->d_mstruct);
	for (list = objlist; list; list = list->next) {
		GdaServerOperation *op;
		op = prepare_dbo_server_operation (ddlc, prov, NULL, GDA_META_DB_OBJECT (list->data), error);
		if (!op) 
			goto onerror;
		else {
			if (!gda_server_provider_perform_operation (prov, ddlc->priv->cnc, op, error)) {
				g_object_unref (op);
				goto onerror;
			}
		}
		g_object_unref (op);
	}

	if (!gda_connection_commit_transaction (ddlc->priv->cnc, NULL, error))
		goto onerror;

	return TRUE;

 onerror:
	gda_connection_rollback_transaction (ddlc->priv->cnc, NULL, NULL);
	return FALSE;
}

