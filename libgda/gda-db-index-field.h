/*
 * gda-db-index-field.h
 * Copyright (C) 2019 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GDA_DB_INDEX_FIELD_H
#define GDA_DB_INDEX_FIELD_H

#include <glib.h>
#include <glib-object.h>
#include "gda-db-base.h"
#include "gda-db-column.h"

G_BEGIN_DECLS

#define GDA_TYPE_DB_INDEX_FIELD (gda_db_index_field_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbIndexField, gda_db_index_field, GDA, DB_INDEX_FIELD, GObject)

struct _GdaDbIndexFieldClass
{
  GObjectClass parent_class;
};

/**
 * GdaDbIndexSortOrder:
 * @GDA_DB_INDEX_SORT_ORDER_ASC: ascending sorting
 * @GDA_DB_INDEX_SORT_ORDER_DESC: descending sorting
 *
 * Enum values to specify the sorting
 */
typedef enum
{
  GDA_DB_INDEX_SORT_ORDER_ASC = 0,
  GDA_DB_INDEX_SORT_ORDER_DESC
} GdaDbIndexSortOrder;

GdaDbIndexField     *gda_db_index_field_new            (void);

void                 gda_db_index_field_set_column     (GdaDbIndexField *self,
                                                        GdaDbColumn *column);

GdaDbColumn         *gda_db_index_field_get_column     (GdaDbIndexField *self);

void                 gda_db_index_field_set_collate    (GdaDbIndexField *self,
                                                        const gchar *collate);

const gchar *        gda_db_index_field_get_collate    (GdaDbIndexField *self);

void                 gda_db_index_field_set_sort_order (GdaDbIndexField *self,
                                                        GdaDbIndexSortOrder sorder);

GdaDbIndexSortOrder  gda_db_index_field_get_sort_order (GdaDbIndexField *self);

const gchar *        gda_db_index_field_get_sort_order_str (GdaDbIndexField *self);

G_END_DECLS

#endif /* end of include guard: GDA-DB-INDEX_H */




