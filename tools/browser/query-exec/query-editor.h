/* GNOME DB library
 * Copyright (C) 1999 - 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __QUERY_EDITOR_H__
#define __QUERY_EDITOR_H__

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define QUERY_TYPE_EDITOR            (query_editor_get_type())
#define QUERY_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, QUERY_TYPE_EDITOR, QueryEditor))
#define QUERY_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, QUERY_TYPE_EDITOR, QueryEditorClass))
#define QUERY_IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, QUERY_TYPE_EDITOR))
#define QUERY_IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_TYPE_EDITOR))

typedef struct _QueryEditor        QueryEditor;
typedef struct _QueryEditorClass   QueryEditorClass;
typedef struct _QueryEditorPrivate QueryEditorPrivate;

struct _QueryEditor {
	GtkVBox parent;
	QueryEditorPrivate *priv;
};

struct _QueryEditorClass {
	GtkVBoxClass parent_class;

	/* signals */
	void (* text_changed) (QueryEditor editor);
};

#define QUERY_EDITOR_LANGUAGE_SQL "sql"

GType      query_editor_get_type (void) G_GNUC_CONST;
GtkWidget *query_editor_new (void);

gboolean   query_editor_get_editable (QueryEditor *editor);
void       query_editor_set_editable (QueryEditor *editor, gboolean editable);
gboolean   query_editor_get_highlight (QueryEditor *editor);
void       query_editor_set_highlight (QueryEditor *editor, gboolean highlight);
void       query_editor_set_text (QueryEditor *editor, const gchar *text, gint len);
gchar     *query_editor_get_all_text (QueryEditor *editor);
gboolean   query_editor_load_from_file (QueryEditor *editor, const gchar *filename);
gboolean   query_editor_save_to_file (QueryEditor *editor, const gchar *filename);

void       query_editor_copy_clipboard (QueryEditor *editor);
void       query_editor_cut_clipboard (QueryEditor *editor);
void       query_editor_paste_clipboard (QueryEditor *editor);

G_END_DECLS

#endif
