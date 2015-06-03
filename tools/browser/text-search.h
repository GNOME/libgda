/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __TEXT_EDITOR_H__
#define __TEXT_EDITOR_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define TEXT_SEARCH_TYPE            (text_search_get_type())
#define TEXT_SEARCH(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, TEXT_SEARCH_TYPE, TextSearch))
#define TEXT_SEARCH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, TEXT_SEARCH_TYPE, TextSearchClass))
#define IS_TEXT_SEARCH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, TEXT_SEARCH_TYPE))
#define IS_TEXT_SEARCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEXT_SEARCH_TYPE))

typedef struct _TextSearch        TextSearch;
typedef struct _TextSearchClass   TextSearchClass;
typedef struct _TextSearchPrivate TextSearchPrivate;

struct _TextSearch {
	GtkBox            parent;
	TextSearchPrivate *priv;
};

struct _TextSearchClass {
	GtkBoxClass         parent_class;
};

GType        text_search_get_type       (void) G_GNUC_CONST;
GtkWidget   *text_search_new            (GtkTextView *view);
void         text_search_rerun          (TextSearch *tsearch);

G_END_DECLS

#endif
