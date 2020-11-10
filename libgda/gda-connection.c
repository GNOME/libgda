/*
 * Copyright (C) 2000 - 2001 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2002 Cleber Rodrigues <cleberrrjr@bol.com.br>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@src.gnome.org>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2004 José María Casanova Crespo <jmcasanova@igalia.com>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2005 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2006 - 2012 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2008 Johannes Schmid <jschmid@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011, 2018-2019 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
 * Copyright (C) 2014 Anders Jonsson <anders.jonsson@norsjovallen.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#undef GDA_DISABLE_DEPRECATED
#undef GSEAL_ENABLE

#define G_LOG_DOMAIN "GDA-connection"

#include <stdio.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-internal.h>
#include <libgda/gda-connection-event.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-private.h>
#include "gda-marshal.h"
#include <libgda/gda-transaction-status-private.h>
#include <string.h>
#include <libgda/gda-enum-types.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-set.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-statement-struct-trans.h>
#include <libgda/gda-meta-store-extra.h>
#include <libgda/gda-util.h>
#include <libgda/gda-lockable.h>
#include <libgda/gda-repetitive-statement.h>
#include "gda-statement-priv.h"
#include <libgda/sqlite/virtual/gda-vconnection-data-model.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-data-handler.h>
#include <libgda/thread-wrapper/itsignaler.h>
#include <libgda/gda-marshal.h>

#include <glib/gstdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static GMutex global_mutex;
static GdaSqlParser *internal_parser = NULL;
static GHashTable *all_context_hash = NULL; /* key = a #GThread, value = a #GMainContext (ref held) */

/* GdaLockable interface */
static void                 gda_connection_lockable_init (GdaLockableInterface *iface);
static void                 gda_connection_lock      (GdaLockable *lockable);
static gboolean             gda_connection_trylock   (GdaLockable *lockable);
static void                 gda_connection_unlock    (GdaLockable *lockable);

/* number of GdaConnectionEvent kept by each connection. Should be enough to avoid losing any
 * event, considering that the events are reseted after each statement execution */
#define EVENTS_ARRAY_SIZE 5

typedef struct {
	GdaServerProvider    *provider_obj;
	GdaConnectionOptions  options; /* ORed flags */
	gchar                *dsn;
	gchar                *cnc_string;
	gchar                *auth_string;

	GHashTable           *context_hash; /* key = a #GThread, value = a #GMainContext (ref held) */

	GdaMetaStore         *meta_store;

	gboolean              auto_clear_events; /* TRUE if events_list is cleared before any statement execution */
	GdaConnectionEvent  **events_array; /* circular array */
	gint                  events_array_size;
	gboolean              events_array_full;
	gint                  events_array_next;
	GList                *events_list; /* for API compat */

	GdaConnectionStatus   status;
	guint                 busy_count;

	GdaTransactionStatus *trans_status;
	GHashTable           *prepared_stmts;

	GdaServerProviderConnectionData *provider_data;
	GThread              *worker_thread; /* no ref held, used only for comparisons */

	/* multi threading locking */
	GRecMutex             rmutex;
  gboolean              mutex_initalized;

	/* auto meta data update */
	GArray               *trans_meta_context; /* Array of GdaMetaContext pointers */

	gboolean              exec_times;
	guint                 exec_slowdown;
} GdaConnectionPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaConnection, gda_connection, G_TYPE_OBJECT, 
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_LOCKABLE, gda_connection_lockable_init)
                         G_ADD_PRIVATE (GdaConnection))


static void add_exec_time_to_object (GObject *obj, GTimer *timer);

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc);
static void gda_connection_dispose    (GObject *object);
static void gda_connection_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_connection_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);


static void update_meta_store_after_statement_exec (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params);
static void change_events_array_max_size (GdaConnection *cnc, gint size);

enum {
	SIG_ERROR,
	OPENED,
        CLOSED,
	DSN_CHANGED,
	TRANSACTION_STATUS_CHANGED,
	STATUS_CHANGED,
	LAST_SIGNAL
};

static gint gda_connection_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0 };

/* properties */
enum
{
        PROP_0,
        PROP_DSN,
        PROP_CNC_STRING,
        PROP_PROVIDER_OBJ,
        PROP_AUTH_STRING,
        PROP_OPTIONS,
	PROP_META_STORE,
	PROP_EVENTS_HISTORY_SIZE,
	PROP_EXEC_TIMES,
	PROP_EXEC_SLOWDOWN
};

extern GdaServerProvider *_gda_config_sqlite_provider; /* defined in gda-config.c */

static gint debug_level = -1;
static void
dump_exec_params (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params)
{
	if (params && (debug_level & 8)) {
		GSList *list;
		gchar *sql;
		sql = gda_statement_to_sql_extended (stmt, cnc, params, GDA_STATEMENT_SQL_PARAMS_SHORT,
						     NULL, NULL);
#ifdef GDA_DEBUG
		g_print ("EVENT> COMMAND: parameters (on cnx %p) for statement [%s]\n", cnc, sql);
#endif
		for (list = gda_set_get_holders (params); list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			gchar *str;
			const GValue *value;
			value = gda_holder_get_value (holder);
			str = value ? gda_value_stringify (value) : "NULL";
#ifdef GDA_DEBUG
			g_print ("\t%s: type=>%s, value=>%s\n", gda_holder_get_id (holder),
				 gda_g_type_to_string (gda_holder_get_g_type (holder)),
				 str);
#endif
			if (value)
				g_free (str);
		}
		g_free (sql);
	}
}


/*
 * GdaConnection class implementation
 * @klass:
 */
static void
gda_connection_class_init (GdaConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * GdaConnection::error:
	 * @cnc: the #GdaConnection
	 * @event: a #GdaConnectionEvent object
	 *
	 * Gets emitted whenever a connection event occurs. Check the nature of @event to
	 * see if it's an error or a simple notification
	 */
	gda_connection_signals[SIG_ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_CONNECTION_EVENT);
	/**
	 * GdaConnection::opened:
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the connection has been opened to the database
	 */
	gda_connection_signals[OPENED] =
                g_signal_new ("opened",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, opened),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	/**
	 * GdaConnection::closed:
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the connection to the database has been closed
	 */
        gda_connection_signals[CLOSED] =    /* runs after user handlers */
                g_signal_new ("closed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaConnectionClass, closed),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	/**
	 * GdaConnection::dsn-changed:
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the DSN used by @cnc has been changed
	 */
	gda_connection_signals[DSN_CHANGED] =
		g_signal_new ("dsn-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, dsn_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	/**
	 * GdaConnection::transaction-status-changed:
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the transaction status of @cnc has changed (a transaction has been 
	 * started, rolled back, a savepoint added,...)
	 */
	gda_connection_signals[TRANSACTION_STATUS_CHANGED] =
		g_signal_new ("transaction-status-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, transaction_status_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * GdaConnection::status-changed:
	 * @cnc: the #GdaConnection
	 * @status: (type GdaConnectionStatus): the new connection status
	 *
	 * Gets emitted when the @cnc's status has changed (usually when a the connection is being used to execute
	 * a statement)
	 *
	 * Since: 6.0
	 */
	gda_connection_signals[STATUS_CHANGED] =
		g_signal_new ("status-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, status_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1, GDA_TYPE_CONNECTION_STATUS);

	/* Properties */
        object_class->set_property = gda_connection_set_property;
        object_class->get_property = gda_connection_get_property;

	g_object_class_install_property (object_class, PROP_DSN,
                                         g_param_spec_string ("dsn", NULL, _("DSN to use"), NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_CNC_STRING,
                                         g_param_spec_string ("cnc-string", NULL, _("Connection string to use"), NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_PROVIDER_OBJ,
                                         g_param_spec_object ("provider", NULL, _("Provider to use"),
                                                               GDA_TYPE_SERVER_PROVIDER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

        g_object_class_install_property (object_class, PROP_AUTH_STRING,
                                         g_param_spec_string ("auth-string", NULL,_("Authentication string to use"),
                                                              NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_OPTIONS,
                                         g_param_spec_flags ("options", NULL, _("Options"),
							    GDA_TYPE_CONNECTION_OPTIONS, GDA_CONNECTION_OPTIONS_NONE,
							    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_META_STORE,
					 /* To translators: Don't translate "GdaMetaStore", it's a class name */
					 g_param_spec_object ("meta-store", NULL, _ ("GdaMetaStore used by the connection"),
							      GDA_TYPE_META_STORE,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/**
	 * GdaConnection:events-history-size:
	 *
	 * Defines the number of #GdaConnectionEvent objects kept in memory which can
	 * be fetched using gda_connection_get_events().
	 *
	 * Since: 4.2
	 */
	g_object_class_install_property (object_class, PROP_EVENTS_HISTORY_SIZE,
					 g_param_spec_int ("events-history-size", NULL,
							   _("Number of history events to keep in memory"), EVENTS_ARRAY_SIZE, G_MAXINT,
							   EVENTS_ARRAY_SIZE,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
	/**
	 * GdaConnection:execution-timer:
	 *
	 * Computes execution times for each statement executed.
	 *
	 * Since: 4.2.9
	 **/
	g_object_class_install_property (object_class, PROP_EXEC_TIMES,
					 g_param_spec_boolean ("execution-timer", NULL,
							       _("Computes execution delay for each executed statement"),
							       FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/**
	 * GdaConnection:execution-slowdown:
	 *
	 * Artificially slows down the execution of queries. This property can be used to
	 * debug some problems. If non zero, this value is the number of microseconds waited before actually
	 * executing each query.
	 * NB: this parameter is ignored during the meta store update (it is set to 0 before the meta data update
	 * and restored to its state after).
	 *
	 * Since: 5.2.0
	 **/
	g_object_class_install_property (object_class, PROP_EXEC_SLOWDOWN,
					 g_param_spec_uint ("execution-slowdown", NULL,
							    _("Artificially slows down the execution of queries"),
							    0, G_MAXUINT, 0,
							    (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->dispose = gda_connection_dispose;

	/* computing debug level */
	if (debug_level == -1) {
		const gchar *str;
		debug_level = 0;
		str = getenv ("GDA_CONNECTION_EVENTS_SHOW"); /* Flawfinder: ignore */
		if (str) {
			gchar **array;
			guint i;
			guint array_len;
			array = g_strsplit_set (str, " ,/;:", 0);
			array_len = g_strv_length (array);
			for (i = 0; i < array_len; i++) {
				if (!g_ascii_strcasecmp (array[i], "notice"))
					debug_level += 1;
				else if (!g_ascii_strcasecmp (array[i], "warning"))
					debug_level += 2;
				else if (!g_ascii_strcasecmp (array[i], "error"))
					debug_level += 4;
				else if (!g_ascii_strcasecmp (array[i], "command"))
					debug_level += 8;
			}
			g_strfreev (array);
		}
	}
}

static void
gda_connection_lockable_init (GdaLockableInterface *iface)
{
	iface->lock = gda_connection_lock;
	iface->trylock = gda_connection_trylock;
	iface->unlock = gda_connection_unlock;
}

static void
all_context_hash_func (GThread *key, GMainContext *context, GHashTable *copyto)
{
	g_hash_table_insert (copyto, key, g_main_context_ref (context));
}

static void
gda_connection_init (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_rec_mutex_init (&priv->rmutex);
  priv->mutex_initalized = TRUE;
	priv->provider_obj = NULL;
	priv->dsn = NULL;
	priv->cnc_string = NULL;
	priv->auth_string = NULL;
	if (all_context_hash) {
		g_mutex_lock (&global_mutex);
		priv->context_hash = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_main_context_unref);
		g_hash_table_foreach (all_context_hash, (GHFunc) all_context_hash_func, priv->context_hash);
		g_mutex_unlock (&global_mutex);
	}
	else
		priv->context_hash = NULL;
	priv->auto_clear_events = TRUE;
	priv->events_array_size = EVENTS_ARRAY_SIZE;
	priv->events_array = g_new0 (GdaConnectionEvent*, EVENTS_ARRAY_SIZE);
	priv->events_array_full = FALSE;
	priv->events_array_next = 0;
	priv->status = GDA_CONNECTION_STATUS_CLOSED;
	priv->busy_count = 0;
	priv->trans_status = NULL; /* no transaction yet */
	priv->prepared_stmts = NULL;

	priv->trans_meta_context = NULL;
	priv->provider_data = NULL;
	priv->worker_thread = NULL;

	priv->exec_times = FALSE;
	priv->exec_slowdown = 0;
}

static void auto_update_meta_context_free (GdaMetaContext *context);
static void
gda_connection_dispose (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	/* free memory */
	gda_connection_close (cnc, NULL);

	if (priv->context_hash) {
		g_hash_table_destroy (priv->context_hash);
		priv->context_hash = NULL;
	}

	/* get rid of prepared statements to avoid problems */
	if (priv->prepared_stmts) {
		g_hash_table_destroy (priv->prepared_stmts);
		priv->prepared_stmts = NULL;
	}

	if (priv->provider_obj) {
		_gda_server_provider_handlers_clear_for_cnc (priv->provider_obj, cnc);
		g_object_unref (G_OBJECT (priv->provider_obj));
		priv->provider_obj = NULL;
	}

	if (priv->events_list) {
		g_list_free_full (priv->events_list, (GDestroyNotify) g_object_unref);
		priv->events_list = NULL;
	}

	if (priv->events_array) {
		gint i;
		for (i = 0; i < priv->events_array_size ; i++) {
			GdaConnectionEvent *ev;
			ev = priv->events_array [i];
			if (ev)
				g_object_unref (ev);
		}
		g_free (priv->events_array);
		priv->events_array = NULL;
	}

	if (priv->trans_status) {
		g_object_unref (priv->trans_status);
		priv->trans_status = NULL;
	}

	if (priv->meta_store != NULL) {
	        g_object_unref (priv->meta_store);
	        priv->meta_store = NULL;
	}

	if (priv->trans_meta_context) {
		gsize i;
		for (i = 0; i < priv->trans_meta_context->len; i++) {
			GdaMetaContext *context;
			context = g_array_index (priv->trans_meta_context, GdaMetaContext*, i);
			auto_update_meta_context_free (context);
		}
		g_array_free (priv->trans_meta_context, TRUE);
		priv->trans_meta_context = NULL;
	}
	if (priv->dsn) {
		g_free (priv->dsn);
		priv->dsn = NULL;
	}
	if (priv->cnc_string) {
		g_free (priv->cnc_string);
		priv->cnc_string = NULL;
	}
	if (priv->auth_string) {
		g_free (priv->auth_string);
		priv->auth_string = NULL;
	}
  if (priv->mutex_initalized) {
    g_rec_mutex_clear (&priv->rmutex);
    priv->mutex_initalized = FALSE;
  }

	/* chain to parent class */
  G_OBJECT_CLASS (gda_connection_parent_class)->dispose (object);
}

/* module error */
GQuark gda_connection_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_connection_error");
        return quark;
}

static void
gda_connection_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaConnection *cnc;

        cnc = GDA_CONNECTION (object);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
        if (priv) {
                switch (param_id) {
                case PROP_DSN: {
			const gchar *datasource = g_value_get_string (value);
			GdaDsnInfo *dsn;

			gda_connection_lock ((GdaLockable*) cnc);
			if (priv->provider_data) {
				g_warning (_("Can't set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			
			dsn = gda_config_get_dsn_info (datasource);
			if (!dsn) {
				g_warning (_("No DSN named '%s' defined"), datasource);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			
			g_free (priv->dsn);
			priv->dsn = g_strdup (datasource);
#ifdef GDA_DEBUG_signal
			g_print (">> 'DSN_CHANGED' from %s\n", __FUNCTION__);
#endif
			g_signal_emit (G_OBJECT (cnc), gda_connection_signals[DSN_CHANGED], 0);
#ifdef GDA_DEBUG_signal
			g_print ("<< 'DSN_CHANGED' from %s\n", __FUNCTION__);
#endif
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
		}
                case PROP_CNC_STRING:
			gda_connection_lock ((GdaLockable*) cnc);
			if (priv->provider_data) {
				g_warning (_("Can't set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			g_free (priv->cnc_string);
			priv->cnc_string = NULL;
			if (g_value_get_string (value)) 
				priv->cnc_string = g_strdup (g_value_get_string (value));
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
                case PROP_PROVIDER_OBJ:
			gda_connection_lock ((GdaLockable*) cnc);
			if (priv->provider_data) {
				g_warning (_("Can't set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
                        if (priv->provider_obj)
				g_object_unref (priv->provider_obj);

			priv->provider_obj = g_value_get_object (value);
			g_object_ref (G_OBJECT (priv->provider_obj));
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
                case PROP_AUTH_STRING:
			gda_connection_lock ((GdaLockable*) cnc);
			if (priv->provider_data) {
				g_warning (_("Can't set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			else {
				const gchar *str = g_value_get_string (value);
				g_free (priv->auth_string);
				priv->auth_string = NULL;
				if (str)
					priv->auth_string = g_strdup (str);
			}
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
                case PROP_OPTIONS: {
			GdaConnectionOptions flags;
			flags = g_value_get_flags (value);
			gda_connection_lock ((GdaLockable*) cnc);
			if (priv->provider_data &&
			    ((flags & (~GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE)) !=
			     (priv->options & (~GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE)))) {
				g_warning (_("Can't set the '%s' property once the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			priv->options = flags;
			gda_connection_unlock ((GdaLockable*) cnc);
			break;
		}
		case PROP_META_STORE:
			gda_connection_lock ((GdaLockable*) cnc);
			if (priv->meta_store) {
				g_object_unref (priv->meta_store);
				priv->meta_store = NULL;
			}
			priv->meta_store = g_value_get_object (value);
			if (priv->meta_store)
				g_object_ref (priv->meta_store);
			gda_connection_unlock ((GdaLockable*) cnc);
			break;
		case PROP_EVENTS_HISTORY_SIZE:
			gda_connection_lock ((GdaLockable*) cnc);
			change_events_array_max_size (cnc, g_value_get_int (value));
			gda_connection_unlock ((GdaLockable*) cnc);
			break;
		case PROP_EXEC_TIMES:
			priv->exec_times = g_value_get_boolean (value);
			break;
		case PROP_EXEC_SLOWDOWN:
			priv->exec_slowdown = g_value_get_uint (value);
			break;
                }
        }	
}

static void
gda_connection_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaConnection *cnc;

        cnc = GDA_CONNECTION (object);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
        if (priv) {
                switch (param_id) {
                case PROP_DSN:
			g_value_set_string (value, priv->dsn);
                        break;
                case PROP_CNC_STRING:
			g_value_set_string (value, priv->cnc_string);
			break;
                case PROP_PROVIDER_OBJ:
			g_value_set_object (value, (GObject*) priv->provider_obj);
                        break;
                case PROP_AUTH_STRING:
			g_value_set_string (value, priv->auth_string);
                        break;
                case PROP_OPTIONS:
			g_value_set_flags (value, priv->options);
			break;
		case PROP_META_STORE:
			g_value_set_object (value, priv->meta_store);
			break;
		case PROP_EVENTS_HISTORY_SIZE:
			g_value_set_int (value, priv->events_array_size);
			break;
		case PROP_EXEC_TIMES:
			g_value_set_boolean (value, priv->exec_times);
			break;
		case PROP_EXEC_SLOWDOWN:
			g_value_set_uint (value, priv->exec_slowdown);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }	
}

/**
 * gda_connection_set_main_context:
 * @cnc: (nullable): a #GdaConnection, or %NULL
 * @thread: (nullable): the #GThread in which @context will be used, or %NULL (for the current thread)
 * @context: (nullable): a #GMainContext, or %NULL
 *
 * Defines the #GMainContext which will still process events while a potentially blocking operation is performed using
 * @cnc. If @cnc is %NULL, then this function applies to all the connections, except the ones for which a different
 * context has been defined (be it user defined connections or internal connections used in other objects).
 * On the other hand, if @cnc is not %NULL, then the setting only applied to @cnc.
 *
 * For exemple if there is a GUI which needs to continue to handle events, then you can use this function to pass
 * the default #GMainContext used for the UI refreshing, for example:
 *
 * <programlisting><![CDATA[GMainContext *context;
 * cnc = gda_connection_new_...;
 * context = g_main_context_ref_thread_default ();
 * gda_connection_set_main_context (cnc, NULL, context);
 * g_main_context_unref (context);
 * GError *error = NULL;
 * if (! gda_connection_open (cnc, &error))
 *     ...
 * ]]></programlisting>
 *
 * If @context is %NULL, then potentially blocking operation will actually block any event from being processed
 * while the blocking operation is being performed.
 *
 * Since: 6.0
 */
void
gda_connection_set_main_context (GdaConnection *cnc, GThread *thread, GMainContext *context)
{
	if (!thread)
		thread = g_thread_self ();

	g_mutex_lock (&global_mutex);

	if (cnc) {
		g_return_if_fail (GDA_IS_CONNECTION (cnc));
		GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
		if (context) {
			if (! priv->context_hash)
				priv->context_hash = g_hash_table_new_full (NULL, NULL, NULL,
										 (GDestroyNotify) g_main_context_unref);
			g_main_context_ref (context);
			g_hash_table_insert (priv->context_hash, thread, g_main_context_ref (context));
			g_main_context_unref (context);
		}
		else if (priv->context_hash)
			g_hash_table_remove (priv->context_hash, thread);
	}
	else {
		if (context) {
			if (! all_context_hash)
				all_context_hash = g_hash_table_new_full (NULL, NULL, NULL,
									  (GDestroyNotify) g_main_context_unref);
			g_main_context_ref (context);
			g_hash_table_insert (all_context_hash, thread, g_main_context_ref (context));
			g_main_context_unref (context);
		}
		else if (all_context_hash)
			g_hash_table_remove (all_context_hash, thread);
	}

	g_mutex_unlock (&global_mutex);
}

/**
 * gda_connection_get_main_context:
 * @cnc: (nullable): a #GdaConnection, or %NULL
 * @thread: (nullable): the #GThread in which @context will be used, or %NULL (for the current thread)
 *
 * Get the #GMainContext used while a potentially blocking operation is performed using @nc, see
 * gda_connection_set_main_context(). If @cnc is %NULL, then the setting applies to all the connections for which
 * no other similar setting has been set.
 *
 * If no main context has been defined, then some function calls (for example connection opening) may block until the
 * operation has finished.
 *
 * Returns: (transfer none): a #GMainContext, or %NULL
 *
 * Since: 6.0
 */
GMainContext *
gda_connection_get_main_context (GdaConnection *cnc, GThread *thread)
{
	if (!thread)
		thread = g_thread_self ();

	GMainContext *context = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
		g_mutex_lock (&global_mutex);
		if (priv->context_hash)
			context = g_hash_table_lookup (priv->context_hash, thread);
		if (!context && all_context_hash)
			context = g_hash_table_lookup (all_context_hash, thread);
		g_mutex_unlock (&global_mutex);
	}
	else {
		g_mutex_lock (&global_mutex);
		if (all_context_hash)
			context = g_hash_table_lookup (all_context_hash, thread);
		g_mutex_unlock (&global_mutex);
	}

	return context;
}

/*
 * Returns: a new GType array, free using g_free(), or %NULL if none to compute
 */
static GType *
merge_column_types (const GType *struct_types, const GType *user_types)
{
	if (! user_types || !struct_types)
		return NULL;
	GArray *array;
	guint i;
	array = g_array_new (TRUE, FALSE, sizeof (GType));
	for (i = 0;
	     (user_types [i] != G_TYPE_NONE) && (struct_types [i] != G_TYPE_NONE);
	     i++) {
		GType type;
		if (user_types [i] == 0)
			type = struct_types [i];
		else
			type = user_types [i];
		g_array_append_val (array, type);
	}
	if (user_types [i] != G_TYPE_NONE) {
		for (; user_types [i] != G_TYPE_NONE; i++) {
			GType type = user_types [i];
			g_array_append_val (array, type);
		}
	}
	else {
		for (; struct_types [i] != G_TYPE_NONE; i++) {
			GType type = struct_types [i];
			g_array_append_val (array, type);
		}
	}
	GType *retval;
	guint len;
	len = array->len;
	retval = (GType*) g_array_free (array, FALSE);
	retval [len] = G_TYPE_NONE;
	return retval;
}

/**
 * gda_connection_new_from_dsn_name:
 * @dsn_name: data source name.
 * @auth_string: (nullable): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function creates a new function, using a pre-defined data source (DSN) name,
 * see gda_config_define_dsn() for more information about how to define a DSN. If you don't want to define
 * a DSN, it is possible to use gda_connection_new_from_string() instead of this method.
 *
 * The @dsn string must have the following format: "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" 
 * (if &lt;username&gt; and/or &lt;password&gt; are provided, and @auth_string is %NULL, then these username
 * and passwords will be used). Note that if provided, &lt;username&gt; and &lt;password&gt; 
 * must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * The @auth_string can contain the authentication information for the server
 * to accept the connection. It is a string containing semi-colon separated named value, usually 
 * like "USERNAME=...;PASSWORD=..." where the ... are replaced by actual values. Note that each
 * name and value must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * The actual named parameters required depend on the provider being used, and that list is available
 * as the <parameter>auth_params</parameter> member of the #GdaProviderInfo structure for each installed
 * provider (use gda_config_get_provider_info() to get it). Also one can use the "gda-sql-6.0 -L" command
 * to list the possible named parameters.
 *
 * This method may fail with a GDA_CONNECTION_ERROR domain error (see the #GdaConnectionError error codes)
 * or a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * The returned connection is not yet opened, you need to call gda_connection_open() or gda_connection_open_async().
 *
 * Returns: (transfer full): a new #GdaConnection or %NULL if there was an error.
 *
 * Since: 6.0
 */
GdaConnection *
gda_connection_new_from_dsn_name (const gchar *dsn_name,
                                  const gchar *auth_string,
                                  GdaConnectionOptions options,
                                  GError **error)
{
	GdaConnection *cnc = NULL;
	GdaDsnInfo *dsn_info;
	gchar *user, *pass, *real_dsn;
	gchar *real_auth_string = NULL;

	g_return_val_if_fail (dsn_name && *dsn_name, NULL);

	gda_dsn_split (dsn_name, &real_dsn, &user, &pass);
	if (!real_dsn) {
		g_free (user);
		g_free (pass);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Malformed data source specification '%s'"), dsn_name);
		return NULL;
	}

	/* get the data source info */
	dsn_info = gda_config_get_dsn_info (real_dsn);
	if (!dsn_info) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Data source %s not found in configuration"), real_dsn);
		g_free (real_dsn);
		g_free (user);
		g_free (pass);
		return NULL;
	}
	if (!auth_string && user) {
		gchar *s1;
		s1 = gda_rfc1738_encode (user);
		if (pass) {
			gchar *s2;
			s2 = gda_rfc1738_encode (pass);
			real_auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", s1, s2);
			g_free (s2);
		}
		else
			real_auth_string = g_strdup_printf ("USERNAME=%s", s1);
		g_free (s1);
	}

	/* try to find provider */
	if (dsn_info->provider != NULL) {
		GdaProviderInfo *pinfo;
		GdaServerProvider *prov = NULL;

		pinfo = gda_config_get_provider_info (dsn_info->provider);
		if (pinfo)
			prov = gda_config_get_provider (dsn_info->provider, error);
		else
			g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
				     _("No provider '%s' installed"), dsn_info->provider);

		if (prov)
			cnc = _gda_server_provider_create_connection (prov, real_dsn, NULL,
								      auth_string ? auth_string : real_auth_string,
								      options);
	}
	else 
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR, 
			      "%s", _("Datasource configuration error: no provider specified"));

	g_free (real_auth_string);
	g_free (real_dsn);
	g_free (user);
	g_free (pass);
	return cnc;
}

/**
 * gda_connection_new_from_dsn:
 * @dsn: data source name.
 * @auth_string: (nullable): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function creates a new connection, using a pre-defined data source (DSN), see
 * gda_config_define_dsn() for more information about how to define a DSN. If you don't want to
 * define a DSN, it is possible to use gda_connection_new_from_string() instead of this method.
 *
 * The @auth_string can contain the authentication information for the server
 * to accept the connection. It is a string containing semi-colon seperated named value, usually
 * like "USERNAME=...;PASSWORD=..." where the ... are replaced by actual values. Note that each
 * name and value must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * If @auth_string is given, it wil be used, otherwise auth_string of #GdaDsnInfo will be used.
 *
 * The actual named parameters required depend on the provider being used, and that list is available
 * as the <parameter>auth_params</parameter> member of the #GdaProviderInfo structure for each installed
 * provider (use gda_config_get_provider_info() to get it). Also one can use the "gda-sql-6.0 -L" command
 * to list the possible named parameters.
 *
 * This method may fail with a GDA_CONNECTION_ERROR domain error (see the #GdaConnectionError error codes)
 * or a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * The returned connection is not yet opened, you need to call gda_connection_open() or gda_connection_open_async().
 *
 * Returns: (transfer full): a new #GdaConnection or %NULL if there was an error.
 *
 * Since: 6.0
 */
GdaConnection *
gda_connection_new_from_dsn (GdaDsnInfo *dsn,
                             const gchar *auth_string,
                             GdaConnectionOptions options,
                             GError **error)
{
  GdaConnection *cnc = NULL;

  g_return_val_if_fail (dsn, NULL);
  g_return_val_if_fail (dsn->cnc_string, NULL);

/* try to find provider */
  if (dsn->provider)
   {
      GdaProviderInfo *pinfo = NULL;
      GdaServerProvider *prov = NULL;

      pinfo = gda_config_get_provider_info (dsn->provider);
      if (pinfo)
        prov = gda_config_get_provider (dsn->provider, error);
      else
        g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
                     _("No provider '%s' installed"), dsn->provider);

      if (prov)
        cnc = _gda_server_provider_create_connection (prov, NULL, dsn->cnc_string,
                                                      auth_string ? auth_string : dsn->auth_string,
                                                      options);
    }
  else
    g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR,
                 "%s", _("Datasource configuration error: no provider specified"));

  return cnc;
}

/**
 * gda_connection_open_from_dsn_name:
 * @dsn_name: data source name.
 * @auth_string: (nullable): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function creates a connection and opens it, using a DSN name. If opening fails, then no connection is created. The named DSN should be available.
 * See gda_connection_new_from_dsn_name() for more information.
 *
 * Returns: (transfer full): a new #GdaConnection if connection opening was successful or %NULL if there was an error.
 */
GdaConnection *
gda_connection_open_from_dsn_name (const gchar *dsn_name,
                                   const gchar *auth_string,
                                   GdaConnectionOptions options,
                                   GError **error)
{
  GdaConnection *cnc = NULL;

  g_return_val_if_fail (dsn_name && *dsn_name, NULL);

  cnc = gda_connection_new_from_dsn_name (dsn_name, auth_string, options, error);

  if (cnc && !gda_connection_open (cnc, error))
    g_clear_object (&cnc);

  return cnc;
}

/**
 * gda_connection_open_from_dsn:
 * @dsn: data sourcename.
 * @auth_string: (nullable): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function creates a connection and opens it, using a DSN. If opening fails, then no connection is created.
 * See gda_connection_new_from_dsn() for more information.
 *
 * Returns: (transfer full): a new #GdaConnection if connection opening was successful or %NULL if there was an error.
 * Since: 6.0
 */
GdaConnection *
gda_connection_open_from_dsn (GdaDsnInfo *dsn,
                              const gchar *auth_string,
                              GdaConnectionOptions options,
                              GError **error)
{
  GdaConnection *cnc = NULL;

  g_return_val_if_fail (dsn, NULL);
  g_return_val_if_fail (dsn->cnc_string, NULL);

  cnc = gda_connection_new_from_dsn (dsn, auth_string, options, error);

  if (cnc && !gda_connection_open (cnc, error))
    g_clear_object (&cnc);

  return cnc;
}

/**
 * gda_connection_new_from_string:
 * @provider_name: (nullable): provider ID to connect to, or %NULL
 * @cnc_string: connection string.
 * @auth_string: (nullable): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * Opens a connection given a provider ID and a connection string. This
 * allows applications to open connections without having to create
 * a data source (DSN) in the configuration. The format of @cnc_string is
 * similar to PostgreSQL and MySQL connection strings. It is a semicolumn-separated
 * series of &lt;key&gt;=&lt;value&gt; pairs, where each key and value are encoded as per RFC 1738, 
 * see gda_rfc1738_encode() for more information.
 *
 * The possible keys depend on the provider, the "gda-sql-6.0 -L" command
 * can be used to list the actual keys for each installed database provider.
 *
 * For example the connection string to open an SQLite connection to a database
 * file named "my_data.db" in the current directory would be <constant>"DB_DIR=.;DB_NAME=my_data"</constant>.
 *
 * The @cnc_string string must have the following format: 
 * "[&lt;provider&gt;://][&lt;username&gt;[:&lt;password&gt;]@]&lt;connection_params&gt;"
 * (if &lt;username&gt; and/or &lt;password&gt; are provided, and @auth_string is %NULL, then these username
 * and passwords will be used, and if &lt;provider&gt; is provided and @provider_name is %NULL then this
 * provider will be used). Note that if provided, &lt;username&gt;, &lt;password&gt; and  &lt;provider&gt;
 * must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * The @auth_string must contain the authentication information for the server
 * to accept the connection. It is a string containing semi-colon seperated named values, usually 
 * like "USERNAME=...;PASSWORD=..." where the ... are replaced by actual values. Note that each
 * name and value must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * The actual named parameters required depend on the provider being used, and that list is available
 * as the <parameter>auth_params</parameter> member of the #GdaProviderInfo structure for each installed
 * provider (use gda_config_get_provider_info() to get it). Similarly to the format of the connection
 * string, use the "gda-sql-6.0 -L" command to list the possible named parameters.
 *
 * Additionally, it is possible to have the connection string
 * respect the "&lt;provider_name&gt;://&lt;real cnc string&gt;" format, in which case the provider name
 * and the real connection string will be extracted from that string (note that if @provider_name
 * is not %NULL then it will still be used as the provider ID).\
 *
 * This method may fail with a GDA_CONNECTION_ERROR domain error (see the #GdaConnectionError error codes) 
 * or a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * The returned connection is not yet opened, you need to call gda_connection_open() or gda_connection_open_async().
 *
 * Returns: (transfer full): a new #GdaConnection or %NULL if there was an error.
 *
 * Since: 5.0.2
 */
GdaConnection *
gda_connection_new_from_string (const gchar *provider_name, const gchar *cnc_string, const gchar *auth_string,
				 GdaConnectionOptions options, GError **error)
{
	GdaConnection *cnc = NULL;
	gchar *user, *pass, *real_cnc, *real_provider;
	gchar *real_auth_string = NULL;

	g_return_val_if_fail (cnc_string && *cnc_string, NULL);

	gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
	if (!real_cnc) {
		g_free (user);
		g_free (pass);
		g_free (real_provider);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Malformed connection string '%s'"), cnc_string);
		return NULL;
	}

	if (!provider_name && !real_provider) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR, 
			      "%s", _("No database driver specified"));
		g_free (user);
		g_free (pass);
		g_free (real_cnc);
		return NULL;
	}

	if (!auth_string && user) {
		gchar *s1;
		s1 = gda_rfc1738_encode (user);
		if (pass) {
			gchar *s2;
			s2 = gda_rfc1738_encode (pass);
			real_auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", s1, s2);
			g_free (s2);
		}
		else
			real_auth_string = g_strdup_printf ("USERNAME=%s", s1);
		g_free (s1);
	}

	/* try to find provider */
	if (provider_name || real_provider) {
		GdaProviderInfo *pinfo;
		GdaServerProvider *prov = NULL;

		pinfo = gda_config_get_provider_info (provider_name ? provider_name : real_provider);
		if (pinfo)
			prov = gda_config_get_provider (provider_name ? provider_name : real_provider, error);
		else
			g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
				     _("No provider '%s' installed"), provider_name ? provider_name : real_provider);

		if (prov)
			cnc = _gda_server_provider_create_connection (prov, NULL, real_cnc,
								      auth_string ? auth_string : real_auth_string, options);
	}

	g_free (real_auth_string);
	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);

	return cnc;
}

/**
 * gda_connection_open_from_string:
 * @provider_name: (nullable): provider ID to connect to, or %NULL
 * @cnc_string: connection string.
 * @auth_string: (nullable): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function creates a connection and opens it, using a connection string. If opening fails, then no connection is created.
 * See gda_connection_new_from_string() for more information.
 *
 * Returns: (transfer full): a new #GdaConnection if connection opening was successful or %NULL if there was an error.
 */
GdaConnection *
gda_connection_open_from_string (const gchar *provider_name, const gchar *cnc_string, const gchar *auth_string,
				 GdaConnectionOptions options, GError **error)
{
	GdaConnection *cnc;
	cnc = gda_connection_new_from_string (provider_name, cnc_string, auth_string, options, error);
	if (cnc && !gda_connection_open (cnc, error)) {
		g_object_unref (cnc);
		cnc = NULL;
	}
	return cnc;
}


/*
 * Uses _gda_config_sqlite_provider to open a connection
 */
GdaConnection *
_gda_open_internal_sqlite_connection (const gchar *cnc_string)
{
	GdaConnection *cnc = NULL;
	GdaServerProvider *prov = _gda_config_sqlite_provider;
	gchar *user, *pass, *real_cnc, *real_provider;

	/*g_print ("%s(%s)\n", __FUNCTION__, cnc_string);*/
	gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
	g_free (user);
	g_free (pass);
	g_free (real_provider);

        if (!real_cnc)
                return NULL;

	cnc = _gda_server_provider_create_connection (prov, NULL, real_cnc, NULL, GDA_CONNECTION_OPTIONS_NONE);
	g_free (real_cnc);

	/* open the connection */
	if (!gda_connection_open (cnc, NULL)) {
		g_object_unref (cnc);
		cnc = NULL;
	}
	return cnc;
}

static void
sqlite_connection_closed_cb (GdaConnection *cnc, G_GNUC_UNUSED gpointer data)
{
	gchar *filename;
	filename = g_object_get_data (G_OBJECT (cnc), "__gda_fname");
	g_assert (filename && *filename);
	g_unlink (filename);
}

/**
 * gda_connection_open_sqlite:
 * @directory: (nullable): the directory the database file will be in, or %NULL for the default TMP directory
 * @filename: the database file name
 * @auto_unlink: if %TRUE, then the database file will be removed afterwards
 *
 * Opens an SQLite connection even if the SQLite provider is not installed,
 * to be used by database providers which need a temporary database to store
 * some information.
 *
 * Returns: (transfer full): a new #GdaConnection, or %NULL if an error occurred
 */
GdaConnection *
gda_connection_open_sqlite (const gchar *directory, const gchar *filename, gboolean auto_unlink)
{
	GdaConnection *cnc;
	gchar *fname;
	gint fd;

	if (!directory)
		directory = g_get_tmp_dir(); /* Flawfinder: ignore */
	else
		g_return_val_if_fail (*directory, NULL);
	g_return_val_if_fail (filename && *filename, NULL);

	fname = g_build_filename (directory, filename, NULL);
#ifdef G_OS_WIN32
	fd = g_open (fname, O_WRONLY | O_CREAT | O_TRUNC,
		     S_IRUSR | S_IWUSR);
#else
	fd = g_open (fname, O_WRONLY | O_CREAT | O_NOCTTY | O_TRUNC,
		     S_IRUSR | S_IWUSR);
#endif
	if (fd == -1) {
		g_free (fname);
		return NULL;
	}
	close (fd);

	gchar *tmp1, *tmp2, *cncstring;
	tmp1 = gda_rfc1738_encode (directory);
	tmp2 = gda_rfc1738_encode (filename);
	cncstring = g_strdup_printf ("SQLite://DB_DIR=%s;DB_NAME=%s", tmp1, tmp2);
	g_free (tmp1);
	g_free (tmp2);
	
	cnc = _gda_open_internal_sqlite_connection (cncstring);
	g_free (cncstring);

	if (auto_unlink) {
		g_object_set_data_full (G_OBJECT (cnc), "__gda_fname", fname, g_free);
		g_signal_connect (cnc, "closed",
				  G_CALLBACK (sqlite_connection_closed_cb), NULL);
	}
	else
		g_free (fname);
	return cnc;
}
/*
 * Upon return, @out_params and @out_auth point to new #GdaQuarkList
 *
 * Returns: %TRUE if no error occurred
 */
static gboolean
compute_params_and_auth_quarks (GdaConnection *cnc, GdaQuarkList **out_params, GdaQuarkList **out_auth, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	GdaDsnInfo *dsn_info = NULL;
	GdaQuarkList *params, *auth;

	*out_params = NULL;
	*out_auth = NULL;

	gda_connection_lock ((GdaLockable*) cnc);

	/* provider test */
	if (!priv->provider_obj) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
			      "%s", _("No provider specified"));
		gda_connection_unlock ((GdaLockable*) cnc);
		return FALSE;
	}

	/* connection string */
	if (priv->dsn) {
		/* get the data source info */
		dsn_info = gda_config_get_dsn_info (priv->dsn);
		if (!dsn_info) {
			gda_log_error (_("Data source %s not found in configuration"), priv->dsn);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
				     _("Data source %s not found in configuration"), priv->dsn);
			gda_connection_unlock ((GdaLockable*) cnc);
			return FALSE;
		}

		g_free (priv->cnc_string);
		priv->cnc_string = g_strdup (dsn_info->cnc_string);
	}
	else {
		if (!priv->cnc_string) {
			gda_log_error (_("No DSN or connection string specified"));
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_CNC_SPEC_ERROR,
				      "%s", _("No DSN or connection string specified"));
			gda_connection_unlock ((GdaLockable*) cnc);
			return FALSE;
		}
		/* try to see if connection string has the <provider>://<rest of the string> format */
	}
	params = gda_quark_list_new_from_string (priv->cnc_string);

	/* authentication string */
	char *real_auth_string;
	if (priv->auth_string)
		real_auth_string = g_strdup (priv->auth_string);
	else {
		if (dsn_info && dsn_info->auth_string)
			real_auth_string = g_strdup (dsn_info->auth_string);
		else 
			/* look for authentication parameters in cnc string */
			real_auth_string = g_strdup (priv->cnc_string);
	}
	auth = gda_quark_list_new_from_string (real_auth_string);
	g_free (real_auth_string);

	gda_connection_unlock ((GdaLockable*) cnc);

	*out_params = params;
	*out_auth = auth;

	return TRUE;
}

/**
 * gda_connection_open:
 * @cnc: a #GdaConnection object
 * @error: a place to store errors, or %NULL
 *
 * Tries to open the connection. The function either blocks or, if a #GMaincontext has been specified using
 * gda_connection_set_main_context(), processes the events for that main context until either the
 * connection opening has succeeded or failed.
 *
 * If the connection is already opened, then this function returns %TRUE immediately.
 *
 * Returns: TRUE if the connection is opened, and FALSE otherwise.
 */
gboolean
gda_connection_open (GdaConnection *cnc, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	/* don't do anything if connection is already opened */
	if (gda_connection_is_opened (cnc))
		return TRUE;

	GdaQuarkList *params, *auth;
	if (!compute_params_and_auth_quarks (cnc, &params, &auth, error))
		return FALSE;

	/* try to open the connection, with the "active waiting" at this point */
	gboolean opened;
	opened = _gda_server_provider_open_connection_sync (priv->provider_obj, cnc, params, auth, error);

	if (opened && !priv->provider_data && !GDA_IS_VIRTUAL_PROVIDER (priv->provider_obj)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_ERROR, "Internal error: connection has been reported as opened, yet no provider's data has been setted");
		opened = FALSE;
	}

	return opened;
}

/**
 * gda_connection_open_async:
 * @cnc: a #GdaConnection object
 * @callback: (scope notified): a #GdaConnectionOpenFunc which will be called after the connection has been opened (of failed to open)
 * @data: (nullable): data to pass to @callback when called
 * @error: a place to store errors, or %NULL
 *
 * This function requests that the connection be opened.
 *
 * If the connection is already opened, then this function returns an error (with the %GDA_CONNECTION_ALREADY_OPENED_ERROR code).
 *
 * Note: @callback function will be called when processing events from the #GMainContext defined by
 * gda_connection_set_main_context(), for example when there is a main loop for that main context.
 *
 * Returns: a job ID 
 *
 * Since: 6.0
 */
guint
gda_connection_open_async (GdaConnection *cnc, GdaConnectionOpenFunc callback, gpointer data, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), 0);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	/* return an error if connection is already opened */
	if (gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_ALREADY_OPENED_ERROR,
			     "%s", _("Connection is already opened"));
		return 0;
	}

	GdaQuarkList *params, *auth;
	if (!compute_params_and_auth_quarks (cnc, &params, &auth, error))
		return FALSE;

	/* try to open the connection */
	gboolean submitted;
	guint job_id;
	submitted = _gda_server_provider_open_connection_async (priv->provider_obj, cnc, params, auth,
								callback, data, &job_id, error);
	return submitted ? job_id : 0;
}

static void
add_connection_event_from_error (GdaConnection *cnc, GError **error)
{
	GdaConnectionEvent *event;
	gchar *str;
	event = GDA_CONNECTION_EVENT (g_object_new (GDA_TYPE_CONNECTION_EVENT,
							  "type", (int)GDA_CONNECTION_EVENT_WARNING, NULL));
	str = g_strdup_printf (_("Error while maintaining the meta data up to date: %s"),
			       error && *error && (*error)->message ? (*error)->message : _("No detail"));
	gda_connection_event_set_description (event, str);
	g_free (str);
	if (error)
		g_clear_error (error);
	gda_connection_add_event (cnc, event);
}

/**
 * gda_connection_close:
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source.
 */
gboolean
gda_connection_close (GdaConnection *cnc, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (! priv->provider_data)
		return TRUE;

	g_object_ref (cnc);
	gda_connection_lock ((GdaLockable*) cnc);

	if (! priv->provider_data) {
		/* connection already closed */
		gda_connection_unlock ((GdaLockable*) cnc);
		g_object_unref (cnc);
		return TRUE;
	}

	if (priv->meta_store &&
	    priv->trans_meta_context &&
	    gda_connection_get_transaction_status (cnc)) {
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (priv->meta_store);
		if (cnc != mscnc) {
			gsize i;
			for (i = 0; i < priv->trans_meta_context->len; i++) {
				GdaMetaContext *context;
				GError *lerror = NULL;
				context = g_array_index (priv->trans_meta_context, GdaMetaContext*, i);
				if (! gda_connection_update_meta_store (cnc, context, &lerror))
					add_connection_event_from_error (cnc, &lerror);
				auto_update_meta_context_free (context);
			}
			g_array_free (priv->trans_meta_context, TRUE);
			priv->trans_meta_context = NULL;
		}
	}

	/* get rid of prepared statements to avoid problems */
	if (priv->prepared_stmts) {
		g_hash_table_destroy (priv->prepared_stmts);
		priv->prepared_stmts = NULL;
	}

	/* really close connection */
	gboolean retval;
	retval = _gda_server_provider_close_connection (priv->provider_obj, cnc, error);

	gda_connection_unlock ((GdaLockable*) cnc);
	g_object_unref (cnc);

	return retval;
}

/*
 * _gda_connection_get_worker:
 *
 * Returns: (transfer none): the #GdaWorker internally used by @cnc, or %NULL if connection is not yet opened
 */
GdaWorker *
_gda_connection_get_worker (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (priv->provider_data)
		return priv->provider_data->worker;
	return NULL;
}

/*
 * _gda_connection_get_exec_slowdown:
 *
 * Returns: the number of milliseconds to wait when executing things, used for testing and debugging
 */
guint
_gda_connection_get_exec_slowdown (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), 0);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	return priv->exec_slowdown;
}

static void
assert_status_transaction (GdaConnectionStatus old, GdaConnectionStatus new)
{
	if (old == new)
		return;
	switch (old) {
	case GDA_CONNECTION_STATUS_CLOSED:
		g_assert (new == GDA_CONNECTION_STATUS_OPENING);
		break;
	case GDA_CONNECTION_STATUS_OPENING:
		g_assert ((new == GDA_CONNECTION_STATUS_IDLE) || (new == GDA_CONNECTION_STATUS_CLOSED));
		break;
	case GDA_CONNECTION_STATUS_IDLE:
		g_assert ((new == GDA_CONNECTION_STATUS_BUSY) || (new == GDA_CONNECTION_STATUS_CLOSED));
		break;
	case GDA_CONNECTION_STATUS_BUSY:
		g_assert ((new == GDA_CONNECTION_STATUS_IDLE) || (new == GDA_CONNECTION_STATUS_CLOSED));
		break;
	default:
		g_assert_not_reached ();
	}
}

/*
 * _gda_connection_declare_closed:
 * @cnc: a #GdaConnection
 */
void
_gda_connection_declare_closed (GdaConnection *cnc)
{
	g_return_if_fail (cnc != NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	g_return_if_fail (priv->status == GDA_CONNECTION_STATUS_IDLE);

	assert_status_transaction (priv->status, GDA_CONNECTION_STATUS_CLOSED);
	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[STATUS_CHANGED], 0, GDA_CONNECTION_STATUS_CLOSED);
}

/*
 * _gda_connection_set_status:
 * This function can't be used to switch to GDA_CONNECTION_STATUS_BUSY, one must switch to
 * GDA_CONNECTION_STATUS_IDLE and use gda_connection_increase/decrease_usage() functions.
 *
 * WARNING: @cnc _MUST_ be locked using gda_connection_lock() before this function is called
 */
void
_gda_connection_set_status (GdaConnection *cnc, GdaConnectionStatus status)
{
	g_return_if_fail (cnc != NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (!cnc || (priv->status == status))
		return;
	if ((status == GDA_CONNECTION_STATUS_CLOSED) ||
	    (status == GDA_CONNECTION_STATUS_OPENING))
		g_return_if_fail (priv->busy_count == 0);
	g_return_if_fail (status != GDA_CONNECTION_STATUS_BUSY);
	assert_status_transaction (priv->status, status);
	priv->status = status;
	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[STATUS_CHANGED], 0, status);
	/*g_print ("CNC %p status is %d\n", cnc, priv->status);*/
}

/**
 * gda_connection_increase_usage:
 * @cnc: a #GdaConnection
 *
 * Declare that @cnc is being used, which may emit the "status-changed" signal along the way. Any call to this function
 * must be followed by one single call to gda_connection_decrease_usage(). The connection's status must either be
 * IDLE, BUSY, or OPENING when this function is called. If the status is IDLE, then it will be switched to BUSY.
 *
 * Note: This function is reserved to database provider's implementation
 *
 * WARNING: @cnc _MUST_ be locked using gda_lockable_lock() before this function is called
 */
void
gda_connection_increase_usage (GdaConnection *cnc)
{
	g_return_if_fail (cnc != NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	g_return_if_fail ((priv->status == GDA_CONNECTION_STATUS_IDLE) ||
			  (priv->status == GDA_CONNECTION_STATUS_BUSY) ||
			  (priv->status == GDA_CONNECTION_STATUS_OPENING));

	priv->busy_count ++;
	if (priv->status == GDA_CONNECTION_STATUS_IDLE) {
		assert_status_transaction (priv->status, GDA_CONNECTION_STATUS_BUSY);
		priv->status = GDA_CONNECTION_STATUS_BUSY;
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[STATUS_CHANGED], 0, GDA_CONNECTION_STATUS_BUSY);
		/*g_print ("CNC %p status is %d\n", cnc, priv->status);*/
	}
}

/**
 * gda_connection_decrease_usage:
 * @cnc: a #GdaConnection
 *
 * Declare that @cnc is not being used, which may emit the "status-changed" signal along the way. Any call to this function
 * must be following a single call to gda_connection_increase_usage(). The connection's status must either be
 * BUSY or OPENING when this function is called. If it's BUSY, then it may be changed to IDLE after this call.
 *
 * Note: This function is reserved to database provider's implementation
 *
 * WARNING: @cnc _MUST_ be locked using gda_lockable_lock() before this function is called
 */
void
gda_connection_decrease_usage (GdaConnection *cnc)
{
	g_return_if_fail (cnc != NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	g_assert (priv->busy_count > 0);
	g_return_if_fail ((priv->status == GDA_CONNECTION_STATUS_BUSY) ||
			  (priv->status == GDA_CONNECTION_STATUS_OPENING));

	priv->busy_count --;
	if ((priv->busy_count == 0) && (priv->status == GDA_CONNECTION_STATUS_BUSY)) {
		assert_status_transaction (priv->status, GDA_CONNECTION_STATUS_IDLE);
		priv->status = GDA_CONNECTION_STATUS_IDLE;
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[STATUS_CHANGED], 0, GDA_CONNECTION_STATUS_IDLE);
		/*g_print ("CNC %p status is %d\n", cnc, priv->status);*/
	}
}

/**
 * gda_connection_get_status:
 * @cnc: a #GdaConnection
 *
 * Get the current status of @cnc. Note that this function needs to lock the connection (see #GdaLockable)
 * to obtain the result.
 *
 * Returns: the connection's status
 *
 * Since: 6.0
 */
GdaConnectionStatus
gda_connection_get_status (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), GDA_CONNECTION_STATUS_CLOSED);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	GdaConnectionStatus status;
	gda_connection_lock ((GdaLockable*) cnc);
	status = priv->status;
	gda_connection_unlock ((GdaLockable*) cnc);
	return status;
}

/**
 * gda_connection_is_opened:
 * @cnc: a #GdaConnection object.
 *
 * Checks whether a connection is open or not.
 *
 * Returns: %TRUE if the connection is open, %FALSE if it's not.
 */
gboolean
gda_connection_is_opened (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return priv->provider_data ? TRUE : FALSE;
}


/**
 * gda_connection_get_options:
 * @cnc: a #GdaConnection object.
 *
 * Gets the #GdaConnectionOptions used to open this connection.
 *
 * Returns: the connection options.
 */
GdaConnectionOptions
gda_connection_get_options (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), -1);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return priv->options;
}

/**
 * gda_connection_get_provider:
 * @cnc: a #GdaConnection object
 *
 * Gets a pointer to the #GdaServerProvider object used to access the database
 *
 * Returns: (transfer none): the #GdaServerProvider (NEVER NULL)
 */
GdaServerProvider *
gda_connection_get_provider (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return priv->provider_obj;
}

/**
 * gda_connection_get_provider_name:
 * @cnc: a #GdaConnection object
 *
 * Gets the name (identifier) of the database provider used by @cnc
 *
 * Returns: a non modifiable string
 */
const gchar *
gda_connection_get_provider_name (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (!priv->provider_obj)
		return NULL;

	return gda_server_provider_get_name (priv->provider_obj);
}

/**
 * gda_connection_get_dsn:
 * @cnc: a #GdaConnection object
 *
 * Returns: the data source name the connection object is connected
 * to.
 */
const gchar *
gda_connection_get_dsn (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return (const gchar *) priv->dsn;
}

/**
 * gda_connection_get_cnc_string:
 * @cnc: a #GdaConnection object.
 *
 * Gets the connection string used to open this connection.
 *
 * The connection string is the string sent over to the underlying
 * database provider, which describes the parameters to be used
 * to open a connection on the underlying data source.
 *
 * Returns: the connection string used when opening the connection.
 */
const gchar *
gda_connection_get_cnc_string (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return (const gchar *) priv->cnc_string;
}

/**
 * gda_connection_get_authentication:
 * @cnc: a #GdaConnection object.
 *
 * Gets the user name used to open this connection.
 *
 * Returns: the user name.
 */
const gchar *
gda_connection_get_authentication (GdaConnection *cnc)
{
	const gchar *str;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	str = (const gchar *) priv->auth_string;
	if (!str) 
		str = "";
	return str;
}

/**
 * gda_connection_get_date_format:
 * @cnc: a #GdaConnection object
 * @out_first: (out) (nullable): the place to store the first part of the date, or %NULL
 * @out_second: (out) (nullable): the place to store the second part of the date, or %NULL
 * @out_third: (out) (nullable): the place to store the third part of the date, or %NULL
 * @out_sep: (out) (nullable): the place to store the separator (used between year, month and day parts) part of the date, or %NULL
 * @error: (nullable): a place to store errors, or %NULL
 *
 * This function allows you to determine the actual format for the date values.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.2
 */
gboolean
gda_connection_get_date_format (GdaConnection *cnc, GDateDMY *out_first,
				GDateDMY *out_second, GDateDMY *out_third, gchar *out_sep,
				GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	GdaDataHandler *dh;
	dh = gda_server_provider_get_data_handler_g_type (priv->provider_obj, cnc, G_TYPE_DATE);
	if (!dh) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Provider does not provide a GdaDataHandler for dates"));
		return FALSE;
	}

	GDate *tdate;
	tdate = g_date_new_dmy (15, 12, 2003);
	g_assert (tdate && g_date_valid (tdate));

	GValue *value;
	value = gda_value_new (G_TYPE_DATE);
	g_value_set_boxed (value, tdate);
	g_date_free (tdate);

	gchar *str;
	str = gda_data_handler_get_str_from_value (dh, value);
	gda_value_free (value);

	/* parsing */
	guint nb;
	gchar *ptr;
	GDateDMY order[3];
	gchar sep;

	/* 1st part */
	for (nb = 0, ptr = str; *ptr; ptr++) {
		if ((*ptr <= '9') && (*ptr >= '0'))
			nb = nb * 10 + (*ptr - '0');
		else
			break;
	}
	if (nb == 2003)
		order[0] = G_DATE_YEAR;
	else if (nb == 12)
		order[0] = G_DATE_MONTH;
	else if (nb == 15)
		order[0] = G_DATE_DAY;
	else {
		g_free (str);
		return FALSE;
	}

	/* separator */
	sep = *ptr;
	if (!sep) {
		g_free (str);
		return FALSE;
	}

	/* 2nd part */
	for (nb = 0, ptr++; *ptr; ptr++) {
		if ((*ptr <= '9') && (*ptr >= '0'))
			nb = nb * 10 + (*ptr - '0');
		else
			break;
	}
	if (nb == 2003)
		order[1] = G_DATE_YEAR;
	else if (nb == 12)
		order[1] = G_DATE_MONTH;
	else if (nb == 15)
		order[1] = G_DATE_DAY;
	else {
		g_free (str);
		return FALSE;
	}

	if (sep != *ptr) {
		g_free (str);
		return FALSE;
	}

	/* 3rd part */
	for (nb = 0, ptr++; *ptr; ptr++) {
		if ((*ptr <= '9') && (*ptr >= '0'))
			nb = nb * 10 + (*ptr - '0');
		else
			break;
	}
	if (nb == 2003)
		order[2] = G_DATE_YEAR;
	else if (nb == 12)
		order[2] = G_DATE_MONTH;
	else if (nb == 15)
		order[2] = G_DATE_DAY;
	else {
		g_free (str);
		return FALSE;
	}
	g_free (str);

	/* result */
	if (out_first)
		*out_first = order [0];
	if (out_second)
		*out_second = order [1];
	if (out_third)
		*out_third = order [2];
	if (out_sep)
		*out_sep = sep;

	return TRUE;
}

/**
 * gda_connection_insert_row_into_table:
 * @cnc: an opened connection
 * @table: table's name to insert into
 * @error: a place to store errors, or %NULL
 * @...: a list of string/GValue pairs with the name of the column to use and the
 * GValue pointer containing the value to insert for the column (value can be %NULL), finished by a %NULL. There must be
 * at least one column name and value
 *
 * This is a convenience function, which creates an INSERT statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: INSERT INTO &lt;table&gt; (&lt;column_name&gt; [,...]) VALUES (&lt;column_name&gt; = &lt;new_value&gt; [,...]).
 *
 * A simple example to add a row in database
 * 
 * |[<!-- language="C" -->
 * 
 * GdaConnection *cnc;
 * // Open connection here
 * 
 * GError        *error = NULL;
 * 
 * GValue *v_first_name = gda_value_new_from_string (first_name, G_TYPE_STRING);
 * GValue *v_last_name  = gda_value_new_from_string (last_name, G_TYPE_STRING);
 * GValue *v_email      = gda_value_new_from_string (email, G_TYPE_STRING);
 * GValue *v_cod        = gda_value_new (G_TYPE_INT);
 * 
 * g_value_set_int (v_cod, cod);
 * 
 * if (!gda_connection_insert_row_into_table (cnc, "TABLE_CONTACTS", &error,
 *                                            "col_first_name", v_first_name,
 *                                            "col_last_name", v_last_name,
 *                                            "col_email", v_email,
 *                                            "col_cod", v_cod, NULL))
 * {
 *   g_error ("It was not possible to add a new row in the table: %s\n",
 *            error && error->message ? error->message : "No detail");
 * }
 * 
 * gda_value_free (v_first_name);
 * gda_value_free (v_last_name);
 * gda_value_free (v_email);
 * gda_value_free (v_cod);
 * 
 * g_error_free (error);
 * 
 * 
 * ]|
 * 
 * Returns: TRUE if no error occurred
 *
 * Since: 4.2.3
 */
G_GNUC_NULL_TERMINATED
gboolean
gda_connection_insert_row_into_table (GdaConnection *cnc, const gchar *table, GError **error, ...)
{
	GSList *clist = NULL;
	GSList *vlist = NULL;
	gboolean retval;
	va_list  args;
	gchar *col_name;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	va_start (args, error);
	while ((col_name = va_arg (args, gchar*))) {
		clist = g_slist_prepend (clist, col_name);
		GValue *value;
		value = va_arg (args, GValue *);
		vlist = g_slist_prepend (vlist, value);
	}

	va_end (args);

	if (!clist) {
		g_warning ("No specified column or value");
		return FALSE;
	}

	clist = g_slist_reverse (clist);
	vlist = g_slist_reverse (vlist);
	retval = gda_connection_insert_row_into_table_v (cnc, table, clist, vlist, error);
	g_slist_free (clist);
	g_slist_free (vlist);

	return retval;
}

/**
 * gda_connection_insert_row_into_table_v:
 * @cnc: an opened connection
 * @table: table's name to insert into
 * @col_names: (element-type utf8): a list of column names (as const gchar *)
 * @values: (element-type GValue): a list of values (as #GValue)
 * @error: a place to store errors, or %NULL
 *
 * @col_names and @values must have length (&gt;= 1).
 *
 * This is a convenience function, which creates an INSERT statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: INSERT INTO &lt;table&gt; (&lt;column_name&gt; [,...]) VALUES (&lt;column_name&gt; = &lt;new_value&gt; [,...]).
 *
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Since: 4.2.3
 */
gboolean
gda_connection_insert_row_into_table_v (GdaConnection *cnc, const gchar *table,
					GSList *col_names, GSList *values,
					GError **error)
{
	gboolean retval;
	GSList *fields = NULL;
	GSList *expr_values = NULL;
	GdaSqlStatement *sql_stm;
	GdaSqlStatementInsert *ssi;
	GdaStatement *insert;
	gint i;

	GSList *holders = NULL;
	GSList *l1, *l2;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	g_return_val_if_fail (col_names, FALSE);
	g_return_val_if_fail (g_slist_length (col_names) == g_slist_length (values), FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	/* Construct insert query and list of GdaHolders */
	sql_stm = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
	ssi = (GdaSqlStatementInsert*) sql_stm->contents;
	g_assert (GDA_SQL_ANY_PART (ssi)->type == GDA_SQL_ANY_STMT_INSERT);

	ssi->table = gda_sql_table_new (GDA_SQL_ANY_PART (ssi));
	ssi->table->table_name = gda_sql_identifier_quote (table, cnc, NULL, FALSE, FALSE);

	i = 0;
	for (l1 = col_names, l2 = values;
	     l1;
	     l1 = l1->next, l2 = l2->next) {
		GdaSqlField *field;
		GdaSqlExpr *expr;
		GValue *value = (GValue *) l2->data;
		const gchar *col_name = (const gchar*) l1->data;

		/* field */
		field = gda_sql_field_new (GDA_SQL_ANY_PART (ssi));
		field->field_name = gda_sql_identifier_quote (col_name, cnc, NULL, FALSE, FALSE);
		fields = g_slist_prepend (fields, field);

		/* value */
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ssi));
		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* create a GdaSqlExpr with a parameter */
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup_printf ("+%d", i);
			param->g_type = G_VALUE_TYPE (value);
			param->is_param = TRUE;
			expr->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* create a NULL GdaSqlExpr => nothing to do */
		}
		expr_values = g_slist_prepend (expr_values, expr);

		i++;
	}

	ssi->fields_list = g_slist_reverse (fields);
	ssi->values_list = g_slist_prepend (NULL, g_slist_reverse (expr_values));

	insert = gda_statement_new ();
	g_object_set (G_OBJECT (insert), "structure", sql_stm, NULL);
	gda_sql_statement_free (sql_stm);

	/* execute statement */
	GdaSet *set = NULL;
	if (holders) {
		set = gda_set_new (holders);
		g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, insert, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (insert);

	return retval;
}

/**
 * gda_connection_update_row_in_table:
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @error: a place to store errors, or %NULL
 * @...: a list of string/GValue pairs with the name of the column to use and the
 * GValue pointer containing the value to update the column to (value can be %NULL), finished by a %NULL. There must be
 * at least one column name and value
 *
 * This is a convenience function, which creates an UPDATE statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: UPDATE &lt;table&gt; SET &lt;column_name&gt; = &lt;new_value&gt; [,...] WHERE &lt;condition_column_name&gt; = &lt;condition_value&gt;.
 *
 * A simple example for updating a specific row in the table
 * 
 * |[<!-- language="C" -->
 * 
 * GdaConnection *cnc;
 * //Open connection here
 * 
 * GError        *error = NULL;
 * 
 * GValue *v_id         = gda_value_new (G_TYPE_INT);
 * GValue *v_first_name = gda_value_new_from_string (first_name, G_TYPE_STRING);
 * GValue *v_last_name  = gda_value_new_from_string (last_name, G_TYPE_STRING);
 * GValue *v_email      = gda_value_new_from_string (email, G_TYPE_STRING);
 * GValue *v_cod        = gda_value_new (G_TYPE_INT);
 * 
 * g_value_set_int (v_id, id);
 * g_value_set_int (v_cod, cod);
 * 
 * if (!gda_connection_update_row_in_table (cnc, "TABLE_CONTACTS",
 *                                          "col_id", v_id, &error,
 *                                          "col_first_name", v_first_name,
 *                                          "col_last_name", v_last_name,
 *                                          "col_email", v_email,
 *                                          "col_cod", v_cod, NULL))
 * {
 *   g_error ("Could not update row in table: %s\n",
 *            error && error->message ? error->message : "No detail");
 * }
 * 
 * gda_value_free (v_id);
 * gda_value_free (v_first_name);
 * gda_value_free (v_last_name);
 * gda_value_free (v_email);
 * gda_value_free (v_cod);
 * 
 * g_error_free (error);
 * 
 * 
 * ]|
 * 
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Since: 4.2.3
 */
G_GNUC_NULL_TERMINATED
gboolean
gda_connection_update_row_in_table (GdaConnection *cnc, const gchar *table,
				    const gchar *condition_column_name,
				    GValue *condition_value, GError **error, ...)
{
	GSList *clist = NULL;
	GSList *vlist = NULL;
	gboolean retval;
	va_list  args;
	gchar *col_name;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	va_start (args, error);
	while ((col_name = va_arg (args, gchar*))) {
		clist = g_slist_prepend (clist, col_name);
		GValue *value;
		value = va_arg (args, GValue *);
		vlist = g_slist_prepend (vlist, value);
	}

	va_end (args);

	if (!clist) {
		g_warning ("No specified column or value");
		return FALSE;
	}

	clist = g_slist_reverse (clist);
	vlist = g_slist_reverse (vlist);
	retval = gda_connection_update_row_in_table_v (cnc, table, condition_column_name, condition_value, clist, vlist, error);
	g_slist_free (clist);
	g_slist_free (vlist);

	return retval;
}

/**
 * gda_connection_update_row_in_table_v:
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @col_names: (element-type utf8): a list of column names (as const gchar *)
 * @values: (element-type GValue): a list of values (as #GValue)
 * @error: a place to store errors, or %NULL
 *
 * @col_names and @values must have length (&gt;= 1).
 *
 * This is a convenience function, which creates an UPDATE statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: UPDATE &lt;table&gt; SET &lt;column_name&gt; = &lt;new_value&gt; [,...] WHERE &lt;condition_column_name&gt; = &lt;condition_value&gt;.
 *
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Since: 4.2.3
 */
gboolean
gda_connection_update_row_in_table_v (GdaConnection *cnc, const gchar *table,
				      const gchar *condition_column_name,
				      GValue *condition_value,
				      GSList *col_names, GSList *values,
				      GError **error)
{
	gboolean retval;
	GSList *fields = NULL;
	GSList *expr_values = NULL;
	GdaSqlStatement *sql_stm;
	GdaSqlStatementUpdate *ssu;
	GdaStatement *update;
	gint i;

	GSList *holders = NULL;
	GSList *l1, *l2;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	g_return_val_if_fail (col_names, FALSE);
	g_return_val_if_fail (g_slist_length (col_names) == g_slist_length (values), FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	/* Construct update query and list of GdaHolders */
	sql_stm = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
	ssu = (GdaSqlStatementUpdate*) sql_stm->contents;
	g_assert (GDA_SQL_ANY_PART (ssu)->type == GDA_SQL_ANY_STMT_UPDATE);

	ssu->table = gda_sql_table_new (GDA_SQL_ANY_PART (ssu));
	ssu->table->table_name = gda_sql_identifier_quote (table, cnc, NULL, FALSE, FALSE);

	if (condition_column_name) {
		GdaSqlExpr *where, *op;
		where = gda_sql_expr_new (GDA_SQL_ANY_PART (ssu));
		ssu->cond = where;

		where->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (where));
		where->cond->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_prepend (NULL, op);
		op->value = gda_value_new (G_TYPE_STRING);
		g_value_take_string (op->value, gda_sql_identifier_quote (condition_column_name, cnc, NULL,
									  FALSE, FALSE));

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_append (where->cond->operands, op);
		if (condition_value) {
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup ("cond");
			param->g_type = G_VALUE_TYPE (condition_value);
			param->is_param = TRUE;
			op->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (condition_value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, condition_value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* nothing to do: NULL */
		}
	}

	i = 0;
	for (l1 = col_names, l2 = values;
	     l1;
	     l1 = l1->next, l2 = l2->next) {
		GValue *value = (GValue *) l2->data;
		const gchar *col_name = (const gchar*) l1->data;
		GdaSqlField *field;
		GdaSqlExpr *expr;

		/* field */
		field = gda_sql_field_new (GDA_SQL_ANY_PART (ssu));
		field->field_name = gda_sql_identifier_quote (col_name, cnc, NULL, FALSE, FALSE);
		fields = g_slist_prepend (fields, field);

		/* value */
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ssu));
		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* create a GdaSqlExpr with a parameter */
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup_printf ("+%d", i);
			param->g_type = G_VALUE_TYPE (value);
			param->is_param = TRUE;
			expr->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* create a NULL GdaSqlExpr => nothing to do */
		}
		expr_values = g_slist_prepend (expr_values, expr);

		i++;
	}

	ssu->fields_list = g_slist_reverse (fields);
	ssu->expr_list = g_slist_reverse (expr_values);

	update = gda_statement_new ();
	g_object_set (G_OBJECT (update), "structure", sql_stm, NULL);
	gda_sql_statement_free (sql_stm);

	/* execute statement */
	GdaSet *set = NULL;
	if (holders) {
		set = gda_set_new (holders);
		g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, update, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (update);

	return retval;
}

/**
 * gda_connection_delete_row_from_table:
 * @cnc: an opened connection
 * @table: the table's name with the row's values to be updated
 * @condition_column_name: the name of the column to used in the WHERE condition clause
 * @condition_value: the @condition_column_type's GType
 * @error: a place to store errors, or %NULL
 *
 * This is a convenience function, which creates a DELETE statement and executes it using the values
 * provided. It internally relies on variables which makes it immune to SQL injection problems.
 *
 * The equivalent SQL command is: DELETE FROM &lt;table&gt; WHERE &lt;condition_column_name&gt; = &lt;condition_value&gt;.
 *
 * A simple example to remove a row in database.
 *
 * |[<!-- language="C" -->
 *
 * GdaConnection *cnc;
 * //Open connection here
 *
 * GError *error = NULL;
 *
 * GValue *v_id = gda_value_new (G_TYPE_INT);
 * GValue *v_name = gda_value_new_from_string ("Aldibino Refinino", G_TYPE_STRING);
 *
 * //The number 10 represents a primary key record in the table
 * g_value_set_int (v_id, 10);
 *
 * //Delete a record with a specific ID in the col_id column
 * if (!gda_connection_delete_row_from_table (cnc, "TABLE_CONTACTS",
 *                                            "col_id", v_id,
 *                                            &error))
 * {
 *   g_error ("Could not delete row in table: %s\n",
 *            error && error->message ? error->message : "No detail");
 * }
 *
 * //Delete a record with a specific NAME in the col_name column
 * if (!gda_connection_delete_row_from_table (cnc, "TABLE_CONTACTS",
 *                                            "col_name", v_name,
 *                                            &error))
 * {
 *   g_error ("Could not delete row in table: %s\n",
 *            error && error->message ? error->message : "No detail");
 * }
 *
 * gda_value_free (v_id);
 * gda_value_free (v_name);
 *
 * g_error_free (error);
 *
 * ]|
 *
 * Returns: TRUE if no error occurred, FALSE otherwise
 *
 * Since: 4.2.3
 */
gboolean
gda_connection_delete_row_from_table (GdaConnection *cnc, const gchar *table,
				      const gchar *condition_column_name,
				      GValue *condition_value, GError **error)
{
	gboolean retval;
	GdaSqlStatement *sql_stm;
	GdaSqlStatementDelete *ssd;
	GdaStatement *delete;

	GSList *holders = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table && *table, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	/* Construct delete query and list of GdaHolders */
	sql_stm = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE);
	ssd = (GdaSqlStatementDelete*) sql_stm->contents;
	g_assert (GDA_SQL_ANY_PART (ssd)->type == GDA_SQL_ANY_STMT_DELETE);

	ssd->table = gda_sql_table_new (GDA_SQL_ANY_PART (ssd));
	ssd->table->table_name = gda_sql_identifier_quote (table, cnc, NULL, FALSE, FALSE);

	if (condition_column_name) {
		GdaSqlExpr *where, *op;
		where = gda_sql_expr_new (GDA_SQL_ANY_PART (ssd));
		ssd->cond = where;

		where->cond = gda_sql_operation_new (GDA_SQL_ANY_PART (where));
		where->cond->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_prepend (NULL, op);
		op->value = gda_value_new (G_TYPE_STRING);
		g_value_take_string (op->value, gda_sql_identifier_quote (condition_column_name, cnc, NULL,
									  FALSE, FALSE));

		op = gda_sql_expr_new (GDA_SQL_ANY_PART (where->cond));
		where->cond->operands = g_slist_append (where->cond->operands, op);
		if (condition_value) {
			GdaSqlParamSpec *param;
			param = g_new0 (GdaSqlParamSpec, 1);
			param->name = g_strdup ("cond");
			param->g_type = G_VALUE_TYPE (condition_value);
			param->is_param = TRUE;
			op->param_spec = param;

			GdaHolder *holder;
			holder = (GdaHolder*)  g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (condition_value),
							     "id", param->name, NULL);
			g_assert (gda_holder_set_value (holder, condition_value, NULL));
			holders = g_slist_prepend (holders, holder);
		}
		else {
			/* nothing to do: NULL */
		}
	}

	delete = gda_statement_new ();
	g_object_set (G_OBJECT (delete), "structure", sql_stm, NULL);
	gda_sql_statement_free (sql_stm);

	/* execute statement */
	GdaSet *set = NULL;
	if (holders) {
		set = gda_set_new (holders);
		g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, delete, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (delete);

	return retval;
}

/**
 * gda_connection_parse_sql_string:
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @sql: an SQL command to parse, not %NULL
 * @params: (out) (nullable) (transfer full): a place to store a new #GdaSet, for parameters used in SQL command, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * This function helps to parse a SQL string which uses parameters and store them at @params.
 *
 * Returns: (transfer full): a #GdaStatement representing the SQL command, or %NULL if an error occurred
 *
 * Since: 4.2.3
 */
GdaStatement*
gda_connection_parse_sql_string (GdaConnection *cnc, const gchar *sql, GdaSet **params, GError **error)
{
	GdaStatement *stmt;
	GdaSqlParser *parser = NULL;

	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sql, NULL);

	if (params)
		*params = NULL;
	if (cnc)
		parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, error);
	g_object_unref (parser);
	if (! stmt)
		return NULL;

	if (params && !gda_statement_get_parameters (stmt, params, error)) {
		g_object_unref (stmt);
		return NULL;
	}

	return stmt;
}

/**
 * gda_connection_point_available_event:
 * @cnc: a #GdaConnection object
 * @type: a #GdaConnectionEventType
 *
 * Use this method to get a pointer to the next available connection event which can then be customized
 * and taken into account using gda_connection_add_event().
 *
 * Returns: (transfer full): a pointer to the next available connection event, or %NULL if event should
 * be ignored
 *
 * Since: 4.2
 */
GdaConnectionEvent *
gda_connection_point_available_event (GdaConnection *cnc, GdaConnectionEventType type)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	/* ownership is transfered to the caller ! */

	GdaConnectionEvent *eev;
	eev = priv->events_array [priv->events_array_next];
	if (!eev)
		eev = GDA_CONNECTION_EVENT (g_object_new (GDA_TYPE_CONNECTION_EVENT,
							  "type", (int)type, NULL));
	else {
		gda_connection_event_set_event_type (eev, type);
		priv->events_array [priv->events_array_next] = NULL;
	}

	return eev;
}

#ifdef GDA_DEBUG_NO
static void
dump_events_array (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	gint i;
	g_print ("=== Array dump for %p ===\n", cnc);
	for (i = 0; i < priv->events_array_size; i++) {
		g_print ("   [%d] => %p\n", i, priv->events_array [i]);
	}

	const GList *list;
	for (list = gda_connection_get_events (cnc); list; list = list->next) {
		GdaConnectionEvent *ev = GDA_CONNECTION_EVENT (list->data);
		g_print ("    => %p\n", ev);
	}
}
#endif

/**
 * gda_connection_add_event:
 * @cnc: a #GdaConnection object.
 * @event: (transfer full): is stored internally, so you don't need to unref it.
 *
 * Adds an event to the given connection. This function is usually
 * called by providers, to inform clients of events that happened
 * during some operation.
 *
 * As soon as a provider (or a client, it does not matter) calls this
 * function with an @event object which is an error,
 * the connection object emits the "error" signal, to which clients can connect to be
 * informed of events.
 *
 * WARNING: the reference to the @event object is stolen by this function!
 */
void
gda_connection_add_event (GdaConnection *cnc, GdaConnectionEvent *event)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	g_object_ref (cnc);
	gda_connection_lock ((GdaLockable*) cnc);

	/* clear external list of events */
	if (priv->events_list) {
		g_list_free_full (priv->events_list, (GDestroyNotify) g_object_unref);
		priv->events_list = NULL;
	}

	/* add event, ownership is transfered to @cnc */
	GdaConnectionEvent *eev;
	eev = priv->events_array [priv->events_array_next];
	if (eev != event) {
		if (eev)
			g_object_unref (eev);
		priv->events_array [priv->events_array_next] = event;
	}

	/* handle indexes */
	priv->events_array_next ++;
	if (priv->events_array_next == priv->events_array_size) {
		priv->events_array_next = 0;
		priv->events_array_full = TRUE;
	}

	if (debug_level > 0) {
		const gchar *str = NULL;
		switch (gda_connection_event_get_event_type (event)) {
		case GDA_CONNECTION_EVENT_NOTICE:
			if (debug_level & 1) str = "NOTICE";
			break;
		case GDA_CONNECTION_EVENT_WARNING:
			if (debug_level & 2) str = "WARNING";
			break;
		case GDA_CONNECTION_EVENT_ERROR:
			if (debug_level & 4) str = "ERROR";
			break;
		case GDA_CONNECTION_EVENT_COMMAND:
			if (debug_level & 8) str = "COMMAND";
			break;
		default:
			break;
		}
		if (str)
			g_print ("EVENT> %s: %s (on cnx %p, %s)\n", str, 
				 gda_connection_event_get_description (event), cnc,
				 gda_connection_event_get_sqlstate (event));
	}

	if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR)
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[SIG_ERROR], 0, event);

#ifdef GDA_DEBUG_NO
	dump_events_array (cnc);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
	g_object_unref (cnc);
}

/**
 * gda_connection_add_event_string:
 * @cnc: a #GdaConnection object.
 * @str: a format string (see the printf(3) documentation).
 * @...: the arguments to insert in the error message.
 *
 * Adds a new error to the given connection object. This is just a convenience
 * function that simply creates a #GdaConnectionEvent and then calls
 * #gda_server_connection_add_error.
 *
 * Returns: (transfer none): a new #GdaConnectionEvent object, however the caller does not hold a reference to the returned object, and if need be the caller must call g_object_ref() on it.
 */
GdaConnectionEvent *
gda_connection_add_event_string (GdaConnection *cnc, const gchar *str, ...)
{
	GdaConnectionEvent *error;

	va_list args;
	gchar sz[2048];

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (str != NULL, NULL);

	/* build the message string */
	va_start (args, str);
	g_vsnprintf (sz, 2048, str, args);
	va_end (args);
	
	error = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_description (error, sz);
	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, gda_connection_get_provider_name (cnc));
	gda_connection_event_set_sqlstate (error, "-1");
	
	gda_connection_add_event (cnc, error);

	return error;
}

static void
_clear_connection_events (GdaConnection *locked_cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (locked_cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (locked_cnc);
	if (priv->auto_clear_events) {
		priv->events_array_full = FALSE;
		priv->events_array_next = 0;
	}
}

/**
 * gda_connection_clear_events_list:
 * @cnc: a #GdaConnection object.
 *
 * This function lets you clear the list of #GdaConnectionEvent's of the
 * given connection. 
 */
void
gda_connection_clear_events_list (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	gda_connection_lock ((GdaLockable*) cnc);
	_clear_connection_events (cnc);
	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_create_operation:
 * @cnc: a #GdaConnection object
 * @type: the type of operation requested
 * @options: (nullable): an optional list of parameters
 * @error: a place to store an error, or %NULL
 *
 * Creates a new #GdaServerOperation object which can be modified in order 
 * to perform the type type of action. It is a wrapper around the gda_server_provider_create_operation()
 * method.
 *
 * Returns: (transfer full): a new #GdaServerOperation object, or %NULL in the connection's provider does not support the @type type
 * of operation or if an error occurred
 */
GdaServerOperation*
gda_connection_create_operation (GdaConnection *cnc, GdaServerOperationType type, 
                                 GdaSet *options, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);

	return gda_server_provider_create_operation (priv->provider_obj, cnc, type, options, error);
}

/**
 * gda_connection_perform_operation:
 * @cnc: a #GdaConnection object
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Performs the operation described by @op (which should have been created using
 * gda_connection_create_operation()). It is a wrapper around the gda_server_provider_perform_operation()
 * method.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_perform_operation (GdaConnection *cnc, GdaServerOperation *op, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	priv->auto_clear_events = FALSE;
	retval = gda_server_provider_perform_operation (priv->provider_obj, cnc, op, error);
	priv->auto_clear_events = TRUE;
	return retval;
}

/**
 * gda_connection_create_parser:
 * @cnc: a #GdaConnection object
 *
 * Creates a new parser object able to parse the SQL dialect understood by @cnc. 
 * If the #GdaServerProvider object internally used by @cnc does not have its own parser, 
 * then %NULL is returned, and a general SQL parser can be obtained
 * using gda_sql_parser_new().
 *
 * Returns: (transfer full): a new #GdaSqlParser object, or %NULL
 */
GdaSqlParser *
gda_connection_create_parser (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);

	return gda_server_provider_create_parser (priv->provider_obj, cnc);
}

/*
 * Also resets the events list (as perceived when calling gda_connection_get_events()
 */
static void
change_events_array_max_size (GdaConnection *cnc, gint size)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	size ++; /* add 1 to compensate the "lost" slot when rotating the events array */
	if (size == priv->events_array_size)
		return;

	if (size > priv->events_array_size) {
		gint i;
		priv->events_array = g_renew (GdaConnectionEvent*, priv->events_array,
						   size);
		for (i = priv->events_array_size; i < size; i++)
			priv->events_array [i] = NULL;
	}
	else if (size >= EVENTS_ARRAY_SIZE) {
		gint i;
		for (i = size; i < priv->events_array_size; i++) {
			if (priv->events_array [i])
				g_object_unref (priv->events_array [i]);
		}
		priv->events_array = g_renew (GdaConnectionEvent*, priv->events_array,
						   size);
	}
	priv->events_array_size = size;
	priv->events_array_full = FALSE;
	priv->events_array_next = 0;
}

/**
 * gda_connection_batch_execute:
 * @cnc: a #GdaConnection object
 * @batch: a #GdaBatch object which contains all the statements to execute
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_batch_get_parameters()), or %NULL
 * @model_usage:  specifies how the returned data model(s) will be used, as a #GdaStatementModelUsage enum
 * @error: a place to store errors, or %NULL
 *
 * Executes all the statements contained in @batch (in the order in which they were added to @batch), and
 * returns a list of #GObject objects, at most one #GObject for each statement; see gda_connection_statement_execute()
 * for details about the returned objects.
 *
 * If one of the statement fails, then none of the subsequent statement will be executed, and the method returns
 * the list of #GObject created by the correct execution of the previous statements. If a transaction is required,
 * then it should be started before calling this method.
 *
 * Returns: (transfer full) (element-type GObject): a new list of #GObject objects
 */
GSList *
gda_connection_batch_execute (GdaConnection *cnc, GdaBatch *batch, GdaSet *params,
			      GdaStatementModelUsage model_usage, GError **error)
{
	GSList *retlist = NULL, *stmt_list;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_BATCH (batch), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	gda_connection_lock ((GdaLockable*) cnc);
	priv->auto_clear_events = FALSE;

	/* increase the size of priv->events_array to be able to store all the
	 * connection events */
	stmt_list = (GSList*) gda_batch_get_statements (batch);
	change_events_array_max_size (cnc, g_slist_length (stmt_list) * 2);

	for (; stmt_list; stmt_list = stmt_list->next) {
		GObject *obj;
		obj = gda_connection_statement_execute (cnc, GDA_STATEMENT (stmt_list->data), params,
							model_usage, NULL, error);
		if (!obj)
			break;
		retlist = g_slist_prepend (retlist, obj);
	}
	priv->auto_clear_events = TRUE;
	gda_connection_unlock ((GdaLockable*) cnc);
	
	return g_slist_reverse (retlist);
}


/**
 * gda_connection_quote_sql_identifier:
 * @cnc: a #GdaConnection object
 * @id: an SQL identifier
 *
 * Use this method to get a correctly quoted (if necessary) SQL identifier which can be used
 * in SQL statements, from @id. If @id is already correctly quoted for @cnc, then a copy of @id
 * may be returned.
 *
 * This method may add double quotes (or other characters) around @id:
 * <itemizedlist>
 *  <listitem><para>if @id is a reserved SQL keyword (such as SELECT, INSERT, ...)</para></listitem>
 *  <listitem><para>if @id contains non allowed characters such as spaces, or if it starts with a digit</para></listitem>
 *  <listitem><para>in any other event as necessary for @cnc, depending on the the options passed when opening the @cnc
 *            connection, and specifically the <link linkend="GDA-CONNECTION-OPTIONS-SQL-IDENTIFIERS-CASE-SENSITIVE:CAPS">
 *            GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE</link> option.</para></listitem>
 * </itemizedlist>
 *
 * One can safely pass an already quoted @id to this method, either with quoting characters allowed by @cnc or using the
 * double quote (") character.
 *
 * Returns: a new string, to free with g_free() once not needed anymore
 *
 * Since: 4.0.3
 */
gchar *
gda_connection_quote_sql_identifier (GdaConnection *cnc, const gchar *id)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (id, NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return gda_sql_identifier_quote (id, cnc, NULL, FALSE,
					 priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
}

/**
 * gda_connection_statement_to_sql:
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @stmt: a #GdaStatement object
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @flags: SQL rendering flags, as #GdaStatementSqlFlag OR'ed values
 * @params_used: (nullable) (element-type Gda.Holder) (out) (transfer container): a place to store the list of individual #GdaHolder objects within @params which have been used
 * @error: a place to store errors, or %NULL
 *
 * Renders @stmt as an SQL statement, adapted to the SQL dialect used by @cnc
 *
 * Returns: (transfer full): a new string, or %NULL if an error occurred
 */
gchar *
gda_connection_statement_to_sql (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				 GSList **params_used, GError **error)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	gchar *sql = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
		g_return_val_if_fail (priv->provider_obj, NULL);
		if (gda_connection_is_opened (cnc))
			sql = _gda_server_provider_statement_to_sql (priv->provider_obj, cnc, stmt, params, flags,
								     params_used, error);
		else
			sql = _gda_server_provider_statement_to_sql (priv->provider_obj, NULL, stmt, params, flags,
								     params_used, error);
	}
	return sql ? sql : gda_statement_to_sql_extended (stmt, cnc, params, flags, params_used, error);
}

/**
 * gda_connection_statement_prepare:
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @error: a place to store errors, or %NULL
 *
 * Ask the database accessed through the @cnc connection to prepare the usage of @stmt. This is only useful
 * if @stmt will be used more than once (however some database providers may always prepare statements 
 * before executing them).
 *
 * This function is also useful to make sure @stmt is fully understood by the database before actually executing it.
 *
 * Note however that it is also possible that gda_connection_statement_prepare() fails when
 * gda_connection_statement_execute() does not fail (this will usually be the case with statements such as
 * <![CDATA["SELECT * FROM ##tablename::string"]]> because database usually don't allow variables to be used in place of a 
 * table name).
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_connection_statement_prepare (GdaConnection *cnc, GdaStatement *stmt, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	return _gda_server_provider_statement_prepare (priv->provider_obj, cnc, stmt, error);
}

/*
 * @args is consumed as a succession of (column number, GType) arguments
 * which ends with a column number of -1. The returned array is always terminated by G_TYPE_NONE,
 * and contains 0 where no column type information has been provided
 */
static GType *
make_col_types_array (va_list args)
{
	GType *types;
	gint max = 10;
	gint col, lastidx = 0;

	col = va_arg (args, gint);
	if (col < 0)
		return NULL;

	types = g_new0 (GType, max + 1);
	types [max] = G_TYPE_NONE;
	for (; col >= 0; col = va_arg (args, gint)) {
		if (col >= max) {
			gint i;
			types = g_renew (GType, types, col + 5 + 1);
			for (i = max; i <= col + 5; i ++)
				types[i] = 0;
			max = col + 5;
			types [max] = G_TYPE_NONE;
		}
		types [col] = va_arg (args, GType);
		lastidx = col+1;
	}

	if (lastidx == 0) {
		g_free (types);
		types = NULL;
	}
	else
		types [lastidx] = G_TYPE_NONE;

	return types;
}

static void
add_exec_time_to_object (GObject *obj, GTimer *timer)
{
	gdouble etime;
	etime = g_timer_elapsed (timer, NULL);
	if (GDA_IS_DATA_SELECT (obj))
		g_object_set (obj, "execution-delay", etime, NULL);
	else if (GDA_IS_SET (obj)) {
		GdaHolder *holder;
		holder = gda_holder_new_inline (G_TYPE_DOUBLE, "EXEC_DELAY", etime);
		gda_set_add_holder ((GdaSet*) obj, holder);
		g_object_unref ((GObject*) holder);
	}
	else
		TO_IMPLEMENT;
}

/*
 * Wrapper which adds @... ellipse
 */
static GObject *
gda_connection_statement_execute_v (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
				    GdaStatementModelUsage model_usage, GdaSet **last_inserted_row, GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	va_list ap;
	GObject *obj = NULL;
	GType *types, *req_types;
	GTimer *timer = NULL;
	va_start (ap, error);
	types = make_col_types_array (ap);
	va_end (ap);

	g_object_ref ((GObject*) cnc);

	gda_connection_lock ((GdaLockable*) cnc);
	_clear_connection_events (cnc);
	gda_connection_unlock ((GdaLockable*) cnc);

	if (last_inserted_row) 
		*last_inserted_row = NULL;

	if (!priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		g_object_unref ((GObject*) cnc);
		g_free (types);
		return NULL;
	}

	req_types = merge_column_types (_gda_statement_get_requested_types (stmt), types);
	if (req_types) {
		g_free (types);
		types = req_types;
		req_types = NULL;
	}
	else if (_gda_statement_get_requested_types (stmt))
		req_types = (GType*) _gda_statement_get_requested_types (stmt);

	if (! (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) &&
	    ! (model_usage & GDA_STATEMENT_MODEL_CURSOR_FORWARD))
		model_usage |= GDA_STATEMENT_MODEL_RANDOM_ACCESS;

	dump_exec_params (cnc, stmt, params);
	if (priv->exec_times)
		timer = g_timer_new ();

	obj = _gda_server_provider_statement_execute (priv->provider_obj, cnc, stmt, params, model_usage,
						      req_types ? req_types : types, last_inserted_row, error);
	if (timer)
		g_timer_stop (timer);
	g_free (types);

	if (obj) {
		if (timer)
			add_exec_time_to_object (obj, timer);
		update_meta_store_after_statement_exec (cnc, stmt, params);
	}

	g_object_unref ((GObject*) cnc);

	if (timer)
		g_timer_destroy (timer);

	return obj;
}


/**
 * gda_connection_execute_select_command:
 * @cnc: an opened connection
 * @sql: a query statement that must begin with "SELECT"
 * @error: a place to store errors, or %NULL
 *
 * Execute a SQL SELECT command over an opened connection.
 *
 * Returns: (transfer full): a new #GdaDataModel if successful, %NULL otherwise
 *
 * Since: 4.2.3
 */
GdaDataModel *
gda_connection_execute_select_command (GdaConnection *cnc, const gchar *sql, GError **error)
{
	GdaStatement *stmt;
	GdaDataModel *model;

	g_return_val_if_fail (sql != NULL
			      || GDA_IS_CONNECTION (cnc)
			      || !gda_connection_is_opened (cnc),
			      NULL);

	while (isspace (*sql))
		sql++;
	g_return_val_if_fail (((sql[0] == 'S') || (sql[0] == 's')) &&
			      ((sql[1] == 'E') || (sql[1] == 'e')) &&
			      ((sql[2] == 'L') || (sql[2] == 'l')) &&
			      ((sql[3] == 'E') || (sql[3] == 'e')) &&
			      ((sql[4] == 'C') || (sql[4] == 'c')) &&
			      ((sql[5] == 'T') || (sql[5] == 't')) &&
			      isspace (sql[6]), NULL);

	g_mutex_lock (&global_mutex);
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();
	g_mutex_unlock (&global_mutex);

	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt)
		return NULL;
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, error);
	g_object_unref (stmt);

	return model;
}

/**
 * gda_connection_execute_non_select_command:
 * @cnc: an opened connection
 * @sql: a query statement that must not begin with "SELECT"
 * @error: a place to store errors, or %NULL
 *
 * This is a convenience function to execute a SQL command over the opened connection. For the
 * returned value, see gda_connection_statement_execute_non_select()'s documentation.
 *
 * Returns: the number of rows affected or -1, or -2
 *
 * Since: 4.2.3
 */
gint
gda_connection_execute_non_select_command (GdaConnection *cnc, const gchar *sql, GError **error)
{
	GdaStatement *stmt;
	gint retval;

	g_return_val_if_fail (sql != NULL
			      || GDA_IS_CONNECTION (cnc)
			      || !gda_connection_is_opened (cnc), -1);

	g_mutex_lock (&global_mutex);
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();
	g_mutex_unlock (&global_mutex);

	stmt = gda_sql_parser_parse_string (internal_parser, sql, NULL, error);
	if (!stmt)
		return -1;

	retval = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error);
	g_object_unref (stmt);
	return retval;
}

/**
 * gda_connection_statement_execute:
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: in the case where @stmt is a SELECT statement, specifies how the returned data model will be used
 * @last_insert_row: (out) (transfer full) (nullable): a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Executes @stmt. 
 *
 * As @stmt can, by design (and if not abused), contain only one SQL statement, the
 * return object will either be:
 * <itemizedlist>
 *   <listitem><para>a #GdaDataSelect object (which is also a #GdaDataModel) if @stmt is a SELECT statement 
 *             (usually a GDA_SQL_STATEMENT_SELECT, see #GdaSqlStatementType)
 *             containing the results of the SELECT. The resulting data model is by default read only, but
 *             modifications can be enabled, see the #GdaDataSelect's documentation for more information.</para></listitem>
 *   <listitem><para>a #GdaSet for any other SQL statement which correctly executed. In this case
 *        (if the provider supports it), then the #GdaSet may contain value holders named:
 *        <itemizedlist>
 *          <listitem><para>a (gint) #GdaHolder named "IMPACTED_ROWS"</para></listitem>
 *          <listitem><para>a (GObject) #GdaHolder named "EVENT" which contains a #GdaConnectionEvent</para></listitem>
 *        </itemizedlist></para></listitem>
 * </itemizedlist>
 *
 * If @last_insert_row is not %NULL and @stmt is an INSERT statement, then it will contain a new #GdaSet
 * object composed of value holders named "+&lt;column number&gt;"
 * starting at column 0 which contain the actual inserted values. For example if a table is composed of an 'id' column
 * which is auto incremented and a 'name' column then the execution of a "INSERT INTO mytable (name) VALUES ('joe')"
 * query will return a #GdaSet with two holders:
 * <itemizedlist>
 *   <listitem><para>one with the '+0' ID which may for example contain 1 (note that its "name" property should be "id")</para></listitem>
 *   <listitem><para>one with the '+1' ID which will contain 'joe' (note that its "name" property should be "name")</para></listitem>
 * </itemizedlist>
 * Note that the value pointer by @last_insert_row may be %NULL after the function call if either the database provider
 * does not support it, or if the last interted row could not be determined (for example with SQLite if the table
 * in which the data is inserted has the WITHOUT ROWID optimization).
 *
 * This method may fail with a %GDA_SERVER_PROVIDER_ERROR domain error (see the #GdaServerProviderError error codes).
 *
 * Note1: If @stmt is a SELECT statement which has some parameters and  if @params is %NULL, then the statement can't
 * be executed and this method will return %NULL.
 *
 * Note2: If @stmt is a SELECT statement which has some parameters and  if @params is not %NULL but contains some
 * invalid parameters, then the statement can't be executed and this method will return %NULL, unless the
 * @model_usage has the GDA_STATEMENT_MODEL_ALLOW_NOPARAM flag.
 *
 * Note3: If @stmt is a SELECT statement which has some parameters and  if @params is not %NULL but contains some
 * invalid parameters and if @model_usage has the GDA_STATEMENT_MODEL_ALLOW_NOPARAM flag, then the returned
 * data model will contain no row but will have all the correct columns (even though some of the columns might
 * report as GDA_TYPE_NULL). In this case, if (after this method call) any of @params' parameters change
 * then the resulting data model will re-run itself, see the GdaDataSelect's 
 * <link linkend="GdaDataSelect--auto-reset">auto-reset</link> property for more information.
 *
 * Note4: if @model_usage does not contain the GDA_STATEMENT_MODEL_RANDOM_ACCESS or
 * GDA_STATEMENT_MODEL_CURSOR_FORWARD flags, then the default will be to return a random access data model
 *
 * Note5: If @stmt is a SELECT statement which returns blob values (of type %GDA_TYPE_BLOB), then an implicit
 * transaction will have been started by the database provider, and it's up to the caller to close the transaction
 * (which will then be locked) once all the blob ressources have been
 * liberated (when the returned data model is destroyed). See the section about
 * <link linkend="gen:blobs">Binary large objects (BLOBs)</link> for more information.
 *
 * Also see the <link linkend="limitations">provider's limitations</link>, and the
 * <link linkend="data-select">Advanced GdaDataSelect usage</link> sections.
 *
 * Returns: (transfer full): a #GObject, or %NULL if an error occurred 
 */
GObject *
gda_connection_statement_execute (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
				  GdaStatementModelUsage model_usage, GdaSet **last_inserted_row, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	return gda_connection_statement_execute_v (cnc, stmt, params, model_usage, last_inserted_row, error, -1);
}

/**
 * gda_connection_statement_execute_non_select:
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @last_insert_row: (out) (transfer full) (nullable): a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store an error, or %NULL
 *
 * Executes a non-selection statement on the given connection.
 *
 * This function returns the number of rows affected by the execution of @stmt, or -1
 * if an error occurred, or -2 if the connection's provider does not return the number of rows affected.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function. 
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * See gda_connection_statement_execute() form more information about @last_insert_row.
 *
 * Returns: the number of rows affected (&gt;=0) or -1 or -2 
 */
gint
gda_connection_statement_execute_non_select (GdaConnection *cnc, GdaStatement *stmt,
					     GdaSet *params, GdaSet **last_insert_row, GError **error)
{
	GdaSet *set;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), -1);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, -1);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), -1);

	if ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
	    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			      "%s", _("Statement is a selection statement"));
		return -1;
	}
	
	if (last_insert_row)
		*last_insert_row = NULL;

	set = (GdaSet *) gda_connection_statement_execute_v (cnc, stmt, params, 
							     GDA_STATEMENT_MODEL_RANDOM_ACCESS, last_insert_row, 
							     error, -1);
	if (!set)
		return -1;
	
	if (!GDA_IS_SET (set)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			      "%s", _("Statement is a selection statement"));
		g_object_unref (set);
		return -1;
	}
	else {
		GdaHolder *h;
		gint retval = -2;

		h = gda_set_get_holder (set, "IMPACTED_ROWS");
		if (h) {
			const GValue *value;
			value = gda_holder_get_value (h);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_INT))
				retval = g_value_get_int (value);
		}
		g_object_unref (set);
		return retval;
	}
}

/**
 * gda_connection_statement_execute_select:
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @error: a place to store an error, or %NULL
 *
 * Executes a selection command on the given connection.
 *
 * This function returns a #GdaDataModel resulting from the SELECT statement, or %NULL
 * if an error occurred.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function.
 *
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * Returns: (transfer full): a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select (GdaConnection *cnc, GdaStatement *stmt,
					 GdaSet *params, GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	model = (GdaDataModel *) gda_connection_statement_execute_v (cnc, stmt, params, 
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL,
								     error, -1);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			      "%s", _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_statement_execute_select_fullv:
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @error: a place to store an error, or %NULL
 * @...: a (-1 terminated) list of (column number, GType) specifying for each column mentioned the GType
 * of the column in the returned #GdaDataModel.
 *
 * Executes a selection command on the given connection.
 *
 * This function returns a #GdaDataModel resulting from the SELECT statement, or %NULL
 * if an error occurred.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function.
 *
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * Returns: (transfer full): a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select_fullv (GdaConnection *cnc, GdaStatement *stmt,
					       GdaSet *params, GdaStatementModelUsage model_usage,
					       GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return NULL;
	}

	va_list ap;
	GdaDataModel *model = NULL;
	GType *types, *req_types;
	GTimer *timer = NULL;

	va_start (ap, error);
	types = make_col_types_array (ap);
	va_end (ap);

	g_object_ref ((GObject*) cnc);

	gda_connection_lock ((GdaLockable*) cnc);
	_clear_connection_events (cnc);
	gda_connection_unlock ((GdaLockable*) cnc);

	if (!priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		g_object_unref ((GObject*) cnc);
		g_free (types);
		return NULL;
	}

	req_types = merge_column_types (_gda_statement_get_requested_types (stmt), types);
	if (req_types) {
		g_free (types);
		types = req_types;
		req_types = NULL;
	}
	else if (_gda_statement_get_requested_types (stmt))
		req_types = (GType*) _gda_statement_get_requested_types (stmt);

	if (! (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) &&
	    ! (model_usage & GDA_STATEMENT_MODEL_CURSOR_FORWARD))
		model_usage |= GDA_STATEMENT_MODEL_RANDOM_ACCESS;

	dump_exec_params (cnc, stmt, params);
	if (priv->exec_times)
		timer = g_timer_new ();

	model = (GdaDataModel*) _gda_server_provider_statement_execute (priv->provider_obj, cnc, stmt, params, model_usage,
									req_types ? req_types : types, NULL, error);
	if (timer)
		g_timer_stop (timer);

	g_object_unref ((GObject*) cnc);
	g_free (types);
	if (model && timer)
		add_exec_time_to_object ((GObject*) model, timer);
	if (timer)
		g_timer_destroy (timer);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     "%s", _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
		update_meta_store_after_statement_exec (cnc, stmt, params);
	}
	return model;
}

/**
 * gda_connection_statement_execute_select_full:
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: (nullable): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @col_types: (array) (nullable): an array of GType to request each returned #GdaDataModel's column's GType, terminated with the G_TYPE_NONE
 * value. Any value left to 0 will make the database provider determine the real GType. @col_types can also be %NULL if no
 * column type is specified.
 * @error: a place to store an error, or %NULL
 *
 * Executes a selection command on the given connection.
 *
 * This function returns a #GdaDataModel resulting from the SELECT statement, or %NULL
 * if an error occurred.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function.
 *
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * Returns: (transfer full): a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select_full (GdaConnection *cnc, GdaStatement *stmt,
					      GdaSet *params, GdaStatementModelUsage model_usage,
					      GType *col_types, GError **error)
{
	GdaDataModel *model = NULL;
	GTimer *timer = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return NULL;
	}

	g_object_ref ((GObject*) cnc);

	gda_connection_lock ((GdaLockable*) cnc);
 	_clear_connection_events (cnc);
	gda_connection_unlock ((GdaLockable*) cnc);
	
	if (!priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		g_object_unref ((GObject*) cnc);
		return NULL;
	}

	GType *req_types;
	req_types = merge_column_types (_gda_statement_get_requested_types (stmt), col_types);
	if (!req_types && !col_types)
		col_types = (GType*) _gda_statement_get_requested_types (stmt);

	if (! (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) &&
	    ! (model_usage & GDA_STATEMENT_MODEL_CURSOR_FORWARD))
		model_usage |= GDA_STATEMENT_MODEL_RANDOM_ACCESS;

	dump_exec_params (cnc, stmt, params);
	if (priv->exec_times)
		timer = g_timer_new ();

	model = (GdaDataModel*) _gda_server_provider_statement_execute (priv->provider_obj, cnc, stmt, params, model_usage,
									req_types ? req_types : col_types, NULL, error);
	if (timer)
		g_timer_stop (timer);
	g_free (req_types);
	g_object_unref ((GObject*) cnc);

	if (model && timer)
		add_exec_time_to_object ((GObject*) model, timer);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     "%s", _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
		update_meta_store_after_statement_exec (cnc, stmt, params);
	}
	if (timer)
		g_timer_destroy (timer);
	return model;
}

/**
 * gda_connection_repetitive_statement_execute:
 * @cnc: a #GdaConnection
 * @rstmt: a #GdaRepetitiveStatement object
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @col_types: (array) (nullable): an array of GType to request each returned GdaDataModel's column's GType, see gda_connection_statement_execute_select_full() for more information
 * @stop_on_error: set to TRUE if the method has to stop on the first error.
 * @error: a place to store errors, or %NULL
 *
 * Executes the statement upon which @rstmt is built. Note that as several statements can actually be executed by this
 * method, it is recommended to be within a transaction.
 *
 * If @error is not %NULL and @stop_on_error is %FALSE, then it may contain the last error which occurred.
 *
 * Returns: (transfer full) (element-type GObject): a new list of #GObject pointers (see gda_connection_statement_execute() for more information about what they
 * represent), one for each actual execution of the statement upon which @rstmt is built. If @stop_on_error is %FALSE, then
 * the list may contain some %NULL pointers which refer to statements which failed to execute.
 *
 * Since: 4.2
 */
GSList *
gda_connection_repetitive_statement_execute (GdaConnection *cnc, GdaRepetitiveStatement *rstmt,
					     GdaStatementModelUsage model_usage, GType *col_types,
					     gboolean stop_on_error, GError **error)
{
	GSList *sets_list, *list;
	GSList *retlist = NULL;
	GdaStatement *stmt;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_REPETITIVE_STATEMENT (rstmt), NULL);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return NULL;
	}

	g_object_get (rstmt, "statement", &stmt, NULL);
	g_return_val_if_fail (stmt, NULL);

	g_object_ref ((GObject*) cnc);

	gda_connection_lock ((GdaLockable*) cnc);
 	_clear_connection_events (cnc);
	gda_connection_unlock ((GdaLockable*) cnc);

	if (!priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		g_object_unref ((GObject*) cnc);
		return NULL;
	}

	GType *req_types;
	req_types = merge_column_types (_gda_statement_get_requested_types (stmt), col_types);
	if (!req_types && !col_types)
		col_types = (GType*) _gda_statement_get_requested_types (stmt);

	if (! (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) &&
	    ! (model_usage & GDA_STATEMENT_MODEL_CURSOR_FORWARD))
		model_usage |= GDA_STATEMENT_MODEL_RANDOM_ACCESS;

	sets_list = gda_repetitive_statement_get_all_sets (rstmt);
	for (list = sets_list; list; list = list->next) {
		GObject *obj = NULL;
		GError *lerror = NULL;
		GTimer *timer = NULL;

		dump_exec_params (cnc, stmt, (GdaSet*) list->data);
		if (priv->exec_times)
			timer = g_timer_new ();

		obj = _gda_server_provider_statement_execute (priv->provider_obj, cnc, stmt, GDA_SET (list->data),
							      model_usage, req_types ? req_types : col_types,
							      NULL, &lerror);
		if (timer)
			g_timer_stop (timer);
		if (!obj) {
			if (stop_on_error) {
				if (timer)
					g_timer_destroy (timer);
				break;
			}
			else {
				if (error && *error) {
					g_error_free (*error);
					*error = NULL;
				}
				g_propagate_error (error, lerror);
			}
		}
		else {
			if (timer)
				add_exec_time_to_object (obj, timer);
			update_meta_store_after_statement_exec (cnc, stmt, (GdaSet*) list->data);

			if (obj)
				retlist = g_slist_prepend (retlist, obj);
		}
		if (timer)
			g_timer_destroy (timer);
	}
	g_slist_free (sets_list);
	g_free (req_types);

	g_object_unref ((GObject*) cnc);
	g_object_unref (stmt);

	return g_slist_reverse (retlist);
}

/**
 * gda_connection_begin_transaction:
 * @cnc: a #GdaConnection object.
 * @name: (nullable): the name of the transation to start, or %NULL
 * @level: the requested transaction level (use %GDA_TRANSACTION_ISOLATION_SERVER_DEFAULT to apply the server default)
 * @error: a place to store errors, or %NULL
 *
 * Starts a transaction on the data source, identified by the @name parameter.
 *
 * Before starting a transaction, you can check whether the underlying
 * provider does support transactions or not by using the gda_connection_supports_feature() function.
 *
 * Returns: %TRUE if the transaction was started successfully with the corresponding isolation level, %FALSE otherwise.
 */
gboolean
gda_connection_begin_transaction (GdaConnection *cnc, const gchar *name, GdaTransactionIsolation level,
				  GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	return _gda_server_provider_begin_transaction (priv->provider_obj, cnc, name, level, error);
}

/**
 * gda_connection_commit_transaction:
 * @cnc: a #GdaConnection object.
 * @name: (nullable): the name of the transation to commit, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Commits the given transaction to the backend database. You need to call
 * gda_connection_begin_transaction() first.
 *
 * Returns: %TRUE if the transaction was finished successfully,
 * %FALSE otherwise.
 */
gboolean
gda_connection_commit_transaction (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	return _gda_server_provider_commit_transaction (priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_rollback_transaction:
 * @cnc: a #GdaConnection object.
 * @name: (nullable): the name of the transation to commit, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Rollbacks the given transaction. This means that all changes
 * made to the underlying data source since the last call to
 * #gda_connection_begin_transaction() or #gda_connection_commit_transaction()
 * will be discarded.
 *
 * Returns: %TRUE if the operation was successful, %FALSE otherwise.
 */
gboolean
gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	return _gda_server_provider_rollback_transaction (priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_add_savepoint:
 * @cnc: a #GdaConnection object
 * @name: (nullable): name of the savepoint to add
 * @error: a place to store errors or %NULL
 *
 * Adds a SAVEPOINT named @name.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_add_savepoint (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	return _gda_server_provider_add_savepoint (priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_rollback_savepoint:
 * @cnc: a #GdaConnection object
 * @name: (nullable): name of the savepoint to rollback to
 * @error: a place to store errors or %NULL
 *
 * Rollback all the modifications made after the SAVEPOINT named @name.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_rollback_savepoint (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	return _gda_server_provider_rollback_savepoint (priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_delete_savepoint:
 * @cnc: a #GdaConnection object
 * @name: (nullable): name of the savepoint to delete
 * @error: a place to store errors or %NULL
 *
 * Delete the SAVEPOINT named @name when not used anymore.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_delete_savepoint (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	return _gda_server_provider_delete_savepoint (priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_get_transaction_status:
 * @cnc: a #GdaConnection object
 *
 * Get the status of @cnc regarding transactions. The returned object should not be modified
 * or destroyed; however it may be modified or destroyed by the connection itself.
 *
 * If %NULL is returned, then no transaction has been associated with @cnc
 *
 * Returns: (transfer none): a #GdaTransactionStatus object, or %NULL
 */
GdaTransactionStatus *
gda_connection_get_transaction_status (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	return priv->trans_status;
}

/**
 * gda_connection_supports_feature:
 * @cnc: a #GdaConnection object.
 * @feature: feature to ask for.
 *
 * Asks the underlying provider for if a specific feature is supported.
 *
 * Returns: %TRUE if the provider supports it, %FALSE if not.
 */
gboolean
gda_connection_supports_feature (GdaConnection *cnc, GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	return gda_server_provider_supports_feature (priv->provider_obj, cnc, feature);
}

/* builds a list of #GdaMetaContext contexts templates: contexts which have a non NULL table_name,
 * and empty or partially filled column names and values specifications */
static GSList *
build_upstream_context_templates (GdaMetaStore *store, GdaMetaContext *context, GSList *elist, GError **error)
{
	GSList *depend_on_contexts;
	GSList *retlist;
	GError *lerror = NULL;
	depend_on_contexts = _gda_meta_store_schema_get_upstream_contexts (store, context, &lerror);
	if (!depend_on_contexts) {
		if (lerror) {
			/* error while getting dependencies */
			g_propagate_error (error, lerror);
			return FALSE;
		}
		return elist;
	}
	else {
		GSList *list;
		retlist = NULL;
		for (list = depend_on_contexts; list; list = list->next) 
			retlist = build_upstream_context_templates (store, (GdaMetaContext *) list->data,
								    retlist, error);
		list = g_slist_concat (depend_on_contexts, elist);
		retlist = g_slist_concat (retlist, list);
		return retlist;
	}
}


/* builds a list of #GdaMetaContext contexts templates: contexts which have a non NULL table_name,
 * and empty or partially filled column names and values specifications */
static GSList *
build_downstream_context_templates (GdaMetaStore *store, GdaMetaContext *context, GSList *elist, GError **error)
{
	GSList *depending_contexts;
	GSList *retlist;
	GError *lerror = NULL;
	depending_contexts = _gda_meta_store_schema_get_downstream_contexts (store, context, &lerror);
	if (!depending_contexts) {
		if (lerror) {
			/* error while getting dependencies */
			g_propagate_error (error, lerror);
			return NULL;
		}
		return elist;
	}
	else {
		GSList *list;
		retlist = NULL;
		for (list = depending_contexts; list; list = list->next) 
			retlist = build_downstream_context_templates (store, (GdaMetaContext *) list->data,
								      retlist, error);
		list = g_slist_concat (elist, depending_contexts);
		retlist = g_slist_concat (list, retlist);
		return retlist;
	}
}

/*#define GDA_DEBUG_META_STORE_UPDATE*/
#ifdef GDA_DEBUG_META_STORE_UPDATE
static void
meta_context_dump (GdaMetaContext *context)
{
	gchar *str;
	str = gda_meta_context_stringify (context);
	g_print ("GdaMetaContext for table %s: %s\n", context->table_name, str);
	g_free (str);
}
#endif

/*
 *
 */
static gint
check_parameters (GdaMetaContext *context, GError **error, gint nb, ...)
{
#define MAX_PARAMS 10
	gint i;
	va_list ap;
	gint retval = -1;
	GValue **pvalue;
	struct {
		GValue **pvalue;
		GType    type;
	} spec_array [MAX_PARAMS];
	gint nb_params = 0;

	va_start (ap, nb);
	/* make a list of all the GValue pointers */
	for (pvalue = va_arg (ap, GValue **); pvalue; pvalue = va_arg (ap, GValue **), nb_params++) {
		g_assert (nb_params < MAX_PARAMS); /* hard limit, recompile to change it (should never be needed) */
		spec_array[nb_params].pvalue = pvalue;
		spec_array[nb_params].type = va_arg (ap, GType);
	}

	/* test against each test case */
	for (i = 0; i < nb; i++) {
		gchar *pname;
		gboolean allfound = TRUE;
		gint j;
		for (j = 0; j < nb_params; j++)
			*(spec_array[j].pvalue) = NULL;

		for (pname = va_arg (ap, gchar*); pname; pname = va_arg (ap, gchar*)) {
			gint j;
			pvalue = va_arg (ap, GValue **);
			*pvalue = NULL;
			for (j = 0; allfound && (j < context->size); j++) {
				if (!strcmp (context->column_names[j], pname)) {
					*pvalue = context->column_values[j];
					break;
				}
			}
			if (j == context->size)
				allfound = FALSE;
		}
		if (allfound) {
			retval = i;
			break;
		}
	}
	va_end (ap);

	if (retval >= 0) {
		gint j;
		for (j = 0; j < nb_params; j++) {
			GValue *v = *(spec_array[j].pvalue);
			if (v && (gda_value_is_null (v) || (G_VALUE_TYPE (v) != spec_array[j].type))) {
				g_set_error (error, GDA_CONNECTION_ERROR,
					     GDA_CONNECTION_META_DATA_CONTEXT_ERROR,
					     "%s", _("Invalid argument"));
				retval = -1;
			}
		}
	}
	else {
		gchar *str;
		str = gda_meta_context_stringify (context);
		g_set_error (error, GDA_CONNECTION_ERROR,
			     GDA_CONNECTION_META_DATA_CONTEXT_ERROR,
			     _("Missing or wrong arguments for table '%s': %s"),
			     context->table_name, str);
		g_free (str);
		retval = -1;
	}

	/*g_print ("Check arguments context => found %d\n", retval);*/
	return retval;
}

static gboolean
local_meta_update (GdaServerProvider *provider, GdaConnection *cnc, GdaMetaContext *context, GError **error)
{
	const gchar *tname = context->table_name;
	GdaMetaStore *store;
	gboolean retval;

	const GValue *catalog = NULL;
	const GValue *schema = NULL;
	const GValue *name = NULL;
	gint i;

#ifdef GDA_DEBUG_META_STORE_UPDATE
	g_print ("%s() => ", __FUNCTION__);
	meta_context_dump (context);
#endif

	if (*tname != '_')
		return TRUE;
	tname ++;
	
	store = gda_connection_get_meta_store (cnc);
	switch (*tname) {
	case 'b': {
		/* _builtin_data_types, params: 
		 *  - none
		 */
		ASSERT_TABLE_NAME (tname, "builtin_data_types");
		retval = _gda_server_provider_meta_0arg (provider, cnc, store, context, GDA_SERVER_META__BTYPES, error);
		WARN_META_UPDATE_FAILURE (retval, "_btypes");
		return retval;
	}
	break;
	case 'c': 
		if ((tname[1] == 'o') && (tname[2] == 'l') && (tname[3] == 'u')) {
			/* _columns,  params: 
			 *  -0- @table_catalog, @table_schema, @table_name
			 *  -1- @character_set_catalog, @character_set_schema, @character_set_name
			 *  -2- @collation_catalog, @collation_schema, @collation_name
			 */
			if (gda_meta_context_get_n_columns (context) == 3)
			{
				i = check_parameters (context, error, 3,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &name, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &name, NULL,
							    "character_set_catalog", &catalog, "character_set_schema", &schema, "character_set_name", &name, NULL,
							    "collation_catalog", &catalog, "collation_schema", &schema, "collation_name", &name, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "columns");
				retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
									 GDA_SERVER_META_COLUMNS, catalog, schema, name, error);
				WARN_META_UPDATE_FAILURE (retval, "columns");
			}
			else {
				ASSERT_TABLE_NAME (tname, "columns");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__COLUMNS, error);
				WARN_META_UPDATE_FAILURE (retval, "columns");
			}
			return retval;
		}
		else if ((tname[1] == 'o') && (tname[2] == 'l')) {
			/* _collations, params: 
			 *  -0- @collation_catalog, @collation_schema, @collation_name
			 *  -1- @collation_catalog, @collation_schema
			 */
			i = check_parameters (context, error, 2,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "collation_catalog", &catalog, "collation_schema", &schema, "collation_name", &name, NULL,
					      "collation_catalog", &catalog, "collation_schema", &schema, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "collations");
			retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
								 GDA_SERVER_META_COLLATIONS, catalog, schema, name, error);
			WARN_META_UPDATE_FAILURE (retval, "collations");
			return retval;
		}
		else if ((tname[1] == 'h') && (tname[2] == 'a')) {
			/* _character_sets, params: 
			 *  -0- @character_set_catalog, @character_set_schema, @character_set_name
			 *  -1- @character_set_catalog, @character_set_schema
			 */
			i = check_parameters (context, error, 2,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "character_set_catalog", &catalog, "character_set_schema", &schema, "character_set_name", &name, NULL,
					      "character_set_catalog", &catalog, "character_set_schema", &schema, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "character_sets");
			retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
								 GDA_SERVER_META_CHARACTER_SETS, catalog, schema, name, error);
			WARN_META_UPDATE_FAILURE (retval, "character_sets");
			return retval;
		}
		else if ((tname[1] == 'h') && (tname[2] == 'e')) {
			/* _check_column_usage, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
			 *  -1- @table_catalog, @table_schema, @table_name, @column_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "check_column_usage");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__CHECK_COLUMNS, error);
				WARN_META_UPDATE_FAILURE (retval, "check_columns");
			} else {
				const GValue *tabname = NULL;
				const GValue *cname = NULL;
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &tabname, G_TYPE_STRING,
							    &cname, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "constraint_name", &cname, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "column_name", &cname, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "check_column_usage");
				retval = _gda_server_provider_meta_4arg (provider, cnc, store, context,
									 GDA_SERVER_META_CHECK_COLUMNS, catalog, schema, tabname, cname, error);
				WARN_META_UPDATE_FAILURE (retval, "check_columns");
			}
			return retval;
		}
		break;

	case 'd': 
		if ((tname[1] == 'o') && (tname[2] == 'm') && (tname[3] == 'a') && (tname[4] == 'i') && (tname[5] == 'n') &&
		    (tname[6] == 's')) {
			/* _domains, params: 
			 *  -0- @domain_catalog, @domain_schema
			 */
			i = check_parameters (context, error, 1,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING, NULL,
					      "domain_catalog", &catalog, "domain_schema", &schema, NULL);

			ASSERT_TABLE_NAME (tname, "domains");
			if (i < 0) {
				g_clear_error (error);
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__DOMAINS, error);
				WARN_META_UPDATE_FAILURE (retval, "domains");
			} else {
				retval = _gda_server_provider_meta_2arg (provider, cnc, store, context,
									 GDA_SERVER_META_DOMAINS, catalog, schema, error);
				WARN_META_UPDATE_FAILURE (retval, "domains");
			}
			return retval;
		}
		else {
			/* _domain_constraints, params: 
			 *  -0- @domain_catalog, @domain_schema, @domain_name
			 *  -1- @constraint_catalog, @constraint_schema
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "domain_constraints");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__CONSTRAINTS_DOM, error);
				WARN_META_UPDATE_FAILURE (retval, "constraints_dom");
			} else {
				const GValue *domname = NULL;
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &domname, G_TYPE_STRING, NULL,
							    "domain_catalog", &catalog, "domain_schema", &schema, "domain_name", &domname, NULL,
							    "constraint_catalog", &catalog, "constraint_schema", &schema, NULL);

				if (i < 0)
					return FALSE;
				if (i == 1)
					return TRUE; /* nothing to do */

				ASSERT_TABLE_NAME (tname, "domain_constraints");
				retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
									 GDA_SERVER_META_CONSTRAINTS_DOM, catalog, schema, domname, error);
				WARN_META_UPDATE_FAILURE (retval, "constraints_dom");
			}

			return retval;
		}
		break;

	case 'e':
		if (tname[1] == 'n') {
			/* _enums, params: 
			 *  -0- @udt_catalog, @udt_schema, @udt_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "enums");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__ENUMS, error);
				WARN_META_UPDATE_FAILURE (retval, "enums");
			} else {
				i = check_parameters (context, error, 1,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "udt_catalog", &catalog, "udt_schema", &schema, "udt_name", &name, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "enums");
				retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
									 GDA_SERVER_META_ENUMS, catalog, schema, name, error);
				WARN_META_UPDATE_FAILURE (retval, "enums");
			}

			return retval;
		}
		else {
			/* _element_types, params:
			 *  - none
			 *  -0- @specific_name
			 */
			i = check_parameters (context, error, 1,
					      &name, G_TYPE_STRING, NULL,
					      "specific_name", &name, NULL);

			ASSERT_TABLE_NAME (tname, "element_types");
			if (i < 0) {
				g_clear_error (error);
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__EL_TYPES, error);
				WARN_META_UPDATE_FAILURE (retval, "_el_types");
			} else {
				retval = _gda_server_provider_meta_1arg (provider, cnc, store, context,
									 GDA_SERVER_META_EL_TYPES, name, error);
				WARN_META_UPDATE_FAILURE (retval, "el_types");
			}
			return retval;
		}
		break;

	case 'i':
		if ((tname[1] == 'n') && (tname[2] == 'f')) {
			/* _information_schema_catalog_name, params: 
			 *  - none
			 */
			ASSERT_TABLE_NAME (tname, "information_schema_catalog_name");
			retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
								 GDA_SERVER_META__INFO, error);
			WARN_META_UPDATE_FAILURE (retval, "_info");
			return retval;
		}
		else {
			/* _index_column_usage, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @index_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "index_column_usage");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__INDEX_COLS, error);
				WARN_META_UPDATE_FAILURE (retval, "index_cols");
			} else {
				const GValue *iname = NULL;
				i = check_parameters (context, error, 1,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &name, G_TYPE_STRING,
							    &iname, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &name, "index_name", &iname, NULL);

				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "index_column_usage");
				retval = _gda_server_provider_meta_4arg (provider, cnc, store, context,
									 GDA_SERVER_META_INDEX_COLS, catalog, schema, name, iname, error);
				WARN_META_UPDATE_FAILURE (retval, "index_cols");
			}
			return retval;
		}
		break;
	case 'k': {
		/* _key_column_usage, params: 
		 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
		 *  -0- @ref_table_catalog, @ref_table_schema, @ref_table_name, @ref_constraint_name
		 */
		if (gda_meta_context_get_n_columns (context) == 0) {
			ASSERT_TABLE_NAME (tname, "key_column_usage");
			retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
								 GDA_SERVER_META__KEY_COLUMNS, error);
			WARN_META_UPDATE_FAILURE (retval, "key_columns");
		} else {
			const GValue *tabname = NULL;
			const GValue *cname = NULL;
			i = check_parameters (context, error, 2,
						    &catalog, G_TYPE_STRING,
						    &schema, G_TYPE_STRING,
						    &tabname, G_TYPE_STRING,
						    &cname, G_TYPE_STRING, NULL,
						    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "constraint_name", &cname, NULL,
						    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "column_name", &cname, NULL);
			if (i < 0)
				return FALSE;

			ASSERT_TABLE_NAME (tname, "key_column_usage");
			retval = _gda_server_provider_meta_4arg (provider, cnc, store, context,
								 GDA_SERVER_META_KEY_COLUMNS, catalog, schema, tabname, cname, error);
			WARN_META_UPDATE_FAILURE (retval, "key_columns");
		}
		return retval;
		break;
	}

	case 'p':
		/* _parameters, params: 
		 *  -0- @specific_catalog, @specific_schema, @specific_name
		 */
		i = check_parameters (context, error, 1,
				      &catalog, G_TYPE_STRING,
				      &schema, G_TYPE_STRING,
				      &name, G_TYPE_STRING, NULL,
				      "specific_catalog", &catalog, "specific_schema", &schema, "specific_name", &name, NULL);

		ASSERT_TABLE_NAME (tname, "parameters");
		if (i < 0) {
			g_clear_error (error);
			retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
								 GDA_SERVER_META__ROUTINE_PAR, error);
			WARN_META_UPDATE_FAILURE (retval, "routine_par");
		} else {
			retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
								 GDA_SERVER_META_ROUTINE_PAR, catalog, schema, name, error);
			WARN_META_UPDATE_FAILURE (retval, "routine_par");
		}
		return retval;

	case 'r': 
		if ((tname[1] == 'e') && (tname[2] == 'f')) {
			/* _referential_constraints, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
			 *  -0- @ref_table_catalog, @ref_table_schema, @ref_table_name, @ref_constraint_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "referential_constraints");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
										 GDA_SERVER_META__CONSTRAINTS_REF, error);
				WARN_META_UPDATE_FAILURE (retval, "constraints_ref");
				return retval;
			} else {
				const GValue *tabname = NULL;
				const GValue *cname = NULL;
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &tabname, G_TYPE_STRING,
							    &cname, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "constraint_name", &cname, NULL,
							    "ref_table_catalog", &catalog, "ref_table_schema", &schema, "ref_table_name", &tabname, "ref_constraint_name", &cname, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "referential_constraints");
				if (i == 0) {
					retval = _gda_server_provider_meta_4arg (provider, cnc, store, context,
										 GDA_SERVER_META_CONSTRAINTS_REF, catalog, schema, tabname, cname, error);
					WARN_META_UPDATE_FAILURE (retval, "constraints_ref");
					return retval;
				}
				else {
					/* nothing to do */
					return TRUE;
				}
			}
			return TRUE;
		}
		if ((tname[1] == 'o') && (tname[2] == 'u') && (tname[3] == 't') && (tname[4] == 'i') &&
		    (tname[5] == 'n') && (tname[6] == 'e') && (tname[7] == 's')) {
			/* _routines, params: 
			 *  -0- @specific_catalog, @specific_schema, @specific_name
			 *  -1- @specific_catalog, @specific_schema
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "routines");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__ROUTINES, error);
				WARN_META_UPDATE_FAILURE (retval, "routines");
			} else {
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &name, G_TYPE_STRING, NULL,
							    "specific_catalog", &catalog, "specific_schema", &schema, "specific_name", &name, NULL,
							    "specific_catalog", &catalog, "specific_schema", &schema, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "routines");
				retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
									 GDA_SERVER_META_ROUTINES, catalog, schema, name, error);
				WARN_META_UPDATE_FAILURE (retval, "routines");
			}

			return retval;
		}
		else {
			/* _routine_columns, params: 
			 *  -0- @specific_catalog, @specific_schema, @specific_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "routine_columns");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__ROUTINE_COL, error);
				WARN_META_UPDATE_FAILURE (retval, "routine_col");
			} else {
				const GValue *col_name = NULL;
				const GValue *ordinal = NULL;
				i = check_parameters (context, error, 1,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &name, G_TYPE_STRING,
							    &col_name, G_TYPE_STRING,
							    &ordinal, G_TYPE_INT, NULL,
							    "specific_catalog", &catalog, "specific_schema", &schema, "specific_name", &name, "column_name", &col_name, "ordinal_position", &ordinal, NULL);
				if (i < 0) {
					g_message ("Fail _routine_columns");
					return FALSE;
				}
				ASSERT_TABLE_NAME (tname, "routine_columns");
				retval = _gda_server_provider_meta_5arg (provider, cnc, store, context,
									 GDA_SERVER_META_ROUTINE_COL, catalog, schema, name, col_name, ordinal, error);
				WARN_META_UPDATE_FAILURE (retval, "routine_col");
			}
			return retval;
		}
		break;

	case 's': {
		/* _schemata, params:
		 *  -0- @catalog_name, @schema_name 
		 *  -1- @catalog_name
		 */
		ASSERT_TABLE_NAME (tname, "schemata");
		if (gda_meta_context_get_n_columns (context) == 0) {
			retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
							 GDA_SERVER_META__SCHEMATA, error);
			WARN_META_UPDATE_FAILURE (retval, "schemata");
		} else {
			i = check_parameters (context, error, 2,
						    &schema, G_TYPE_STRING, NULL,
						    "catalog_name", &catalog, "schema_name", &schema, NULL,
						    "catalog_name", &catalog, NULL);
			if (i < 0)
			return FALSE;

			retval = _gda_server_provider_meta_2arg (provider, cnc, store, context,
								 GDA_SERVER_META_SCHEMATA, catalog, schema, error);
			WARN_META_UPDATE_FAILURE (retval, "schemata");
		}
		return retval;
	}
	case 't': 
		if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && (tname[5] == 's')) {
			/* _tables, params: 
			 *  -0- @table_catalog, @table_schema, @table_name
			 *  -1- @table_catalog, @table_schema
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "tables");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__TABLES_VIEWS, error);
				WARN_META_UPDATE_FAILURE (retval, "tables_views");
			} else {
				i = check_parameters (context, error, 3,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &name, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &name, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, NULL,
							    "table_name", &name, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "tables");
				retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
									 GDA_SERVER_META_TABLES_VIEWS, catalog, schema, name, error);
				WARN_META_UPDATE_FAILURE (retval, "tables_views");
			}
			
			return retval;
		}
		else if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && 
			 (tname[5] == '_') && (tname[6] == 'c')) {
			/* _tables_constraints, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
			 *  -1- @table_catalog, @table_schema, @table_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
								 GDA_SERVER_META__CONSTRAINTS_TAB, error);
				WARN_META_UPDATE_FAILURE (retval, "constraints_tab");
			} else {
				const GValue *cname = NULL;
				const GValue *tabname = NULL;
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &cname, G_TYPE_STRING,
							    &tabname, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "constraint_name", &cname, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, NULL);

				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "table_constraints");
				retval = _gda_server_provider_meta_4arg (provider, cnc, store, context,
									 GDA_SERVER_META_CONSTRAINTS_TAB, catalog, schema, tabname, cname, error);
				WARN_META_UPDATE_FAILURE (retval, "constraints_tab");
			}

			return retval;
		}
		else if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && 
			 (tname[5] == '_') && (tname[6] == 'i')) {
			/* _table_indexes, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @index_name
			 *  -1- @table_catalog, @table_schema, @table_name
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "table_indexes");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__INDEXES_TAB, error);
				WARN_META_UPDATE_FAILURE (retval, "indexes_tab");
			} else {
				const GValue *iname = NULL;
				const GValue *tabname = NULL;
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &iname, G_TYPE_STRING,
							    &tabname, G_TYPE_STRING, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, "index_name", &iname, NULL,
							    "table_catalog", &catalog, "table_schema", &schema, "table_name", &tabname, NULL);

				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "table_indexes");
				retval = _gda_server_provider_meta_4arg (provider, cnc, store, context,
									 GDA_SERVER_META_INDEXES_TAB, catalog, schema, tabname, iname, error);
				WARN_META_UPDATE_FAILURE (retval, "indexes_tab");
			}

			return retval;
		}
		else if ((tname[1] == 'r') && (tname[2] == 'i')) {
			/* _triggers,  params: 
			 *  -0- @trigger_catalog, @trigger_schema
			 *  -1- @event_object_catalog, @event_object_schema, @event_object_table
			 */

			ASSERT_TABLE_NAME (tname, "triggers");
			if (gda_meta_context_get_n_columns (context) == 0) {
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
										 GDA_SERVER_META__TRIGGERS, error);
			} else {
				i = check_parameters (context, error, 2,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING,
							    &name, G_TYPE_STRING, NULL,
							    "trigger_catalog", &catalog, "trigger_schema", &schema, NULL,
							    "event_object_catalog", &catalog, "event_object_schema", &schema, "event_object_table", &name, NULL);
				if (i < 0)
					return FALSE;
				if (i == 1) {
					retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
										 GDA_SERVER_META_TRIGGERS, catalog, schema, name, error);
					WARN_META_UPDATE_FAILURE (retval, "triggers");
					return retval;
				}
				else {
					/* nothing to do */
					return TRUE;
				}
			}
		}
		break;

	case 'u': 
		if ((tname[1] == 'd') && (tname[2] == 't') && (tname[3] == 0)) {
			/* _udt, params: 
			 *  -0- @udt_catalog, @udt_schema
			 */
			if (gda_meta_context_get_n_columns (context) == 0) {
				ASSERT_TABLE_NAME (tname, "udt");
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__UDT, error);
				WARN_META_UPDATE_FAILURE (retval, "udt");
			} else {
				i = check_parameters (context, error, 1,
							    &catalog, G_TYPE_STRING,
							    &schema, G_TYPE_STRING, NULL,
							    "udt_catalog", &catalog, "udt_schema", &schema, NULL);
				if (i < 0)
					return FALSE;

				ASSERT_TABLE_NAME (tname, "udt");
				retval = _gda_server_provider_meta_2arg (provider, cnc, store, context,
									 GDA_SERVER_META_UDT, catalog, schema, error);
				WARN_META_UPDATE_FAILURE (retval, "udt");
			}
			return retval;
		}
		else if ((tname[1] == 'd') && (tname[2] == 't') && (tname[3] == '_')) {
			/* _udt_columns, params: 
			 *  -0- @udt_catalog, @udt_schema, @udt_name
			 */
			i = check_parameters (context, error, 1,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "udt_catalog", &catalog, "udt_schema", &schema, "udt_name", &name, NULL);
			if (i < 0) {
				g_clear_error (error);
				retval = _gda_server_provider_meta_0arg (provider, cnc, store, context,
									 GDA_SERVER_META__UDT_COLS, error);
				WARN_META_UPDATE_FAILURE (retval, "_el_types");
			} else {
				ASSERT_TABLE_NAME (tname, "udt_columns");
				retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
									 GDA_SERVER_META_UDT_COLS, catalog, schema, name, error);
				WARN_META_UPDATE_FAILURE (retval, "udt_cols");
			}
			return retval;
		}
		break;

	case 'v': {
		/* _view_column_usage,  params: 
		 *  -0- @view_catalog, @view_schema, @view_name
		 *  -1- @table_catalog, @table_schema, @table_name, @column_name
		 */
		if (gda_meta_context_get_n_columns (context) == 0) {
			retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
								 GDA_SERVER_META_VIEW_COLS, catalog, schema, name, error);
			WARN_META_UPDATE_FAILURE (retval, "view_cols");
		} else {
			const GValue *colname = NULL;
			i = check_parameters (context, error, 2,
						    &catalog, G_TYPE_STRING,
						    &schema, G_TYPE_STRING,
						    &colname, G_TYPE_STRING,
						    &name, G_TYPE_STRING, NULL,
						    "view_catalog", &catalog, "view_schema", &schema, "view_name", &name, NULL,
						    "table_catalog", &catalog, "table_schema", &schema, "table_name", &name,
						    "column_name", &colname, NULL);
			if (i < 0)
				return FALSE;

			ASSERT_TABLE_NAME (tname, "view_column_usage");
			retval = _gda_server_provider_meta_3arg (provider, cnc, store, context,
								 GDA_SERVER_META_VIEW_COLS, catalog, schema, name, error);
			WARN_META_UPDATE_FAILURE (retval, "view_cols");
		}

		return retval;
		break;
	}
	default:
		break;
	}
	return TRUE;
}

typedef struct {
	GdaServerProvider  *prov;
	GdaConnection      *cnc;
	GError             *error;
	GSList             *context_templates;
	GHashTable         *context_templates_hash;
} DownstreamCallbackData;

// UNUSED METHOD
/* static GError * */
/* suggest_update_cb_downstream (G_GNUC_UNUSED GdaMetaStore *store, GdaMetaContext *suggest, DownstreamCallbackData *data) */
/* { */
/* #define MAX_CONTEXT_SIZE 10 */
/* 	if (data->error) */
/* 		return data->error; */

/* 	GdaMetaContext *templ_context; */
/* 	GdaMetaContext loc_suggest; */
/* 	gchar *column_names[MAX_CONTEXT_SIZE]; */
/* 	GValue *column_values[MAX_CONTEXT_SIZE]; */

	/* if there is no context with the same table name in the templates, then exit right now */
/* 	templ_context = g_hash_table_lookup (data->context_templates_hash, suggest->table_name); */
/* 	if (!templ_context) */
/* 		return NULL; */
	
/* 	if (templ_context->size > 0) { */
		/* setup @loc_suggest */
/* 		gint i, j; */

/* 		if (suggest->size > MAX_CONTEXT_SIZE) { */
/* 			g_warning ("Internal limitation at %s(), limitation should be at least %d, please report a bug", */
/* 				   __FUNCTION__, suggest->size); */
/* 			return NULL; */
/* 		} */
/* 		loc_suggest.size = suggest->size; */
/* 		loc_suggest.table_name = suggest->table_name; */
/* 		loc_suggest.column_names = column_names; */
/* 		loc_suggest.column_values = column_values; */
/* 		memcpy (loc_suggest.column_names, suggest->column_names, sizeof (gchar *) * suggest->size);  */ /* Flawfinder: ignore */
/* 		memcpy (loc_suggest.column_values, suggest->column_values, sizeof (GValue *) * suggest->size);  */ /* Flawfinder: ignore */
		
		/* check that any @suggest's columns which is in @templ_context's has the same values */
/* 		for (j = 0; j < suggest->size; j++) { */
/* 			for (i = 0; i < templ_context->size; i++) { */
/* 				if (!strcmp (templ_context->column_names[i], suggest->column_names[j])) { */
					/* same column name, now check column value */
/* 					if (G_VALUE_TYPE (templ_context->column_values[i]) !=  */
/* 					    G_VALUE_TYPE (suggest->column_values[j])) { */
/* 						g_warning ("Internal error: column types mismatch for GdaMetaContext " */
/* 							   "table '%s' and column '%s' (%s/%s)", */
/* 							   templ_context->table_name, templ_context->column_names[i],  */
/* 							   g_type_name (G_VALUE_TYPE (templ_context->column_values[i])), */
/* 							   g_type_name (G_VALUE_TYPE (suggest->column_values[j]))); */
/* 						return NULL; */
/* 					} */
/* 					if (gda_value_compare (templ_context->column_values[i], suggest->column_values[j])) */
						/* different values */
/* 						return NULL; */
/* 					break; */
/* 				} */
/* 			} */
/* 		} */

		/* @templ_context may contain some more columns => add them to @loc_suggest */
/* 		for (i = 0; i < templ_context->size; i++) { */
/* 			for (j = 0; j < suggest->size; j++) { */
/* 				if (!strcmp (templ_context->column_names[i], suggest->column_names[j])) { */
/* 					j = -1; */
/* 					break; */
/* 				} */
/* 			} */
/* 			if (j >= 0) { */
/* 				if (loc_suggest.size >= MAX_CONTEXT_SIZE) { */
/* 					g_warning ("Internal limitation at %s(), limitation should be at least %d, please report a bug", */
/* 						   __FUNCTION__, loc_suggest.size + 1); */
/* 					return NULL; */
/* 				} */
/* 				loc_suggest.column_names [loc_suggest.size] = templ_context->column_names [i]; */
/* 				loc_suggest.column_values [loc_suggest.size] = templ_context->column_values [i]; */
/* 				loc_suggest.size ++; */
/* 			} */
/* 		} */

/* 		suggest = &loc_suggest; */
/* 	} */
	
/* 	GError *lerror = NULL; */
/* 	if (!local_meta_update (data->prov, data->cnc, suggest, &lerror)) { */
/* 		if (lerror) */
/* 			data->error = lerror; */
/* 		else { */
/* 			g_set_error (&lerror,GDA_CONNECTION_ERROR, */
/* 				     GDA_CONNECTION_META_DATA_CONTEXT_ERROR, */
/* 				      "%s", _("Meta update error")); */
/* 			data->error = lerror; */
/* 		} */

/* 		return data->error; */
/* 	} */

/* 	return NULL; */
/* } */

/**
 * gda_connection_update_meta_store:
 * @cnc: a #GdaConnection object.
 * @context: (nullable): description of which part of @cnc's associated #GdaMetaStore should be updated, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Updates @cnc's associated #GdaMetaStore. If @context is not %NULL, then only the parts described by
 * @context will be updated, and if it is %NULL, then the complete meta store will be updated. Detailed
 * explanations follow:
 *
 * In order to keep the meta store's contents in a consistent state, the update process involves updating
 * the contents of all the tables related to one where the contents change. For example the "_columns"
 * table (which lists all the columns of a table) depends on the "_tables" table (which lists all the tables
 * in a schema), so if a row is added, removed or modified in the "_tables", then the "_columns" table's contents
 * needs to be updated as well regarding that row.
 *
 * If @context is %NULL, then the update process will simply overwrite any data that was present in all the
 * meta store's tables with new (up to date) data even if nothing has changed, without having to build the
 * tables' dependency tree. This is the recommended way of proceeding when dealing with a meta store which
 * might be outdated.
 *
 * On the other hand, if @context is not %NULL, then a tree of the dependencies has to be built (depending on
 * @context) and only some parts of the meta store are updated following that dependencies tree. Specifying a
 * context may be useful for example in the following situations:
 * <itemizedlist>
 *   <listitem><para>One knows that a database object has changed (for example a table created), and
 *                   may use the @context to request that only the information about that table be updated
 *             </para></listitem>
 *   <listitem><para>One is only interested in the list of views, and may request that only the information
 *                   about views may be updated</para></listitem>
 * </itemizedlist>
 *
 * When @context is not %NULL, and contains specified SQL identifiers (for example the "table_name" of the "_tables"
 * table), then each SQL identifier has to match the convention the #GdaMetaStore has adopted regarding
 * case sensitivity, using gda_connection_quote_sql_identifier() or gda_meta_store_sql_identifier_quote().
 *
 * see the <link linkend="information_schema:sql_identifiers">
 * meta data section about SQL identifiers</link> for more information, and the documentation about the
 * gda_sql_identifier_quote() function which will be most useful.
 *
 * Note however that usually <emphasis>more</emphasis> information will be updated than strictly requested by
 * the @context argument.
 *
 * For more information, see the <link linkend="information_schema">Database structure</link> section, and
 * the <link linkend="howto-meta2">Update the meta data about a table</link> howto.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_update_meta_store (GdaConnection *cnc, GdaMetaContext *context, GError **error)
{
	GdaMetaStore *store;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}
	gda_connection_lock ((GdaLockable*) cnc);

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	g_assert (store);

	guint real_slowdown = priv->exec_slowdown;
	if (real_slowdown > 0) {
		/* we don't honor the exec slowdown during meta data updates, it would complicate
		 * the code quite a lot (slowdown is done in the worker thread) and it's only
		 * for debug purposes */
		priv->exec_slowdown = 0;
	}

	gda_connection_increase_usage (cnc); /* USAGE ++ */

	if (context) {
		GdaMetaContext *lcontext;
		GSList *list;
		GSList *up_templates;
		GSList *dn_templates;
		GError *lerror = NULL;

		lcontext = _gda_meta_store_validate_context (store, context, error);
		if (!lcontext) {
			gda_connection_decrease_usage (cnc); /* USAGE -- */
			gda_connection_unlock ((GdaLockable*) cnc);
			priv->exec_slowdown = real_slowdown;
			return FALSE;
		}
		/* alter local context because "_tables" and "_views" always go together so only
		   "_tables" should be updated and providers should always update "_tables" and "_views"
		*/
		if (!strcmp (lcontext->table_name, "_views"))
			lcontext->table_name = "_tables";

		/* build context templates */
		up_templates = build_upstream_context_templates (store, lcontext, NULL, &lerror);
		if (!up_templates) {
			if (lerror) {
				gda_connection_decrease_usage (cnc); /* USAGE -- */
				gda_connection_unlock ((GdaLockable*) cnc);
				g_propagate_error (error, lerror);
				priv->exec_slowdown = real_slowdown;
				return FALSE;
			}
		}
		dn_templates = build_downstream_context_templates (store, lcontext, NULL, &lerror);
		if (!dn_templates) {
			if (lerror) {
				gda_connection_decrease_usage (cnc); /* USAGE -- */
				gda_connection_unlock ((GdaLockable*) cnc);
				g_propagate_error (error, lerror);
				priv->exec_slowdown = real_slowdown;
				return FALSE;
			}
		}

#ifdef GDA_DEBUG_META_STORE_UPDATE
		g_print ("\n*********** TEMPLATES:\n");
		for (list = up_templates; list; list = list->next) {
			g_print ("UP: ");
			meta_context_dump ((GdaMetaContext*) list->data);
		}
		g_print ("->: ");
		meta_context_dump (lcontext);
		for (list = dn_templates; list; list = list->next) {
			g_print ("DN: ");
			meta_context_dump ((GdaMetaContext*) list->data);
		}
#endif
					
		DownstreamCallbackData cbd;
		gboolean retval = TRUE;
		
		cbd.prov = priv->provider_obj;
		cbd.cnc = cnc;
		cbd.error = NULL;
		cbd.context_templates = g_slist_concat (g_slist_append (up_templates, lcontext), dn_templates);
		cbd.context_templates_hash = g_hash_table_new (g_str_hash, g_str_equal);
		retval = local_meta_update (priv->provider_obj, cnc,
					    lcontext, error);
		if (retval) {
			for (list = cbd.context_templates; list; list = list->next) {
				retval = local_meta_update (priv->provider_obj, cnc,
				         (GdaMetaContext*) (list->data), error);
				if (!retval) {
					break;
				}
			}
		}

		/* free the memory associated with each template */
		for (list = cbd.context_templates; list; list = list->next) {
			GdaMetaContext *c = (GdaMetaContext *) list->data;
			if (c == lcontext) {
				gint i;
				for (i = 0; i < c->size; i++) {
					g_free (c->column_names [i]);
					if (c->column_values [i])
						gda_value_free (c->column_values [i]);
				}
			}
			if (c->size > 0) {
				g_free (c->column_names);
				g_free (c->column_values);
			}
			g_free (c);
		}
		g_slist_free (cbd.context_templates);
		g_hash_table_destroy (cbd.context_templates_hash);

		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_connection_unlock ((GdaLockable*) cnc);
		priv->exec_slowdown = real_slowdown;
		return retval;
	}
	else {
		typedef struct {
			gchar *table_name;
			gchar *func_name;
			GdaServerProviderMetaType func_type;
		} RMeta;

		GdaMetaContext lcontext;
		gint nb = 0, i;
		RMeta rmeta[] = {
			{"_information_schema_catalog_name", "_info", GDA_SERVER_META__INFO},
			{"_builtin_data_types", "_btypes", GDA_SERVER_META__BTYPES},
			{"_udt", "_udt", GDA_SERVER_META__UDT},
			{"_udt_columns", "_udt_cols", GDA_SERVER_META__UDT_COLS},
			{"_enums", "_enums", GDA_SERVER_META__ENUMS},
			{"_domains", "_domains", GDA_SERVER_META__DOMAINS},
			{"_domain_constraints", "_constraints_dom", GDA_SERVER_META__CONSTRAINTS_DOM},
			{"_element_types", "_el_types", GDA_SERVER_META__EL_TYPES},
			{"_collations", "_collations", GDA_SERVER_META__COLLATIONS},
			{"_character_sets", "_character_sets", GDA_SERVER_META__CHARACTER_SETS},
			{"_schemata", "_schemata", GDA_SERVER_META__SCHEMATA},
			{"_tables_views", "_tables_views", GDA_SERVER_META__TABLES_VIEWS},
			{"_columns", "_columns", GDA_SERVER_META__COLUMNS},
			{"_view_column_usage", "_view_cols", GDA_SERVER_META__VIEW_COLS},
			{"_table_constraints", "_constraints_tab", GDA_SERVER_META__CONSTRAINTS_TAB},
			{"_referential_constraints", "_constraints_ref", GDA_SERVER_META__CONSTRAINTS_REF},
			{"_key_column_usage", "_key_columns", GDA_SERVER_META__KEY_COLUMNS},
			{"_check_column_usage", "_check_columns", GDA_SERVER_META__CHECK_COLUMNS},
			{"_triggers", "_triggers", GDA_SERVER_META__TRIGGERS},
			{"_routines", "_routines", GDA_SERVER_META__ROUTINES},
			{"_routine_columns", "_routine_col", GDA_SERVER_META__ROUTINE_COL},
			{"_parameters", "_routine_par", GDA_SERVER_META__ROUTINE_PAR},
			{"_table_indexes", "_indexes_tab", GDA_SERVER_META__INDEXES_TAB},
			{"_index_column_usage", "_index_cols", GDA_SERVER_META__INDEX_COLS}
		};
		GdaServerProvider *provider = priv->provider_obj;
		gboolean retval;

		if (! _gda_meta_store_begin_data_reset (store, error)) {
			gda_connection_decrease_usage (cnc); /* USAGE -- */
			gda_connection_unlock ((GdaLockable*) cnc);
			priv->exec_slowdown = real_slowdown;
			return FALSE;
		}

		lcontext.size = 0;
		nb = sizeof (rmeta) / sizeof (RMeta);
		for (i = 0; i < nb; i++) {
			/*g_print ("%s() %s(cnc=>%p store=>%p)\n", __FUNCTION__, rmeta [i].func_name, cnc, store);*/
			lcontext.table_name = rmeta [i].table_name;
			if (! _gda_server_provider_meta_0arg (provider, cnc, store, &lcontext,
							      rmeta[i].func_type, error)) {
				/*
				  g_print ("%s() %p CNC %p ERROR, prov=%p (%s)\n", __FUNCTION__, cnc,
				  gda_connection_get_provider (cnc),
				  gda_connection_get_provider_name (cnc));
				*/
				/*if (error && *error)
				  g_warning ("%s (Provider %s)\n", (*error)->message,
				  gda_connection_get_provider_name (cnc));
				*/
				
				WARN_META_UPDATE_FAILURE (FALSE, rmeta [i].func_name);
				goto onerror;
			}
		}
		retval = _gda_meta_store_finish_data_reset (store, error);
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_connection_unlock ((GdaLockable*) cnc);
		priv->exec_slowdown = real_slowdown;
		return retval;

	onerror:
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_connection_unlock ((GdaLockable*) cnc);
		_gda_meta_store_cancel_data_reset (store, NULL);
		priv->exec_slowdown = real_slowdown;
		return FALSE;
	}
}

/**
 * gda_connection_get_meta_store_data: (skip)
 * @cnc: a #GdaConnection object.
 * @meta_type: describes which data to get.
 * @error: a place to store errors, or %NULL
 * @nb_filters: the number of filters in the @... argument
 * @...: a list of (filter name (gchar *), filter value (GValue*)) pairs specifying
 * the filter to apply to the returned data model's contents (there must be @nb_filters pairs)
 *
 * Retrieves data stored in @cnc's associated #GdaMetaStore object. This method is useful
 * to easily get some information about the meta-data associated to @cnc, such as the list of
 * tables, views, and other database objects.
 *
 * Note: it's up to the caller to make sure the information contained within @cnc's associated #GdaMetaStore
 * is up to date using gda_connection_update_meta_store() (it can become outdated if the database's schema
 * is modified).
 *
 * For more information about the returned data model's attributes, or about the @meta_type and ... filter arguments,
 * see <link linkend="GdaConnectionMetaTypeHead">this description</link>.
 *
 * Also, when using filters involving data which are SQL identifiers, make sure each SQL identifier
 * is represented using the #GdaMetaStore convention, using gda_meta_store_sql_identifier_quote() or
 * gda_meta_store_sql_identifier_quote().
 *
 * See the <link linkend="information_schema:sql_identifiers">
 * meta data section about SQL identifiers</link> for more information, and the documentation about the
 * gda_sql_identifier_quote() function which will be most useful.
 * 
 * Returns: (transfer full): a #GdaDataModel containing the data required. The caller is responsible
 * for freeing the returned model using g_object_unref().
 */
GdaDataModel *
gda_connection_get_meta_store_data (GdaConnection *cnc,
				    GdaConnectionMetaType meta_type,
				    GError **error, gint nb_filters, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	GList* filters = NULL;
	GdaDataModel* model = NULL;
	gint i;
  
	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return NULL;
	}

	if (nb_filters > 0) {
		va_list ap;
		va_start (ap, nb_filters);
		for (i = 0; (i < nb_filters); i++) {
			GdaHolder *h;
			GValue *v;
			gchar* fname;
			GError *lerror = NULL;

			fname = va_arg (ap, gchar*);
			if (!fname)
				break;
			v = va_arg (ap, GValue*);
			if (!v || gda_value_is_null (v))
				continue;
			h = g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (v), "id", fname, NULL);
			filters = g_list_append (filters, h);
			if (!gda_holder_set_value (h, v, &lerror)) {
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_META_DATA_CONTEXT_ERROR,
				             _("While extracting data from Data Store: %s"),
										 lerror && lerror->message ? lerror->message : "No error message was set");
				g_clear_error (&lerror);
				va_end (ap);
				goto onerror;
			}
		}
		va_end (ap);
	}
	model = gda_connection_get_meta_store_data_v (cnc, meta_type, filters, error);

 onerror:
	g_list_free_full (filters, (GDestroyNotify) g_object_unref);

	return model;
}

/**
 * gda_connection_get_meta_store_data_v: (rename-to gda_connection_get_meta_store_data)
 * @cnc: a #GdaConnection object.
 * @meta_type: describes which data to get.
 * @error: a place to store errors, or %NULL
 * @filters: (element-type GdaHolder): a #GList of #GdaHolder objects
 *
 * see #gda_connection_get_meta_store_data
 * 
 * Returns: (transfer full): a #GdaDataModel containing the data required. The caller is responsible
 * for freeing the returned model using g_object_unref().
 */
GdaDataModel *
gda_connection_get_meta_store_data_v (GdaConnection *cnc, GdaConnectionMetaType meta_type,
				      GList* filters, GError **error)
{
	GdaMetaStore *store;
	GdaDataModel *model = NULL;
	GdaStatement *stmt = NULL;
	GdaSet *set = NULL;
	GdaSqlParser *parser;
	GList* node;
	gchar *sql = NULL;
	gboolean fail = FALSE;
	GError *lerror = NULL;
	gint nparams = 0;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, NULL);

	if (! gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     "%s", _("Connection is closed"));
		return FALSE;
	}

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	if (store == NULL) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			      _("Internal error: No meta store in connection"));
		return NULL;
	}
	set = GDA_SET (g_object_new (GDA_TYPE_SET, NULL));
	for (node = filters;
	     node;
	     node = node->next) {
		gda_set_add_holder (set, GDA_HOLDER (node->data));
		nparams++;
	}

	switch (meta_type) {
	case GDA_CONNECTION_META_NAMESPACES:
		sql = "SELECT schema_name, schema_owner, schema_internal FROM _schemata";
		if (nparams > 0) {
			sql = "SELECT schema_name, schema_owner, schema_internal FROM _schemata WHERE schema_name=##name::string";
			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter namespaces"));
				fail = TRUE;
			}
		}
		break;
	case GDA_CONNECTION_META_TYPES:
		sql = "SELECT short_type_name, gtype, comments, synonyms FROM _all_types WHERE NOT internal";
		if (nparams > 0) {
			sql = "SELECT short_type_name, gtype, comments, synonyms FROM _all_types WHERE NOT internal AND short_type_name=##name::string";
			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter data types"));
				fail = TRUE;
			}
		}
		break;
	case GDA_CONNECTION_META_TABLES:
		sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type LIKE '%TABLE%' AND table_short_name != table_full_name";
		if (nparams > 0) {
			sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type LIKE '%TABLE%' AND table_short_name != table_full_name AND table_short_name=##name::string";
			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter tables"));
				fail = TRUE;
			}
		}
		break;
	case GDA_CONNECTION_META_VIEWS:
		sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t WHERE t.table_short_name != t.table_full_name";
		if (nparams > 0) {
			sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t WHERE t.table_short_name != t.table_full_name AND table_short_name=##name::string";
			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter views"));
				fail = TRUE;
			}
		}
		break;
	case GDA_CONNECTION_META_FIELDS:
		if (nparams > 0) {
			sql = "SELECT c.column_name, c.data_type, c.gtype, c.numeric_precision, c.numeric_scale, c.is_nullable AS 'Nullable', c.column_default, c.extra FROM _columns as c NATURAL JOIN _tables as t WHERE t.table_short_name=##name::string ORDER BY c.ordinal_position";
			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter fields"));
				fail = TRUE;
			} else if (nparams > 1) {
				sql = "SELECT c.column_name, c.data_type, c.gtype, c.numeric_precision, c.numeric_scale, c.is_nullable AS 'Nullable', c.column_default, c.extra FROM _columns as c NATURAL JOIN _tables as t WHERE t.table_short_name=##name::string AND c.column_name = ##field_name::string ORDER BY c.ordinal_position";
				if (gda_set_get_holder (set, "field_name") == NULL) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
					    _("Parameter 'field_name' is required to filter fields"));
					fail = TRUE;
				}
			}
		} else {
			sql = "";
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
					    _("Parameters: 'name' and/or 'field_name' are required to filter fields"));
			fail = TRUE;
		}
		break;
	case GDA_CONNECTION_META_INDEXES:
		if (nparams > 0) {
			sql = "SELECT i.table_name, i.table_schema, i.index_name, d.column_name, d.ordinal_position, i.index_type FROM _table_indexes as i INNER JOIN _index_column_usage as d ON (d.table_catalog = i.table_catalog AND d.table_schema = i.table_schema AND d.table_name = i.table_name) INNER JOIN _tables t ON (t.table_catalog = i.table_catalog AND t.table_schema = i.table_schema AND t.table_name = i.table_name) WHERE t.table_short_name=##name::string";
			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter indexes"));
				fail = TRUE;
			} else if (nparams > 1) {
				sql = "SELECT i.table_name, i.table_schema, i.index_name, d.column_name, d.ordinal_position, i.index_type FROM _table_indexes as i INNER JOIN _index_column_usage as d ON (d.table_catalog = i.table_catalog AND d.table_schema = i.table_schema AND d.table_name = i.table_name) INNER JOIN _tables t ON (t.table_catalog = i.table_catalog AND t.table_schema = i.table_schema AND t.table_name = i.table_name) WHERE t.table_short_name=##name::string AND i.index_name=##index_name::string";
				if (gda_set_get_holder (set, "index_name") == NULL) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
					    _("Parameter 'index_name' is required to filter indexes"));
					fail = TRUE;
				}
			}
		} else {
			sql = "";
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
					    _("Parameters: 'name' and/or 'index_name' are required to filter indexes"));
			fail = TRUE;
		}

		if (nparams > 0) {

			if (gda_set_get_holder (set, "name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'name' is required to filter indexes"));
				fail = TRUE;
			} else if (gda_set_get_holder (set, "field_name") == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      _("Parameter 'index_name' is required to filter indexes"));
				fail = TRUE;
			}
		}
		break;
	}
	if (sql == NULL) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			      _("Invalid meta type to get data from store"));
		fail = TRUE;
	}
	if (fail) {
		return NULL;
	}
	parser = gda_sql_parser_new ();
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, error);
  g_object_unref (parser);
	if (stmt == NULL) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			      _("Internal error while creating statement to retrieve data: %s"),
			      lerror && lerror->message ? lerror->message : "No internal error reported; please file an issue to https://gitlab.gnome.org/GNOME/libgda/issues");
		g_clear_error (&lerror);
		return NULL;
	}

	/* execute statement to fetch the requested result from the meta store's connection
	 * REM: at a latter time the data model should be specific and update itself whenever
	 * the meta store is updated
	 */
	model = gda_connection_statement_execute_select (gda_meta_store_get_internal_connection (store), 
							 stmt, set, error);
	if (set)
		g_object_unref (set);

	return model;
}


/**
 * gda_connection_get_events:
 * @cnc: a #GdaConnection.
 *
 * Retrieves a list of the last errors occurred during the connection. The returned list is
 * chronologically ordered such as that the most recent event is the #GdaConnectionEvent of the first node.
 *
 * Warning: the @cnc object may change the list if connection events occur
 *
 * Returns: (transfer none) (element-type Gda.ConnectionEvent): a #GList of #GdaConnectionEvent objects (the list should not be modified)
 */
const GList *
gda_connection_get_events (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (priv->events_list)
		return priv->events_list;
	

	/* a new list of the GdaConnectionEvent objects is created, the
	 * ownership of each GdaConnectionEvent object is transfered to the list */
	GList *list = NULL;
	if (priv->events_array_full) {
		gint i;
		for (i = priv->events_array_next + 1; ; i++) {
			if (i == priv->events_array_size)
				i = 0;
			if (i == priv->events_array_next)
				break;
			GdaConnectionEvent *ev;
			ev = priv->events_array [i];
			priv->events_array [i] = NULL;
			g_assert (ev);
			list = g_list_prepend (list, ev);
		}
	}
	else {
		gint i;
		for (i = 0; i < priv->events_array_next; i++) {
			GdaConnectionEvent *ev;
			ev = priv->events_array [i];
			g_assert (ev);
			list = g_list_prepend (list, ev);
			priv->events_array [i] = NULL;
		}
	}
	priv->events_list = g_list_reverse (list);

	/* reset events */
	priv->events_array_full = FALSE;
	priv->events_array_next = 0;

	return priv->events_list;
}

/**
 * gda_connection_value_to_sql_string:
 * @cnc: a #GdaConnection object.
 * @from: #GValue to convert from
 *
 * Produces a fully quoted and escaped string from a GValue
 *
 * Returns: escaped and quoted value or NULL if not supported.
 */
gchar *
gda_connection_value_to_sql_string (GdaConnection *cnc, GValue *from)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_val_if_fail (priv->provider_obj, FALSE);

	/* execute the command on the provider */
	return gda_server_provider_value_to_sql_string (priv->provider_obj, cnc, from);
}

/**
 * gda_connection_internal_transaction_started:
 * @cnc: a #GdaConnection
 * @parent_trans: (nullable): name of the parent transaction, or %NULL
 * @trans_name: transaction's name, or %NULL
 * @isol_level: isolation level.
 *
 * Internal functions to be called by database providers when a transaction has been started
 * to keep track of the transaction status of the connection.
 *
 * Note: this function should not be called if gda_connection_internal_statement_executed()
 * has already been called because a statement's execution was necessary to perform
 * the action.
 */
void
gda_connection_internal_transaction_started (GdaConnection *cnc, const gchar *parent_trans, const gchar *trans_name, 
					     GdaTransactionIsolation isol_level)
{
	GdaTransactionStatus *parent, *st;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	st = gda_transaction_status_new (trans_name);
	gda_transaction_status_set_isolation_level (st, isol_level);

	gda_connection_lock ((GdaLockable*) cnc);

	parent = gda_transaction_status_find (priv->trans_status, parent_trans, NULL);
	if (!parent)
		priv->trans_status = st;
	else {
		gda_transaction_status_add_event_sub (parent, st);
		g_object_unref (st);
	}
#ifdef GDA_DEBUG_signal
        g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif

#ifdef GDA_DEBUG_NO
	if (priv->trans_status)
		gda_transaction_status_dump (priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_transaction_rolledback:
 * @cnc: a #GdaConnection
 * @trans_name: (nullable): transaction's name, or %NULL
 *
 * Internal functions to be called by database providers when a transaction has been rolled
 * back to keep track of the transaction status of the connection
 *
 * Note: this function should not be called if gda_connection_internal_statement_executed()
 * has already been called because a statement's execution was necessary to perform
 * the action.
 */
void 
gda_connection_internal_transaction_rolledback (GdaConnection *cnc, const gchar *trans_name)
{
	GdaTransactionStatus *st = NULL;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);

	if (priv->trans_status)
		st = gda_transaction_status_find (priv->trans_status, trans_name, &ev);
	if (st) {
		if (ev) {
			/* there is a parent transaction */
			gda_transaction_status_free_events (ev->trans, ev, TRUE);
		}
		else {
			/* no parent transaction */
			g_object_unref (priv->trans_status);
			priv->trans_status = NULL;
		}
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else {
		g_warning (_("Connection transaction status tracking: no transaction exists for %s"), "ROLLBACK");
	}
#ifdef GDA_DEBUG_NO
	if (priv->trans_status)
		gda_transaction_status_dump (priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_transaction_committed:
 * @cnc: a #GdaConnection
 * @trans_name: (nullable): transaction's name, or %NULL
 *
 * Internal functions to be called by database providers when a transaction has been committed
 * to keep track of the transaction status of the connection
 *
 * Note: this function should not be called if gda_connection_internal_statement_executed()
 * has already been called because a statement's execution was necessary to perform
 * the action.
 */
void 
gda_connection_internal_transaction_committed (GdaConnection *cnc, const gchar *trans_name)
{
	GdaTransactionStatus *st = NULL;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);

	if (priv->trans_status)
		st = gda_transaction_status_find (priv->trans_status, trans_name, &ev);
	if (st) {
		if (ev) {
			/* there is a parent transaction */
			gda_transaction_status_free_events (ev->trans, ev, TRUE);
		}
		else {
			/* no parent transaction */
			g_object_unref (priv->trans_status);
			priv->trans_status = NULL;
		}
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else {
		g_warning (_("Connection transaction status tracking: no transaction exists for %s"), "COMMIT");
	}
#ifdef GDA_DEBUG_NO
	if (priv->trans_status)
		gda_transaction_status_dump (priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_savepoint_added:
 * @cnc: a #GdaConnection
 * @parent_trans: (nullable): name of the parent transaction, or %NULL
 * @svp_name: savepoint's name, or %NULL
 *
 * Internal functions to be called by database providers when a savepoint has been added
 * to keep track of the transaction status of the connection
 *
 * Note: this function should not be called if gda_connection_internal_statement_executed()
 * has already been called because a statement's execution was necessary to perform
 * the action.
 */
void
gda_connection_internal_savepoint_added (GdaConnection *cnc, const gchar *parent_trans, const gchar *svp_name)
{
	GdaTransactionStatus *st;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);

	st = gda_transaction_status_find (priv->trans_status, parent_trans, NULL);
	if (st) {
		gda_transaction_status_add_event_svp (st, svp_name);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else {
		g_warning (_("Connection transaction status tracking: no transaction exists for %s"), "ADD SAVEPOINT");
	}
#ifdef GDA_DEBUG_NO
	if (priv->trans_status)
		gda_transaction_status_dump (priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_savepoint_rolledback:
 * @cnc: a #GdaConnection
 * @svp_name: (nullable): savepoint's name, or %NULL
 *
 * Internal functions to be called by database providers when a savepoint has been rolled back
 * to keep track of the transaction status of the connection
 *
 * Note: this function should not be called if gda_connection_internal_statement_executed()
 * has already been called because a statement's execution was necessary to perform
 * the action.
 */
void
gda_connection_internal_savepoint_rolledback (GdaConnection *cnc, const gchar *svp_name)
{
	GdaTransactionStatus *st;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);

	st = gda_transaction_status_find (priv->trans_status, svp_name, &ev);
	if (st) {
		gda_transaction_status_free_events (st, ev, TRUE);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else {
		g_warning (_("Connection transaction status tracking: no transaction exists for %s"), "ROLLBACK SAVEPOINT");
	}
#ifdef GDA_DEBUG_NO
	if (priv->trans_status)
		gda_transaction_status_dump (priv->trans_status, 5);
#endif	

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_savepoint_removed:
 * @cnc: a #GdaConnection
 * @svp_name: (nullable): savepoint's name, or %NULL
 *
 * Internal functions to be called by database providers when a savepoint has been removed
 * to keep track of the transaction status of the connection
 *
 * Note: this function should not be called if gda_connection_internal_statement_executed()
 * has already been called because a statement's execution was necessary to perform
 * the action.
 */
void
gda_connection_internal_savepoint_removed (GdaConnection *cnc, const gchar *svp_name)
{
	GdaTransactionStatus *st;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);

	st = gda_transaction_status_find (priv->trans_status, svp_name, &ev);
	if (st) {
		gda_transaction_status_free_events (st, ev, FALSE);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else {
		g_warning (_("Connection transaction status tracking: no transaction exists for %s"), "REMOVE SAVEPOINT");
	}
#ifdef GDA_DEBUG_NO
	if (priv->trans_status)
		gda_transaction_status_dump (priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_statement_executed:
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement which has been executed
 * @params: (nullable): execution's parameters
 * @error: (transfer none): a #GdaConnectionEvent if the execution failed, or %NULL
 *
 * Internal functions to be called by database providers when a statement has been executed
 * to keep track of the transaction status of the connection
 */
void 
gda_connection_internal_statement_executed (GdaConnection *cnc, GdaStatement *stmt,
					    G_GNUC_UNUSED GdaSet *params, GdaConnectionEvent *error)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (!error || (error && (gda_connection_event_get_event_type (error) != GDA_CONNECTION_EVENT_ERROR))) {
		const GdaSqlStatement *sqlst;
		GdaSqlStatementTransaction *trans;
		sqlst = _gda_statement_get_internal_struct (stmt);
		trans = (GdaSqlStatementTransaction*) sqlst->contents; /* warning: this may be inaccurate if stmt_type is not
									  a transaction type, but the compiler does not care */

		switch (sqlst->stmt_type) {
		case GDA_SQL_STATEMENT_BEGIN:
			gda_connection_internal_transaction_started (cnc, NULL, trans->trans_name, 
								     trans->isolation_level);
			break;
		case GDA_SQL_STATEMENT_ROLLBACK:
			gda_connection_internal_transaction_rolledback (cnc, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_COMMIT:
			gda_connection_internal_transaction_committed (cnc, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_SAVEPOINT:
			gda_connection_internal_savepoint_added (cnc, NULL, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT:
			gda_connection_internal_savepoint_rolledback (cnc, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_DELETE_SAVEPOINT:
			gda_connection_internal_savepoint_removed (cnc, trans->trans_name);
			break;
		default: {
			GdaTransactionStatus *st = NULL;
			
			gda_connection_lock ((GdaLockable*) cnc);

			if (priv->trans_status)
				st = gda_transaction_status_find_current (priv->trans_status, NULL, FALSE);
			if (st) {
				if (sqlst->sql)
					gda_transaction_status_add_event_sql (st, sqlst->sql, error);
				else {
					gchar *sql;
					sql = gda_statement_to_sql_extended (stmt, cnc, NULL, 
									     GDA_STATEMENT_SQL_PARAMS_SHORT,
									     NULL, NULL);
					gda_transaction_status_add_event_sql (st, sql, error);
					g_free (sql);
				}
			}
#ifdef GDA_DEBUG_signal
			g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
			g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
			g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
#ifdef GDA_DEBUG_NO
			if (priv->trans_status)
				gda_transaction_status_dump (priv->trans_status, 5);
#endif
			gda_connection_unlock ((GdaLockable*) cnc);
			break;
		}
		}
	}
}

/**
 * gda_connection_internal_change_transaction_state:
 * @cnc: a #GdaConnection
 * @newstate: the new state
 *
 * Internal function to be called by database providers to force a transaction status
 * change.
 */
void
gda_connection_internal_change_transaction_state (GdaConnection *cnc,
						  GdaTransactionStatusState newstate)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);

	g_return_if_fail (priv->trans_status);

	if (gda_transaction_status_get_state (priv->trans_status) == newstate)
		return;

	gda_transaction_status_set_state (priv->trans_status, newstate);
#ifdef GDA_DEBUG_signal
	g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_reset_transaction_status:
 * @cnc: a #GdaConnection
 *
 * Internal function to be called by database providers to reset the transaction status.
 */
void
gda_connection_internal_reset_transaction_status (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);
	if (priv->trans_status) {
		g_object_unref (priv->trans_status);
		priv->trans_status = NULL;
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	gda_connection_unlock ((GdaLockable*) cnc);
}

/*
 * Prepared statements handling
 */

static void prepared_stmts_stmt_reset_cb (GdaStatement *gda_stmt, GdaConnection *cnc);

static void
prepared_stmts_stmt_reset_cb (GdaStatement *gda_stmt, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	gda_connection_lock ((GdaLockable*) cnc);
	g_object_ref (gda_stmt);

	g_signal_handlers_disconnect_by_func (gda_stmt, G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);
	g_assert (priv->prepared_stmts);
	g_hash_table_remove (priv->prepared_stmts, gda_stmt);
	g_object_unref (gda_stmt);

	gda_connection_unlock ((GdaLockable*) cnc);
}

typedef struct {
	GdaStatement   *statement;
	GdaPStmt       *prepared_stmt;
} PreparedStatementRef;

PreparedStatementRef*
_gda_prepared_estatement_new (GdaStatement *stmt, GdaPStmt *pstmt) {
	PreparedStatementRef *nps = g_new0(PreparedStatementRef,1);
	nps->statement = g_object_ref (stmt);
	nps->prepared_stmt = g_object_ref (pstmt);
	return nps;
}

void
_gda_prepared_estatement_free (PreparedStatementRef *ps) {
	g_object_unref (ps->statement);
	g_object_unref (ps->prepared_stmt);
  g_free (ps);
}

/**
 * gda_connection_add_prepared_statement:
 * @cnc: a #GdaConnection object
 * @gda_stmt: a #GdaStatement object
 * @prepared_stmt: a prepared statement object (as a #GdaPStmt object, or more likely a descendant)
 *
 * Declares that @prepared_stmt is a prepared statement object associated to @gda_stmt within the connection
 * (meaning the connection increments the reference counter of @prepared_stmt).
 *
 * If @gda_stmt changes or is destroyed, the the association will be lost and the connection will lose the
 * reference it has on @prepared_stmt.
 */
void 
gda_connection_add_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt, GdaPStmt *prepared_stmt)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_STATEMENT (gda_stmt));
	g_return_if_fail (GDA_IS_PSTMT (prepared_stmt));
	g_object_ref (prepared_stmt);
	g_object_ref (gda_stmt);

	gda_connection_lock ((GdaLockable*) cnc);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (!priv->prepared_stmts)
		priv->prepared_stmts = g_hash_table_new_full (g_direct_hash, g_direct_equal,
								   NULL, (GDestroyNotify) _gda_prepared_estatement_free);
	g_hash_table_remove (priv->prepared_stmts, gda_stmt);
	PreparedStatementRef *ref = _gda_prepared_estatement_new (gda_stmt, prepared_stmt);
	g_hash_table_insert (priv->prepared_stmts, gda_stmt, ref);
	
	g_signal_connect (G_OBJECT (gda_stmt), "reset",
			  G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);

	gda_connection_unlock ((GdaLockable*) cnc);
	g_object_unref (prepared_stmt);
	g_object_unref (gda_stmt);
}

/**
 * gda_connection_get_prepared_statement:
 * @cnc: a #GdaConnection object
 * @gda_stmt: a #GdaStatement object
 *
 * Retrieves a pointer to an object representing a prepared statement for @gda_stmt within @cnc. The
 * association must have been done using gda_connection_add_prepared_statement().
 *
 * Returns: (transfer none): the prepared statement, or %NULL if no association exists
 */
GdaPStmt *
gda_connection_get_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt)
{
	GdaPStmt *retval = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	gda_connection_lock ((GdaLockable*) cnc);
	if (priv->prepared_stmts) {
		PreparedStatementRef *ref = g_hash_table_lookup (priv->prepared_stmts, gda_stmt);
		if (ref)
			retval = ref->prepared_stmt;
	}
	gda_connection_unlock ((GdaLockable*) cnc);

	return retval;
}

/**
 * gda_connection_del_prepared_statement:
 * @cnc: a #GdaConnection object
 * @gda_stmt: a #GdaStatement object
 *
 * Removes any prepared statement associated to @gda_stmt in @cnc: this undoes what
 * gda_connection_add_prepared_statement() does.
 */
void
gda_connection_del_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt)
{
	g_return_if_fail (cnc != NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_object_ref (gda_stmt);
	g_hash_table_remove (priv->prepared_stmts, gda_stmt);
	g_object_unref (gda_stmt);
	gda_connection_unlock ((GdaLockable*) cnc);
}

/*
 * Provider's specific connection data management
 */

/**
 * gda_connection_internal_set_provider_data:
 * @cnc: a #GdaConnection object
 * @data: a #GdaServerProviderConnectionData, which can be extended as needed by the provider for which @cnc is opened
 * @destroy_func: function to call when the connection closes and @data needs to be destroyed
 *
 * Note: calling this function more than once will not make it call @destroy_func on any previously
 * set opaque @data, you'll have to do it yourself.
 *
 * Note: @destroy_func, needs to free the memory associated to @data, if necessary.
 */
void 
gda_connection_internal_set_provider_data (GdaConnection *cnc, GdaServerProviderConnectionData *data, GDestroyNotify destroy_func)
{

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail ((data && destroy_func) || (!data && !destroy_func));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (priv->provider_data && data) {
		g_warning ("Changing the provider's data is not allowed during the lifetime of a connection");
		return;
	}

	if (!data)
		gda_connection_lock ((GdaLockable*) cnc);

	priv->provider_data = data;
	if (data)
		data->provider_data_destroy_func = destroy_func;

	if (!data)
		gda_connection_unlock ((GdaLockable*) cnc);
}

void
_gda_connection_internal_set_worker_thread (GdaConnection *cnc, GThread *thread)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	g_mutex_lock (&global_mutex);
	if (priv->worker_thread && thread)
		g_warning ("Trying to overwriting connection's associated internal thread");
	else
		priv->worker_thread = thread;
	g_mutex_unlock (&global_mutex);
}

/**
 * gda_connection_internal_get_worker:
 * @data: (nullable): a #GdaServerProviderConnectionData, or %NULL
 *
 * Retreive a pointer to the #GdaWorker used internally by the connection. This function is reserved to
 * database provider's implementation and should not be used otherwise.
 *
 * Returns: (transfer none): the #GdaWorker, or %NULL
 */
GdaWorker *
gda_connection_internal_get_worker (GdaServerProviderConnectionData *data)
{
	if (data)
		return data->worker;
	else
		return NULL;
}

/**
 * gda_connection_internal_get_provider_data_error:
 * @cnc: a #GdaConnection object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Get the opaque pointer previously set using gda_connection_internal_set_provider_data().
 * If it's not set, then add a connection event and returns %NULL
 *
 * Returns: (nullable): the pointer to the opaque structure set using gda_connection_internal_set_provider_data(), or %NULL
 *
 * Since: 5.0.2
 */
GdaServerProviderConnectionData *
gda_connection_internal_get_provider_data_error (GdaConnection *cnc, GError **error)
{
	gpointer retval;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	retval = priv->provider_data;
	if (!retval)
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
                             _("Connection is closed"));
	return retval;
}

/**
 * gda_connection_get_meta_store:
 * @cnc: a #GdaConnection object
 *
 * Get or initializes the #GdaMetaStore associated to @cnc
 *
 * Returns: (transfer none): a #GdaMetaStore object
 */
GdaMetaStore *
gda_connection_get_meta_store (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	gda_connection_lock ((GdaLockable*) cnc);
	if (!priv->meta_store) {
		priv->meta_store = gda_meta_store_new (NULL);
		GdaConnection *scnc;
		scnc = gda_meta_store_get_internal_connection (priv->meta_store);
		if ((scnc != cnc) &&
		    gda_connection_get_main_context (cnc, NULL) &&
		    ! gda_connection_get_main_context (scnc, NULL))
			gda_connection_set_main_context (scnc, NULL, gda_connection_get_main_context (cnc, NULL));
	}
	gda_connection_unlock ((GdaLockable*) cnc);

	return priv->meta_store;
}

/* Note about the GdaConnection's locking mechanism:
 * When a thread wants to lock a #GdaConnection:
 *   - if it's the GdaWorker's thread then nothing needs to be done, and returns immediately
 *   - otherwise try to lock the GRecMutex, and if Ok, then we are done.
 *   - if the try_lock() did not work:
 *     - if there is no GMainContext associated to the current thread, then block on mutex lock
 *     - otherwise, we create a new main loop for that GMainContext and an ITSignaler source
 *          which will receive a notification from the thread which currently
 *          locks the GRecMutex right after it has released it.
 *
 *  The ITSignaler is appended to the @lock_its_list list, and removed once the lock has been
 *  acquired by the thread
 */
static GSList *lock_its_list = NULL;

typedef struct {
	GdaLockable *lockable;
	GMainLoop   *loop;
} TryLockData;

static gboolean
itsignaler_trylock (TryLockData *data)
{
	if (gda_connection_trylock (data->lockable)) {
		g_main_loop_quit (data->loop);
		return G_SOURCE_REMOVE;
	}
	else
		return G_SOURCE_CONTINUE;
}

/*
 * To avoid the thread being blocked (possibly forever if the single thread which can use the
 * connection is not the current thead), then it is possible to use gda_connection_trylock() instead.
 */
static void
gda_connection_lock (GdaLockable *lockable)
{
	GdaConnection *cnc = (GdaConnection *) lockable;
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (priv->worker_thread == g_thread_self ()) {
		/* the sitation here is that the connection _has been_ locked by the
		 * calling thread of the GdaWorker, and as we are in the worker thread
		 * of the GdaWorker, we don't need to lock it again: it would be useless because
		 * by desing the connection object may be modified but only by the provider code, which
		 * is Ok, and because it would require a very complex mechanism to "transfer the lock"
		 * from one thread to the other */
		return;
	}

	GMainContext *context;
	context = gda_connection_get_main_context (cnc, NULL);
	if (context) {
		g_mutex_lock (&global_mutex);
		if (gda_connection_trylock (lockable)) {
			g_mutex_unlock (&global_mutex);
			return;
		}

		/* create ITSignaler and add it to the list */
		ITSignaler *its;
		its = itsignaler_new ();
		lock_its_list = g_slist_append (lock_its_list, its);
		/*g_print ("++ ITSigaler_len is %u\n", g_slist_length (lock_its_list));*/
		g_mutex_unlock (&global_mutex);

		/* create main loop to must process events from @context */
		GMainLoop *loop;
		loop = g_main_loop_new (context, FALSE);

		TryLockData lockdata;
		lockdata.lockable = lockable;
		lockdata.loop = loop;

		GSource *itsource;
		itsource = itsignaler_create_source (its);
		g_source_attach (itsource, context);
		g_source_set_callback (itsource, G_SOURCE_FUNC (itsignaler_trylock), &lockdata, NULL);
		g_source_unref (itsource);

		g_main_loop_run (loop);
		g_main_loop_unref (loop);

		/* get rid of @its */
		g_mutex_lock (&global_mutex);
		lock_its_list = g_slist_remove (lock_its_list, its);
		/*g_print ("-- ITSigaler_len is %u\n", g_slist_length (lock_its_list));*/
		g_mutex_unlock (&global_mutex);
		itsignaler_unref (its);
	}
	else
		g_rec_mutex_lock (& priv->rmutex);
}

/*
 * Tries to lock @cnc for the exclusive usage of the current thread, as gda_connection_lock(), except
 * that if it can't, then the calling thread is not locked by it simply returns FALSE.
 *
 * Returns: TRUE if successfully locked, or FALSE if lock could not be acquired
 */
static gboolean
gda_connection_trylock (GdaLockable *lockable)
{
	GdaConnection *cnc = (GdaConnection *) lockable;
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (priv->worker_thread == g_thread_self ()) {
		/* See gda_connection_lock() for explanations */
		return TRUE;
	}

	return g_rec_mutex_trylock (& priv->rmutex);
}

/*
 * Unlocks @cnc's usage. Any other thread blocked (after having called gda_connection_lock()) gets
 * the opportunity to lock the connection.
 */
static void
gda_connection_unlock  (GdaLockable *lockable)
{
	GdaConnection *cnc = (GdaConnection *) lockable;
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);

	if (priv->worker_thread == g_thread_self ()) {
		/* See gda_connection_lock() for explanations */
		return;
	}

	/* help other threads locking the connection */
	g_rec_mutex_unlock (& priv->rmutex);
	g_mutex_lock (&global_mutex);
	if (lock_its_list) {
		ITSignaler *its;
		its = (ITSignaler*) lock_its_list->data;
		itsignaler_push_notification (its, (gpointer) 0x01, NULL); /* "wake up" the waiting thread */
	}
	g_mutex_unlock (&global_mutex);
}

/*
 * REM: if @for_ident is %FALSE, then the keywords are converted to upper case
 */
static gchar *
get_next_word (gchar *str, gboolean for_ident, gchar **out_next)
{
	if (!str) {
		*out_next = NULL;
		return NULL;
	}

	gchar *start;
	gchar *ptr;
	gboolean inquotes = FALSE;
	for (ptr = str; *ptr; ptr++) {
		if ((*ptr == ' ') || (*ptr == '\n') || (*ptr == '\t') || (*ptr == '\r'))
			continue;
		break;
	}
	start = ptr;
	/*g_print ("%s ([%s]) => [%s]", __FUNCTION__, str, start);*/
	for (; *ptr; ptr++) {
		if ((*ptr >= 'a') && (*ptr <= 'z')) {
			if (! for_ident)
				*ptr += 'A' - 'a';
			continue;
		}
		else if ((*ptr >= 'A') && (*ptr <= 'Z'))
			continue;
		else if ((*ptr >= '0') && (*ptr <= '9'))
			continue;
		else if (*ptr == '_')
			continue;
		else if (for_ident) {
			if ((*ptr == '"') || (*ptr == '\'') || (*ptr == '`')) {
				if (inquotes) {
					*ptr = '"';
					inquotes = FALSE;
					ptr++;
					break;
				}
				else {
					*ptr = '"';
					inquotes = TRUE;
				}
				continue;
			}
		}
		else if (inquotes)
			continue;
		break;
	}
	if (ptr != start) {
		if (*ptr) {
			*ptr = 0;
			*out_next = ptr + 1;
		}
		else
			*out_next = ptr;
	}
	else
		*out_next = NULL;
	/*g_print (" -- [%s]\n", *out_next);*/
	return start;
}


/*
 * the contexts in returned list have:
 *  - the @table_name attribute as a static string
 *  - the @column_names[x] as a static string, not the @column_names array itself which has to be freed
 *
 * Returns: a new list of new #GdaMetaContext
 */
static GSList *
meta_data_context_from_statement (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	gboolean ignore_create_drop = FALSE;
	if (GDA_IS_VCONNECTION_DATA_MODEL (cnc))
		/* meta data is updated when the virtual connection emits the
		 * "vtable-created" or "vtable-dropped" signals
		 */
		ignore_create_drop = TRUE;

	GdaMetaContext *context = NULL;
	gchar *sql, *current, *next;
	sql = gda_statement_to_sql (stmt, params, NULL);
	if (!sql)
		return NULL;

	GSList *clist = NULL;
	current = get_next_word (sql, FALSE, &next);
	if (!current)
		goto out;

	if (!strcmp (current, "ALTER") ||
	    (!ignore_create_drop && (!strcmp (current, "CREATE") || !strcmp (current, "DROP")))) {
		const gchar *tname = NULL;
		const gchar *opname;
		opname = current;
		current = get_next_word (next, FALSE, &next);
		if (current && (!strcmp (current, "TABLE") || !strcmp (current, "VIEW"))) {
			tname = get_next_word (next, TRUE, &next);
			if ((! strcmp (opname, "CREATE") || !strcmp (opname, "DROP"))
			    && !g_ascii_strcasecmp (tname, "IF")) {
				gchar *tmpnext;
				tmpnext = next;
				if (! strcmp (opname, "CREATE")) {
					const gchar *s1, *s2;
					s1 = get_next_word (tmpnext, FALSE, &tmpnext);
					s2 = get_next_word (tmpnext, FALSE, &tmpnext);
					if (! strcmp (s1, "NOT") &&
					    ! strcmp (s2, "EXISTS")) {
						next = tmpnext;
						tname = get_next_word (next, TRUE, &next);
					}
				}
				else {
					const gchar *s1;
					s1 = get_next_word (tmpnext, FALSE, &tmpnext);
					if (! strcmp (s1, "EXISTS")) {
						next = tmpnext;
						tname = get_next_word (next, TRUE, &next);
					}
				}
			}
		}
		if (tname) {
			gchar *tmp;
			/*g_print ("CONTEXT: update for table [%s]\n", tname);*/
			context = g_new0 (GdaMetaContext, 1);
			context->table_name = "_tables";
			context->size = 1;
			context->column_names = g_new0 (gchar *, 1);
			context->column_names[0] = "table_name";
			context->column_values = g_new0 (GValue *, 1);
			tmp = gda_sql_identifier_quote (tname, cnc, priv->provider_obj,
							TRUE,
							priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
			g_value_take_string ((context->column_values[0] = gda_value_new (G_TYPE_STRING)),
					     tmp);
			clist = g_slist_prepend (clist, context);

			/* seek RENAME TO */
			current = get_next_word (next, FALSE, &next);
			if (!current || strcmp (current, "RENAME"))
				goto out;
			current = get_next_word (next, FALSE, &next);
			if (!current || strcmp (current, "TO"))
				goto out;
			tname = get_next_word (next, TRUE, &next);
			if (tname) {
				gchar *tmp;
				/*g_print ("CONTEXT: update for table [%s]\n", tname);*/
				context = g_new0 (GdaMetaContext, 1);
				context->table_name = "_tables";
				context->size = 1;
				context->column_names = g_new0 (gchar *, 1);
				context->column_names[0] = "table_name";
				context->column_values = g_new0 (GValue *, 1);
				tmp = gda_sql_identifier_quote (tname, cnc, priv->provider_obj,
								TRUE,
								priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
				g_value_take_string ((context->column_values[0] = gda_value_new (G_TYPE_STRING)),
						     tmp);
				clist = g_slist_prepend (clist, context);
			}
		}
	}

 out:
	g_free (sql);
	return clist;
}

/*
 * update_meta_store_after_statement_exec
 *
 * Updates the meta store associated to @cnc if it exists and if @cnc has the
 * GDA_CONNECTION_OPTIONS_AUTO_META_DATA flag.
 */
static void
update_meta_store_after_statement_exec (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (! priv->meta_store ||
	    ! (priv->options & GDA_CONNECTION_OPTIONS_AUTO_META_DATA))
		return;

	GdaSqlStatementType type;
	type = gda_statement_get_statement_type (stmt);
	if (type == GDA_SQL_STATEMENT_BEGIN) {
		/* initialize priv->trans_meta_context if meta store's connection is not @cnc */
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (priv->meta_store);
		if (cnc != mscnc) {
			g_assert (! priv->trans_meta_context);
			priv->trans_meta_context = g_array_new (FALSE, FALSE, sizeof (GdaMetaContext*));
		}
		return;
	}
	else if (type == GDA_SQL_STATEMENT_ROLLBACK) {
		/* re-run all the meta store updates started since the BEGIN */
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (priv->meta_store);
		if (cnc != mscnc) {
			gsize i;
			g_assert (priv->trans_meta_context);
			for (i = 0; i < priv->trans_meta_context->len; i++) {
				GdaMetaContext *context;
				GError *lerror = NULL;
				context = g_array_index (priv->trans_meta_context, GdaMetaContext*, i);
				if (! gda_connection_update_meta_store (cnc, context, &lerror))
					add_connection_event_from_error (cnc, &lerror);
				auto_update_meta_context_free (context);
			}
			g_array_free (priv->trans_meta_context, TRUE);
			priv->trans_meta_context = NULL;
		}
		return;
	}
	else if (type == GDA_SQL_STATEMENT_COMMIT) {
		/* get rid of the meta store updates */
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (priv->meta_store);
		if (cnc != mscnc) {
			gsize i;
			g_assert (priv->trans_meta_context);
			for (i = 0; i < priv->trans_meta_context->len; i++) {
				GdaMetaContext *context;
				context = g_array_index (priv->trans_meta_context, GdaMetaContext*, i);
				auto_update_meta_context_free (context);
			}
			g_array_free (priv->trans_meta_context, TRUE);
			priv->trans_meta_context = NULL;
		}
		return;
	}
	else if (type != GDA_SQL_STATEMENT_UNKNOWN)
		return;

	GSList *clist, *list;
	clist = meta_data_context_from_statement (cnc, stmt, params);
	for (list = clist; list; list = list->next) {
		GdaMetaContext *context;
		context = (GdaMetaContext*) list->data;
		if (context) {
			GError *lerror = NULL;
			if (! gda_connection_update_meta_store (cnc, context, &lerror))
				add_connection_event_from_error (cnc, &lerror);
			
			if (priv->trans_meta_context)
				g_array_prepend_val (priv->trans_meta_context, context);
			else
				auto_update_meta_context_free (context);
		}
	}
	g_slist_free (clist);
}

void
_gda_connection_signal_meta_table_update (GdaConnection *cnc, const gchar *table_name)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaConnectionPrivate *priv = gda_connection_get_instance_private (cnc);
	if (! priv->meta_store ||
	    ! (priv->options & GDA_CONNECTION_OPTIONS_AUTO_META_DATA))
		return;

	GdaMetaContext *context;
	gchar **split;
	gchar *tmp;
	/*g_print ("CONTEXT: update for table [%s]\n", table_name);*/
	split = gda_sql_identifier_split (table_name);
	if (!split)
		return;
	if (!split [0]) {
		g_strfreev (split);
		return;
	}

	context = g_new0 (GdaMetaContext, 1);
	context->table_name = "_tables";

	if (split [1]) {
		context->size = 2;
		context->column_names = g_new0 (gchar *, context->size);
		context->column_names[0] = "table_schema";
		context->column_names[1] = "table_name";
		context->column_values = g_new0 (GValue *, context->size);
		tmp = gda_sql_identifier_quote (split[0], cnc, priv->provider_obj,
						TRUE,
						priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
		g_value_take_string ((context->column_values[0] = gda_value_new (G_TYPE_STRING)),
				     tmp);
		tmp = gda_sql_identifier_quote (split[1], cnc, priv->provider_obj,
						TRUE,
						priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
		g_value_take_string ((context->column_values[1] = gda_value_new (G_TYPE_STRING)),
				     tmp);
	}
	else {
		context->size = 1;
		context->column_names = g_new0 (gchar *, context->size);
		context->column_names[0] = "table_name";
		context->column_values = g_new0 (GValue *, context->size);
		tmp = gda_sql_identifier_quote (split[0], cnc, priv->provider_obj,
						TRUE,
						priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
		g_value_take_string ((context->column_values[0] = gda_value_new (G_TYPE_STRING)),
				     tmp);
	}

	GError *lerror = NULL;
	if (! gda_connection_update_meta_store (cnc, context, &lerror))
		add_connection_event_from_error (cnc, &lerror);
	
	if (priv->trans_meta_context)
		g_array_prepend_val (priv->trans_meta_context, context);
	else
		auto_update_meta_context_free (context);

	g_strfreev (split);
}

/*
 * Free @context which must have been created by meta_data_context_from_statement()
 */
static void
auto_update_meta_context_free (GdaMetaContext *context)
{
	gint i;
	context->table_name = NULL; /* don't free */
	g_free (context->column_names); /* don't free the strings in the array */
	for (i = 0; i < context->size; i++)
		gda_value_free (context->column_values[i]);
	g_free (context->column_values);
	g_free (context);
}


/*
 * _gda_connection_get_table_virtual_name
 * @table_name: a non %NULL string
 *
 * Returns: a new string.
 */
gchar *
_gda_connection_compute_table_virtual_name (GdaConnection *cnc, const gchar *table_name)
{
	gchar **array;

	g_assert (table_name && *table_name);
	array = gda_sql_identifier_split (table_name);
	if (array) {
		GString *string = NULL;
		gint i;
		gchar *tmp;
		for (i = 0; ; i++) {
			if (array [i]) {
				tmp = gda_sql_identifier_quote (array[i], cnc, NULL, TRUE, FALSE);
				if (string) {
					g_string_append_c (string, '.');
					g_string_append (string, tmp);
				}
				else
					string = g_string_new (tmp);
			}
			else
				break;
		}
		g_strfreev (array);
		return g_string_free (string, FALSE);
	}
	else {
		/*
		** If X is a character that can be used in an identifier then
		** IdChar(X) will be true.  Otherwise it is false.
		**  
		** For ASCII, any character with the high-order bit set is
		** allowed in an identifier.  For 7-bit characters, 
		** sqlite3IsIdChar[X] must be 1. 
		*/
		static const char AsciiIdChar[] = {
			/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
			0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 2x */
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
			0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
			0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
		};
#define IdChar(C) (((c=C)&0x80)!=0 || (c>0x1f && AsciiIdChar[c-0x20]))
		gchar *tmp, *ptr, c;
		tmp = g_strdup (table_name);

		/* first try removing some double quotes only */
		for (ptr = tmp; *ptr; ptr++) {
			if (! IdChar(*ptr)) {
				if ((*ptr == '"') && (ptr[1] == '"')) {
					gchar *ptr2;
					for (ptr2 = ptr; ptr2[2]; ptr2++)
						*ptr2 = ptr2[2];
					*ptr2 = 0;
				}
				else if ((*ptr != '"') && (*ptr != '.'))
					*ptr = '_';
			}
		}

		ptr = NULL;
		if (strcmp (tmp, table_name))
			ptr = _gda_connection_compute_table_virtual_name (cnc, tmp);

		/* if it dow not work, replace all non IdChar character with '_' */
		if (!ptr) {
			for (ptr = tmp; *ptr; ptr++) {
				if (! IdChar(*ptr))
					*ptr = '_';
			}
			ptr = _gda_connection_compute_table_virtual_name (cnc, tmp);
		}
		g_free (tmp);
		return ptr;
	}
}


GdaServerProviderConnectionData *
gda_server_provider_connection_data_copy (GdaServerProviderConnectionData *src) {
	GdaServerProviderConnectionData *cp = g_new0 (GdaServerProviderConnectionData, 1);
	cp->worker = src->worker;
	cp->provider_data_destroy_func = src->provider_data_destroy_func;
	return cp;
}
void
gda_server_provider_connection_data_free (GdaServerProviderConnectionData *obj) {
	g_free (obj);
}

G_DEFINE_BOXED_TYPE(GdaServerProviderConnectionData, gda_server_provider_connection_data, gda_server_provider_connection_data_copy, gda_server_provider_connection_data_free)

/**
 * gda_connection_prepare_operation_create_table_v:
 * @cnc: an opened connection
 * @table_name: name of the table to create
 * @error: a place to store errors, or %NULL
 * @...: group of three arguments for column's name, column's #GType
 * and a #GdaServerOperationCreateTableFlag flag, finished with %NULL
 *
 * Convenient funtion for table creation.
 *
 * For details about arguments see #gda_server_operation_prepare_create_table_v().
 *
 * Returns: (transfer full) (nullable): a #GdaServerOperation if no errors; NULL and set @error otherwise
 *
 * Since: 6.0
 */
G_GNUC_NULL_TERMINATED
GdaServerOperation*
gda_connection_prepare_operation_create_table_v (GdaConnection *cnc, const gchar *table_name, GError **error, ...)
{
	GdaServerOperation *op;

	g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

	va_list  args;
	gchar   *arg;
	GType    type;
	GdaServerOperationCreateTableFlag flag;
	gint refs;
	GList *arguments = NULL;
	GdaServerOperationCreateTableArg* argument;

	va_start (args, error);
	type = 0;
	arg = NULL;
	refs = -1;

	while ((arg = va_arg (args, gchar*))) {
		argument = gda_server_operation_create_table_arg_new ();
		/* First argument for Column's name */
		gda_server_operation_create_table_arg_set_column_name (argument, (const gchar*) arg);

		/* Second to Define column's type */
		type = va_arg (args, GType);
		gda_server_operation_create_table_arg_set_column_type (argument, type);

		/* Third for column's flags */
		flag = va_arg (args, GdaServerOperationCreateTableFlag);
		gda_server_operation_create_table_arg_set_flags (argument, flag);
		if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG) {
			gint j;
			gint fields;
			gchar *fkey_table;
			gchar *fkey_ondelete;
			gchar *fkey_onupdate;

			refs++;
			GList *lfields = NULL;

			fkey_table = va_arg (args, gchar*);
			gda_server_operation_create_table_arg_set_fkey_table (argument, fkey_table);
			/* Get number of referenced fields */
			fields = va_arg (args, gint);

			for (j = 0; j < fields; j++) {
				gchar *field;
				GdaServerOperationCreateTableArgFKeyRefField *rfields;

				/* First pair arguments give local field and referenced field */
				field = va_arg (args, gchar*);
				rfields = gda_server_operation_create_table_arg_fkey_ref_field_new ();
				gda_server_operation_create_table_arg_fkey_ref_field_set_local_field (rfields, field);
				gda_server_operation_create_table_arg_fkey_ref_field_set_referenced_field (rfields, field);
				lfields = g_list_append (lfields, rfields);
			}
			gda_server_operation_create_table_arg_set_fkey_refs (argument, lfields);

			/* ON DELETE and ON UPDATE events constraints */
			fkey_ondelete = va_arg (args, gchar*);
			gda_server_operation_create_table_arg_set_fkey_ondelete (argument, fkey_ondelete);

			fkey_onupdate = va_arg (args, gchar*);
			gda_server_operation_create_table_arg_set_fkey_ondupdate (argument, fkey_onupdate);
		}

		arguments = g_list_append (arguments, argument);
	}

	va_end (args);

	op = gda_connection_prepare_operation_create_table (cnc, table_name, arguments, error);
	g_list_free_full (arguments, (GDestroyNotify) gda_server_operation_create_table_arg_free);
	return op;
}

/**
 * gda_connection_prepare_operation_create_table:
 * @cnc: an opened connection
 * @table_name: name of the table to create
 * @arguments: (element-type GdaServerOperationCreateTableArg): list of arguments as #GdaServerOperationPrepareCreateTableArg containing column's name,
 * column's #GType and a #GdaServerOperationCreateTableFlag flag
 * @error: a place to store errors, or %NULL
 *
 * Add more arguments if the flag needs them:
 *
 * GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG:
 * <itemizedlist>
 *   <listitem><para>string with the table's name referenced</para></listitem>
 *   <listitem><para>an integer with the number pairs "local_field", "referenced_field"
 *   used in the reference</para></listitem>
 *   <listitem><para>Pairs of "local_field", "referenced_field" to use, must match
 *    the number specified above.</para></listitem>
 *   <listitem><para>a string with the action for ON DELETE; can be: "RESTRICT", "CASCADE",
 *    "NO ACTION", "SET NULL" and "SET DEFAULT". Example: "ON UPDATE CASCADE".</para></listitem>
 *   <listitem><para>a string with the action for ON UPDATE (see above).</para></listitem>
 * </itemizedlist>
 *
 * Create a #GdaServerOperation object using an opened connection, taking three
 * arguments, a column's name the column's GType and #GdaServerOperationCreateTableFlag
 * flag, you need to finish the list using %NULL.
 *
 * You'll be able to modify the #GdaServerOperation object to add custom options
 * to the operation. When finished call #gda_server_operation_perform_create_table
 * or #gda_server_provider_perform_operation
 * in order to execute the operation.
 *
 * Returns: (transfer full) (nullable): a #GdaServerOperation if no errors; NULL and set @error otherwise
 *
 * Since: 6.0
 */
GdaServerOperation*
gda_connection_prepare_operation_create_table (GdaConnection *cnc, const gchar *table_name, GList *arguments, GError **error)
{
	GdaServerOperation *op;
	GdaServerProvider *server;

	g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

	server = gda_connection_get_provider (cnc);

	if (!table_name) {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_OBJECT_NAME_ERROR,
			     "%s", _("Unspecified table name"));
		return NULL;
	}

	if (gda_server_provider_supports_operation (server, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL)) {
		gchar   *cname;
		GType    type;
		gchar   *dbms_type;
		GdaServerOperationCreateTableFlag flag;
		gint i;
		gint refs;

		op = gda_server_provider_create_operation (server, cnc,
							   GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
		if (!GDA_IS_SERVER_OPERATION(op))
			return NULL;
		if(!gda_server_operation_set_value_at (op, table_name, error, "/TABLE_DEF_P/TABLE_NAME")) {
			g_object_unref (op);
			return NULL;
		}

		type = 0;
		cname = NULL;
		i = 0;
		refs = -1;

		GList *l = arguments;
		while (l != NULL) {
			GdaServerOperationCreateTableArg *argument;
			argument = (GdaServerOperationCreateTableArg*) l->data;
			cname = gda_server_operation_create_table_arg_get_column_name (argument);
			/* First argument for Column's name */
			if(!gda_server_operation_set_value_at (op, cname, error, "/FIELDS_A/@COLUMN_NAME/%d", i)) {
				g_object_unref (op);
				return NULL;
			}
			g_free (cname);

			/* Second to Define column's type */
			type = gda_server_operation_create_table_arg_get_column_type (argument);
			if (type == 0) {
				g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
					     "%s", _("Invalid type"));
				g_object_unref (op);
				return NULL;
			}
			dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (server,
											 cnc, type);
			if (!gda_server_operation_set_value_at (op, dbms_type, error, "/FIELDS_A/@COLUMN_TYPE/%d", i)){
				g_object_unref (op);
				return NULL;
			}

			/* Third for column's flags */
			flag = gda_server_operation_create_table_arg_get_flags (argument);
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_PKEY/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_NOT_NULL_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_NNUL/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_AUTOINC_FLAG)
				if (!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_AUTOINC/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_UNIQUE_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_UNIQUE/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG) {
				gint j;
				gchar *fkey_table;
				gchar *fkey_ondelete;
				gchar *fkey_onupdate;

				fkey_table = gda_server_operation_create_table_arg_get_column_name (argument);
				if (!gda_server_operation_set_value_at (op, fkey_table, error,
								   "/FKEY_S/%d/FKEY_REF_TABLE", refs)){
					g_object_unref (op);
					return NULL;
				}

				refs++;
				GList* lr = gda_server_operation_create_table_arg_get_fkey_refs (argument);
				j = 0;

				while (lr) {
					gchar *field, *rfield;
					GdaServerOperationCreateTableArgFKeyRefField *ref;
					ref = (GdaServerOperationCreateTableArgFKeyRefField*) l->data;

					field = gda_server_operation_create_table_arg_fkey_ref_field_get_local_field (ref);
					if(!gda_server_operation_set_value_at (op, field, error,
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d", refs, j)){
						g_object_unref (op);
						return NULL;
					}

					rfield = gda_server_operation_create_table_arg_fkey_ref_field_get_referenced_field (ref);
					if(!gda_server_operation_set_value_at (op, rfield, error,
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d", refs, j)){
						g_object_unref (op);
						return NULL;
					}
					j++;
					lr = g_list_next (lr);
				}

				fkey_ondelete = gda_server_operation_create_table_arg_get_fkey_ondelete (argument);
				if (!gda_server_operation_set_value_at (op, fkey_ondelete, error,
								   "/FKEY_S/%d/FKEY_ONDELETE", refs)){
					g_object_unref (op);
					return NULL;
				}
				fkey_onupdate = gda_server_operation_create_table_arg_get_fkey_onupdate (argument);
				if(!gda_server_operation_set_value_at (op, fkey_onupdate, error,
								   "/FKEY_S/%d/FKEY_ONUPDATE", refs)){
					g_object_unref (op);
					return NULL;
				}
			}

			l = g_list_next (l);
			i++;
		}

		g_object_set_data_full (G_OBJECT (op), "_gda_connection", g_object_ref (cnc), g_object_unref);

		return op;
	}
	else {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_OBJECT_NAME_ERROR,
			     "%s", _("CREATE TABLE operation is not supported by the database server"));
		return NULL;
	}
}

/**
 * gda_connection_prepare_operation_drop_table:
 * @cnc: an opened connection
 * @table_name: name of the table to drop
 * @error: a place to store errors, or %NULL
 *
 * This is just a convenient function to create a #GdaServerOperation to drop a
 * table in an opened connection.
 *
 * Returns: (transfer full) (nullable): a new #GdaServerOperation or %NULL if couldn't create the opereration.
 *
 * Since: 6.0
 */
GdaServerOperation*
gda_server_operation_prepare_drop_table (GdaConnection *cnc, const gchar *table_name, GError **error)
{
	GdaServerOperation *op;
	GdaServerProvider *server;

	server = gda_connection_get_provider (cnc);

	op = gda_server_provider_create_operation (server, cnc,
						   GDA_SERVER_OPERATION_DROP_TABLE, NULL, error);

	if (GDA_IS_SERVER_OPERATION (op)) {
		g_return_val_if_fail (table_name != NULL
				      || GDA_IS_CONNECTION (cnc)
				      || !gda_connection_is_opened (cnc), NULL);

		if (gda_server_operation_set_value_at (op, table_name,
						       error, "/TABLE_DESC_P/TABLE_NAME")) {
			g_object_set_data_full (G_OBJECT (op), "_gda_connection", g_object_ref (cnc), g_object_unref);
			return op;
		}
		else
			return NULL;
	}
	else
		return NULL;
}


/**
 * gda_connection_operation_get_sql_identifier_at:
 * @cnc: a #GdaConnection
 * @op: a #GdaServerOperation object
 * @path_format: a complete path to a node (starting with "/")
 * @error: (nullable): a place to store errors, or %NULL
 * @...: arguments to use with @path_format to make a complete path
 *
 * This method is similar to gda_server_operation_get_value_at(), but for SQL identifiers: a new string
 * is returned instead of a #GValue. Also the returned string is assumed to represents an SQL identifier
 * and will correctly be quoted to be used with @cnc.
 *
 * Returns: (transfer full) (nullable): a new string, or %NULL if the value is undefined or
 * if the @path is not defined or @path does not hold any value, or if the value held is not a string
 * (in that last case a warning is shown).
 *
 * Since: 6.0
 */
gchar *
gda_connection_operation_get_sql_identifier_at (GdaConnection *cnc, GdaServerOperation *op,
					    const gchar *path_format, GError **error, ...)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);

	gchar *path, *ret;
	va_list args;

	/* build path */
	va_start (args, error);
	path = g_strdup_vprintf (path_format, args);
	va_end (args);

	ret = gda_connection_operation_get_sql_identifier_at_path (cnc, op, path, error);
	g_free (path);

	return ret;
}



/**
 * gda_connection_operation_get_sql_identifier_at_path:
 * @op: a #GdaServerOperation object
 * @cnc: (nullable): a #GdaConnection, or %NULL
 * @path: a complete path to a node (starting with "/")
 * @error: (nullable): a place to store errors, or %NULL
 *
 * This method is similar to gda_server_operation_get_value_at(), but for SQL identifiers: a new string
 * is returned instead of a #GValue. Also the returned string is assumed to represents an SQL identifier
 * and will correctly be quoted to be used with @cnc.
 *
 * Returns: (transfer full) (nullable): a new string, or %NULL if the value is undefined or
 * if the @path is not defined or @path does not hold any value, or if the value held is not a string or
 * a valid SQL identifier.
 *
 * Since: 6.0
 */
gchar *
gda_connection_operation_get_sql_identifier_at_path (GdaConnection *cnc, GdaServerOperation *op,
						 const gchar *path, GError **error)
{
	const GValue *value = NULL;
	GdaConnectionOptions cncoptions = 0;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);

	value = gda_server_operation_get_value_at_path (op, path);

	if (!value || (G_VALUE_TYPE (value) == GDA_TYPE_NULL)) {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     _("Wrong SQL identifier value"));
		return NULL;
	}

	g_return_val_if_fail (G_VALUE_TYPE (value) == G_TYPE_STRING, NULL);

	const gchar *str;
	str = g_value_get_string (value);
	if (!str || !*str) {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     _("Wrong SQL identifier value"));
		return NULL;
	}

	if (cnc)
		g_object_get (G_OBJECT (cnc), "options", &cncoptions, NULL);
	return gda_sql_identifier_quote (str, cnc, gda_connection_get_provider (cnc), FALSE,
					 cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
}

/**
 * gda_connection_create_db_catalog:
 * @cnc: A #GdaConnection object to use
 *
 * A convenient method to create a new #GdaDbCatalog instance and set the current @cnc as a
 * property.  If for some reason, this approach doesn't fit well, the same task can be achieved
 * by the following code:
 *
 * GdaDbCatalog *catalog = gda_db_catalog_new ();
 * g_object_set (catalog, "connection", cnc, NULL);
 *
 * Returns: (transfer full): A new instance of #GdaDbCatalog. The new object should be deallocated
 * using g_object_unref().
 *
 * Since: 6.0
 */
GdaDbCatalog*
gda_connection_create_db_catalog (GdaConnection *cnc)
{
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc),NULL);

  return g_object_new (GDA_TYPE_DB_CATALOG,"connection",cnc,NULL);
}
