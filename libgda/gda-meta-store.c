/* gda-meta-store.c
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
#include <libgda/gda-meta-store.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gda-marshal.h"
#include <stdarg.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-util.h>

/*
 * Main static functions
 */
static void gda_meta_store_class_init (GdaMetaStoreClass *klass);
static GObject *gda_meta_store_constructor (GType type,
					    guint n_construct_properties,
					    GObjectConstructParam *construct_properties);
static void gda_meta_store_init (GdaMetaStore *store);
static void gda_meta_store_dispose (GObject *object);
static void gda_meta_store_finalize (GObject *object);

static void gda_meta_store_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_meta_store_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static gboolean initialize_cnc_struct (GdaMetaStore *store, GError **error);
static void gda_meta_store_change_free (GdaMetaStoreChange *change);

/* simple predefined statements */
enum {
	STMT_GET_VERSION,
	STMT_SET_VERSION,
	
	STMT_LAST
} PreStmtType;

/*
 * Complements the DbObject structure, for tables only
 * contains predefined statements for data selection and modifications
 */
typedef struct {
	GSList       *columns;

	/* data access statements */
	GdaStatement *current_all;
	GdaStatement *delete_all;
	
	GdaStatement *insert;
	GdaStatement *update;
	GdaStatement *delete;
	
	/* a single #GdaSet for all the statements above */
	GdaSet       *params;
	
	/* PK fields index */
	gint         *pk_cols_array;
	gint          pk_cols_nb;

	/* GType for each column of the SELECT statement */
	GType        *type_cols_array; /* terminated with G_TYPE_NONE */

	/* fk_list */
	GSList       *reverse_fk_list; /* list of TableFKey where @depend_on == this DbObject */
	GSList       *fk_list; /* list of TableFKey where @table_info == this DbObject */
} TableInfo;
static void table_info_free_contents (TableInfo *info);

/*
 * Complements the DbObject structure, for views only
 * contains view definition
 */
typedef struct {
	gchar        *view_def;
}  ViewInfo;
static void view_info_free_contents (ViewInfo *info);

/* 
 * Struture to hold information about each object
 * which can be created in the internal GdaMetaStore's connection.
 * It is available for tables, views, triggers, ...
 */
typedef struct {
	GdaServerOperationType  obj_type;
	gchar                  *obj_name; /* may be %NULL */
	GdaServerOperation     *create_op; /* may be %NULL */
	GSList                 *depend_list; /* list of DbObject pointers on which this object depends */

	union {
		TableInfo         table_info;
		ViewInfo          view_info;
	}                       extra;
} DbObject;
#define DB_OBJECT(x) ((DbObject*)(x))
#define TABLE_INFO(dbobj) (&((dbobj)->extra.table_info))
#define VIEW_INFO(dbobj) (&((dbobj)->extra.view_info))

typedef struct {
	gchar        *column_name;
	gchar        *column_type;
	GType         gtype;
	gboolean      pkey;
        gboolean      nullok; 
} TableColumn;
static void table_column_free (TableColumn *tcol);
#define TABLE_COLUMN(x) ((TableColumn*)(x))

typedef struct {
	DbObject     *table_info;
	DbObject     *depend_on;

	gint          cols_nb;	
	gint         *fk_cols_array; /* FK fields index */
	gchar       **fk_names_array; /* FK fields names */
	gint         *ref_pk_cols_array; /* Ref PK fields index */
	gchar       **ref_pk_names_array; /* Ref PK fields names */

	gchar        *fk_fields_cond;
} TableFKey;
static void table_fkey_free (TableFKey *tfk);

/* statements defined by the @... arguments of the gda_meta_store_modify() function */
typedef struct {
	/* actual statements */
	GdaStatement *select;
	GdaStatement *delete;

	/* a single #GdaSet for all the statements above */
	GdaSet       *params;
} TableConditionInfo;

struct _GdaMetaStoreClassPrivate {
	GdaSqlParser  *parser;;
	GdaStatement **prep_stmts; /* Simple prepared statements, of size STMT_LAST, general usage */

	/* Internal database's schema information */
	GSList        *db_objects; /* list of DbObject structures */
	GHashTable    *db_objects_hash; /* key = table name, value = a DbObject structure */
        GHashTable    *table_cond_info_hash; /* key = string composed of the column names of
					      * the parameters separated by a period, 
					      * value = a TableConditionInfo structure */
	GHashTable    *provider_specifics; /* key = a ProviderSpecificKey , value = a ProviderSpecificValue) */
};

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

struct _GdaMetaStorePrivate {
	GdaConnection *cnc;
	gint           version;
	gboolean       schema_ok;
};

static void db_object_free    (DbObject *dbobj);
static void create_db_objects (GdaMetaStoreClass *klass);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum {
	SUGGEST_UPDATE,
	META_CHANGED,
	LAST_SIGNAL
};

static gint gda_meta_store_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum {
	PROP_0,
	PROP_CNC_STRING,
	PROP_CNC_OBJECT,
};

/* module error */
GQuark gda_meta_store_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_meta_store_error");
	return quark;
}


GType
gda_meta_store_get_type (void) {
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaMetaStoreClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_meta_store_class_init,
			NULL,
			NULL,
			sizeof (GdaMetaStore),
			0,
			(GInstanceInitFunc) gda_meta_store_init
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GdaMetaStore", &info, 0);
	}
	return type;
}

static GdaStatement *
compute_prepared_stmt (GdaSqlParser *parser, const gchar *sql) {
	GdaStatement *stmt;
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_warning ("INTERNAL GdaMetaStore error: could not parse internal statement '%s'", sql);
	return stmt;
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
gda_meta_store_class_init (GdaMetaStoreClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);
	
	gda_meta_store_signals[SUGGEST_UPDATE] =
		g_signal_new ("suggest_update",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GdaMetaStoreClass, suggest_update),
		NULL, NULL,
		gda_marshal_VOID__POINTER, G_TYPE_NONE,
		1, G_TYPE_POINTER);
	gda_meta_store_signals[META_CHANGED] =
		g_signal_new ("meta_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GdaMetaStoreClass, meta_changed),
		NULL, NULL,
		gda_marshal_VOID__POINTER, G_TYPE_NONE,
		1, G_TYPE_POINTER);
	
	klass->suggest_update = NULL;
	klass->meta_changed = NULL;
	
	/* Properties */
	object_class->set_property = gda_meta_store_set_property;
	object_class->get_property = gda_meta_store_get_property;
	g_object_class_install_property (object_class, PROP_CNC_STRING,
		g_param_spec_string ("cnc-string", _ ("Connection string for the internal connection to use"), NULL, NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_CNC_OBJECT,
		g_param_spec_object ("cnc", _ ("Connection object internally used"),
		NULL, GDA_TYPE_CONNECTION,
		(G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	
	object_class->constructor = gda_meta_store_constructor;
	object_class->dispose = gda_meta_store_dispose;
	object_class->finalize = gda_meta_store_finalize;
	
	/* Class private data */
	klass->cpriv = g_new0 (GdaMetaStoreClassPrivate, 1);
	klass->cpriv->prep_stmts = g_new0 (GdaStatement *, STMT_LAST);
	klass->cpriv->parser = gda_sql_parser_new ();
	klass->cpriv->provider_specifics = g_hash_table_new (ProviderSpecific_hash, ProviderSpecific_equal);
	klass->cpriv->db_objects_hash = g_hash_table_new (g_str_hash, g_str_equal);
	create_db_objects (klass);
        klass->cpriv->table_cond_info_hash = g_hash_table_new (g_str_hash, g_str_equal);

	klass->cpriv->prep_stmts[STMT_SET_VERSION] =
		compute_prepared_stmt (klass->cpriv->parser, 
				       "INSERT INTO _attributes (att_name, att_value) VALUES ('_schema_version', '1')");
	klass->cpriv->prep_stmts[STMT_GET_VERSION] =
		compute_prepared_stmt (klass->cpriv->parser, 
				       "SELECT att_value FROM _attributes WHERE att_name='_schema_version'");

#ifdef GDA_DEBUG_GRAPH
#define INFORMATION_SCHEMA_GRAPH_FILE "information_schema.dot"
	GString *string;
	GSList *list;
	string = g_string_new ("digraph G {\nrankdir = BT;\nnode [shape = box];\n");
	for (list = klass->cpriv->db_objects; list; list = list->next) {
		DbObject *dbo = (DbObject*) list->data;
		switch (dbo->obj_type) {
		case GDA_SERVER_OPERATION_CREATE_TABLE:
			g_string_append_printf (string, "%s;\n", dbo->obj_name);
			break;
		case GDA_SERVER_OPERATION_CREATE_VIEW:
			g_string_append_printf (string, "%s [ shape = ellipse ];\n", dbo->obj_name);
			break;
		default:
			g_string_append_printf (string, "%s [ shape = diamond ];\n", dbo->obj_name);
			break;
		}

		GSList *dep_list;
		for (dep_list = dbo->depend_list; dep_list; dep_list = dep_list->next)
			g_string_append_printf (string, "%s -> %s\n", dbo->obj_name,
						((DbObject*) dep_list->data)->obj_name);
	}
	g_string_append_c (string, '}');
	GError *lerror = NULL;
	if (g_file_set_contents (INFORMATION_SCHEMA_GRAPH_FILE, string->str, -1, &lerror))
		g_print ("Information schema graph written to '%s'\n"
			 "Use 'dot' (from the GraphViz package) to create a picture, for example:\n"
			 "\tdot -Tpng -o graph.png %s\n", INFORMATION_SCHEMA_GRAPH_FILE, INFORMATION_SCHEMA_GRAPH_FILE);
	else {
		g_print ("Information schema graph not written to '%s': %s\n", INFORMATION_SCHEMA_GRAPH_FILE,
			 lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
	}
	g_string_free (string, TRUE);
#endif
}


static void
gda_meta_store_init (GdaMetaStore *store) {
	store->priv = g_new0 (GdaMetaStorePrivate, 1);
	store->priv->cnc = NULL;
	store->priv->schema_ok = FALSE;
	store->priv->version = 0;
}

static GObject *
gda_meta_store_constructor (GType type,
	guint n_construct_properties,
	GObjectConstructParam *construct_properties) {
	GObject *object;
	gint i;
	GError *error = NULL;
	GdaMetaStore *store;
	gboolean been_specified = FALSE;
	
	object = G_OBJECT_CLASS (parent_class)->constructor (type,
		n_construct_properties,
		construct_properties);
	for (i = 0; i< n_construct_properties; i++) {
		GObjectConstructParam *prop = &(construct_properties[i]);
		if (!strcmp (g_param_spec_get_name (prop->pspec), "cnc-string")) {
			if (g_value_get_string (prop->value))
				been_specified = TRUE;
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "system_file")) {
			if (g_value_get_pointer (prop->value))
				been_specified = TRUE;
		}
	}
	store = (GdaMetaStore *) object;
	
	if (!store->priv->cnc && !been_specified)
		/* in memory DB */
		g_object_set (object, "cnc-string", "SQLite://DB_DIR=.;DB_NAME=:memory:", NULL);
	
	if (store->priv->cnc) {
		store->priv->schema_ok = initialize_cnc_struct (store, &error);
		if (! store->priv->schema_ok) {
			g_warning ("GdaMetaStore not initialized, reported error is: %s\n",
				   error && error->message ? error->message : _ ("No detail"));
			if (error)
				g_error_free (error);
		}
	}
	
	return object;
}

/**
 * gda_meta_store_new_with_file
 * @file_name: a file name
 *
 * Create a new #GdaMetaStore object using @file_name as its internal
 * database
 *
 * Returns: the newly created object, or %NULL if an error occurred
 */
GdaMetaStore *
gda_meta_store_new_with_file (const gchar *file_name) 
{
	gchar *string;
	gchar *base, *dir;
	GdaMetaStore *store;
	
	g_return_val_if_fail (file_name && *file_name, NULL);
	
	base = g_path_get_basename (file_name);
	dir = g_path_get_dirname (file_name);
	if (g_str_has_suffix (base, ".db"))
		base [strlen (base) - 3] = 0;
	string = g_strdup_printf ("SQLite://DB_DIR=%s;DB_NAME=%s", dir, base);
	g_free (base);
	g_free (dir);
	store = gda_meta_store_new (string);
	g_free (string);
	return store;
}

/**
 * gda_meta_store_new
 * @string: a connection string, or %NULL for an in-memory internal database
 *
 * Create a new #GdaMetaStore object.
 *
 * Returns: the newly created object, or %NULL if an error occurred
 */
GdaMetaStore *
gda_meta_store_new (const gchar *string) 
{
	GObject *obj;
	GdaMetaStore *store;
	
	if (string)
		obj = g_object_new (GDA_TYPE_META_STORE, "cnc-string", string, NULL);
	else
		obj = g_object_new (GDA_TYPE_META_STORE, "cnc-string", "SQLite://DB_NAME=:memory:", NULL);
	store = GDA_META_STORE (obj);
	if (!store->priv->cnc) {
		g_object_unref (store);
		store = NULL;
	}
	
	return store;
}

static void
gda_meta_store_dispose (GObject *object) 
{
	GdaMetaStore *store;
	
	g_return_if_fail (GDA_IS_META_STORE (object));
	
	store = GDA_META_STORE (object);
	if (store->priv) {
		/* internal connection */
		if (store->priv->cnc) {
			g_object_unref (G_OBJECT (store->priv->cnc));
			store->priv->cnc = NULL;
		}
	}
	
	/* parent class */
	parent_class->dispose (object);
}

static void
gda_meta_store_finalize (GObject *object)
{
	GdaMetaStore *store;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_META_STORE (object));
	
	store = GDA_META_STORE (object);
	if (store->priv) {
		g_free (store->priv);
		store->priv = NULL;
	}
	
	/* parent class */
	parent_class->finalize (object);
}

static void
gda_meta_store_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec) 
{
	GdaMetaStore *store;
	const gchar *cnc_string;
	
	store = GDA_META_STORE (object);
	if (store->priv) {
		switch (param_id) {
			case PROP_CNC_STRING:
				if (!store->priv->cnc) {
					cnc_string = g_value_get_string (value);
					if (cnc_string) {
						GdaConnection *cnc;
						cnc = gda_connection_open_from_string (NULL, cnc_string, NULL, 0, NULL);
						store->priv->cnc = cnc;
					}
				}
				break;
			case PROP_CNC_OBJECT:
				if (!store->priv->cnc)
					store->priv->cnc = g_value_get_object (value);
				break;
		}
	}
}

static void
gda_meta_store_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec) 
{
	GdaMetaStore *store;
	store = GDA_META_STORE (object);
	
	if (store->priv) {
		switch (param_id) {
			case PROP_CNC_STRING:
				g_assert_not_reached ();
				break;
			case PROP_CNC_OBJECT:
				g_value_set_object (value, (GObject *) store->priv->cnc);
				break;
		}
	}
}

/*
 * Checks the structure of @store->priv->cnc and update it if necessary
 */
static gboolean prepare_server_operations (GdaMetaStore *store, GError **error);
static gboolean handle_schema_version (GdaMetaStore *store, gboolean *schema_present, GError **error);
static gboolean
initialize_cnc_struct (GdaMetaStore *store, GError **error)
{
	gboolean schema_present;
	GdaMetaStoreClass *klass;
	g_return_val_if_fail (GDA_IS_CONNECTION (store->priv->cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (store->priv->cnc), FALSE);
	
	if (handle_schema_version (store, &schema_present, error))
		return TRUE;
	if (schema_present)
		return FALSE;
	
	if (error && *error) {
		g_error_free (*error);
		*error = NULL;
	}
	
	/* assume schema not present => create it */
	klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	GSList *list;
	gboolean allok = TRUE;
	GdaServerProvider *prov;
	if (!prepare_server_operations (store, error))
		return FALSE;

	prov = gda_connection_get_provider_obj (store->priv->cnc);
	for (list = klass->cpriv->db_objects; list; list = list->next) {
		DbObject *dbo = DB_OBJECT (list->data);
		/*g_print ("Creating object: %s\n", dbo->obj_name);*/
		if (dbo->create_op) {
			if (!gda_server_provider_perform_operation (prov, store->priv->cnc, dbo->create_op, error)) {
				allok = FALSE;
				break;
			}
			g_object_unref (dbo->create_op);
			dbo->create_op = NULL;
		}
	}
	if (!allok)
		return FALSE;

	/* set version info, version 1 for now */
	if (gda_connection_statement_execute_non_select (store->priv->cnc,
							 klass->cpriv->prep_stmts[STMT_SET_VERSION],
							 NULL, NULL, NULL) == -1) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA,
			_ ("Could not set the internal schema's version"));
		return FALSE;
	}
	
	return handle_schema_version (store, NULL, error);
}


static GdaServerOperation *create_server_operation_for_table (GHashTable *specific_hash, 
							      GdaServerProvider *prov, GdaConnection *cnc, 
							      DbObject *dbobj, GError **error);
static GdaServerOperation *create_server_operation_for_view  (GHashTable *specific_hash, 
							      GdaServerProvider *prov, GdaConnection *cnc, 
							      DbObject *dbobj, GError **error);
static gboolean
prepare_server_operations (GdaMetaStore *store, GError **error)
{
	GSList *objects;
	GdaServerProvider *prov;
	GdaMetaStoreClass *klass;

	klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	prov = gda_connection_get_provider_obj (store->priv->cnc);

	for (objects = klass->cpriv->db_objects; objects; objects = objects->next) {
		DbObject *dbo = DB_OBJECT (objects->data);
		if (dbo->create_op) {
			g_object_unref (dbo->create_op);
			dbo->create_op = NULL;
		}
		
		switch (dbo->obj_type) {
		case GDA_SERVER_OPERATION_CREATE_TABLE:
			dbo->create_op = create_server_operation_for_table (klass->cpriv->provider_specifics, 
									    prov, store->priv->cnc, dbo, error);
			if (!dbo->create_op)
				return FALSE;
			break;
		case GDA_SERVER_OPERATION_CREATE_VIEW:
			dbo->create_op = create_server_operation_for_view (klass->cpriv->provider_specifics,
									   prov, store->priv->cnc, dbo, error);
			if (!dbo->create_op)
				return FALSE;
			break;
		default:
			break;
		}
	}

	return TRUE;
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

static GdaServerOperation *
create_server_operation_for_table (GHashTable *specific_hash, 
				   GdaServerProvider *prov, GdaConnection *cnc, DbObject *dbobj, GError **error)
{
	GdaServerOperation *op;
	GSList *list;
	gint index;
	const gchar *repl;

	op = gda_server_provider_create_operation (prov, cnc, dbobj->obj_type, NULL, error);
	if (!op)
		return NULL;
	if (! gda_server_operation_set_value_at (op, dbobj->obj_name, error, "/TABLE_DEF_P/TABLE_NAME"))
		goto onerror;

	/* columns */
	for (index = 0, list = TABLE_INFO (dbobj)->columns; list; list = list->next, index++) {
		TableColumn *tcol = TABLE_COLUMN (list->data);
		if (! gda_server_operation_set_value_at (op, tcol->column_name, error,
							 "/FIELDS_A/@COLUMN_NAME/%d", index))
			goto onerror;
		repl = provider_specific_match (specific_hash, prov, tcol->column_type ? tcol->column_type : "string",
						"/FIELDS_A/@COLUMN_TYPE");
		if (! gda_server_operation_set_value_at (op, repl ? repl : "string", error,
							 "/FIELDS_A/@COLUMN_TYPE/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, tcol->nullok ? "FALSE" : "TRUE", error,
							 "/FIELDS_A/@COLUMN_NNUL/%d", index))
			goto onerror;
		repl = provider_specific_match (specific_hash, prov, NULL, "/FIELDS_A/@COLUMN_PKEY");
		if (repl) {
			if (! gda_server_operation_set_value_at (op, tcol->pkey ? "TRUE" : "FALSE", error,
								 "/FIELDS_A/@COLUMN_PKEY/%d", index))
				goto onerror;
		}
	}

	/* foreign keys */
	repl = provider_specific_match (specific_hash, prov, NULL, "/FKEY_S");
	for (index = 0, list = TABLE_INFO (dbobj)->fk_list; repl && list; list = list->next, index++) {
		TableFKey *tfk = (TableFKey*) list->data;
		gint i;

		if (! gda_server_operation_set_value_at (op, tfk->depend_on->obj_name, error,
							 "/FKEY_S/%d/FKEY_REF_TABLE", index))
			goto onerror;
		for (i = 0; i < tfk->cols_nb; i++) {
			TableColumn *tc = g_slist_nth_data (TABLE_INFO (dbobj)->columns, tfk->fk_cols_array [i]);
			if (! gda_server_operation_set_value_at (op, tc->column_name, error,
								 "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d",
								 index, i))
				goto onerror;
			if (! gda_server_operation_set_value_at (op, tfk->ref_pk_names_array [i], error,
								 "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
								 index, i))
				goto onerror;
		}
	}

	return op;
 onerror:
	g_object_unref (op);
	return NULL;
}

static GdaServerOperation *
create_server_operation_for_view (GHashTable *specific_hash, 
				  GdaServerProvider *prov, GdaConnection *cnc, DbObject *dbobj, GError **error)
{
	GdaServerOperation *op;

	op = gda_server_provider_create_operation (prov, cnc, dbobj->obj_type, NULL, error);
	if (!op)
		return NULL;
	if (! gda_server_operation_set_value_at (op, dbobj->obj_name, error, "/VIEW_DEF_P/VIEW_NAME"))
		goto onerror;
	if (! gda_server_operation_set_value_at (op, VIEW_INFO (dbobj)->view_def, error, "/VIEW_DEF_P/VIEW_DEF"))
		goto onerror;

	return op;
 onerror:
	g_object_unref (op);
	return NULL;
}

static DbObject *create_table_object (GdaMetaStoreClass *klass, xmlNodePtr node, GError **error);
static DbObject *create_view_object (GdaMetaStoreClass *klass, xmlNodePtr node, GError **error);
static GSList *reorder_db_objects (GSList *objects, GHashTable *hash);
static gboolean complement_db_objects (GSList *objects, GHashTable *hash, GError **error);

/*
 * Creates all DbObject structures and place them into klass->cpriv->db_objects
 */
static void
create_db_objects (GdaMetaStoreClass *klass)
{
	xmlNodePtr node;
	GError *lerror = NULL;
	GError **error = &lerror;
	gchar *file;
	xmlDocPtr doc;

	/* load information schema's structure XML file */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "information_schema.xml", NULL);
	doc = xmlParseFile (file);
	if (!doc) 
		g_error ("Missing or malformed file '%s', check your installation", file);
	
	node = xmlDocGetRootElement (doc);
	if (!node || strcmp ((gchar *) node->name, "schema")) 
		g_error ("Root node of file '%s' should be <schema>.", file);
	g_free (file);
	
	/* walk through the xmlDoc */
	klass->cpriv->db_objects = NULL;
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
				gboolean pname_used = FALSE;
				for (rnode = snode->children; rnode; rnode = rnode->next) {
					if (!strcmp ((gchar *) rnode->name, "replace") ||
					    !strcmp ((gchar *) rnode->name, "ignore")) {
						xmlChar *context;
						context = xmlGetProp (rnode, BAD_CAST "context");
						if (!context) {
							g_warning ("<%s> section ignored because no context specified",
								   snode->name);
							continue;
						}
						ProviderSpecificKey *key = g_new0 (ProviderSpecificKey, 1);
						ProviderSpecificValue *val = g_new0 (ProviderSpecificValue, 1);
						key->prov = (gchar *) pname; 
						pname_used = TRUE;
						key->path = (gchar *) context;
						key->expr = (gchar *) xmlGetProp (rnode, BAD_CAST "expr");
						val->repl = (gchar *) xmlGetProp (rnode, BAD_CAST "replace_with");
						g_hash_table_insert (klass->cpriv->provider_specifics, key, val);
						/*g_print ("RULE: %s, %s, %s => %s\n", key->prov,
						  key->path, key->expr, val->repl);*/
					}
				}
				if (!pname_used)
					xmlFree (pname);
			}
		}

		/* <table> tag for table creation */
		else if (!strcmp ((gchar *) node->name, "table")) {
			DbObject *dbo;
			dbo = create_table_object (klass, node, error);
			if (!dbo)
				g_error ("Information schema creation error: %s", 
					 lerror && lerror->message ? lerror->message : "No detail");
		}
		/* <view> tag for view creation */
		else if (!strcmp ((gchar *) node->name, "view")) {
			DbObject *dbo;
			dbo = create_view_object (klass, node, error);
			if (!dbo)
				g_error ("Information schema creation error: %s", 
					 lerror && lerror->message ? lerror->message : "No detail");
		}
	}
	xmlFreeDoc (doc);
	
	klass->cpriv->db_objects = reorder_db_objects (klass->cpriv->db_objects, klass->cpriv->db_objects_hash);
	if (!complement_db_objects (klass->cpriv->db_objects, klass->cpriv->db_objects_hash, error)) 
		g_error ("Information schema structure error: %s", 
			 lerror && lerror->message ? lerror->message : "No detail");
}

/*
 * Returns: -1 if column not found
 */
static gint
column_name_to_index (TableInfo *tinfo, const gchar *column_name)
{
	GSList *list;
	gint i;
	for (i = 0, list = tinfo->columns; list; list = list->next, i++) 
		if (!strcmp (TABLE_COLUMN (list->data)->column_name, column_name))
			return i;
	return -1;
}

static void compute_view_dependencies (GdaMetaStoreClass *klass, DbObject *view_dbobj, GdaSqlStatement *sqlst);
static DbObject *
create_view_object (GdaMetaStoreClass *klass, xmlNodePtr node, GError **error)
{
	DbObject *dbobj;
	xmlChar *view_name;
	view_name = xmlGetProp (node, BAD_CAST "name");
	if (!view_name) {
		g_set_error (error, 0, 0, 
			     _("Missing view name from <view> node"));
		goto onerror;
	}
	
	/* DbObject structure */
	dbobj = g_hash_table_lookup (klass->cpriv->db_objects_hash, view_name);
	if (!dbobj) {
		dbobj = g_new0 (DbObject, 1);
		klass->cpriv->db_objects = g_slist_prepend (klass->cpriv->db_objects, dbobj);
		dbobj->obj_name = g_strdup ((gchar *) view_name);
		g_hash_table_insert (klass->cpriv->db_objects_hash, dbobj->obj_name, dbobj);
	}
	xmlFree (view_name);
	dbobj->obj_type = GDA_SERVER_OPERATION_CREATE_VIEW;
	
	/* walk through the view attributes */
	xmlNodePtr cnode;
	for (cnode = node->children; cnode; cnode = cnode->next) {
		xmlChar *def;
		if (strcmp ((gchar *) cnode->name, "definition"))
			continue;
		def = xmlNodeGetContent (cnode);
		if (!def) {
			g_set_error (error, 0, 0, 
				     _("Missing view definition from <view> node"));
			goto onerror;
		}
		
		/* use a parser to analyse the view dependencies */
		GdaStatement *stmt;
		const gchar *remain;
		stmt = gda_sql_parser_parse_string (klass->cpriv->parser, (gchar *) def, &remain, error);
		if (!stmt) {
			xmlFree (def);
			goto onerror;
		}
		if (remain) {
			g_set_error (error, 0, 0, 
				     _("View definition contains more than one statement (for view '%s')"),
				     dbobj->obj_name);
			g_object_unref (stmt);
			xmlFree (def);
			goto onerror;
		}
		VIEW_INFO (dbobj)->view_def = g_strdup ((gchar *) def);
		xmlFree (def);
		
		if ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
		    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND)) {
			GdaSqlStatement *sqlst;
			g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
			compute_view_dependencies (klass, dbobj, sqlst);
			gda_sql_statement_free (sqlst);
			g_object_unref (stmt);
					
#ifdef GDA_DEBUG_NO
			g_print ("View %s depends on: ", dbobj->obj_name);
			GSList *list;
			for (list = dbobj->depend_list; list; list = list->next) 
				g_print ("%s ", DB_OBJECT (list->data)->obj_name);
			g_print ("\n");
#endif
		}
		else {
			g_set_error (error, 0, 0, 
				     _("View definition is not a selection statement (for view '%s')"), 
				     dbobj->obj_name);
			g_object_unref (stmt);
			goto onerror;	
		}
	}
	return dbobj;
	
onerror:
	db_object_free (dbobj);
	return NULL;	
}

static GdaSqlExpr *make_expr_EQUAL (GdaSqlAnyPart *parent, xmlChar *cname, xmlChar *type, GType ptype, gboolean nullok, gint index);
static GdaSqlExpr *make_expr_AND (GdaSqlAnyPart *parent, GdaSqlExpr *current);
static DbObject *
create_table_object (GdaMetaStoreClass *klass, xmlNodePtr node, GError **error)
{
	DbObject *dbobj;
	xmlChar *table_name;
	table_name = xmlGetProp (node, BAD_CAST "name");
	if (!table_name) {
		g_set_error (error, 0, 0, 
			     _("Missing table name from <table> node"));
		return NULL;
	}

	/* DbObject structure */
	dbobj = g_hash_table_lookup (klass->cpriv->db_objects_hash, table_name);
	if (!dbobj) {
		dbobj = g_new0 (DbObject, 1);
		klass->cpriv->db_objects = g_slist_prepend (klass->cpriv->db_objects, dbobj);
		dbobj->obj_name = g_strdup ((gchar *) table_name);
		g_hash_table_insert (klass->cpriv->db_objects_hash, dbobj->obj_name, dbobj);
	}
	xmlFree (table_name);
	dbobj->obj_type = GDA_SERVER_OPERATION_CREATE_TABLE;

	/* current_all */
	gchar *sql;
	sql = g_strdup_printf ("SELECT * FROM %s", dbobj->obj_name);
	TABLE_INFO (dbobj)->current_all = compute_prepared_stmt (klass->cpriv->parser, sql);
	g_free (sql);
	if (!TABLE_INFO (dbobj)->current_all) {
		g_set_error (error, 0, 0,
			     "Internal fatal error: could not create SELECT ALL statement (for table '%s')", 
			     dbobj->obj_name);
		goto onerror;
	}
	
	/* delete all */
	sql = g_strdup_printf ("DELETE FROM %s", dbobj->obj_name);
	TABLE_INFO (dbobj)->delete_all = compute_prepared_stmt (klass->cpriv->parser, sql);
	g_free (sql);
	if (!TABLE_INFO (dbobj)->delete_all) {
		g_set_error (error, 0, 0,
			     "Internal fatal error: could not create DELETE ALL statement (for table '%s')", 
			     dbobj->obj_name);
		goto onerror;
	}

	/* INSERT, UPDATE and DELETE statements */
	GdaSqlStatementInsert *ist;
	GdaSqlStatementUpdate *ust;
	GdaSqlStatementDelete *dst;
	ist = g_new0 (GdaSqlStatementInsert, 1);
	GDA_SQL_ANY_PART (ist)->type = GDA_SQL_ANY_STMT_INSERT;
	ust = g_new0 (GdaSqlStatementUpdate, 1);
	GDA_SQL_ANY_PART (ust)->type = GDA_SQL_ANY_STMT_UPDATE;
	dst = g_new0 (GdaSqlStatementDelete, 1);
	GDA_SQL_ANY_PART (dst)->type = GDA_SQL_ANY_STMT_DELETE;
	
	ist->table = gda_sql_table_new (GDA_SQL_ANY_PART (ist));
	ist->table->table_name = g_strdup ((gchar *) dbobj->obj_name);
	
	ust->table = gda_sql_table_new (GDA_SQL_ANY_PART (ust));
	ust->table->table_name = g_strdup ((gchar *) dbobj->obj_name);
	
	dst->table = gda_sql_table_new (GDA_SQL_ANY_PART (dst));
	dst->table->table_name = g_strdup ((gchar *) dbobj->obj_name);
	
	/* walk through the columns and Fkey nodes */
	xmlNodePtr cnode;
	gint colindex = 0;
	GSList *insert_values_list = NULL;
	for (cnode = node->children; cnode; cnode = cnode->next) {
		if (!strcmp ((gchar *) cnode->name, "column")) {
			xmlChar *cname, *ctype, *xstr;
                        gboolean pkey = FALSE;
                        gboolean nullok = FALSE; 
 
                        if (strcmp ((gchar *) cnode->name, "column"))
                                continue;
                        cname = xmlGetProp (cnode, BAD_CAST "name");
                        if (!cname)
                                g_error ("Missing column name (table=%s)", dbobj->obj_name);
                        xstr = xmlGetProp (cnode, BAD_CAST "pkey");
                        if (xstr) {
                                if ((*xstr == 't') || (*xstr == 'T'))
                                        pkey = TRUE;
                                xmlFree (xstr);
                        }
                        xstr = xmlGetProp (cnode, BAD_CAST "nullok");
                        if (xstr) {
                                if ((*xstr == 't') || (*xstr == 'T'))
                                        nullok = TRUE;
                                xmlFree (xstr);
                        }
                        ctype = xmlGetProp (cnode, BAD_CAST "type");
                        
                        /* a field */
                        GdaSqlField *field;
                        field = gda_sql_field_new (GDA_SQL_ANY_PART (ist));
                        field->field_name = g_strdup ((gchar *) cname);
                        ist->fields_list = g_slist_append (ist->fields_list, field);

			field = gda_sql_field_new (GDA_SQL_ANY_PART (ust));
                        field->field_name = g_strdup ((gchar *) cname);
                        ust->fields_list = g_slist_append (ust->fields_list, field);

			/* parameter */
                        GType ptype;
                        GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
                        GdaSqlExpr *expr;
                        ptype = ctype ? gda_g_type_from_string ((gchar *) ctype) : G_TYPE_STRING;
                        pspec->name = g_strdup_printf ("+%d", colindex);
                        pspec->g_type = ptype;
                        pspec->type = g_strdup (ctype ? (gchar *) ctype : "string");
                        pspec->nullok = nullok;
                        expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ist));
                        expr->param_spec = pspec;
                        insert_values_list = g_slist_append (insert_values_list, expr);

                        pspec = g_new0 (GdaSqlParamSpec, 1);
                        pspec->name = g_strdup_printf ("+%d", colindex);
                        pspec->g_type = ptype;
                        pspec->type = g_strdup (ctype ? (gchar *) ctype : "string");
                        pspec->nullok = nullok;
                        expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ust));
                        expr->param_spec = pspec;
                        ust->expr_list = g_slist_append (ust->expr_list, expr);

                        /* condition */
			if (pkey) {
                                /* Key columns */
                                TABLE_INFO (dbobj)->pk_cols_nb ++;
                                if (TABLE_INFO (dbobj)->pk_cols_nb == 1)
                                        TABLE_INFO (dbobj)->pk_cols_array = g_new0 (gint, TABLE_INFO (dbobj)->pk_cols_nb);
                                else
                                        TABLE_INFO (dbobj)->pk_cols_array = g_renew (gint, TABLE_INFO (dbobj)->pk_cols_array,
										     TABLE_INFO (dbobj)->pk_cols_nb);
                                TABLE_INFO (dbobj)->pk_cols_array [TABLE_INFO (dbobj)->pk_cols_nb - 1] = colindex;

                                /* WHERE for UPDATE */
                                expr = make_expr_EQUAL (GDA_SQL_ANY_PART (ust), cname, ctype, ptype, nullok, colindex);
                                if (!ust->cond)
                                        ust->cond = expr;
                                else {
                                        g_assert (ust->cond->cond);
                                        if (ust->cond->cond->operator == GDA_SQL_OPERATOR_AND)
                                                ust->cond->cond->operands = g_slist_append (ust->cond->cond->operands,
                                                                                            expr);
                                        else {
                                                ust->cond = make_expr_AND (GDA_SQL_ANY_PART (ust), ust->cond);
                                                ust->cond->cond->operands = g_slist_append (ust->cond->cond->operands,
                                                                                            expr);
                                        }
                                }
				/* WHERE for DELETE */
                                expr = make_expr_EQUAL (GDA_SQL_ANY_PART (dst), cname, ctype, ptype, nullok, colindex);
                                if (!dst->cond)
                                        dst->cond = expr;
                                else {
                                        g_assert (dst->cond->cond);
                                        if (dst->cond->cond->operator == GDA_SQL_OPERATOR_AND)
                                                dst->cond->cond->operands = g_slist_append (dst->cond->cond->operands,
                                                                                            expr);
                                        else {
                                                dst->cond = make_expr_AND (GDA_SQL_ANY_PART (dst), dst->cond);
                                                dst->cond->cond->operands = g_slist_append (dst->cond->cond->operands,
                                                                                            expr);
                                        }
                                }
                        }
			/* columns type */
                        if (colindex == 0)
                                TABLE_INFO (dbobj)->type_cols_array = g_new0 (GType, colindex + 2);
                        else
                                TABLE_INFO (dbobj)->type_cols_array = g_renew (GType, TABLE_INFO (dbobj)->type_cols_array, 
									       colindex + 2);
                        TABLE_INFO (dbobj)->type_cols_array [colindex] = ptype;
                        TABLE_INFO (dbobj)->type_cols_array [colindex+1] = G_TYPE_NONE;

			/* TableColumn */
			TableColumn *tcol = g_new0 (TableColumn, 1);
			TABLE_INFO (dbobj)->columns = g_slist_append (TABLE_INFO (dbobj)->columns, tcol);
			tcol->column_name = g_strdup ((gchar *) cname);
			tcol->column_type = ctype ? g_strdup ((gchar *) ctype) : NULL;
			tcol->gtype = ptype;
			tcol->pkey = pkey;
			tcol->nullok = nullok;

			/* free mem */
			xmlFree (cname);
                        if (ctype)
                                xmlFree (ctype);
                        colindex++;
		}
		else if (!strcmp ((gchar *) cnode->name, "fkey")) {
			xmlNodePtr fnode;
			xmlChar *ref_table;

			ref_table = xmlGetProp (cnode, BAD_CAST "ref_table");
			if (!ref_table) {
				g_set_error (error, 0, 0, 
					     _("Missing foreign key's referenced table name (for table '%s')"), 
					     dbobj->obj_name);
				goto onerror;
			}
			
			/* referenced DbObject */
			DbObject *ref_obj;
			ref_obj = g_hash_table_lookup (klass->cpriv->db_objects_hash, ref_table);
			if (!ref_obj) {
				ref_obj = g_new0 (DbObject, 1);
				klass->cpriv->db_objects = g_slist_prepend (klass->cpriv->db_objects, ref_obj);
				ref_obj->obj_name = g_strdup ((gchar *) ref_table);
				g_hash_table_insert (klass->cpriv->db_objects_hash, ref_obj->obj_name, ref_obj);
			}
			xmlFree (ref_table);
			dbobj->depend_list = g_slist_append (dbobj->depend_list, ref_obj);
			
			/* TableFKey structure */
			TableFKey *tfk = g_new0 (TableFKey, 1);
			
			tfk->table_info = dbobj;
			tfk->depend_on = ref_obj;

			for (fnode = cnode->children; fnode; fnode = fnode->next) {
				if (!strcmp ((gchar *) fnode->name, "part"))
					tfk->cols_nb ++;
			}

			tfk->fk_cols_array = g_new0 (gint, tfk->cols_nb);
			tfk->fk_names_array = g_new0 (gchar *, tfk->cols_nb);
			tfk->ref_pk_cols_array = g_new0 (gint, tfk->cols_nb);
			tfk->ref_pk_names_array = g_new0 (gchar*, tfk->cols_nb);

			gint fkcolindex = 0;
			for (fnode = cnode->children; fnode; fnode = fnode->next) {
				xmlChar *col, *ref_col;
				if (strcmp ((gchar *) fnode->name, "part"))
					continue;
				col = xmlGetProp (fnode, BAD_CAST "column");
				if (!col) {
					g_set_error (error, 0, 0, 
						     _("Missing foreign key's column name (for table '%s')"), 
						     dbobj->obj_name);
					table_fkey_free (tfk);
					goto onerror;
				}
				ref_col = xmlGetProp (fnode, BAD_CAST "ref_column");
				tfk->fk_cols_array [fkcolindex] = column_name_to_index (TABLE_INFO (dbobj),
											(gchar *) col);
				tfk->fk_names_array [fkcolindex] = g_strdup ((gchar *) col);
				if (tfk->fk_cols_array [fkcolindex] < 0) {
					g_set_error (error, 0, 0,
						     _("Column '%s' not found in table '%s'"), (gchar *) col,
						     dbobj->obj_name);
					table_fkey_free (tfk);
					goto onerror;
				}
				tfk->ref_pk_cols_array [fkcolindex] = -1;
				tfk->ref_pk_names_array [fkcolindex] = ref_col ?
					g_strdup ((gchar*) ref_col) : g_strdup ((gchar*) col);

				xmlFree (col);
				if (ref_col)
					xmlFree (ref_col);
				fkcolindex ++;
			}

			TABLE_INFO (dbobj)->fk_list = g_slist_append (TABLE_INFO (dbobj)->fk_list, tfk);
		}
	}

	/* finish the statements */
	GdaSqlStatement *st;
	ist->values_list = g_slist_append (NULL, insert_values_list);
	st = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
	st->contents = ist;
	TABLE_INFO (dbobj)->insert = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
	gda_sql_statement_free (st);
	
	st = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
	st->contents = ust;
	TABLE_INFO (dbobj)->update = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
	gda_sql_statement_free (st);
	
	st = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE);
	st->contents = dst;
	TABLE_INFO (dbobj)->delete = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
	gda_sql_statement_free (st);
	
	if (TABLE_INFO (dbobj)->pk_cols_nb == 0)
		g_error ("Missing key fields identification (table=%s)", dbobj->obj_name);

#ifdef GDA_DEBUG_NO
	/* debug */
	gchar *str;
	str = gda_statement_to_sql (TABLE_INFO (dbobj)->insert, NULL, NULL);
	g_print ("INSERT: %s\n", str);
	g_free (str);
	str = gda_statement_to_sql (TABLE_INFO (dbobj)->update, NULL, NULL);
	g_print ("UPDATE: %s\n", str);
	g_free (str);
	str = gda_statement_to_sql (TABLE_INFO (dbobj)->delete, NULL, NULL);
	g_print ("DELETE: %s\n", str);
	g_free (str);
#endif

	/* determine parameters */
	GdaSet *params;
	if (!gda_statement_get_parameters (TABLE_INFO (dbobj)->insert, &(TABLE_INFO (dbobj)->params), NULL))
		g_error ("Internal fatal error: could not get INSERT statement's parameters (table=%s)",
			 dbobj->obj_name);
	if (!gda_statement_get_parameters (TABLE_INFO (dbobj)->update, &params, NULL))
		g_error ("Internal fatal error: could not get UPDATE statement's parameters (table=%s)",
			 dbobj->obj_name);
	gda_set_merge_with_set (TABLE_INFO (dbobj)->params, params);
	g_object_unref (params);
	
	if (!gda_statement_get_parameters (TABLE_INFO (dbobj)->delete, &params, NULL))
		g_error ("Internal fatal error: could not get DELETE statement's parameters (table=%s)",
			 dbobj->obj_name);
	gda_set_merge_with_set (TABLE_INFO (dbobj)->params, params);
	g_object_unref (params);
	
	/* insert DbObject */
	g_hash_table_insert (klass->cpriv->db_objects_hash, dbobj->obj_name, dbobj);
	
	return dbobj;

 onerror:
	db_object_free (dbobj);
	return NULL;
}


static GdaSqlExpr *
make_expr_AND (GdaSqlAnyPart *parent, GdaSqlExpr *current) 
{
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (parent);
	expr->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
	expr->cond->operator = GDA_SQL_OPERATOR_AND;

	expr->cond->operands = g_slist_append (NULL, current);
	GDA_SQL_ANY_PART (current)->parent = GDA_SQL_ANY_PART (expr->cond);

	return expr;
}

static GdaSqlExpr *
make_expr_EQUAL (GdaSqlAnyPart *parent, xmlChar *cname, xmlChar *type, GType ptype, gboolean nullok, gint index) 
{
	GdaSqlOperation *op;
	GdaSqlExpr *retexpr, *expr;
	GdaSqlParamSpec *pspec;

	retexpr = gda_sql_expr_new (parent);

	op = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
	op->operator = GDA_SQL_OPERATOR_EQ;
	retexpr->cond = op;
	
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	g_value_set_string ((expr->value = gda_value_new (G_TYPE_STRING)), (gchar *) cname);
	op->operands = g_slist_append (op->operands, expr);
	
	pspec = g_new0 (GdaSqlParamSpec, 1);
	pspec->name = g_strdup_printf ("-%d", index);
	pspec->g_type = ptype;
	pspec->type = g_strdup (type ? (gchar *) type : "string");
	pspec->nullok = nullok;
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	expr->param_spec = pspec;
	op->operands = g_slist_append (op->operands, expr);

	return retexpr;
}


static void
compute_view_dependencies (GdaMetaStoreClass *klass, DbObject *view_dbobj, GdaSqlStatement *sqlst) {	
	if (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) {
		GdaSqlStatementSelect *selst;
		selst = (GdaSqlStatementSelect*) (sqlst->contents);
		GSList *targets;
		for (targets = selst->from->targets; targets; targets = targets->next) {
			GdaSqlSelectTarget *t = (GdaSqlSelectTarget *) targets->data;
			
			if (!t->table_name)
				continue;
			DbObject *ref_obj;
			ref_obj = g_hash_table_lookup (klass->cpriv->db_objects_hash, t->table_name);
			if (!ref_obj) {
				ref_obj = g_new0 (DbObject, 1);
				klass->cpriv->db_objects = g_slist_prepend (klass->cpriv->db_objects, ref_obj);
				ref_obj->obj_name = g_strdup (t->table_name);
				g_hash_table_insert (klass->cpriv->db_objects_hash, ref_obj->obj_name, ref_obj);
			}
			view_dbobj->depend_list = g_slist_append (view_dbobj->depend_list, ref_obj);
		}
	}
	else if (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND) {
		GdaSqlStatementCompound *cst;
		GSList *list;
		cst = (GdaSqlStatementCompound*) (sqlst->contents);
		for (list = cst->stmt_list; list; list = list->next)
			compute_view_dependencies (klass, view_dbobj, (GdaSqlStatement*) list->data);
	}
	else
		g_assert_not_reached ();
}

/*
 * Makes a list of all the DbObject structures listed in @objects
 * which are not present in @ordered_list and for which no dependency is in @ordered_list
 */
static GSList *
build_pass (GSList *objects, GSList *ordered_list)
{
	GSList *retlist = NULL, *list;

	for (list = objects; list; list = list->next) {
		gboolean has_dep = FALSE;
		GSList *dep_list;
		if (g_slist_find (ordered_list, list->data))
			continue;
		for (dep_list = DB_OBJECT (list->data)->depend_list; dep_list; dep_list = dep_list->next) {
			if (!g_slist_find (ordered_list, dep_list->data)) {
				has_dep = TRUE;
				break;
			}
		}
		if (has_dep)
			continue;
		retlist = g_slist_prepend (retlist, list->data);
	}

#ifdef GDA_DEBUG_NO
	g_print (">> PASS\n");
	for (list = retlist; list; list = list->next) 
		g_print ("--> %s\n", DB_OBJECT (list->data)->obj_name);
	g_print ("<<\n");
#endif

	return retlist;
}

/*
 * Returns: a new list containing all the items in @objects but correctly ordered
 * in a way that for any given DbObject in the list, all the dependencies are _before_ it in the list
 */
static GSList *
reorder_db_objects (GSList *objects, GHashTable *hash)
{
	GSList *pass_list;
	GSList *ordered_list = NULL;

	for (pass_list = build_pass (objects, ordered_list); pass_list; pass_list = build_pass (objects, ordered_list)) 
		ordered_list = g_slist_concat (ordered_list, pass_list);

#ifdef GDA_DEBUG_NO
	GSList *list;
	for (list = ordered_list; list; list = list->next) 
		g_print ("--> %s\n", ((DbObject*) list->data)->obj_name);
#endif	

	return ordered_list;
}

/*
 * Computes TableInfo->reverse_fk_list, TableFKey->ref_pk_cols_array and TableFKey->fk_fields_cond
 *
 * Returns: TRUE if all information is Ok
 */
static gboolean
complement_db_objects (GSList *objects, GHashTable *hash, GError **error)
{
	GSList *list;
	for (list = objects; list; list = list->next) {
		if (DB_OBJECT (list->data)->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE)
			continue;

		TableInfo *info = TABLE_INFO (DB_OBJECT (list->data));
		GSList *klist;
		for (klist = info->fk_list; klist; klist = klist->next) {
			TableFKey *tfk = (TableFKey *) klist->data;
			gint i;
			GString *cond = NULL;

			/* fix TableFKey->ref_pk_cols_array */
			for (i = 0; i < tfk->cols_nb; i++) {
				gint col;
				col = column_name_to_index (TABLE_INFO (tfk->depend_on), tfk->ref_pk_names_array[i]);
				if (col < 0) {
					g_set_error (error, 0, 0,
						     _("Foreign key column '%s' not found in table '%s'"),
						     tfk->ref_pk_names_array[i], tfk->depend_on->obj_name);
					if (cond)
						g_string_free (cond, TRUE);
					return FALSE;
				}
				tfk->ref_pk_cols_array[i] = col;

				TableColumn *tcol;
				if (cond)
					g_string_append (cond, " AND ");
				else
					cond = g_string_new ("");
				tcol = TABLE_COLUMN (g_slist_nth_data (info->columns, tfk->fk_cols_array[i]));
				g_assert (tcol);
				g_string_append_printf (cond, "%s = ##%s::%s%s", tcol->column_name, 
							tfk->fk_names_array[i],
							tcol->column_type ? tcol->column_type : g_type_name (tcol->gtype),
							tcol->nullok ? "::NULL" : "");
			}

			/* fix TableFKey->fk_fields_cond */
			g_assert (cond);
			tfk->fk_fields_cond = cond->str;
			g_string_free (cond, FALSE);

			/* fix TableInfo->reverse_fk_list, 
			 * but we don't want the "_tables"-->"_views" reverse dependency */
			if (strcmp (tfk->depend_on->obj_name, "_tables") ||
			    strcmp (tfk->table_info->obj_name, "_views"))
				TABLE_INFO (tfk->depend_on)->reverse_fk_list = 
					g_slist_append (TABLE_INFO (tfk->depend_on)->reverse_fk_list, tfk);
		}
	}

	return TRUE;
}

static void
db_object_free (DbObject *dbobj)
{
	g_free (dbobj->obj_name);
	if (dbobj->create_op)
		g_object_unref (dbobj->create_op);
	if (dbobj->depend_list)
		g_slist_free (dbobj->depend_list);
	switch (dbobj->obj_type) {
	case GDA_SERVER_OPERATION_CREATE_TABLE:
		table_info_free_contents (TABLE_INFO (dbobj));
		break;
	case GDA_SERVER_OPERATION_CREATE_VIEW:
		view_info_free_contents (VIEW_INFO (dbobj));
		break;
	default:
		TO_IMPLEMENT;
		break;
	}
	g_free (dbobj);
}

static void
table_info_free_contents (TableInfo *info)
{
	g_slist_foreach (info->columns, (GFunc) table_column_free, NULL);
	g_slist_free (info->columns);
	if (info->current_all)
		g_object_unref (info->current_all);
	if (info->delete_all)
		g_object_unref (info->delete_all);
	if (info->insert)
		g_object_unref (info->insert);
	if (info->update)
		g_object_unref (info->update);
	if (info->delete)
		g_object_unref (info->delete);
	if (info->params)
		g_object_unref (info->params);
	g_free (info->pk_cols_array);
	g_free (info->type_cols_array);
	g_slist_free (info->fk_list);
}

static void
view_info_free_contents (ViewInfo *info)
{
	g_free (info->view_def);
}

static void
table_column_free (TableColumn *tcol)
{
	g_free (tcol->column_name);
	g_free (tcol->column_type);
	g_free (tcol);
}

static void
table_fkey_free (TableFKey *tfk)
{
	gint i;
	g_free (tfk->fk_cols_array);
	g_free (tfk->ref_pk_cols_array);
	for (i = 0; i < tfk->cols_nb; i++) {
		g_free (tfk->ref_pk_names_array [i]);
		g_free (tfk->fk_names_array [i]);
	}
	g_free (tfk->ref_pk_names_array);
	g_free (tfk->fk_names_array);
	g_free (tfk->fk_fields_cond);
	g_free (tfk);
}


static gboolean
handle_schema_version (GdaMetaStore *store, gboolean *schema_present, GError **error) {
	GdaDataModel *model;
	GdaMetaStoreClass *klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	
	model = gda_connection_statement_execute_select_fullv (store->priv->cnc,
							       klass->cpriv->prep_stmts[STMT_GET_VERSION],
							       NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, 
							       0, G_TYPE_STRING, -1);
	if (schema_present)
		*schema_present = model ? TRUE : FALSE;
	if (model) {
		const GValue *version;
		if (gda_data_model_get_n_rows (model) != 1) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA,
				_ ("Could not get the internal schema's version"));
			g_object_unref (model);
			return FALSE;
		}
		
		version = gda_data_model_get_value_at (model, 0, 0);
		if (gda_value_is_null (version) || !gda_value_isa (version, G_TYPE_STRING)) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA,
				_ ("Could not get the internal schema's version"));
			g_object_unref (model);
			return FALSE;
		}
		store->priv->version = atoi (g_value_get_string (version));
		
		if (store->priv->version != 1) {
			TO_IMPLEMENT; /* migrate to current version */
			/* As there is now only version 1 => it's an error */
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA,
				_ ("Unknown internal schema's version: %d"), g_value_get_int (version));
			g_object_unref (model);
			return FALSE;
		}
		g_object_unref (model);
		return TRUE;
	}
	else {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA,
			_ ("Could not get the internal schema's version"));
		return FALSE;
	}
}

/**
 * gda_meta_store_get_version
 * @store: a #GdaMetaStore object
 *
 * Get @store's internal schema's version
 *
 * Retunrs: the version (1 at the moment)
 */
gint
gda_meta_store_get_version (GdaMetaStore *store) {
	g_return_val_if_fail (GDA_IS_META_STORE (store), 0);
	g_return_val_if_fail (store->priv, 0);
	
	return store->priv->version;
}

/**
 * gda_meta_store_get_internal_connection
 * @store: a #GdaMetaStore object
 *
 * Get a pointer to the #GdaConnection object internally used by @store to store
 * its contents.
 *
 * The returned connection can be used to access some other data than the one managed by @store
 * itself. Don not close the connection.
 *
 * Returns: a #GdaConnection, or %NULL
 */
GdaConnection *
gda_meta_store_get_internal_connection (GdaMetaStore *store) {
	g_return_val_if_fail (GDA_IS_META_STORE (store), 0);
	g_return_val_if_fail (store->priv, 0);
	
	return store->priv->cnc;
}

/**
 * gda_meta_store_extract
 * @store: a #GdaMetaStore object
 * @select_sql: a SELECT statement
 * @error: a place to store errors, or %NULL
 * @...: a list of (variable name (gchar *), GValue *value) terminated with NULL, representing values for all the
 * variables mentionned in @select_sql. If there is no variable then this part can be omitted.
 *
 * Extracts some data stored in @store using a custom SELECT query.
 *
 * Returns: a new #GdaDataModel, or %NULL if an error occurred
 */
GdaDataModel *
gda_meta_store_extract (GdaMetaStore *store, const gchar *select_sql, GError **error, ...)
{
	GdaMetaStoreClass *klass;
	GdaStatement *stmt;
	const gchar *remain;
	GdaDataModel *model;
	GdaSet *params = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (store->priv, NULL);

	/* statement creation */
	klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	stmt = gda_sql_parser_parse_string (klass->cpriv->parser, select_sql, &remain, error);
	if (!stmt)
		return NULL;
	if (remain) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
			     _("More than one SQL statement"));
		g_object_unref (stmt);
		return NULL;
	}
	
	/* parameters */
	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return NULL;
	}
	if (params) {
		va_list ap;
		gchar *pname;
		GSList *list, *params_set = NULL;
		va_start (ap, error);
		for (pname = va_arg (ap, gchar *); pname; pname = va_arg (ap, gchar *)) {
			GValue *value;
			GdaHolder *h;
			value = va_arg (ap, GValue *);
			h = gda_set_get_holder (params, pname);
			if (!h)
				g_warning (_("Parameter '%s' is not present in statement"), pname);
			else {
				if (!gda_holder_set_value (h, value)) {
					g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
						     _("Could not set value of parameter '%s'"), pname);
					g_object_unref (stmt);
					g_object_unref (params);
					va_end (ap);
					g_slist_free (params_set);
					return NULL;
				}
				params_set = g_slist_prepend (params_set, h);
			}
		}
		va_end (ap);

		for (list = params->holders; list; list = list->next) {
			if (!g_slist_find (params_set, list->data))
				g_warning (_("No value set for parameter '%s'"), 
					   gda_holder_get_id (GDA_HOLDER (list->data)));
		}
		g_slist_free (params_set);
	}

	/* execution */
	model = gda_connection_statement_execute_select (store->priv->cnc, stmt, params, error);
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	return model;
}

static gboolean prepare_tables_infos (GdaMetaStore *store, TableInfo **out_table_infos, 
				      TableConditionInfo **out_cond_infos, gboolean *out_with_key,
				      const gchar *table_name, const gchar *condition, GError **error, 
				      gint nvalues, const gchar **value_names, const GValue **values);
static gint find_row_in_model (GdaDataModel *find_in, GdaDataModel *data, gint row,
			       gint *pk_cols, gint pk_cols_nb, GError **error);
static gboolean gda_meta_store_modify_v (GdaMetaStore *store, const gchar *table_name, 
					 GdaDataModel *new_data, const gchar *condition, GError **error, 
					 gint nvalues, const gchar **value_names, const GValue **values);

/**
 * gda_meta_store_modify
 * @store: a #GdaMetaStore object
 * @table_name: the name of the table to modify within @store
 * @new_data: a #GdaDataModel containing the new data to set in @table_name, or %NULL (treated as a data model
 * with no row at all)
 * @condition: SQL expression (which may contain variables) defining the rows which are being obsoleted by @new_data, or %NULL
 * @error: a place to store errors, or %NULL
 * @...: a list of (variable name (gchar *), GValue *value) terminated with NULL, representing values for all the
 * variables mentionned in @condition.
 *
 * Propagates an update to @store, the update's contents is represented by @new_data, this function is
 * primarily reserved to database providers.
 *
 * For example tell @store to update its list of tables, @new_data should contain the same columns as the "_tables"
 * table of @store, and contain one row per table in the store; there should not be any more argument after the @error
 * argument.
 *
 * Now, to update only one table, the @new_data data model should have one row for the table to update (or no row
 * at all if the table does not exist anymore), and have values for the promary key of the "_tables" table of
 * @store, namely "table_catalog", "table_schema" and "table_name".
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_store_modify (GdaMetaStore *store, const gchar *table_name, 
	               GdaDataModel *new_data, const gchar *condition, GError **error, ...) 
{
	va_list ap;
	gboolean retval;
	gint size = 5, n_values = 0;
	const gchar **value_names;
	const GValue **values;
	gchar *name;

	value_names = g_new (const gchar *, size);
	values = g_new (const GValue *, size);

	va_start (ap, error);
	for (n_values = 0, name = va_arg (ap, gchar*); name; n_values++, name = va_arg (ap, gchar*)) {
		GValue *v;
		v = va_arg (ap, GValue *);
		if (n_values >= size) {
			size += 5;
			value_names = g_renew (const gchar *, value_names, size);
			values = g_renew (const GValue *, values, size);
		}
		value_names [n_values] = name;
		values [n_values] = v;
	}
	va_end (ap);
	retval = gda_meta_store_modify_v (store, table_name, new_data, condition, error,
					  n_values, value_names, values);
	g_free (value_names);
	g_free (values);
	return retval;
}

/**
 * gda_meta_store_modify_with_context
 * @store: a #GdaMetaStore object
 * @context: a #GdaMetaContext context describing what to modify in @store
 * @new_data: a #GdaDataModel containing the new data to set in @table_name, or %NULL (treated as a data model
 * with no row at all)
 * @error: a place to store errors, or %NULL
 *
 * Propagates an update to @store, the update's contents is represented by @new_data, this function is
 * primarily reserved to database providers.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_store_modify_with_context (GdaMetaStore *store, GdaMetaContext *context, 
				    GdaDataModel *new_data, GError **error)
{
	GString *cond = NULL;
	gint i;
	gboolean retval;

	for (i = 0; i < context->size; i++) {
		if (i == 0)
			cond = g_string_new ("");
		else
			g_string_append (cond, " AND ");
		g_string_append_printf (cond, "%s = ##%s::%s", context->column_names [i], 
					context->column_names [i], 
					g_type_name (G_VALUE_TYPE (context->column_values [i])));
	}

	retval = gda_meta_store_modify_v (store, context->table_name, new_data, cond ? cond->str : NULL, error,
					  context->size, 
					  (const gchar **) context->column_names, (const GValue **)context->column_values);
	if (cond)
		g_string_free (cond, TRUE);
	return retval;
}


static gboolean
gda_meta_store_modify_v (GdaMetaStore *store, const gchar *table_name, 
			 GdaDataModel *new_data, const gchar *condition, GError **error, 
			 gint nvalues, const gchar **value_names, const GValue **values)
{
	TableInfo *schema_set;
	TableConditionInfo *custom_set;
	gboolean prep, with_cond;
	gboolean retval = TRUE;
	GSList *all_changes = NULL;
	gboolean started_transaction = FALSE;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (store->priv, FALSE);
	g_return_val_if_fail (store->priv->cnc, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (store->priv->cnc), FALSE);
	g_return_val_if_fail (!new_data || GDA_IS_DATA_MODEL (new_data), FALSE);
	
	/* get the correct TableInfo */
	prep = prepare_tables_infos (store, &schema_set, &custom_set, &with_cond, table_name, 
				     condition, error, nvalues, value_names, values);
	if (!prep)
		return FALSE;
	
	GdaDataModel *current = NULL;
	gboolean *rows_to_del = NULL;
	gint current_n_rows = 0, i;
	gint current_n_cols = 0;

	/* get current data */
	current = gda_connection_statement_execute_select_full (store->priv->cnc,
								with_cond ? custom_set->select : schema_set->current_all,
								with_cond ? custom_set->params : NULL, 
								GDA_STATEMENT_MODEL_RANDOM_ACCESS,	
								schema_set->type_cols_array, error);
	if (!current)
		return FALSE;
	current_n_rows = gda_data_model_get_n_rows (current);
	current_n_cols = gda_data_model_get_n_columns (current);
	rows_to_del = g_new (gboolean, current_n_rows);
	memset (rows_to_del, TRUE, sizeof (gboolean) * current_n_rows);
#ifdef GDA_DEBUG_NO
	g_print ("CURRENT:\n");
	gda_data_model_dump (current, stdout);
#endif
	
	/* start a transaction if possible */
	if (! gda_connection_get_transaction_status (store->priv->cnc)) {
		started_transaction = gda_connection_begin_transaction (store->priv->cnc, NULL,
									GDA_TRANSACTION_ISOLATION_UNKNOWN,
									NULL);
		g_print ("------- BEGIN\n");
	}

	/* treat rows to insert / update */
	if (new_data) {
		gint new_n_rows, new_n_cols;
		new_n_rows = gda_data_model_get_n_rows (new_data);
		new_n_cols = gda_data_model_get_n_columns (new_data);
#ifdef GDA_DEBUG_NO
		g_print ("NEW:\n");
		gda_data_model_dump (new_data, stdout);
#endif		

		for (i = 0; i < new_n_rows; i++) {
			/* find existing row if necessary */
			gint erow = -1;
			if (current)
				erow = find_row_in_model (current, new_data, i,
					schema_set->pk_cols_array, schema_set->pk_cols_nb, error);
			if (erow == -3)
				/* nothing to do */
				continue;
			if (erow < -1) {
				retval = FALSE;
				goto out;
			}
			
			/* prepare change information */
			GdaMetaStoreChange *change = NULL;
			change = g_new0 (GdaMetaStoreChange, 1);
			change->c_type = -1;
			change->table_name = g_strdup (table_name);
			change->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
							      g_free, (GDestroyNotify) gda_value_free);
			all_changes = g_slist_append (all_changes, change);
			
			/* bind parameters for new values */
			gint j;
			for (j = 0; j < new_n_cols; j++) {
				gchar *pid = g_strdup_printf ("+%d", j);
				GdaHolder *h;
				h = gda_set_get_holder (schema_set->params, pid);
				if (h) {
					const GValue *value;
					value = gda_data_model_get_value_at (new_data, j, i);
					gda_holder_set_value (h, value);
					if (change) {
						g_hash_table_insert (change->keys, pid, gda_value_copy (value));
						pid = NULL;
					}
				}
				g_free (pid);
			}
			
			/* execute INSERT or UPDATE statements */
			if (erow == -1) {
				/* INSERT: bind INSERT parameters */
#ifdef GDA_DEBUG_NO
				g_print ("Insert for new row %d...\n", i);
#endif
				if (gda_connection_statement_execute_non_select (store->priv->cnc,
										 schema_set->insert, schema_set->params, 
										 NULL, error) == -1) {
					retval = FALSE;
					goto out;
				}
				if (change) 
					change->c_type = GDA_META_STORE_ADD;
			}
			else {
				/* also bind parameters with existing values */
				for (j = 0; j < current_n_cols; j++) {
					gchar *pid = g_strdup_printf ("-%d", j);
					GdaHolder *h;
					h = gda_set_get_holder (schema_set->params, pid);
					if (h) {
						const GValue *value;
						value = gda_data_model_get_value_at (current, j, erow);
						gda_holder_set_value (h, value);
						if (change) {
							g_hash_table_insert (change->keys, pid, gda_value_copy (value));
							pid = NULL;
						}
					}
					g_free (pid);
				}
				/* UPDATE */
#ifdef GDA_DEBUG_NO
				g_print ("Update for new row %d (old row was %d)...\n", i, erow);
#endif
				if (gda_connection_statement_execute_non_select (store->priv->cnc,
										 schema_set->update, schema_set->params, 
										 NULL, error) == -1) {
					retval = FALSE;
					goto out;
				}
				if (change) 
					change->c_type = GDA_META_STORE_MODIFY;
				rows_to_del [erow] = FALSE;
			}

			/* Dependencies update: reverse_fk_list */
			GSList *list;
			for (list = schema_set->reverse_fk_list; list; list = list->next) {
				TableFKey *tfk = (TableFKey*) list->data;
				gint k;
				GdaMetaContext context;
				
				context.table_name = tfk->table_info->obj_name;
				context.size = tfk->cols_nb;
				context.column_names = tfk->fk_names_array;
				context.column_values = g_new (GValue *, context.size);
				
				for (k = 0; k < tfk->cols_nb; k++) 
					context.column_values [k] = (GValue*) gda_data_model_get_value_at (new_data, 
													   tfk->ref_pk_cols_array[k], i);
#ifdef GDA_DEBUG
				g_print ("Suggest update data into table '%s':", tfk->table_info->obj_name);
				for (k = 0; k < tfk->cols_nb; k++) {
					gchar *str;
					str = gda_value_stringify (context.column_values [k]);
					g_print (" [%s => %s]", context.column_names[k], str);
					g_free (str);
				}
				g_print ("\n");
#endif
				g_signal_emit (store, gda_meta_store_signals[SUGGEST_UPDATE], 0, &context);
				g_free (context.column_values);
			}
		}
	}
	
	/* treat rows to delete */
	for (i = 0; i < current_n_rows; i++) {
		if (rows_to_del [i]) {
			/* prepare change information */
			GdaMetaStoreChange *change = NULL;
			change = g_new0 (GdaMetaStoreChange, 1);
			change->c_type = GDA_META_STORE_REMOVE;
			change->table_name = g_strdup (table_name);
			change->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
							      g_free, (GDestroyNotify) gda_value_free);
			all_changes = g_slist_append (all_changes, change);

			/* DELETE */
			gint j;
			for (j = 0; j < current_n_cols; j++) {
				gchar *pid = g_strdup_printf ("-%d", j);
				GdaHolder *h;
				h = gda_set_get_holder (schema_set->params, pid);
				if (h) {
					const GValue *value;
					value = gda_data_model_get_value_at (current, j, i);
					gda_holder_set_value (h, value);
					if (change) {
						g_hash_table_insert (change->keys, pid, gda_value_copy (value));
						pid = NULL;
					}
				}
				g_free (pid);
			}
#ifdef GDA_DEBUG_NO
			g_print ("Delete for existing row %d...\n", i);
#endif
			/* reverse_fk_list */
			GSList *list;
			for (list = schema_set->reverse_fk_list; list; list = list->next) {
				TableFKey *tfk = (TableFKey*) list->data;
				const GValue **values;
				gint k;

				/*g_print ("Also remove data from table '%s'...\n", tfk->table_info->obj_name);*/
				values = g_new (const GValue *, tfk->cols_nb);
				for (k = 0; k < tfk->cols_nb; k++) 
					values [k] = gda_data_model_get_value_at (current, tfk->ref_pk_cols_array[k], i);
				if (!gda_meta_store_modify_v (store, tfk->table_info->obj_name, NULL,
							      tfk->fk_fields_cond, error, 
							      tfk->cols_nb, (const gchar **) tfk->fk_names_array, values)) {
					retval = FALSE;
					g_free (values);
					goto out;
				}
				g_free (values);
			}
			
			if (gda_connection_statement_execute_non_select (store->priv->cnc,
									 schema_set->delete, schema_set->params, 
									 NULL, error) == -1) {
				retval = FALSE;
				goto out;
			}
		}
	}

	if (retval && started_transaction) {
		retval = gda_connection_commit_transaction (store->priv->cnc, NULL, NULL);
		g_print ("------- COMMIT\n");
	}
	if (retval && all_changes) 
		g_signal_emit (store, gda_meta_store_signals[META_CHANGED], 0, all_changes);
	
out:
	if (all_changes) {
		g_slist_foreach (all_changes, (GFunc) gda_meta_store_change_free, NULL);
		g_slist_free (all_changes);
	}
	g_free (rows_to_del);
	if (current)
		g_object_unref (current);
	if (!retval && started_transaction) {
		gda_connection_rollback_transaction (store->priv->cnc, NULL, NULL);
		g_print ("------- ROLLBACK\n");
	}
	return retval;
}

/*
 * Find the row in @find_in from the values of @data at line @row, and columns pointed by
 * the values of @pk_cols
 *
 * Returns: -3 if found and no change need to be made, 
 *          -2 on error, 
 *          -1 if not found, 
 *          >0 if found and changes need to be made
 */
static gint
find_row_in_model (GdaDataModel *find_in, GdaDataModel *data, gint row, gint *pk_cols, gint pk_cols_nb, GError **error) {
	gint i, erow;
	GSList *values = NULL;
	
	for (i = 0; i < pk_cols_nb; i++) 
		values = g_slist_append (values, (gpointer) gda_data_model_get_value_at (data, pk_cols[i], row));
	erow = gda_data_model_get_row_from_values (find_in, values, pk_cols);
	g_slist_free (values);
	
	if (erow >= 0) {
		gint ncols;
		ncols = gda_data_model_get_n_columns (find_in);
		if (ncols > gda_data_model_get_n_columns (data)) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
				_("Data models should have the same number of columns"));
			erow = -2;
		}
		else {
			gboolean changed = FALSE;
			for (i = 0; i < ncols; i++) {
				const GValue *v1, *v2;
				v1 = gda_data_model_get_value_at (find_in, i, erow);
				v2 = gda_data_model_get_value_at (data, i, row);
				if (((!v1 || gda_value_is_null (v1)) && v2) || 
				    (v1 && (!v2 || gda_value_is_null (v2)))) {
					changed = TRUE;
					break;
				}
				else if (v1 && v2) {
					if (G_VALUE_TYPE (v1) != G_VALUE_TYPE (v2)) {
						g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
							     _("Data types differ for column %d (expected type '%s' and got '%s')"),
							     i, g_type_name (G_VALUE_TYPE (v1)), g_type_name (G_VALUE_TYPE (v2)));
						erow = -2;
						break;
					}
					if (gda_value_compare (v1, v2)) {
						changed = TRUE;
						break;
					}
				}
			}
			if (!changed && (erow >= 0))
				erow = -3;
		}
	}
	
	return erow;
}

static TableConditionInfo *create_custom_set (GdaMetaStore *store, const gchar *table_name, const gchar *condition, GError **error);

static gboolean
prepare_tables_infos (GdaMetaStore *store, TableInfo **out_table_infos, TableConditionInfo **out_cond_infos, 
		      gboolean *out_with_key,
		      const gchar *table_name, const gchar *condition, GError **error, 
		      gint nvalues, const gchar **value_names, const GValue **values) 
{
	GdaMetaStoreClass *klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	
	g_assert (out_table_infos);
	g_assert (out_cond_infos);
	g_assert (out_with_key);
	*out_with_key = FALSE;
	
	/* compute *out_cond_infos */
	*out_cond_infos= NULL;
	if (condition) {
		gchar *key;
		key = g_strdup_printf ("%s.%s", table_name, condition);
		*out_with_key = TRUE;
		*out_cond_infos = g_hash_table_lookup (klass->cpriv->table_cond_info_hash, key);
		if (! *out_cond_infos) {
			*out_cond_infos = create_custom_set (store, table_name, condition, error);
			if (! *out_cond_infos)
				return FALSE;
			g_hash_table_insert (klass->cpriv->table_cond_info_hash, key, *out_cond_infos);
		}
		else
			g_free (key);
	}
	
	/* fetch or create *out_table_infos */
	DbObject *dbobj = g_hash_table_lookup (klass->cpriv->db_objects_hash, table_name);
	if (!dbobj) {
		/* FIXME: allow for external definition of TableInfo structures for
		 * other kind of data to be stored in meta store object */
		TO_IMPLEMENT;
		return FALSE;
	}
	*out_table_infos = TABLE_INFO (dbobj);

	/* bind parameters for *out_cond_infos */
	gint i;
	for (i = 0; i < nvalues; i++) {
		GdaHolder *h;
		
		h = gda_set_get_holder ((*out_cond_infos)->params, value_names [i]);
		if (!h || !gda_holder_set_value (h, values[i])) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INTERNAL_ERROR,
				_ ("Could not set value for parameter '%s'"), value_names [i]);
			return FALSE;
		}
	}
	
	return TRUE;
}

static TableConditionInfo *
create_custom_set (GdaMetaStore *store, const gchar *table_name, const gchar *condition, GError **error)
{
	GdaMetaStoreClass *klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	gchar *sql;
	TableConditionInfo *set;
	
	g_assert (condition);
	set = g_new0 (TableConditionInfo, 1);
	
	/* SELECT */
	sql = g_strdup_printf ("SELECT * FROM %s WHERE %s", table_name, condition);
	set->select = compute_prepared_stmt (klass->cpriv->parser, sql);
	g_free (sql);
	if (!set->select ||
		(gda_statement_get_statement_type (set->select) != GDA_SQL_STATEMENT_SELECT)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
			_("Could not create SELECT statement"));
		goto out;
	}
	
	/* parameters (are the same for both statements */
	if (! gda_statement_get_parameters (set->select, &(set->params), error))
		goto out;
	
	/* DELETE */
	sql = g_strdup_printf ("DELETE FROM %s WHERE %s", table_name, condition);
	set->delete = compute_prepared_stmt (klass->cpriv->parser, sql);
	g_free (sql);
	if (!set->delete||
		(gda_statement_get_statement_type (set->delete) != GDA_SQL_STATEMENT_DELETE)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
			_("Could not create DELETE statement"));
		goto out;
	}
	return set;
	
out:
	if (set->select)
		g_object_unref (set->select);
        if (set->delete)
		g_object_unref (set->delete);
        if (set->params)
		g_object_unref (set->params);
        g_free (set);
	return NULL;
}

/*
 * gda_meta_store_change_free
 * @change: a #GdaMetaStoreChange structure
 *
 * Free the memory associated to @change
 */
static void
gda_meta_store_change_free (GdaMetaStoreChange *change) {
	if (!change) return;
	
	g_free (change->table_name);
	g_hash_table_destroy (change->keys);
	g_free (change);
}

/**
 * gda_meta_store_create_modify_data_model
 * @store: a #GdaMetaStore object
 * @table_name: the name of a table present in @store
 * 
 * Creates a new #GdaDataModelArray data model which can be used, after being correctly filled,
 * with the gda_meta_store_modify*() methods.*
 *
 * To be used by provider's implementation
 *
 * Returns: a new #GdaDataModel
 */
GdaDataModel *
gda_meta_store_create_modify_data_model (GdaMetaStore *store, const gchar *table_name)
{
	GdaMetaStoreClass *klass;
	DbObject *dbobj;
	TableInfo *tinfo;
	GdaDataModel *model;
	GSList *list;
	gint i;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	dbobj = g_hash_table_lookup (klass->cpriv->db_objects_hash, table_name);
	if (!dbobj) {
		g_warning ("Table '%s' is not known by the GdaMetaStore", table_name);
		return NULL;
	}
	if (dbobj->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE) {
		g_warning ("Table '%s' is not a database table in the GdaMetaStore", table_name);
		return NULL;
	}

	tinfo = TABLE_INFO (dbobj);
	model = gda_data_model_array_new (g_slist_length (tinfo->columns));
	for (i = 0, list = tinfo->columns; list; list = list->next, i++) {
		GdaColumn *col;
		TableColumn *tcol = TABLE_COLUMN (list->data);

		col = gda_data_model_describe_column (model, i);
		gda_column_set_g_type (col, tcol->gtype);
		gda_column_set_name (col, tcol->column_name);
	}
	return model;
}

/**
 * gda_meta_store_get_depending_tables
 * @store: a #GdaMetaStore object
 * @table_name: the name of a table present in @store
 *
 * Get a list of the tables which depend on the table named @table_name.
 *
 * Returns: a new list of tables names (as gchar*), the list must be freed when no longer needed, 
 * but the strings present in the list must not be modified.
 */
GSList *
gda_meta_store_get_depending_tables (GdaMetaStore *store, const gchar *table_name)
{
	GSList *list, *ret;
	GdaMetaStoreClass *klass;
	DbObject *dbobj;
	TableInfo *tinfo;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (table_name && *table_name, NULL);

	klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	dbobj = g_hash_table_lookup (klass->cpriv->db_objects_hash, table_name);
	if (!dbobj) {
		g_warning ("Table '%s' is not known by the GdaMetaStore", table_name);
		return NULL;
	}
	if (dbobj->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE) {
		g_warning ("Table '%s' is not a database table in the GdaMetaStore", table_name);
		return NULL;
	}

	tinfo = TABLE_INFO (dbobj);
	for (ret = NULL, list = tinfo->reverse_fk_list; list; list = list->next) {
		TableFKey *tfk = (TableFKey*) list->data;
		ret = g_slist_prepend (ret, tfk->table_info->obj_name);
	}

	return g_slist_reverse (ret);
}

/**
 * gda_meta_store_get_schema_tables
 * @store: a #GdaMetaStore object
 *
 * Get an ordered list of the tables @store knows about. The tables are ordered in a way that tables dependencies
 * are respected: if table B has a foreign key on table A, then table A will be listed before table B in the returned
 * list.
 *
 * Returns: a new list of tables names (as gchar*), the list must be freed when no longer needed, 
 * but the strings present in the list must not be modified.
 */
GSList *
gda_meta_store_get_schema_tables (GdaMetaStore *store)
{
	GSList *list, *ret;
	GdaMetaStoreClass *klass;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);

	klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	for (ret = NULL, list = klass->cpriv->db_objects; list; list = list->next) {
		DbObject *dbobj = DB_OBJECT (list->data);
		if (dbobj->obj_type == GDA_SERVER_OPERATION_CREATE_TABLE)
			ret = g_slist_prepend (ret, dbobj->obj_name);
	}

	return g_slist_reverse (ret);
}
