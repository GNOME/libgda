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

#include "config.h"
#include "gda-connection-pool.h"
#include "gda-common.h"
#include "GDA.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough. */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#ifdef HAVE_GOBJECT
static void gda_connection_pool_class_init (Gda_ConnectionPoolClass *klass,
                                            gpointer data);
static void gda_connection_pool_init       (Gda_ConnectionPool *pool,
                                            Gda_ConnectionPoolClass *klass);
static void gda_connection_pool_finalize   (GObject *object);
#else
static void gda_connection_pool_class_init (Gda_ConnectionPoolClass *klass);
static void gda_connection_pool_init       (Gda_ConnectionPool *pool);
static void gda_connection_pool_destroy    (Gda_ConnectionPool *pool);
#endif

/*
 * Gda_ConnectionPool object signals
 */
enum
{
  NEW_CONNECTION,
  LAST_SIGNAL
};

static gint gda_connection_pool_signals[LAST_SIGNAL] = { 0, };

/*
 * Gda_ConnectionPool object interface
 */
#ifdef HAVE_GOBJECT
static void
gda_connection_pool_class_init (Gda_ConnectionPoolClass *klass,
                                gpointer data)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  /* FIXME: No signals in GObject yet */
  object_class->finalize = &gda_connection_pool_finalize;
  klass->parent = g_type_class_peek_parent (klass);
  klass->new_connection = NULL;
}
#else
static void
gda_connection_pool_class_init (Gda_ConnectionPoolClass *klass)
{
  GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

  gda_connection_pool_signals[NEW_CONNECTION] =
    gtk_signal_new("new_connection",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(Gda_ConnectionPoolClass, new_connection),
		   gtk_marshal_NONE__POINTER,
		   GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

  gtk_object_class_add_signals(object_class, gda_connection_pool_signals, LAST_SIGNAL);
  object_class->destroy = gda_connection_pool_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_connection_pool_init (Gda_ConnectionPool *pool,
                          Gda_ConnectionPoolClass *klass)
#else
gda_connection_pool_init (Gda_ConnectionPool *pool)
#endif
{
  g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));

  pool->connections = NULL;
}

#ifdef HAVE_GOBJECT
GType
gda_connection_pool_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo info =
      {
        sizeof (Gda_ConnectionPoolClass),                /* class_size */
	NULL,                                            /* base_init */
	NULL,                                            /* base_finalize */
        (GClassInitFunc) gda_connection_pool_class_init, /* class_init */
	NULL,                                            /* class_finalize */
	NULL,                                            /* class_data */
        sizeof (Gda_ConnectionPool),                     /* instance_size */
	0,                                               /* n_preallocs */
        (GInstanceInitFunc) gda_connection_pool_init,    /* instance_init */
	NULL,                                            /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "Gda_ConnectionPool",
                                     &info);
    }
  return type;
}
#else
GtkType
gda_connection_pool_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
	"Gda_ConnectionPool",
	sizeof (Gda_ConnectionPool),
	sizeof (Gda_ConnectionPoolClass),
	(GtkClassInitFunc) gda_connection_pool_class_init,
	(GtkObjectInitFunc) gda_connection_pool_init,
	(GtkArgSetFunc) NULL,
	(GtkArgSetFunc) NULL
      };
      type = gtk_type_unique(gtk_object_get_type(), &info);
    }
  return type;
}
#endif

/**
 * gda_connection_pool_new
 *
 * Create a new #Gda_ConnectionPool object, which is used to manage connection
 * pools. These are very useful if you've got to manage several connections
 * to different data sources
 */
Gda_ConnectionPool *
gda_connection_pool_new (void)
{
#ifdef HAVE_GOBJECT
  return GDA_CONNECTION_POOL (g_object_new (GDA_TYPE_CONNECTION_POOL, NULL));
#else
  return GDA_CONNECTION_POOL(gtk_type_new(gda_connection_pool_get_type()));
#endif
}

#ifdef HAVE_GOBJECT
static void
gda_connection_pool_finalize (GObject *object)
{
  Gda_ConnectionPool *pool = GDA_CONNECTION_POOL (object);
  Gda_ConnectionPoolClass *klass =
    G_TYPE_INSTANCE_GET_CLASS (object, GDA_CONNECTION_POOL_CLASS,
                               Gda_ConnectionPoolClass);

  gda_connection_pool_close_all (pool);
  klass->parent->finalize (object);
}
#else
static void
gda_connection_pool_destroy (Gda_ConnectionPool *pool)
{
  gda_connection_pool_close_all(pool);
}
#endif

/**
 * gda_connection_pool_free
 */
void
gda_connection_pool_free (Gda_ConnectionPool *pool)
{
  g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));
#ifdef HAVE_GOBJECT
  g_object_unref (G_OBJECT (pool));
#else
  gtk_object_destroy(GTK_OBJECT(pool));
#endif
}

/**
 * gda_connection_pool_open_connection
 * @pool: a #Gda_ConnectionPool object
 * @gda_name: GDA data source name
 * @username: user name
 * @password: password
 */
Gda_Connection *
gda_connection_pool_open_connection (Gda_ConnectionPool *pool,
				     const gchar *gda_name,
				     const gchar *username,
				     const gchar *password)
{
  Gda_Dsn*        dsn;
  Gda_Connection* cnc;
  GList*          node;

  g_return_val_if_fail(IS_GDA_CONNECTION_POOL(pool), NULL);
  g_return_val_if_fail(gda_name != NULL, NULL);

  /* look for the connection already open */
  for (node = g_list_first(pool->connections); node; node = g_list_next(node))
    {
      cnc = GDA_CONNECTION(node->data);
      if (IS_GDA_CONNECTION(cnc))
	{
#ifndef HAVE_GOBJECT
	  dsn = (Gda_Dsn *) gtk_object_get_data(GTK_OBJECT(cnc), "GDA_ConnectionPool_DSN");
#endif
	  if (dsn)
	    {
	      gchar* dsn_username;
	      gchar* dsn_password;

	      dsn_username = gda_connection_get_user(cnc);
	      dsn_password = gda_connection_get_password(cnc);
	      if (!g_strcasecmp(gda_name, GDA_DSN_GDA_NAME(dsn)) &&
		  (!g_strcasecmp(username, dsn_username) || (!username && !dsn_username)) &&
		  (!g_strcasecmp(password, dsn_password) || (!password && !dsn_password)))
		return cnc;
	    }
	}
    }

  /* not found, so create new connection */
  dsn = gda_dsn_find_by_name(gda_name);
  if (dsn)
    {
      cnc = gda_connection_new(gda_corba_get_orb());
      gda_connection_set_provider(cnc, GDA_DSN_PROVIDER(dsn));
#ifndef HAVE_GOBJECT /* FIXME */
      gtk_object_set_data(GTK_OBJECT(cnc), "GDA_ConnectionPool_DSN", (gpointer) dsn);
#endif
      if (gda_connection_open(cnc, GDA_DSN_DSN(dsn), username, password) != 0)
	{
	  g_warning(_("could not open connection to %s"), gda_name);
	  gda_connection_free(cnc);
	  return NULL;
	}

      pool->connections = g_list_append(pool->connections, (gpointer) cnc);
#ifndef HAVE_GOBJECT /* FIXME */
      gtk_signal_emit(GTK_OBJECT(pool),
		      gda_connection_pool_signals[NEW_CONNECTION],
		      cnc);
#endif
      return cnc;
    }
  else g_warning(_("Data source %s not found"), gda_name);
  return NULL;
}

/**
 * gda_connection_pool_close_all
 * @pool: a #Gda_ConnectionPool object
 */
void
gda_connection_pool_close_all (Gda_ConnectionPool *pool)
{
  GList* node;

  g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));

  /* close all open connections */
  while ((node = g_list_first(pool->connections)))
    {
      Gda_Dsn*        dsn;
      Gda_Connection* cnc = GDA_CONNECTION(node->data);
      pool->connections = g_list_remove(pool->connections, (gpointer) cnc);
      if (IS_GDA_CONNECTION(cnc))
	{
#ifndef HAVE_GOBJECT
	  dsn = (Gda_Dsn *) gtk_object_get_data(GTK_OBJECT(cnc), "GDA_ConnectionPool_DSN");
#endif
	  if (dsn) gda_dsn_free(dsn);
	  gda_connection_free(cnc);
	}
    }
}
