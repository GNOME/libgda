/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_PROVIDER_SPEC_EDITOR_H__
#define __GDAUI_PROVIDER_SPEC_EDITOR_H__

#include <libgda-ui/gdaui-basic-form.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_PROVIDER_SPEC_EDITOR            (_gdaui_provider_spec_editor_get_type())
#define GDAUI_PROVIDER_SPEC_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_PROVIDER_SPEC_EDITOR, GdauiProviderSpecEditor))
#define GDAUI_PROVIDER_SPEC_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_PROVIDER_SPEC_EDITOR, GdauiProviderSpecEditorClass))
#define GDAUI_IS_PROVIDER_SPEC_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_PROVIDER_SPEC_EDITOR))
#define GDAUI_IS_PROVIDER_SPEC_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_PROVIDER_SPEC_EDITOR))

typedef struct _GdauiProviderSpecEditor        GdauiProviderSpecEditor;
typedef struct _GdauiProviderSpecEditorClass   GdauiProviderSpecEditorClass;
typedef struct _GdauiProviderSpecEditorPrivate GdauiProviderSpecEditorPrivate;

struct _GdauiProviderSpecEditor {
	GtkBox                box;
	GdauiProviderSpecEditorPrivate *priv;
};

struct _GdauiProviderSpecEditorClass {
	GtkBoxClass           parent_class;

	/* signals */
	void                (* changed) (GdauiProviderSpecEditor *spec);
};

GType       _gdaui_provider_spec_editor_get_type     (void) G_GNUC_CONST;
GtkWidget  *_gdaui_provider_spec_editor_new          (const gchar *provider);

void        _gdaui_provider_spec_editor_set_provider (GdauiProviderSpecEditor *spec, const gchar *provider);
gboolean    _gdaui_provider_spec_editor_is_valid     (GdauiProviderSpecEditor *spec);
gchar      *_gdaui_provider_spec_editor_get_specs    (GdauiProviderSpecEditor *spec);
void        _gdaui_provider_spec_editor_set_specs    (GdauiProviderSpecEditor *spec, const gchar *specs_string);

void        _gdaui_provider_spec_editor_add_to_size_group (GdauiProviderSpecEditor *spec, GtkSizeGroup *size_group,
							   GdauiBasicFormPart part);

G_END_DECLS

#endif
