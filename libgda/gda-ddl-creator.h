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
#include "gda-ddl-view.h"
#include "gda-server-operation.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_CREATOR (gda_ddl_creator_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlCreator, gda_ddl_creator, GDA, DDL_CREATOR, GObject)

struct _GdaDdlCreatorClass {
    GObjectClass parent;
};

/**
 * GdaDdlCreatorError:
 * @GDA_DDL_CREATOR_CONTEXT_NULL: Context is %NULL. Should not be %NULL for normal operation.
 * @GDA_DDL_CREATOR_DOC_NULL: #xmlDocPtr is %NULL. 
 * @GDA_DDL_CREATOR_INVALID_XML: Sets if xml check fails. Xml file structure doesn't match with DTD
 * file
 * @GDA_DDL_CREATOR_INVALID_SCHEMA: Sets if the used schema is invalid. 
 * @GDA_DDL_CREATOR_SERVER_OPERATION: Sets if server operation is %NULL. 
 * @GDA_DDL_CREATOR_FILE_READ: Sets if xml file is not readable
 * @GDA_DDL_CREATOR_PARSE_CONTEXT: Sets if an error with context during parsing an xml file
 * @GDA_DDL_CREATOR_PARSE: Sets if parsing reports an error
 * @GDA_DDL_CREATOR_PARSE_CHUNK: If set, error with parse chunk algorithm.
 * @GDA_DDL_CREATOR_CONNECTION_CLOSED: Connection is not open.
 *
 * These error are primary for developers to troubleshoot the problem rather than for user. 
 */
typedef  enum {
    GDA_DDL_CREATOR_CONTEXT_NULL,
    GDA_DDL_CREATOR_DOC_NULL,
    GDA_DDL_CREATOR_INVALID_XML,
    GDA_DDL_CREATOR_INVALID_SCHEMA,
    GDA_DDL_CREATOR_SERVER_OPERATION,
    GDA_DDL_CREATOR_FILE_READ,
    GDA_DDL_CREATOR_PARSE_CONTEXT,
    GDA_DDL_CREATOR_PARSE,
    GDA_DDL_CREATOR_PARSE_CHUNK,
    GDA_DDL_CREATOR_CONNECTION_CLOSED
} GdaDdlCreatorError;

#define GDA_DDL_CREATOR_ERROR gda_ddl_creator_error_quark()
GQuark gda_ddl_creator_error_quark (void);

GdaDdlCreator		*gda_ddl_creator_new        (void);

gboolean		     gda_ddl_creator_parse_file_from_path	(GdaDdlCreator *self,
                                                       const gchar *xmlfile,
                                                       GError **error);

gboolean         gda_ddl_creator_parse_file (GdaDdlCreator *self,
                                             GFile *xmlfile,
                                             GError **error);

GList           *gda_ddl_creator_get_tables	(GdaDdlCreator *self);
GList           *gda_ddl_creator_get_views	(GdaDdlCreator *self);

gboolean         gda_ddl_creator_parse_cnc (GdaDdlCreator *self,
                                            GdaConnection *cnc,
                                            GError **error);

void             gda_ddl_creator_append_table (GdaDdlCreator *self,
                                               GdaDdlTable *table);

void             gda_ddl_creator_append_view (GdaDdlCreator *self,
                                              GdaDdlView *view);

gboolean         gda_ddl_creator_perform_operation (GdaDdlCreator *self,
                                                    GdaConnection *cnc,
                                                    GError **error);

gboolean         gda_ddl_creator_write_to_file (GdaDdlCreator *self,
                                                GFile *file,
                                                GError **error);

gboolean         gda_ddl_creator_write_to_path (GdaDdlCreator *self,
                                                const gchar *path,
                                                GError **error);

gboolean         gda_ddl_creator_validate_file_from_path (const gchar *xmlfile,
                                                          GError **error);

void             gda_ddl_creator_set_connection (GdaDdlCreator *self,
                                                 GdaConnection *cnc);

GdaConnection   *gda_ddl_creator_get_connection (GdaDdlCreator *self);
G_END_DECLS

#endif /* GDA_DDL_CREATOR_H */

