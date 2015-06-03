/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __ENTRY_PROPERTIES_H__
#define __ENTRY_PROPERTIES_H__

#include "common/t-connection.h"

G_BEGIN_DECLS

#define ENTRY_PROPERTIES_TYPE            (entry_properties_get_type())
#define ENTRY_PROPERTIES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, ENTRY_PROPERTIES_TYPE, EntryProperties))
#define ENTRY_PROPERTIES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, ENTRY_PROPERTIES_TYPE, EntryPropertiesClass))
#define IS_ENTRY_PROPERTIES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, ENTRY_PROPERTIES_TYPE))
#define IS_ENTRY_PROPERTIES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ENTRY_PROPERTIES_TYPE))

typedef struct _EntryProperties        EntryProperties;
typedef struct _EntryPropertiesClass   EntryPropertiesClass;
typedef struct _EntryPropertiesPrivate EntryPropertiesPrivate;

struct _EntryProperties {
	GtkBox                 parent;
	EntryPropertiesPrivate *priv;
};

struct _EntryPropertiesClass {
	GtkBoxClass            parent_class;

	/* signals */
	void                  (*open_dn) (EntryProperties *eprop, const gchar *dn);
	void                  (*open_class) (EntryProperties *eprop, const gchar *classname);
};

GType                    entry_properties_get_type (void) G_GNUC_CONST;

GtkWidget               *entry_properties_new      (TConnection *tcnc);
void                     entry_properties_set_dn   (EntryProperties *eprop, const gchar *dn);

G_END_DECLS

#endif
