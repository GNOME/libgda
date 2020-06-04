/*
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

#ifndef __GDA_DB_TABLE_H__
#define __GDA_DB_TABLE_H__

#include "gda-db-base.h"
#include "gda-db-column.h"
#include "gda-db-fkey.h"
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>
#include "gda-server-operation.h"
#include <libgda/sql-parser/gda-sql-statement.h>
#include "gda-meta-struct.h"
#include "gda-db-index.h"

G_BEGIN_DECLS

#define GDA_TYPE_DB_TABLE (gda_db_table_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDbTable, gda_db_table, GDA, DB_TABLE, GdaDbBase)

struct _GdaDbTableClass {
    GdaDbBaseClass parent_class;
};

/**
 * GdaDbTableError:
 * @GDA_DB_TABLE_COLUMN_EMPTY: Table doesn't contain columns
 * @GDA_DB_TABLE_CONNECTION_NOT_OPENED: Closed connection was passed as parameter
 * @GDA_DB_TABLE_SERVER_OPERATION: Error related to #GdaServerOperation
 */
typedef enum {
    GDA_DB_TABLE_COLUMN_EMPTY,
    GDA_DB_TABLE_CONNECTION_NOT_OPENED,
    GDA_DB_TABLE_SERVER_OPERATION
} GdaDbTableError;

#define GDA_DB_TABLE_ERROR gda_db_table_error_quark()
GQuark gda_db_table_error_quark(void);

GdaDbTable*     gda_db_table_new               (void);
gboolean        gda_db_table_is_valid          (GdaDbTable *self);
GList*          gda_db_table_get_columns       (GdaDbTable *self);
GList*          gda_db_table_get_fkeys         (GdaDbTable *self);

void            gda_db_table_append_column     (GdaDbTable *self,
                                                 GdaDbColumn *column);

gboolean        gda_db_table_get_is_temp       (GdaDbTable *self);
void            gda_db_table_set_is_temp       (GdaDbTable *self,
                                                 gboolean istemp);

gboolean        gda_db_table_prepare_create (GdaDbTable *self,
                                             GdaServerOperation *op,
                                             gboolean ifnotexists,
                                             GError **error);

gboolean        gda_db_table_update          (GdaDbTable *self,
                                              GdaMetaTable *obj,
                                              GdaConnection *cnc,
                                              GError **error);

void            gda_db_table_append_fkey (GdaDbTable *self,
                                          GdaDbFkey *fkey);

void            gda_db_table_append_constraint (GdaDbTable *self,
                                                const gchar *constr);

G_END_DECLS

#endif /* end of include guard: GDA-DB-TABLE_H */


