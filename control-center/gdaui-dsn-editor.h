/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DSN_EDITOR_H__
#define __GDAUI_DSN_EDITOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DSN_EDITOR            (gdaui_dsn_editor_get_type())
#define GDAUI_DSN_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DSN_EDITOR, GdauiDsnEditor))
#define GDAUI_DSN_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_DSN_EDITOR, GdauiDsnEditorClass))
#define GDAUI_IS_DSN_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DSN_EDITOR))
#define GDAUI_IS_DSN_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DSN_EDITOR))

typedef struct _GdauiDsnEditor        GdauiDsnEditor;
typedef struct _GdauiDsnEditorClass   GdauiDsnEditorClass;
typedef struct _GdauiDsnEditorPrivate GdauiDsnEditorPrivate;

struct _GdauiDsnEditor {
	GtkBox box;
	GdauiDsnEditorPrivate *priv;
};

struct _GdauiDsnEditorClass {
	GtkBoxClass parent_class;

	/* signals */
	void (* changed) (GdauiDsnEditor *config);
};

typedef enum {
	GDAUI_DSN_EDITOR_PANE_DEFINITION,
	GDAUI_DSN_EDITOR_PANE_PARAMS,
	GDAUI_DSN_EDITOR_PANE_AUTH
} GdauiDsnEditorPaneType;

GType                    gdaui_dsn_editor_get_type (void) G_GNUC_CONST;
GtkWidget               *gdaui_dsn_editor_new (void);
gboolean                 gdaui_dsn_editor_has_been_changed (GdauiDsnEditor *config);

const GdaDsnInfo        *gdaui_dsn_editor_get_dsn (GdauiDsnEditor *config);
void                     gdaui_dsn_editor_set_dsn (GdauiDsnEditor *config, const GdaDsnInfo *dsn_info);

void                     gdaui_dsn_editor_show_pane (GdauiDsnEditor *config, GdauiDsnEditorPaneType type);
gboolean                 gdaui_dsn_editor_need_authentication (GdauiDsnEditor *config);

G_END_DECLS

#endif
