/*
 * Copyright (C) 2008 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 - 2013, 2018-2019 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2013 Carl-Anton Ingmarsson <ca.ingmarsson@gmail.com>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#undef GDA_DISABLE_DEPRECATED

/*
 * Note about schema versions:
 * Each time the schema evolves:
 *  - the schema is backward compatible so it can be used by older versions of Libgda
 *  - it does not modify any table or view not starting with a '_'
 *  - the version number is increased by 1:
 *      version 1 for libgda between 4.0.0 and 4.1.3
 *      version 2 for libgda >= 4.1.4: added the "_table_indexes" and "_index_column_usage" tables
 *      version 3 for libgda >= 4.2.4: added the "__declared_fk" table
 *      version 4 for libgda >= 5.2.0: added "schema_default" to "_schemata" table
 *      version 5 for libgda >= 6.0: changed columns types to use TEXT types
 */
#define CURRENT_SCHEMA_VERSION "5"

#define G_LOG_DOMAIN "GDA-meta-store"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-meta-store-extra.h>
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
#include "gda-custom-marshal.h"
#include <stdarg.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-util.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-internal.h>
#include <libgda/gda-lockable.h>
#include <libgda/gda-connection-sqlite.h>
#include "gda-data-meta-wrapper.h"
#include <libgda/gda-debug-macros.h>

// This is a copy of the macro located at gda-value.h
#define l_g_value_unset(val) G_STMT_START{ if (G_IS_VALUE (val)) g_value_unset (val); }G_STMT_END


/* change general information */
struct _GdaMetaStoreChange {
	GdaMetaStoreChangeType  c_type;
	gchar                  *table_name;
	GHashTable             *keys; /* key = ('+' or '-') and a column position in @table (string) starting at 0, 
	                               * value = a GValue pointer */
};

G_DEFINE_BOXED_TYPE (GdaMetaStoreChange, gda_meta_store_change, gda_meta_store_change_copy, gda_meta_store_change_free)


/**
 * gda_meta_store_change_new:
 *
 * Creates a new #GdaMetaStoreChange
 */
GdaMetaStoreChange*
gda_meta_store_change_new (void)
{
  GdaMetaStoreChange* change = g_new0 (GdaMetaStoreChange, 1);
  change->table_name = NULL;
  change->c_type = -1;
  change->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
								      g_free, (GDestroyNotify) gda_value_free);
	return change;
}

void
gda_meta_store_change_set_change_type (GdaMetaStoreChange *change, GdaMetaStoreChangeType ctype)
{
  g_return_if_fail (change != NULL);
  change->c_type = ctype;
}

GdaMetaStoreChangeType
gda_meta_store_change_get_change_type (GdaMetaStoreChange *change)
{
  g_return_val_if_fail (change != NULL, GDA_META_STORE_MODIFY);
  return change->c_type;
}
/**
 * gda_meta_store_change_set_table_name:
 * @change: a #GdaMetaStoreChange
 * @table_name: name of the table
 *
 */
void
gda_meta_store_change_set_table_name (GdaMetaStoreChange *change, const gchar *table_name)
{
  g_return_if_fail (change != NULL);
  change->table_name = g_strdup (table_name);
}
/**
 * gda_meta_store_change_get_table_name:
 * @change: a #GdaMetaStoreChange
 *
 * Returns: (transfer full): a string with the table name
 */
gchar*
gda_meta_store_change_get_table_name (GdaMetaStoreChange *change)
{
  g_return_val_if_fail (change != NULL, NULL);
  return g_strdup (change->table_name);
}
/**
 * gda_meta_store_change_get_keys:
 * @change: a #GdaMetaStoreChange
 *
 * Returns: (element-type utf8 GValue) (transfer none): hash table with string key key = ('+' or '-') and a column position in @table (string) starting at 0 and value as #GValue pointer
 */
GHashTable*
gda_meta_store_change_get_keys (GdaMetaStoreChange *change)
{
  g_return_val_if_fail (change != NULL, NULL);
  return change->keys;
}

/**
 * gda_meta_store_change_copy:
 * @src: a #GdaMetaStoreChange
 *
 * Returns: a new #GdaMetaStoreChange
 */
GdaMetaStoreChange*
gda_meta_store_change_copy (GdaMetaStoreChange *src)
{
  GdaMetaStoreChange* copy;
  g_return_val_if_fail (src != NULL, NULL);
  copy = gda_meta_store_change_new ();
  gda_meta_store_change_set_change_type (copy, gda_meta_store_change_get_change_type (src));
  gda_meta_store_change_set_table_name (copy, gda_meta_store_change_get_table_name (src));
  GList *keys = g_hash_table_get_keys (src->keys);
  while (keys) {
    g_hash_table_insert (copy->keys, keys->data, gda_value_copy (g_hash_table_lookup (src->keys, keys->data)));
    keys = keys->next;
  }
  return copy;
}

/**
 * gda_meta_store_change_free:
 * @change: a #GdaMetaStoreChange to be freed
 *
 */
void
gda_meta_store_change_free (GdaMetaStoreChange *change)
{
  g_return_if_fail (change != NULL);
  if (change->table_name != NULL)
    g_free (change->table_name);
  g_hash_table_unref (change->keys);
}

/**
 * gda_value_set_meta_store_change:
 * @value: a #GValue to set value to
 * @change: a #GdaMetaStoreChange to be set
 *
 */
void
gda_value_set_meta_store_change (GValue *value, GdaMetaStoreChange *change)
{
  g_return_if_fail (value != NULL);
  g_return_if_fail (change != NULL);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_META_STORE_CHANGE);
	if (change)
		g_value_set_boxed (value, change);
	else {
		g_value_set_boxed (value, gda_meta_store_change_new ());
	}
}

/**
 * gda_value_get_meta_store_change:
 * @value: a #GValue to get value from
 * 
 * Returns: (transfer none): a #GdaMetaStoreChange
 *
 */
GdaMetaStoreChange*
gda_value_get_meta_store_change (GValue *value)
{
  GdaMetaStoreChange *val;

	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_META_STORE_CHANGE), NULL);

	val = (GdaMetaStoreChange*) g_value_get_boxed (value);

	return val;
}

/*
	IMPLEMENTATION NOTES:
	In this implementation, for 5.2, the new API for GdaMetaContext struct doesn't touch size, column_names and
	column_values members, but added a HashTable member in order to make more easy to access to column/value
	elements.
	
	Take care about to use gda_meta_context_free or g_free on this struct if you don't use the new API because 
	unexpected results.
	
	Changes include: table string is copied and columns are stored in a GHashTable witch it's reference counting
	is incremented. When is freed, table string is freed and hash ref is decremented.
*/


/**
 * gda_meta_context_new:
 * 
 * Creates a new #GdaMetaContext struct with a #GHashTable to store column/value pairs.
 *
 * Return: (transfer full): a new #GdaMetaContext struct with a new created hash to
 * store column name/value pairs.
 *
 * Since: 5.2
 */
GdaMetaContext*
gda_meta_context_new (void)
{
	GdaMetaContext *ctx = g_new0 (GdaMetaContext, 1);
	ctx->table_name = g_strdup ("");
	ctx->columns = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, 
							(GDestroyNotify) gda_value_free);
	return ctx;
}

/**
 * gda_meta_context_copy:
 * @ctx: a #GdaMetaContext
 *
 * Copy constructor.
 *
 * Returns: a new #GdaMetaContext
 *
 * Since: 5.2
 */
GdaMetaContext *
gda_meta_context_copy (GdaMetaContext *ctx)
{
	g_return_val_if_fail (ctx, NULL);

	GdaMetaContext *n;
	n = gda_meta_context_new ();
	gda_meta_context_set_table (n, gda_meta_context_get_table (ctx));
	gda_meta_context_set_columns (n, ctx->columns, NULL);

	return n;
}

/**
 * gda_meta_context_set_table:
 * @ctx: a #GdaMetaContext struct to set table to
 * @table: a string with the table's name to use in context
 * 
 * Set table's name to use in the context. The table is one of <link linkend="information_schema">database
 * schema</link> used to store meta information about the database. Use "_tables" to update meta information
 * about database's tables.
 *
 * Since: 5.2
 */
void
gda_meta_context_set_table (GdaMetaContext *ctx, const gchar *table_name)
{
	g_return_if_fail (ctx && table_name);
	ctx->table_name = g_strdup (table_name);
}

/**
 * gda_meta_context_get_table:
 * @ctx: a #GdaMetaContext struct to get table's name from
 * 
 * Get table's name to used in the context.
 *
 * Return: (transfer none): A string with the table's name used in the context.
 *
 * Since: 5.2
 */
const gchar*
gda_meta_context_get_table (GdaMetaContext *ctx)
{
	g_return_val_if_fail (ctx, NULL);
	return ctx->table_name;
}

/**
 * gda_meta_context_set_column:
 * @ctx: a #GdaMetaContext struct to add column/value pais to
 * @column: (transfer none): the column's name
 * @value: (transfer none): the column's value
 * @cnc: (nullable): a #GdaConnection to be used when identifier are normalized, or NULL
 * 
 * Sets a new column/value pair to the given context @ctx. Column, must be a column in the given table's
 * name setted by #gda_meta_context_set_table () (a table in the <link linkend="information_schema">database
 * schema</link>). If the given @column already exists it's value is overwrited.
 *
 * Column's name and value is copied and destroyed when #gda_meta_context_free is called.
 *
 * Since: 5.2
 */
void
gda_meta_context_set_column (GdaMetaContext *ctx, const gchar* column, const GValue* value, GdaConnection *cnc)
{
	g_return_if_fail (ctx && column && value);
	GValue *v;
	if (G_VALUE_HOLDS_STRING((GValue*)value)) {
		v = gda_value_new (G_TYPE_STRING);
		g_value_take_string (v, gda_sql_identifier_quote (g_value_get_string ((GValue*)value), cnc, NULL,
										TRUE, FALSE));
		g_hash_table_insert (ctx->columns, (gpointer) g_strdup (column), (gpointer) v);
	}
	else
		g_hash_table_insert (ctx->columns, (gpointer) g_strdup (column), (gpointer) gda_value_copy (value));
}

/**
 * gda_meta_context_set_columns:
 * @ctx: a #GdaMetaContext struct to set colums to
 * @columns: (element-type utf8 GObject.Value): a #GHashTable with the table's columns' name and their values
 * to use in context.
 * @cnc: (nullable): a #GdaConnection to used to normalize identifiers quoting, or NULL
 * 
 * Set columns to use in the context. The #GHashTable use column's name as key and a #GValue as value,
 * to represent its value.
 * 
 * @columns incements its reference counting. Is recommended to use #gda_meta_context_free in order to free them.
 *
 * Since: 5.2
 */
void
gda_meta_context_set_columns (GdaMetaContext *ctx, GHashTable *columns, GdaConnection *cnc)
{
	g_return_if_fail (ctx && columns && cnc);
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_hash_table_unref (ctx->columns);
	ctx->columns = g_hash_table_ref (columns);
	// FIXME: Old but necesary initialization
	// FIXME: Remove code that use column_names and column_values arrays
	if (ctx->column_names)
		g_free (ctx->column_names);
	if (ctx->column_values)
		g_free (ctx->column_values);
	ctx->column_names = g_new (gchar*, g_hash_table_size (ctx->columns));
	ctx->column_values = g_new (GValue*, g_hash_table_size (ctx->columns));
	
	GHashTableIter iter;
	gpointer key, value;
	gint i = 0; // FIXME: Code to remove
	g_hash_table_iter_init (&iter, ctx->columns);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		// Normalize identifier quote
		if (G_VALUE_HOLDS_STRING((GValue*)value)) {
			GValue *v = gda_value_new (G_TYPE_STRING);
			g_value_take_string (v, gda_sql_identifier_quote (g_value_get_string ((GValue*)value), cnc, NULL,
											TRUE, FALSE));
			g_hash_table_insert (ctx->columns, key, (gpointer) v);
		}

		// FIXME: Code to remove
		ctx->column_names[i] = (gchar*) key;
		ctx->column_values[i] = (GValue*) value;
		i++;
	}
}

/**
 * gda_meta_context_free:
 * @ctx: (nullable): a #GdaMetaContext struct to free
 * 
 * Frees any resources taken by @ctx struct. If @ctx is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gda_meta_context_free (GdaMetaContext *ctx)
{
	if (!ctx)
		return;

	g_free (ctx->table_name);
	g_hash_table_unref (ctx->columns);
	g_free (ctx);
	// REMIND: ctx->column_names and ctx->column_values must not be freed
}

/**
 * gda_meta_context_stringify:
 * @ctx: a #GdaMetaContext
 *
 * Creates a string representation of given context.
 *
 * Returns: (transfer full): a new string with the representation of the context
 */
gchar*
gda_meta_context_stringify (GdaMetaContext *ctx)
{
	g_return_val_if_fail (ctx != NULL, g_strdup (""));
	gint i;
	gchar *str;
	GString *string = g_string_new ("{");
	g_string_append (string, ctx->table_name);
	g_string_append (string, "}=>{{");

	for (i = 0; i < ctx->size; i++) {
		if (i > 0)
			g_string_append_c (string, ' ');
		str = gda_value_stringify (ctx->column_values[i]);
		g_string_append_printf (string, " [%s => %s]", ctx->column_names[i], str);
		g_free (str);
	}
	if (i == 0)
		g_string_append (string, "no constraints in context");
	g_string_append (string, "}}");
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

/**
 * gda_meta_context_get_n_columns:
 * @ctx: a #GdaMetaContext
 *
 * Returns: the number of columns in the context
 */
gint
gda_meta_context_get_n_columns (GdaMetaContext *ctx)
{
	g_return_val_if_fail (ctx != NULL, 0);
	return ctx->size;
}


G_DEFINE_BOXED_TYPE(GdaMetaContext, gda_meta_context, gda_meta_context_copy, gda_meta_context_free)

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

/* simple predefined statements */
enum {
	STMT_GET_VERSION,
	STMT_SET_VERSION,
	STMT_UPD_VERSION,
	STMT_DEL_ATT_VALUE,
	STMT_SET_ATT_VALUE,
	STMT_ADD_DECLARE_FK,
	STMT_DEL_DECLARE_FK,
	STMT_LAST
} PreStmtType;

/*
 * Complements the DbObject structure, for tables only
 * contains predefined statements for data selection and modifications
 */
typedef struct {
	GSList       *columns; /* list of TableColumn */

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

	/* columns which contain SQL identifiers */
	gint         *ident_cols;
	gint          ident_cols_size;

	GType          gtype_column; /* -1 if no column represents a GType */
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
	GdaMetaStore            *store; /* if not NULL, the store in which this db object is,
					 * or %NULL if it's a class db object */
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
	gboolean      autoinc;
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
static void provider_specific_key_free (ProviderSpecificKey *key);
static void provider_specific_value_free (ProviderSpecificValue *val);

typedef struct {
	GdaConnection *cnc;
	GdaSqlIdentifierStyle ident_style;
	GdaSqlReservedKeywordsFunc reserved_keyword_func;

	GError        *init_error;
	gint           version;
	gboolean       schema_ok;

	gchar         *catalog; /* name of the catalog in which all the objects are created, or NULL if none specified */
	gchar         *schema;  /* name of the schema in which all the objects are created, or NULL if none specified */

	GSList        *p_db_objects; /* list of DbObject structures */
	GHashTable    *p_db_objects_hash; /* key = table name, value = a DbObject structure */

	gboolean       override_mode;

	gint           max_extract_stmt; /* 0 => don't keep GdaStatement */
	gint           current_extract_stmt;
	GHashTable    *extract_stmt_hash; /* key = a SQL string, value = a #GdaStatement */

	GRecMutex      mutex;
  /* Internal */
  GdaSqlParser  *parser;;
	GdaStatement **prep_stmts; /* Simple prepared statements, of size STMT_LAST, general usage */

	/* Internal database's schema information */
	GSList        *db_objects; /* list of DbObject structures */
	GHashTable    *db_objects_hash; /* key = table name, value = a DbObject structure */
        GHashTable    *table_cond_info_hash; /* key = string composed of the column names of
					      * the parameters separated by a period,
					      * value = a TableConditionInfo structure */
	GHashTable    *provider_specifics; /* key = a ProviderSpecificKey , value = a ProviderSpecificValue */
  GdaSet        *attributes_set;
} GdaMetaStorePrivate;


G_DEFINE_TYPE_WITH_PRIVATE (GdaMetaStore, gda_meta_store, G_TYPE_OBJECT)

static void db_object_free    (DbObject *dbobj);
static void create_db_objects (GdaMetaStorePrivate *priv, GdaMetaStore *store);


/* signals */
enum {
	SUGGEST_UPDATE,
	META_CHANGED,
	META_RESET,
	LAST_SIGNAL
};

static gint gda_meta_store_signals[LAST_SIGNAL] = { 0, 0, 0 };

/* properties */
enum {
	PROP_0,
	PROP_CNC_STRING,
	PROP_CNC_OBJECT,
	PROP_CATALOG,
	PROP_SCHEMA
};

/* module error */
GQuark gda_meta_store_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_meta_store_error");
	return quark;
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
provider_specific_key_free (ProviderSpecificKey *key)
{
  g_free (key->expr);
  g_free (key);
}
static void
provider_specific_value_free (ProviderSpecificValue *val)
{
  g_free (val->repl);
  g_free (val);
}

static gboolean
suggest_update_accumulator (G_GNUC_UNUSED GSignalInvocationHint *ihint,
			    GValue *return_accu,
			    const GValue *handler_return,
			    G_GNUC_UNUSED gpointer data)
{
        GError *error;

        error = g_value_get_boxed (handler_return);
        g_value_set_boxed (return_accu, error);

        return error ? FALSE : TRUE; /* stop signal if 'thisvalue' is FALSE */
}

static GError *
m_suggest_update (G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED GdaMetaContext *suggest)
{
        return NULL; /* defaults allows update suggest */
}

static void
gda_meta_store_class_init (GdaMetaStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * GdaMetaStore::suggest-update:
	 * @store: the #GdaMetaStore instance that emitted the signal
	 * @suggest: (type Gda.MetaContext): the suggested update, as a #GdaMetaContext structure
	 *
	 * This signal is emitted when the contents of a table should be updated (data to update or insert only;
	 * deleting data is done automatically). This signal is used for internal purposes by the #GdaConnection
	 * object.
	 *
	 * Returns: a new #GError error structure if there was an error when processing the
	 * signal, or %NULL if signal propagation should continue
	 **/
	gda_meta_store_signals[SUGGEST_UPDATE] =
		g_signal_new ("suggest-update",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GdaMetaStoreClass, suggest_update),
		suggest_update_accumulator, NULL,
		_gda_marshal_ERROR__METACONTEXT, G_TYPE_ERROR,
		1, G_TYPE_POINTER);
	/**
	 * GdaMetaStore::meta-changed:
	 * @store: the #GdaMetaStore instance that emitted the signal
	 * @changes: (type GLib.SList) (element-type Gda.MetaStoreChange): a list of changes made, as a #GSList of pointers to #GdaMetaStoreChange (which must not be modified)
	 *
	 * This signal is emitted when the @store's contents have changed (the changes are in the @changes list)
	 */
	gda_meta_store_signals[META_CHANGED] =
		g_signal_new ("meta-changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GdaMetaStoreClass, meta_changed),
		NULL, NULL,
		_gda_marshal_VOID__SLIST, G_TYPE_NONE,
		1, G_TYPE_POINTER);
	/**
	 * GdaMetaStore::meta-reset:
	 * @store: the #GdaMetaStore instance that emitted the signal
	 *
	 * This signal is emitted when the @store's contents have been reset completely and when
	 * no detailed changes are available
	 */
	gda_meta_store_signals[META_RESET] =
		g_signal_new ("meta-reset",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GdaMetaStoreClass, meta_reset),
		NULL, NULL,
		_gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	klass->suggest_update = m_suggest_update;
	klass->meta_changed = NULL;
	klass->meta_reset = NULL;

	/* Properties */
	object_class->set_property = gda_meta_store_set_property;
	object_class->get_property = gda_meta_store_get_property;
	g_object_class_install_property (object_class, PROP_CNC_STRING,
		g_param_spec_string ("cnc-string", NULL, _ ("Connection string for the internal connection to use"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_CNC_OBJECT,
		g_param_spec_object ("cnc", NULL, _ ("Connection object internally used"), GDA_TYPE_CONNECTION,
		(G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_CATALOG,
		g_param_spec_string ("catalog", NULL, _ ("Catalog in which the database objects will be created"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_SCHEMA,
		g_param_spec_string ("schema", NULL, _ ("Schema in which the database objects will be created"), NULL,
		(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));

	object_class->constructor = gda_meta_store_constructor;
	object_class->dispose = gda_meta_store_dispose;
	object_class->finalize = gda_meta_store_finalize;
}


static void
gda_meta_store_init (GdaMetaStore *store)
{
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	priv->cnc = NULL;
	priv->ident_style = GDA_SQL_IDENTIFIERS_LOWER_CASE;
	priv->reserved_keyword_func = NULL;
	priv->schema_ok = FALSE;

	priv->init_error = NULL;
	priv->version = 0;

	priv->catalog = NULL;
	priv->schema = NULL;

	priv->p_db_objects = NULL;
	priv->p_db_objects_hash = g_hash_table_new (g_str_hash, g_str_equal);

	priv->override_mode = FALSE;

	priv->max_extract_stmt = 10;
	priv->current_extract_stmt = 0;
	priv->extract_stmt_hash = NULL;

  /* Class private data */
	priv->prep_stmts = g_new0 (GdaStatement *, STMT_LAST);
	priv->parser = gda_sql_parser_new ();
	priv->provider_specifics = g_hash_table_new_full (ProviderSpecific_hash,
                                                    ProviderSpecific_equal,
                                                    (GDestroyNotify) provider_specific_key_free,
                                                    (GDestroyNotify) provider_specific_value_free);

	/* priv->provider_specifics = g_hash_table_new (ProviderSpecific_hash, ProviderSpecific_equal); */
	priv->db_objects_hash = g_hash_table_new (g_str_hash, g_str_equal);
	create_db_objects (priv, NULL);
  priv->table_cond_info_hash = g_hash_table_new (g_str_hash, g_str_equal);

	priv->prep_stmts[STMT_SET_VERSION] =
		compute_prepared_stmt (priv->parser,
				       "INSERT INTO _attributes (att_name, att_value) VALUES ('_schema_version', ##version::string)");
	priv->prep_stmts[STMT_UPD_VERSION] =
		compute_prepared_stmt (priv->parser,
				       "UPDATE _attributes SET att_value = ##version::string WHERE att_name = '_schema_version'");
	priv->prep_stmts[STMT_GET_VERSION] =
		compute_prepared_stmt (priv->parser,
				       "SELECT att_value FROM _attributes WHERE att_name='_schema_version'");
	priv->prep_stmts[STMT_DEL_ATT_VALUE] =
		compute_prepared_stmt (priv->parser,
				       "DELETE FROM _attributes WHERE att_name = ##name::string");
	priv->prep_stmts[STMT_SET_ATT_VALUE] =
		compute_prepared_stmt (priv->parser,
				       "INSERT INTO _attributes VALUES (##name::string, ##value::string::null)");
	priv->prep_stmts[STMT_ADD_DECLARE_FK] =
		compute_prepared_stmt (priv->parser,
				       "INSERT INTO __declared_fk (constraint_name, table_catalog, table_schema, table_name, column_name, ref_table_catalog, ref_table_schema, ref_table_name, ref_column_name) VALUES (##fkname::string, ##tcal::string, ##tschema::string, ##tname::string, ##colname::string, ##ref_tcal::string, ##ref_tschema::string, ##ref_tname::string, ##ref_colname::string)");
	priv->prep_stmts[STMT_DEL_DECLARE_FK] =
		compute_prepared_stmt (priv->parser,
				       "DELETE FROM __declared_fk WHERE constraint_name = ##fkname::string AND table_catalog = ##tcal::string AND table_schema = ##tschema::string AND table_name = ##tname::string AND ref_table_catalog = ##ref_tcal::string AND ref_table_schema = ##ref_tschema::string AND ref_table_name = ##ref_tname::string");
  priv->attributes_set = NULL;
/*#define GDA_DEBUG_GRAPH*/
#ifdef GDA_DEBUG_GRAPH
#define INFORMATION_SCHEMA_GRAPH_FILE "information_schema.dot"
	GString *string;
	GSList *list;
	string = g_string_new ("digraph G {\nrankdir = RL;\nnode [shape = box];\n");
	for (list = priv->db_objects; list; list = list->next) {
		DbObject *dbo = (DbObject*) list->data;
		switch (dbo->obj_type) {
		case GDA_SERVER_OPERATION_CREATE_TABLE:
			g_string_append_printf (string, "%s;\n", dbo->obj_name);
			break;
		case GDA_SERVER_OPERATION_CREATE_VIEW:
			g_string_append_printf (string, "%s [ shape = ellipse ];\n", dbo->obj_name);
			break;
		default:
			g_string_append_printf (string, "%s [ shape = note ];\n", dbo->obj_name);
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

	g_rec_mutex_init (& (priv->mutex));
}

static GObject *
gda_meta_store_constructor (GType type,
			    guint n_construct_properties,
			    GObjectConstructParam *construct_properties)
{
	GObject *object;
	guint i;
	GdaMetaStore *store;
	gboolean been_specified = FALSE;

	static GRecMutex init_rmutex;
	g_rec_mutex_lock (&init_rmutex);
	object = G_OBJECT_CLASS (G_OBJECT_CLASS (gda_meta_store_parent_class))->constructor (type,
		n_construct_properties,
		construct_properties);
	for (i = 0; i< n_construct_properties; i++) {
		GObjectConstructParam *prop = &(construct_properties[i]);
		if (!strcmp (g_param_spec_get_name (prop->pspec), "cnc-string")) {
			if (g_value_get_string (prop->value))
				been_specified = TRUE;
		}
		else if (!strcmp (g_param_spec_get_name (prop->pspec), "system-filename")) {
			if (g_value_get_pointer (prop->value))
				been_specified = TRUE;
		}
	}
	store = (GdaMetaStore *) object;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (!priv->cnc && !been_specified) {
		/* in memory DB */
		g_object_set (object, "cnc-string", "SQLite://DB_DIR=.;DB_NAME=__gda_tmp", NULL);
	}

	if (priv->cnc) {
		gda_lockable_lock (GDA_LOCKABLE (priv->cnc));
		gda_connection_increase_usage (priv->cnc); /* USAGE ++ */
		priv->schema_ok = initialize_cnc_struct (store, &(priv->init_error));
		gda_connection_decrease_usage (priv->cnc); /* USAGE -- */
		gda_lockable_unlock (GDA_LOCKABLE (priv->cnc));
	}

	/* create a local copy of all the DbObject structures defined in klass->cpriv */
	if (priv->catalog && !priv->schema) {
		if (! priv->init_error)
			g_set_error (&(priv->init_error), GDA_META_STORE_ERROR,
				     GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
				     "%s",
				     _("Catalog specified but no schema specified, store will not be usable"));
		priv->schema_ok = FALSE;
	}
	else {
		GdaMetaStoreClass *klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
		GSList *list;
		if (!priv->schema) {
			/* uses all the priv->db_objects AS IS in priv->p_db_objects */
			priv->p_db_objects = g_slist_copy (priv->db_objects);
			for (list = priv->p_db_objects; list; list = list->next)
				g_hash_table_insert (priv->p_db_objects_hash, DB_OBJECT (list->data)->obj_name,
						     list->data);
		}
		else
			create_db_objects (priv, store);
	}
	g_rec_mutex_unlock (&init_rmutex);
	return object;
}

/**
 * gda_meta_store_new_with_file:
 * @file_name: a file name
 *
 * Create a new #GdaMetaStore object using @file_name as its internal
 * database
 *
 * Returns: (transfer full): the newly created object, or %NULL if an error occurred
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
 * gda_meta_store_new:
 * @cnc_string: (nullable): a connection string, or %NULL for an in-memory internal database
 *
 * Create a new #GdaMetaStore object.
 *
 * Returns: (transfer full): the newly created object, or %NULL if an error occurred
 */
GdaMetaStore *
gda_meta_store_new (const gchar *cnc_string)
{
	GObject *obj;
	GdaMetaStore *store;
#ifdef GDA_DEBUG_NO
	GTimer *timer;
	timer = g_timer_new ();
#endif
	static GRecMutex init_rmutex;
	g_rec_mutex_lock (&init_rmutex);
	if (cnc_string)
		obj = g_object_new (GDA_TYPE_META_STORE, "cnc-string", cnc_string, NULL);
	else
		obj = g_object_new (GDA_TYPE_META_STORE, "cnc-string", "SQLite://DB_NAME=__gda_tmp", NULL);
	g_rec_mutex_unlock (&init_rmutex);
#ifdef GDA_DEBUG_NO
	g_timer_stop (timer);
	g_print ("GdaMetaStore took %.03f sec. to create.\n", g_timer_elapsed (timer, NULL));
	g_timer_destroy (timer);
#endif
	store = GDA_META_STORE (obj);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	if (!priv->cnc) {
		g_object_unref (store);
		store = NULL;
	}
	else {
		if (gda_lockable_trylock (GDA_LOCKABLE (priv->cnc)))
			gda_lockable_unlock (GDA_LOCKABLE (priv->cnc));
		else {
			g_warning (_("Can't obtain connection lock"));
			g_object_unref (store);
			store = NULL;
		}
	}

	return store;
}

static void
gda_meta_store_dispose (GObject *object)
{
	GdaMetaStore *store;

	g_return_if_fail (GDA_IS_META_STORE (object));

	store = GDA_META_STORE (object);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	GSList *list;

	if (priv->extract_stmt_hash) {
		g_hash_table_destroy (priv->extract_stmt_hash);
		priv->extract_stmt_hash = NULL;
	}

	if (priv->override_mode)
		_gda_meta_store_cancel_data_reset (store, NULL);

	g_free (priv->catalog);
	g_free (priv->schema);

	/* custom db objects */
	g_hash_table_destroy (priv->p_db_objects_hash);
	for (list = priv->p_db_objects; list; list = list->next) {
		if (DB_OBJECT (list->data)->store == store)
			db_object_free (DB_OBJECT (list->data));
	}
	g_slist_free (priv->p_db_objects);

	/* internal connection */
	if (priv->cnc) {
		g_object_unref (G_OBJECT (priv->cnc));
		priv->cnc = NULL;
	}
	if (priv->parser) {
		g_object_unref (G_OBJECT (priv->parser));
		priv->parser = NULL;
	}
  if (priv->db_objects) {
    g_slist_free_full (priv->db_objects, (GDestroyNotify) db_object_free);
    priv->db_objects = NULL;
  }
  if (priv->db_objects_hash) {
    g_hash_table_unref (priv->db_objects_hash);
    priv->db_objects_hash = NULL;
  }
  if (priv->table_cond_info_hash) {
    g_hash_table_unref (priv->table_cond_info_hash);
    priv->table_cond_info_hash = NULL;
  }
  if (priv->provider_specifics) {
    g_hash_table_unref (priv->provider_specifics);
    priv->provider_specifics = NULL;
  }
  if (priv->prep_stmts) {
    g_object_unref (priv->prep_stmts[STMT_SET_VERSION]);
	  g_object_unref (priv->prep_stmts[STMT_UPD_VERSION]);
	  g_object_unref (priv->prep_stmts[STMT_GET_VERSION]);
	  g_object_unref (priv->prep_stmts[STMT_DEL_ATT_VALUE]);
	  g_object_unref (priv->prep_stmts[STMT_SET_ATT_VALUE]);
	  g_object_unref (priv->prep_stmts[STMT_ADD_DECLARE_FK]);
	  g_object_unref (priv->prep_stmts[STMT_DEL_DECLARE_FK]);
    g_free (priv->prep_stmts);
    priv->prep_stmts = NULL;
  }
  if (priv->attributes_set != NULL) {
    g_object_unref (priv->attributes_set);
    priv->attributes_set = NULL;
  }

	g_rec_mutex_clear (& (priv->mutex));

	/* parent class */
	G_OBJECT_CLASS (gda_meta_store_parent_class)->dispose (object);
}

static void
gda_meta_store_finalize (GObject *object)
{
	GdaMetaStore *store;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_META_STORE (object));

	store = GDA_META_STORE (object);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	if (priv->init_error)
		g_error_free (priv->init_error);

	/* parent class */
	G_OBJECT_CLASS (gda_meta_store_parent_class)->finalize (object);
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
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	switch (param_id) {
	case PROP_CNC_STRING:
		if (!priv->cnc) {
			cnc_string = g_value_get_string (value);
			if (cnc_string) {
				GdaConnection *cnc;
				GError *error = NULL;
				cnc = gda_connection_new_from_string (NULL, cnc_string, NULL,
								      GDA_CONNECTION_OPTIONS_NONE,
								      &error);
				if (cnc) {
					if (!gda_connection_open (cnc, &error)) {
						g_object_unref (cnc);
					  g_warning (_("Could not open internal GdaMetaStore connection: %s"),
						     error && error->message ? error->message : _("No detail"));
						g_clear_error (&error);
						cnc = NULL;
					}
				} else {
					g_warning (_("Could not create internal GdaMetaStore connection: %s"),
						   error && error->message ? error->message : _("No detail"));
					g_clear_error (&error);
					if (g_ascii_strcasecmp (cnc_string, "sqlite")) {
						/* use _gda_config_sqlite_provider */
						g_clear_error (&error);
						cnc = _gda_open_internal_sqlite_connection (cnc_string);
					}
				}
				priv->cnc = cnc;
			}
		}
		break;
	case PROP_CNC_OBJECT:
		if (!priv->cnc)
			priv->cnc = g_value_dup_object (value);
		break;
	case PROP_CATALOG:
		g_free (priv->catalog);
		if (g_value_get_string (value) && *g_value_get_string (value))
			priv->catalog = g_strdup (g_value_get_string (value));
		else
			priv->catalog = NULL;
		break;
	case PROP_SCHEMA:
		g_free (priv->schema);
		if (g_value_get_string (value) && *g_value_get_string (value))
			priv->schema = g_strdup (g_value_get_string (value));
		else
			priv->schema = NULL;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
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
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	switch (param_id) {
	case PROP_CNC_STRING:
		g_assert_not_reached ();
		break;
	case PROP_CNC_OBJECT:
		g_value_set_object (value, (GObject *) priv->cnc);
		break;
	case PROP_CATALOG:
		g_value_set_string (value, priv->catalog);
		break;
	case PROP_SCHEMA:
		g_value_set_string (value, priv->schema);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gda_meta_store_set_identifiers_style:
 * @store: a #GdaMetaStore object
 * @style: a style
 *
 * Specifies how @store must handle SQL identifiers it has to store. This method is mainly used by
 * database providers.
 *
 * Since: 4.2
 */
void
gda_meta_store_set_identifiers_style (GdaMetaStore *store, GdaSqlIdentifierStyle style)
{
	g_return_if_fail (GDA_IS_META_STORE (store));
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	priv->ident_style = style;
}

/**
 * gda_meta_store_set_reserved_keywords_func:
 * @store: a #GdaMetaStore object
 * @func: (nullable) (scope call): a #GdaSqlReservedKeywordsFunc function, or %NULL
 *
 * Specifies a function which @store will use to determine if a keyword is an SQL reserved
 * keyword or not.
 *
 * This method is mainly used by database providers.
 *
 * Since: 4.2
 */
void
gda_meta_store_set_reserved_keywords_func(GdaMetaStore *store, GdaSqlReservedKeywordsFunc func)
{
	g_return_if_fail (GDA_IS_META_STORE (store));
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	priv->reserved_keyword_func = func;
}

/*
 * Checks the structure of @priv->cnc and update it if necessary
 */
static gboolean prepare_server_operations (GdaMetaStore *store, GError **error);
static gboolean handle_schema_version (GdaMetaStore *store, gboolean *schema_present, GError **error);
static gboolean
initialize_cnc_struct (GdaMetaStore *store, GError **error)
{
	gboolean schema_present;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (priv->cnc), FALSE);

	if (handle_schema_version (store, &schema_present, error))
		return TRUE;
	if (schema_present)
		return FALSE;

	g_clear_error (error);

	/* assume schema not present => create it */
	GSList *list;
	gboolean allok = TRUE;
	GdaServerProvider *prov;
	if (!prepare_server_operations (store, error))
		return FALSE;

	prov = gda_connection_get_provider (priv->cnc);
	for (list = priv->db_objects; list; list = list->next) {
		DbObject *dbo = DB_OBJECT (list->data);
		if (dbo->create_op) {
			if (!gda_server_provider_perform_operation (prov, priv->cnc, dbo->create_op, error)) {
				gchar *sql = NULL;
				GError *lerror = NULL;
				sql = gda_server_operation_render (dbo->create_op, &lerror);
				if (sql == NULL) {
					g_warning (_("Internal error while trying to render operation in MetaStore creation: %s"),
				           lerror && lerror->message ? lerror->message : _("No error was set"));
					g_clear_error (&lerror);
				} else {
					g_warning ("Interna Error. Operation tried to create MetaStore table '%s': %s",
				           dbo->obj_name, sql);
				}
				g_warning (_("Internal Error. Couldn't create MetaStore table '%s': %s"),
				           dbo->obj_name,
				           (*error) && (*error)->message ? (*error)->message : _("No error was set"));
				allok = FALSE;
				break;
			}
			g_object_unref (dbo->create_op);
			dbo->create_op = NULL;
		}
	}
	if (!allok)
		return FALSE;

	/* set version info to CURRENT_SCHEMA_VERSION */
	GdaSet *params;
	if (! gda_statement_get_parameters (priv->prep_stmts[STMT_SET_VERSION], &params, NULL)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
			"%s", _ ("Could not set the internal schema's version. No prepared statement's parameters were found"));
		return FALSE;
	}
	g_assert (gda_set_set_holder_value (params, NULL, "version", CURRENT_SCHEMA_VERSION));
	if (gda_connection_statement_execute_non_select (priv->cnc,
							 priv->prep_stmts[STMT_SET_VERSION],
							 params, NULL, NULL) == -1) {
		g_object_unref (params);
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
			"%s", _ ("Could not set the internal schema's version. Statement execution fails"));
		return FALSE;
	}
	g_object_unref (params);

	return handle_schema_version (store, NULL, error);
}


static GdaServerOperation *create_server_operation_for_table (GHashTable *specific_hash,
							      GdaServerProvider *prov, GdaConnection *cnc,
							      DbObject *dbobj, GError **error);
static GdaServerOperation *create_server_operation_for_view  (GHashTable *specific_hash,
							      GdaServerProvider *prov, GdaConnection *cnc,
							      DbObject *dbobj, GError **error);
static gboolean prepare_dbo_server_operation (GdaMetaStorePrivate *priv, GdaMetaStore *store, GdaServerProvider *prov,
					      DbObject *dbo, GError **error);
static gboolean
prepare_server_operations (GdaMetaStore *store, GError **error)
{
	GSList *objects;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	for (objects = priv->db_objects; objects; objects = objects->next) {
		DbObject *dbo = DB_OBJECT (objects->data);
		if (! prepare_dbo_server_operation (priv, store, gda_connection_get_provider (priv->cnc),
						    dbo, error))
			return FALSE;
	}

	return TRUE;
}

static gboolean
prepare_dbo_server_operation (GdaMetaStorePrivate *priv, GdaMetaStore *store, GdaServerProvider *prov,
			      DbObject *dbo, GError **error)
{
	if (dbo->create_op) {
		g_object_unref (dbo->create_op);
		dbo->create_op = NULL;
	}

	switch (dbo->obj_type) {
	case GDA_SERVER_OPERATION_CREATE_TABLE:
		dbo->create_op = create_server_operation_for_table (priv->provider_specifics,
								    prov, priv->cnc, dbo, error);
		if (!dbo->create_op)
			return FALSE;
		break;
	case GDA_SERVER_OPERATION_CREATE_VIEW:
		dbo->create_op = create_server_operation_for_view (priv->provider_specifics,
								   prov, priv->cnc, dbo, error);
		if (!dbo->create_op)
			return FALSE;
		break;
	default:
		break;
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

	if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR (TRUE),
                                          error,
                                          "/TABLE_DEF_P/TABLE_IFNOTEXISTS"))
    return FALSE;

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
		if (! gda_server_operation_set_value_at (op, NULL, error,
							 "/FIELDS_A/@COLUMN_SIZE/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, tcol->nullok ? "FALSE" : "TRUE", error,
							 "/FIELDS_A/@COLUMN_NNUL/%d", index))
			goto onerror;
		if (! gda_server_operation_set_value_at (op, tcol->autoinc ? "TRUE" : "FALSE", error,
							 "/FIELDS_A/@COLUMN_AUTOINC/%d", index))
			goto onerror;
		repl = provider_specific_match (specific_hash, prov, "dummy", "/FIELDS_A/@COLUMN_PKEY");
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

	/* foreign keys: just used as an indication by the GdaMetaStore object: they are not enforced because
	 * the database may not be up to date everywhere at any time and because the references are sometimes
	 * on views and not on tables, which is not supported by databases */

	return op;
 onerror:
	g_object_unref (op);
	return NULL;
}

static GdaServerOperation *
create_server_operation_for_view (G_GNUC_UNUSED GHashTable *specific_hash,
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

static DbObject *create_table_object (GdaMetaStorePrivate *priv, GdaMetaStore *store, xmlNodePtr node, GError **error);
static DbObject *create_view_object (GdaMetaStorePrivate *priv, GdaMetaStore *store, xmlNodePtr node, GError **error);
static GSList *reorder_db_objects (GSList *objects, GHashTable *hash);
static gboolean complement_db_objects (GSList *objects, GHashTable *hash, GError **error);

/*
 * Creates all DbObject structures and place them into priv->db_objects or priv->p_db_objects
 *
 * If @store is %NULL, then all the DB objects created are attached to priv->db_objects, otherwise
 * they are placed in priv->p_db_objects
 */
static void
create_db_objects (GdaMetaStorePrivate *priv, GdaMetaStore *store)
{
	xmlNodePtr node;
	GError *lerror = NULL;
	GError **error = &lerror;
	GOutputStream *ostream;
	GInputStream *istream;
	GFile *res;
	xmlDocPtr doc = NULL;
	gssize size;
	gchar *schema;

	/* load information schema's structure XML file */
	ostream = g_memory_output_stream_new_resizable ();
	res = g_file_new_for_uri ("resource:///libgda/information_schema.xml");
	istream = G_INPUT_STREAM (g_file_read (res, NULL, &lerror));
	if (istream == NULL) {
		g_warning (_("Internal error: no information schema was found: %s"),
							 lerror && lerror->message ? lerror->message : "No error message was set");
		g_clear_error (&lerror);
		return;
	}
	g_clear_error (&lerror);
	size = g_output_stream_splice (ostream, istream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE, NULL, &lerror);
	if (size == -1) {
		g_warning (_("Internal error: can't read information schema: %s"),
							 lerror && lerror->message ? lerror->message : "No error message was set");
		g_clear_error (&lerror);
		return;
	}
	g_clear_error (&lerror);
	schema = (gchar*) g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (ostream));
	doc = xmlReadDoc ((const xmlChar*) schema, "", NULL, XML_PARSE_RECOVER);
	g_object_unref (ostream);
	g_object_unref (istream);
	g_object_unref (res);

	node = xmlDocGetRootElement (doc);
	if (!node || strcmp ((gchar *) node->name, "schema")) {
		g_warning ("Root node of current information schema should be <schema>.");
		return;
	}

	/* walk through the xmlDoc */
	if (store) {
		priv->p_db_objects = NULL;
	} else
		priv->db_objects = NULL;
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
						g_hash_table_insert (priv->provider_specifics, key, val);
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
			dbo = create_table_object (priv, store, node, error);
			if (!dbo)
				g_error ("Information schema creation error: %s",
					 lerror && lerror->message ? lerror->message : "No detail");
		}
		/* <view> tag for view creation */
		else if (!strcmp ((gchar *) node->name, "view")) {
			DbObject *dbo;
			dbo = create_view_object (priv, store, node, error);
			if (!dbo)
				g_error ("Information schema creation error: %s",
					 lerror && lerror->message ? lerror->message : "No detail");
		}
	}
	xmlFreeDoc (doc);

	if (store) {
		priv->p_db_objects = reorder_db_objects (priv->p_db_objects, priv->p_db_objects_hash);
		if (!complement_db_objects (priv->p_db_objects, priv->p_db_objects_hash, error))
			g_error ("Information schema structure error: %s",
				 lerror && lerror->message ? lerror->message : "No detail");
	}
	else {
		priv->db_objects = reorder_db_objects (priv->db_objects, priv->db_objects_hash);
		if (!complement_db_objects (priv->db_objects, priv->db_objects_hash, error))
			g_error ("Information schema structure error: %s",
				 lerror && lerror->message ? lerror->message : "No detail");
	}

	/* make sure that for each Table DbObject, every TableFKey in @fk_list references a primary key of the referenced table */
	GSList *list;
	for (list = priv->db_objects; list; list = list->next) {
		DbObject *dbo;
		dbo = DB_OBJECT (list->data);

		if (dbo->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE)
			continue;

		TableInfo *tinfo;
		GSList *fklist;
		tinfo = TABLE_INFO (dbo);
		for (fklist = tinfo->fk_list; fklist; fklist = fklist->next) {
			TableFKey *tfk;
			TableInfo *reftinfo;
			gint i;
			tfk = (TableFKey*) fklist->data;
			if (tfk->table_info != dbo)
				g_error ("Information schema structure error for table '%s': "
					 "Foreign key structure attached to wrong table", dbo->obj_name);
			if (!tfk->depend_on || (tfk->depend_on->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE))
				g_error ("Information schema structure error for table '%s': "
					 "Foreign key references an object '%s' which is not a table",
					 dbo->obj_name, tfk->depend_on ? tfk->depend_on->obj_name : "nothing referenced");
			reftinfo = TABLE_INFO (tfk->depend_on);
			if (tfk->cols_nb <= 0)
				g_error ("Information schema structure error for table '%s': "
					 "Foreign key is not composed of at least one column", dbo->obj_name);
			if (!tfk->fk_cols_array || !tfk->fk_names_array ||
			    !tfk->ref_pk_cols_array || !tfk->ref_pk_names_array)
				g_error ("Information schema structure error for table '%s': "
					 "Foreign key is not completely defined", dbo->obj_name);
			for (i = 0; i < tfk->cols_nb; i++) {
				gint j;
				for (j = 0; j < tfk->cols_nb; j++) {
					if ((i != j) && (tfk->ref_pk_cols_array [i] == tfk->ref_pk_cols_array [j]))
						g_error ("Information schema structure error for table '%s': "
							 "column is referenced twice, at position %d and %d", dbo->obj_name,
							 tfk->ref_pk_cols_array [i], tfk->ref_pk_cols_array [j]);
				}
			}
			for (i = 0; i < tfk->cols_nb; i++) {
				TableColumn *tcol;
				tcol = g_slist_nth_data (reftinfo->columns, tfk->ref_pk_cols_array [i]);
				if (!tcol)
					g_error ("Information schema structure error for table '%s': "
						 "cannot identify column at position %d", dbo->obj_name, tfk->ref_pk_cols_array [i]);
				if (!tcol->pkey)
					g_error ("Information schema structure error for table '%s': "
						 "referenced column at position %d is not part of a primary key",
						 dbo->obj_name, tfk->ref_pk_cols_array [i]);
				if (column_name_to_index (reftinfo, tfk->ref_pk_names_array [i]) != tfk->ref_pk_cols_array [i])
					g_error ("Information schema structure error for table '%s': "
						 "referenced column at position %d has wrong associated name '%s'",
						 dbo->obj_name, tfk->ref_pk_cols_array [i], tfk->ref_pk_names_array [i]);
			}
			if (tfk->cols_nb != reftinfo->pk_cols_nb)
				g_error ("Information schema structure error for table '%s': "
					 "Foreign key does only reference part of reterenced table's primary key",
					 dbo->obj_name);
		}
	}
}

static void compute_view_dependencies (GdaMetaStorePrivate *priv, GdaMetaStore *store,
				       DbObject *view_dbobj, GdaSqlStatement *sqlst);
static DbObject *
create_view_object (GdaMetaStorePrivate *priv, GdaMetaStore *store, xmlNodePtr node, GError **error)
{
	DbObject *dbobj = NULL;
	xmlChar *view_name;
	gchar *complete_obj_name = NULL;

	view_name = xmlGetProp (node, BAD_CAST "name");
	if (!view_name) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
			     "%s", _("Missing view name from <view> node"));
		return NULL;
	}

	/* determine object's complete name */
	if (store) {
		if (priv->schema) {
			complete_obj_name = g_strdup_printf ("%s.%s", priv->schema, (gchar *) view_name);
		}
	} else
		complete_obj_name = g_strdup ((gchar *) view_name);

	/* DbObject structure */
	if (store) {
		dbobj = g_hash_table_lookup (priv->p_db_objects_hash, view_name);
	} else
		dbobj = g_hash_table_lookup (priv->db_objects_hash, view_name);
	if (!dbobj) {
		dbobj = g_new0 (DbObject, 1);
		dbobj->store = store;
		dbobj->obj_name = g_strdup ((gchar *) view_name);
		if (store) {
			priv->p_db_objects = g_slist_prepend (priv->p_db_objects, dbobj);
			g_hash_table_insert (priv->p_db_objects_hash, dbobj->obj_name, dbobj);
		}
		else {
			priv->db_objects = g_slist_prepend (priv->db_objects, dbobj);
			g_hash_table_insert (priv->db_objects_hash, dbobj->obj_name, dbobj);
		}
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
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
				     "%s", _("Missing view definition from <view> node"));
			goto onerror;
		}

		/* use a parser to analyze the view dependencies */
		GdaStatement *stmt;
		const gchar *remain;
		stmt = gda_sql_parser_parse_string (priv->parser, (gchar *) def, &remain, error);
		if (!stmt) {
			xmlFree (def);
			goto onerror;
		}
		if (remain) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
				     _("View definition contains more than one statement (for view '%s')"),
				     complete_obj_name);
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
			compute_view_dependencies (priv, store, dbobj, sqlst);
			gda_sql_statement_free (sqlst);
			g_object_unref (stmt);

#ifdef GDA_DEBUG_NO
			g_print ("View %s depends on: ", complete_obj_name);
			GSList *list;
			for (list = dbobj->depend_list; list; list = list->next)
				g_print ("%s ", DB_OBJECT (list->data)->obj_name);
			g_print ("\n");
#endif
		}
		else {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
				     _("View definition is not a selection statement (for view '%s')"),
				     complete_obj_name);
			g_object_unref (stmt);
			goto onerror;
		}
	}
	g_free (complete_obj_name);
	return dbobj;

onerror:
	g_free (complete_obj_name);
	db_object_free (dbobj);
	return NULL;
}

static GdaSqlExpr *make_expr_EQUAL (GdaSqlAnyPart *parent, xmlChar *cname, xmlChar *type, GType ptype, gboolean nullok, gint index);
static GdaSqlExpr *make_expr_AND (GdaSqlAnyPart *parent, GdaSqlExpr *current);
static DbObject *
create_table_object (GdaMetaStorePrivate *priv, GdaMetaStore *store, xmlNodePtr node, GError **error)
{
	DbObject *dbobj;
	xmlChar *table_name;
	gchar *complete_obj_name;

	table_name = xmlGetProp (node, BAD_CAST "name");
	if (!table_name) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
			     "%s", _("Missing table name from <table> node"));
		return NULL;
	}

	/* determine object's complete name */
  if (store)
    {
      priv = gda_meta_store_get_instance_private (store);
      if (priv->schema)
        complete_obj_name = g_strdup_printf ("%s.%s", priv->schema, (gchar *) table_name);
      else
        complete_obj_name = g_strdup ((gchar *) table_name);
    }
  else
    complete_obj_name = g_strdup ((gchar *) table_name);

	/* DbObject structure */
	if (store) {
		priv = gda_meta_store_get_instance_private (store);
		dbobj = g_hash_table_lookup (priv->p_db_objects_hash, table_name);
	} else
		dbobj = g_hash_table_lookup (priv->db_objects_hash, table_name);
	if (!dbobj) {
		dbobj = g_new0 (DbObject, 1);
		dbobj->store = store;
		dbobj->obj_name = g_strdup ((gchar *) table_name);
		if (store) {
			priv->p_db_objects = g_slist_prepend (priv->p_db_objects, dbobj);
			g_hash_table_insert (priv->p_db_objects_hash, dbobj->obj_name, dbobj);
		}
		else {
			priv->db_objects = g_slist_prepend (priv->db_objects, dbobj);
			g_hash_table_insert (priv->db_objects_hash, dbobj->obj_name, dbobj);
		}
	}
	xmlFree (table_name);
	dbobj->obj_type = GDA_SERVER_OPERATION_CREATE_TABLE;
	TABLE_INFO (dbobj)->gtype_column = -1;

	/* current_all */
	gchar *sql;
	sql = g_strdup_printf ("SELECT * FROM %s", complete_obj_name);
	TABLE_INFO (dbobj)->current_all = compute_prepared_stmt (priv->parser, sql);
	g_free (sql);
	if (!TABLE_INFO (dbobj)->current_all) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INTERNAL_ERROR,
			     "Internal fatal error: could not create SELECT ALL statement (for table '%s')",
			     complete_obj_name);
		goto onerror;
	}

	/* delete all */
	sql = g_strdup_printf ("DELETE FROM %s", complete_obj_name);
	TABLE_INFO (dbobj)->delete_all = compute_prepared_stmt (priv->parser, sql);
	g_free (sql);
	if (!TABLE_INFO (dbobj)->delete_all) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INTERNAL_ERROR,
			     "Internal fatal error: could not create DELETE ALL statement (for table '%s')",
			     complete_obj_name);
		goto onerror;
	}

	/* INSERT, UPDATE and DELETE statements */
	GdaSqlStatement *sql_ist;
	GdaSqlStatementInsert *ist;
	GdaSqlStatement *sql_ust;
	GdaSqlStatementUpdate *ust;
	GdaSqlStatement *sql_dst;
	GdaSqlStatementDelete *dst;

	sql_ist = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
	ist = (GdaSqlStatementInsert*) sql_ist->contents;
	g_assert (GDA_SQL_ANY_PART (ist)->type == GDA_SQL_ANY_STMT_INSERT);

	sql_ust = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
	ust = (GdaSqlStatementUpdate*) sql_ust->contents;
	g_assert (GDA_SQL_ANY_PART (ust)->type == GDA_SQL_ANY_STMT_UPDATE);

	sql_dst = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE);
	dst = (GdaSqlStatementDelete*) sql_dst->contents;
	g_assert (GDA_SQL_ANY_PART (dst)->type == GDA_SQL_ANY_STMT_DELETE);

	ist->table = gda_sql_table_new (GDA_SQL_ANY_PART (ist));
	ist->table->table_name = g_strdup ((gchar *) complete_obj_name);

	ust->table = gda_sql_table_new (GDA_SQL_ANY_PART (ust));
	ust->table->table_name = g_strdup ((gchar *) complete_obj_name);

	dst->table = gda_sql_table_new (GDA_SQL_ANY_PART (dst));
	dst->table->table_name = g_strdup ((gchar *) complete_obj_name);

	/* walk through the columns and Fkey nodes */
	xmlNodePtr cnode;
	gint colindex = 0;
	GSList *insert_values_list = NULL;
	for (cnode = node->children; cnode; cnode = cnode->next) {
		if (!strcmp ((gchar *) cnode->name, "column")) {
			xmlChar *cname, *ctype, *xstr;
                        gboolean pkey = FALSE;
                        gboolean nullok = FALSE;
			gboolean autoinc = FALSE;

                        cname = xmlGetProp (cnode, BAD_CAST "name");
                        if (!cname)
                                g_error ("Missing column name (table=%s)", complete_obj_name);
			if (g_str_has_suffix ((const gchar*) cname, "gtype"))
				TABLE_INFO (dbobj)->gtype_column = colindex;

                        xstr = xmlGetProp (cnode, BAD_CAST "pkey");
                        if (xstr) {
                                if ((*xstr == 'T') || (*xstr == 't'))
                                        pkey = TRUE;
                                xmlFree (xstr);
                        }
                        xstr = xmlGetProp (cnode, BAD_CAST "nullok");
                        if (xstr) {
                                if ((*xstr == 'T') || (*xstr == 't'))
                                        nullok = TRUE;
                                xmlFree (xstr);
                        }
                        xstr = xmlGetProp (cnode, BAD_CAST "autoinc");
                        if (xstr) {
                                if ((*xstr == 'T') || (*xstr == 't'))
                                        autoinc = TRUE;
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

			/* SQL identifier ? */
#define MAX_IDENT_IN_TABLE 11
			xstr = xmlGetProp (cnode, BAD_CAST "ident");
                        if (xstr) {
                                if ((*xstr == 'T') || (*xstr == 't')) {
					if (!TABLE_INFO (dbobj)->ident_cols) {
						TABLE_INFO (dbobj)->ident_cols = g_new0 (gint, MAX_IDENT_IN_TABLE);
						TABLE_INFO (dbobj)->ident_cols_size = 0;
					}
					g_assert (TABLE_INFO (dbobj)->ident_cols_size < MAX_IDENT_IN_TABLE);
					TABLE_INFO (dbobj)->ident_cols [TABLE_INFO (dbobj)->ident_cols_size] = colindex;
					TABLE_INFO (dbobj)->ident_cols_size ++;
				}
                                xmlFree (xstr);
                        }

			/* parameter */
                        GType ptype;
                        GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
                        GdaSqlExpr *expr;
                        ptype = ctype ? gda_g_type_from_string ((gchar *) ctype) : G_TYPE_STRING;
			if (ptype == G_TYPE_INVALID)
				ptype = GDA_TYPE_NULL;
                        pspec->name = g_strdup_printf ("+%d", colindex);
                        pspec->g_type = ptype;
                        pspec->nullok = nullok;
                        expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ist));
                        expr->param_spec = pspec;
                        insert_values_list = g_slist_append (insert_values_list, expr);

                        pspec = g_new0 (GdaSqlParamSpec, 1);
                        pspec->name = g_strdup_printf ("+%d", colindex);
                        pspec->g_type = ptype;
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
                                        if (ust->cond->cond->operator_type == GDA_SQL_OPERATOR_TYPE_AND)
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
                                        if (dst->cond->cond->operator_type == GDA_SQL_OPERATOR_TYPE_AND)
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
			TableColumn *tcol = NULL;
			GSList *tlist;
			for (tlist = TABLE_INFO (dbobj)->columns; tlist; tlist = tlist->next) {
				if (((TableColumn*) tlist->data)->column_name &&
				    !strcmp (((TableColumn*) tlist->data)->column_name, (gchar*) cname)) {
					tcol = (TableColumn*) tlist->data;
					if ((tcol->gtype != ptype) ||
					    (tcol->pkey != pkey) ||
					    (tcol->nullok != nullok) ||
					    (tcol->autoinc != autoinc) ||
					    (! tcol->column_type && ctype) ||
					    (tcol->column_type && !ctype) ||
					    (tcol->column_type && strcmp (tcol->column_type, (gchar *) ctype))) {
						g_set_error (error, GDA_META_STORE_ERROR,
							     GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
							     _("Column '%s' already exists and has different characteristics"),
							     tcol->column_name);
						xmlFree (cname);
						if (ctype)
							xmlFree (ctype);
						goto onerror;
					}
					break;
				}
			}
			if (!tcol) {
				tcol = g_new0 (TableColumn, 1);
				TABLE_INFO (dbobj)->columns = g_slist_append (TABLE_INFO (dbobj)->columns, tcol);
				tcol->column_name = g_strdup ((gchar *) cname);
				tcol->column_type = ctype ? g_strdup ((gchar *) ctype) : NULL;
				tcol->gtype = ptype;
				tcol->pkey = pkey;
				tcol->nullok = nullok;
				tcol->autoinc = autoinc;
			}

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
				g_set_error (error, GDA_META_STORE_ERROR,
					     GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
					     _("Missing foreign key's referenced table name (for table '%s')"),
					     complete_obj_name);
				goto onerror;
			}

			/* referenced DbObject */
			DbObject *ref_obj;
			if (store) {
				priv = gda_meta_store_get_instance_private (store);
				ref_obj = g_hash_table_lookup (priv->p_db_objects_hash, ref_table);
			} else
				ref_obj = g_hash_table_lookup (priv->db_objects_hash, ref_table);
			if (!ref_obj) {
				ref_obj = g_new0 (DbObject, 1);
				ref_obj->store = store;
				ref_obj->obj_name = g_strdup ((gchar *) ref_table);
				if (store) {
					priv = gda_meta_store_get_instance_private (store);
					priv->p_db_objects = g_slist_prepend (priv->p_db_objects,
											  ref_obj);
					g_hash_table_insert (priv->p_db_objects_hash, ref_obj->obj_name,
							     ref_obj);
				}
				else {
					priv->db_objects = g_slist_prepend (priv->db_objects, ref_obj);
					g_hash_table_insert (priv->db_objects_hash, ref_obj->obj_name, ref_obj);
				}
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
					g_set_error (error, GDA_META_STORE_ERROR,
						     GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
						     _("Missing foreign key's column name (for table '%s')"),
						     complete_obj_name);
					table_fkey_free (tfk);
					goto onerror;
				}
				ref_col = xmlGetProp (fnode, BAD_CAST "ref_column");
				tfk->fk_cols_array [fkcolindex] = column_name_to_index (TABLE_INFO (dbobj),
											(gchar *) col);
				tfk->fk_names_array [fkcolindex] = g_strdup ((gchar *) col);
				if (tfk->fk_cols_array [fkcolindex] < 0) {
					g_set_error (error, GDA_META_STORE_ERROR,
						     GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
						     _("Column '%s' not found in table '%s'"), (gchar *) col,
						     complete_obj_name);
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
	ist->values_list = g_slist_append (NULL, insert_values_list);
	TABLE_INFO (dbobj)->insert = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_ist, NULL);
	gda_sql_statement_free (sql_ist);

	TABLE_INFO (dbobj)->update = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_ust, NULL);
	gda_sql_statement_free (sql_ust);

	TABLE_INFO (dbobj)->delete = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_dst, NULL);
	gda_sql_statement_free (sql_dst);

	if (TABLE_INFO (dbobj)->pk_cols_nb == 0)
		g_error ("Missing key fields identification (table=%s)", complete_obj_name);

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
			 complete_obj_name);
	if (!gda_statement_get_parameters (TABLE_INFO (dbobj)->update, &params, NULL))
		g_error ("Internal fatal error: could not get UPDATE statement's parameters (table=%s)",
			 complete_obj_name);
	gda_set_merge_with_set (TABLE_INFO (dbobj)->params, params);
	g_object_unref (params);

	if (!gda_statement_get_parameters (TABLE_INFO (dbobj)->delete, &params, NULL))
		g_error ("Internal fatal error: could not get DELETE statement's parameters (table=%s)",
			 complete_obj_name);
	gda_set_merge_with_set (TABLE_INFO (dbobj)->params, params);
	g_object_unref (params);

	/* insert DbObject */
	if (store) {
		priv = gda_meta_store_get_instance_private (store);
		g_hash_table_insert (priv->p_db_objects_hash, dbobj->obj_name, dbobj);
	} else
		g_hash_table_insert (priv->db_objects_hash, dbobj->obj_name, dbobj);

	g_free (complete_obj_name);
	return dbobj;

 onerror:
	g_free (complete_obj_name);
	db_object_free (dbobj);
	return NULL;
}


static GdaSqlExpr *
make_expr_AND (GdaSqlAnyPart *parent, GdaSqlExpr *current)
{
	GdaSqlExpr *expr;
	expr = gda_sql_expr_new (parent);
	expr->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
	expr->cond->operator_type = GDA_SQL_OPERATOR_TYPE_AND;

	expr->cond->operands = g_slist_append (NULL, current);
	GDA_SQL_ANY_PART (current)->parent = GDA_SQL_ANY_PART (expr->cond);

	return expr;
}

static GdaSqlExpr *
make_expr_EQUAL (GdaSqlAnyPart *parent, xmlChar *cname, G_GNUC_UNUSED xmlChar *type, GType ptype,
		 gboolean nullok, gint index)
{
	GdaSqlOperation *op;
	GdaSqlExpr *retexpr, *expr;
	GdaSqlParamSpec *pspec;

	retexpr = gda_sql_expr_new (parent);

	op = gda_sql_operation_new (GDA_SQL_ANY_PART (retexpr));
	op->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
	retexpr->cond = op;

	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	g_value_set_string ((expr->value = gda_value_new (G_TYPE_STRING)), (gchar *) cname);
	op->operands = g_slist_append (op->operands, expr);

	pspec = g_new0 (GdaSqlParamSpec, 1);
	pspec->name = g_strdup_printf ("-%d", index);
	pspec->g_type = ptype;
	pspec->nullok = nullok;
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	expr->param_spec = pspec;
	op->operands = g_slist_append (op->operands, expr);

	return retexpr;
}


static void
compute_view_dependencies (GdaMetaStorePrivate *priv, GdaMetaStore *store,
			   DbObject *view_dbobj, GdaSqlStatement *sqlst)
{
	if (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) {
		GdaSqlStatementSelect *selst;
		selst = (GdaSqlStatementSelect*) (sqlst->contents);
		GSList *targets;
		for (targets = selst->from->targets; targets; targets = targets->next) {
			GdaSqlSelectTarget *t = (GdaSqlSelectTarget *) targets->data;

			if (!t->table_name)
				continue;
			DbObject *ref_obj = NULL;
			if (store) {
				priv = gda_meta_store_get_instance_private (store);
				ref_obj = g_hash_table_lookup (priv->p_db_objects_hash, t->table_name);
			} else
				ref_obj = g_hash_table_lookup (priv->db_objects_hash, t->table_name);

			if (!ref_obj) {
				ref_obj = g_new0 (DbObject, 1);
				ref_obj->store = store;
				ref_obj->obj_name = g_strdup (t->table_name);
				if (store) {
					priv = gda_meta_store_get_instance_private (store);
					priv->p_db_objects = g_slist_prepend (priv->p_db_objects,
											  ref_obj);
					g_hash_table_insert (priv->p_db_objects_hash, ref_obj->obj_name,
							     ref_obj);
				}
				else {
					priv->db_objects = g_slist_prepend (priv->db_objects, ref_obj);
					g_hash_table_insert (priv->db_objects_hash, ref_obj->obj_name, ref_obj);
				}
			}
			view_dbobj->depend_list = g_slist_append (view_dbobj->depend_list, ref_obj);
		}
	}
	else if (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND) {
		GdaSqlStatementCompound *cst;
		GSList *list;
		cst = (GdaSqlStatementCompound*) (sqlst->contents);
		for (list = cst->stmt_list; list; list = list->next)
			compute_view_dependencies (priv, store, view_dbobj, (GdaSqlStatement*) list->data);
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
reorder_db_objects (GSList *objects, G_GNUC_UNUSED GHashTable *hash)
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
complement_db_objects (GSList *objects, G_GNUC_UNUSED GHashTable *hash, GError **error)
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
					g_set_error (error, GDA_META_STORE_ERROR,
						     GDA_META_STORE_META_CONTEXT_ERROR,
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
	if (dbobj) {
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
}

static void
table_info_free_contents (TableInfo *info)
{
	g_slist_free_full (info->columns, (GDestroyNotify) table_column_free);
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
	g_slist_free_full (info->fk_list, (GDestroyNotify) table_fkey_free);
	g_slist_free (info->reverse_fk_list);
	if (info->ident_cols)
		g_free (info->ident_cols);
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
update_schema_version (GdaMetaStore *store, G_GNUC_UNUSED const gchar *version, GError **error)
{
	GdaSet *params;
	GdaMetaStoreClass *klass;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (! gda_statement_get_parameters (priv->prep_stmts[STMT_UPD_VERSION], &params, NULL)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
			"%s", _ ("Could not update the internal schema's version. No prepared statement's parameters were found"));
		return FALSE;
	}
	g_assert (gda_set_set_holder_value (params, NULL, "version", CURRENT_SCHEMA_VERSION));
	if (gda_connection_statement_execute_non_select (priv->cnc,
							 priv->prep_stmts[STMT_UPD_VERSION],
							 params, NULL, NULL) == -1) {
		g_object_unref (params);
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
			"%s", _ ("Could not update the internal schema's version. Statement execution fails"));
		return FALSE;
	}
	g_object_unref (params);


	/* update version */
	priv->version = atoi (CURRENT_SCHEMA_VERSION); /* Flawfinder: ignore */
	return TRUE;
}

/*
 * If no transaction is started, then start one
 * @out_started: a place to store if a transaction was actually started in the function
 *
 * Returns: TRUE if no error occurred
 */
static gboolean
check_transaction_started (GdaConnection *cnc, gboolean *out_started)
{
        GdaTransactionStatus *trans;

        trans = gda_connection_get_transaction_status (cnc);
        if (!trans) {
                if (!gda_connection_begin_transaction (cnc, NULL,
                                                       GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL))
                        return FALSE;
                else
                        *out_started = TRUE;
        }
        return TRUE;
}

/*
 * Create a database object from its name,
 * to be used by functions implementing versions migrations
 */
static gboolean
create_a_dbobj (GdaMetaStore *store, const gchar *obj_name, GError **error)
{
	DbObject *dbobj;
	GdaServerProvider *prov;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	dbobj = g_hash_table_lookup (priv->db_objects_hash, obj_name);
	if (!dbobj) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
			     _("Schema description does not contain the object '%s', check installation"),
			     obj_name);
		return FALSE;
	}
	prov = gda_connection_get_provider (priv->cnc);
	if (! prepare_dbo_server_operation (priv, store, prov, dbobj, error))
		return FALSE;
	g_assert (dbobj->create_op);

	gboolean retval;
	retval = gda_server_provider_perform_operation (prov, priv->cnc, dbobj->create_op, error);
	g_object_unref (dbobj->create_op);
	dbobj->create_op = NULL;
	return retval;
}

/*
 * Create a database object from its name,
 * to be used by functions implementing versions migrations
 */
static gboolean
add_a_column (GdaMetaStore *store, const gchar *table_name, const gchar *column_name, GError **error)
{
	DbObject *dbobj;
	GdaServerProvider *prov;
	gboolean retval = FALSE;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	dbobj = g_hash_table_lookup (priv->db_objects_hash, table_name);
	if (!dbobj) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
			     _("Schema description does not contain the object '%s', check installation"),
			     table_name);
		return FALSE;
	}
	prov = gda_connection_get_provider (priv->cnc);
	
	GdaServerOperation *op;
	op = gda_server_provider_create_operation (prov, priv->cnc, GDA_SERVER_OPERATION_ADD_COLUMN, NULL, error);
	if (!op)
		return FALSE;

	GSList *list;
	if (! gda_server_operation_set_value_at (op, table_name, error, "/COLUMN_DEF_P/TABLE_NAME"))
		goto out;

	for (list = TABLE_INFO (dbobj)->columns; list; list = list->next) {
		TableColumn *tcol = TABLE_COLUMN (list->data);
		if (!strcmp (tcol->column_name, column_name)) {
			const gchar *repl;
			if (! gda_server_operation_set_value_at (op, tcol->column_name,
								 error, "/COLUMN_DEF_P/COLUMN_NAME"))
				goto out;
			repl = provider_specific_match (priv->provider_specifics, prov,
							tcol->column_type ? tcol->column_type : "string",
							"/FIELDS_A/@COLUMN_TYPE");
			if (! gda_server_operation_set_value_at (op, repl ? repl : "string", error,
								 "/COLUMN_DEF_P/COLUMN_TYPE"))
				goto out;
			if (! gda_server_operation_set_value_at (op, NULL, error,
								 "/COLUMN_DEF_P/COLUMN_SIZE"))
				goto out;
			if (! gda_server_operation_set_value_at (op, tcol->nullok ? "FALSE" : "TRUE", error,
								 "/COLUMN_DEF_P/COLUMN_NNUL"))
				goto out;
			break;
		}
	}
	if (!list) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
			     _("Could not find description for column named '%s'"), column_name);
		goto out;
	}

	retval = gda_server_provider_perform_operation (prov, priv->cnc, op, error);

 out:
	g_object_unref (op);
	return retval;
}

/* migrate schema from version 1 to 2 */
static void
migrate_schema_from_v1_to_v2 (GdaMetaStore *store, GError **error)
{
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	g_return_if_fail (GDA_IS_CONNECTION (priv->cnc));
	g_return_if_fail (gda_connection_is_opened (priv->cnc));

	/* begin a transaction if possible */
	gboolean transaction_started = FALSE;
	if (! check_transaction_started (priv->cnc, &transaction_started))
		return;

	/* create tables for this migration */
	if (! create_a_dbobj (store, "_table_indexes", error))
		goto out;
	if (! create_a_dbobj (store, "_index_column_usage", error))
		goto out;

	/* set version info to CURRENT_SCHEMA_VERSION */
	update_schema_version (store, "2", error);

 out:
	if (transaction_started) {
		/* handle transaction started if necessary */
		if (priv->version != 2)
			gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
		else
			gda_connection_commit_transaction (priv->cnc, NULL, NULL);
	}
}

/* migrate schema from version 2 to 3 */
static void
migrate_schema_from_v2_to_v3 (GdaMetaStore *store, GError **error)
{
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	g_return_if_fail (GDA_IS_CONNECTION (priv->cnc));
	g_return_if_fail (gda_connection_is_opened (priv->cnc));

	/* begin a transaction if possible */
	gboolean transaction_started = FALSE;
	if (! check_transaction_started (priv->cnc, &transaction_started))
		return;

	/* create tables for this migration */
	if (! create_a_dbobj (store, "__declared_fk", error))
		goto out;

	/* set version info to CURRENT_SCHEMA_VERSION */
	update_schema_version (store, "3", error);

 out:
	if (transaction_started) {
		/* handle transaction started if necessary */
		if (priv->version != 3)
			gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
		else
			gda_connection_commit_transaction (priv->cnc, NULL, NULL);
	}
}

/* migrate schema from version 2 to 3 */
static void
migrate_schema_from_v3_to_v4 (GdaMetaStore *store, GError **error)
{
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	g_return_if_fail (GDA_IS_CONNECTION (priv->cnc));
	g_return_if_fail (gda_connection_is_opened (priv->cnc));

	/* begin a transaction if possible */
	gboolean transaction_started = FALSE;
	if (! check_transaction_started (priv->cnc, &transaction_started))
		return;

	if (! add_a_column (store, "_schemata", "schema_default", error))
		goto out;

	/* set version info to CURRENT_SCHEMA_VERSION */
	update_schema_version (store, "4", error);

 out:
	if (transaction_started) {
		/* handle transaction started if necessary */
		if (priv->version != 4)
			gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
		else
			gda_connection_commit_transaction (priv->cnc, NULL, NULL);
	}
}

static gboolean
handle_schema_version (GdaMetaStore *store, gboolean *schema_present, GError **error)
{
	GdaDataModel *model;
	GdaMetaStoreClass *klass = (GdaMetaStoreClass *) G_OBJECT_GET_CLASS (store);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	model = gda_connection_statement_execute_select_fullv (priv->cnc,
							       priv->prep_stmts[STMT_GET_VERSION],
							       NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL,
							       0, G_TYPE_STRING, -1);
	if (schema_present)
		*schema_present = model ? TRUE : FALSE;
	if (model) {
		const GValue *version;
		if (gda_data_model_get_n_rows (model) != 1) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
				"%s", _("Could not get the internal schema's version"));
			g_object_unref (model);
			return FALSE;
		}

		version = gda_data_model_get_value_at (model, 0, 0, error);
		if (!version)
			return FALSE;

		if (gda_value_is_null (version) || !gda_value_isa (version, G_TYPE_STRING)) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
				"%s", _("Could not get the internal schema's version"));
			g_object_unref (model);
			return FALSE;
		}
		priv->version = atoi (g_value_get_string (version)); /* Flawfinder: ignore */
		if (priv->version < 1)
			priv->version = 1;
		if (priv->version != atoi (CURRENT_SCHEMA_VERSION)) { /* Flawfinder: ignore */
			switch (priv->version) {
			case 1:
				migrate_schema_from_v1_to_v2 (store, error);
                                /* fallthrough */
			case 2:
				migrate_schema_from_v2_to_v3 (store, error);
                                /* fallthrough */
			case 3:
				migrate_schema_from_v3_to_v4 (store, error);
                                /* fallthrough */
			case 4:
				/* function call for migration from V4 will be here */
				break;
			default:
				/* no downgrade to do */
				break;
			}

			if (priv->version != atoi (CURRENT_SCHEMA_VERSION)) { /* Flawfinder: ignore */
				/* it's an error */
				g_object_unref (model);
				return FALSE;
			}
		}
		g_object_unref (model);
		return TRUE;
	}
	else {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
			"%s", _("Could not get the internal schema's version"));
		return FALSE;
	}
}

/**
 * gda_meta_store_get_version:
 * @store: a #GdaMetaStore object
 *
 * Get @store's internal schema's version
 *
 * Returns: the version (incremented each time the schema changes, backward compatible)
 */
gint
gda_meta_store_get_version (GdaMetaStore *store) {
	g_return_val_if_fail (GDA_IS_META_STORE (store), 0);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	return priv->version;
}

/**
 * gda_meta_store_get_internal_connection:
 * @store: a #GdaMetaStore object
 *
 * Get a pointer to the #GdaConnection object internally used by @store to store
 * its contents.
 *
 * The returned connection can be used to access some other data than the one managed by @store
 * itself. The returned object is not owned by the caller (if you need to keep it, then use g_object_ref()).
 * Do not close the connection.
 *
 * Returns: (transfer none): a #GdaConnection, or %NULL
 */
GdaConnection *
gda_meta_store_get_internal_connection (GdaMetaStore *store) {
	g_return_val_if_fail (GDA_IS_META_STORE (store), 0);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	return priv->cnc;
}

/**
 * gda_meta_store_sql_identifier_quote:
 * @id: an SQL identifier
 * @cnc: a #GdaConnection
 *
 * Use this method to get a correctly quoted (if necessary) SQL identifier which can be used
 * to retrieve or filter information in a #GdaMetaStore which stores meta data about @cnc.
 *
 * The returned SQL identifier can be used in conjunction with gda_connection_update_meta_store(),
 * gda_connection_get_meta_store_data(), gda_connection_get_meta_store_data_v() and
 * gda_meta_store_extract().
 *
 * Returns: (transfer full): a new string, to free with g_free() once not needed anymore
 *
 * Since: 4.0.3
 */
gchar *
gda_meta_store_sql_identifier_quote (const gchar *id, GdaConnection *cnc)
{
	GdaConnectionOptions cncoptions;
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	g_object_get (G_OBJECT (cnc), "options", &cncoptions, NULL);
	return gda_sql_identifier_quote (id, cnc, NULL, TRUE,
					 cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
}

/**
 * gda_meta_store_extract:
 * @store: a #GdaMetaStore object
 * @select_sql: a SELECT statement
 * @error: (nullable): a place to store errors, or %NULL
 * @...: a list of (variable name (gchar *), GValue *value) terminated with NULL, representing values for all the
 * variables mentioned in @select_sql. If there is no variable then this part can be omitted.
 *
 * Extracts some data stored in @store using a custom SELECT query. If the @select_sql filter involves
 * SQL identifiers (such as table or column names), then the values should have been adapted using
 * gda_meta_store_sql_identifier_quote().
 *
 * For more information about
 * SQL identifiers are represented in @store, see the
 * <link linkend="information_schema:sql_identifiers">meta data section about SQL identifiers</link>.
 *
 * Returns: (transfer full): a new #GdaDataModel, or %NULL if an error occurred
 */
GdaDataModel *
gda_meta_store_extract (GdaMetaStore *store, const gchar *select_sql, GError **error, ...)
{
	GdaStatement *stmt = NULL;
	GdaDataModel *model;
	GdaSet *params = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (select_sql, NULL);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return NULL;
	}

	g_rec_mutex_lock (& (priv->mutex));

	if ((priv->max_extract_stmt > 0) && !priv->extract_stmt_hash)
		priv->extract_stmt_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	/* statement creation */
	if (priv->extract_stmt_hash)
		stmt = g_hash_table_lookup (priv->extract_stmt_hash, select_sql);
	if (stmt)
		g_object_ref (stmt);
	else {
		const gchar *remain;

		stmt = gda_sql_parser_parse_string (priv->parser, select_sql, &remain, error);
		if (!stmt) {
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
		if (remain) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
				     "%s", _("More than one SQL statement"));
			g_object_unref (stmt);
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}

		if (priv->current_extract_stmt < priv->max_extract_stmt) {
			g_hash_table_insert (priv->extract_stmt_hash, g_strdup (select_sql), g_object_ref (stmt));
			priv->current_extract_stmt++;
		}
	}

	/* parameters */
	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		g_rec_mutex_unlock (& (priv->mutex));
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
				GError *lerror = NULL;
				if (!gda_holder_set_value (h, value, &lerror)) {
					g_object_unref (stmt);
					g_object_unref (params);
					va_end (ap);
					g_slist_free (params_set);
					g_rec_mutex_unlock (& (priv->mutex));
					g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
					     _("While extracting data from Meta Store: %s"),
					     lerror && lerror->message ? lerror->message : "No error message was set");
					g_clear_error (&lerror);
					return NULL;
				}
				params_set = g_slist_prepend (params_set, h);
			}
		}
		va_end (ap);

		for (list = gda_set_get_holders (params); list; list = list->next) {
			if (!g_slist_find (params_set, list->data))
				g_warning (_("No value set for parameter '%s'"),
					   gda_holder_get_id (GDA_HOLDER (list->data)));
		}
		g_slist_free (params_set);
	}

	/* execution */
	model = gda_connection_statement_execute_select (priv->cnc, stmt, params, error);
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);

	g_rec_mutex_unlock (& (priv->mutex));
	return model;
}

/**
 * gda_meta_store_extract_v: (rename-to gda_meta_store_extract)
 * @store: a #GdaMetaStore object
 * @select_sql: a SELECT statement
 * @vars: (element-type utf8 GObject.Value) (nullable): a hash table with all variables names as keys and GValue* as
 * value, representing values for all the variables mentioned in @select_sql. If there is no variable then this part can be
 * omitted.
 * @error: a place to store errors, or %NULL
 *
 * Extracts some data stored in @store using a custom SELECT query. If the @select_sql filter involves
 * SQL identifiers (such as table or column names), then the values should have been adapted using
 * gda_meta_store_sql_identifier_quote().
 *
 * For more information about
 * SQL identifiers are represented in @store, see the
 * <link linkend="information_schema:sql_identifiers">meta data section about SQL identifiers</link>.
 *
 * Returns: (transfer full): a new #GdaDataModel, or %NULL if an error occurred
 *
 * Since: 4.2.6
 */
GdaDataModel *
gda_meta_store_extract_v (GdaMetaStore *store, const gchar *select_sql, GHashTable *vars, GError **error)
{
	GdaStatement *stmt = NULL;
	GdaDataModel *model;
	GdaSet *params = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (select_sql, NULL);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return NULL;
	}

	g_rec_mutex_lock (& (priv->mutex));

	if ((priv->max_extract_stmt > 0) && !priv->extract_stmt_hash)
		priv->extract_stmt_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	/* statement creation */
	if (priv->extract_stmt_hash)
		stmt = g_hash_table_lookup (priv->extract_stmt_hash, select_sql);
	if (stmt)
		g_object_ref (stmt);
	else {
		const gchar *remain;

		stmt = gda_sql_parser_parse_string (priv->parser, select_sql, &remain, error);
		if (!stmt) {
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
		if (remain) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
				     "%s", _("More than one SQL statement"));
			g_object_unref (stmt);
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}

		if (priv->current_extract_stmt < priv->max_extract_stmt) {
			g_hash_table_insert (priv->extract_stmt_hash, g_strdup (select_sql), g_object_ref (stmt));
			priv->current_extract_stmt++;
		}
	}

	/* parameters */
	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}
	if (params) {
		GSList *list, *params_set = NULL;
		gchar *pname;
		GValue *value;
		GHashTableIter iter;
		g_hash_table_iter_init (&iter, vars);
		while (g_hash_table_iter_next (&iter, (gpointer*)&pname, (gpointer*)&value)) {
			GdaHolder *h;
			h = gda_set_get_holder (params, pname);
			if (!h)
				g_message (_("Parameter '%s' is not present in statement"), pname);
			else {
				GError *lerror = NULL;
				if (!gda_holder_set_value (h, value, &lerror)) {
					g_object_unref (stmt);
					g_object_unref (params);
					g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
					     _("While extracting data from store: %s"),
					     lerror && lerror->message ? lerror->message : "No error message was set");
					g_clear_error (&lerror);
					g_rec_mutex_unlock (& (priv->mutex));
					return NULL;
				}
				params_set = g_slist_prepend (params_set, h);
			}
		}

		for (list = gda_set_get_holders (params); list; list = list->next) {
			if (!g_slist_find (params_set, list->data))
				g_message (_("No value set for parameter '%s'"),
					   gda_holder_get_id (GDA_HOLDER (list->data)));
		}
		g_slist_free (params_set);
	}

	/* execution */
	model = gda_connection_statement_execute_select (priv->cnc, stmt, params, error);
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);

	g_rec_mutex_unlock (& (priv->mutex));
	return model;
}

static gboolean prepare_tables_infos (GdaMetaStore *store, TableInfo **out_table_infos,
				      TableConditionInfo **out_cond_infos, gboolean *out_with_key,
				      const gchar *table_name, const gchar *condition, GError **error,
				      gint nvalues, const gchar **value_names, const GValue **values);
static gint find_row_in_model (GdaDataModel *find_in, GdaDataModel *data, gint row,
			       gint *pk_cols, gint pk_cols_nb, gboolean *out_has_changed, GError **error);
/**
 * gda_meta_store_modify:
 * @store: a #GdaMetaStore object
 * @table_name: the name of the table to modify within @store
 * @new_data: (nullable): a #GdaDataModel containing the new data to set in @table_name, or %NULL (treated as a data model
 * with no row at all)
 * @condition: (nullable): SQL expression (which may contain variables) defining the rows which are being obsoleted by @new_data, or %NULL
 * @error: (nullable): a place to store errors, or %NULL
 * @...: a list of (variable name (gchar *), GValue *value) terminated with NULL, representing values for all the
 * variables mentioned in @condition.
 *
 * Propagates an update to @store, the update's contents is represented by @new_data, this function is
 * primarily reserved to database providers.
 *
 * For example tell @store to update its list of tables, @new_data should contain the same columns as the "_tables"
 * table of @store, and contain one row per table in the store; there should not be any more argument after the @error
 * argument.
 *
 * Now, to update only one table, the @new_data data model should have one row for the table to update (or no row
 * at all if the table does not exist anymore), and have values for the primary key of the "_tables" table of
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

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (table_name, FALSE);
	g_return_val_if_fail (!new_data || GDA_IS_DATA_MODEL (new_data), FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

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
	retval = gda_meta_store_modify_v (store, table_name, new_data, condition,
					  n_values, value_names, values, error);
	g_free (value_names);
	g_free (values);
	return retval;
}

/**
 * gda_meta_store_modify_with_context:
 * @store: a #GdaMetaStore object
 * @context: (transfer none): a #GdaMetaContext context describing what to modify in @store
 * @new_data: (transfer none) (nullable): a #GdaDataModel containing the new data to set in @table_name, or %NULL (treated as a data model
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

	g_return_val_if_fail (context, FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	for (i = 0; i < context->size; i++) {
		if (i == 0)
			cond = g_string_new ("");
		else
			g_string_append (cond, " AND ");
		g_string_append_printf (cond, "%s = ##%s::%s", context->column_names [i],
					context->column_names [i],
					g_type_name (G_VALUE_TYPE (context->column_values [i])));
	}

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		if (cond)
			g_string_free (cond, TRUE);
		return FALSE;
	}

	retval = gda_meta_store_modify_v (store, context->table_name, new_data, cond ? cond->str : NULL,
					  context->size,
					  (const gchar **) context->column_names,
					  (const GValue **)context->column_values, error);
	if (cond)
		g_string_free (cond, TRUE);
	return retval;
}

/*#define DEBUG_STORE_MODIFY*/
/**
 * gda_meta_store_modify_v: (rename-to gda_meta_store_modify)
 * @store: a #GdaMetaStore object
 * @table_name: the name of the table to modify within @store
 * @new_data: (nullable): a #GdaDataModel containing the new data to set in @table_name, or %NULL (treated as a data model
 * with no row at all)
 * @condition: (nullable): SQL expression (which may contain variables) defining the rows which are being obsoleted by @new_data, or %NULL
 * @nvalues: number of values in @value_names and @values
 * @value_names: (array length=nvalues): names of values
 * @values: (array length=nvalues): values
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Propagates an update to @store, the update's contents is represented by @new_data, this function is
 * primarily reserved to database providers.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2.6
 */
gboolean
gda_meta_store_modify_v (GdaMetaStore *store, const gchar *table_name,
			 GdaDataModel *new_data, const gchar *condition,
			 gint nvalues, const gchar **value_names, const GValue **values,
			 GError **error)
{
	TableInfo *schema_set;
	TableConditionInfo *custom_set;
	gboolean prep, with_cond;
	gboolean retval = TRUE;
	GSList *all_changes = NULL;
	gboolean started_transaction = FALSE;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	g_return_val_if_fail (priv->cnc, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (priv->cnc), FALSE);
	g_return_val_if_fail (!new_data || GDA_IS_DATA_MODEL (new_data), FALSE);

	g_rec_mutex_lock (& (priv->mutex));

	/* get the correct TableInfo */
	prep = prepare_tables_infos (store, &schema_set, &custom_set, &with_cond, table_name,
				     condition, error, nvalues, value_names, values);
	if (!prep) {
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	GdaDataModel *current = NULL;
	gboolean *rows_to_del = NULL;
	gint current_n_rows = 0, i;
	gint current_n_cols = 0;

	if (! priv->override_mode) {
		/* get current data */
		current = gda_connection_statement_execute_select_full (priv->cnc,
									with_cond ? custom_set->select :
									schema_set->current_all,
									with_cond ? custom_set->params : NULL,
									GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									schema_set->type_cols_array, error);
		if (!current) {
			g_rec_mutex_unlock (& (priv->mutex));
			return FALSE;
		}
		current_n_rows = gda_data_model_get_n_rows (current);
		current_n_cols = gda_data_model_get_n_columns (current);
		rows_to_del = g_new (gboolean, current_n_rows);
		memset (rows_to_del, TRUE, sizeof (gboolean) * current_n_rows);
#ifdef DEBUG_STORE_MODIFY
		g_print ("CURRENT:\n");
		gda_data_model_dump (current, stdout);
#endif

		/* start a transaction if possible */
		if (! gda_connection_get_transaction_status (priv->cnc)) {
			started_transaction = gda_connection_begin_transaction (priv->cnc, NULL,
										GDA_TRANSACTION_ISOLATION_UNKNOWN,
										NULL);
#ifdef DEBUG_STORE_MODIFY
			g_print ("------- BEGIN\n");
#endif
		}
	}
	else {
		/* remove everything from table */
		if (gda_connection_statement_execute_non_select (priv->cnc,
								 schema_set->delete_all, NULL,
								 NULL, error) == -1) {
			/*g_print ("may be error: %s\n", error && *error && (*error)->message ? (*error)->message : "Nope");*/
			g_rec_mutex_unlock (& (priv->mutex));
			return FALSE;
		}
	}

	/* treat rows to insert / update */
	if (new_data) {
		GdaDataModel *wrapped_data;
		gint new_n_rows, new_n_cols;

		if (schema_set->ident_cols)
			wrapped_data = _gda_data_meta_wrapper_new (new_data, !priv->override_mode,
								   schema_set->ident_cols, schema_set->ident_cols_size,
								   priv->ident_style,
								   priv->reserved_keyword_func);
		else
			wrapped_data = g_object_ref (new_data);

		new_n_rows = gda_data_model_get_n_rows (wrapped_data);
		new_n_cols = gda_data_model_get_n_columns (wrapped_data);
#ifdef DEBUG_STORE_MODIFY
		if (new_data != wrapped_data) {
			g_print ("NEW for table %s:\n", table_name);
			gda_data_model_dump (new_data, stdout);

			g_print ("wrapped as:\n");
			gda_data_model_dump (wrapped_data, stdout);
		}
#endif

		for (i = 0; i < new_n_rows; i++) {
			/* find existing row if necessary */
			gint erow = -1;
			gboolean has_changed = FALSE;
			GdaMetaStoreChange *change = NULL;

			if (!priv->override_mode) {
				if (current) {
					erow = find_row_in_model (current, wrapped_data, i,
								  schema_set->pk_cols_array,
								  schema_set->pk_cols_nb, &has_changed,
								  error);
#ifdef DEBUG_STORE_MODIFY
					g_print ("FIND row %d(/%d) returned row %d (%s)\n", i, new_n_rows - 1, erow,
						 has_changed ? "CHANGED" : "unchanged");
#endif
				}
				if (erow < -1) {
					retval = FALSE;
					g_object_unref (wrapped_data);
					goto out;
				}

				/* prepare change information */
				change = gda_meta_store_change_new ();
				gda_meta_store_change_set_table_name (change, table_name);
				all_changes = g_slist_append (all_changes, change);
			}

			/* bind parameters for new values */
			gint j;
			for (j = 0; j < new_n_cols; j++) {
				gchar *pid = g_strdup_printf ("+%d", j);
				GdaHolder *h;
				h = gda_set_get_holder (schema_set->params, pid);
				if (h) {
					const GValue *value;
					GError *lerror = NULL;
					value = gda_data_model_get_value_at (wrapped_data, j, i, error);
					if (!value) {
						g_free (pid);
						retval = FALSE;
						g_object_unref (wrapped_data);
						goto out;
					}
					/* optionaly test reported GType validity */
					if ((G_VALUE_TYPE (value) == schema_set->gtype_column) &&
					    (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
						GType gtype;
						g_assert (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_STRING));
						gtype = gda_g_type_from_string (g_value_get_string (value));
						if (g_type_is_a (gtype, G_TYPE_INVALID) || g_type_is_a (gtype, GDA_TYPE_NULL)) {
							g_warning (" Unknown reported type '%s', please report this bug to "
								   "https://gitlab.gnome.org/GNOME/libgda/issues", g_value_get_string (value));
						}
					}

					if (! gda_holder_set_value (h, value, &lerror)) {
						g_free (pid);
						retval = FALSE;
						g_object_unref (wrapped_data);
						g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_EXTRACT_SQL_ERROR,
					     _("Internal error, while updating internal meta store table '%s': Parameter value type error: %s"),
					     table_name, lerror && lerror->message ? lerror->message : "No error message was set");
						g_clear_error (&lerror);
						goto out;
					}
					if (change) {
						/*g_print ("CH %p key=[%s] value=[%s]\n", change, pid, gda_value_stringify (value)); */
						g_hash_table_insert (change->keys, pid, gda_value_copy (value));
						pid = NULL;
					}
				}
				g_free (pid);
			}

			/* execute INSERT or UPDATE statements */
			if (erow == -1) {
				/* INSERT: bind INSERT parameters */
#ifdef DEBUG_STORE_MODIFY
				g_print ("Insert new row %d into table %s\n", i, table_name);
#endif
				if (gda_connection_statement_execute_non_select (priv->cnc,
										 schema_set->insert, schema_set->params,
										 NULL, error) == -1) {
					retval = FALSE;
					g_object_unref (wrapped_data);
					goto out;
				}
				if (change)
					change->c_type = GDA_META_STORE_ADD;
			}
			else if (has_changed) {
				/* also bind parameters with existing values */
				for (j = 0; j < current_n_cols; j++) {
					gchar *pid = g_strdup_printf ("-%d", j);
					GdaHolder *h;
					h = gda_set_get_holder (schema_set->params, pid);
					if (h) {
						const GValue *value;
						value = gda_data_model_get_value_at (current, j, erow, error);
						if (!value) {
							g_free (pid);
							retval = FALSE;
							g_object_unref (wrapped_data);
							goto out;
						}
						if (!gda_holder_set_value (h, value, error)) {
							g_free (pid);
							retval = FALSE;
							g_object_unref (wrapped_data);
							goto out;
						}
						if (change) {
							/* g_print ("CH %p key=[%s] value=[%s]\n", change, pid, gda_value_stringify (value)); */
							g_hash_table_insert (change->keys, pid, gda_value_copy (value));
							pid = NULL;
						}
					}
					g_free (pid);
				}
				/* UPDATE */
#ifdef DEBUG_STORE_MODIFY
				g_print ("Update for row %d (old row was %d) into table %s\n", i, erow, table_name);
#endif
				if (gda_connection_statement_execute_non_select (priv->cnc,
										 schema_set->update, schema_set->params,
										 NULL, error) == -1) {
					retval = FALSE;
					g_object_unref (wrapped_data);
					goto out;
				}
				if (change)
					change->c_type = GDA_META_STORE_MODIFY;
				rows_to_del [erow] = FALSE;
			}
			else if (rows_to_del)
				/* row has not changed */
				rows_to_del [erow] = FALSE;

			if (!priv->override_mode) {
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

					for (k = 0; k < tfk->cols_nb; k++) {
						context.column_values [k] = (GValue*) gda_data_model_get_value_at (new_data,
									    tfk->ref_pk_cols_array[k], i, error);
						if (!context.column_values [k]) {
							g_free (context.column_values);
							retval = FALSE;
							g_object_unref (wrapped_data);
							goto out;
						}
					}
#ifdef DEBUG_STORE_MODIFY
					g_print ("Suggest update data into table '%s':", tfk->table_info->obj_name);
					for (k = 0; k < tfk->cols_nb; k++) {
						gchar *str;
						str = gda_value_stringify (context.column_values [k]);
						g_print (" [%s => %s]", context.column_names[k], str);
						g_free (str);
					}
					g_print ("\n");
#endif
					GError *suggest_reports_error = NULL;
					g_signal_emit (store, gda_meta_store_signals[SUGGEST_UPDATE], 0, &context,
						       &suggest_reports_error);
					g_free (context.column_values);
					if (suggest_reports_error) {
						retval = FALSE;
						if (error && !(*error))
							g_propagate_error (error, suggest_reports_error);
						else
							g_error_free (suggest_reports_error);
						g_object_unref (wrapped_data);
						goto out;
					}
				}
			}
		}
		g_object_unref (wrapped_data);
	}

	if (!priv->override_mode) {
		/* treat rows to delete */
		for (i = 0; i < current_n_rows; i++) {
			if (rows_to_del [i]) {
				/* prepare change information */
				GdaMetaStoreChange *change = NULL;
				change = gda_meta_store_change_new ();
				gda_meta_store_change_set_change_type (change, GDA_META_STORE_REMOVE);
				gda_meta_store_change_set_table_name (change, table_name);
				all_changes = g_slist_append (all_changes, change);

				/* DELETE */
				gint j;
				for (j = 0; j < current_n_cols; j++) {
					gchar *pid = g_strdup_printf ("-%d", j);
					GdaHolder *h;
					const GValue *value;
					value = gda_data_model_get_value_at (current, j, i, error);
					if (!value) {
						g_free (pid);
						retval = FALSE;
						goto out;
					}

					h = gda_set_get_holder (schema_set->params, pid);
					if (h && ! gda_holder_set_value (h, value, error)) {
						g_free (pid);
						retval = FALSE;
						goto out;
					}

					/* g_print ("CH %p key=[%s] value=[%s]\n", change, pid, gda_value_stringify (value)); */
					g_hash_table_insert (change->keys, pid, gda_value_copy (value));
				}
#ifdef DEBUG_STORE_MODIFY
				g_print ("Delete existing row %d from table %s\n", i, table_name);
#endif
				/* reverse_fk_list */
				GSList *list;
				for (list = schema_set->reverse_fk_list; list; list = list->next) {
					TableFKey *tfk = (TableFKey*) list->data;
					const GValue **values;
					gint k;

					/*g_print ("Also remove data from table '%s'...\n", tfk->table_info->obj_name);*/
					values = g_new (const GValue *, tfk->cols_nb);
					for (k = 0; k < tfk->cols_nb; k++) {
						values [k] = gda_data_model_get_value_at (current, tfk->ref_pk_cols_array[k], i, error);
						if (!values [k]) {
							g_free (values);
							retval = FALSE;
							goto out;
						}
					}
					if (!gda_meta_store_modify_v (store, tfk->table_info->obj_name, NULL,
								      tfk->fk_fields_cond,
								      tfk->cols_nb, (const gchar **) tfk->fk_names_array, values, error)) {
						retval = FALSE;
						g_free (values);
						goto out;
					}
					g_free (values);
				}

				if (gda_connection_statement_execute_non_select (priv->cnc,
										 schema_set->delete, schema_set->params,
										 NULL, error) == -1) {
					retval = FALSE;
					goto out;
				}
			}
		}

		if (retval && started_transaction) {
			retval = gda_connection_commit_transaction (priv->cnc, NULL, NULL);
#ifdef DEBUG_STORE_MODIFY
			g_print ("------- COMMIT\n");
#endif
		}
		if (retval && all_changes)
			g_signal_emit (store, gda_meta_store_signals[META_CHANGED], 0, all_changes);
	}

out:
	if (all_changes) {
		g_slist_free_full (all_changes, (GDestroyNotify) gda_meta_store_change_free);
	}
	g_free (rows_to_del);
	if (current)
		g_object_unref (current);
	if (!retval && started_transaction) {
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
#ifdef DEBUG_STORE_MODIFY
		g_print ("------- ROLLBACK\n");
#endif
	}

	g_rec_mutex_unlock (& (priv->mutex));
	return retval;
}


/*
 * Find the row in @find_in from the values of @data at line @row, and columns pointed by
 * the values of @pk_cols
 *
 * Returns:
 *          -2 on error,
 *          -1 if not found,
 *          >=0 if found (if changes need to be made, then @out_has_changed is set to TRUE).
 */
static gint
find_row_in_model (GdaDataModel *find_in, GdaDataModel *data, gint row, gint *pk_cols, gint pk_cols_nb,
		   gboolean *out_has_changed, GError **error)
{
	gint i, erow;
	GSList *values = NULL;

	for (i = 0; i < pk_cols_nb; i++) {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (data, pk_cols[i], row, error);
		if (!cvalue)
			return -2;
		values = g_slist_append (values, (gpointer) cvalue);
	}
	erow = gda_data_model_get_row_from_values (find_in, values, pk_cols);
	g_slist_free (values);

	if (erow >= 0) {
		gint ncols;
		ncols = gda_data_model_get_n_columns (find_in);
		if (ncols > gda_data_model_get_n_columns (data)) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
				"%s", _("Data models should have the same number of columns"));
			erow = -2;
		}
		else {
			gboolean changed = FALSE;
			for (i = 0; i < ncols; i++) {
				const GValue *v1, *v2;
				v1 = gda_data_model_get_value_at (find_in, i, erow, error);
				if (!v1)
					return -2;
				v2 = gda_data_model_get_value_at (data, i, row, error);
				if (!v2)
					return -2;
				if (gda_value_compare (v1, v2)) {
					changed = TRUE;
					break;
				}
			}
			*out_has_changed = changed;
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
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

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
		*out_cond_infos = g_hash_table_lookup (priv->table_cond_info_hash, key);
		if (! *out_cond_infos) {
			*out_cond_infos = create_custom_set (store, table_name, condition, error);
			if (! *out_cond_infos) {
				g_free (key);
				return FALSE;
			}
			g_hash_table_insert (priv->table_cond_info_hash, key, *out_cond_infos);
		}
		else
			g_free (key);
	}

	/* fetch or create *out_table_infos */
	DbObject *dbobj = g_hash_table_lookup (priv->db_objects_hash, table_name);
	if (!dbobj) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
			     _("Unknown database object '%s'"), table_name);
		return FALSE;
	}
	*out_table_infos = TABLE_INFO (dbobj);

	/* bind parameters for *out_cond_infos */
	gint i;
	for (i = 0; i < nvalues; i++) {
		GdaHolder *h;
		GValue *wvalue;

		wvalue = _gda_data_meta_wrapper_compute_value (values [i], priv->ident_style,
							       priv->reserved_keyword_func);
		h = gda_set_get_holder ((*out_cond_infos)->params, value_names [i]);
		if (!h || !gda_holder_set_value (h, wvalue ? wvalue : values[i], NULL)) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_INTERNAL_ERROR,
				_ ("Could not set value for parameter '%s'"), value_names [i]);
			if (wvalue)
				gda_value_free (wvalue);
			return FALSE;
		}
		if (wvalue)
			gda_value_free (wvalue);
	}

	return TRUE;
}

static TableConditionInfo *
create_custom_set (GdaMetaStore *store, const gchar *table_name, const gchar *condition, GError **error)
{
	gchar *sql;
	TableConditionInfo *set;
  GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	g_assert (condition);
	set = g_new0 (TableConditionInfo, 1);

	/* SELECT */
	sql = g_strdup_printf ("SELECT * FROM %s WHERE %s", table_name, condition);
	set->select = compute_prepared_stmt (priv->parser, sql);
	g_free (sql);
	if (!set->select ||
		(gda_statement_get_statement_type (set->select) != GDA_SQL_STATEMENT_SELECT)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
			"%s", _("Could not create SELECT statement"));
		goto out;
	}

	/* parameters (are the same for both statements */
	if (! gda_statement_get_parameters (set->select, &(set->params), error))
		goto out;

	/* DELETE */
	sql = g_strdup_printf ("DELETE FROM %s WHERE %s", table_name, condition);
	set->delete = compute_prepared_stmt (priv->parser, sql);
	g_free (sql);
	if (!set->delete||
		(gda_statement_get_statement_type (set->delete) != GDA_SQL_STATEMENT_DELETE)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_MODIFY_CONTENTS_ERROR,
			"%s", _("Could not create DELETE statement"));
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

/**
 * _gda_meta_store_begin_data_reset:
 * @store: a #GdaMetaStore object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Sets @store in a mode where only the modifications completely overriding a table
 * will be allowed, where no detailed modifications report is made and where the "suggest-update"
 * signal is not emitted.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
_gda_meta_store_begin_data_reset (GdaMetaStore *store, GError **error)
{
	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	if (priv->override_mode)
		return TRUE;

	g_rec_mutex_lock (& (priv->mutex));
	if (! gda_connection_get_transaction_status (priv->cnc)) {
		if (!gda_connection_begin_transaction (priv->cnc, NULL,
						       GDA_TRANSACTION_ISOLATION_UNKNOWN, error)) {
			g_rec_mutex_unlock (& (priv->mutex));
			return FALSE;
		}
	}
	else {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_TRANSACTION_ALREADY_STARTED_ERROR,
			     "%s", _("A transaction has already been started"));
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;

	}
	priv->override_mode = TRUE;
	g_rec_mutex_unlock (& (priv->mutex));
	return TRUE;
}

/**
 * _gda_meta_store_cancel_data_reset:
 * @store: a #GdaMetaStore object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Cancels any modification done since _gda_meta_store_begin_data_reset() was called.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
_gda_meta_store_cancel_data_reset (GdaMetaStore *store, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	g_rec_mutex_lock (& (priv->mutex));
	if (!priv->override_mode) {
		g_rec_mutex_unlock (& (priv->mutex));
		return TRUE;
	}

	priv->override_mode = FALSE;
	retval = gda_connection_rollback_transaction (priv->cnc, NULL, error);
	g_rec_mutex_unlock (& (priv->mutex));
	return retval;
}

/**
 * _gda_meta_store_finish_data_reset:
 * @store: a #GdaMetaStore object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Commits any modification done since _gda_meta_store_begin_data_reset() was called.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
_gda_meta_store_finish_data_reset (GdaMetaStore *store, GError **error)
{
	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	g_rec_mutex_lock (& (priv->mutex));
	if (!priv->override_mode) {
		g_rec_mutex_unlock (& (priv->mutex));
		return TRUE;
	}

	priv->override_mode = FALSE;
	if (!gda_connection_commit_transaction (priv->cnc, NULL, error)) {
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}
	else {
		g_signal_emit (store, gda_meta_store_signals[META_RESET], 0);
		g_rec_mutex_unlock (& (priv->mutex));
		return TRUE;
	}
}



/**
 * gda_meta_store_create_modify_data_model:
 * @store: a #GdaMetaStore object
 * @table_name: the name of a table present in @store
 *
 * Creates a new #GdaDataModelArray data model which can be used, after being correctly filled,
 * with the gda_meta_store_modify*() methods.*
 *
 * To be used by provider's implementation
 *
 * Returns: (transfer full): a new #GdaDataModel
 */
GdaDataModel *
gda_meta_store_create_modify_data_model (GdaMetaStore *store, const gchar *table_name)
{
	DbObject *dbobj;
	TableInfo *tinfo;
	GdaDataModel *model;
	GSList *list;
	gint i;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	g_rec_mutex_lock (& (priv->mutex));

	dbobj = g_hash_table_lookup (priv->p_db_objects_hash, table_name);
	if (!dbobj) {
		g_warning ("Table '%s' is not known by the GdaMetaStore", table_name);
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}
	if (dbobj->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE) {
		g_warning ("Table '%s' is not a database table in the GdaMetaStore", table_name);
		g_rec_mutex_unlock (& (priv->mutex));
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

	g_rec_mutex_unlock (& (priv->mutex));
	return model;
}

/**
 * gda_meta_store_schema_get_all_tables:
 * @store: a #GdaMetaStore object
 *
 * Get an ordered list of the tables @store knows about. The tables are ordered in a way that tables dependencies
 * are respected: if table B has a foreign key on table A, then table A will be listed before table B in the returned
 * list.
 *
 * Returns: (transfer container) (element-type utf8): a new list of tables names (as gchar*), the list must be freed when no longer needed, but the strings present in the list must not be modified.
 */
GSList *
gda_meta_store_schema_get_all_tables (GdaMetaStore *store)
{
	GSList *list, *ret;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	g_rec_mutex_lock (& (priv->mutex));

	for (ret = NULL, list = priv->db_objects; list; list = list->next) {
		DbObject *dbobj = DB_OBJECT (list->data);
		if (dbobj->obj_type == GDA_SERVER_OPERATION_CREATE_TABLE)
			ret = g_slist_prepend (ret, dbobj->obj_name);
	}
	for (ret = NULL, list = priv->p_db_objects; list; list = list->next) {
		DbObject *dbobj = DB_OBJECT (list->data);
		if (dbobj->obj_type == GDA_SERVER_OPERATION_CREATE_TABLE)
			ret = g_slist_prepend (ret, dbobj->obj_name);
	}

	g_rec_mutex_unlock (& (priv->mutex));

	return g_slist_reverse (ret);
}

/**
 * gda_meta_store_schema_get_depend_tables:
 * @store: a #GdaMetaStore object
 * @table_name: the name of the table for which all the dependencies must be listed
 *
 *
 * Get an ordered list of the tables @store knows about on which the @table_name table depends (recursively).
 * The tables are ordered in a way that tables dependencies
 * are respected: if table B has a foreign key on table A, then table A will be listed before table B in the returned
 * list.
 *
 * Returns: (transfer container) (element-type utf8): a new list of tables names (as gchar*), the list must be freed when no longer needed, but the strings present in the list must not be modified.
 */
GSList *
gda_meta_store_schema_get_depend_tables (GdaMetaStore *store, const gchar *table_name)
{
	GSList *list, *ret;
	DbObject *dbo;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (table_name && *table_name, NULL);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	g_rec_mutex_lock (& (priv->mutex));
	dbo = g_hash_table_lookup (priv->p_db_objects_hash, table_name);
	if (!dbo) {
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	for (ret = NULL, list = dbo->depend_list; list; list = list->next) {
		DbObject *dbobj = DB_OBJECT (list->data);
		if (dbobj->obj_type == GDA_SERVER_OPERATION_CREATE_TABLE)
			ret = g_slist_prepend (ret, dbobj->obj_name);
	}

	g_rec_mutex_unlock (& (priv->mutex));

	return g_slist_reverse (ret);
}


/**
 * gda_meta_store_schema_get_structure:
 * @store: a #GdaMetaStore object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Creates a new #GdaMetaStruct object representing @store's internal database structure.
 *
 * Returns: (transfer full): a new #GdaMetaStruct object, or %NULL if an error occurred
 */
GdaMetaStruct *
gda_meta_store_schema_get_structure (GdaMetaStore *store, GError **error)
{
	GdaMetaStruct *mstruct;
	GdaDataModel *model;
	gint i, nrows;
	GdaMetaStore *real_store;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);
	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return NULL;
	}

	g_rec_mutex_lock (& (priv->mutex));

	/* make sure the private connection's meta store is up to date */
	if (! gda_connection_update_meta_store (priv->cnc, NULL, error)) {
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	/* create a GdaMetaStruct */
	real_store = gda_connection_get_meta_store (priv->cnc);
	GdaMetaStorePrivate *real_priv = gda_meta_store_get_instance_private (real_store);
	model = gda_meta_store_extract (real_store,
					"SELECT table_catalog, table_schema, table_name FROM _tables",
					error, NULL);
	if (!model) {
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", real_store, "features", GDA_META_STRUCT_FEATURE_ALL, NULL);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		/* FIXME: only take into account the database objects which have a corresponding DbObject */
		const GValue *cv0, *cv1, *cv2;
		cv0 = gda_data_model_get_value_at (model, 0, i, error);
		if (!cv0) {
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
		cv1 = gda_data_model_get_value_at (model, 1, i, error);
		if (!cv1) {
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
		cv2 = gda_data_model_get_value_at (model, 2, i, error);
		if (!cv2) {
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
		if (!gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, cv0, cv1, cv2, error)) {
			g_object_unref (mstruct);
			g_object_unref (model);
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
	}
	g_object_unref (model);

	/* complement the meta struct with some info about dependencies */
	GSList *list, *all_db_obj_list;

	all_db_obj_list = g_slist_copy (priv->db_objects);
	if (real_priv->p_db_objects)
		all_db_obj_list = g_slist_concat (all_db_obj_list,
						  g_slist_copy (real_priv->p_db_objects));

	for (list = all_db_obj_list; list; list = list->next) {
		DbObject *dbobj = DB_OBJECT (list->data);
		if (dbobj->obj_type == GDA_SERVER_OPERATION_CREATE_TABLE) {
			GdaMetaDbObject *mdbo;
			GSList *dep_list;
			GValue *value;

			g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), dbobj->obj_name);
			mdbo = gda_meta_struct_get_db_object (mstruct, NULL, NULL, value);
			gda_value_free (value);
			if (!mdbo)
				continue;
			for (dep_list = dbobj->depend_list; dep_list; dep_list = dep_list->next) {
				GdaMetaDbObject *dep_mdbo;
				g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), DB_OBJECT (dep_list->data)->obj_name);
				dep_mdbo = gda_meta_struct_get_db_object (mstruct, NULL, NULL, value);
				gda_value_free (value);
				if (dep_mdbo && !g_slist_find (mdbo->depend_list, dep_mdbo)) {
					/* FIXME: dependencies are added using the mdbo->depend_list list, and not as
					 * real foreign key */
					mdbo->depend_list = g_slist_append (mdbo->depend_list, dep_mdbo);
				}
			}
		}
	}
	g_slist_free (all_db_obj_list);

	g_rec_mutex_unlock (& (priv->mutex));

	return mstruct;
}

/**
 * gda_meta_store_get_attribute_value:
 * @store: a #GdaMetaStore object
 * @att_name: name of the attribute to get
 * @att_value: (out): the place to store the attribute value
 * @error: (nullable): a place to store errors, or %NULL
 *
 * The #GdaMetaStore object maintains a list of (name,value) attributes (attributes names starting with a '_'
 * character are for internal use only and cannot be altered). This method and the gda_meta_store_set_attribute_value()
 * method allows the user to add, set or remove attributes specific to their usage.
 *
 * This method allows to get the value of a attribute stored in @store. The returned attribute value is
 * placed at @att_value, the caller is responsible for free that string.
 *
 * If there is no attribute named @att_name then @att_value is set to %NULL
 * and @error will contain the GDA_META_STORE_ATTRIBUTE_NOT_FOUND_ERROR error code, and FALSE is returned.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_store_get_attribute_value (GdaMetaStore *store, const gchar *att_name, gchar **att_value, GError **error)
{
	GdaDataModel *model;
	GValue *value;
	gint nrows;
	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (att_name && *att_name, FALSE);
	g_return_val_if_fail (att_value, FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	g_rec_mutex_lock (& (priv->mutex));

	*att_value = NULL;
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), att_name);
	model = gda_meta_store_extract (store, "SELECT att_value FROM _attributes WHERE att_name = ##n::string", error,
					"n", value, NULL);
	gda_value_free (value);
	if (!model) {
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}
	nrows = gda_data_model_get_n_rows (model);
	if (nrows < 1)
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_ATTRIBUTE_NOT_FOUND_ERROR,
			     _("Attribute '%s' not found"), att_name);
	else if (nrows > 1)
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_ATTRIBUTE_ERROR,
			     ngettext ("Attribute '%s' has %d value", "Attribute '%s' has %d values", nrows),
			     att_name, nrows);
	else {
		value = (GValue*) gda_data_model_get_value_at (model, 0, 0, error);
		if (!value) {
			g_rec_mutex_unlock (& (priv->mutex));
			return FALSE;
		}
		if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
			const gchar *val;
			val = g_value_get_string (value);
			if (val)
				*att_value = g_strdup (val);
		}
		g_rec_mutex_unlock (& (priv->mutex));
		return TRUE;
	}
	g_rec_mutex_unlock (& (priv->mutex));
	return FALSE;
}

/**
 * gda_meta_store_set_attribute_value:
 * @store: a #GdaMetaStore object
 * @att_name: name of the attribute to set
 * @att_value: (nullable): value of the attribute to set, or %NULL to unset the attribute
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Set the value of the attribute named @att_name to @att_value; see gda_meta_store_get_attribute_value() for
 * more information.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_store_set_attribute_value (GdaMetaStore *store, const gchar *att_name,
				    const gchar *att_value, GError **error)
{
	gboolean started_transaction = FALSE;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (att_name && *att_name, FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (*att_name == '_') {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_ATTRIBUTE_ERROR,
			     "%s", _("Attributes names starting with a '_' are reserved for internal usage"));
		return FALSE;
	}

	g_rec_mutex_lock (& (priv->mutex));

	if (!priv->attributes_set) {
		if (!gda_statement_get_parameters (priv->prep_stmts [STMT_SET_ATT_VALUE], &priv->attributes_set, error)) {
			g_rec_mutex_unlock (& (priv->mutex));
			return FALSE;
		}
	}

	if (!gda_set_set_holder_value (priv->attributes_set, error, "name", att_name)) {
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	/* start a transaction if possible */
	gda_lockable_lock (GDA_LOCKABLE (priv->cnc));
	gda_connection_increase_usage (priv->cnc); /* USAGE ++ */
	if (! gda_connection_get_transaction_status (priv->cnc))
		started_transaction = gda_connection_begin_transaction (priv->cnc, NULL,
									GDA_TRANSACTION_ISOLATION_UNKNOWN,
									NULL);
	else
		g_warning (_("Could not start a transaction because one already started, this could lead to GdaMetaStore "
			     "attributes problems"));

	/* delete existing attribute */
	if (gda_connection_statement_execute_non_select (priv->cnc,
							 priv->prep_stmts [STMT_DEL_ATT_VALUE], priv->attributes_set,
							 NULL, error) == -1)
		goto onerror;

	if (att_value) {
		/* set new attribute */
		if (!gda_set_set_holder_value (priv->attributes_set, error, "value", att_value))
			goto onerror;

		if (gda_connection_statement_execute_non_select (priv->cnc,
								 priv->prep_stmts [STMT_SET_ATT_VALUE], priv->attributes_set,
								 NULL, error) == -1)
			goto onerror;
	}
	if (started_transaction)
		gda_connection_commit_transaction (priv->cnc, NULL, NULL);
	gda_connection_decrease_usage (priv->cnc); /* USAGE -- */
	gda_lockable_unlock (GDA_LOCKABLE (priv->cnc));
	g_rec_mutex_unlock (& (priv->mutex));
	return TRUE;

 onerror:
	if (started_transaction)
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
	gda_connection_decrease_usage (priv->cnc); /* USAGE -- */
	gda_lockable_unlock (GDA_LOCKABLE (priv->cnc));
	g_rec_mutex_unlock (& (priv->mutex));
	return FALSE;
}

/**
 * gda_meta_store_schema_add_custom_object:
 * @store: a #GdaMetaStore object
 * @xml_description: an XML description of the table or view to add to @store
 * @error: (nullable): a place to store errors, or %NULL
 *
 * The internal database used by @store can be 'augmented' with some user-defined database objects
 * (such as tables or views). This method allows one to add a new database object.
 *
 * If the internal database already contains the object, then:
 * <itemizedlist>
 *   <listitem><para>if the object is equal to the provided description then TRUE is returned</para></listitem>
 *   <listitem><para>if the object exists but differs from the provided description, then FALSE is returned,
 *      with the GDA_META_STORE_SCHEMA_OBJECT_CONFLICT_ERROR error code</para></listitem>
 * </itemizedlist>
 *
 * The @xml_description defines the table of view's definition, for example:
 * <programlisting><![CDATA[<table name="mytable">
    <column name="id" pkey="TRUE"/>
    <column name="value"/>
</table>]]></programlisting>
 *
 * The partial DTD for this XML description of the object to add is the following (the top node must be
 * a &lt;table&gt; or a &lt;view&gt;):
 * <programlisting><![CDATA[<!ELEMENT table (column*,check*,fkey*)>
<!ATTLIST table
          name NMTOKEN #REQUIRED>

<!ELEMENT column EMPTY>
<!ATTLIST column
          name NMTOKEN #REQUIRED
          type CDATA #IMPLIED
          pkey (TRUE|FALSE) #IMPLIED
          autoinc (TRUE|FALSE) #IMPLIED
          nullok (TRUE|FALSE) #IMPLIED>

<!ELEMENT check (#PCDATA)>

<!ELEMENT fkey (part+)>
<!ATTLIST fkey
          ref_table NMTOKEN #REQUIRED>

<!ELEMENT part EMPTY>
<!ATTLIST part
          column NMTOKEN #IMPLIED
          ref_column NMTOKEN #IMPLIED>

<!ELEMENT view (definition)>
<!ATTLIST view
          name NMTOKEN #REQUIRED
          descr CDATA #IMPLIED>

<!ELEMENT definition (#PCDATA)>]]></programlisting>
 *
 * Returns: TRUE if the new object has successfully been added
 */
gboolean
gda_meta_store_schema_add_custom_object (GdaMetaStore *store, const gchar *xml_description, GError **error)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	DbObject *dbo = NULL;
	GValue *value;
	GdaMetaStore *pstore = NULL;
	GdaMetaStruct *mstruct = NULL;
	GError *lerror = NULL;

	GSList *pre_p_db_objects = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (xml_description && *xml_description, FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	/* load XML description */
	doc = xmlParseDoc (BAD_CAST xml_description);
	if (!doc) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
			     "%s", _("Could not parse XML description of custom database object to add"));
		return FALSE;
	}
	node = xmlDocGetRootElement (doc);

	g_rec_mutex_lock (& (priv->mutex));


  /* check that object name does not start with '_' */
	xmlChar *prop;
	prop = xmlGetProp (node, BAD_CAST "name");
	if (!prop) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
			     "%s", _("Missing custom database object name"));
		goto onerror;
	}
	else if (*prop == '_') {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
			     "%s", _("Custom database object names starting with a '_' are reserved for internal usage"));
		goto onerror;
	}

	/* keep a list of custom DB objects _before_ adding the new one(s) (more than
	 * one if there are dependencies) */
	pre_p_db_objects = g_slist_copy (priv->p_db_objects);

	/* create DbObject structure from XML description, stored in @store's custom db objects */
	if (!strcmp ((gchar *) node->name, "table"))
		dbo = create_table_object (priv, store, node, error);
	else if (!strcmp ((gchar *) node->name, "view"))
		dbo = create_view_object (priv, store, node, error);

	if (!dbo)
		goto onerror;
	xmlFreeDoc (doc);
	doc = NULL;

	/* check for an already existing database object with the same name */
	/*g_print ("Obj name: %s\n", dbo->obj_name);*/

	/* make sure the private connection's meta store is up to date about the requested object */
	switch (dbo->obj_type) {
	case GDA_SERVER_OPERATION_CREATE_TABLE:
	case GDA_SERVER_OPERATION_CREATE_VIEW: {
		GdaMetaContext context;
		gboolean upd_ok;
		memset (&context, 0, sizeof (GdaMetaContext));
		context.table_name = "_tables";
		context.size = 1;
		context.column_names = g_new0 (gchar *, 3);
		context.column_values = g_new0 (GValue *, 3);
		context.column_names[0] = "table_name";
		g_value_set_string ((context.column_values[0] = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
		upd_ok = gda_connection_update_meta_store (priv->cnc, &context, error);
		g_free (context.column_names);
		gda_value_free (context.column_values[0]);
		g_free (context.column_values);
		if (!upd_ok)
			goto onerror;
		break;
	}
	default:
		TO_IMPLEMENT;
	}

	/* create a GdaMetaStruct */
	GdaMetaDbObject *eobj;
	gboolean needs_creation = TRUE;
	pstore = gda_connection_get_meta_store (priv->cnc);
	mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", pstore, "features", GDA_META_STRUCT_FEATURE_ALL, NULL);
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
	if (!(eobj = gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN,
						 NULL, NULL, value, &lerror))) {
		if (lerror && (lerror->domain == GDA_META_STRUCT_ERROR) &&
		    (lerror->code == GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR))
			g_error_free (lerror);
		else {
			g_propagate_error (error, lerror);
			goto onerror;
		}
	}
	gda_value_free (value);

	if (eobj) {
		gboolean conflict = FALSE;

		/*g_print ("Check Existing object's conformance...\n");*/
		switch (eobj->obj_type) {
		case GDA_META_DB_TABLE:
			if (dbo->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE)
				conflict = TRUE;
			else {
				GdaMetaTable *mt = GDA_META_TABLE (eobj);
				TableInfo *ti = TABLE_INFO (dbo);
				if (g_slist_length (mt->columns) != g_slist_length (ti->columns))
					conflict = TRUE;
			}
			break;
		case GDA_META_DB_VIEW:
			if (dbo->obj_type != GDA_SERVER_OPERATION_CREATE_VIEW)
				conflict = TRUE;
			else {
				GdaMetaView *mv = GDA_META_VIEW (eobj);
				ViewInfo *vi = VIEW_INFO (dbo);
				if (!mv->view_def ||
				    !vi->view_def ||
				    strcmp (mv->view_def, vi->view_def))
					conflict = TRUE;
			}
			break;
		default:
			TO_IMPLEMENT;
		}

		if (conflict) {
			g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_CONFLICT_ERROR,
				     "%s", _("Another object with the same name already exists"));
			goto onerror;
		}
		needs_creation = FALSE;
	}
	g_object_unref (mstruct);
	mstruct = NULL;

	if (needs_creation) {
		/* prepare the create operation */
		GdaServerProvider *prov;
		prov = gda_connection_get_provider (priv->cnc);
		if (! prepare_dbo_server_operation (priv, store, prov, dbo, error))
			goto onerror;

		/* actually create the object in database */
		/*g_print ("Creating object: %s\n", dbo->obj_name);*/
		if (dbo->create_op) {
			if (!gda_server_provider_perform_operation (prov, priv->cnc, dbo->create_op, error))
				goto onerror;
			g_object_unref (dbo->create_op);
			dbo->create_op = NULL;
		}
	}

	g_rec_mutex_unlock (& (priv->mutex));
	return TRUE;

 onerror:
	if (doc)
		xmlFreeDoc (doc);
	if (dbo) {
		GSList *current_objects, *list;
		current_objects = g_slist_copy (priv->p_db_objects);
		for (list = current_objects; list; list = list->next) {
			dbo = DB_OBJECT (list->data);
			if (!g_slist_find (pre_p_db_objects, dbo)) {
				/* remove the DbObject */
				priv->p_db_objects = g_slist_remove (priv->p_db_objects, dbo);
				g_hash_table_remove (priv->p_db_objects_hash, dbo->obj_name);
				db_object_free (dbo);
			}
		}
		g_slist_free (current_objects);
	}
	g_rec_mutex_unlock (& (priv->mutex));
	g_slist_free (pre_p_db_objects);
	if (pstore)
		g_object_unref (pstore);
	if (mstruct)
		g_object_unref (mstruct);

	return FALSE;
}

/**
 * gda_meta_store_schema_remove_custom_object:
 * @store: a #GdaMetaStore object
 * @obj_name: name of the custom object to remove
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Removes the custom database object named @obj_name.
 *
 * Returns: TRUE if the custom object has successfully been removed
 */
gboolean
gda_meta_store_schema_remove_custom_object (GdaMetaStore *store, const gchar *obj_name, GError **error)
{
	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (obj_name && *obj_name, FALSE);
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	g_rec_mutex_lock (& (priv->mutex));
	TO_IMPLEMENT;
	g_rec_mutex_unlock (& (priv->mutex));

	return FALSE;
}

/*
 * Makes sure @context is well formed, and call gda_sql_identifier_prepare_for_compare() on SQL
 * identifiers's values
 *
 * Returns: a new #GdaMetaContext
 */
GdaMetaContext *
_gda_meta_store_validate_context (GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (context != NULL, NULL);

	gint i;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return FALSE;
	}

	if (!context->table_name || !(*context->table_name)) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
			     "%s", _("Missing table name in meta data context"));
		return NULL;
	}

	/* handle quoted SQL identifiers */
	DbObject *dbobj = g_hash_table_lookup (priv->db_objects_hash, context->table_name);
	if (dbobj && (dbobj->obj_type == GDA_SERVER_OPERATION_CREATE_TABLE)) {
		GdaMetaContext *lcontext;
		TableInfo *tinfo;

		lcontext = g_new0 (GdaMetaContext, 1);
		lcontext->table_name = context->table_name;
		lcontext->size = context->size;
		if (lcontext->size > 0) {
			lcontext->column_names = g_new0 (gchar*, lcontext->size);
			lcontext->column_values = g_new0 (GValue*, lcontext->size);
		}

		tinfo = TABLE_INFO (dbobj);
		for (i = 0; i < lcontext->size; i++) {
			GSList *list;
			gint colindex;

			if (!context->column_names [i]) {
				g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
					     "%s", _("Missing column name in meta data context"));
				goto onerror;
			}
			lcontext->column_names [i] = g_strdup (context->column_names [i]);

			for (colindex = 0, list = tinfo->columns;
			     list;
			     colindex++, list = list->next) {
				if (!strcmp (TABLE_COLUMN (list->data)->column_name, lcontext->column_names [i])) {
					gint j;
					for (j = 0; j < tinfo->ident_cols_size; j++) {
						if (tinfo->ident_cols [j] == colindex) {
							/* we have an SQL identifier */
							if (! context->column_values [i]) {
								g_set_error (error, GDA_META_STORE_ERROR,
									     GDA_META_STORE_META_CONTEXT_ERROR,
									     _("Missing condition in meta data context"));
								goto onerror;
							}
							else if (G_VALUE_TYPE (context->column_values [i]) == G_TYPE_STRING) {
								gchar *id;
								id = g_value_dup_string (context->column_values [i]);
								gda_sql_identifier_prepare_for_compare (id);
								if (priv->ident_style == GDA_SQL_IDENTIFIERS_UPPER_CASE) {
									/* move to upper case */
									gchar *ptr;
									for (ptr = id; *ptr; ptr++) {
										if ((*ptr >= 'a') && (*ptr <= 'z'))
											*ptr += 'A' - 'a';
									}
								}
								lcontext->column_values [i] = gda_value_new (G_TYPE_STRING);
								g_value_take_string (lcontext->column_values [i], id);

							}
							else if (G_VALUE_TYPE (context->column_values [i]) == GDA_TYPE_NULL) {
								lcontext->column_values [i] = gda_value_new_null ();
							}
							else {
								g_set_error (error, GDA_META_STORE_ERROR,
									     GDA_META_STORE_META_CONTEXT_ERROR,
									     _("Malformed condition in meta data context"));
								goto onerror;
							}
						}
					}

					if (! lcontext->column_values [i]) {
						lcontext->column_values [i] = gda_value_copy (context->column_values [i]);
					}
					colindex = -1;
					break;
				}
			}

			if (colindex != -1) {
				/* column not found => error */
				g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
					     _("Unknown column name '%s' in meta data context"),
					     lcontext->column_names [i]);
				goto onerror;
			}
			else
				continue;

		onerror:
			for (i = 0; i < lcontext->size; i++) {
				g_free (lcontext->column_names [i]);
				if (lcontext->column_values [i])
					gda_value_free (lcontext->column_values [i]);
			}
			g_free (lcontext->column_names);
			g_free (lcontext->column_values);
			g_free (lcontext);
			return NULL;
		}
		return lcontext;
	}
	else {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_META_CONTEXT_ERROR,
			     "%s", _("Unknown table in meta data context"));
		return NULL;
	}
}

/*
 * Returns: a list of new #GdaMetaContext structures, one for each dependency context->table_name has,
 * or %NULL if there is no downstream context, or if an error occurred (check @error to make the difference).
 *
 * WARNING: each new GdaMetaContext structure is allocated, but:
 *    - the @table_name argument is not copied
 *    - if @size > 0 then @column_names and @column_values are allocated, but their contents is not!
 */
GSList *
_gda_meta_store_schema_get_upstream_contexts (GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	DbObject *dbo;
	GSList *list, *retlist = NULL;
	TableInfo *tinfo;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return NULL;
	}

	g_rec_mutex_lock (& (priv->mutex));

	/* find the associated DbObject */
	dbo = g_hash_table_lookup (priv->p_db_objects_hash, context->table_name);
	if (!dbo) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
			     _("Unknown database object '%s'"), context->table_name);
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}
	if (dbo->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE) {
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	tinfo = TABLE_INFO (dbo);
	if (!tinfo->fk_list) {
		/* this is not an error, just that there are no dependency */
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	/* Identify the TableFKey if @context permits it */
	if (context->size > 0) {
		for (list = tinfo->fk_list; list; list = list->next) {
			TableFKey *tfk = (TableFKey*) list->data;
			gint i, j, partial_parts = 0;
			gint *cols_array;

			cols_array = g_new (gint, tfk->cols_nb);
			for (i = 0; i < tfk->cols_nb; i++) {
				cols_array [i] = -1;
				for (j = 0; j < context->size; j++) {
					if (!strcmp (tfk->fk_names_array[i], context->column_names[j])) {
						cols_array [i] = j;
						partial_parts++;
						break;
					}
				}
			}
			if (partial_parts > 0) {
				GdaMetaContext *ct;
				ct = g_new0 (GdaMetaContext, 1);
				ct->table_name = tfk->depend_on->obj_name;
				ct->size = partial_parts;
				ct->column_names = g_new0 (gchar *, ct->size);
				ct->column_values = g_new0 (GValue *, ct->size);
				retlist = g_slist_prepend (retlist, ct);
				for (j = 0, i = 0; i < tfk->cols_nb; i++) {
					if (cols_array [i] >= 0) {
						ct->column_names [j] = tfk->ref_pk_names_array [i];
						ct->column_values [j] = context->column_values [cols_array [i]];
						j++;
					}
				}
				g_free (cols_array);
				break;
			}
			else {
				GdaMetaContext *ct;
				ct = g_new0 (GdaMetaContext, 1);
				ct->table_name = tfk->depend_on->obj_name;
				ct->size = 0;
				retlist = g_slist_prepend (retlist, ct);
			}
			g_free (cols_array);
		}
	}
	else {
		for (list = tinfo->fk_list; list; list = list->next) {
			TableFKey *tfk = (TableFKey*) list->data;
			GdaMetaContext *ct;
			ct = g_new0 (GdaMetaContext, 1);
			ct->table_name = tfk->depend_on->obj_name;
			ct->size = 0;
			retlist = g_slist_prepend (retlist, ct);
		}
	}

	g_rec_mutex_unlock (& (priv->mutex));
	return g_slist_reverse (retlist);
}

/*
 * Returns: a list of new #GdaMetaContext structures, one for each reverse dependency context->table_name has,
 * or %NULL if there is no downstream context, or if an error occurred (check @error to make the difference).
 *
 * WARNING: each new GdaMetaContext structure is allocated, but:
 *    - the @table_name argument is not copied
 *    - if @size > 0 then @column_names and @column_values are allocated, but their contents is not!
 */
GSList *
_gda_meta_store_schema_get_downstream_contexts (GdaMetaStore *store, GdaMetaContext *context, GError **error)
{
	DbObject *dbo;
	GSList *list, *retlist = NULL;
	TableInfo *tinfo;
	GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (priv->init_error) {
		g_propagate_error (error, g_error_copy (priv->init_error));
		return NULL;
	}

	g_rec_mutex_lock (& (priv->mutex));

	/* find the associated DbObject */
	dbo = g_hash_table_lookup (priv->p_db_objects_hash, context->table_name);
	if (!dbo) {
		g_set_error (error, GDA_META_STORE_ERROR, GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
			     _("Unknown database object '%s'"), context->table_name);
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}
	if (dbo->obj_type != GDA_SERVER_OPERATION_CREATE_TABLE) {
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	tinfo = TABLE_INFO (dbo);
	if (!tinfo->reverse_fk_list) {
		/* this is not an error, just that there are no dependency */
		g_rec_mutex_unlock (& (priv->mutex));
		return NULL;
	}

	for (list = tinfo->reverse_fk_list; list; list = list->next) {
		TableFKey *tfk = (TableFKey*) list->data;
		GdaMetaContext *ct;

		/* REM: there may be duplicates, but we don't really care here (it'd take more resources to get rid of
		* them than it takes to put duplicates in a hash table */
		ct = g_new0 (GdaMetaContext, 1);
		ct->table_name = tfk->table_info->obj_name;
		ct->size = 0;
		retlist = g_slist_prepend (retlist, ct);
	}

	g_rec_mutex_unlock (& (priv->mutex));
	return g_slist_reverse (retlist);
}

/*
 * arguments_to_name
 *
 * Returns: a new string
 */
static gchar *
arguments_to_name (const gchar *catalog, const gchar *schema, const gchar *table)
{
	g_assert (table);
	gchar *str;
	if (catalog) {
		if (schema)
			str = g_strdup_printf ("%s.%s.%s", catalog, schema, table);
		else
			str = g_strdup (table); /* should not happen */
	}
	else if (schema)
		str = g_strdup_printf ("%s.%s", schema, table);
	else
		str = g_strdup (table);
	return str;
}

/**
 * gda_meta_store_declare_foreign_key:
 * @store: a #GdaMetaStore
 * @mstruct: (nullable): a #GdaMetaStruct, or %NULL
 * @fk_name: the name of the foreign key to declare
 * @catalog: (nullable): the catalog in which the table (for which the foreign key is for) is, or %NULL
 * @schema: (nullable): the schema in which the table (for which the foreign key is for) is, or %NULL
 * @table: the name of the table (for which the foreign key is for)
 * @ref_catalog: (nullable): the catalog in which the referenced table is, or %NULL
 * @ref_schema: (nullable): the schema in which the referenced table is, or %NULL
 * @ref_table: the name of the referenced table
 * @nb_cols: the number of columns involved (>0)
 * @colnames: (array length=nb_cols): an array of column names from the table for which the foreign key is for
 * @ref_colnames: (array length=nb_cols): an array of column names from the referenced table
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Defines a new declared foreign key into @store. If another declared foreign key is already defined
 * between the two tables and with the same name, then it is first removed.
 *
 * This method begins a transaction if possible (ie. none is already started), and if it can't,
 * then if there is an error, the job may be partially done.
 *
 * A check is always performed to make sure all the database objects actually
 * exist and returns an error if not. The check is performed using @mstruct if it's not %NULL (in
 * this case only the tables already represented in @mstruct will be considered, in other words: @mstruct
 * will not be modified), and using an internal #GdaMetaStruct is %NULL.
 *
 * The @catalog, @schema, @table, @ref_catalog, @ref_schema and @ref_table must follow the SQL
 * identifiers naming convention, see the <link linkend="gen:sql_identifiers">SQL identifiers</link>
 * section. The same convention needs to be respected for the strings in @conames and @ref_colnames.
 *
 * If @catalog is not %NULL, then @schema must also be not %NULL (the same restriction applies to
 * @ref_catalog and @ref_schema).
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2.4
 */
gboolean
gda_meta_store_declare_foreign_key (GdaMetaStore *store, GdaMetaStruct *mstruct,
				    const gchar *fk_name,
				    const gchar *catalog, const gchar *schema, const gchar *table,
				    const gchar *ref_catalog, const gchar *ref_schema, const gchar *ref_table,
				    guint nb_cols,
				    gchar **colnames, gchar **ref_colnames,
				    GError **error)
{
 	gboolean retval = FALSE;
	GdaSet *params = NULL;
	GdaMetaTable *mtable = NULL, *ref_mtable = NULL;
	GdaMetaDbObject *dbo = NULL, *ref_dbo = NULL;
	GdaMetaStruct *u_mstruct = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), FALSE);
	g_return_val_if_fail (fk_name, FALSE);
	g_return_val_if_fail (!catalog || (catalog && schema), FALSE);
	g_return_val_if_fail (!ref_catalog || (ref_catalog && ref_schema), FALSE);
	g_return_val_if_fail (table, FALSE);
	g_return_val_if_fail (ref_table, FALSE);
	g_return_val_if_fail (nb_cols > 0, FALSE);
	g_return_val_if_fail (colnames, FALSE);
	g_return_val_if_fail (ref_colnames, FALSE);

  GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (!mstruct)
		u_mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", store, "features", GDA_META_STRUCT_FEATURE_NONE, NULL);

	/* find database objects */
	GValue *v1 = NULL, *v2 = NULL, *v3;

	if (catalog)
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), catalog);
	if (schema)
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), schema);
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), table);
	if (mstruct) {
		dbo = gda_meta_struct_get_db_object (mstruct, v1, v2, v3);
		if (!dbo || (dbo->obj_type != GDA_META_DB_TABLE)) {
			gchar *tmp;
			tmp = arguments_to_name (catalog, schema, table);
			g_set_error (error, GDA_META_STRUCT_ERROR,
				     GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Could not find object named '%s'"), tmp);
			g_free (tmp);
			dbo = NULL;
		}
	}
	else
		dbo = gda_meta_struct_complement (u_mstruct, GDA_META_DB_TABLE, v1, v2, v3, error);
	if (v1)
		gda_value_free (v1);
	if (v2)
		gda_value_free (v2);
	gda_value_free (v3);
	if (! dbo)
		goto out;
	mtable = GDA_META_TABLE (dbo);

	v1 = NULL;
	v2 = NULL;
	if (ref_catalog)
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), ref_catalog);
	if (ref_schema)
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), ref_schema);
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), ref_table);
	if (mstruct) {
		ref_dbo = gda_meta_struct_get_db_object (mstruct, v1, v2, v3);
		if (!ref_dbo || (ref_dbo->obj_type != GDA_META_DB_TABLE)) {
			gchar *tmp;
			tmp = arguments_to_name (ref_catalog, ref_schema, ref_table);
			g_set_error (error, GDA_META_STRUCT_ERROR,
				     GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Could not find object named '%s'"), tmp);
			g_free (tmp);
			ref_dbo = NULL;
		}
	}
	else
		ref_dbo = gda_meta_struct_complement (u_mstruct, GDA_META_DB_TABLE, v1, v2, v3, error);
	if (v1)
		gda_value_free (v1);
	if (v2)
		gda_value_free (v2);
	gda_value_free (v3);
	if (! ref_dbo)
		goto out;
	ref_mtable = GDA_META_TABLE (ref_dbo);

	/* set statement's variables */
	if (! gda_statement_get_parameters (priv->prep_stmts[STMT_ADD_DECLARE_FK],
					    &params, error))
		goto out;
	if (! gda_set_set_holder_value (params, error, "tcal", dbo->obj_catalog))
		goto out;
	if (! gda_set_set_holder_value (params, error, "tschema", dbo->obj_schema))
		goto out;
	if (! gda_set_set_holder_value (params, error, "tname",	dbo->obj_name))
		goto out;
	if (! gda_set_set_holder_value (params, error, "ref_tcal", ref_dbo->obj_catalog))
		goto out;
	if (! gda_set_set_holder_value (params, error, "ref_tschema", ref_dbo->obj_schema))
		goto out;
	if (! gda_set_set_holder_value (params, error, "ref_tname", ref_dbo->obj_name))
		goto out;

	if (! gda_set_set_holder_value (params, error, "fkname", fk_name))
                goto out;

	GdaConnection *store_cnc;
	gboolean intrans;
	store_cnc = gda_meta_store_get_internal_connection (store);
	intrans = gda_connection_begin_transaction (store_cnc, NULL,
						    GDA_TRANSACTION_ISOLATION_UNKNOWN,
						    NULL);

	/* remove existing declared FK, if any */
	if (gda_connection_statement_execute_non_select (store_cnc,
							 priv->prep_stmts[STMT_DEL_DECLARE_FK],
							 params,
							 NULL, error) == -1) {
		if (intrans)
			gda_connection_rollback_transaction (store_cnc, NULL, NULL);
		goto out;
	}

	/* declare FK */
	guint l;
	for (l = 0; l < nb_cols; l++) {
		/* check that column name exists */
		GSList *list;
		for (list = mtable->columns; list; list = list->next) {
			if (!strcmp (GDA_META_TABLE_COLUMN (list->data)->column_name,
				     colnames[l]))
				break;
		}
		if (!list) {
			gchar *str;
			str = arguments_to_name (catalog, schema, table);
			g_set_error (error, GDA_META_STORE_ERROR,
				     GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
				     _("Could not find column '%s' in table '%s'"),
				     colnames[l], str);
			g_free (str);
			goto out;
		}
		if (! gda_set_set_holder_value (params, error, "colname", colnames[l]))
			goto out;

		/* check that column name exists */
		for (list = ref_mtable->columns; list; list = list->next) {
			if (!strcmp (GDA_META_TABLE_COLUMN (list->data)->column_name,
				     ref_colnames[l]))
				break;
		}
		if (!list) {
			gchar *str;
			str = arguments_to_name (ref_catalog, ref_schema, ref_table);
			g_set_error (error, GDA_META_STORE_ERROR,
				     GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
				     _("Could not find column '%s' in table '%s'"),
				     ref_colnames[l], str);
			g_free (str);
			goto out;
		}
		if (! gda_set_set_holder_value (params, error, "ref_colname", ref_colnames[l]))
			goto out;

		if (gda_connection_statement_execute_non_select (store_cnc,
								 priv->prep_stmts[STMT_ADD_DECLARE_FK],
								 params,
								 NULL, error) == -1) {
			if (intrans)
				gda_connection_rollback_transaction (store_cnc, NULL,
								     NULL);
			goto out;
		}
	}

	if (intrans)
		gda_connection_commit_transaction (store_cnc, NULL, NULL);
	retval = TRUE;

 out:
	if (u_mstruct)
		g_object_unref (u_mstruct);
	if (params)
		g_object_unref (params);

	return retval;
}

/**
 * gda_meta_store_undeclare_foreign_key:
 * @store: a #GdaMetaStore
 * @mstruct: (nullable): a #GdaMetaStruct, or %NULL
 * @fk_name: the name of the foreign key to declare
 * @catalog: (nullable): the catalog in which the table (for which the foreign key is for) is, or %NULL
 * @schema: (nullable): the schema in which the table (for which the foreign key is for) is, or %NULL
 * @table: the name of the table (for which the foreign key is for)
 * @ref_catalog: (nullable): the catalog in which the referenced table is, or %NULL
 * @ref_schema: (nullable): the schema in which the referenced table is, or %NULL
 * @ref_table: the name of the referenced table
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Removes a declared foreign key from @store.
 *
 * This method begins a transaction if possible (ie. none is already started), and if it can't, then if there
 * is an error, the job may be partially done.
 *
 * A check is always performed to make sure all the database objects actually
 * exist and returns an error if not. The check is performed using @mstruct if it's not %NULL (in
 * this case only the tables already represented in @mstruct will be considered, in other words: @mstruct
 * will not be modified), and using an internal #GdaMetaStruct is %NULL.
 *
 * See gda_meta_store_declare_foreign_key() for more information anout the @catalog, @schema, @name,
 * @ref_catalog, @ref_schema and @ref_name arguments.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2.4
 */
gboolean
gda_meta_store_undeclare_foreign_key (GdaMetaStore *store, GdaMetaStruct *mstruct,
				      const gchar *fk_name,
				      const gchar *catalog, const gchar *schema, const gchar *table,
				      const gchar *ref_catalog, const gchar *ref_schema, const gchar *ref_table,
				      GError **error)
{
 	gboolean retval = FALSE;
	GdaSet *params = NULL;
	GdaMetaDbObject *dbo = NULL, *ref_dbo = NULL;
	GdaMetaStruct *u_mstruct = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), FALSE);
	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), FALSE);
	g_return_val_if_fail (fk_name, FALSE);
	g_return_val_if_fail (!catalog || (catalog && schema), FALSE);
	g_return_val_if_fail (!ref_catalog || (ref_catalog && ref_schema), FALSE);
	g_return_val_if_fail (table, FALSE);
	g_return_val_if_fail (ref_table, FALSE);

  GdaMetaStorePrivate *priv = gda_meta_store_get_instance_private (store);

	if (!mstruct)
		u_mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", store, "features", GDA_META_STRUCT_FEATURE_NONE, NULL);

	/* find database objects */
	GValue *v1 = NULL, *v2 = NULL, *v3;

	if (catalog)
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), catalog);
	if (schema)
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), schema);
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), table);
	if (mstruct) {
		dbo = gda_meta_struct_get_db_object (mstruct, v1, v2, v3);
		if (!dbo || (dbo->obj_type != GDA_META_DB_TABLE)) {
			gchar *tmp;
			tmp = arguments_to_name (catalog, schema, table);
			g_set_error (error, GDA_META_STRUCT_ERROR,
				     GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Could not find object named '%s'"), tmp);
			g_free (tmp);
			dbo = NULL;
		}
	}
	else
		dbo = gda_meta_struct_complement (u_mstruct, GDA_META_DB_TABLE, v1, v2, v3, error);
	if (v1)
		gda_value_free (v1);
	if (v2)
		gda_value_free (v2);
	gda_value_free (v3);
	if (! dbo)
		goto out;

	v1 = NULL;
	v2 = NULL;
	if (ref_catalog)
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), ref_catalog);
	if (ref_schema)
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), ref_schema);
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), ref_table);
	if (mstruct) {
		ref_dbo = gda_meta_struct_get_db_object (mstruct, v1, v2, v3);
		if (!ref_dbo || (ref_dbo->obj_type != GDA_META_DB_TABLE)) {
			gchar *tmp;
			tmp = arguments_to_name (ref_catalog, ref_schema, ref_table);
			g_set_error (error, GDA_META_STRUCT_ERROR,
				     GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Could not find object named '%s'"), tmp);
			g_free (tmp);
			ref_dbo = NULL;
		}
	}
	else
		ref_dbo = gda_meta_struct_complement (u_mstruct, GDA_META_DB_TABLE, v1, v2, v3, error);
	if (v1)
		gda_value_free (v1);
	if (v2)
		gda_value_free (v2);
	gda_value_free (v3);
	if (! ref_dbo)
		goto out;

	/* set statement's variables */
	if (! gda_statement_get_parameters (priv->prep_stmts[STMT_DEL_DECLARE_FK],
					    &params, error))
		goto out;
	if (! gda_set_set_holder_value (params, error, "tcal", dbo->obj_catalog))
		goto out;
	if (! gda_set_set_holder_value (params, error, "tschema", dbo->obj_schema))
		goto out;
	if (! gda_set_set_holder_value (params, error, "tname",	dbo->obj_name))
		goto out;
	if (! gda_set_set_holder_value (params, error, "ref_tcal", ref_dbo->obj_catalog))
		goto out;
	if (! gda_set_set_holder_value (params, error, "ref_tschema", ref_dbo->obj_schema))
		goto out;
	if (! gda_set_set_holder_value (params, error, "ref_tname", ref_dbo->obj_name))
		goto out;

	if (! gda_set_set_holder_value (params, error, "fkname", fk_name))
                goto out;

	GdaConnection *store_cnc;
	gboolean intrans;
	store_cnc = gda_meta_store_get_internal_connection (store);
	intrans = gda_connection_begin_transaction (store_cnc, NULL,
						    GDA_TRANSACTION_ISOLATION_UNKNOWN,
						    NULL);

	/* remove existing declared FK, if any */
	if (gda_connection_statement_execute_non_select (store_cnc,
							 priv->prep_stmts[STMT_DEL_DECLARE_FK],
							 params,
							 NULL, error) == -1) {
		if (intrans)
			gda_connection_rollback_transaction (store_cnc, NULL, NULL);
		goto out;
	}

	if (intrans)
		gda_connection_commit_transaction (store_cnc, NULL, NULL);
	retval = TRUE;

 out:
	if (u_mstruct)
		g_object_unref (u_mstruct);
	if (params)
		g_object_unref (params);

	return retval;
}

/**
 * gda_meta_store_create_struct:
 * @store: a #GdaMetaStore
 * @features: features of new struct
 *
 * Returns: (transfer full): a new #GdaMetaStruct using @store
 */
GdaMetaStruct*
gda_meta_store_create_struct (GdaMetaStore *store, GdaMetaStructFeature features)
{
  return (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", store, "features", features, NULL);
}

