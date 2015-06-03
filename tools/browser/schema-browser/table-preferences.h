/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __TABLE_PREFERENCES_H__
#define __TABLE_PREFERENCES_H__


#include "table-info.h"

G_BEGIN_DECLS

#define TABLE_PREFERENCES_TYPE            (table_preferences_get_type())
#define TABLE_PREFERENCES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, TABLE_PREFERENCES_TYPE, TablePreferences))
#define TABLE_PREFERENCES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, TABLE_PREFERENCES_TYPE, TablePreferencesClass))
#define IS_TABLE_PREFERENCES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, TABLE_PREFERENCES_TYPE))
#define IS_TABLE_PREFERENCES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TABLE_PREFERENCES_TYPE))

typedef struct _TablePreferences        TablePreferences;
typedef struct _TablePreferencesClass   TablePreferencesClass;
typedef struct _TablePreferencesPrivate TablePreferencesPrivate;

struct _TablePreferences {
	GtkBox               parent;
	TablePreferencesPrivate *priv;
};

struct _TablePreferencesClass {
	GtkBoxClass          parent_class;
};

GType                    table_preferences_get_type (void) G_GNUC_CONST;

GtkWidget               *table_preferences_new      (TableInfo *tinfo);

G_END_DECLS

#endif
