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

#include "config.h"
#include "gda-connection-pool.h"
#include "gda-common.h"
#include "GDA.h"

#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

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
static void gda_connection_pool_class_init (GdaConnectionPoolClass *klass,
                                            gpointer data);
static void gda_connection_pool_init (GdaConnectionPool *pool,
									  GdaConnectionPoolClass *klass);
static void gda_connection_pool_finalize (GObject *object);
#else
static void gda_connection_pool_class_init (GdaConnectionPoolClass *klass);
static void gda_connection_pool_init (GdaConnectionPool *pool);
static void gda_connection_pool_destroy (GdaConnectionPool *pool);
#endif

/*
 * GdaConnectionPool object signals
 */
enum {
	OPEN,
	LAST_SIGNAL
};

static gint gda_connection_pool_signals[LAST_SIGNAL] = { 0, };

/*
 * Callbacks
 */
static void
connection_destroyed_cb (GtkObject *object, gpointer user_data)
{
	GdaConnection* cnc = (GdaConnection *) object;
	GdaConnectionPool* pool = (GdaConnectionPool *) user_data;

	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));

	pool->connections = g_list_remove(pool->connections, (gpointer) cnc);
}

static void
connection_opened_cb (GdaConnection *cnc, gpointer user_data)
{
	GdaConnectionPool *pool = (GdaConnectionPool *) user_data;

	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));

#ifndef HAVE_GOBJECT /* FIXME */
	gtk_signal_emit(GTK_OBJECT(pool),
					gda_connection_pool_signals[OPEN],
					cnc);
#endif
}

/*
 * GdaConnectionPool object interface
 */
#ifdef HAVE_GOBJECT
static void
gda_connection_pool_class_init (GdaConnectionPoolClass *klass,
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
gda_connection_pool_class_init (GdaConnectionPoolClass *klass)
{
	GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

	gda_connection_pool_signals[OPEN] =
		gtk_signal_new("open",
					   GTK_RUN_FIRST,
					   object_class->type,
					   GTK_SIGNAL_OFFSET(GdaConnectionPoolClass, open),
					   gtk_marshal_NONE__POINTER,
					   GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals(object_class, gda_connection_pool_signals, LAST_SIGNAL);
	object_class->destroy = gda_connection_pool_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_connection_pool_init (GdaConnectionPool *pool,
                          GdaConnectionPoolClass *klass)
#else
gda_connection_pool_init (GdaConnectionPool *pool)
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

	if (!type) {
		GTypeInfo info = {
			sizeof (GdaConnectionPoolClass),                /* class_size */
			NULL,                                            /* base_init */
			NULL,                                            /* base_finalize */
			(GClassInitFunc) gda_connection_pool_class_init, /* class_init */
			NULL,                                            /* class_finalize */
			NULL,                                            /* class_data */
			sizeof (GdaConnectionPool),                     /* instance_size */
			0,                                               /* n_preallocs */
			(GInstanceInitFunc) gda_connection_pool_init,    /* instance_init */
			NULL,                                            /* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaConnectionPool",
					       &info, 0);
	}
	return type;
}
#else
GtkType
gda_connection_pool_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaConnectionPool",
			sizeof (GdaConnectionPool),
			sizeof (GdaConnectionPoolClass),
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
 * Create a new #GdaConnectionPool object, which is used to manage connection
 * pools. These are very useful if you've got to manage several connections
 * to different data sources
 */
GdaConnectionPool *
gda_connection_pool_new (void)
{
#ifdef HAVE_GOBJECT
	return GDA_CONNECTION_POOL(g_object_new(GDA_TYPE_CONNECTION_POOL, NULL));
#else
	return GDA_CONNECTION_POOL(gtk_type_new(gda_connection_pool_get_type()));
#endif
}

#ifdef HAVE_GOBJECT
static void
gda_connection_pool_finalize (GObject *object)
{
	GdaConnectionPool *pool = GDA_CONNECTION_POOL (object);
	GdaConnectionPoolClass *klass =
		G_TYPE_INSTANCE_GET_CLASS (object, GDA_CONNECTION_POOL_CLASS,
								   GdaConnectionPoolClass);

	gda_connection_pool_close_all (pool);
	klass->parent->finalize (object);
}
#else
static void
gda_connection_pool_destroy (GdaConnectionPool *pool)
{
	gda_connection_pool_close_all(pool);
}
#endif

/**
 * gda_connection_pool_free
 */
void
gda_connection_pool_free (GdaConnectionPool *pool)
{
	g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (pool));
#else
	gtk_object_unref(GTK_OBJECT(pool));
#endif
}

/**
 * gda_connection_pool_open_connection
 * @pool: a #GdaConnectionPool object
 * @gda_name: GDA data source name
 * @username: user name
 * @password: password
 */
GdaConnection *
gda_connection_pool_open_connection (GdaConnectionPool *pool,
									 const gchar *gda_name,
									 const gchar *username,
									 const gchar *password)
{
	GdaDsn*        dsn;
	GdaConnection* cnc;
	GList*          node;

	g_return_val_if_fail(IS_GDA_CONNECTION_POOL(pool), NULL);
	g_return_val_if_fail(gda_name != NULL, NULL);

	/* look for the connection already open */
	for (node = g_list_first(pool->connections); node; node = g_list_next(node)) {
		cnc = GDA_CONNECTION(node->data);
		if (IS_GDA_CONNECTION(cnc)) {
#ifndef HAVE_GOBJECT
			dsn = (GdaDsn *) gtk_object_get_data(GTK_OBJECT(cnc), "GDA_ConnectionPool_DSN");
#endif
			if (dsn) {
				gchar* dsn_username;
				gchar* dsn_password;

				dsn_username = gda_connection_get_user(cnc);
				dsn_password = gda_connection_get_password(cnc);
				if (!g_strcasecmp(gda_name, GDA_DSN_GDA_NAME(dsn)) &&
					(!g_strcasecmp(username, dsn_username) || (!username && !dsn_username)) &&
					(!g_strcasecmp(password, dsn_password) || (!password && !dsn_password))) {
#ifndef HAVE_GOBJECT /* FIXME */
					/* emit the open signal, for top-level uses */
					gtk_signal_emit(GTK_OBJECT(pool),
									gda_connection_pool_signals[OPEN],
									cnc);
					gtk_object_ref(GTK_OBJECT(cnc));
#endif
					return cnc;
				}
			}
		}
    }

	/* not found, so create new connection */
	dsn = gda_dsn_find_by_name(gda_name);
	if (dsn) {
		cnc = gda_connection_new(gda_corba_get_orb());
		gda_connection_set_provider(cnc, GDA_DSN_PROVIDER(dsn));
#ifndef HAVE_GOBJECT /* FIXME */
		gtk_object_set_data(GTK_OBJECT(cnc), "GDA_ConnectionPool_DSN", (gpointer) dsn);

		gtk_signal_connect(GTK_OBJECT(cnc),
						   "open",
						   GTK_SIGNAL_FUNC(connection_opened_cb),
						   (gpointer) pool);
		gtk_signal_connect(GTK_OBJECT(cnc),
						   "destroy",
						   GTK_SIGNAL_FUNC(connection_destroyed_cb),
						   (gpointer) pool);
#endif

		/* open the connection */
		if (gda_connection_open(cnc, GDA_DSN_DSN(dsn), username, password) != 0) {
			g_warning(_("could not open connection to %s"), gda_name);
			gda_connection_free(cnc);
			return NULL;
		}

		pool->connections = g_list_append(pool->connections, (gpointer) cnc);
		return cnc;
    }
	else g_warning(_("Data source %s not found"), gda_name);
	return NULL;
}

/**
 * gda_connection_pool_close_connection
 * @pool: a #GdaConnectionPool object
 * @cnc: a #GdaConnection object
 *
 * Detach a client from the given connection in the specified pool. That
 * is, it removes the reference from that client to the #GdaConnection
 * object. From each connection open, the #GdaConnectionPool keeps
 * a reference count, so that it knows always how many clients are using
 * each of the #GdaConnection objects being managed by the pool. This way,
 * it does not actually close the connection until no clients are attached
 * to it.
 */
void
gda_connection_pool_close_connection (GdaConnectionPool *pool, GdaConnection *cnc)
{
	GList* node;

	g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));
	g_return_if_fail(IS_GDA_CONNECTION(cnc));

	/* look for the given connection in our list */
	for (node = g_list_first(pool->connections); node != NULL; node = g_list_next(node)) {
		GdaConnection* tmp = GDA_CONNECTION(node->data);

		if (tmp == cnc) {
			/* found, unref the object */
#ifdef HAVE_GOBJECT
			g_object_unref(G_OBJECT(cnc));
#else
			gtk_object_unref(GTK_OBJECT(cnc));
#endif
		}
	}
}

/**
 * gda_connection_pool_close_all
 * @pool: a #GdaConnectionPool object
 */
void
gda_connection_pool_close_all (GdaConnectionPool *pool)
{
	GList* node;

	g_return_if_fail(IS_GDA_CONNECTION_POOL(pool));

	/* close all open connections */
	while ((node = g_list_first(pool->connections))) {
		GdaDsn*        dsn;
		GdaConnection* cnc = GDA_CONNECTION(node->data);

		pool->connections = g_list_remove(pool->connections, (gpointer) cnc);
		if (IS_GDA_CONNECTION(cnc)) {
#ifndef HAVE_GOBJECT
			dsn = (GdaDsn *) gtk_object_get_data(GTK_OBJECT(cnc), "GDA_ConnectionPool_DSN");
#endif
			if (dsn)
				gda_dsn_free(dsn);
			gda_connection_free(cnc);
		}
    }
}
