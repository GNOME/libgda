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

#ifndef __TABLE_INFO_H__
#define __TABLE_INFO_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"

G_BEGIN_DECLS

#define TABLE_INFO_TYPE            (table_info_get_type())
#define TABLE_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, TABLE_INFO_TYPE, TableInfo))
#define TABLE_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, TABLE_INFO_TYPE, TableInfoClass))
#define IS_TABLE_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, TABLE_INFO_TYPE))
#define IS_TABLE_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TABLE_INFO_TYPE))

typedef struct _TableInfo        TableInfo;
typedef struct _TableInfoClass   TableInfoClass;
typedef struct _TableInfoPrivate TableInfoPrivate;

struct _TableInfo {
	GtkBox               parent;
	TableInfoPrivate     *priv;
};

struct _TableInfoClass {
	GtkBoxClass          parent_class;
};

GType                    table_info_get_type (void) G_GNUC_CONST;

GtkWidget               *table_info_new      (TConnection *tcnc,
					      const gchar *schema, const gchar *table);
const gchar             *table_info_get_table_schema (TableInfo *table_info);
const gchar             *table_info_get_table_name (TableInfo *table_info);
TConnection             *table_info_get_connection (TableInfo *table_info);

G_END_DECLS

#endif
