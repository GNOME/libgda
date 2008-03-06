/* gda-meta-struct.h
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

#ifndef __GDA_META_STRUCT_H_
#define __GDA_META_STRUCT_H_

#include <glib-object.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-meta-store.h>

G_BEGIN_DECLS

#define GDA_TYPE_META_STRUCT          (gda_meta_struct_get_type())
#define GDA_META_STRUCT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_meta_struct_get_type(), GdaMetaStruct)
#define GDA_META_STRUCT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_meta_struct_get_type (), GdaMetaStructClass)
#define GDA_IS_META_STRUCT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_meta_struct_get_type ())

typedef struct _GdaMetaStruct        GdaMetaStruct;
typedef struct _GdaMetaStructClass   GdaMetaStructClass;
typedef struct _GdaMetaStructPrivate GdaMetaStructPrivate;

/* error reporting */
extern GQuark gda_meta_struct_error_quark (void);
#define GDA_META_STRUCT_ERROR gda_meta_struct_error_quark ()

typedef enum {
	GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
        GDA_META_STRUCT_DUPLICATE_OBJECT_ERROR,
        GDA_META_STRUCT_INCOHERENCE_ERROR
} GdaMetaStructError;


/* struct for the object's data */
struct _GdaMetaStruct
{
	GObject               object;
	GSList                *db_objects; /* list of GdaMetaDbObject structures */
	GdaMetaStructPrivate  *priv;
};

/* struct for the object's class */
struct _GdaMetaStructClass
{
	GObjectClass              parent_class;
};

/*
 * Type of database object which can be handled
 */
typedef enum {
	GDA_META_DB_UNKNOWN,
	GDA_META_DB_TABLE,
	GDA_META_DB_VIEW
} GdaMetaDbObjectType;

/*
 * Controls which features are computed about database objects
 */
typedef enum {
	GDA_META_STRUCT_FEATURE_ALL,
	GDA_META_STRUCT_FEATURE_FOREIGN_KEYS,
	GDA_META_STRUCT_FEATURE_VIEW_DEPENDENCIES
} GdaMetaStructFeature;

/*
 * Types of sorting
 */
typedef enum {
	GDA_META_SORT_ALHAPETICAL,
	GDA_META_SORT_DEPENDENCIES
} GdaMetaSortType;

/*
 * Complements the GdaMetaDbObject structure, for tables only
 * contains predefined statements for data selection and modifications
 */
typedef struct {
	GSList       *columns; /* list of GdaMetaTableColumn */
	
	/* PK fields index */
	gint         *pk_cols_array;
	gint          pk_cols_nb;

	/* Foreign keys */
	GSList       *reverse_fk_list; /* list of GdaMetaTableForeignKey where @depend_on == this GdaMetaDbObject */
	GSList       *fk_list; /* list of GdaMetaTableForeignKey where @gda_meta_table == this GdaMetaDbObject */
} GdaMetaTable;

/*
 * Complements the GdaMetaDbObject structure, for views only
 * contains more information than for tables
 */
typedef struct {
	GdaMetaTable  table;
	gchar        *view_def;
	gboolean      is_updatable;
} GdaMetaView;

/* 
 * Struture to hold information about each object
 * which can be created in the internal GdaMetaStore's connection.
 * It is available for tables, views, triggers, ...
 */
typedef struct {
	GdaMetaDbObjectType     obj_type;
	gchar                  *obj_catalog;
	gchar                  *obj_schema;
	gchar                  *obj_name;
	gchar                  *obj_short_name;
	gchar                  *obj_full_name;
	gchar                  *obj_owner;

	union {
		GdaMetaTable    meta_table;
		GdaMetaView     meta_view;
	}                       extra;

	GSList                 *depend_list; /* list of GdaMetaDbObject pointers on which this object depends */
} GdaMetaDbObject;
#define GDA_META_DB_OBJECT(x) ((GdaMetaDbObject*)(x))
#define GDA_META_DB_OBJECT_GET_TABLE(dbobj) (&((dbobj)->extra.meta_table))
#define GDA_META_DB_OBJECT_GET_VIEW(dbobj) (&((dbobj)->extra.meta_view))

typedef struct {
	gchar        *column_name;
	gchar        *column_type;
	GType         gtype;
	gboolean      pkey;
        gboolean      nullok;
	gchar        *default_value;
} GdaMetaTableColumn;
#define GDA_META_TABLE_COLUMN(x) ((GdaMetaTableColumn*)(x))

typedef struct {
	GdaMetaDbObject  *meta_table;
	GdaMetaDbObject  *depend_on;

	gint              cols_nb;	
	gint             *fk_cols_array; /* FK fields index */
	gchar           **fk_names_array; /* FK fields names */
	gint             *ref_pk_cols_array; /* Ref PK fields index */
	gchar           **ref_pk_names_array; /* Ref PK fields names */
} GdaMetaTableForeignKey;
#define GDA_META_TABLE_FOREIGN_KEY(x) ((GdaMetaTableForeignKey*)(x))


GType               gda_meta_struct_get_type         (void) G_GNUC_CONST;
GdaMetaStruct      *gda_meta_struct_new              (void);
GdaMetaDbObject    *gda_meta_struct_complement       (GdaMetaStruct *mstruct, GdaMetaStore *store, GdaMetaDbObjectType type,
                                                      const GValue *catalog, const GValue *schema, const GValue *name,
                                                      GError **error);
gboolean            gda_meta_struct_sort_db_objects  (GdaMetaStruct *mstruct, GdaMetaSortType sort_type, GError **error);
GdaMetaDbObject    *gda_meta_struct_get_db_object    (GdaMetaStruct *mstruct,
                                                      const GValue *catalog, const GValue *schema, const GValue *name);
GdaMetaTableColumn *gda_meta_struct_get_table_column (GdaMetaStruct *mstruct, GdaMetaTable *table,
                                                      const GValue *col_name);

G_END_DECLS

#endif
