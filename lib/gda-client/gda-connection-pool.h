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

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif

#include <gda-connection.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _GdaConnectionPool      GdaConnectionPool;
typedef struct _GdaConnectionPoolClass GdaConnectionPoolClass;

#define GDA_TYPE_CONNECTION_POOL            (gda_connection_pool_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_CONNECTION_POOL(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION_POOL, \
                                        GdaConnectionPool)
#  define GDA_CONNECTION_POOL_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONNECTION_POOL, \
                                     GdaConnectionPoolClass)
#  define IS_GDA_CONNECTION_POOL(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_CONNECTION_POOL)
#  define IS_GDA_CONNECTION_POOL_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_CONNECTION_POOL)
#else
#  define GDA_CONNECTION_POOL(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_CONNECTION_POOL, GdaConnectionPool)
#  define GDA_CONNECTION_POOL_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_CONNECTION_POOL, GdaConnectionPoolClass)
#  define IS_GDA_CONNECTION_POOL(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_CONNECTION_POOL)
#  define IS_GDA_CONNECTION_POOL_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION_POOL))
#endif

struct _GdaConnectionPool
{
#ifdef HAVE_GOBJECT
	GObject   object;
#else
	GtkObject object;
#endif
	GList*    connections;
};

struct _GdaConnectionPoolClass
{
#ifdef HAVE_GOBJECT
	GObjectClass   parent_class;
	GObjectClass   *parent;
#else
	GtkObjectClass parent_class;
#endif
	/* signals */
	void (*open)(GdaConnectionPool *pool, GdaConnection *cnc);
	void (*error)(GdaConnectionPool *pool, GdaConnection *cnc);
};

#ifdef HAVE_GOBJECT
GType              gda_connection_pool_get_type (void);
#else
GtkType            gda_connection_pool_get_type (void);
#endif

GdaConnectionPool* gda_connection_pool_new (void);
void               gda_connection_pool_free (GdaConnectionPool *pool);

GdaConnection*     gda_connection_pool_open_connection (GdaConnectionPool *pool,
														const gchar *gda_name,
														const gchar *username,
														const gchar *password);
void               gda_connection_pool_close_connection (GdaConnectionPool *pool,
														 GdaConnection *cnc);
void               gda_connection_pool_close_all (GdaConnectionPool *pool);

typedef void (*GdaConnectionPoolForeachFunc)(GdaConnectionPool *pool,
											 const gchar *gda_name,
											 const gchar *username,
											 const gchar *password,
											 gpointer user_data);

void               gda_connection_pool_foreach (GdaConnectionPool *pool,
												GdaConnectionPoolForeachFunc func,
												gpointer user_data);

#if defined(__cplusplus)
}
#endif

#endif
