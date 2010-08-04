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

#ifndef __XML_SPEC_EDITOR_H__
#define __XML_SPEC_EDITOR_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"
#include "data-source-manager.h"

G_BEGIN_DECLS

#define XML_SPEC_EDITOR_TYPE            (xml_spec_editor_get_type())
#define XML_SPEC_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, XML_SPEC_EDITOR_TYPE, XmlSpecEditor))
#define XML_SPEC_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, XML_SPEC_EDITOR_TYPE, XmlSpecEditorClass))
#define IS_XML_SPEC_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, XML_SPEC_EDITOR_TYPE))
#define IS_XML_SPEC_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XML_SPEC_EDITOR_TYPE))


typedef struct _XmlSpecEditor        XmlSpecEditor;
typedef struct _XmlSpecEditorClass   XmlSpecEditorClass;
typedef struct _XmlSpecEditorPrivate XmlSpecEditorPrivate;

struct _XmlSpecEditor {
	GtkVBox parent;
	XmlSpecEditorPrivate *priv;
};

struct _XmlSpecEditorClass {
	GtkVBoxClass parent_class;
};

GType          xml_spec_editor_get_type     (void) G_GNUC_CONST;
GtkWidget     *xml_spec_editor_new          (DataSourceManager *mgr);

void           xml_spec_editor_set_xml_text (XmlSpecEditor *sped, const gchar *xml);
gchar         *xml_spec_editor_get_xml_text (XmlSpecEditor *sped);

G_END_DECLS

#endif
