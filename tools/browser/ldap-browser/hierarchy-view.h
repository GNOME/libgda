/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __HIERARCHY_VIEW_H__
#define __HIERARCHY_VIEW_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"

G_BEGIN_DECLS

#define HIERARCHY_VIEW_TYPE            (hierarchy_view_get_type())
#define HIERARCHY_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, HIERARCHY_VIEW_TYPE, HierarchyView))
#define HIERARCHY_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, HIERARCHY_VIEW_TYPE, HierarchyViewClass))
#define IS_HIERARCHY_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, HIERARCHY_VIEW_TYPE))
#define IS_HIERARCHY_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HIERARCHY_VIEW_TYPE))

typedef struct _HierarchyView        HierarchyView;
typedef struct _HierarchyViewClass   HierarchyViewClass;
typedef struct _HierarchyViewPrivate HierarchyViewPrivate;

struct _HierarchyView {
	GtkTreeView           parent;
	HierarchyViewPrivate *priv;
};

struct _HierarchyViewClass {
	GtkTreeViewClass      parent_class;
};

GType        hierarchy_view_get_type       (void) G_GNUC_CONST;

GtkWidget   *hierarchy_view_new            (TConnection *tcnc, const gchar *dn);
const gchar *hierarchy_view_get_current_dn (HierarchyView *hierarchy_view, const gchar **out_current_cn);
void         hierarchy_view_set_current_dn (HierarchyView *hierarchy_view, const gchar *dn);

G_END_DECLS

#endif
