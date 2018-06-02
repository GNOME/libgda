/* gda-ddl-view.h
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
#ifndef GDA_DDL_VIEW_H
#define GDA_DDL_VIEW_H

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include "gda-ddl-base.h"

G_BEGIN_DECLS

#define GDA_TYPE_DDL_VIEW (gda_ddl_view_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlView, gda_ddl_view, GDA, DDL_VIEW, GdaDdlBase)

struct _GdaDdlViewClass
{
  GdaDdlBaseClass parent_class;
};

GdaDdlView*  gda_ddl_view_new           (void);
void         gda_ddl_view_free          (GdaDdlView *self);

const gchar* gda_ddl_view_get_name      (GdaDdlView *self);
void         gda_ddl_view_set_name      (GdaDdlView *self, const gchar *name);

gboolean     gda_ddl_view_get_istemp    (GdaDdlView *self);
void         gda_ddl_view_set_istemp    (GdaDdlView *self,gboolean temp);

gboolean     gda_ddl_view_get_ifnoexist (GdaDdlView *self);
void         gda_ddl_view_set_ifnoexist (GdaDdlView *self,gboolean noexist);

const gchar* gda_ddl_view_get_defstring (GdaDdlView *self);
void         gda_ddl_view_set_defstring (GdaDdlView *self, const gchar *str);

gboolean     gda_ddl_view_get_replace   (GdaDdlView *self);
void         gda_ddl_view_set_replace   (GdaDdlView *self,gboolean replace);

G_END_DECLS

#endif /* GDA_DDL_VIEW_H */


