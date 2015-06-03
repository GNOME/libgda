/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __T_VIRTUAL_CONNECTION_H_
#define __T_VIRTUAL_CONNECTION_H_

#include "t-connection.h"

G_BEGIN_DECLS

#define T_TYPE_VIRTUAL_CONNECTION          (t_virtual_connection_get_type())
#define T_VIRTUAL_CONNECTION(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_virtual_connection_get_type(), TVirtualConnection)
#define T_VIRTUAL_CONNECTION_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_virtual_connection_get_type (), TVirtualConnectionClass)
#define T_IS_VIRTUAL_CONNECTION(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_virtual_connection_get_type ())

typedef struct _TVirtualConnection TVirtualConnection;
typedef struct _TVirtualConnectionPrivate TVirtualConnectionPrivate;
typedef struct _TVirtualConnectionClass TVirtualConnectionClass;

/* struct for the object's data */
struct _TVirtualConnection
{
	TConnection                object;
	TVirtualConnectionPrivate *priv;
};

/* struct for the object's class */
struct _TVirtualConnectionClass
{
	TConnectionClass           parent_class;
};

/* parts types */
typedef enum {
	T_VIRTUAL_CONNECTION_PART_MODEL,
	T_VIRTUAL_CONNECTION_PART_CNC,
} TVirtualConnectionType;

/* part: a table from a GdaDataModel */
typedef struct {
	gchar *table_name;
	GdaDataModel *model;	
} TVirtualConnectionModel;

/* part: tables from a TConnection */
typedef struct {
	gchar *table_schema;
	TConnection *source_cnc;
} TVirtualConnectionCnc;

/* generic part */
typedef struct {
	TVirtualConnectionType part_type;
	union {
		TVirtualConnectionModel model;
		TVirtualConnectionCnc   cnc;
	} u;
} TVirtualConnectionPart;
TVirtualConnectionPart *t_virtual_connection_part_copy (const TVirtualConnectionPart *part);
void                          t_virtual_connection_part_free (TVirtualConnectionPart *part);

/* specs */
typedef struct {
	GSList *parts;
} TVirtualConnectionSpecs;
TVirtualConnectionSpecs *t_virtual_connection_specs_copy (const TVirtualConnectionSpecs *specs);
void                     t_virtual_connection_specs_free (TVirtualConnectionSpecs *specs);

GType              t_virtual_connection_get_type               (void) G_GNUC_CONST;
TConnection       *t_virtual_connection_new                    (const TVirtualConnectionSpecs *specs,
								GError **error);
gboolean           t_virtual_connection_modify_specs           (TVirtualConnection *bcnc,
								const TVirtualConnectionSpecs *new_specs,
								GError **error);
G_END_DECLS

#endif
