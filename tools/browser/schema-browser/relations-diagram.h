/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __RELATIONS_DIAGRAM_H__
#define __RELATIONS_DIAGRAM_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"

G_BEGIN_DECLS

#define RELATIONS_DIAGRAM_TYPE            (relations_diagram_get_type())
#define RELATIONS_DIAGRAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, RELATIONS_DIAGRAM_TYPE, RelationsDiagram))
#define RELATIONS_DIAGRAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, RELATIONS_DIAGRAM_TYPE, RelationsDiagramClass))
#define IS_RELATIONS_DIAGRAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, RELATIONS_DIAGRAM_TYPE))
#define IS_RELATIONS_DIAGRAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RELATIONS_DIAGRAM_TYPE))

typedef struct _RelationsDiagram        RelationsDiagram;
typedef struct _RelationsDiagramClass   RelationsDiagramClass;
typedef struct _RelationsDiagramPrivate RelationsDiagramPrivate;

struct _RelationsDiagram {
	GtkBox                  parent;
	RelationsDiagramPrivate *priv;
};

struct _RelationsDiagramClass {
	GtkBoxClass             parent_class;
};

GType                    relations_diagram_get_type (void) G_GNUC_CONST;
GtkWidget               *relations_diagram_new (TConnection *tcnc);
GtkWidget               *relations_diagram_new_with_fav_id (TConnection *tcnc, gint fav_id, GError **error);
gint                     relations_diagram_get_fav_id (RelationsDiagram *diagram);

G_END_DECLS

#endif
