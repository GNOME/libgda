/*
 * Copyright (C) 2010 The GNOME Foundation.
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

#ifndef __SPEC_EDITOR_H__
#define __SPEC_EDITOR_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"
#include "data-source-manager.h"

G_BEGIN_DECLS

#define SPEC_EDITOR_TYPE            (spec_editor_get_type())
#define SPEC_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, SPEC_EDITOR_TYPE, SpecEditor))
#define SPEC_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, SPEC_EDITOR_TYPE, SpecEditorClass))
#define IS_SPEC_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, SPEC_EDITOR_TYPE))
#define IS_SPEC_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPEC_EDITOR_TYPE))


typedef struct _SpecEditor        SpecEditor;
typedef struct _SpecEditorClass   SpecEditorClass;
typedef struct _SpecEditorPrivate SpecEditorPrivate;

struct _SpecEditor {
	GtkVBox parent;
	SpecEditorPrivate *priv;
};

struct _SpecEditorClass {
	GtkVBoxClass parent_class;
};

typedef enum {
	SPEC_EDITOR_XML,
	SPEC_EDITOR_UI
} SpecEditorMode;


GType       spec_editor_get_type   (void) G_GNUC_CONST;
SpecEditor *spec_editor_new        (DataSourceManager *mgr);

void        spec_editor_set_xml_text (SpecEditor *sped, const gchar *xml);
gchar      *spec_editor_get_xml_text (SpecEditor *sped);
void        spec_editor_set_mode     (SpecEditor *sped, SpecEditorMode mode);
SpecEditorMode spec_editor_get_mode  (SpecEditor *sped);

G_END_DECLS

#endif
