/*
 * Copyright (C) 2011 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __FILTER_EDITOR_H__
#define __FILTER_EDITOR_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define FILTER_EDITOR_TYPE            (filter_editor_get_type())
#define FILTER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, FILTER_EDITOR_TYPE, FilterEditor))
#define FILTER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, FILTER_EDITOR_TYPE, FilterEditorClass))
#define IS_FILTER_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, FILTER_EDITOR_TYPE))
#define IS_FILTER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_EDITOR_TYPE))

typedef struct _FilterEditor        FilterEditor;
typedef struct _FilterEditorClass   FilterEditorClass;
typedef struct _FilterEditorPrivate FilterEditorPrivate;

struct _FilterEditor {
	GtkVBox              parent;
	FilterEditorPrivate *priv;
};

struct _FilterEditorClass {
	GtkVBoxClass         parent_class;
	
	/* signals */
	void               (*activate) (FilterEditor *feditor);
};

GType        filter_editor_get_type       (void) G_GNUC_CONST;

GtkWidget   *filter_editor_new            (BrowserConnection *bcnc);
void         filter_editor_clear          (FilterEditor *fedit);
void         filter_editor_set_settings   (FilterEditor *fedit,
					   const gchar *base_dn, const gchar *filter,
					   const gchar *attributes, GdaLdapSearchScope scope);
void         filter_editor_get_settings   (FilterEditor *fedit,
					   gchar **out_base_dn, gchar **out_filter,
					   gchar **out_attributes, GdaLdapSearchScope *out_scope);

G_END_DECLS

#endif
