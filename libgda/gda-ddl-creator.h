/* gda-ddl-creator.h
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef GDA_DDL_CREATOR_H
#define GDA_DDL_CREATOR_H

#include <glib-object.h>
#include <gmodule.h>
#include "gda-connection.h"
#include "gda-ddl-table.h"
#include "gda-server-operation.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_CREATOR (gda_ddl_creator_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlCreator, gda_ddl_creator, GDA, DDL_CREATOR, GObject)

struct _GdaDdlCreatorClass {
    GObjectClass parent;
};

typedef  enum {
    GDA_DDL_CREATOR_CONTEXT_NULL,
    GDA_DDL_CREATOR_DOC_NULL,
    GDA_DDL_CREATOR_UNVALID_XML,
    GDA_DDL_CREATOR_UNVALID_SCHEMA,
    GDA_DDL_CREATOR_SERVER_OPERATION
} GdaDdlCreatorError;

#define GDA_DDL_CREATOR_ERROR gda_ddl_creator_error_quark()
GQuark gda_ddl_creator_error_quark (void);

typedef enum {
    GDA_DDL_CREATOR_UPDATE,
    GDA_DDL_CREATOR_REPLACE
}GdaDdlCreatorMode;


GdaDdlCreator		*gda_ddl_creator_new        (void);
void             gda_ddl_creator_free       (GdaDdlCreator *self);

gboolean		     gda_ddl_creator_parse_file_from_path	(GdaDdlCreator *self,
                                                       const gchar *xmlfile,
                                                       GError **error);

const GList     *gda_ddl_creator_get_tables	(GdaDdlCreator *self);
const GList     *gda_ddl_creator_get_views	(GdaDdlCreator *self);

gboolean         gda_ddl_creator_parse_cnc (GdaDdlCreator *self,
                                            GdaConnection *cnc,
                                            GError **error);

void             gda_ddl_creator_append_table (GdaDdlCreator *self,
                                               const GdaDdlTable *table);

void             gda_ddl_creator_append_view (GdaDdlCreator *self,
                                              const GdaDdlTable *view);

gboolean         gda_ddl_creator_perform_operation (GdaDdlCreator *self,
                                                    GdaConnection *cnc,
                                                    GError **error);

gboolean         gda_ddl_creator_write_to_file (GdaDdlCreator *self,
                                                GFile *path,
                                                GError **error);

gboolean         gda_ddl_creator_write_to_path (GdaDdlCreator *self,
                                                const gchar *path,
                                                GError **error);

G_END_DECLS

#endif /* GDA_DDL_CREATOR_H */

