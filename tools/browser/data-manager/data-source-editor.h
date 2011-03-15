/* 
 * Copyright (C) 2010 Vivien Malerba
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
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


#ifndef __DATA_SOURCE_EDITOR_H_
#define __DATA_SOURCE_EDITOR_H_

#include "data-source.h"

G_BEGIN_DECLS

#define DATA_SOURCE_EDITOR_TYPE          (data_source_editor_get_type())
#define DATA_SOURCE_EDITOR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, data_source_editor_get_type(), DataSourceEditor)
#define DATA_SOURCE_EDITOR_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, data_source_editor_get_type (), DataSourceEditorClass)
#define IS_DATA_SOURCE_EDITOR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, data_source_editor_get_type ())

typedef struct _DataSourceEditor DataSourceEditor;
typedef struct _DataSourceEditorClass DataSourceEditorClass;
typedef struct _DataSourceEditorPrivate DataSourceEditorPrivate;

/* struct for the object's data */
struct _DataSourceEditor
{
	GtkVBox                  object;
	DataSourceEditorPrivate *priv;
};

/* struct for the object's class */
struct _DataSourceEditorClass
{
	GtkVBoxClass             parent_class;

	/* signals */
	/*void             (*changed) (DataSourceEditor *mgr);*/
};

GType         data_source_editor_get_type            (void) G_GNUC_CONST;

GtkWidget    *data_source_editor_new                 (void);
void          data_source_editor_display_source      (DataSourceEditor *editor, DataSource *source);
void          data_source_editor_set_readonly        (DataSourceEditor *editor);

G_END_DECLS

#endif
