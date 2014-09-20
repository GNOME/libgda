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

#ifndef __XML_SPEC_EDITOR_H__
#define __XML_SPEC_EDITOR_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"
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
	GtkBox parent;
	XmlSpecEditorPrivate *priv;
};

struct _XmlSpecEditorClass {
	GtkBoxClass parent_class;
};

GType          xml_spec_editor_get_type     (void) G_GNUC_CONST;
GtkWidget     *xml_spec_editor_new          (DataSourceManager *mgr);

void           xml_spec_editor_set_xml_text (XmlSpecEditor *sped, const gchar *xml);
gchar         *xml_spec_editor_get_xml_text (XmlSpecEditor *sped);

G_END_DECLS

#endif
