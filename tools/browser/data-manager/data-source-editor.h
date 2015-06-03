/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
	GtkBox                  object;
	DataSourceEditorPrivate *priv;
};

/* struct for the object's class */
struct _DataSourceEditorClass
{
	GtkBoxClass             parent_class;

	/* signals */
	/*void             (*changed) (DataSourceEditor *mgr);*/
};

GType         data_source_editor_get_type            (void) G_GNUC_CONST;

GtkWidget    *data_source_editor_new                 (void);
void          data_source_editor_display_source      (DataSourceEditor *editor, DataSource *source);
void          data_source_editor_set_readonly        (DataSourceEditor *editor);

G_END_DECLS

#endif
