/*
 * gda-ddl-buildable.h
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
#ifndef GDA_DDL_BUILDABLE_H
#define GDA_DDL_BUILDABLE_H

#include <glib-object.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_BUILDABLE gda_ddl_buildable_get_type ()
G_DECLARE_INTERFACE(GdaDdlBuildable, gda_ddl_buildable,GDA, DDL_BUILDABLE,GObject)

struct _GdaDdlBuildableInterface
{
  GTypeInterface parent_iface;

  gboolean (*parse_node)(GdaDdlBuildable *self,
                         xmlNodePtr node,
                         GError **error);

  gboolean (*write_node)(GdaDdlBuildable *self,
                         xmlTextWriterPtr writer,
                         GError **error);
};

gboolean gda_ddl_buildable_parse_node (GdaDdlBuildable *self,
                                       xmlNodePtr node,
                                       GError **error);

gboolean gda_ddl_buildable_write_node (GdaDdlBuildable *self,
                                       xmlTextWriterPtr writer,
                                       GError **error);

typedef enum {
    GDA_DDL_BUILDABLE_ERROR_START_ELEMENT,
    GDA_DDL_BUILDABLE_ERROR_ATTRIBUTE,
    GDA_DDL_BUILDABLE_ERROR_END_ELEMENT
}GdaDdlBuildableError;

#define GDA_DDL_BUILDABLE_ERROR gda_ddl_buildable_error_quark()
GQuark gda_ddl_buildable_error_quark (void);
G_END_DECLS

#endif /* end of include guard: GDA-DDL-BUILDABLE_H */


