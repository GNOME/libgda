/* gda-db-catalog.h
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

#ifndef GDA_DB_CATALOG_H
#define GDA_DB_CATALOG_H

#include <glib-object.h>
#include <gmodule.h>
#include "gda-db-table.h"
#include "gda-db-view.h"
#include "gda-server-operation.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define GDA_TYPE_DB_CATALOG (gda_db_catalog_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbCatalog, gda_db_catalog, GDA, DB_CATALOG, GObject)

struct _GdaDbCatalogClass
{
    GObjectClass parent;
};

/**
 * GdaDbCatalogError:
 * @GDA_DB_CATALOG_CONTEXT_NULL: Context is %NULL. Should not be %NULL for normal operation.
 * @GDA_DB_CATALOG_DOC_NULL: #xmlDocPtr is %NULL.
 * @GDA_DB_CATALOG_INVALID_XML: Sets if xml check fails. Xml file structure doesn't match with DTD
 * file
 * @GDA_DB_CATALOG_INVALID_SCHEMA: Sets if the used schema is invalid.
 * @GDA_DB_CATALOG_SERVER_OPERATION: Sets if server operation is %NULL.
 * @GDA_DB_CATALOG_FILE_READ: Sets if xml file is not readable
 * @GDA_DB_CATALOG_PARSE_CONTEXT: Sets if an error with context during parsing an xml file
 * @GDA_DB_CATALOG_PARSE: Sets if parsing reports an error
 * @GDA_DB_CATALOG_PARSE_CHUNK: If set, error with parse chunk algorithm.
 * @GDA_DB_CATALOG_CONNECTION_CLOSED: Connection is not open.
 *
 * These error are primary for developers to troubleshoot the problem rather than for user.
 */
typedef  enum {
    GDA_DB_CATALOG_CONTEXT_NULL,
    GDA_DB_CATALOG_DOC_NULL,
    GDA_DB_CATALOG_INVALID_XML,
    GDA_DB_CATALOG_INVALID_SCHEMA,
    GDA_DB_CATALOG_SERVER_OPERATION,
    GDA_DB_CATALOG_FILE_READ,
    GDA_DB_CATALOG_PARSE_CONTEXT,
    GDA_DB_CATALOG_PARSE,
    GDA_DB_CATALOG_PARSE_CHUNK,
    GDA_DB_CATALOG_CONNECTION_CLOSED
} GdaDbCatalogError;

#define GDA_DB_CATALOG_ERROR gda_db_catalog_error_quark()
GQuark gda_db_catalog_error_quark (void);

GdaDbCatalog		*gda_db_catalog_new        (void);

gboolean		     gda_db_catalog_parse_file_from_path	(GdaDbCatalog *self,
                                                       const gchar *xmlfile,
                                                       GError **error);

gboolean         gda_db_catalog_parse_file (GdaDbCatalog *self,
                                            GFile *xmlfile,
                                            GError **error);

GList           *gda_db_catalog_get_tables	(GdaDbCatalog *self);
GList           *gda_db_catalog_get_views	(GdaDbCatalog *self);

gboolean         gda_db_catalog_parse_cnc (GdaDbCatalog *self,
                                           GError **error);

void             gda_db_catalog_append_table (GdaDbCatalog *self,
                                              GdaDbTable *table);

void             gda_db_catalog_append_view (GdaDbCatalog *self,
                                             GdaDbView *view);

gboolean         gda_db_catalog_perform_operation (GdaDbCatalog *self,
                                                   GError **error);

gboolean         gda_db_catalog_write_to_file (GdaDbCatalog *self,
                                               GFile *file,
                                               GError **error);

gboolean         gda_db_catalog_write_to_path (GdaDbCatalog *self,
                                               const gchar *path,
                                               GError **error);

gboolean         gda_db_catalog_validate_file_from_path (const gchar *xmlfile,
                                                         GError **error);
G_END_DECLS

#endif /* GDA_DB_CATALOG_H */

