/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000,2001 Rodrigo Moya
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

#if !defined(__gda_connection_pool_h__)
#  define __gda_connection_pool_h__

#include <glib.h>
#include <gtk/gtkobject.h>
#include <gda-common-defs.h>
#include <gda-connection.h>

G_BEGIN_DECLS

typedef struct _GdaConnectionPool GdaConnectionPool;
typedef struct _GdaConnectionPoolClass GdaConnectionPoolClass;

#define GDA_TYPE_CONNECTION_POOL            (gda_connection_pool_get_type())
#  define GDA_CONNECTION_POOL(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_CONNECTION_POOL, GdaConnectionPool)
#  define GDA_CONNECTION_POOL_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_CONNECTION_POOL, GdaConnectionPoolClass)
#  define GDA_IS_CONNECTION_POOL(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_CONNECTION_POOL)
#  define GDA_IS_CONNECTION_POOL_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION_POOL))

struct _GdaConnectionPool {
	GtkObject object;
	GList *connections;
};

struct _GdaConnectionPoolClass {
	GtkObjectClass parent_class;

	/* signals */
	void (*open) (GdaConnectionPool * pool, GdaConnection * cnc);
	void (*error) (GdaConnectionPool * pool, GdaConnection * cnc, GList *error_list);
};

GtkType gda_connection_pool_get_type (void);

GdaConnectionPool *gda_connection_pool_new (void);
void gda_connection_pool_free (GdaConnectionPool * pool);

GdaConnection *gda_connection_pool_open_connection (GdaConnectionPool *pool,
						    const gchar *gda_name,
						    const gchar *username,
						    const gchar *password);
void gda_connection_pool_close_connection (GdaConnectionPool * pool,
					   GdaConnection * cnc);
void gda_connection_pool_close_all (GdaConnectionPool * pool);

typedef void (*GdaConnectionPoolForeachFunc) (GdaConnectionPool *pool,
					      GdaConnection * cnc,
					      const gchar * gda_name,
					      const gchar * username,
					      const gchar * password,
					      gpointer user_data);

void gda_connection_pool_foreach (GdaConnectionPool * pool,
				  GdaConnectionPoolForeachFunc func,
				  gpointer user_data);

G_END_DECLS

#endif
