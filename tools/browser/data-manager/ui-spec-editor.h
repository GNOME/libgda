/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __UI_SPEC_EDITOR_H__
#define __UI_SPEC_EDITOR_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"
#include "data-source-manager.h"

G_BEGIN_DECLS

#define UI_SPEC_EDITOR_TYPE            (ui_spec_editor_get_type())
#define UI_SPEC_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, UI_SPEC_EDITOR_TYPE, UiSpecEditor))
#define UI_SPEC_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, UI_SPEC_EDITOR_TYPE, UiSpecEditorClass))
#define IS_UI_SPEC_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, UI_SPEC_EDITOR_TYPE))
#define IS_UI_SPEC_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UI_SPEC_EDITOR_TYPE))


typedef struct _UiSpecEditor        UiSpecEditor;
typedef struct _UiSpecEditorClass   UiSpecEditorClass;
typedef struct _UiSpecEditorPrivate UiSpecEditorPrivate;

struct _UiSpecEditor {
	GtkBox parent;
	UiSpecEditorPrivate *priv;
};

struct _UiSpecEditorClass {
	GtkBoxClass parent_class;
};

GType          ui_spec_editor_get_type      (void) G_GNUC_CONST;
GtkWidget     *ui_spec_editor_new           (DataSourceManager *mgr);
void           ui_spec_editor_select_source (UiSpecEditor *editor, DataSource *source);
DataSource    *ui_spec_editor_get_selected_source (UiSpecEditor *editor);

G_END_DECLS

#endif
