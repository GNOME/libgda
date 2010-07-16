/* 
 * Copyright (C) 2009 - 2010 Vivien Malerba
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


#ifndef __DATA_SOURCE_H_
#define __DATA_SOURCE_H_

#include <libgda-ui/libgda-ui.h>
#include "browser-favorites.h"
#include "decl.h"

G_BEGIN_DECLS

#define DATA_SOURCE_TYPE          (data_source_get_type())
#define DATA_SOURCE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, data_source_get_type(), DataSource)
#define DATA_SOURCE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, data_source_get_type (), DataSourceClass)
#define IS_DATA_SOURCE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, data_source_get_type ())

typedef struct _DataSource DataSource;
typedef struct _DataSourceClass DataSourceClass;
typedef struct _DataSourcePrivate DataSourcePrivate;

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
	void             (*execution_started) (DataSource *source);
	void             (*execution_finished) (DataSource *source, GError *error);
};

GType               data_source_get_type            (void) G_GNUC_CONST;

DataSource         *data_source_new_from_xml_node   (BrowserConnection *bcnc, xmlNodePtr node, GError **error);
void                data_source_set_params          (DataSource *source, GdaSet *params);
xmlNodePtr          data_source_to_xml_node         (DataSource *source);

GdaSet             *data_source_get_import          (DataSource *source);
GArray             *data_source_get_export_names    (DataSource *source);
GHashTable         *data_source_get_export_columns  (DataSource *source);

void                data_source_execute             (DataSource *source, GError **error);
gboolean            data_source_execution_going_on  (DataSource *source);
GtkWidget          *data_source_create_grid         (DataSource *source);
const gchar        *data_source_get_title           (DataSource *source);

/*
DataSource         *data_source_new_from_table      (BrowserConnection *bcnc,
						     const gchar *table_schema, const gchar *table_name);
DataSource         *data_source_new_from_select     (BrowserConnection *bcnc, const gchar *select_sql);
*/

G_END_DECLS

#endif
