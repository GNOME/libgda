/* GNOME DB library
 * Copyright (C) 2008 The GNOME Foundation
 *
 * AUTHORS:
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

#ifndef __GDAUI_PROVIDER_AUTH_EDITOR_H__
#define __GDAUI_PROVIDER_AUTH_EDITOR_H__

#include <libgda-ui/gdaui-basic-form.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_PROVIDER_AUTH_EDITOR            (_gdaui_provider_auth_editor_get_type())
#define GDAUI_PROVIDER_AUTH_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_PROVIDER_AUTH_EDITOR, GdauiProviderAuthEditor))
#define GDAUI_PROVIDER_AUTH_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_PROVIDER_AUTH_EDITOR, GdauiProviderAuthEditorClass))
#define GDAUI_IS_PROVIDER_AUTH_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_PROVIDER_AUTH_EDITOR))
#define GDAUI_IS_PROVIDER_AUTH_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_PROVIDER_AUTH_EDITOR))

typedef struct _GdauiProviderAuthEditor        GdauiProviderAuthEditor;
typedef struct _GdauiProviderAuthEditorClass   GdauiProviderAuthEditorClass;
typedef struct _GdauiProviderAuthEditorPrivate GdauiProviderAuthEditorPrivate;

struct _GdauiProviderAuthEditor {
	GtkVBox                box;
	GdauiProviderAuthEditorPrivate *priv;
};

struct _GdauiProviderAuthEditorClass {
	GtkVBoxClass           parent_class;

	/* signals */
	void                (* changed) (GdauiProviderAuthEditor *auth);
};

GType       _gdaui_provider_auth_editor_get_type     (void) G_GNUC_CONST;
GtkWidget  *_gdaui_provider_auth_editor_new          (const gchar *provider);

void        _gdaui_provider_auth_editor_set_provider (GdauiProviderAuthEditor *auth, const gchar *provider);
gboolean    _gdaui_provider_auth_editor_is_valid     (GdauiProviderAuthEditor *auth);

gchar      *_gdaui_provider_auth_editor_get_auth     (GdauiProviderAuthEditor *auth);
void        _gdaui_provider_auth_editor_set_auth     (GdauiProviderAuthEditor *auth, const gchar *auth_string);

void        _gdaui_provider_auth_editor_add_to_size_group (GdauiProviderAuthEditor *auth, GtkSizeGroup *size_group,
							   GdauiBasicFormPart part);
G_END_DECLS

#endif
