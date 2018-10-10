/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDAUI_RT_EDITOR__
#define __GDAUI_RT_EDITOR__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gdaui_rt_editor_error_quark (void);
#define GDAUI_RT_EDITOR_ERROR gdaui_rt_editor_error_quark ()

typedef enum {
	GDAUI_RT_EDITOR_GENERAL_ERROR
} GdauiRtEditorError;


#define GDAUI_TYPE_RT_EDITOR          (gdaui_rt_editor_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiRtEditor, gdaui_rt_editor, GDAUI, RT_EDITOR, GtkBox)
/* struct for the object's class */
struct _GdauiRtEditorClass
{
	GtkBoxClass         parent_class;

	/* signals */
	void             (* changed)     (GdauiRtEditor *editor);
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-rt-editor
 * @short_description: Rich text editor which uses a subset of the <ulink url="http://www.txt2tags.org/markup.html">txt2tags</ulink> markup.
 * @title: GdauiRtEditor
 * @stability: Stable
 * @Image: vi-rte.png
 * @see_also: 
 *
 * The text entered in the editor can be formatted using bold, underline, title, ... attributes
 * and then extracted using a subset of the <ulink url="http://www.txt2tags.org/markup.html">txt2tags</ulink>
 * markup. Use this widget to edit textual fields where some markup is desirable to organize the text.
 *
 * For example the real text used to obtain the formatting in the figure is:
 * <programlisting>
 *blah //italic// blah.
 *and ** BOLD!//both italic and bold// Bold!**
 *Nice Picture: [[[R2RrUAA...y8vLy8tYQwAA]]] Yes
 *- List item --One--
 *- List item **Two**
 * - sub1
 * - sub2</programlisting>
 * where the picture's serialized data has been truncated here for readability
 * (between the [[[ and ]]] markers). Pictures are usually inserted using the incorporated
 * tollbar and not y hand (even though it's possible).
 */

GtkWidget *gdaui_rt_editor_new                   (void);
gchar     *gdaui_rt_editor_get_contents          (GdauiRtEditor *editor);
void       gdaui_rt_editor_set_contents          (GdauiRtEditor *editor, const gchar *markup, gint length);
void       gdaui_rt_editor_set_editable          (GdauiRtEditor *editor, gboolean editable);

G_END_DECLS

#endif



