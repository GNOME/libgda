/* gda-db-column.h
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
#ifndef GDA_DB_COLUMN_H
#define GDA_DB_COLUMN_H

#include <glib-object.h>
#include <glib.h>
#include <libxml/parser.h>
#include "gda-db-buildable.h"
#include "gda-server-operation.h"
#include "gda-meta-struct.h"

G_BEGIN_DECLS

#define GDA_TYPE_DB_COLUMN (gda_db_column_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbColumn, gda_db_column, GDA, DB_COLUMN, GObject)

struct _GdaDbColumnClass
{
  GObjectClass parent;
};

/**
 * GdaDbColumnError:
 * @GDA_DB_COLUMN_ERROR_TYPE: Set if wrong column type was given in the xml file.
 * @GDA_DB_COLUMN_ERROR_WRONG_OPERATION: Wrong operation requested
 *
 * Values used to describe the source of the error.
 */
typedef  enum
{
  GDA_DB_COLUMN_ERROR_TYPE,
  GDA_DB_COLUMN_ERROR_WRONG_OPERATION
} GdaDbColumnError;

#define GDA_DB_COLUMN_ERROR gda_db_column_error_quark()
GQuark gda_db_column_error_quark (void);

GdaDbColumn*    gda_db_column_new              (void);

const gchar*    gda_db_column_get_name         (GdaDbColumn *self);
void            gda_db_column_set_name         (GdaDbColumn *self, const gchar *name);

GType           gda_db_column_get_gtype        (GdaDbColumn *self);
const gchar*    gda_db_column_get_ctype        (GdaDbColumn *self);
void            gda_db_column_set_type         (GdaDbColumn *self, GType type);

guint           gda_db_column_get_scale        (GdaDbColumn *self);
void            gda_db_column_set_scale        (GdaDbColumn *self,
                                                guint scale);

gboolean        gda_db_column_get_pkey         (GdaDbColumn *self);
void            gda_db_column_set_pkey         (GdaDbColumn *self,
                                                gboolean pkey);

gboolean        gda_db_column_get_nnul         (GdaDbColumn *self);
void            gda_db_column_set_nnul         (GdaDbColumn *self,
                                                gboolean nnul);

gboolean        gda_db_column_get_autoinc      (GdaDbColumn *self);
void            gda_db_column_set_autoinc      (GdaDbColumn *self,
                                                gboolean autoinc);

gboolean        gda_db_column_get_unique       (GdaDbColumn *self);
void            gda_db_column_set_unique       (GdaDbColumn *self,
                                                gboolean unique);

const gchar*    gda_db_column_get_comment      (GdaDbColumn *self);
void            gda_db_column_set_comment      (GdaDbColumn *self,
                                                const gchar *comnt);

guint           gda_db_column_get_size         (GdaDbColumn *self);
void            gda_db_column_set_size         (GdaDbColumn *self,
                                                guint size);

const gchar*    gda_db_column_get_default      (GdaDbColumn *self);
void            gda_db_column_set_default      (GdaDbColumn *self,
                                                const gchar *value);

const gchar*    gda_db_column_get_check        (GdaDbColumn *self);
void            gda_db_column_set_check        (GdaDbColumn *self,
                                                const gchar *value);

gboolean        gda_db_column_prepare_create   (GdaDbColumn *self,
                                                GdaServerOperation *op,
                                                guint order,
                                                GError **error);

gboolean        gda_db_column_prepare_add      (GdaDbColumn *self,
                                                GdaServerOperation *op,
                                                GError **error);

G_END_DECLS

#endif /* GDA_DB_COLUMN_H */

