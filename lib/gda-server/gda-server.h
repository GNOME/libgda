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

#if !defined(__gda_server_h__)
#  define __gda_server_h__

#include <gda-common.h>

G_BEGIN_DECLS

typedef struct _GdaServer        GdaServer;
typedef struct _GdaServerClass   GdaServerClass;
typedef struct _GdaServerPrivate GdaServerPrivate;

struct _GdaServer {
	GObject object;
	GdaServerPrivate *priv;
};

struct _GdaServerClass {
	GObjectClass parent_class;

	/* signals */
	void (* last_component_gone) (GdaServer *server);
	void (* last_client_gone) (GdaServer *server);
};

GType      gda_server_get_type (void);
GdaServer *gda_server_new (const gchar *factory_iid);
void       gda_server_register_component (GdaServer *server, const char *iid, GType type);
void       gda_server_unregister_component (GdaServer *server, const char *iid);

G_END_DECLS

#endif
