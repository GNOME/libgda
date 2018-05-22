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
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_TABLE (gda_ddl_table_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaDdlTable, gda_ddl_table, GDA, DDL_TABLE, GdaDdlBase)

struct _GdaDdlTableClass {
    GdaDdlBaseClass parent_class;
};

typedef enum {
    GDA_DDL_TABLE_SOME,
}GdaDdlTableError;

#define GDA_DDL_TABLE_ERROR gda_ddl_table_error_quark()
GQuark gda_ddl_table_error_quark(void);

GdaDdlTable*    gda_ddl_table_new               (void);
void            gda_ddl_table_free              (GdaDdlTable *self);
gboolean        gda_ddl_table_is_valid          (GdaDdlTable *self);
const GList*    gda_ddl_table_get_columns       (GdaDdlTable *self);
const GList*    gda_ddl_table_get_fkeys         (GdaDdlTable *self);

gboolean        gda_ddl_table_is_temp           (GdaDdlTable *self);
void            gda_ddl_table_set_temp          (GdaDdlTable *self,
                                                 gboolean status);

gboolean        gda_ddl_table_parse_node        (GdaDdlTable  *self,
                                                 xmlNodePtr node,
                                                 GError **error);

G_END_DECLS

#endif /* end of include guard: GDA-DDL-TABLE_H */







