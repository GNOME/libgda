/*
 * gda-db-buildable.h
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
#ifndef GDA_DB_BUILDABLE_H
#define GDA_DB_BUILDABLE_H

#include <glib-object.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>

G_BEGIN_DECLS

#define GDA_TYPE_DB_BUILDABLE gda_db_buildable_get_type ()

G_DECLARE_INTERFACE(GdaDbBuildable, gda_db_buildable, GDA, DB_BUILDABLE, GObject)

struct _GdaDbBuildableInterface
{
  GTypeInterface parent_iface;

  gboolean (*parse_node)(GdaDbBuildable *self,
                         xmlNodePtr node,
                         GError **error);

  gboolean (*write_node)(GdaDbBuildable *self,
                         xmlNodePtr node,
                         GError **error);
};

gboolean gda_db_buildable_parse_node (GdaDbBuildable *self,
                                      xmlNodePtr node,
                                      GError **error);

gboolean gda_db_buildable_write_node (GdaDbBuildable *self,
                                      xmlNodePtr node,
                                      GError **error);

G_END_DECLS

#endif /* end of include guard: GDA-DB-BUILDABLE_H */


