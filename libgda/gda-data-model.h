/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2005 Dan Winship <danw@src.gnome.org>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011,2019 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_DATA_MODEL_H__
#define __GDA_DATA_MODEL_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-column.h>
#include <libgda/gda-value.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-model-iter.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gda_data_model_error_quark (void);
#define GDA_DATA_MODEL_ERROR gda_data_model_error_quark ()

typedef enum {
	GDA_DATA_MODEL_ACCESS_RANDOM = 1 << 0,
	GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD = 1 << 1,
	GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD = 1 << 2,
	GDA_DATA_MODEL_ACCESS_CURSOR = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD,
	GDA_DATA_MODEL_ACCESS_INSERT  = 1 << 3,
	GDA_DATA_MODEL_ACCESS_UPDATE  = 1 << 4,
	GDA_DATA_MODEL_ACCESS_DELETE  = 1 << 5,
	GDA_DATA_MODEL_ACCESS_WRITE = GDA_DATA_MODEL_ACCESS_INSERT | GDA_DATA_MODEL_ACCESS_UPDATE |
	GDA_DATA_MODEL_ACCESS_DELETE
} GdaDataModelAccessFlags;

typedef enum {
	GDA_DATA_MODEL_HINT_START_BATCH_UPDATE,
	GDA_DATA_MODEL_HINT_END_BATCH_UPDATE,
	GDA_DATA_MODEL_HINT_REFRESH
} GdaDataModelHint;

/**
 * GdaDataModelIOFormat:
 * @GDA_DATA_MODEL_IO_DATA_ARRAY_XML: data is exported as an XML structure
 * @GDA_DATA_MODEL_IO_TEXT_SEPARATED: data is exported as CSV
 * @GDA_DATA_MODEL_IO_TEXT_TABLE: data is exported as a human readable table
 *
 * Format to use when exporting a data model, see gda_data_model_export_to_string() and gda_data_model_export_to_file()
 */
typedef enum {
	GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
	GDA_DATA_MODEL_IO_TEXT_SEPARATED,
	GDA_DATA_MODEL_IO_TEXT_TABLE
} GdaDataModelIOFormat;

typedef enum {
	GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
	GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
	GDA_DATA_MODEL_VALUES_LIST_ERROR,
	GDA_DATA_MODEL_VALUE_TYPE_ERROR,
	GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
	GDA_DATA_MODEL_ACCESS_ERROR,
	GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
	GDA_DATA_MODEL_FILE_EXIST_ERROR,
	GDA_DATA_MODEL_XML_FORMAT_ERROR,

	GDA_DATA_MODEL_TRUNCATED_ERROR,
	GDA_DATA_MODEL_INVALID,
	GDA_DATA_MODEL_OTHER_ERROR
} GdaDataModelError;


#define GDA_TYPE_DATA_MODEL            (gda_data_model_get_type())
G_DECLARE_INTERFACE (GdaDataModel, gda_data_model, GDA, DATA_MODEL, GObject)

/* struct for the interface */
struct _GdaDataModelInterface {
	GTypeInterface           g_iface;

	/* virtual table */
	gint                 (* get_n_rows)       (GdaDataModel *model);
	gint                 (* get_n_columns)    (GdaDataModel *model);

	GdaColumn           *(* describe_column)  (GdaDataModel *model, gint col);
	GdaDataModelAccessFlags (* get_access_flags) (GdaDataModel *model);

	const GValue        *(* get_value_at)     (GdaDataModel *model, gint col, gint row, GError **error);
	GdaValueAttribute    (* get_attributes_at)(GdaDataModel *model, gint col, gint row);
	GdaDataModelIter    *(* create_iter)      (GdaDataModel *model);

	gboolean             (* set_value_at)     (GdaDataModel *model, gint col, gint row,
						     const GValue *value, GError **error);
	gboolean             (* set_values)       (GdaDataModel *model, gint row, GList *values,
						     GError **error);
	gint                 (* append_values)    (GdaDataModel *model, const GList *values, GError **error);
	gint                 (* append_row)       (GdaDataModel *model, GError **error);
	gboolean             (* remove_row)       (GdaDataModel *model, gint row, GError **error);
	gint                 (* find_row)         (GdaDataModel *model, GSList *values, gint *cols_index);

	void                 (* freeze)           (GdaDataModel *model);
	void                 (* thaw)             (GdaDataModel *model);
	gboolean             (* get_notify)       (GdaDataModel *model);
	void                 (* send_hint)        (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value);

	GError             **(* get_exceptions)   (GdaDataModel *model);

	/* signals */
	void                 (* row_inserted)       (GdaDataModel *model, gint row);
	void                 (* row_updated)        (GdaDataModel *model, gint row);
	void                 (* row_removed)        (GdaDataModel *model, gint row);
	void                 (* changed)            (GdaDataModel *model);
	void                 (* reset)              (GdaDataModel *model);
	void                 (* access_changed)     (GdaDataModel *model);
};

/**
 * SECTION:gda-data-model
 * @short_description: Data model interface
 * @title: GdaDataModel
 * @stability: Stable
 * @see_also: #GdaDataModelIter
 *
 * A #GdaDataModel represents an array of values organized in rows and columns. All the data in the same 
 * column have the same type, and all the data in each row have the same semantic meaning. The #GdaDataModel is
 * actually an interface implemented by other objects to support various kinds of data storage and operations.
 *
 * When a SELECT statement is executed using an opened #GdaConnection, the returned value (if no error occurred)
 * is a #GdaDataSelect object which implements the #GdaDataModel interface. Please see the #GdaDataSelect's
 * documentation for more information.
 *
 * Depending on the real implementation, the contents of data models may be modified by the user using functions
 * provided by the model. The actual operations a data model permits can be known using the 
 * gda_data_model_get_access_flags() method.
 *
 * Again, depending on the real implementation, data retrieving can be done either accessing direct random
 * values located by their row and column, or using a cursor, or both. Use the gda_data_model_get_access_flags() 
 * method to know how the data model can be accessed. 
 * <itemizedlist>
 *   <listitem><para>Random access to a data model's contents is done using gda_data_model_get_value_at(), or using
 *       one or more #GdaDataModelIter object(s);</para></listitem>
 *   <listitem><para>Cursor access to a data model's contents is done using a #GdaDataModelIter object. If this mode is
 *       the only supported, then only one #GdaDataModelIter object can be created and
 *       it is <emphasis>not possible</emphasis> to use gda_data_model_get_value_at() in this case.</para></listitem>
 * </itemizedlist>
 *
 * Random access data models are easier to use since picking a value is very simple using the gda_data_model_get_value_at(),
 * but consume more memory since all the accessible values must generally be present in memory even if they are not used.
 * Thus if a data model must handle large quantities of data, it is generally wiser to use a data model which can be 
 * only accessed using a cursor.
 *
 * As a side note there are also data models which wrap other data models such as:
 * <itemizedlist>
 *     <listitem><para>The #GdaDataProxy data model which stores temporary modifications and shows only some
 * 	parts of the wrapped data model</para></listitem>
 *     <listitem><para>The #GdaDataAccessWrapper data model which offers a memory efficient random access on top of a
 * 	wrapped cursor based access data model</para></listitem>
 * </itemizedlist>
 *
 * Also see the section about <link linkend="gda-data-model-writing">writing your own GdaDataModel</link>
 *
 * Finally, the #GdaDataModel object implements its own locking mechanism and can be used simultaneously from several threads.
 */

GdaDataModelAccessFlags gda_data_model_get_access_flags   (GdaDataModel *model);

gint                gda_data_model_get_n_rows             (GdaDataModel *model);
gint                gda_data_model_get_n_columns          (GdaDataModel *model);

GdaColumn          *gda_data_model_describe_column        (GdaDataModel *model, gint col);
gint                gda_data_model_get_column_index       (GdaDataModel *model, const gchar *name);
const gchar        *gda_data_model_get_column_name       (GdaDataModel *model, gint col);
void                gda_data_model_set_column_name       (GdaDataModel *model, gint col, const gchar *name);
const gchar        *gda_data_model_get_column_title       (GdaDataModel *model, gint col);
void                gda_data_model_set_column_title       (GdaDataModel *model, gint col, const gchar *title);

const GValue       *gda_data_model_get_value_at           (GdaDataModel *model, gint col, gint row, GError **error);
const GValue       *gda_data_model_get_typed_value_at     (GdaDataModel *model, gint col, gint row, 
							   GType expected_type, gboolean nullok, GError **error);
GdaValueAttribute   gda_data_model_get_attributes_at      (GdaDataModel *model, gint col, gint row);
GdaDataModelIter   *gda_data_model_create_iter            (GdaDataModel *model);
void                gda_data_model_freeze                 (GdaDataModel *model);
void                gda_data_model_thaw                   (GdaDataModel *model);
gboolean            gda_data_model_get_notify             (GdaDataModel *model);
gboolean            gda_data_model_set_value_at           (GdaDataModel *model, gint col, gint row, 
							   const GValue *value, GError **error);
gboolean            gda_data_model_set_values             (GdaDataModel *model, gint row, 
							   GList *values, GError **error);
gint                gda_data_model_append_row             (GdaDataModel *model, GError **error);
gint                gda_data_model_append_values          (GdaDataModel *model, const GList *values, GError **error);
gboolean            gda_data_model_remove_row             (GdaDataModel *model, gint row, GError **error);
gint                gda_data_model_get_row_from_values    (GdaDataModel *model, GSList *values, gint *cols_index);

void                gda_data_model_send_hint              (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value);

GError            **gda_data_model_get_exceptions         (GdaDataModel *model);

/* contents saving and loading */
gchar              *gda_data_model_export_to_string       (GdaDataModel *model, GdaDataModelIOFormat format, 
							   const gint *cols, gint nb_cols, 
							   const gint *rows, gint nb_rows, GdaSet *options);
gboolean            gda_data_model_export_to_file         (GdaDataModel *model, GdaDataModelIOFormat format, 
							   const gchar *file,
							   const gint *cols, gint nb_cols, 
							   const gint *rows, gint nb_rows, 
							   GdaSet *options, GError **error);

gboolean            gda_data_model_import_from_model      (GdaDataModel *to, GdaDataModel *from, gboolean overwrite,
							   GHashTable *cols_trans, GError **error);
gboolean            gda_data_model_import_from_string     (GdaDataModel *model,
							   const gchar *string, GHashTable *cols_trans,
							   GdaSet *options, GError **error);
gboolean            gda_data_model_import_from_file       (GdaDataModel *model,
							   const gchar *file, GHashTable *cols_trans,
							   GdaSet *options, GError **error);

/* debug functions */
void                gda_data_model_dump                   (GdaDataModel *model, FILE *to_stream);
gchar              *gda_data_model_dump_as_string         (GdaDataModel *model);

G_END_DECLS

#endif
