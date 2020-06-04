/*
 * gda-db-index.h
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

#ifndef GDA_DB_INDEX_H
#define GDA_DB_INDEX_H

#include <glib.h>
#include <glib-object.h>
#include "gda-db-base.h"
#include "gda-db-index-field.h"

G_BEGIN_DECLS

#define GDA_TYPE_DB_INDEX (gda_db_index_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbIndex, gda_db_index, GDA, DB_INDEX, GdaDbBase)

struct _GdaDbIndexClass
{
  GdaDbBaseClass parent_class;
};

/**
 * GdaDbIndexError:
 *
 */
typedef enum {
    GDA_DB_INDEX_CONNECTION_NOT_OPENED,
    GDA_DB_INDEX_SERVER_OPERATION
} GdaDbIndexError;

#define GDA_DB_INDEX_ERROR gda_db_index_error_quark()

GQuark gda_db_index_error_quark(void);

GdaDbIndex       *gda_db_index_new (void);

void              gda_db_index_set_unique (GdaDbIndex *self, gboolean val);
gboolean          gda_db_index_get_unique (GdaDbIndex *self);

void              gda_db_index_append_field (GdaDbIndex *self, GdaDbIndexField *field);

void              gda_db_index_remove_field (GdaDbIndex *self, const gchar *name);

GSList           *gda_db_index_get_fields   (GdaDbIndex *self);

G_END_DECLS

#endif /* end of include guard: GDA-DB-INDEX_H */



