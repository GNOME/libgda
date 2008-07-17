/* gda-ddl-creator.h
 *
 * Copyright (C) 2008 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDA_DDL_CREATOR_H_
#define __GDA_DDL_CREATOR_H_

#include <glib-object.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

typedef struct _GdaDDLCreator        GdaDDLCreator;
typedef struct _GdaDDLCreatorClass   GdaDDLCreatorClass;
typedef struct _GdaDDLCreatorPrivate GdaDDLCreatorPrivate;
typedef struct _GdaDDLCreatorClassPrivate GdaDDLCreatorClassPrivate;

#define GDA_TYPE_DDL_CREATOR          (gda_ddl_creator_get_type())
#define GDA_DDL_CREATOR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_ddl_creator_get_type(), GdaDDLCreator)
#define GDA_DDL_CREATOR_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_ddl_creator_get_type (), GdaDDLCreatorClass)
#define GDA_IS_DDL_CREATOR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_ddl_creator_get_type ())

/* error reporting */
extern GQuark gda_ddl_creator_error_quark (void);
#define GDA_DDL_CREATOR_ERROR gda_ddl_creator_error_quark ()

typedef enum {
	GDA_DDL_CREATOR_SPECFILE_NOT_FOUND_ERROR,
	GDA_DDL_CREATOR_INCORRECT_SCHEMA_ERROR,
	GDA_DDL_CREATOR_NO_CONNECTION_ERROR,
	GDA_DDL_CREATOR_INTERNAL_ERROR, 

	GDA_DDL_CREATOR_UNSUPPORTED_PROVIDER_ERROR,
	GDA_DDL_CREATOR_DDL_CONTEXT_ERROR,
	GDA_DDL_CREATOR_MODIFY_CONTENTS_ERROR,
	GDA_DDL_CREATOR_EXTRACT_SQL_ERROR,
	GDA_DDL_CREATOR_ATTRIBUTE_NOT_FOUND_ERROR,
	GDA_DDL_CREATOR_ATTRIBUTE_ERROR,
	GDA_DDL_CREATOR_SCHEMA_OBJECT_NOT_FOUND_ERROR,
	GDA_DDL_CREATOR_SCHEMA_OBJECT_CONFLICT_ERROR,
	GDA_DDL_CREATOR_SCHEMA_OBJECT_DESCR_ERROR,
	GDA_DDL_CREATOR_TRANSACTION_ALREADY_STARTED_ERROR
} GdaDDLCreatorError;


/* struct for the object's data */
struct _GdaDDLCreator
{
	GObject               object;
	GdaDDLCreatorPrivate  *priv;
};

/* struct for the object's class */
struct _GdaDDLCreatorClass
{
	GObjectClass              parent_class;
};

GType             gda_ddl_creator_get_type             (void) G_GNUC_CONST;
GdaDDLCreator    *gda_ddl_creator_new_with_file        (const gchar *xml_spec_file, GError **error);

void              gda_ddl_creator_set_connection       (GdaDDLCreator *ddlc, GdaConnection *cnc);
gchar            *gda_ddl_creator_get_sql_for_create_objects (GdaDDLCreator *ddlc, GError **error);

/* must use a transaction, fails if it cannot start one */
gboolean          gda_ddl_creator_create_objects       (GdaDDLCreator *ddlc, GError **error);
gboolean          gda_ddl_creator_delete_objects       (GdaDDLCreator *ddlc, GError **error);


/* ideas:
GdaDDLCreator    *gda_ddl_creator_new_with_meta_struct (GdaMetaStruct *mstruct);

gboolean          gda_ddl_creator_update_objects       (GdaDDLCreator ddlc, GError **error);
*/

G_END_DECLS

#endif
