/* GNOME DB library
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

#include <gda-connection.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_ConnectionPool      Gda_ConnectionPool;
typedef struct _Gda_ConnectionPoolClass Gda_ConnectionPoolClass;

#define GDA_TYPE_CONNECTION_POOL            (gda_connection_pool_get_type())
#define GDA_CONNECTION_POOL(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_CONNECTION_POOL, Gda_ConnectionPool)
#define GDA_CONNECTION_POOL_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_CONNECTION_POOL, GdaConnectionPoolClass)
#define IS_GDA_CONNECTION_POOL(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_CONNECTION_POOL)
#define IS_GDA_CONNECTION_POOL_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION_POOL))

struct _Gda_ConnectionPool
{
  GtkObject object;

  GList*    connections;
};

struct _Gda_ConnectionPoolClass
{
  GtkObjectClass parent_class;

  /* signals */
  void (*new_connection)(Gda_ConnectionPool *pool, Gda_Connection *cnc);
};

GtkType             gda_connection_pool_get_type        (void);
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
