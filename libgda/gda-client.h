/* GDA client library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#if !defined(__gda_client_h__)
#  define __gda_client_h__

#include <gda-connection.h>
#include <GNOME_Database.h>
#include <bonobo/bonobo-xobject.h>

G_BEGIN_DECLS

typedef struct _GdaClientClass   GdaClientClass;
typedef struct _GdaClientPrivate GdaClientPrivate;

struct _GdaClient {
	BonoboXObject object;
	GdaClientPrivate;
};

struct _GdaClientClass {
	BonoboXObjectClass parent_class;
	POA_GNOME_Database_Client__epv epv;

	/* signals */
	void (* action_notified) (GdaClient *client,
				  GNOME_Database_ActionId action,
				  GdaParameterList *params);
};

GType          gda_client_get_type (void);
GdaClient     *gda_client_new (const gchar *iid);

GdaConnection *gda_client_open_connection (GdaClient *client,
					   const gchar *cnc_string,
					   const gchar *username,
					   const gchar *password);

G_END_DECLS

#endif
