/* gda-db-view.h
 *
 * Copyright (C) 2018-2019 Pavlo Solntsev <p.sun.fun@gmail.com>
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
#ifndef GDA_DB_VIEW_H
#define GDA_DB_VIEW_H

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include "gda-db-base.h"
#include "gda-db-buildable.h"
#include "gda-meta-struct.h"
#include "gda-server-operation.h"


G_BEGIN_DECLS

#define GDA_TYPE_DB_VIEW (gda_db_view_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbView, gda_db_view, GDA, DB_VIEW, GdaDbBase)

struct _GdaDbViewClass
{
  GdaDbBaseClass parent_class;
};

typedef enum
{
  GDA_DB_VIEW_RESTRICT = 0,
  GDA_DB_VIEW_CASCADE
} GdaDbViewRefAction;


GdaDbView*  gda_db_view_new            (void);

gboolean     gda_db_view_get_istemp    (GdaDbView *self);
void         gda_db_view_set_istemp    (GdaDbView *self,
                                        gboolean temp);

gboolean     gda_db_view_get_ifnoexist (GdaDbView *self);
void         gda_db_view_set_ifnoexist (GdaDbView *self,
                                        gboolean noexist);

const gchar* gda_db_view_get_defstring (GdaDbView *self);
void         gda_db_view_set_defstring (GdaDbView *self,
                                        const gchar *str);

gboolean     gda_db_view_get_replace   (GdaDbView *self);
void         gda_db_view_set_replace   (GdaDbView *self,
                                        gboolean replace);

gboolean     gda_db_view_prepare_create (GdaDbView *self,
                                         GdaServerOperation *op,
                                         GError **error);

G_END_DECLS

#endif /* GDA_DB_VIEW_H */


