/* 
 * Copyright (C) 2010 Vivien Malerba
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


#ifndef __DATA_SOURCE_MANAGER_H_
#define __DATA_SOURCE_MANAGER_H_

#include "../browser-connection.h"
#include "data-source.h"

G_BEGIN_DECLS

#define DATA_SOURCE_MANAGER_TYPE          (data_source_manager_get_type())
#define DATA_SOURCE_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, data_source_manager_get_type(), DataSourceManager)
#define DATA_SOURCE_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, data_source_manager_get_type (), DataSourceManagerClass)
#define IS_DATA_SOURCE_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, data_source_manager_get_type ())

typedef struct _DataSourceManager DataSourceManager;
typedef struct _DataSourceManagerClass DataSourceManagerClass;
typedef struct _DataSourceManagerPrivate DataSourceManagerPrivate;

/* struct for the object's data */
struct _DataSourceManager
{
	GObject            object;
	DataSourceManagerPrivate *priv;
};

/* struct for the object's class */
struct _DataSourceManagerClass
{
	GObjectClass       parent_class;

	/* signals */
	void             (*changed) (DataSourceManager *mgr);
};

GType               data_source_manager_get_type            (void) G_GNUC_CONST;

DataSourceManager  *data_source_manager_new                 (BrowserConnection *bcnc);
void                data_source_manager_add_source          (DataSourceManager *mgr, DataSource *source);
void                data_source_manager_remove_source       (DataSourceManager *mgr, DataSource *source);
void                data_source_manager_remove_all          (DataSourceManager *mgr);

GdaSet              *data_source_manager_get_params (DataSourceManager *mgr);
BrowserConnection   *data_source_manager_get_browser_cnc (DataSourceManager *mgr);

GArray              *data_source_manager_get_sources_array (DataSourceManager *mgr, GError **error);
void                 data_source_manager_destroy_sources_array (GArray *array);


G_END_DECLS

#endif
