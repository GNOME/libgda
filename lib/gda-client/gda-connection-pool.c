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
#include "GNOME_Database.h"
#include <gobject/gsignal.h>
#include <bonobo/bonobo-marshal.h>

static void gda_connection_pool_class_init (GdaConnectionPoolClass * klass);
static void gda_connection_pool_init       (GdaConnectionPool * pool, GdaConnectionPoolClass *klass);
static void gda_connection_pool_finalize   (GObject *object);

/*
 * GdaConnectionPool object signals
 */
enum {
	OPEN,
	ERROR,
	LAST_SIGNAL
};

static gint gda_connection_pool_signals[LAST_SIGNAL] = { 0, };

/*
 * Callbacks
 */
static void
connection_destroyed_cb (GObject * object, gpointer user_data)
{
	GdaConnection *cnc = (GdaConnection *) object;
	GdaConnectionPool *pool = (GdaConnectionPool *) user_data;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));

	pool->connections = g_list_remove (pool->connections, (gpointer) cnc);
}

static void
connection_error_cb (GdaConnection * cnc, GList * errors, gpointer user_data)
{
	GdaConnectionPool *pool = (GdaConnectionPool *) user_data;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));

	g_signal_emit (G_OBJECT (pool),
		       gda_connection_pool_signals[ERROR], 0, cnc, errors);
}

static void
connection_opened_cb (GdaConnection * cnc, gpointer user_data)
{
	GdaConnectionPool *pool = (GdaConnectionPool *) user_data;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));

	g_signal_emit (G_OBJECT (pool),
		       gda_connection_pool_signals[OPEN], 0, cnc);
}

/*
 * GdaConnectionPool object interface
 */
static void
gda_connection_pool_class_init (GdaConnectionPoolClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gda_connection_pool_signals[OPEN] =
		g_signal_new ("open",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionPoolClass, open),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_connection_pool_signals[ERROR] =
		gtk_signal_new ("error",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GdaConnectionPoolClass, error),
				NULL, NULL,
				bonobo_marshal_VOID__POINTER_POINTER,
				G_TYPE_NONE, 2, G_TYPE_POINTER,
				G_TYPE_POINTER);

	object_class->finalize = gda_connection_pool_finalize;
	klass->open = NULL;
	klass->error = NULL;
}

static void
gda_connection_pool_init (GdaConnectionPool *pool, GdaConnectionPoolClass *klass)
{
	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));

	pool->connections = NULL;
}

GType
gda_connection_pool_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaConnectionPoolClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_connection_pool_class_init,
			NULL,
			NULL,
			sizeof (GdaConnectionPool),
			0,
			(GInstanceInitFunc) gda_connection_pool_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaConnectionPool", &info, 0);
	}
	return type;
}

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
	return GDA_CONNECTION_POOL (gtk_object_new (GDA_TYPE_CONNECTION_POOL));
}

static void
gda_connection_pool_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaConnectionPool *pool = (GdaConnectionPool *) object;

	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));

	gda_connection_pool_close_all (pool);

	parent_class = G_OBJECT_CLASS (g_type_class_peek (G_TYPE_OBJECT));
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

/**
 * gda_connection_pool_free
 */
void
gda_connection_pool_free (GdaConnectionPool * pool)
{
	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));
	g_object_unref (G_OBJECT (pool));
}

/**
 * gda_connection_pool_open_connection
 * @pool: a #GdaConnectionPool object
 * @gda_name: GDA data source name
 * @username: user name
 * @password: password
 */
GdaConnection *
gda_connection_pool_open_connection (GdaConnectionPool * pool,
				     const gchar * gda_name,
				     const gchar * username,
				     const gchar * password)
{
	GdaDsn *dsn;
	GdaConnection *cnc;
	GList *node;

	g_return_val_if_fail (GDA_IS_CONNECTION_POOL (pool), NULL);
	g_return_val_if_fail (gda_name != NULL, NULL);

	/* look for the connection already open */
	for (node = g_list_first (pool->connections); node;
	     node = g_list_next (node)) {
		cnc = GDA_CONNECTION (node->data);
		if (GDA_IS_CONNECTION (cnc)) {
			dsn = (GdaDsn *) g_object_get_data (G_OBJECT (cnc),
							    "GDA_ConnectionPool_DSN");
			if (dsn) {
				gchar *dsn_username;
				gchar *dsn_password;

				dsn_username = gda_connection_get_user (cnc);
				dsn_password = gda_connection_get_password (cnc);
				if (!g_strcasecmp (gda_name, GDA_DSN_GDA_NAME (dsn))
				    && (!g_strcasecmp (username, dsn_username)
					|| (!username && !dsn_username))
				    && (!g_strcasecmp (password, dsn_password)
					|| (!password && !dsn_password))) {
					/* emit the open signal, for top-level uses */
					g_signal_emit (GTK_OBJECT (pool),
						       gda_connection_pool_signals[OPEN],
						       0, cnc);
					g_object_ref (G_OBJECT (cnc));
					gda_config_save_last_connection (gda_name, username);

					return cnc;
				}
			}
		}
	}

	/* not found, so create new connection */
	dsn = gda_dsn_find_by_name (gda_name);
	if (dsn) {
		cnc = gda_connection_new (gda_corba_get_orb ());
		gda_connection_set_provider (cnc, GDA_DSN_PROVIDER (dsn));
		g_object_set_data (G_OBJECT (cnc),
				   "GDA_ConnectionPool_DSN",
				   (gpointer) dsn);

		g_signal_connect (GTK_OBJECT (cnc),
				  "open",
				  G_CALLBACK (connection_opened_cb),
				  (gpointer) pool);
		g_signal_connect (G_OBJECT (cnc),
				  "destroy",
				  G_CALLBACK (connection_destroyed_cb),
				  (gpointer) pool);
		g_signal_connect (G_OBJECT (cnc),
				  "error",
				  G_CALLBACK (connection_error_cb),
				  (gpointer) pool);

		/* open the connection */
		if (gda_connection_open (cnc, GDA_DSN_DSN (dsn), username, password) != 0) {
			g_warning (_("could not open connection to %s"),
				   gda_name);
			gda_connection_free (cnc);
			return NULL;
		}

		pool->connections = g_list_append (pool->connections, (gpointer) cnc);
		gda_config_save_last_connection (gda_name, username);

		return cnc;
	}
	else
		g_warning (_("Data source %s not found"), gda_name);

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
gda_connection_pool_close_connection (GdaConnectionPool * pool,
				      GdaConnection * cnc)
{
	GList *node;

	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* look for the given connection in our list */
	for (node = g_list_first (pool->connections); node != NULL;
	     node = g_list_next (node)) {
		GdaConnection *tmp = GDA_CONNECTION (node->data);

		if (tmp == cnc) {
			/* found, unref the object */
			g_object_unref (G_OBJECT (cnc));
			break;
		}
	}
}

/**
 * gda_connection_pool_close_all
 * @pool: a #GdaConnectionPool object
 */
void
gda_connection_pool_close_all (GdaConnectionPool * pool)
{
	GList *node;

	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));

	/* close all open connections */
	while ((node = g_list_first (pool->connections))) {
		GdaDsn *dsn;
		GdaConnection *cnc = GDA_CONNECTION (node->data);

		pool->connections = g_list_remove (pool->connections, (gpointer) cnc);
		if (GDA_IS_CONNECTION (cnc)) {
			dsn = (GdaDsn *) g_object_get_data (G_OBJECT (cnc),
							    "GDA_ConnectionPool_DSN");
			if (dsn)
				gda_dsn_free (dsn);
			gda_connection_free (cnc);
		}
	}
}

/**
 * gda_connection_pool_foreach
 */
void
gda_connection_pool_foreach (GdaConnectionPool * pool,
			     GdaConnectionPoolForeachFunc func,
			     gpointer user_data)
{
	GList *l;

	g_return_if_fail (GDA_IS_CONNECTION_POOL (pool));
	g_return_if_fail (func != NULL);

	for (l = g_list_first (pool->connections); l != NULL;
	     l = g_list_next (l)) {
		GdaConnection *cnc = (GdaConnection *) l->data;

		if (GDA_IS_CONNECTION (cnc)) {
			GdaDsn *dsn;

			dsn = (GdaDsn *) g_object_get_data (G_OBJECT (cnc),
							    "GDA_ConnectionPool_DSN");
			func (pool,
			      cnc,
			      GDA_DSN_GDA_NAME (dsn),
			      gda_connection_get_user (cnc),
			      gda_connection_get_password (cnc), user_data);
		}
	}
}
