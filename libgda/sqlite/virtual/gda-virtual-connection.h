/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VIRTUAL_CONNECTION_H__
#define __GDA_VIRTUAL_CONNECTION_H__

#include <libgda/gda-connection.h>
#include "gda-virtual-provider.h"

#define GDA_TYPE_VIRTUAL_CONNECTION            (gda_virtual_connection_get_type())

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE (GdaVirtualConnection, gda_virtual_connection, GDA, VIRTUAL_CONNECTION, GdaConnection)

struct _GdaVirtualConnectionClass {
	GdaConnectionClass           parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-virtual-connection
 * @short_description: Base class for all virtual connection objects
 * @title: GdaVirtualConnection
 * @stability: Stable
 * @see_also: 
 *
 * This is a base virtual class for all virtual connection implementations.
 */

GType          gda_virtual_connection_get_type                   (void) G_GNUC_CONST;
GdaConnection *gda_virtual_connection_open                       (GdaVirtualProvider *virtual_provider, GdaConnectionOptions options,
								  GError **error);
void           gda_virtual_connection_internal_set_provider_data (GdaVirtualConnection *vcnc, 
								  gpointer data, GDestroyNotify destroy_func);
gpointer       gda_virtual_connection_internal_get_provider_data (GdaVirtualConnection *vcnc);

G_END_DECLS

#endif
