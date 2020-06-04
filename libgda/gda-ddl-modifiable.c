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
#include <glib/gi18n-lib.h>

G_DEFINE_QUARK (gda_ddl_modifiable_error, gda_ddl_modifiable_error)

/**
 * SECTION:gda-ddl-modifiable
 * @title: GdaDdlModifiable
 * @short_description: Interface to peform DDL operation
 * @see_also: #GdaDbTable, #GdaDbView, #GdaDbIndex, #GdaDbColumn
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This interface should be used to perform some DDL opration using objects that implement it.
 * Calling gda_ddl_modifiable_create() on #GdaDbColumn operation will execute ADD COLUMN
 * operation. The user should pass a pointer to instance of #GdaDbTable as user_data where
 * column will be added (cretaed).
 *
 * If the underlying object does not implement the operation, then %FALSE is returned and the error
 * is set.
 */

G_DEFINE_INTERFACE (GdaDdlModifiable, gda_ddl_modifiable, G_TYPE_OBJECT)


static void
gda_ddl_modifiable_default_init (GdaDdlModifiableInterface *iface)
{
  /* add properties and signals to the interface here */
}

/**
 * gda_ddl_modifiable_create:
 * @self: Instance of #GdaDdlModifiable
 * @cnc: Opened connection
 * @user_data: Additional information provided by the user
 * @error: Error holder
 *
 * This method executes CREATE operation. That is, #GdaDbTable, #GdaDbIndex, and #GdaDbView
 * implement corresponding CREATE TABLE | CREATE INDEX | CREATE VIEW operations. #GdaDbColumn
 * implements ADD COLUMN operation as part of ALTER TABLE operation.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_ddl_modifiable_create (GdaDdlModifiable *self,
                          GdaConnection *cnc,
                          gpointer user_data,
                          GError **error)
{
  GdaDdlModifiableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DDL_MODIFIABLE (self), FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

  if (!gda_connection_is_opened (cnc))
    {
      g_set_error (error, GDA_DDL_MODIFIABLE_ERROR,
                   GDA_DDL_MODIFIABLE_CONNECTION_NOT_OPENED,
                   _("Connection is not opened"));
      return FALSE;
    }

  iface = GDA_DDL_MODIFIABLE_GET_IFACE (self);
  return iface->create (self, cnc, user_data, error);
}

/**
 * gda_ddl_modifiable_drop:
 * @self: Instance of #GdaDdlModifiable
 * @cnc: Opened connection
 * @user_data: Additional information provided by the user
 * @error: Error holder
 *
 * Execute corresponding DROP operation
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_ddl_modifiable_drop (GdaDdlModifiable *self,
                        GdaConnection *cnc,
                        gpointer user_data,
                        GError **error)
{
  GdaDdlModifiableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DDL_MODIFIABLE (self), FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

  if (!gda_connection_is_opened (cnc))
    {
      g_set_error (error, GDA_DDL_MODIFIABLE_ERROR,
                   GDA_DDL_MODIFIABLE_CONNECTION_NOT_OPENED,
                   _("Connection is not opened"));
      return FALSE;
    }

  iface = GDA_DDL_MODIFIABLE_GET_IFACE (self);
  return iface->drop (self, cnc, user_data, error);
}

/**
 * gda_ddl_modifiable_rename:
 * @self: Instance of #GdaDdlModifiable
 * @cnc: Opened connection
 * @user_data: Additional information provided by the user
 * @error: Error holder
 *
 * Execute corresponding RENAME operation. A lot of RENAME operations are not implemented by
 * SQLite3 provider. In this case, the SQL object must be deleted and a new one should be created.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_ddl_modifiable_rename (GdaDdlModifiable *self,
                          GdaConnection *cnc,
                          gpointer user_data,
                          GError **error)
{
  GdaDdlModifiableInterface *iface = NULL;

  g_return_val_if_fail (GDA_IS_DDL_MODIFIABLE (self), FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

  if (!gda_connection_is_opened (cnc))
    {
      g_set_error (error, GDA_DDL_MODIFIABLE_ERROR,
                   GDA_DDL_MODIFIABLE_CONNECTION_NOT_OPENED,
                   _("Connection is not opened"));
      return FALSE;
    }

  iface = GDA_DDL_MODIFIABLE_GET_IFACE (self);
  return iface->rename (self, cnc, user_data, error);
}

