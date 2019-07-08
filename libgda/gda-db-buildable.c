/*
 * gda-db-buildable.c
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
#define G_LOG_DOMAIN "GDA-db-buildable"

#include "gda-db-buildable.h"

G_DEFINE_INTERFACE (GdaDbBuildable, gda_db_buildable, G_TYPE_OBJECT)

/**
 * SECTION:gda-db-buildable
 * @title: GdaDbBuildable
 * @short: Represents interface for parsing and writing xml nodes
 * @see_also: #GdaDbTable, #GdaDbView
 * @stability: Stable
 * @include: libgda/gda-db-buildable.h
 *
 * #GdaDbBuildable represents an interface for writing and reading xml nodes. #GdaDbTable and
 * #GdaDbView implement this interface.
 */

static void
gda_db_buildable_default_init (GdaDbBuildableInterface *iface)
{
  /* add properties and signals to the interface here */
}

/**
 * gda_db_buildable_parse_node:
 * @self: an instance of #GdaDbBuildable where parsed data should be storred
 * @node: a node to parse
 * @error: (nullable): an object to store the error
 *
 * This method parse XML node and populate @self object.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 * Since: 6.0
 * Stability: stable
 */
gboolean
gda_db_buildable_parse_node (GdaDbBuildable  *self,
                             xmlNodePtr        node,
                             GError         **error)
{
  GdaDbBuildableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DB_BUILDABLE (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  iface = GDA_DB_BUILDABLE_GET_IFACE (self);
  g_return_val_if_fail (iface->parse_node != NULL, FALSE);
  return iface->parse_node (self, node, error);
}

/**
 * gda_db_buildable_write_node:
 * @self: an instance of #GdaDbBuildable where data should be taken
 * @node: a node to write data in
 * @error: (nullable): an object to store error
 *
 * Write content from the @self to the @node
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 * Since: 6.0
 * Stability: stable
 */
gboolean
gda_db_buildable_write_node (GdaDbBuildable *self,
                             xmlNodePtr node,
                             GError **error)
{
  GdaDbBuildableInterface *iface;

  g_return_val_if_fail (GDA_IS_DB_BUILDABLE (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  iface = GDA_DB_BUILDABLE_GET_IFACE (self);
  g_return_val_if_fail (iface->write_node != NULL, FALSE);
  return iface->write_node (self, node, error);
}


