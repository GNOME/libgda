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

#ifndef __TABLE_RELATIONS_H__
#define __TABLE_RELATIONS_H__


#include "table-info.h"

G_BEGIN_DECLS

#define TABLE_RELATIONS_TYPE            (table_relations_get_type())
#define TABLE_RELATIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, TABLE_RELATIONS_TYPE, TableRelations))
#define TABLE_RELATIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, TABLE_RELATIONS_TYPE, TableRelationsClass))
#define IS_TABLE_RELATIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, TABLE_RELATIONS_TYPE))
#define IS_TABLE_RELATIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TABLE_RELATIONS_TYPE))

typedef struct _TableRelations        TableRelations;
typedef struct _TableRelationsClass   TableRelationsClass;
typedef struct _TableRelationsPrivate TableRelationsPrivate;

struct _TableRelations {
	GtkBox               parent;
	TableRelationsPrivate *priv;
};

struct _TableRelationsClass {
	GtkBoxClass          parent_class;
};

GType                    table_relations_get_type (void) G_GNUC_CONST;

GtkWidget               *table_relations_new      (TableInfo *tinfo);

G_END_DECLS

#endif
