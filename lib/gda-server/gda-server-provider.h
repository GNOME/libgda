/* GDA server library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_server_provider_h__)
#  define __gda_server_provider_h__

#include <gda-common.h>
#include <gda-server-connection.h>
#include <bonobo/bonobo-xobject.h>

G_BEGIN_DECLS

typedef struct _GdaServerProvider        GdaServerProvider;
typedef struct _GdaServerProviderClass   GdaServerProviderClass;
typedef struct _GdaServerProviderPrivate GdaServerProviderPrivate;

struct _GdaServerProvider {
	BonoboXObject object;
	GdaServerProviderPrivate *priv;
};

struct _GdaServerProviderClass {
	BonoboXObjectClass parent_class;
	POA_GNOME_Database_Provider__epv epv;

	/* virtual methods */
	GdaServerConnection * (* open_connection) (GdaServerProvider *provider,
						   const gchar *cnc_string,
						   const gchar *username,
						   const gchar *password);
	gboolean (* close_connection) (GdaServerProvider *provider,
				       GdaServerConnection *cnc);
	
};

GType                gda_server_provider_get_type (void);
GdaServerConnection *gda_server_provider_open_connection (GdaServerProvider *provider,
							  const gchar *cnc_string,
							  const gchar *username,
							  const gchar *password);

G_END_DECLS

#endif
