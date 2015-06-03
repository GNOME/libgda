/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __SCHEMA_BROWSER_PERSPECTIVE_H_
#define __SCHEMA_BROWSER_PERSPECTIVE_H_

#include <glib-object.h>
#include "../browser-perspective.h"

G_BEGIN_DECLS

#define TYPE_SCHEMA_BROWSER_PERSPECTIVE          (schema_browser_perspective_get_type())
#define SCHEMA_BROWSER_PERSPECTIVE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, schema_browser_perspective_get_type(), SchemaBrowserPerspective)
#define SCHEMA_BROWSER_PERSPECTIVE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, schema_browser_perspective_get_type (), SchemaBrowserPerspectiveClass)
#define IS_SCHEMA_BROWSER_PERSPECTIVE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, schema_browser_perspective_get_type ())

typedef struct _SchemaBrowserPerspective SchemaBrowserPerspective;
typedef struct _SchemaBrowserPerspectiveClass SchemaBrowserPerspectiveClass;
typedef struct _SchemaBrowserPerspectivePrivate SchemaBrowserPerspectivePrivate;

/* struct for the object's data */
struct _SchemaBrowserPerspective
{
	GtkBox              object;
	SchemaBrowserPerspectivePrivate *priv;
};

/* struct for the object's class */
struct _SchemaBrowserPerspectiveClass
{
	GtkBoxClass         parent_class;
};

/**
 * SECTION:schema-browser-perspective
 * @short_description: Perspective to analyse the database's schema
 * @title: Schema Browser perspective
 * @stability: Stable
 * @see_also:
 */

GType                schema_browser_perspective_get_type               (void) G_GNUC_CONST;
BrowserPerspective  *schema_browser_perspective_new                    (BrowserWindow *bwin);
void                 schema_browser_perspective_display_table_info     (SchemaBrowserPerspective *bpers,
									const gchar *table_schema,
									const gchar *table_name,
									const gchar *table_short_name);
void                 schema_browser_perspective_display_diagram        (SchemaBrowserPerspective *bpers,
									gint fav_id);

G_END_DECLS

#endif
