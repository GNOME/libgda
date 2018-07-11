/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __RAW_DDL_CREATOR_H_
#define __RAW_DDL_CREATOR_H_

#include <glib-object.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

typedef struct _RawDDLCreator        RawDDLCreator;
typedef struct _RawDDLCreatorClass   RawDDLCreatorClass;
typedef struct _RawDDLCreatorPrivate RawDDLCreatorPrivate;
typedef struct _RawDDLCreatorClassPrivate RawDDLCreatorClassPrivate;

#define RAW_TYPE_DDL_CREATOR          (raw_ddl_creator_get_type())
#define RAW_DDL_CREATOR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, raw_ddl_creator_get_type(), RawDDLCreator)
#define RAW_DDL_CREATOR_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, raw_ddl_creator_get_type (), RawDDLCreatorClass)
#define RAW_IS_DDL_CREATOR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, raw_ddl_creator_get_type ())

/* error reporting */
extern GQuark raw_ddl_creator_error_quark (void);
#define RAW_DDL_CREATOR_ERROR raw_ddl_creator_error_quark ()

typedef enum {
	RAW_DDL_CREATOR_SPECFILE_NOT_FOUND_ERROR,
	RAW_DDL_CREATOR_INCORRECT_SCHEMA_ERROR,
	RAW_DDL_CREATOR_NO_CONNECTION_ERROR
} RawDDLCreatorError;


/* struct for the object's data */
struct _RawDDLCreator
{
	GObject               object;
	RawDDLCreatorPrivate  *priv;
};

/* struct for the object's class */
struct _RawDDLCreatorClass
{
	GObjectClass              parent_class;
};

GType             raw_ddl_creator_get_type             (void) G_GNUC_CONST;
RawDDLCreator    *raw_ddl_creator_new                  (void);

gboolean          raw_ddl_creator_set_dest_from_file   (RawDDLCreator *ddlc, const gchar *xml_spec_file,
							GError **error);
//gboolean          raw_ddl_creator_set_dest_from_meta   (RawDDLCreator *ddlc, GdaMetaStruct *struct, GError **error);

void              raw_ddl_creator_set_connection       (RawDDLCreator *ddlc, GdaConnection *cnc);


gchar            *raw_ddl_creator_get_sql              (RawDDLCreator *ddlc, GError **error);
gboolean          raw_ddl_creator_execute              (RawDDLCreator *ddlc, GError **error);

G_END_DECLS

#endif
