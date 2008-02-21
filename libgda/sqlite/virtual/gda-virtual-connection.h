/* GDA virtual connection
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDA_VIRTUAL_CONNECTION_H__
#define __GDA_VIRTUAL_CONNECTION_H__

#include <libgda/gda-connection.h>
#include <virtual/gda-virtual-provider.h>

#define GDA_TYPE_VIRTUAL_CONNECTION            (gda_virtual_connection_get_type())
#define GDA_VIRTUAL_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VIRTUAL_CONNECTION, GdaVirtualConnection))
#define GDA_VIRTUAL_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VIRTUAL_CONNECTION, GdaVirtualConnectionClass))
#define GDA_IS_VIRTUAL_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VIRTUAL_CONNECTION))
#define GDA_IS_VIRTUAL_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VIRTUAL_CONNECTION))

G_BEGIN_DECLS

typedef struct _GdaVirtualConnection      GdaVirtualConnection;
typedef struct _GdaVirtualConnectionClass GdaVirtualConnectionClass;
typedef struct _GdaVirtualConnectionPrivate GdaVirtualConnectionPrivate;

struct _GdaVirtualConnection {
	GdaConnection                connection;
	GdaVirtualConnectionPrivate *priv;
};

struct _GdaVirtualConnectionClass {
	GdaConnectionClass           parent_class;
};

GType          gda_virtual_connection_get_type                   (void) G_GNUC_CONST;
GdaConnection *gda_virtual_connection_open                       (GdaVirtualProvider *virtual_provider, GError **error);
void           gda_virtual_connection_internal_set_provider_data (GdaVirtualConnection *vcnc, 
								  gpointer data, GDestroyNotify destroy_func);
gpointer       gda_virtual_connection_internal_get_provider_data (GdaVirtualConnection *cnc);

G_END_DECLS

#endif
