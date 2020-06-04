/*
 * gda-ddl-modifiable.h
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
#ifndef GDA_DDL_MODIFIABLE_H
#define GDA_DDL_MODIFIABLE_H

#include <glib-object.h>
#include <glib.h>
#include "gda-connection.h"

G_BEGIN_DECLS

#define GDA_TYPE_DDL_MODIFIABLE gda_ddl_modifiable_get_type ()

G_DECLARE_INTERFACE (GdaDdlModifiable, gda_ddl_modifiable, GDA, DDL_MODIFIABLE, GObject)

struct _GdaDdlModifiableInterface
{
  GTypeInterface parent_iface;

  gboolean (*create)(GdaDdlModifiable *self,
                     GdaConnection *cnc,
                     gpointer user_data,
                     GError **error);

  gboolean (*drop)(GdaDdlModifiable *self,
                   GdaConnection *cnc,
                   gpointer user_data,
                   GError **error);

  gboolean (*rename)(GdaDdlModifiable *self,
                     GdaConnection *cnc,
                     gpointer user_data,
                     GError **error);
};

typedef enum {
    GDA_DDL_MODIFIABLE_NOT_IMPLEMENTED,
    GDA_DDL_MODIFIABLE_CONNECTION_NOT_OPENED,
    GDA_DDL_MODIFIABLE_MISSED_DATA
} GdaDdlModifiableError;

#define GDA_DDL_MODIFIABLE_ERROR gda_ddl_modifiable_error_quark()
GQuark gda_ddl_modifiable_error_quark (void);

gboolean gda_ddl_modifiable_create (GdaDdlModifiable *self,
                                    GdaConnection *cnc,
                                    gpointer user_data,
                                    GError **error);

gboolean gda_ddl_modifiable_drop (GdaDdlModifiable *self,
                                  GdaConnection *cnc,
                                  gpointer user_data,
                                  GError **error);

gboolean gda_ddl_modifiable_rename (GdaDdlModifiable *self,
                                    GdaConnection *cnc,
                                    gpointer user_data,
                                    GError **error);

G_END_DECLS

#endif /* end of include guard: GDA-DB-MODIFIABLE*/



