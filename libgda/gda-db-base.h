/* gda-db-base.h
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

#ifndef GDA_DB_BASE_H
#define GDA_DB_BASE_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define GDA_TYPE_DB_BASE (gda_db_base_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbBase, gda_db_base, GDA, DB_BASE, GObject)

struct _GdaDbBaseClass
{
  GObjectClass parent;
};

GdaDbBase*    gda_db_base_new (void);

void          gda_db_base_set_names     (GdaDbBase *self,
                                         const gchar *catalog,
                                         const gchar *schema,
                                         const gchar *name);

const gchar*  gda_db_base_get_full_name  (GdaDbBase *self);
const gchar*  gda_db_base_get_name       (GdaDbBase *self);
void          gda_db_base_set_name       (GdaDbBase *self,
                                          const gchar *name);

const gchar*  gda_db_base_get_catalog    (GdaDbBase *self);
void          gda_db_base_set_catalog    (GdaDbBase  *self,
                                          const gchar *catalog);

const gchar*  gda_db_base_get_schema     (GdaDbBase *self);
void          gda_db_base_set_schema     (GdaDbBase  *self,
                                          const gchar *schema);

gint          gda_db_base_compare        (GdaDbBase *a, GdaDbBase *b);

#define GDA_BOOL_TO_STR(x) (x ? "TRUE" : "FALSE")
G_END_DECLS

#endif /* GDA_DB_BASE_H */

