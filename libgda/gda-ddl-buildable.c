/*
 * gda-ddl-buildable.c
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

#include "gda-ddl-buildable.h"

G_DEFINE_QUARK (gda-ddl-buildable-error, gda_ddl_buildable_error)

G_DEFINE_INTERFACE (GdaDdlBuildable, gda_ddl_buildable, G_TYPE_OBJECT)

static void
gda_ddl_buildable_default_init (GdaDdlBuildableInterface *iface)
{
  /* add properties and signals to the interface here */
}

gboolean
gda_ddl_buildable_parse_node (GdaDdlBuildable  *self,
                              xmlNodePtr        node,
                              GError         **error)
{
  GdaDdlBuildableInterface *iface;

  g_return_val_if_fail (GDA_IS_DDL_BUILDABLE (self),FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL,FALSE);

  iface = GDA_DDL_BUILDABLE_GET_IFACE (self);
  g_return_val_if_fail (iface->parse_node != NULL,FALSE);
  return iface->parse_node (self, node, error);
}

gboolean
gda_ddl_buildable_write_node (GdaDdlBuildable *self,
                              xmlTextWriterPtr writer,
                              GError **error)
{
  GdaDdlBuildableInterface *iface;

  g_return_val_if_fail (GDA_IS_DDL_BUILDABLE (self),FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL,FALSE);

  iface = GDA_DDL_BUILDABLE_GET_IFACE (self);
  g_return_val_if_fail (iface->write_node != NULL,FALSE);
  return iface->write_node (self, writer, error);
}


