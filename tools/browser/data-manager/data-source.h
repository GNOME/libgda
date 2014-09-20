/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __DATA_SOURCE_H_
#define __DATA_SOURCE_H_

#include <libgda-ui/libgda-ui.h>
#include "decl.h"
#include "common/t-connection.h"

G_BEGIN_DECLS

#define DATA_SOURCE_TYPE          (data_source_get_type())
#define DATA_SOURCE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, data_source_get_type(), DataSource)
#define DATA_SOURCE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, data_source_get_type (), DataSourceClass)
#define IS_DATA_SOURCE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, data_source_get_type ())

typedef struct _DataSource DataSource;
typedef struct _DataSourceClass DataSourceClass;
typedef struct _DataSourcePrivate DataSourcePrivate;

typedef enum {
	DATA_SOURCE_UNKNOWN,
	DATA_SOURCE_TABLE,
	DATA_SOURCE_SELECT,
} DataSourceType;

/* struct for the object's data */
struct _DataSource
{
	GObject            object;
	DataSourcePrivate *priv;
};

/* struct for the object's class */
struct _DataSourceClass
{
	GObjectClass       parent_class;

	/* signals */
	void             (*changed) (DataSource *source);
	void             (*execution_started) (DataSource *source);
	void             (*execution_finished) (DataSource *source, GError *error);
};

GType               data_source_get_type            (void) G_GNUC_CONST;

DataSourceType      data_source_get_source_type     (DataSource *source);
void                data_source_set_id              (DataSource *source, const gchar *id);
const gchar        *data_source_get_id              (DataSource *source);
void                data_source_set_title           (DataSource *source, const gchar *title);
const gchar        *data_source_get_title           (DataSource *source);

/* Data source as table API */
gboolean            data_source_set_table           (DataSource *source, const gchar *table, GError **error);
const gchar        *data_source_get_table           (DataSource *source);
gboolean            data_source_add_dependency      (DataSource *source, const gchar *table,
						     const char *id,
						     gint col_name_size, const gchar **col_names,
						     GError **error);

/* Data source as SQL query API */
void                data_source_set_query           (DataSource *source, const gchar *sql, GError **warning);

/* other API */
DataSource         *data_source_new                 (TConnection *tcnc, DataSourceType type);
DataSource         *data_source_new_from_xml_node   (TConnection *tcnc, xmlNodePtr node, GError **error);
void                data_source_set_params          (DataSource *source, GdaSet *params);
xmlNodePtr          data_source_to_xml_node         (DataSource *source);

GdaStatement       *data_source_get_statement       (DataSource *source);

GdaSet             *data_source_get_import          (DataSource *source);
GArray             *data_source_get_export_names    (DataSource *source);
GHashTable         *data_source_get_export_columns  (DataSource *source);

void                data_source_execute             (DataSource *source, GError **error);
gboolean            data_source_execution_going_on  (DataSource *source);
GtkWidget          *data_source_create_grid         (DataSource *source);

/*
DataSource         *data_source_new_from_table      (TConnection *tcnc,
						     const gchar *table_schema, const gchar *table_name);
DataSource         *data_source_new_from_select     (TConnection *tcnc, const gchar *select_sql);
*/

void                data_source_should_rerun        (DataSource *source);

G_END_DECLS

#endif
