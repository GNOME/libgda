/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_connection_pool_h__)
#  define __gda_connection_pool_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtk.h>
#endif

#include <gda-connection.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_ConnectionPool      Gda_ConnectionPool;
typedef struct _Gda_ConnectionPoolClass Gda_ConnectionPoolClass;

#define GDA_TYPE_CONNECTION_POOL            (gda_connection_pool_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_CONNECTION_POOL(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION_POOL, \
                                        Gda_ConnectionPool)
#  define GDA_CONNECTION_POOL_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONNECTION_POOL, \
                                     Gda_ConnectionPoolClass)
#  define IS_GDA_CONNECTION_POOL(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_CONNECTION_POOL)
#  define IS_GDA_CONNECTION_POOL_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_CONNECTION_POOL)
#else
#  define GDA_CONNECTION_POOL(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_CONNECTION_POOL, Gda_ConnectionPool)
#  define GDA_CONNECTION_POOL_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_CONNECTION_POOL, GdaConnectionPoolClass)
#  define IS_GDA_CONNECTION_POOL(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_CONNECTION_POOL)
#  define IS_GDA_CONNECTION_POOL_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION_POOL))
#endif

struct _Gda_ConnectionPool
{
#ifdef HAVE_GOBJECT
  GObject   object;
#else
  GtkObject object;
#endif
  GList*    connections;
};

struct _Gda_ConnectionPoolClass
{
#ifdef HAVE_GOBJECT
  GObjectClass   parent_class;
  GObjectClass   *parent;
#else
  GtkObjectClass parent_class;
#endif
  /* signals */
  void (*new_connection)(Gda_ConnectionPool *pool, Gda_Connection *cnc);
};

#ifdef HAVE_GOBJECT
GType               gda_connection_pool_get_type        (void);
#else
GtkType             gda_connection_pool_get_type        (void);
#endif

Gda_ConnectionPool* gda_connection_pool_new             (void);
void                gda_connection_pool_free            (Gda_ConnectionPool *pool);

Gda_Connection*     gda_connection_pool_open_connection (Gda_ConnectionPool *pool,
							 const gchar *gda_name,
							 const gchar *username,
							 const gchar *password);
void                gda_connection_pool_close_all       (Gda_ConnectionPool *pool);

#if defined(__cplusplus)
}
#endif

#endif
