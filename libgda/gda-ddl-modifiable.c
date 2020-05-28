/*
 * gda-ddl-operation.c
 *
 * Copyright (C) 2020 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#define G_LOG_DOMAIN "GDA-ddl-modifiable"
#include "gda-ddl-modifiable.h"

G_DEFINE_QUARK (gda_ddl_modifiable_error, gda_ddl_modifiable_error)

/**
 * SECTION:gda-ddl-operation
 * @title: GdaDdlModifiable
 * @short_description: Interface to peform DDL operation
 * @see_also: #GdaDbTable, #GdaDbView, #GdaDbIndex, #GdaDbColumn
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This interface should be used to perform some DDL opration using objects that implement it.
 * This interface is implemented by #GdaDbTable, #GdaDbColumn, #GdaDbIndex, and #GdaDbView. Calling
 * gda_ddl_modifiable_create() on #GdaDbColumn operation will execute ADD COLUMN operation. The
 * should pass a pointer to instance of GdaDbTable as user_data where column will be added (cretaed).
 *
 */

G_DEFINE_INTERFACE (GdaDdlModifiable, gda_ddl_modifiable, G_TYPE_OBJECT)


static void
gda_ddl_modifiable_default_init (GdaDdlModifiableInterface *iface)
{
  /* add properties and signals to the interface here */
}

gboolean
gda_ddl_modifiable_create (GdaDdlModifiable *self,
                          GdaConnection *cnc,
                          gpointer user_data,
                          GError **error)
{
  GdaDdlModifiableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DDL_MODIFIABLE (self), FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

  iface = GDA_DDL_MODIFIABLE_GET_IFACE (self);
  return iface->create (self, cnc, user_data, error);
}

gboolean
gda_ddl_modifiable_drop (GdaDdlModifiable *self,
                        GdaConnection *cnc,
                        gpointer user_data,
                        GError **error)
{
  GdaDdlModifiableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DDL_MODIFIABLE (self), FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

  iface = GDA_DDL_MODIFIABLE_GET_IFACE (self);
  return iface->drop (self, cnc, user_data, error);
}

gboolean
gda_ddl_modifiable_rename (GdaDdlModifiable *self,
                          GdaConnection *cnc,
                          gpointer user_data,
                          GError **error)
{
  GdaDdlModifiableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DDL_MODIFIABLE (self), FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

  iface = GDA_DDL_MODIFIABLE_GET_IFACE (self);
  return iface->rename (self, cnc, user_data, error);
}

