/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __BROWSER_VIRTUAL_CONNECTION_H_
#define __BROWSER_VIRTUAL_CONNECTION_H_

#include <gtk/gtk.h>
#include "browser-connection.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_VIRTUAL_CONNECTION          (browser_virtual_connection_get_type())
#define BROWSER_VIRTUAL_CONNECTION(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_virtual_connection_get_type(), BrowserVirtualConnection)
#define BROWSER_VIRTUAL_CONNECTION_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_virtual_connection_get_type (), BrowserVirtualConnectionClass)
#define BROWSER_IS_VIRTUAL_CONNECTION(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_virtual_connection_get_type ())

typedef struct _BrowserVirtualConnection BrowserVirtualConnection;
typedef struct _BrowserVirtualConnectionPrivate BrowserVirtualConnectionPrivate;
typedef struct _BrowserVirtualConnectionClass BrowserVirtualConnectionClass;

/* struct for the object's data */
struct _BrowserVirtualConnection
{
	BrowserConnection                object;
	BrowserVirtualConnectionPrivate *priv;
};

/* struct for the object's class */
struct _BrowserVirtualConnectionClass
{
	BrowserConnectionClass           parent_class;
};

/* parts types */
typedef enum {
	BROWSER_VIRTUAL_CONNECTION_PART_MODEL,
	BROWSER_VIRTUAL_CONNECTION_PART_CNC,
} BrowserVirtualConnectionType;

/* part: a table from a GdaDataModel */
typedef struct {
	gchar *table_name;
	GdaDataModel *model;	
} BrowserVirtualConnectionModel;

/* part: tables from a BrowserConnection */
typedef struct {
	gchar *table_schema;
	BrowserConnection *source_cnc;
} BrowserVirtualConnectionCnc;

/* generic part */
typedef struct {
	BrowserVirtualConnectionType part_type;
	union {
		BrowserVirtualConnectionModel model;
		BrowserVirtualConnectionCnc   cnc;
	} u;
} BrowserVirtualConnectionPart;
BrowserVirtualConnectionPart *browser_virtual_connection_part_copy (const BrowserVirtualConnectionPart *part);
void                          browser_virtual_connection_part_free (BrowserVirtualConnectionPart *part);

/* specs */
typedef struct {
	GSList *parts;
} BrowserVirtualConnectionSpecs;
BrowserVirtualConnectionSpecs *browser_virtual_connection_specs_copy (const BrowserVirtualConnectionSpecs *specs);
void                           browser_virtual_connection_specs_free (BrowserVirtualConnectionSpecs *specs);

GType              browser_virtual_connection_get_type               (void) G_GNUC_CONST;
BrowserConnection *browser_virtual_connection_new                    (const BrowserVirtualConnectionSpecs *specs,
								      GError **error);
gboolean           browser_virtual_connection_modify_specs           (BrowserVirtualConnection *bcnc,
								      const BrowserVirtualConnectionSpecs *new_specs,
								      GError **error);
G_END_DECLS

#endif
