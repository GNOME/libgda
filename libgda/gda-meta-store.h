/*
 * Copyright (C) 2008 - 2011 Vivien Malerba
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

#ifndef __GDA_META_STORE_H_
#define __GDA_META_STORE_H_

#include <glib-object.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_META_STORE          (gda_meta_store_get_type())
#define GDA_META_STORE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_meta_store_get_type(), GdaMetaStore)
#define GDA_META_STORE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_meta_store_get_type (), GdaMetaStoreClass)
#define GDA_IS_META_STORE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_meta_store_get_type ())

/* error reporting */
extern GQuark gda_meta_store_error_quark (void);
#define GDA_META_STORE_ERROR gda_meta_store_error_quark ()

typedef enum {
	GDA_META_STORE_INCORRECT_SCHEMA_ERROR,
	GDA_META_STORE_UNSUPPORTED_PROVIDER_ERROR,
	GDA_META_STORE_INTERNAL_ERROR,
	GDA_META_STORE_META_CONTEXT_ERROR,
	GDA_META_STORE_MODIFY_CONTENTS_ERROR,
	GDA_META_STORE_EXTRACT_SQL_ERROR,
	GDA_META_STORE_ATTRIBUTE_NOT_FOUND_ERROR,
	GDA_META_STORE_ATTRIBUTE_ERROR,
	GDA_META_STORE_SCHEMA_OBJECT_NOT_FOUND_ERROR,
	GDA_META_STORE_SCHEMA_OBJECT_CONFLICT_ERROR,
	GDA_META_STORE_SCHEMA_OBJECT_DESCR_ERROR,
	GDA_META_STORE_TRANSACTION_ALREADY_STARTED_ERROR
} GdaMetaStoreError;

/* 
 * tracking changes in the meta database 
 */
typedef enum {
	GDA_META_STORE_ADD,
	GDA_META_STORE_REMOVE,
	GDA_META_STORE_MODIFY
} GdaMetaStoreChangeType;

typedef struct {
	/* change general information */
	GdaMetaStoreChangeType  c_type;
	gchar                  *table_name;
	GHashTable             *keys; /* key = ('+' or '-') and a column position in @table (string) starting at 0, 
	                               * value = a GValue pointer */
} GdaMetaStoreChange;

/**
 * GdaMetaContext:
 * @table_name: the name of the table <emphasis>in the GdaMetaStore's internal database</emphasis>
 * @size: the size of the @column_names and @column_values arrays
 * @column_names: an array of column names (columns of the @table_name table)
 * @column_values: an array of values, one for each column named in @column_names
 *
 * The <structname>GdaMetaContext</structname> represents a meta data modification
 * context: the <emphasis>how</emphasis> when used with gda_meta_store_modify_with_context(),
 * and the <emphasis>what</emphasis> when used with gda_connection_update_meta_store().
 */
typedef struct {
	gchar                  *table_name;
	gint                    size;
	gchar                 **column_names;
	GValue                **column_values;
} GdaMetaContext;

/* struct for the object's data */
struct _GdaMetaStore
{
	GObject               object;
	GdaMetaStorePrivate  *priv;
};

/* struct for the object's class */
struct _GdaMetaStoreClass
{
	GObjectClass              parent_class;
	GdaMetaStoreClassPrivate *cpriv;

	/* signals the changes */
	void     (*meta_reset)    (GdaMetaStore *store);
	GError  *(*suggest_update)(GdaMetaStore *store, GdaMetaContext *suggest);
	void     (*meta_changed)  (GdaMetaStore *store, GSList *changes);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-meta-store
 * @short_description: Dictionary object
 * @title: GdaMetaStore
 * @stability: Stable
 * @see_also: #GdaMetaStruct
 *
 * Previous versions of &LIBGDA; relied on an XML based file to store dictionary information, such as
 * the database's schema (tables, views, etc) and various other information. The problems were that it was
 * difficult for an application to integrate its own data into the dictionary and that there were some
 * performances problems as the XML file needed to be parsed (and converted into its own in-memory structure)
 * before any data could be read out of it.
 *
 * The new dictionary now relies on a database structure to store its data (see the 
 * <link linkend="information_schema">database schema</link> section for a detailed description). The actual database can be a
 * single file (using an SQLite database), an entirely in memory database (also using an SQLite database), or
 * a more conventional backend such as a PostgreSQL database for a shared dictionary on a server.
 *
 * The #GdaMetaStore object is thread safe.
 */

GType             gda_meta_store_get_type                 (void) G_GNUC_CONST;
GdaMetaStore     *gda_meta_store_new_with_file            (const gchar *file_name);
GdaMetaStore     *gda_meta_store_new                      (const gchar *cnc_string);
gint              gda_meta_store_get_version              (GdaMetaStore *store);

GdaConnection    *gda_meta_store_get_internal_connection  (GdaMetaStore *store);
gchar            *gda_meta_store_sql_identifier_quote     (const gchar *id, GdaConnection *cnc);
GdaDataModel     *gda_meta_store_extract                  (GdaMetaStore *store, const gchar *select_sql, GError **error, ...);
gboolean          gda_meta_store_modify                   (GdaMetaStore *store, const gchar *table_name, 
							   GdaDataModel *new_data, const gchar *condition, GError **error, ...);
gboolean          gda_meta_store_modify_with_context      (GdaMetaStore *store, GdaMetaContext *context, 
							   GdaDataModel *new_data, GError **error);
GdaDataModel     *gda_meta_store_create_modify_data_model (GdaMetaStore *store, const gchar *table_name);

void              gda_meta_store_set_identifiers_style    (GdaMetaStore *store, GdaSqlIdentifierStyle style);
void              gda_meta_store_set_reserved_keywords_func(GdaMetaStore *store, GdaSqlReservedKeywordsFunc func);

gboolean          gda_meta_store_get_attribute_value      (GdaMetaStore *store, const gchar *att_name, 
							   gchar **att_value, GError **error);
gboolean          gda_meta_store_set_attribute_value      (GdaMetaStore *store, const gchar *att_name, 
							   const gchar *att_value, GError **error);

gboolean          gda_meta_store_schema_add_custom_object    (GdaMetaStore *store, const gchar *xml_description, 
							      GError **error);
gboolean          gda_meta_store_schema_remove_custom_object (GdaMetaStore *store, const gchar *obj_name, GError **error);

GSList           *gda_meta_store_schema_get_all_tables    (GdaMetaStore *store);
GSList           *gda_meta_store_schema_get_depend_tables (GdaMetaStore *store, const gchar *table_name);
GdaMetaStruct    *gda_meta_store_schema_get_structure     (GdaMetaStore *store, GError **error);

gboolean          gda_meta_store_declare_foreign_key      (GdaMetaStore *store, GdaMetaStruct *mstruct,
							   const gchar *fk_name,
							   const gchar *catalog, const gchar *schema, const gchar *table,
							   const gchar *ref_catalog, const gchar *ref_schema, const gchar *ref_table,
							   guint nb_cols,
							   gchar **colnames, gchar **ref_colnames,
							   GError **error);

gboolean          gda_meta_store_undeclare_foreign_key    (GdaMetaStore *store, GdaMetaStruct *mstruct,
							   const gchar *fk_name,
							   const gchar *catalog, const gchar *schema, const gchar *table,
							   const gchar *ref_catalog, const gchar *ref_schema, const gchar *ref_table,
							   GError **error);

G_END_DECLS

#endif
