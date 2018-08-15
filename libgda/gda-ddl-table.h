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

#ifndef __GDA_DDL_TABLE_H__
#define __GDA_DDL_TABLE_H__

#include "gda-ddl-base.h"
#include "gda-ddl-column.h" 
#include "gda-ddl-fkey.h"
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>
#include "gda-server-operation.h"
#include <libgda/sql-parser/gda-sql-statement.h>
#include "gda-meta-struct.h"

G_BEGIN_DECLS

#define GDA_TYPE_DDL_TABLE (gda_ddl_table_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDdlTable, gda_ddl_table, GDA, DDL_TABLE, GdaDdlBase)

struct _GdaDdlTableClass {
    GdaDdlBaseClass parent_class;
};

/**
 * GdaDdlTableError:
 * @GDA_DDL_TABLE_COLUMN_EMPTY: Table doesn't contain columns
 *
 */
typedef enum {
    GDA_DDL_TABLE_COLUMN_EMPTY,
}GdaDdlTableError;

#define GDA_DDL_TABLE_ERROR gda_ddl_table_error_quark()
GQuark gda_ddl_table_error_quark(void);

GdaDdlTable*    gda_ddl_table_new               (void);
gboolean        gda_ddl_table_is_valid          (GdaDdlTable *self);
const GList*    gda_ddl_table_get_columns       (GdaDdlTable *self);
const GList*    gda_ddl_table_get_fkeys         (GdaDdlTable *self);

void            gda_ddl_table_append_column     (GdaDdlTable *self,
                                                 GdaDdlColumn *column);

gboolean        gda_ddl_table_is_temp           (GdaDdlTable *self);
void            gda_ddl_table_set_temp          (GdaDdlTable *self,
                                                 gboolean istemp);

gboolean        gda_ddl_table_prepare_create (GdaDdlTable *self,
                                              GdaServerOperation *op,
                                              GError **error);

gboolean        gda_ddl_table_update          (GdaDdlTable *self,
                                               GdaMetaTable *obj,
                                               GdaConnection *cnc,
                                               GError **error);

gboolean        gda_ddl_table_create          (GdaDdlTable *self,
                                               GdaConnection *cnc,
                                               GError **error);

GdaDdlTable    *gda_ddl_table_new_from_meta    (GdaMetaDbObject *obj);

void            gda_ddl_table_append_fkey (GdaDdlTable *self,
                                           GdaDdlFkey *fkey);
G_END_DECLS

#endif /* end of include guard: GDA-DDL-TABLE_H */


