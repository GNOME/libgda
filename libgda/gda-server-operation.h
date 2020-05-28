/*
 * Copyright (C) 2006 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_SERVER_OPERATION_H__
#define __GDA_SERVER_OPERATION_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-set.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_OPERATION            (gda_server_operation_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaServerOperation, gda_server_operation, GDA, SERVER_OPERATION, GObject)

/*
 * Types of identified operations
 * all the providers don't implement them completely, though
 */
typedef enum {
	GDA_SERVER_OPERATION_CREATE_DB,
	GDA_SERVER_OPERATION_DROP_DB,
		
	GDA_SERVER_OPERATION_CREATE_TABLE,
	GDA_SERVER_OPERATION_DROP_TABLE,
	GDA_SERVER_OPERATION_RENAME_TABLE,

	GDA_SERVER_OPERATION_ADD_COLUMN,
	GDA_SERVER_OPERATION_DROP_COLUMN,
	GDA_SERVER_OPERATION_RENAME_COLUMN,

	GDA_SERVER_OPERATION_CREATE_INDEX,
	GDA_SERVER_OPERATION_DROP_INDEX,
	GDA_SERVER_OPERATION_RENAME_INDEX,

	GDA_SERVER_OPERATION_CREATE_VIEW,
	GDA_SERVER_OPERATION_DROP_VIEW,

	GDA_SERVER_OPERATION_COMMENT_TABLE,
	GDA_SERVER_OPERATION_COMMENT_COLUMN,

	GDA_SERVER_OPERATION_CREATE_USER,
	GDA_SERVER_OPERATION_ALTER_USER,
	GDA_SERVER_OPERATION_DROP_USER,

	GDA_SERVER_OPERATION_LAST
} GdaServerOperationType;

/* error reporting */
extern GQuark gda_server_operation_error_quark (void);
#define GDA_SERVER_OPERATION_ERROR gda_server_operation_error_quark ()

typedef enum {
	GDA_SERVER_OPERATION_OBJECT_NAME_ERROR,
	GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
	GDA_SERVER_OPERATION_XML_ERROR
} GdaServerOperationError;

typedef enum
{
	GDA_SERVER_OPERATION_CREATE_TABLE_NOTHING_FLAG   = 1 << 0,
	GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_FLAG      = 1 << 1,
	GDA_SERVER_OPERATION_CREATE_TABLE_NOT_NULL_FLAG  = 1 << 2,
	GDA_SERVER_OPERATION_CREATE_TABLE_UNIQUE_FLAG    = 1 << 3,
	GDA_SERVER_OPERATION_CREATE_TABLE_AUTOINC_FLAG   = 1 << 4,
	GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG      = 1 << 5,
	/* Flags combinations */
	GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_AUTOINC_FLAG = GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_FLAG | GDA_SERVER_OPERATION_CREATE_TABLE_AUTOINC_FLAG
} GdaServerOperationCreateTableFlag;

typedef enum {
	GDA_SERVER_OPERATION_NODE_PARAMLIST,
	GDA_SERVER_OPERATION_NODE_DATA_MODEL,
	GDA_SERVER_OPERATION_NODE_PARAM,
	GDA_SERVER_OPERATION_NODE_SEQUENCE,
	GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM,

	GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN,
	GDA_SERVER_OPERATION_NODE_UNKNOWN
} GdaServerOperationNodeType;

typedef enum {
	GDA_SERVER_OPERATION_STATUS_OPTIONAL,
	GDA_SERVER_OPERATION_STATUS_REQUIRED,
	GDA_SERVER_OPERATION_STATUS_UNKNOWN
} GdaServerOperationNodeStatus;

/**
 * SECTION:gda-server-operation-sequences
 * @short_description: Manipulating sequences
 * @title: GdaServerOperation: sequences
 * @stability: Stable
 * @see_also: #GdaServerOperation
 *
 * The #GdaServerOperation object can contain sequences of templates. For example when creating a table,
 * one can specify several foreign keys where for each foreign key, one must define the column(s) on which the
 * foreign key applies, the referenced table and the corresponding columns of the referenced table (plus some
 * additional information). In this case the foreign keys are defined as a sequence of templates (the foreign key
 * definition): there can be zero or more foreign keys.
 */

/**
 * SECTION:gda-server-operation-nodes
 * @short_description: Getting information about parts (nodes) composing a path
 * @title: GdaServerOperation: individual nodes
 * @stability: Stable
 * @see_also: #GdaServerOperation
 *
 * To each part of a path is associated a node (as a #GdaServerOperationNode structure). For example the
 * "/TABLE_DEF_P/TABLE_NAME" path has two nodes, one associated to "/TABLE_DEF_P" and one to
 * "/TABLE_DEF_P/TABLE_NAME". For more information about the path's format, see the
 * gda_server_operation_set_value_at()'s documentation.
 *
 * This API is designed to get information about all the nodes present in a #GdaServerOperation object (refer to the
 * gda_server_operation_get_root_nodes() function) and about each node of a path, and allows inspection
 * of its contents. It is mainly reserved for database provider's implementations but can have its purpose
 * outside of this scope.
 */

typedef struct _GdaServerOperationNode {
	GdaServerOperationNodeType    type;
	GdaServerOperationNodeStatus  status;
	
	GdaSet                       *plist;
	GdaDataModel                 *model;
	GdaColumn                    *column;
	GdaHolder                    *param; 
	gpointer                      priv;
} GdaServerOperationNode;

#define GDA_TYPE_SERVER_OPERATION_NODE gda_server_operation_node_get_type ()

GType    gda_server_operation_node_get_type (void) G_GNUC_CONST;
GdaServerOperationNode*
         gda_server_operation_node_copy (GdaServerOperationNode *src);
void     gda_server_operation_node_free (GdaServerOperationNode *src);

struct _GdaServerOperationClass {
	GObjectClass               parent_class;

	/* signals */
	void                     (*seq_item_added) (GdaServerOperation *op, const gchar *seq_path, gint item_index);
	void                     (*seq_item_remove) (GdaServerOperation *op, const gchar *seq_path, gint item_index);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-server-operation
 * @short_description: Handles any DDL query in an abstract way
 * @title: GdaServerOperation
 * @stability: Stable
 * @see_also:
 *
 * This object is basically just a data store: it can store named values, the values being
 * organized hierarchically by their name which are similar to a Unix file path. For example a value can be read from its path
 * using the gda_server_operation_get_value_at() method, or set using the gda_server_operation_set_value_at() method.
 *
 * Each #GdaServerOperation contains some structure which is usually defined by a database provider to implement
 * a specific operation. The structure is composed of the following building blocks:
 * <itemizedlist>
 *   <listitem><para>Named values (internally represented as a #GdaHolder object)</para></listitem>
 *   <listitem><para>Named values in a vector (internally represented as a #GdaSet object)</para></listitem>
 *   <listitem><para>Values in an array (internally represented as a #GdaDataModel object)</para></listitem>
 *   <listitem><para>Sequences of one or more of the previous blocks. A sequence can contain any number of
 *   instances of the template block (there may be lower and upper boundaries to the number of instances)</para></listitem>
 * </itemizedlist>
 *
 * <emphasis>Important note:</emphasis> #GdaServerOperation objects are usually not created 
 * manually using gda_server_operation_new(), but
 * using a #GdaServerProvider object with gda_server_provider_create_operation().
 * See the <link linkend="DDLIntro">global introduction about DDL</link> for more information.
 * Alternatively one can use the <link linkend="libgda-40-Convenience-functions">Convenience functions</link>
 * which internally manipulate #GdaServerOperation objects.
 */

GdaServerOperation        *gda_server_operation_new                     (GdaServerOperationType op_type, const gchar *xml_file);
GdaServerOperationType     gda_server_operation_get_op_type             (GdaServerOperation *op);
const gchar               *gda_server_operation_op_type_to_string       (GdaServerOperationType type);
GdaServerOperationType     gda_server_operation_string_to_op_type       (const gchar *str);
GdaServerOperationNode    *gda_server_operation_get_node_info           (GdaServerOperation *op, const gchar *path_format, ...);

const GValue              *gda_server_operation_get_value_at            (GdaServerOperation *op, const gchar *path_format, ...);
const GValue              *gda_server_operation_get_value_at_path       (GdaServerOperation *op, const gchar *path);
gchar                     *gda_server_operation_get_sql_identifier_at   (GdaServerOperation *op,
									 const gchar *path_format, GError **error,
									 ...);
gchar                     *gda_server_operation_get_sql_identifier_at_path (GdaServerOperation *op, 
									    const gchar *path, GError **error);
gboolean                   gda_server_operation_set_value_at            (GdaServerOperation *op, const gchar *value, 
									 GError **error, const gchar *path_format, ...);
gboolean                   gda_server_operation_set_value_at_path       (GdaServerOperation *op, const gchar *value, 
									 const gchar *path, GError **error);

xmlNodePtr                 gda_server_operation_save_data_to_xml        (GdaServerOperation *op, GError **error);
gchar*                     gda_server_operation_save_data_to_xml_string (GdaServerOperation *op,
                   G_GNUC_UNUSED GError **error);
gboolean                   gda_server_operation_load_data_from_xml      (GdaServerOperation *op, 
									 xmlNodePtr node, GError **error);

gchar**                    gda_server_operation_get_root_nodes          (GdaServerOperation *op);
GdaServerOperationNodeType gda_server_operation_get_node_type           (GdaServerOperation *op, const gchar *path,
								         GdaServerOperationNodeStatus *status);
gchar                     *gda_server_operation_get_node_parent         (GdaServerOperation *op, const gchar *path);
gchar                     *gda_server_operation_get_node_path_portion   (GdaServerOperation *op, const gchar *path);

const gchar               *gda_server_operation_get_sequence_name       (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_size       (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_max_size   (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_min_size   (GdaServerOperation *op, const gchar *path);
gchar                    **gda_server_operation_get_sequence_item_names (GdaServerOperation *op, const gchar *path); 

guint                      gda_server_operation_add_item_to_sequence    (GdaServerOperation *op, const gchar *seq_path);
gboolean                   gda_server_operation_del_item_from_sequence  (GdaServerOperation *op, const gchar *item_path);

gboolean                   gda_server_operation_is_valid                (GdaServerOperation *op, const gchar *xml_file, GError **error);
gboolean                   gda_server_operation_is_valid_from_resource  (GdaServerOperation *op, const gchar *resource, GError **error);

/*
 * Database creation and destruction
 */
gchar              *gda_server_operation_render                        (GdaServerOperation *op, GError **error);

/*
 * Tables creation and destruction
 */

typedef struct _GdaServerOperationCreateTableArg GdaServerOperationCreateTableArg;

#define GDA_TYPE_SERVER_OPERATION_CREATE_TABLE_ARG (gda_server_operation_create_table_arg_get_type ())

GType   gda_server_operation_create_table_arg_get_type        (void) G_GNUC_CONST;
GdaServerOperationCreateTableArg*
        gda_server_operation_create_table_arg_new  (void);
GdaServerOperationCreateTableArg*
        gda_server_operation_create_table_arg_copy (GdaServerOperationCreateTableArg* src);
void    gda_server_operation_create_table_arg_free            (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_column_name (GdaServerOperationCreateTableArg *arg,
                                                                       const gchar *name);
gchar*  gda_server_operation_create_table_arg_get_column_name (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_column_type (GdaServerOperationCreateTableArg *arg,
                                                                       GType ctype);
GType   gda_server_operation_create_table_arg_get_column_type (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_flags (GdaServerOperationCreateTableArg *arg,
                                                                 GdaServerOperationCreateTableFlag flags);
GdaServerOperationCreateTableFlag
        gda_server_operation_create_table_arg_get_flags (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_fkey_table (GdaServerOperationCreateTableArg *arg, const gchar *name);
gchar*  gda_server_operation_create_table_arg_get_fkey_table (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_fkey_ondelete (GdaServerOperationCreateTableArg *arg, const gchar *action);
gchar*  gda_server_operation_create_table_arg_get_fkey_ondelete (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_fkey_ondupdate (GdaServerOperationCreateTableArg *arg, const gchar *action);
gchar*  gda_server_operation_create_table_arg_get_fkey_onupdate (GdaServerOperationCreateTableArg *arg);
void    gda_server_operation_create_table_arg_set_fkey_refs (GdaServerOperationCreateTableArg *arg, GList *refs);
GList*  gda_server_operation_create_table_arg_get_fkey_refs (GdaServerOperationCreateTableArg *arg);

typedef struct _GdaServerOperationCreateTableArgFKeyRefField GdaServerOperationCreateTableArgFKeyRefField;

#define GDA_TYPE_SERVER_OPERATION_CREATE_TABLE_ARG_FKEY_REF_FIELD (gda_server_operation_create_table_arg_fkey_ref_field_get_type ())

GType   gda_server_operation_create_table_arg_fkey_ref_field_get_type (void) G_GNUC_CONST;
GdaServerOperationCreateTableArgFKeyRefField*
        gda_server_operation_create_table_arg_fkey_ref_field_new (void);
GdaServerOperationCreateTableArgFKeyRefField*
        gda_server_operation_create_table_arg_fkey_ref_field_copy (GdaServerOperationCreateTableArgFKeyRefField *src);
void    gda_server_operation_create_table_arg_fkey_ref_field_free (GdaServerOperationCreateTableArgFKeyRefField *ref);
void    gda_server_operation_create_table_arg_fkey_ref_field_set_local_field (GdaServerOperationCreateTableArgFKeyRefField *ref, const gchar *name);
gchar*  gda_server_operation_create_table_arg_fkey_ref_field_get_local_field (GdaServerOperationCreateTableArgFKeyRefField *ref);
void    gda_server_operation_create_table_arg_fkey_ref_field_set_referenced_field (GdaServerOperationCreateTableArgFKeyRefField *ref, const gchar *name);
gchar*  gda_server_operation_create_table_arg_fkey_ref_field_get_referenced_field (GdaServerOperationCreateTableArgFKeyRefField *ref);

G_DEPRECATED_FOR(gda_server_operation_create_table_arg_fkey_ref_field_get_type)
GType   gda_server_operation_create_table_arg_get_fkey_ref_field_get_type (void) G_GNUC_CONST;

GdaServerOperation *gda_server_operation_prepare_create_database   (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_server_operation_perform_create_database   (GdaServerOperation *op, const gchar *provider, GError **error);
GdaServerOperation *gda_server_operation_prepare_drop_database     (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_server_operation_perform_drop_database     (GdaServerOperation *op, const gchar *provider, GError **error);

G_END_DECLS

#endif
