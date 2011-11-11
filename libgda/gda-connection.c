/*
 * Copyright (C) 2000 - 2001 Reinhard MÃ¼ller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2008 Johannes Schmid <jschmid@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 Daniel Espinosa <esodan@gmail.com>
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
#include <stdio.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-internal.h>
#include <libgda/gda-connection-event.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-server-provider.h>
#include "gda-marshal.h"
#include <libgda/gda-transaction-status-private.h>
#include <string.h>
#include <libgda/gda-enum-types.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-set.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-statement-struct-trans.h>
#include <libgda/gda-meta-store-extra.h>
#include <libgda/gda-util.h>
#include <libgda/gda-mutex.h>
#include <libgda/gda-lockable.h>
#include <libgda/thread-wrapper/gda-thread-provider.h>
#include <libgda/gda-repetitive-statement.h>
#include <gda-statement-priv.h>
#include <sqlite/virtual/gda-vconnection-data-model.h>

#include <glib/gstdio.h>
#include <fcntl.h>
#include <unistd.h>

static GStaticMutex parser_mutex = G_STATIC_MUTEX_INIT;
static GdaSqlParser *internal_parser = NULL;

#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

/* number of GdaConnectionEvent kept by each connection. Should be enough to avoid losing any
 * event, considering that the events are reseted after each statement execution */
#define EVENTS_ARRAY_SIZE 5

struct _GdaConnectionPrivate {
	GdaServerProvider    *provider_obj;
	GdaConnectionOptions  options; /* ORed flags */
	gchar                *dsn;
	gchar                *cnc_string;
	gchar                *auth_string;
	gboolean              is_thread_wrapper;
	guint                 monitor_id;

	GdaMetaStore         *meta_store;

	gboolean              auto_clear_events; /* TRUE if events_list is cleared before any statement execution */
	GdaConnectionEvent  **events_array; /* circular array */
	gint                  events_array_size;
	gboolean              events_array_full;
	gint                  events_array_next;
	GList                *events_list; /* for API compat */

	GdaTransactionStatus *trans_status;
	GHashTable           *prepared_stmts;

	gpointer              provider_data;
	GDestroyNotify        provider_data_destroy_func;

	/* multi threading locking */
	GThread              *unique_possible_thread; /* non NULL => only that thread can use this connection */
	GCond                *unique_possible_cond;
	GMutex               *object_mutex;
	GdaMutex             *mutex;

	/* Asynchronous statement execution */
	guint                 next_task_id; /* starts at 1 as 0 is an error */
	GArray               *waiting_tasks; /* array of CncTask pointers to tasks to be executed */
	GArray               *completed_tasks; /* array of CncTask pointers to tasks already executed */

	/* auto meta data update */
	GArray               *trans_meta_context; /* Array of GdaMetaContext pointers */

	gboolean              exec_times;
};

/* represents an asynchronous execution task */
typedef struct {
	guint task_id; /* ID assigned by GdaConnection object */
	guint prov_task_id; /* ID assigned by GdaServerProvider */
	gboolean being_processed; /* TRUE if currently being processed */
	GMutex *mutex;
	GdaStatement *stmt; /* statement to execute */
	GdaStatementModelUsage model_usage;
	GType *col_types;
	GdaSet *params;
	gboolean need_last_insert_row;
	GdaSet *last_insert_row;
	GObject *result;
	GError *error;
	GTimer *exec_timer;
} CncTask;
#define CNC_TASK(x) ((CncTask*)(x))

static CncTask *cnc_task_new (guint id, GdaStatement *stmt, GdaStatementModelUsage model_usage, 
			      GType *col_types, GdaSet *params, gboolean need_last_insert_row);
static void     cnc_task_free (CncTask *task);
#define         cnc_task_lock(task) g_mutex_lock ((task)->mutex)
#define         cnc_task_unlock(task) g_mutex_unlock ((task)->mutex)

static void add_exec_time_to_object (GObject *obj, GTimer *timer);

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc, GdaConnectionClass *klass);
static void gda_connection_dispose    (GObject *object);
static void gda_connection_finalize   (GObject *object);
static void gda_connection_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_connection_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

/* GdaLockable interface */
static void                 gda_connection_lockable_init (GdaLockableIface *iface);
static void                 gda_connection_lock      (GdaLockable *lockable);
static gboolean             gda_connection_trylock   (GdaLockable *lockable);
static void                 gda_connection_unlock    (GdaLockable *lockable);

static void update_meta_store_after_statement_exec (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params);
static void change_events_array_max_size (GdaConnection *cnc, gint size);

enum {
	ERROR,
	CONN_OPENED,
        CONN_TO_CLOSE,
        CONN_CLOSED,
	DSN_CHANGED,
	TRANSACTION_STATUS_CHANGED,
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
	PROP_THREAD_OWNER,
	PROP_IS_THREAD_WRAPPER,
	PROP_MONITOR_WRAPPED_IN_MAINLOOP,
	PROP_EVENTS_HISTORY_SIZE,
	PROP_EXEC_TIMES
};

static GObjectClass *parent_class = NULL;
extern GdaServerProvider *_gda_config_sqlite_provider; /* defined in gda-config.c */
static GdaServerProvider *_gda_thread_wrapper_provider = NULL;

static gint debug_level = -1;
static void
dump_exec_params (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params)
{
	if (params && (debug_level & 8)) {
		GSList *list;
		gchar *sql;
		sql = gda_statement_to_sql_extended (stmt, cnc, params, GDA_STATEMENT_SQL_PARAMS_SHORT,
						     NULL, NULL);
		g_print ("EVENT> COMMAND: parameters (on cnx %p) for statement [%s]\n", cnc, sql);
		for (list = params->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			gchar *str;
			const GValue *value;
			value = gda_holder_get_value (holder);
			str = value ? gda_value_stringify (value) : "NULL";
			g_print ("\t%s: type=>%s, value=>%s\n", gda_holder_get_id (holder),
				 gda_g_type_to_string (gda_holder_get_g_type (holder)),
				 str);
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

	parent_class = g_type_class_peek_parent (klass);

	/**
	 * GdaConnection::error
	 * @cnc: the #GdaConnection
	 * @event: a #GdaConnectionEvent object
	 *
	 * Gets emitted whenever a connection event occurs. Check the nature of @event to
	 * see if it's an error or a simple notification
	 */
	gda_connection_signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_CONNECTION_EVENT);
	/**
	 * GdaConnection::conn-opened
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the connection has been opened to the database
	 */
	gda_connection_signals[CONN_OPENED] =
                g_signal_new ("conn-opened",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_opened),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	/**
	 * GdaConnection::conn-to-close
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the connection to the database is about to be closed
	 */
        gda_connection_signals[CONN_TO_CLOSE] =
                g_signal_new ("conn-to-close",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_to_close),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	/**
	 * GdaConnection::conn-closed
	 * @cnc: the #GdaConnection
	 *
	 * Gets emitted when the connection to the database has been closed
	 */
        gda_connection_signals[CONN_CLOSED] =    /* runs after user handlers */
                g_signal_new ("conn-closed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_closed),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	/**
	 * GdaConnection::dsn-changed
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
	 * GdaConnection::transaction-status-changed
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

	g_object_class_install_property (object_class, PROP_THREAD_OWNER,
					 g_param_spec_pointer ("thread-owner", NULL,
							       _("Unique GThread from which the connection will be available."
								 "This should only be modified by the database providers' implementation"),
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/**
	 * GdaConnection:is-wrapper:
	 *
	 * Since: 4.2
	 **/
	g_object_class_install_property (object_class, PROP_IS_THREAD_WRAPPER,
					 g_param_spec_boolean ("is-wrapper", NULL,
							       _("Tells if the connection acts as a thread wrapper around another connection, making it completely thread safe"),
							       FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	/**
	 * GdaConnection:monitor-wrapped-in-mainloop:
	 *
	 * Useful only when there is a mainloop and when the connection acts as a thread wrapper around another connection,
	 * it sets up a timeout to handle signals coming from the wrapped connection.
	 *
	 * If the connection is not a thread wrapper, then this property has no effect.
	 *
	 * Since: 4.2
	 **/
	g_object_class_install_property (object_class, PROP_MONITOR_WRAPPED_IN_MAINLOOP,
					 g_param_spec_boolean ("monitor-wrapped-in-mainloop", NULL,
							       _("Make the connection set up a monitoring function in the mainloop to monitor the wrapped connection"),
							       FALSE,
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
							   _(""), EVENTS_ARRAY_SIZE, G_MAXINT,
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

	object_class->dispose = gda_connection_dispose;
	object_class->finalize = gda_connection_finalize;

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
gda_connection_lockable_init (GdaLockableIface *iface)
{
	iface->i_lock = gda_connection_lock;
	iface->i_trylock = gda_connection_trylock;
	iface->i_unlock = gda_connection_unlock;
}

static void
gda_connection_init (GdaConnection *cnc, G_GNUC_UNUSED GdaConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaConnectionPrivate, 1);
	cnc->priv->unique_possible_thread = NULL;
	cnc->priv->unique_possible_cond = NULL;
	cnc->priv->object_mutex = g_mutex_new ();
	cnc->priv->mutex = gda_mutex_new ();
	cnc->priv->provider_obj = NULL;
	cnc->priv->dsn = NULL;
	cnc->priv->cnc_string = NULL;
	cnc->priv->auth_string = NULL;
	cnc->priv->auto_clear_events = TRUE;
	cnc->priv->events_array_size = EVENTS_ARRAY_SIZE;
	cnc->priv->events_array = g_new0 (GdaConnectionEvent*, EVENTS_ARRAY_SIZE);
	cnc->priv->events_array_full = FALSE;
	cnc->priv->events_array_next = 0;
	cnc->priv->trans_status = NULL; /* no transaction yet */
	cnc->priv->prepared_stmts = NULL;

	cnc->priv->is_thread_wrapper = FALSE;
	cnc->priv->monitor_id = 0;

	cnc->priv->next_task_id = 1;
	cnc->priv->waiting_tasks = g_array_new (FALSE, FALSE, sizeof (gpointer));
	cnc->priv->completed_tasks = g_array_new (FALSE, FALSE, sizeof (gpointer));

	cnc->priv->trans_meta_context = NULL;
	cnc->priv->provider_data = NULL;
}

static void auto_update_meta_context_free (GdaMetaContext *context);
static void prepared_stms_foreach_func (GdaStatement *gda_stmt, GdaPStmt *prepared_stmt, GdaConnection *cnc);
static void
gda_connection_dispose (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	cnc->priv->unique_possible_thread = NULL;
	gda_connection_close_no_warning (cnc);

	/* get rid of prepared statements to avoid problems */
	if (cnc->priv->prepared_stmts) {
		g_hash_table_foreach (cnc->priv->prepared_stmts, 
				      (GHFunc) prepared_stms_foreach_func, cnc);
		g_hash_table_destroy (cnc->priv->prepared_stmts);
		cnc->priv->prepared_stmts = NULL;
	}

	if (cnc->priv->provider_obj) {
		g_object_unref (G_OBJECT (cnc->priv->provider_obj));
		cnc->priv->provider_obj = NULL;
	}

	if (cnc->priv->events_list) {
		g_list_foreach (cnc->priv->events_list, (GFunc) g_object_unref, NULL);
		g_list_free (cnc->priv->events_list);
		cnc->priv->events_list = NULL;
	}

	if (cnc->priv->events_array) {
		gint i;
		for (i = 0; i < cnc->priv->events_array_size ; i++) {
			GdaConnectionEvent *ev;
			ev = cnc->priv->events_array [i];
			if (ev)
				g_object_unref (ev);
		}
		g_free (cnc->priv->events_array);
		cnc->priv->events_array = NULL;
	}

	if (cnc->priv->trans_status) {
		g_object_unref (cnc->priv->trans_status);
		cnc->priv->trans_status = NULL;
	}

	if (cnc->priv->meta_store != NULL) {
	        g_object_unref (cnc->priv->meta_store);
	        cnc->priv->meta_store = NULL;
	}

	if (cnc->priv->waiting_tasks) {
		gint i, len = cnc->priv->waiting_tasks->len;
		for (i = 0; i < len; i++)
			cnc_task_free (CNC_TASK (g_array_index (cnc->priv->waiting_tasks, gpointer, i)));
		g_array_free (cnc->priv->waiting_tasks, TRUE);
		cnc->priv->waiting_tasks = NULL;
	}

	if (cnc->priv->completed_tasks) {
		gssize i, len = cnc->priv->completed_tasks->len;
		for (i = 0; i < len; i++)
			cnc_task_free (CNC_TASK (g_array_index (cnc->priv->completed_tasks, gpointer, i)));
		g_array_free (cnc->priv->completed_tasks, TRUE);
		cnc->priv->completed_tasks = NULL;
	}

	if (cnc->priv->trans_meta_context) {
		gsize i;
		for (i = 0; i < cnc->priv->trans_meta_context->len; i++) {
			GdaMetaContext *context;
			context = g_array_index (cnc->priv->trans_meta_context, GdaMetaContext*, i);
			auto_update_meta_context_free (context);
		}
		g_array_free (cnc->priv->trans_meta_context, TRUE);
		cnc->priv->trans_meta_context = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_connection_finalize (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	g_free (cnc->priv->dsn);
	g_free (cnc->priv->cnc_string);
	g_free (cnc->priv->auth_string);

	if (cnc->priv->unique_possible_cond)
		g_cond_free (cnc->priv->unique_possible_cond);
	if (cnc->priv->object_mutex)
		g_mutex_free (cnc->priv->object_mutex);
	gda_mutex_free (cnc->priv->mutex);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/* module error */
GQuark gda_connection_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_connection_error");
        return quark;
}

/**
 * gda_connection_get_type:
 * 
 * Registers the #GdaConnection class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 */
GType
gda_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static GTypeInfo info = {
			sizeof (GdaConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_connection_class_init,
			NULL, NULL,
			sizeof (GdaConnection),
			0,
			(GInstanceInitFunc) gda_connection_init,
			0
		};

		static GInterfaceInfo lockable_info = {
                        (GInterfaceInitFunc) gda_connection_lockable_init,
			NULL,
                        NULL
                };

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaConnection", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_LOCKABLE, &lockable_info);
		}
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static gboolean
monitor_wrapped_cnc (GdaThreadWrapper *wrapper)
{
	gda_thread_wrapper_iterate (wrapper, FALSE);
	return TRUE; /* don't remove the monitoring */
}

static void
gda_connection_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaConnection *cnc;

        cnc = GDA_CONNECTION (object);
        if (cnc->priv) {
                switch (param_id) {
		case PROP_THREAD_OWNER:
			g_mutex_lock (cnc->priv->object_mutex);
			gda_mutex_lock (cnc->priv->mutex);
			cnc->priv->unique_possible_thread = g_value_get_pointer (value);
#ifdef GDA_DEBUG_CNC_LOCK
			g_print ("Unique set to %p\n", cnc->priv->unique_possible_thread);
#endif
			if (cnc->priv->unique_possible_cond) {
#ifdef GDA_DEBUG_CNC_LOCK
				g_print ("Signalling on %p\n", cnc->priv->unique_possible_cond);
#endif
				g_cond_broadcast (cnc->priv->unique_possible_cond);
			}
			gda_mutex_unlock (cnc->priv->mutex);
			g_mutex_unlock (cnc->priv->object_mutex);
			break;
                case PROP_DSN: {
			const gchar *datasource = g_value_get_string (value);
			GdaDsnInfo *dsn;

			gda_connection_lock ((GdaLockable*) cnc);
			if (cnc->priv->provider_data) {
				g_warning (_("Could not set the '%s' property when the connection is opened"),
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
			
			g_free (cnc->priv->dsn);
			cnc->priv->dsn = g_strdup (datasource);
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
			if (cnc->priv->provider_data) {
				g_warning (_("Could not set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			g_free (cnc->priv->cnc_string);
			cnc->priv->cnc_string = NULL;
			if (g_value_get_string (value)) 
				cnc->priv->cnc_string = g_strdup (g_value_get_string (value));
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
                case PROP_PROVIDER_OBJ:
			gda_connection_lock ((GdaLockable*) cnc);
			if (cnc->priv->provider_data) {
				g_warning (_("Could not set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
                        if (cnc->priv->provider_obj)
				g_object_unref (cnc->priv->provider_obj);

			cnc->priv->provider_obj = g_value_get_object (value);
			g_object_ref (G_OBJECT (cnc->priv->provider_obj));
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
                case PROP_AUTH_STRING:
			gda_connection_lock ((GdaLockable*) cnc);
			if (cnc->priv->provider_data) {
				g_warning (_("Could not set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			else {
				const gchar *str = g_value_get_string (value);
				g_free (cnc->priv->auth_string);
				cnc->priv->auth_string = NULL;
				if (str)
					cnc->priv->auth_string = g_strdup (str);
			}
			gda_connection_unlock ((GdaLockable*) cnc);
                        break;
                case PROP_OPTIONS: {
			GdaConnectionOptions flags;
			flags = g_value_get_flags (value);
			gda_mutex_lock (cnc->priv->mutex);
			if (cnc->priv->provider_data &&
			    ((flags & (~GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE)) !=
			     (cnc->priv->options & (~GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE)))) {
				g_warning (_("Can't set the '%s' property once the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			cnc->priv->options = flags;
			gda_mutex_unlock (cnc->priv->mutex);
			break;
		}
		case PROP_META_STORE:
			gda_mutex_lock (cnc->priv->mutex);
			if (cnc->priv->meta_store) {
				g_object_unref (cnc->priv->meta_store);
				cnc->priv->meta_store = NULL;
			}
			cnc->priv->meta_store = g_value_get_object (value);
			if (cnc->priv->meta_store)
				g_object_ref (cnc->priv->meta_store);
			if (cnc->priv->is_thread_wrapper) {
				ThreadConnectionData *cdata;
				cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
				if (cdata) 
					g_object_set (G_OBJECT (cdata->sub_connection), "meta-store",
						      cnc->priv->meta_store, NULL);
			}
			gda_mutex_unlock (cnc->priv->mutex);
			break;
		case PROP_IS_THREAD_WRAPPER:
			cnc->priv->is_thread_wrapper = g_value_get_boolean (value);
			break;
		case PROP_MONITOR_WRAPPED_IN_MAINLOOP:
			if (cnc->priv->is_thread_wrapper) {
				/* cancel any existing signal monitoring */
				if (cnc->priv->monitor_id > 0) {
					g_source_remove (cnc->priv->monitor_id);
					cnc->priv->monitor_id = 0;
				}
				if (g_value_get_boolean (value)) {
					/* set up signal monitoring */
					ThreadConnectionData *cdata = NULL;
					cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
					if (cdata) {
						cnc->priv->monitor_id = g_timeout_add_seconds (1,
											       (GSourceFunc) monitor_wrapped_cnc,
											       cdata->wrapper);
						/* steal signals for current thread */
						gsize i;
						for (i = 0; i < cdata->handlers_ids->len; i++) {
							gulong id;
							id = g_array_index (cdata->handlers_ids, gulong, i);
							gda_thread_wrapper_steal_signal (cdata->wrapper, id);
						}
					}
				}
			}
			break;
		case PROP_EVENTS_HISTORY_SIZE:
			gda_connection_lock ((GdaLockable*) cnc);
			change_events_array_max_size (cnc, g_value_get_int (value));
			gda_connection_unlock ((GdaLockable*) cnc);
			break;
		case PROP_EXEC_TIMES:
			cnc->priv->exec_times = g_value_get_boolean (value);
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
        if (cnc->priv) {
                switch (param_id) {
                case PROP_DSN:
			g_value_set_string (value, cnc->priv->dsn);
                        break;
                case PROP_CNC_STRING:
			g_value_set_string (value, cnc->priv->cnc_string);
			break;
                case PROP_PROVIDER_OBJ:
			g_value_set_object (value, (GObject*) cnc->priv->provider_obj);
                        break;
                case PROP_AUTH_STRING:
			g_value_set_string (value, cnc->priv->auth_string);
                        break;
                case PROP_OPTIONS:
			g_value_set_flags (value, cnc->priv->options);
			break;
		case PROP_META_STORE:
			g_value_set_object (value, cnc->priv->meta_store);
			break;
		case PROP_IS_THREAD_WRAPPER:
			g_value_set_boolean (value, cnc->priv->is_thread_wrapper);
			break;
		case PROP_MONITOR_WRAPPED_IN_MAINLOOP:
			g_value_set_boolean (value, cnc->priv->is_thread_wrapper && (cnc->priv->monitor_id > 0) ?
					     TRUE : FALSE);
			break;
		case PROP_EVENTS_HISTORY_SIZE:
			g_value_set_int (value, cnc->priv->events_array_size);
			break;
		case PROP_EXEC_TIMES:
			g_value_set_boolean (value, cnc->priv->exec_times);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }	
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

/*
 * helper functions to manage CncTask
 */
static void task_stmt_reset_cb (GdaStatement *stmt, CncTask *task);
static CncTask *
cnc_task_new (guint id, GdaStatement *stmt, GdaStatementModelUsage model_usage, GType *col_types, 
	      GdaSet *params, gboolean need_last_insert_row)
{
	CncTask *task;

	task = g_new0 (CncTask, 1);
	task->being_processed = FALSE;
	task->task_id = id;
	task->stmt = g_object_ref (stmt);
	task->exec_timer = NULL;
	g_signal_connect (stmt, "reset", /* monitor statement changes */
			  G_CALLBACK (task_stmt_reset_cb), task);
	task->model_usage = model_usage;
	GType *req_types;
	req_types = merge_column_types (_gda_statement_get_requested_types (stmt), col_types);
	if (req_types)
		task->col_types = req_types;
	else {
		if (_gda_statement_get_requested_types (stmt))
			col_types = (GType*) _gda_statement_get_requested_types (stmt);
		if (col_types) {
			gint i;
			for (i = 0; ; i++) {
				if (col_types [i] == G_TYPE_NONE)
					break;
			}
			i++;
			task->col_types = g_new (GType, i);
			memcpy (task->col_types, col_types, i * sizeof (GType)); /* Flawfinder: ignore */
		}
	}
	if (params)
		task->params = gda_set_copy (params);
	task->need_last_insert_row = need_last_insert_row;

	task->mutex = g_mutex_new ();
	return task;
}

static void
task_stmt_reset_cb (G_GNUC_UNUSED GdaStatement *stmt, CncTask *task)
{
	g_mutex_lock (task->mutex);
	g_signal_handlers_disconnect_by_func (task->stmt,
					      G_CALLBACK (task_stmt_reset_cb), task);
	g_object_unref (task->stmt);
	task->stmt = NULL;
	g_mutex_unlock (task->mutex);
}

static void
cnc_task_free (CncTask *task)
{
	g_mutex_lock (task->mutex);
	if (task->stmt) {
		g_signal_handlers_disconnect_by_func (task->stmt,
						      G_CALLBACK (task_stmt_reset_cb), task);
		g_object_unref (task->stmt);
	}
	if (task->params)
		g_object_unref (task->params);
	if (task->col_types)
		g_free (task->col_types);
	if (task->last_insert_row)
		g_object_unref (task->last_insert_row);
	if (task->result)
		g_object_unref (task->result);
	if (task->error)
		g_error_free (task->error);
	if (task->exec_timer)
		g_timer_destroy (task->exec_timer);

	g_mutex_unlock (task->mutex);
	g_mutex_free (task->mutex);
	g_free (task);
}

/**
 * _gda_connection_get_internal_thread_provider
 */ 
GdaServerProvider *
_gda_connection_get_internal_thread_provider (void)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock (&mutex);
	if (!_gda_thread_wrapper_provider)
		_gda_thread_wrapper_provider = GDA_SERVER_PROVIDER (g_object_new (GDA_TYPE_THREAD_PROVIDER, NULL));
	g_static_mutex_unlock (&mutex);
	return _gda_thread_wrapper_provider;
}


/**
 * gda_connection_new_from_dsn:
 * @dsn: data source name.
 * @auth_string: (allow-none): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function is similar to gda_connection_open_from_dsn(), except it does not actually open the
 * connection, you have to open it using gda_connection_open().
 *
 * Returns: (transfer full): a new #GdaConnection if connection opening was successful or %NULL if there was an error.
 *
 * Since: 5.0.2
 */
GdaConnection *
gda_connection_new_from_dsn (const gchar *dsn, const gchar *auth_string, 
			      GdaConnectionOptions options, GError **error)
{
	GdaConnection *cnc = NULL;
	GdaDsnInfo *dsn_info;
	gchar *user, *pass, *real_dsn;
	gchar *real_auth_string = NULL;

	g_return_val_if_fail (dsn && *dsn, NULL);

	if (((options & GDA_CONNECTION_OPTIONS_THREAD_SAFE) || (options & GDA_CONNECTION_OPTIONS_THREAD_SAFE)) &&
	    !g_thread_supported ()) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_UNSUPPORTED_THREADS_ERROR,
			      "%s", _("Multi threading is not supported or enabled"));
		return NULL;
	}

	gda_dsn_split (dsn, &real_dsn, &user, &pass);
	if (!real_dsn) {
		g_free (user);
		g_free (pass);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Malformed data source specification '%s'"), dsn);
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
		if (pinfo) {
			prov = gda_config_get_provider (dsn_info->provider, error);
			if (((options & GDA_CONNECTION_OPTIONS_THREAD_SAFE) &&
			     !gda_server_provider_supports_feature (prov, NULL,
								    GDA_CONNECTION_FEATURE_MULTI_THREADING)) ||
			    (options & GDA_CONNECTION_OPTIONS_THREAD_ISOLATED)) {
				options |= GDA_CONNECTION_OPTIONS_THREAD_ISOLATED;
				prov = _gda_connection_get_internal_thread_provider ();
			}
		}
		else
			g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
				     _("No provider '%s' installed"), dsn_info->provider);
		
		if (prov) {
			if (PROV_CLASS (prov)->create_connection) {
				cnc = PROV_CLASS (prov)->create_connection (prov);
				if (cnc) 
					g_object_set (G_OBJECT (cnc), 
						      "provider", prov,
						      "dsn", real_dsn,
						      "auth-string", auth_string ? auth_string : real_auth_string, 
						      "options", options, NULL);
			}
			else
				cnc = g_object_new (GDA_TYPE_CONNECTION, 
						    "provider", prov, 
						    "dsn", real_dsn, 
						    "auth-string", auth_string ? auth_string : real_auth_string, 
						    "options", options, NULL);
		}
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
 * gda_connection_open_from_dsn:
 * @dsn: data source name.
 * @auth_string: (allow-none): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function is the way of opening database connections with libgda, using a pre-defined data source (DSN),
 * see gda_config_define_dsn() for more information about how to define a DSN. If you don't want to define
 * a DSN, it is possible to use gda_connection_open_from_string() instead of this method.
 *
 * The @dsn string must have the following format: "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" 
 * (if &lt;username&gt; and/or &lt;password&gt; are provided, and @auth_string is %NULL, then these username
 * and passwords will be used). Note that if provided, &lt;username&gt; and &lt;password&gt; 
 * must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * The @auth_string can contain the authentication information for the server
 * to accept the connection. It is a string containing semi-colon seperated named value, usually 
 * like "USERNAME=...;PASSWORD=..." where the ... are replaced by actual values. Note that each
 * name and value must be encoded as per RFC 1738, see gda_rfc1738_encode() for more information.
 *
 * The actual named parameters required depend on the provider being used, and that list is available
 * as the <parameter>auth_params</parameter> member of the #GdaProviderInfo structure for each installed
 * provider (use gda_config_get_provider_info() to get it). Also one can use the "gda-sql-5.0 -L" command to 
 * list the possible named parameters.
 *
 * This method may fail with a GDA_CONNECTION_ERROR domain error (see the #GdaConnectionError error codes) 
 * or a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * Returns: (transfer full): a new #GdaConnection if connection opening was successful or %NULL if there was an error.
 */
GdaConnection *
gda_connection_open_from_dsn (const gchar *dsn, const gchar *auth_string, 
			      GdaConnectionOptions options, GError **error)
{
	GdaConnection *cnc;
	cnc = gda_connection_new_from_dsn (dsn, auth_string, options, error);
	if (cnc && !gda_connection_open (cnc, error)) {
		g_object_unref (cnc);
		cnc = NULL;
	}
	return cnc;
}


/**
 * gda_connection_new_from_string:
 * @provider_name: (allow-none): provider ID to connect to, or %NULL
 * @cnc_string: connection string.
 * @auth_string: (allow-none): authentication string, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function is similar to gda_connection_open_from_string(), except it does not actually open the
 * connection, you have to open it using gda_connection_open().
 *
 * Returns: (transfer full): a new #GdaConnection if connection opening was successful or %NULL if there was an error.
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

	if (options & GDA_CONNECTION_OPTIONS_THREAD_SAFE &&
	    !g_thread_supported ()) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_UNSUPPORTED_THREADS_ERROR,
			      "%s", _("Multi threading is not supported or enabled"));
		return NULL;
	}

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
			      "%s", _("No provider specified"));
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
		if (pinfo) {
			prov = gda_config_get_provider (provider_name ? provider_name : real_provider, error);
			if (((options & GDA_CONNECTION_OPTIONS_THREAD_SAFE) &&
			     !gda_server_provider_supports_feature (prov, NULL,
								    GDA_CONNECTION_FEATURE_MULTI_THREADING)) ||
			    (options & GDA_CONNECTION_OPTIONS_THREAD_ISOLATED)) {
				gchar *tmp;
				tmp = g_strdup_printf ("%s;PROVIDER_NAME=%s", real_cnc, pinfo->id);
				g_free (real_cnc);
				real_cnc = tmp;
				options |= GDA_CONNECTION_OPTIONS_THREAD_ISOLATED;
				prov = _gda_connection_get_internal_thread_provider ();
			}
		}
		else
			g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
				     _("No provider '%s' installed"), provider_name ? provider_name : real_provider);

		if (prov) {
			if (PROV_CLASS (prov)->create_connection) {
				cnc = PROV_CLASS (prov)->create_connection (prov);
				if (cnc) 
					g_object_set (G_OBJECT (cnc), 
						      "provider", prov,
						      "cnc-string", real_cnc,
						      "auth-string", auth_string ? auth_string : real_auth_string, 
						      "options", options, NULL);
			}
			else 
				cnc = (GdaConnection *) g_object_new (GDA_TYPE_CONNECTION, 
								      "provider", prov,
								      "cnc-string", real_cnc, 
								      "auth-string", auth_string ? auth_string : real_auth_string, 
								      "options", options, NULL);
		}
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
 * @provider_name: (allow-none): provider ID to connect to, or %NULL
 * @cnc_string: connection string.
 * @auth_string: (allow-none): authentication string, or %NULL
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
 * The possible keys depend on the provider, the "gda-sql-5.0 -L" command
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
 * string, use the "gda-sql-5.0 -L" command to list the possible named parameters.
 *
 * Additionally, it is possible to have the connection string
 * respect the "&lt;provider_name&gt;://&lt;real cnc string&gt;" format, in which case the provider name
 * and the real connection string will be extracted from that string (note that if @provider_name
 * is not %NULL then it will still be used as the provider ID).\
 *
 * This method may fail with a GDA_CONNECTION_ERROR domain error (see the #GdaConnectionError error codes) 
 * or a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
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
	GdaConnection *cnc;
	GdaServerProvider *prov = _gda_config_sqlite_provider;
	gchar *user, *pass, *real_cnc, *real_provider;

	/*g_print ("%s(%s)\n", __FUNCTION__, cnc_string);*/
	gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
        if (!real_cnc) {
                g_free (user);
                g_free (pass);
                g_free (real_provider);
                return NULL;
        }
	if (PROV_CLASS (prov)->create_connection) {
		cnc = PROV_CLASS (prov)->create_connection (prov);
		if (cnc)
			g_object_set (G_OBJECT (cnc),
				      "provider", prov,
				      "cnc-string", real_cnc,
				      NULL);
	}
	else
		cnc = (GdaConnection *) g_object_new (GDA_TYPE_CONNECTION,
						      "provider", prov,
						      "cnc-string", real_cnc,
						      NULL);
	
	g_free (real_cnc);
        g_free (user);
        g_free (pass);
        g_free (real_provider);

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
 * @directory: (allow-none): the directory the database file will be in, or %NULL for the default TMP directory
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
		g_signal_connect (cnc, "conn-closed",
				  G_CALLBACK (sqlite_connection_closed_cb), NULL);
	}
	return cnc;
}

typedef struct {
	gboolean thread_exists;
	GThread *looked_up_thread;
} ThreadLookupData;

static void
all_threads_func (GThread *thread, ThreadLookupData *data)
{
	if (thread == data->looked_up_thread)
		data->thread_exists = TRUE;
}

/**
 * gda_connection_open:
 * @cnc: a #GdaConnection object
 * @error: a place to store errors, or %NULL
 *
 * Tries to open the connection.
 *
 * Returns: TRUE if the connection is opened, and FALSE otherwise.
 */
gboolean
gda_connection_open (GdaConnection *cnc, GError **error)
{
	GdaDsnInfo *dsn_info = NULL;
	GdaQuarkList *params, *auth;
	char *real_auth_string = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* don't do anything if connection is already opened */
	if (cnc->priv->provider_data)
		return TRUE;

	gda_connection_lock ((GdaLockable*) cnc);

	/* connection string */
	if (cnc->priv->dsn) {
		/* get the data source info */
		dsn_info = gda_config_get_dsn_info (cnc->priv->dsn);
		if (!dsn_info) {
			gda_log_error (_("Data source %s not found in configuration"), cnc->priv->dsn);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
				     _("Data source %s not found in configuration"), cnc->priv->dsn);
			gda_connection_unlock ((GdaLockable*) cnc);
			return FALSE;
		}

		g_free (cnc->priv->cnc_string);
		cnc->priv->cnc_string = g_strdup (dsn_info->cnc_string);
	}
	else {
		if (!cnc->priv->cnc_string) {
			gda_log_error (_("No DSN or connection string specified"));
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_CNC_SPEC_ERROR,
				      "%s", _("No DSN or connection string specified"));
			gda_connection_unlock ((GdaLockable*) cnc);
			return FALSE;
		}
		/* try to see if connection string has the <provider>://<rest of the string> format */
	}

	/* provider test */
	if (!cnc->priv->provider_obj) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
			      "%s", _("No provider specified"));
		gda_connection_unlock ((GdaLockable*) cnc);
		return FALSE;
	}

	/* if there is a limiting thread but it's not yet set, then initialize it to the current
	 * thread */
	if (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread ==
	    GDA_SERVER_PROVIDER_UNDEFINED_LIMITING_THREAD)
		PROV_CLASS (cnc->priv->provider_obj)->limiting_thread = g_thread_self ();

	if (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread &&
	    (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread != g_thread_self ())) {
		ThreadLookupData data;
		data.thread_exists = FALSE;
		data.looked_up_thread = PROV_CLASS (cnc->priv->provider_obj)->limiting_thread;

		g_thread_foreach ((GFunc) all_threads_func, &data);
		if (data.thread_exists) {
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_ERROR,
				     "%s", _("Provider does not allow usage from this thread"));
			gda_connection_unlock ((GdaLockable*) cnc);
			return FALSE;
		}
		else {
			/* the thread which initialized cnc->priv->provider_obj does not exist
			 * anymore, so we steal ownership of the provider from the current
			 * thread */
			PROV_CLASS (cnc->priv->provider_obj)->limiting_thread = g_thread_self ();
		}
	}

	if (!PROV_CLASS (cnc->priv->provider_obj)->open_connection) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_ERROR,
			      "%s", _("Internal error: provider does not implement the open_connection() virtual method"));
		gda_connection_unlock ((GdaLockable*) cnc);
		return FALSE;
	}

	params = gda_quark_list_new_from_string (cnc->priv->cnc_string);

	/* retrieve correct auth_string */
	if (cnc->priv->auth_string)
		real_auth_string = g_strdup (cnc->priv->auth_string);
	else {
		if (dsn_info && dsn_info->auth_string)
			real_auth_string = g_strdup (dsn_info->auth_string);
		else 
			/* look for authentication parameters in cnc string */
			real_auth_string = g_strdup (cnc->priv->cnc_string);
	}

	/* try to open the connection */
	auth = gda_quark_list_new_from_string (real_auth_string);
	gboolean opened;

	opened = PROV_CLASS (cnc->priv->provider_obj)->open_connection (cnc->priv->provider_obj, cnc, params, auth,
									NULL, NULL, NULL);
	if (opened && !cnc->priv->provider_data) {
		g_warning ("Internal error: connection reported as opened, yet no provider data set");
		opened = FALSE;
	}

	if (!opened) {
		const GList *events;
		
		events = gda_connection_get_events (cnc);
		if (events) {
			GList *l;

			for (l = g_list_last ((GList*) events); l; l = l->prev) {
				GdaConnectionEvent *event;

				event = GDA_CONNECTION_EVENT (l->data);
				if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR) {
					if (error && !(*error))
						g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
							     "%s", gda_connection_event_get_description (event));
				}
			}
		}
	}

	/* free memory */
	gda_quark_list_free (params);
	gda_quark_list_free (auth);
	g_free (real_auth_string);

	if (cnc->priv->provider_data) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'CONN_OPENED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_OPENED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'CONN_OPENED' from %s\n", __FUNCTION__);
#endif
	}

	/* limit connection's usable thread */
	if (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread)
		g_object_set (G_OBJECT (cnc), "thread-owner", 
			      PROV_CLASS (cnc->priv->provider_obj)->limiting_thread, NULL);


	gda_connection_unlock ((GdaLockable*) cnc);
	return cnc->priv->provider_data ? TRUE : FALSE;
}


/**
 * gda_connection_close:
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source, but first emits the 
 * "conn-to-close" signal.
 */
void
gda_connection_close (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	if (! cnc->priv->provider_data)
		return;

	gda_connection_lock ((GdaLockable*) cnc);
#ifdef GDA_DEBUG_signal
        g_print (">> 'CONN_TO_CLOSE' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_TO_CLOSE], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'CONN_TO_CLOSE' from %s\n", __FUNCTION__);
#endif

        gda_connection_close_no_warning (cnc);
	gda_connection_unlock ((GdaLockable*) cnc);
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
 * gda_connection_close_no_warning:
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source, without emiting any warning signal.
 */
void
gda_connection_close_no_warning (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	g_object_ref (cnc);
	gda_connection_lock ((GdaLockable*) cnc);

	if (cnc->priv->monitor_id > 0) {
		g_source_remove (cnc->priv->monitor_id);
		cnc->priv->monitor_id = 0;
	}

	if (! cnc->priv->provider_data) {
		g_object_unref (cnc);
		gda_connection_unlock ((GdaLockable*) cnc);
		return;
	}

	if (cnc->priv->meta_store &&
	    cnc->priv->trans_meta_context &&
	    gda_connection_get_transaction_status (cnc)) {
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (cnc->priv->meta_store);
		if (cnc != mscnc) {
			gsize i;
			for (i = 0; i < cnc->priv->trans_meta_context->len; i++) {
				GdaMetaContext *context;
				GError *lerror = NULL;
				context = g_array_index (cnc->priv->trans_meta_context, GdaMetaContext*, i);
				if (! gda_connection_update_meta_store (cnc, context, &lerror))
					add_connection_event_from_error (cnc, &lerror);
				auto_update_meta_context_free (context);
			}
			g_array_free (cnc->priv->trans_meta_context, TRUE);
			cnc->priv->trans_meta_context = NULL;
		}
	}

	/* get rid of prepared statements to avoid problems */
	if (cnc->priv->prepared_stmts) {
		g_hash_table_foreach (cnc->priv->prepared_stmts, 
				      (GHFunc) prepared_stms_foreach_func, cnc);
		g_hash_table_destroy (cnc->priv->prepared_stmts);
		cnc->priv->prepared_stmts = NULL;
	}

	/* really close connection */
	if (PROV_CLASS (cnc->priv->provider_obj)->close_connection) 
		PROV_CLASS (cnc->priv->provider_obj)->close_connection (cnc->priv->provider_obj, 
									cnc);
	if (cnc->priv->provider_data) {
		if (cnc->priv->provider_data_destroy_func)
			cnc->priv->provider_data_destroy_func (cnc->priv->provider_data);
		else
			g_warning ("Provider did not clean its connection data");
		cnc->priv->provider_data = NULL;
	}

	gda_connection_unlock ((GdaLockable*) cnc);
#ifdef GDA_DEBUG_signal
        g_print (">> 'CONN_CLOSED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_CLOSED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'CONN_CLOSED' from %s\n", __FUNCTION__);
#endif
	g_object_unref (cnc);
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

	return cnc->priv->provider_data ? TRUE : FALSE;
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

	return cnc->priv->options;
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

	return cnc->priv->provider_obj;
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
	if (!cnc->priv->provider_obj)
		return NULL;

	return gda_server_provider_get_name (cnc->priv->provider_obj);
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

	return (const gchar *) cnc->priv->dsn;
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

	return (const gchar *) cnc->priv->cnc_string;
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

	str = (const gchar *) cnc->priv->auth_string;
	if (!str) 
		str = "";
	return str;
}

/**
 * gda_connection_insert_row_into_table: (skip)
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
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, insert, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (insert);

	return retval;
}

/**
 * gda_connection_update_row_in_table: (skip)
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
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
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
		g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
		g_slist_free (holders);
	}

	retval = (gda_connection_statement_execute_non_select (cnc, delete, set, NULL, error) == -1) ? FALSE : TRUE;

	if (set)
		g_object_unref (set);
	g_object_unref (delete);

	return retval;
}

/**
 * gda_connection_parse_sql_string:
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @sql: an SQL command to parse, not %NULL
 * @params: (out) (allow-none) (transfer full): a place to store a new #GdaSet, for parameters used in SQL command, or %NULL
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

	/* ownership is transfered to the caller ! */

	GdaConnectionEvent *eev;
	eev = cnc->priv->events_array [cnc->priv->events_array_next];
	if (!eev)
		eev = GDA_CONNECTION_EVENT (g_object_new (GDA_TYPE_CONNECTION_EVENT,
							  "type", (int)type, NULL));
	else {
		gda_connection_event_set_event_type (eev, type);
		cnc->priv->events_array [cnc->priv->events_array_next] = NULL;
	}

	return eev;
}

#ifdef GDA_DEBUG_NO
static void
dump_events_array (GdaConnection *cnc)
{
	gint i;
	g_print ("=== Array dump for %p ===\n", cnc);
	for (i = 0; i < cnc->priv->events_array_size; i++) {
		g_print ("   [%d] => %p\n", i, cnc->priv->events_array [i]);
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

	gda_mutex_lock (cnc->priv->mutex);

	/* clear external list of events */
	if (cnc->priv->events_list) {
		g_list_foreach (cnc->priv->events_list, (GFunc) g_object_unref, NULL);
		g_list_free (cnc->priv->events_list);
		cnc->priv->events_list = NULL;
	}

	/* add event, ownership is transfered to @cnc */
	GdaConnectionEvent *eev;
	eev = cnc->priv->events_array [cnc->priv->events_array_next];
	if (eev != event) {
		if (eev)
			g_object_unref (eev);
		cnc->priv->events_array [cnc->priv->events_array_next] = event;
	}

	/* handle indexes */
	cnc->priv->events_array_next ++;
	if (cnc->priv->events_array_next == cnc->priv->events_array_size) {
		cnc->priv->events_array_next = 0;
		cnc->priv->events_array_full = TRUE;
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
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, event);

#ifdef GDA_DEBUG_NO
	dump_events_array (cnc);
#endif
	gda_mutex_unlock (cnc->priv->mutex);
}

/**
 * gda_connection_add_event_string: (skip)
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
	if (locked_cnc->priv->auto_clear_events) {
		locked_cnc->priv->events_array_full = FALSE;
		locked_cnc->priv->events_array_next = 0;
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
 * @options: (allow-none): an optional list of parameters
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
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	return gda_server_provider_create_operation (cnc->priv->provider_obj, cnc, type, options, error);
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	cnc->priv->auto_clear_events = FALSE;
	retval = gda_server_provider_perform_operation (cnc->priv->provider_obj, cnc, op, error);
	cnc->priv->auto_clear_events = TRUE;
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
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	return gda_server_provider_create_parser (cnc->priv->provider_obj, cnc);
}

/*
 * Also resets the events list (as perceived when calling gda_connection_get_events()
 */
static void
change_events_array_max_size (GdaConnection *cnc, gint size)
{
	size ++; /* add 1 to compensate the "lost" slot when rotating the events array */
	if (size == cnc->priv->events_array_size)
		return;

	if (size > cnc->priv->events_array_size) {
		gint i;
		cnc->priv->events_array = g_renew (GdaConnectionEvent*, cnc->priv->events_array,
						   size);
		for (i = cnc->priv->events_array_size; i < size; i++)
			cnc->priv->events_array [i] = NULL;
	}
	else if (size >= EVENTS_ARRAY_SIZE) {
		gint i;
		for (i = size; i < cnc->priv->events_array_size; i++) {
			if (cnc->priv->events_array [i])
				g_object_unref (cnc->priv->events_array [i]);
		}
		cnc->priv->events_array = g_renew (GdaConnectionEvent*, cnc->priv->events_array,
						   size);
	}
	cnc->priv->events_array_size = size;
	cnc->priv->events_array_full = FALSE;
	cnc->priv->events_array_next = 0;
}

/**
 * gda_connection_batch_execute:
 * @cnc: a #GdaConnection object
 * @batch: a #GdaBatch object which contains all the statements to execute
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_batch_get_parameters()), or %NULL
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

	gda_connection_lock ((GdaLockable*) cnc);
	cnc->priv->auto_clear_events = FALSE;

	/* increase the size of cnc->priv->events_array to be able to store all the
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
	cnc->priv->auto_clear_events = TRUE;
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

	return gda_sql_identifier_quote (id, cnc, NULL, FALSE,
					 cnc->priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
}

/**
 * gda_connection_statement_to_sql:
 * @cnc: a #GdaConnection object
 * @stmt: a #GdaStatement object
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @flags: SQL rendering flags, as #GdaStatementSqlFlag OR'ed values
 * @params_used: (allow-none) (element-type Gda.Holder) (out) (transfer container): a place to store the list of individual #GdaHolder objects within @params which have been used
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
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	if (PROV_CLASS (cnc->priv->provider_obj)->statement_to_sql)
		return (PROV_CLASS (cnc->priv->provider_obj)->statement_to_sql) (cnc->priv->provider_obj, 
										 cnc, stmt, params, flags, 
										 params_used, error);
	else
		return gda_statement_to_sql_extended (stmt, cnc, params, flags, params_used, error);
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->statement_prepare)
		return (PROV_CLASS (cnc->priv->provider_obj)->statement_prepare)(cnc->priv->provider_obj, 
										 cnc, stmt, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			      "%s", _("Provider does not support statement preparation"));
		return FALSE;
	}
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

	types = g_new0 (GType, max + 1);
	types [max] = G_TYPE_NONE;
	for (col = va_arg (args, gint); col >= 0; col = va_arg (args, gint)) {
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
 * No locking is done here must be done before calling
 *
 * Returns: -1 if task not found
 */
static gint
get_task_index (GdaConnection *cnc, guint task_id, gboolean *out_completed, gboolean id_is_prov)
{
	gint i, len;
	CncTask *task;
	len = cnc->priv->completed_tasks->len;
	for (i = 0; i < len; i++) {
		task = CNC_TASK (g_array_index (cnc->priv->completed_tasks, gpointer, i));
		if ((! id_is_prov && (task->task_id == task_id)) ||
		    (id_is_prov && (task->prov_task_id == task_id))) {
			*out_completed = TRUE;
			return i;
		}
	}

	len = cnc->priv->waiting_tasks->len;
	for (i = 0; i < len; i++) {
		task = CNC_TASK (g_array_index (cnc->priv->waiting_tasks, gpointer, i));
		if ((! id_is_prov && (task->task_id == task_id)) ||
		    (id_is_prov && (task->prov_task_id == task_id))) {
			*out_completed = FALSE;
			return i;
		}
	}
	
	return -1;
}

/*
 * This callback is called from the GdaServerProvider object
 */
static void
async_stmt_exec_cb (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, guint task_id,
		    GObject *result_obj, const GError *error, G_GNUC_UNUSED CncTask *task)
{
	gint i;
	gboolean is_completed;

	g_object_ref ((GObject*) cnc);
	gda_connection_lock (GDA_LOCKABLE (cnc));

	i = get_task_index (cnc, task_id, &is_completed, TRUE);
	if (i >= 0) {
		CncTask *task;
		g_assert (!is_completed);

		/* complete @task and free some memory */
		task = CNC_TASK (g_array_index (cnc->priv->waiting_tasks, gpointer, i));
		cnc_task_lock (task);

		task->being_processed = FALSE;
		if (task->exec_timer)
			g_timer_stop (task->exec_timer);

		if (error)
			task->error = g_error_copy (error);
		if (result_obj) {
			task->result = g_object_ref (result_obj);
			if (task->exec_timer)
				add_exec_time_to_object (task->result, task->exec_timer);
		}
		if (task->stmt) {
			g_signal_handlers_disconnect_by_func (task->stmt,
							      G_CALLBACK (task_stmt_reset_cb), task);
			g_object_unref (task->stmt);
			task->stmt = NULL;
		}
		if (task->params) {
			g_object_unref (task->params);
			task->params = NULL;
		}
		if (task->col_types) {
			g_free (task->col_types);
			task->col_types = NULL;
		}

		g_array_remove_index (cnc->priv->waiting_tasks, i);
		g_array_append_val (cnc->priv->completed_tasks, task);

		cnc_task_unlock (task);

		/* execute next waiting task if there is one */
		if (cnc->priv->waiting_tasks->len >= 1) {
			/* execute statement now as there are no other ones to be executed */
			GError *lerror = NULL;
			task = CNC_TASK (g_array_index (cnc->priv->waiting_tasks, gpointer, 0));
			cnc_task_lock (task);
			task->being_processed = TRUE;			
			dump_exec_params (cnc, task->stmt, task->params);
			if (cnc->priv->exec_times)
				g_timer_start (task->exec_timer);

			PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, cnc, 
										 task->stmt, 
										 task->params, 
										 task->model_usage, 
										 task->col_types, 
										 &(task->last_insert_row),
										 &(task->prov_task_id),
										 (GdaServerProviderExecCallback) async_stmt_exec_cb, 
										 task, &lerror);
			if (lerror) {
				/* task execution failed => move it to completed tasks array */
				task->error = lerror;
				task->being_processed = FALSE;
				if (cnc->priv->exec_times)
					g_timer_stop (task->exec_timer);
				g_array_remove_index (cnc->priv->waiting_tasks, 0);
				g_array_append_val (cnc->priv->completed_tasks, task);
			}
			else
				update_meta_store_after_statement_exec (cnc, task->stmt, task->params);
			cnc_task_unlock (task);
		}
	}
	else
		g_warning ("Provider called back for the execution of task %u (provider numbering) which does not exist, "
			   "ignored.\n", task_id);

	gda_connection_unlock (GDA_LOCKABLE (cnc));
	g_object_unref ((GObject*) cnc);
}

/**
 * gda_connection_async_statement_execute:
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: in the case where @stmt is a SELECT statement, specifies how the returned data model will be used
 * @col_types: (array) (allow-none): an array of GType to request each returned #GdaDataModel's column's GType, terminated with the G_TYPE_NONE
 * @need_last_insert_row: TRUE if the values of the last interted row must be computed
 * @error: a place to store errors, or %NULL
 *
 * This method is similar to gda_connection_statement_execute() but is asynchronous as it method returns
 * immediately with a task ID. It's up to the caller to use gda_connection_async_fetch_result() regularly to check
 * if the statement's execution is finished.
 *
 * It is possible to call the method several times to request several statements to be executed asynchronously, the
 * statements will be executed in the order in which they were requested.
 *
 * The parameters, if present, are copied and can be discarded or modified before the statement is actually executed.
 * The @stmt object is not copied but simply referenced (for performance reasons), and if it is modified before
 * it is actually executed, then its execution will not occur. It is however safe to call g_object_unref() on it if
 * it's not needed anymore.
 *
 * The execution failure of any statement has no impact on the execution of other statements except for example if
 * the connection has a transaction started and the failure invalidates the transaction (as decided by the database
 * server).
 *
 * Returns: a task ID, or 0 if an error occurred (not an error regarding @stmt itself as its execution has not yet started
 * but any other error)
 *
 * Since: 4.2
 */
guint
gda_connection_async_statement_execute (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
					GdaStatementModelUsage model_usage, GType *col_types,
					gboolean need_last_insert_row,
					GError **error)
{
	guint id;
	CncTask *task;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), 0);
	g_return_val_if_fail (cnc->priv->provider_obj, 0);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), 0);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, 0);


	g_object_ref ((GObject*) cnc);
	if (! gda_connection_trylock ((GdaLockable*) cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CANT_LOCK_ERROR,
			     _("Can't obtain connection lock"));
		g_object_unref ((GObject*) cnc);
		return 0;
	}

	if (!cnc->priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		gda_connection_unlock (GDA_LOCKABLE (cnc));
		g_object_unref ((GObject*) cnc);
		return 0;
	}

	id = cnc->priv->next_task_id ++;
	task = cnc_task_new (id, stmt, model_usage, col_types, params, need_last_insert_row);
	g_array_append_val (cnc->priv->waiting_tasks, task);
	if (cnc->priv->exec_times) {
		task->exec_timer = g_timer_new ();
		g_timer_stop (task->exec_timer);
	}

	if (cnc->priv->waiting_tasks->len == 1) {
		/* execute statement now as there are no other ones to be executed */
		GError *lerror = NULL;

		cnc_task_lock (task);
		task->being_processed = TRUE;
		dump_exec_params (cnc, task->stmt, task->params);
		if (cnc->priv->exec_times)
			g_timer_start (task->exec_timer);

		PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, cnc, 
									 task->stmt,
									 task->params, 
									 task->model_usage,
									 task->col_types,
									 &(task->last_insert_row),
									 &(task->prov_task_id),
									 (GdaServerProviderExecCallback) async_stmt_exec_cb, 
									 task, &lerror);
		if (lerror) {
			/* task execution failed => move it to completed tasks array */
			gint i;
			gboolean is_completed;

			task->error = lerror;
			task->being_processed = FALSE;
			if (cnc->priv->exec_times)
				g_timer_stop (task->exec_timer);
			i = get_task_index (cnc, id, &is_completed, FALSE);
			g_assert ((i >= 0) && !is_completed);
			g_array_remove_index (cnc->priv->waiting_tasks, i);
			g_array_append_val (cnc->priv->completed_tasks, task);
		}
		else
			update_meta_store_after_statement_exec (cnc, task->stmt, task->params);
		cnc_task_unlock (task);
	}

	gda_connection_unlock ((GdaLockable*) cnc);
	g_object_unref ((GObject*) cnc);

	return id;
}

/**
 * gda_connection_async_fetch_result:
 * @cnc: a #GdaConnection
 * @task_id: a task ID returned by gda_connection_async_statement_execute()
 * @last_insert_row: (out) (transfer full) (allow-none): a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Use this method to obtain the result of the execution of a statement which has been executed asynchronously by
 * calling gda_connection_async_statement_execute(). This function is non locking and will return %NULL (and no
 * error will be set) if the statement has not been executed yet.
 *
 * If the statement has been executed, this method returns the same value as gda_connection_statement_execute()
 * would have if the statement had been
 * executed synchronously.
 *
 * Returns: (transfer full): a #GObject, or %NULL if an error occurred
 *
 * Since: 4.2
 */
GObject *
gda_connection_async_fetch_result (GdaConnection *cnc, guint task_id, GdaSet **last_insert_row, GError **error)
{
	gint i;
	gboolean is_completed;
	GObject *obj = NULL;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (! gda_connection_trylock ((GdaLockable*) cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CANT_LOCK_ERROR,
			     _("Can't obtain connection lock"));
		return NULL;
	}

	/* if provider needs to be awaken, then do it now */
	if (PROV_CLASS (cnc->priv->provider_obj)->handle_async) {
		if (! (PROV_CLASS (cnc->priv->provider_obj)->handle_async (cnc->priv->provider_obj, cnc, error))) {
			gda_connection_unlock ((GdaLockable*) cnc);
			return NULL;
		}
	}
	
	i = get_task_index (cnc, task_id, &is_completed, FALSE);
	if (i < 0) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_TASK_NOT_FOUND_ERROR,
		     _("Can't find task %u"), task_id);
	}
	else if (is_completed) {
		/* task completed */
		CncTask *task;
		task = CNC_TASK (g_array_index (cnc->priv->completed_tasks, gpointer, i));
		g_array_remove_index (cnc->priv->completed_tasks, i);
		
		cnc_task_lock (task);
		if (task->result)
			obj = g_object_ref (task->result);
		if (task->error) {
			g_propagate_error (error, task->error);
			task->error = NULL;
		}
		if (last_insert_row) {
			if (task->last_insert_row)
				*last_insert_row = g_object_ref (task->last_insert_row);
			else
				*last_insert_row = NULL;
		}
		cnc_task_unlock (task);
		cnc_task_free (task);
	}
	else {
		/* task not yet completed */
		/* nothing to do */
	}

	gda_connection_unlock ((GdaLockable*) cnc);
	return obj;
}

/**
 * gda_connection_async_cancel:
 * @cnc: a #GdaConnection
 * @task_id: a task ID returned by gda_connection_async_statement_execute()
 * @error: a place to store errors, or %NULL
 *
 * Requests that a task be cancelled. This operation may of may not have any effect
 * depending on the task's status, even if it returns %TRUE. If it returns %FALSE,
 * then the task has not been cancelled.
 *
 * Returns: TRUE if no error occurred
 *
 * Since: 4.2
 */
gboolean
gda_connection_async_cancel (GdaConnection *cnc, guint task_id, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (! gda_connection_trylock ((GdaLockable*) cnc)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CANT_LOCK_ERROR,
			     _("Can't obtain connection lock"));
		return FALSE;
	}

	gint i;
	gboolean is_completed;
	gboolean retval = TRUE;
	i = get_task_index (cnc, task_id, &is_completed, FALSE);
	if ((i >= 0) && (!is_completed)) {
		CncTask *task;
		task = CNC_TASK (g_array_index (cnc->priv->waiting_tasks, gpointer, i));
		if (task->being_processed) {
			if (PROV_CLASS (cnc->priv->provider_obj)->cancel) {
				retval = PROV_CLASS (cnc->priv->provider_obj)->cancel (cnc->priv->provider_obj, cnc,
										       task->prov_task_id, error);
				if (retval) {
					/* cancellation may have succeeded => remove this task from the tasks to execute */
					g_array_remove_index (cnc->priv->waiting_tasks, i);
					cnc_task_free (task);
				}
			}
			else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
					     "%s", _("Provider does not support asynchronous server operation"));
				retval = FALSE;
			}
			task->being_processed = FALSE;
			if (cnc->priv->exec_times)
				g_timer_stop (task->exec_timer);
		}
		else {
			/* simply remove this task from the tasks to execute */
			g_array_remove_index (cnc->priv->waiting_tasks, i);
			cnc_task_free (task);
		}
	}
	else if (i < 0) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_TASK_NOT_FOUND_ERROR,
			     _("Can't find task %u"), task_id);
		retval = FALSE;
	}

	gda_connection_unlock ((GdaLockable*) cnc);
	return retval;
}

/*
 * Wrapper which adds @...
 */
static GObject *
gda_connection_statement_execute_v (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
				    GdaStatementModelUsage model_usage, GdaSet **last_inserted_row, GError **error, ...)
{
	va_list ap;
	GObject *obj;
	GType *types, *req_types;
	GTimer *timer = NULL;
	va_start (ap, error);
	types = make_col_types_array (ap);
	va_end (ap);

	g_object_ref ((GObject*) cnc);
	gda_connection_lock ((GdaLockable*) cnc);

	_clear_connection_events (cnc);

	if (last_inserted_row) 
		*last_inserted_row = NULL;

	if (!cnc->priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		gda_connection_unlock (GDA_LOCKABLE (cnc));
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
	if (cnc->priv->exec_times)
		timer = g_timer_new ();
	obj = PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, cnc, stmt, params, 
								       model_usage,
								       req_types ? req_types : types,
								       last_inserted_row, 
								       NULL, NULL, NULL, error);
	if (timer)
		g_timer_stop (timer);
	g_free (types);

	if (obj) {
		if (timer)
			add_exec_time_to_object (obj, timer);
		update_meta_store_after_statement_exec (cnc, stmt, params);
	}
	gda_connection_unlock ((GdaLockable*) cnc);
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
			      || !gda_connection_is_opened (cnc)
			      || g_str_has_prefix (sql, "SELECT"),
			      NULL);

	g_static_mutex_lock (&parser_mutex);
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();
	g_static_mutex_unlock (&parser_mutex);

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

	g_static_mutex_lock (&parser_mutex);
	if (!internal_parser)
		internal_parser = gda_sql_parser_new ();
	g_static_mutex_unlock (&parser_mutex);

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
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: in the case where @stmt is a SELECT statement, specifies how the returned data model will be used
 * @last_insert_row: (out) (transfer full) (allow-none): a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
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
 * If @last_insert_row is not %NULL and @stmt is an INSERT statement, then it will contain (if the
 * provider used by @cnc supports it) a new #GdaSet object composed of value holders named "+&lt;column number&gt;"
 * starting at column 0 which contain the actual inserted values. For example if a table is composed of an 'id' column
 * which is auto incremented and a 'name' column then the execution of a "INSERT INTO mytable (name) VALUES ('joe')"
 * query will return a #GdaSet with two holders:
 * <itemizedlist>
 *   <listitem><para>one with the '+0' ID which may for example contain 1 (note that its "name" property should be "id")</para></listitem>
 *   <listitem><para>one with the '+1' ID which will contain 'joe' (note that its "name" property should be "name")</para></listitem>
 * </itemizedlist>
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
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	if (last_inserted_row) 
		*last_inserted_row = NULL;

	return gda_connection_statement_execute_v (cnc, stmt, params, model_usage, last_inserted_row, error, -1);
}

/**
 * gda_connection_statement_execute_non_select:
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @last_insert_row: (out) (transfer full) (allow-none): a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
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
	g_return_val_if_fail (cnc->priv->provider_obj, -1);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), -1);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, -1);

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
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
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
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

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
 * gda_connection_statement_execute_select_fullv: (skip)
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
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
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	va_list ap;
	GdaDataModel *model;
	GType *types, *req_types;
	GTimer *timer = NULL;

	va_start (ap, error);
	types = make_col_types_array (ap);
	va_end (ap);

	g_object_ref ((GObject*) cnc);
	gda_connection_lock ((GdaLockable*) cnc);

	_clear_connection_events (cnc);

	if (!cnc->priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		gda_connection_unlock (GDA_LOCKABLE (cnc));
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
	if (cnc->priv->exec_times)
		timer = g_timer_new ();
	model = (GdaDataModel *) PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, 
											  cnc, stmt, params, model_usage, 
											  req_types ? req_types : types,
											  NULL, NULL, 
											  NULL, NULL, error);
	if (timer)
		g_timer_stop (timer);
	gda_connection_unlock ((GdaLockable*) cnc);
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
 * @params: (allow-none): a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @col_types: (array) (allow-none): an array of GType to request each returned #GdaDataModel's column's GType, terminated with the G_TYPE_NONE
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
	GdaDataModel *model;
	GTimer *timer = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	g_object_ref ((GObject*) cnc);
	gda_connection_lock ((GdaLockable*) cnc);
	
	if (!cnc->priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		gda_connection_unlock (GDA_LOCKABLE (cnc));
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
	if (cnc->priv->exec_times)
		timer = g_timer_new ();
	model = (GdaDataModel *) PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, 
											  cnc, stmt, params, 
											  model_usage,
											  req_types ? req_types : col_types,
											  NULL, 
											  NULL, NULL, NULL, error);
	if (timer)
		g_timer_stop (timer);
	g_free (req_types);
	gda_connection_unlock ((GdaLockable*) cnc);
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
 * @col_types: (array) (allow-none): an array of GType to request each returned GdaDataModel's column's GType, see gda_connection_statement_execute_select_full() for more information
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
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_REPETITIVE_STATEMENT (rstmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	g_object_get (rstmt, "statement", &stmt, NULL);
	g_return_val_if_fail (stmt, NULL);

	g_object_ref ((GObject*) cnc);
	gda_connection_lock ((GdaLockable*) cnc);

	if (!cnc->priv->provider_data) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_CLOSED_ERROR,
			     _("Connection is closed"));
		gda_connection_unlock (GDA_LOCKABLE (cnc));
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
		GObject *obj;
		GError *lerror = NULL;
		GTimer *timer = NULL;

		dump_exec_params (cnc, stmt, (GdaSet*) list->data);
		if (cnc->priv->exec_times)
			timer = g_timer_new ();
		obj = PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, cnc, stmt, 
									       GDA_SET (list->data), 
									       model_usage,
									       req_types ? req_types : col_types,
									       NULL, 
									       NULL, NULL, NULL, &lerror);
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
			retlist = g_slist_prepend (retlist, obj);
		}
		if (timer)
			g_timer_destroy (timer);
	}
	g_slist_free (sets_list);
	g_free (req_types);

	gda_connection_unlock ((GdaLockable*) cnc);
	g_object_unref ((GObject*) cnc);
	g_object_unref (stmt);

	return g_slist_reverse (retlist);
}

/*
 * Forces the GdaTransactionStatus. This is reserved to connections which are thread wrappers
 *
 * @trans_status may be NULL
 * if @trans_status is not NULL, then its ref count is increased.
 */
void
_gda_connection_force_transaction_status (GdaConnection *cnc, GdaConnection *wrapped_cnc)
{
	g_assert (cnc->priv->is_thread_wrapper);
	if (cnc->priv->trans_status) 
		g_object_unref (cnc->priv->trans_status);

	cnc->priv->trans_status = wrapped_cnc->priv->trans_status;
	if (cnc->priv->trans_status)
		g_object_ref (cnc->priv->trans_status);

#ifdef GDA_DEBUG_signal
        g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif

#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
}

/**
 * gda_connection_begin_transaction:
 * @cnc: a #GdaConnection object.
 * @name: (allow-none): the name of the transation to start, or %NULL
 * @level: the requested transaction level (%GDA_TRANSACTION_ISOLATION_UNKNOWN if not specified)
 * @error: a place to store errors, or %NULL
 *
 * Starts a transaction on the data source, identified by the
 * @name parameter.
 *
 * Before starting a transaction, you can check whether the underlying
 * provider does support transactions or not by using the
 * gda_connection_supports_feature() function.
 *
 * Returns: %TRUE if the transaction was started successfully, %FALSE
 * otherwise.
 */
gboolean
gda_connection_begin_transaction (GdaConnection *cnc, const gchar *name, GdaTransactionIsolation level,
				  GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->begin_transaction)
		return PROV_CLASS (cnc->priv->provider_obj)->begin_transaction (cnc->priv->provider_obj, cnc, name, level, error);
	else
		return FALSE;
}

/**
 * gda_connection_commit_transaction:
 * @cnc: a #GdaConnection object.
 * @name: (allow-none): the name of the transation to commit, or %NULL
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->commit_transaction)
		return PROV_CLASS (cnc->priv->provider_obj)->commit_transaction (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_rollback_transaction:
 * @cnc: a #GdaConnection object.
 * @name: (allow-none): the name of the transation to commit, or %NULL
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->rollback_transaction)
		return PROV_CLASS (cnc->priv->provider_obj)->rollback_transaction (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_add_savepoint:
 * @cnc: a #GdaConnection object
 * @name: (allow-none): name of the savepoint to add
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	
	if (PROV_CLASS (cnc->priv->provider_obj)->add_savepoint)
		return PROV_CLASS (cnc->priv->provider_obj)->add_savepoint (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_rollback_savepoint:
 * @cnc: a #GdaConnection object
 * @name: (allow-none): name of the savepoint to rollback to
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);	

	if (PROV_CLASS (cnc->priv->provider_obj)->rollback_savepoint)
		return PROV_CLASS (cnc->priv->provider_obj)->rollback_savepoint (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_delete_savepoint:
 * @cnc: a #GdaConnection object
 * @name: (allow-none): name of the savepoint to delete
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->delete_savepoint)
		return PROV_CLASS (cnc->priv->provider_obj)->delete_savepoint (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
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

	return cnc->priv->trans_status;
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	return gda_server_provider_supports_feature (cnc->priv->provider_obj, cnc, feature);
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

static gchar *
meta_context_stringify (GdaMetaContext *context)
{
	gint i;
	gchar *str;
	GString *string = g_string_new ("");

	for (i = 0; i < context->size; i++) {
		if (i > 0)
			g_string_append_c (string, ' ');
		str = gda_value_stringify (context->column_values[i]);
		g_string_append_printf (string, " [%s => %s]", context->column_names[i], str);
		g_free (str);
	}
	if (i == 0)
		g_string_append (string, "---");
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

/*#define GDA_DEBUG_META_STORE_UPDATE*/
#ifdef GDA_DEBUG_META_STORE_UPDATE
static void
meta_context_dump (GdaMetaContext *context)
{
	gchar *str;
	str = meta_context_stringify (context);
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
		str = meta_context_stringify (context);
		g_set_error (error, GDA_CONNECTION_ERROR,
			     GDA_CONNECTION_META_DATA_CONTEXT_ERROR,
			     _("Missing or wrong arguments for table '%s': %s"),
			     context->table_name, str);
		g_free (str);
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
		if (!PROV_CLASS (provider)->meta_funcs._btypes) {
			WARN_METHOD_NOT_IMPLEMENTED (provider, "_btypes");
			break;
		}
		retval = PROV_CLASS (provider)->meta_funcs._btypes (provider, cnc, store, context, error);
		WARN_META_UPDATE_FAILURE (retval, "_btypes");
		return retval;
	}
	
	case 'c': 
		if ((tname[1] == 'o') && (tname[2] == 'l') && (tname[3] == 'u')) {
			/* _columns,  params: 
			 *  -0- @table_catalog, @table_schema, @table_name
			 *  -1- @character_set_catalog, @character_set_schema, @character_set_name
			 *  -2- @collation_catalog, @collation_schema, @collation_name
			 */
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
			if (i == 0) {
				if (!PROV_CLASS (provider)->meta_funcs.columns) {
					WARN_METHOD_NOT_IMPLEMENTED (provider, "columns");
					break;
				}
				retval = PROV_CLASS (provider)->meta_funcs.columns (provider, cnc, store, context, error, 
										    catalog, schema, name);
				WARN_META_UPDATE_FAILURE (retval, "columns");
				return retval;
			}
			else {
				/* nothing to do */
				return TRUE;
			}
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
			if (!PROV_CLASS (provider)->meta_funcs.collations) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "collations");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.collations (provider, cnc, store, context, error, 
									       catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "collations");
			return retval;
		}
		else if ((tname[1] == 'h') && (tname[1] == 'a')) {
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
			if (!PROV_CLASS (provider)->meta_funcs.character_sets) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "character_sets");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.character_sets (provider, cnc, store, context, error, 
										   catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "character_sets");
			return retval;
		}
		else if ((tname[1] == 'h') && (tname[1] == 'e')) {
			/* _check_column_usage, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
			 *  -1- @table_catalog, @table_schema, @table_name, @column_name
			 */
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
			if (i == 0) {
				if (!PROV_CLASS (provider)->meta_funcs.check_columns) {
					WARN_METHOD_NOT_IMPLEMENTED (provider, "check_columns");
					break;
				}
				retval = PROV_CLASS (provider)->meta_funcs.check_columns (provider, cnc, store, context, error, 
											  catalog, schema, tabname, cname);
				WARN_META_UPDATE_FAILURE (retval, "check_columns");
				return retval;
			}
			else {
				/* nothing to do */
				return TRUE;
			}
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
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "domains");
			if (!PROV_CLASS (provider)->meta_funcs.domains) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "domains");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.domains (provider, cnc, store, context, error, 
									    catalog, schema);
			WARN_META_UPDATE_FAILURE (retval, "domains");
			return retval;
		}
		else {
			/* _domain_constraints, params: 
			 *  -0- @domain_catalog, @domain_schema, @domain_name
			 *  -1- @constraint_catalog, @constraint_schema
			 */
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
			if (!PROV_CLASS (provider)->meta_funcs.constraints_dom) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "constraints_dom");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.constraints_dom (provider, cnc, store, context, error,
										    catalog, schema, domname);
			WARN_META_UPDATE_FAILURE (retval, "constraints_dom");
			return retval;
		}
		break;

	case 'e':
		if (tname[1] == 'n') {
			/* _enums, params: 
			 *  -0- @udt_catalog, @udt_schema, @udt_name
			 */
			i = check_parameters (context, error, 1,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "udt_catalog", &catalog, "udt_schema", &schema, "udt_name", &name, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "enums");
			if (!PROV_CLASS (provider)->meta_funcs.enums) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "enums");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.enums (provider, cnc, store, context, error, 
									  catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "enums");
			return retval;
		}
		else {
			/* _element_types, params: 
			 *  -0- @specific_name
			 */
			i = check_parameters (context, error, 1,
					      &name, G_TYPE_STRING, NULL,
					      "specific_name", &name, NULL);
			if (i < 0)
				return FALSE;

			ASSERT_TABLE_NAME (tname, "element_types");
			if (!PROV_CLASS (provider)->meta_funcs.el_types) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "el_types");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.el_types (provider, cnc, store, context, error, name);
			WARN_META_UPDATE_FAILURE (retval, "el_types");
			return retval;
		}
		break;

	case 'i':
		if ((tname[1] == 'n') && (tname[2] == 'f')) {
			/* _information_schema_catalog_name, params: 
			 *  - none
			 */
			ASSERT_TABLE_NAME (tname, "information_schema_catalog_name");
			if (!PROV_CLASS (provider)->meta_funcs._info) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "_info");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs._info (provider, cnc, store, context, error);
			WARN_META_UPDATE_FAILURE (retval, "_info");
			return retval;
		}
		else {
			/* _index_column_usage, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @index_name
			 */
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
			if (!PROV_CLASS (provider)->meta_funcs.index_cols) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "index_cols");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.index_cols (provider, cnc, store, context, error,
									       catalog, schema, name, iname);
			WARN_META_UPDATE_FAILURE (retval, "index_cols");
			return retval;
		}
		break;
	case 'k': {
		/* _key_column_usage, params: 
		 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
		 *  -0- @ref_table_catalog, @ref_table_schema, @ref_table_name, @ref_constraint_name
		 */
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
		if (i == 0) {
			if (!PROV_CLASS (provider)->meta_funcs.key_columns) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "key_columns");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.key_columns (provider, cnc, store, context, error, 
										catalog, schema, tabname, cname);
			WARN_META_UPDATE_FAILURE (retval, "key_columns");
			return retval;
		}
		else {
			/* nothing to do */
			return TRUE;
		}
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
		if (i < 0)
			return FALSE;
		
		ASSERT_TABLE_NAME (tname, "parameters");
		if (!PROV_CLASS (provider)->meta_funcs.routine_par) {
			WARN_METHOD_NOT_IMPLEMENTED (provider, "routine_par");
			break;
		}
		retval = PROV_CLASS (provider)->meta_funcs.routine_par (provider, cnc, store, context, error, 
									catalog, schema, name);
		WARN_META_UPDATE_FAILURE (retval, "routine_par");
		return retval;

	case 'r': 
		if ((tname[1] == 'e') && (tname[2] == 'f')) {
			/* _referential_constraints, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
			 *  -0- @ref_table_catalog, @ref_table_schema, @ref_table_name, @ref_constraint_name
			 */
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
				if (!PROV_CLASS (provider)->meta_funcs.constraints_ref) {
					WARN_METHOD_NOT_IMPLEMENTED (provider, "constraints_ref");
					break;
				}
				retval = PROV_CLASS (provider)->meta_funcs.constraints_ref (provider, cnc, store, context, error, 
											    catalog, schema, tabname, cname);
				WARN_META_UPDATE_FAILURE (retval, "constraints_ref");
				return retval;
			}
			else {
				/* nothing to do */
				return TRUE;
			}
		}
		if ((tname[1] == 'o') && (tname[2] == 'u') && (tname[3] == 't') && (tname[4] == 'i') &&
		    (tname[5] == 'n') && (tname[6] == 'e') && (tname[7] == 's')) {
			/* _routines, params: 
			 *  -0- @specific_catalog, @specific_schema, @specific_name
			 *  -1- @specific_catalog, @specific_schema
			 */
			i = check_parameters (context, error, 2,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "specific_catalog", &catalog, "specific_schema", &schema, "specific_name", &name, NULL,
					      "specific_catalog", &catalog, "specific_schema", &schema, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "routines");
			if (!PROV_CLASS (provider)->meta_funcs.routines) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "routines");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.routines (provider, cnc, store, context, error, 
									     catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "routines");
			return retval;
		}
		else {
			/* _routine_columns, params: 
			 *  -0- @specific_catalog, @specific_schema, @specific_name
			 */
			i = check_parameters (context, error, 1,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "specific_catalog", &catalog, "specific_schema", &schema, "specific_name", &name, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "routine_columns");
			if (!PROV_CLASS (provider)->meta_funcs.routine_col) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "routine_col");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.routine_col (provider, cnc, store, context, error, 
										catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "routine_col");
			return retval;
		}
		break;

	case 's': {
		/* _schemata, params:
		 *  -0- @catalog_name, @schema_name 
		 *  -1- @catalog_name
		 */
		i = check_parameters (context, error, 2,
				      &schema, G_TYPE_STRING, NULL,
				      "catalog_name", &catalog, "schema_name", &schema, NULL,
				      "catalog_name", &catalog, NULL);
		if (i < 0)
			return FALSE;
		ASSERT_TABLE_NAME (tname, "schemata");

		if (!PROV_CLASS (provider)->meta_funcs.schemata) {
			WARN_METHOD_NOT_IMPLEMENTED (provider, "schemata");
			break;
		}
		retval = PROV_CLASS (provider)->meta_funcs.schemata (provider, cnc, store, context, error, 
								     catalog, schema);
		WARN_META_UPDATE_FAILURE (retval, "schemata");
		return retval;
	}
	case 't': 
		if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && (tname[5] == 's')) {
			/* _tables, params: 
			 *  -0- @table_catalog, @table_schema, @table_name
			 *  -1- @table_catalog, @table_schema
			 */
			i = check_parameters (context, error, 2,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "table_catalog", &catalog, "table_schema", &schema, "table_name", &name, NULL,
					      "table_catalog", &catalog, "table_schema", &schema, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "tables");
			if (!PROV_CLASS (provider)->meta_funcs.tables_views) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "tables_views");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.tables_views (provider, cnc, store, context, error, 
										 catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "tables_views");
			return retval;
		}
		else if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && 
			 (tname[5] == '_') && (tname[6] == 'c')) {
			/* _tables_constraints, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @constraint_name
			 *  -1- @table_catalog, @table_schema, @table_name
			 */
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
			if (!PROV_CLASS (provider)->meta_funcs.constraints_tab) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "constraints_tab");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.constraints_tab (provider, cnc, store, context, error,
										    catalog, schema, tabname, cname);
			WARN_META_UPDATE_FAILURE (retval, "constraints_tab");
			return retval;
		}
		else if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && 
			 (tname[5] == '_') && (tname[6] == 'i')) {
			/* _table_indexes, params: 
			 *  -0- @table_catalog, @table_schema, @table_name, @index_name
			 *  -1- @table_catalog, @table_schema, @table_name
			 */
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
			if (!PROV_CLASS (provider)->meta_funcs.indexes_tab) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "indexes_tab");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.indexes_tab (provider, cnc, store, context, error,
										catalog, schema, tabname, iname);
			WARN_META_UPDATE_FAILURE (retval, "indexes_tab");
			return retval;
		}
		else if ((tname[1] == 'r') && (tname[2] == 'i')) {
			/* _triggers,  params: 
			 *  -0- @trigger_catalog, @trigger_schema
			 *  -1- @event_object_catalog, @event_object_schema, @event_object_table
			 */
			i = check_parameters (context, error, 2,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING,
					      &name, G_TYPE_STRING, NULL,
					      "trigger_catalog", &catalog, "trigger_schema", &schema, NULL,
					      "event_object_catalog", &catalog, "event_object_schema", &schema, "event_object_table", &name, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "triggers");
			if (i == 1) {
				if (!PROV_CLASS (provider)->meta_funcs.triggers) {
					WARN_METHOD_NOT_IMPLEMENTED (provider, "triggers");
					break;
				}
				retval = PROV_CLASS (provider)->meta_funcs.triggers (provider, cnc, store, context, error, 
										     catalog, schema, name);
				WARN_META_UPDATE_FAILURE (retval, "triggers");
				return retval;
			}
			else {
				/* nothing to do */
				return TRUE;
			}
		}
		break;

	case 'u': 
		if ((tname[1] == 'd') && (tname[2] == 't') && (tname[3] == 0)) {
			/* _udt, params: 
			 *  -0- @udt_catalog, @udt_schema
			 */
			i = check_parameters (context, error, 1,
					      &catalog, G_TYPE_STRING,
					      &schema, G_TYPE_STRING, NULL,
					      "udt_catalog", &catalog, "udt_schema", &schema, NULL);
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "udt");
			if (!PROV_CLASS (provider)->meta_funcs.udt) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "udt");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.udt (provider, cnc, store, context, error, 
									catalog, schema);
			WARN_META_UPDATE_FAILURE (retval, "udt");
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
			if (i < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "udt_columns");
			if (!PROV_CLASS (provider)->meta_funcs.udt_cols) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "udt_cols");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.udt_cols (provider, cnc, store, context, error, 
									     catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "udt_cols");
			return retval;
		}
		break;

	case 'v': {
		/* _view_column_usage,  params: 
		 *  -0- @view_catalog, @view_schema, @view_name
		 *  -1- @table_catalog, @table_schema, @table_name, @column_name
		 */
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
		if (i == 0) {
			if (!PROV_CLASS (provider)->meta_funcs.view_cols) {
				WARN_METHOD_NOT_IMPLEMENTED (provider, "view_cols");
				break;
			}
			retval = PROV_CLASS (provider)->meta_funcs.view_cols (provider, cnc, store, context, error, 
									      catalog, schema, name);
			WARN_META_UPDATE_FAILURE (retval, "view_cols");
			return retval;
		}
		else {
			/* nothing to do */
			return TRUE;
		}
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

static GError *
suggest_update_cb_downstream (G_GNUC_UNUSED GdaMetaStore *store, GdaMetaContext *suggest, DownstreamCallbackData *data)
{
#define MAX_CONTEXT_SIZE 10
	if (data->error)
		return data->error;

	GdaMetaContext *templ_context;
	GdaMetaContext loc_suggest;
	gchar *column_names[MAX_CONTEXT_SIZE];
	GValue *column_values[MAX_CONTEXT_SIZE];

	/* if there is no context with the same table name in the templates, then exit right now */
	templ_context = g_hash_table_lookup (data->context_templates_hash, suggest->table_name);
	if (!templ_context)
		return NULL;
	
	if (templ_context->size > 0) {
		/* setup @loc_suggest */
		gint i, j;

		if (suggest->size > MAX_CONTEXT_SIZE) {
			g_warning ("Internal limitation at %s(), limitation should be at least %d, please report a bug",
				   __FUNCTION__, suggest->size);
			return NULL;
		}
		loc_suggest.size = suggest->size;
		loc_suggest.table_name = suggest->table_name;
		loc_suggest.column_names = column_names;
		loc_suggest.column_values = column_values;
		memcpy (loc_suggest.column_names, suggest->column_names, sizeof (gchar *) * suggest->size); /* Flawfinder: ignore */
		memcpy (loc_suggest.column_values, suggest->column_values, sizeof (GValue *) * suggest->size); /* Flawfinder: ignore */
		
		/* check that any @suggest's columns which is in @templ_context's has the same values */
		for (j = 0; j < suggest->size; j++) {
			for (i = 0; i < templ_context->size; i++) {
				if (!strcmp (templ_context->column_names[i], suggest->column_names[j])) {
					/* same column name, now check column value */
					if (G_VALUE_TYPE (templ_context->column_values[i]) != 
					    G_VALUE_TYPE (suggest->column_values[j])) {
						g_warning ("Internal error: column types mismatch for GdaMetaContext "
							   "table '%s' and column '%s' (%s/%s)",
							   templ_context->table_name, templ_context->column_names[i], 
							   g_type_name (G_VALUE_TYPE (templ_context->column_values[i])),
							   g_type_name (G_VALUE_TYPE (suggest->column_values[j])));
						return NULL;
					}
					if (gda_value_compare (templ_context->column_values[i], suggest->column_values[j]))
						/* different values */
						return NULL;
					break;
				}
			}
		}

		/* @templ_context may contain some more columns => add them to @loc_suggest */
		for (i = 0; i < templ_context->size; i++) {
			for (j = 0; j < suggest->size; j++) {
				if (!strcmp (templ_context->column_names[i], suggest->column_names[j])) {
					j = -1;
					break;
				}
			}
			if (j >= 0) {
				if (loc_suggest.size >= MAX_CONTEXT_SIZE) {
					g_warning ("Internal limitation at %s(), limitation should be at least %d, please report a bug",
						   __FUNCTION__, loc_suggest.size + 1);
					return NULL;
				}
				loc_suggest.column_names [loc_suggest.size] = templ_context->column_names [i];
				loc_suggest.column_values [loc_suggest.size] = templ_context->column_values [i];
				loc_suggest.size ++;
			}
		}

		suggest = &loc_suggest;
	}
	
	GError *lerror = NULL;
	if (!local_meta_update (data->prov, data->cnc, suggest, &lerror)) {
		if (lerror)
			data->error = lerror;
		else {
			g_set_error (&lerror,GDA_CONNECTION_ERROR,
				     GDA_CONNECTION_META_DATA_CONTEXT_ERROR,
				      "%s", _("Meta update error"));
			data->error = lerror;
		}

		return data->error;
	}

	return NULL;
}

/**
 * gda_connection_update_meta_store:
 * @cnc: a #GdaConnection object.
 * @context: (allow-none): description of which part of @cnc's associated #GdaMetaStore should be updated, or %NULL
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
 *   <listitem><para>One is only interrested in the list of views, and may request that only the information
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	gda_connection_lock ((GdaLockable*) cnc);

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	g_assert (store);

	/* unlock connection */
	gda_connection_unlock ((GdaLockable*) cnc);

	if (context) {
		GdaMetaContext *lcontext;
		GSList *list;
		GSList *up_templates;
		GSList *dn_templates;
		GError *lerror = NULL;

		lcontext = _gda_meta_store_validate_context (store, context, error);
		if (!lcontext)
			return FALSE;
		/* alter local context because "_tables" and "_views" always go together so only
		   "_tables" should be updated and providers should always update "_tables" and "_views"
		*/
		if (!strcmp (lcontext->table_name, "_views"))
			lcontext->table_name = "_tables";

		/* build context templates */
		up_templates = build_upstream_context_templates (store, lcontext, NULL, &lerror);
		if (!up_templates) {
			if (lerror) {
				g_propagate_error (error, lerror);
				return FALSE;
			}
		}
		dn_templates = build_downstream_context_templates (store, lcontext, NULL, &lerror);
		if (!dn_templates) {
			if (lerror) {
				g_propagate_error (error, lerror);
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
					
		gulong signal_id;
		DownstreamCallbackData cbd;
		gboolean retval = TRUE;
		
		cbd.prov = cnc->priv->provider_obj;
		cbd.cnc = cnc;
		cbd.error = NULL;
		cbd.context_templates = g_slist_concat (g_slist_append (up_templates, lcontext), dn_templates);
		cbd.context_templates_hash = g_hash_table_new (g_str_hash, g_str_equal);
		for (list = cbd.context_templates; list; list = list->next) 
			g_hash_table_insert (cbd.context_templates_hash, ((GdaMetaContext*)list->data)->table_name,
					     list->data);
		
		signal_id = g_signal_connect (store, "suggest-update",
					      G_CALLBACK (suggest_update_cb_downstream), &cbd);
		
		retval = local_meta_update (cnc->priv->provider_obj, cnc, 
					    (GdaMetaContext*) (cbd.context_templates->data), error);
		
		g_signal_handler_disconnect (store, signal_id);

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

		return retval;
	}
	else {
		typedef gboolean (*RFunc) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, 
					   GdaMetaContext *, GError **);
		typedef struct {
			gchar *table_name;
			gchar *func_name;
			RFunc  func;
		} RMeta;

		GdaMetaContext lcontext;
		gint nb = 0, i;
		RMeta rmeta[] = {
			{"_information_schema_catalog_name", "_info", NULL},
			{"_builtin_data_types", "_btypes", NULL},
			{"_udt", "_udt", NULL},
			{"_udt_columns", "_udt_cols", NULL},
			{"_enums", "_enums", NULL},
			{"_domains", "_domains", NULL},
			{"_domain_constraints", "_constraints_dom", NULL},
			{"_element_types", "_el_types", NULL},
			{"_collations", "_collations", NULL},
			{"_character_sets", "_character_sets", NULL},
			{"_schemata", "_schemata", NULL},
			{"_tables_views", "_tables_views", NULL},
			{"_columns", "_columns", NULL},
			{"_view_column_usage", "_view_cols", NULL},
			{"_table_constraints", "_constraints_tab", NULL},
			{"_referential_constraints", "_constraints_ref", NULL},
			{"_key_column_usage", "_key_columns", NULL},
			{"_check_column_usage", "_check_columns", NULL},
			{"_triggers", "_triggers", NULL},
			{"_routines", "_routines", NULL},
			{"_routine_columns", "_routine_col", NULL},
			{"_parameters", "_routine_par", NULL},
			{"_table_indexes", "_indexes_tab", NULL},
			{"_index_column_usage", "_index_cols", NULL}
		};
		GdaServerProvider *provider = cnc->priv->provider_obj;
		gboolean retval;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._info;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._btypes;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._udt;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._udt_cols;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._enums;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._domains;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._constraints_dom;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._el_types;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._collations;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._character_sets;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._schemata;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._tables_views;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._columns;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._view_cols;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._constraints_tab;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._constraints_ref;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._key_columns;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._check_columns;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._triggers;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._routines;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._routine_col;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._routine_par;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._indexes_tab;
		rmeta [nb++].func = PROV_CLASS (provider)->meta_funcs._index_cols;
		g_assert (nb == sizeof (rmeta) / sizeof (RMeta));

		if (! _gda_meta_store_begin_data_reset (store, error)) {
			return FALSE;
		}

		lcontext.size = 0;
		for (i = 0; i < nb; i++) {
			/*g_print ("TH %p %s(cnc=>%p store=>%p)\n", g_thread_self(), rmeta [i].func_name, cnc, store);*/
			if (! rmeta [i].func) 
				WARN_METHOD_NOT_IMPLEMENTED (provider, rmeta [i].func_name);
			else {
				lcontext.table_name = rmeta [i].table_name;
				if (!rmeta [i].func (provider, cnc, store, &lcontext, error)) {
					/*
					g_print ("TH %p CNC %p ERROR, prov=%p (%s)\n", g_thread_self(), cnc,
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
		}
		retval = _gda_meta_store_finish_data_reset (store, error);
		return retval;

	onerror:
		_gda_meta_store_cancel_data_reset (store, NULL);
		return FALSE;
	}
}

/*
 * predefined statements for meta store data retrieval
 */
typedef struct {
	GdaConnectionMetaType  meta_type;
	gint                   nb_filters;
	gchar                **filters;
} MetaKey;

static guint
meta_key_hash (gconstpointer key)
{
	return ((((MetaKey*) key)->meta_type) << 2) + ((MetaKey*) key)->nb_filters;
}

static gboolean
meta_key_equal (gconstpointer a, gconstpointer b)
{
	MetaKey* ak = (MetaKey*) a;
	MetaKey* bk = (MetaKey*) b;
	gint i;

	if ((ak->meta_type != bk->meta_type) ||
	    (ak->nb_filters != bk->nb_filters))
		return FALSE;
	for (i = 0; i < ak->nb_filters; i++) 
		if (strcmp (ak->filters[i], bk->filters[i]))
			return FALSE;

	return TRUE;
}

static GHashTable *
prepare_meta_statements_hash (void)
{
	GHashTable *h;
	MetaKey *key;
	GdaStatement *stmt;
	GdaSqlParser *parser = gda_sql_parser_new ();
	const gchar *sql;

	gchar **name_array = g_new (gchar *, 1);
	name_array[0] = "name";

	gchar **name_col_array = g_new (gchar *, 2);
	name_col_array[0] = "name";
	name_col_array[1] = "field_name";

	gchar **name_index_array = g_new (gchar *, 2);
	name_index_array[0] = "name";
	name_index_array[1] = "index_name";

	h = g_hash_table_new (meta_key_hash, meta_key_equal);
	
	/* GDA_CONNECTION_META_NAMESPACES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_NAMESPACES;
	sql = "SELECT schema_name, schema_owner, schema_internal FROM _schemata";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);
	
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_NAMESPACES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT schema_name, schema_owner, schema_internal FROM _schemata WHERE schema_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_TYPES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TYPES;
	sql = "SELECT short_type_name, gtype, comments, synonyms FROM _all_types WHERE NOT internal";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TYPES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT short_type_name, gtype, comments, synonyms FROM _all_types WHERE NOT internal AND short_type_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_TABLES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TABLES;
	sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type LIKE '%TABLE%' AND table_short_name != table_full_name";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TABLES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type LIKE '%TABLE%' AND table_short_name != table_full_name AND table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_VIEWS */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_VIEWS;
	sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t WHERE t.table_short_name != t.table_full_name";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_VIEWS;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t WHERE t.table_short_name != t.table_full_name AND table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_FIELDS */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_FIELDS;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT c.column_name, c.data_type, c.gtype, c.numeric_precision, c.numeric_scale, c.is_nullable AS 'Nullable', c.column_default, c.extra FROM _columns as c NATURAL JOIN _tables as t WHERE t.table_short_name=##name::string ORDER BY c.ordinal_position";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_FIELDS;
	key->nb_filters = 2;
	key->filters = name_col_array;
	sql = "SELECT c.column_name, c.data_type, c.gtype, c.numeric_precision, c.numeric_scale, c.is_nullable AS 'Nullable', c.column_default, c.extra FROM _columns as c NATURAL JOIN _tables as t WHERE t.table_short_name=##name::string AND c.column_name = ##field_name::string ORDER BY c.ordinal_position";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_INDEXES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_INDEXES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT i.table_name, i.table_schema, i.index_name, d.column_name, d.ordinal_position, i.index_type FROM _table_indexes as i INNER JOIN _index_column_usage as d ON (d.table_catalog = i.table_catalog AND d.table_schema = i.table_schema AND d.table_name = i.table_name) INNER JOIN _tables t ON (t.table_catalog = i.table_catalog AND t.table_schema = i.table_schema AND t.table_name = i.table_name) WHERE t.table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_INDEXES;
	key->nb_filters = 2;
	key->filters = name_index_array;
	sql = "SELECT i.table_name, i.table_schema, i.index_name, d.column_name, d.ordinal_position, i.index_type FROM _table_indexes as i INNER JOIN _index_column_usage as d ON (d.table_catalog = i.table_catalog AND d.table_schema = i.table_schema AND d.table_name = i.table_name) INNER JOIN _tables t ON (t.table_catalog = i.table_catalog AND t.table_schema = i.table_schema AND t.table_name = i.table_name) WHERE t.table_short_name=##name::string AND i.index_name=##index_name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	return h;
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
	GList* filters = NULL;
	GdaDataModel* model = NULL;
	gint i;
  
	if (nb_filters > 0) {
		va_list ap;
		va_start (ap, nb_filters);
		for (i = 0; (i < nb_filters); i++) {
			GdaHolder *h;
			GValue *v;
			gchar* fname;
      
			fname = va_arg (ap, gchar*);
			if (!fname)
				break;
			v = va_arg (ap, GValue*);
			if (!v || gda_value_is_null (v))
				continue;
			h = g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (v), "id", fname, NULL);
			filters = g_list_append (filters, h);
			if (!gda_holder_set_value (h, v, error)) {
				va_end (ap);
				goto onerror;
			}
		}
		va_end (ap);
	}
	model = gda_connection_get_meta_store_data_v (cnc, meta_type, filters, error);

 onerror:
	g_list_foreach (filters, (GFunc) g_object_unref, NULL);
	g_list_free (filters);

	return model;
}

/**
 * gda_connection_get_meta_store_data_v:
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
	static GHashTable *stmt_hash = NULL;
	GdaStatement *stmt;
	GdaSet *set = NULL;
	GList* node;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	g_assert (store);
	
	/* fetch the statement */
	MetaKey key;
	gint i;
	if (!stmt_hash)
		stmt_hash = prepare_meta_statements_hash ();
	key.meta_type = meta_type;
	key.nb_filters = g_list_length (filters);
	key.filters = NULL;
	if (key.nb_filters > 0)
		key.filters = g_new (gchar *, key.nb_filters);
	for (node = filters, i = 0; 
	     node; 
	     node = node->next, i++) {
		if (!set)
			set = g_object_new (GDA_TYPE_SET, NULL);
		gda_set_add_holder (set, GDA_HOLDER (node->data));
		key.filters[i] = (gchar*) gda_holder_get_id (GDA_HOLDER (node->data));
	}
	stmt = g_hash_table_lookup (stmt_hash, &key);
	g_free (key.filters);
	if (!stmt) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			      "%s", _("Wrong filter arguments"));
		if (set)
			g_object_unref (set);
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

	if (cnc->priv->events_list)
		return cnc->priv->events_list;
	

	/* a new list of the GdaConnectionEvent objects is created, the
	 * ownership of each GdaConnectionEvent object is transfered to the list */
	GList *list = NULL;
	if (cnc->priv->events_array_full) {
		gint i;
		for (i = cnc->priv->events_array_next + 1; ; i++) {
			if (i == cnc->priv->events_array_size)
				i = 0;
			if (i == cnc->priv->events_array_next)
				break;
			GdaConnectionEvent *ev;
			ev = cnc->priv->events_array [i];
			cnc->priv->events_array [i] = NULL;
			g_assert (ev);
			list = g_list_prepend (list, ev);
		}
	}
	else {
		gint i;
		for (i = 0; i < cnc->priv->events_array_next; i++) {
			GdaConnectionEvent *ev;
			ev = cnc->priv->events_array [i];
			g_assert (ev);
			list = g_list_prepend (list, ev);
			cnc->priv->events_array [i] = NULL;
		}
	}
	cnc->priv->events_list = g_list_reverse (list);

	/* reset events */
	cnc->priv->events_array_full = FALSE;
	cnc->priv->events_array_next = 0;

	return cnc->priv->events_list;
}

/**
 * gda_connection_value_to_sql_string
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
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	/* execute the command on the provider */
	return gda_server_provider_value_to_sql_string (cnc->priv->provider_obj, cnc, from);
}

/**
 * gda_connection_internal_transaction_started: (skip)
 * @cnc: a #GdaConnection
 * @parent_trans: (allow-none): name of the parent transaction, or %NULL
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

	st = gda_transaction_status_new (trans_name);
	st->isolation_level = isol_level;

	gda_connection_lock ((GdaLockable*) cnc);

	parent = gda_transaction_status_find (cnc->priv->trans_status, parent_trans, NULL);
	if (!parent)
		cnc->priv->trans_status = st;
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
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_transaction_rolledback: (skip)
 * @cnc: a #GdaConnection
 * @trans_name: (allow-none): transaction's name, or %NULL
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

	gda_connection_lock ((GdaLockable*) cnc);

	if (cnc->priv->trans_status)
		st = gda_transaction_status_find (cnc->priv->trans_status, trans_name, &ev);
	if (st) {
		if (ev) {
			/* there is a parent transaction */
			gda_transaction_status_free_events (ev->trans, ev, TRUE);
		}
		else {
			/* no parent transaction */
			g_object_unref (cnc->priv->trans_status);
			cnc->priv->trans_status = NULL;
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
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_transaction_committed: (skip)
 * @cnc: a #GdaConnection
 * @trans_name: (allow-none): transaction's name, or %NULL
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

	gda_connection_lock ((GdaLockable*) cnc);

	if (cnc->priv->trans_status)
		st = gda_transaction_status_find (cnc->priv->trans_status, trans_name, &ev);
	if (st) {
		if (ev) {
			/* there is a parent transaction */
			gda_transaction_status_free_events (ev->trans, ev, TRUE);
		}
		else {
			/* no parent transaction */
			g_object_unref (cnc->priv->trans_status);
			cnc->priv->trans_status = NULL;
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
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_savepoint_added: (skip)
 * @cnc: a #GdaConnection
 * @parent_trans: (allow-none): name of the parent transaction, or %NULL
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

	gda_connection_lock ((GdaLockable*) cnc);

	st = gda_transaction_status_find (cnc->priv->trans_status, parent_trans, NULL);
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
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_savepoint_rolledback: (skip)
 * @cnc: a #GdaConnection
 * @svp_name: (allow-none): savepoint's name, or %NULL
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

	gda_connection_lock ((GdaLockable*) cnc);

	st = gda_transaction_status_find (cnc->priv->trans_status, svp_name, &ev);
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
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif	

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_savepoint_removed: (skip)
 * @cnc: a #GdaConnection
 * @svp_name: (allow-none): savepoint's name, or %NULL
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

	gda_connection_lock ((GdaLockable*) cnc);

	st = gda_transaction_status_find (cnc->priv->trans_status, svp_name, &ev);
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
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_statement_executed: (skip)
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement which has been executed
 * @params: (allow-none): execution's parameters
 * @error: a #GdaConnectionEvent if the execution failed, or %NULL
 *
 * Internal functions to be called by database providers when a statement has been executed
 * to keep track of the transaction status of the connection
 */
void 
gda_connection_internal_statement_executed (GdaConnection *cnc, GdaStatement *stmt,
					    G_GNUC_UNUSED GdaSet *params, GdaConnectionEvent *error)
{
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

			if (cnc->priv->trans_status)
				st = gda_transaction_status_find_current (cnc->priv->trans_status, NULL, FALSE);
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
			if (cnc->priv->trans_status)
				gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
			gda_connection_unlock ((GdaLockable*) cnc);
			break;
		}
		}
	}
}

/**
 * gda_connection_internal_change_transaction_state: (skip)
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

	gda_connection_lock ((GdaLockable*) cnc);

	g_return_if_fail (cnc->priv->trans_status);

	if (cnc->priv->trans_status->state == newstate)
		return;

	cnc->priv->trans_status->state = newstate;
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
 * gda_connection_internal_reset_transaction_status: (skip)
 * @cnc: a #GdaConnection
 *
 * Internal function to be called by database providers to reset the transaction status.
 */
void
gda_connection_internal_reset_transaction_status (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->trans_status) {
		g_object_unref (cnc->priv->trans_status);
		cnc->priv->trans_status = NULL;
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
static void statement_weak_notify_cb (GdaConnection *cnc, GdaStatement *stmt);

static void 
prepared_stmts_stmt_reset_cb (GdaStatement *gda_stmt, GdaConnection *cnc)
{
	gda_connection_lock ((GdaLockable*) cnc);

	g_signal_handlers_disconnect_by_func (gda_stmt, G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);
	g_object_weak_unref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
	g_assert (cnc->priv->prepared_stmts);
	g_hash_table_remove (cnc->priv->prepared_stmts, gda_stmt);

	gda_connection_unlock ((GdaLockable*) cnc);
}

static void
prepared_stms_foreach_func (GdaStatement *gda_stmt, G_GNUC_UNUSED GdaPStmt *prepared_stmt, GdaConnection *cnc)
{
	g_signal_handlers_disconnect_by_func (gda_stmt, G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);
	g_object_weak_unref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
}

static void
statement_weak_notify_cb (GdaConnection *cnc, GdaStatement *stmt)
{
	gda_mutex_lock (cnc->priv->mutex);

	g_assert (cnc->priv->prepared_stmts);
	g_hash_table_remove (cnc->priv->prepared_stmts, stmt);

	gda_mutex_unlock (cnc->priv->mutex);
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

	gda_connection_lock ((GdaLockable*) cnc);

	if (!cnc->priv->prepared_stmts)
		cnc->priv->prepared_stmts = g_hash_table_new_full (g_direct_hash, g_direct_equal,
								   NULL, g_object_unref);
	g_hash_table_remove (cnc->priv->prepared_stmts, gda_stmt);
	g_hash_table_insert (cnc->priv->prepared_stmts, gda_stmt, g_object_ref (prepared_stmt));
	
	/* destroy the prepared statement if gda_stmt is destroyed, or changes */
	g_object_weak_ref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
	g_signal_connect (G_OBJECT (gda_stmt), "reset",
			  G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);

	gda_connection_unlock ((GdaLockable*) cnc);
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

	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->prepared_stmts) 
		retval = g_hash_table_lookup (cnc->priv->prepared_stmts, gda_stmt);
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
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	gda_connection_lock ((GdaLockable*) cnc);
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	if (gda_connection_get_prepared_statement (cnc, gda_stmt))
		prepared_stmts_stmt_reset_cb (gda_stmt, cnc);
	gda_connection_unlock ((GdaLockable*) cnc);
}

/*
 * Provider's specific connection data management
 */

/**
 * gda_connection_internal_set_provider_data: (skip)
 * @cnc: a #GdaConnection object
 * @data: an opaque structure, known only to the provider for which @cnc is opened
 * @destroy_func: function to call when the connection closes and @data needs to be destroyed
 *
 * Note: calling this function more than once will not make it call @destroy_func on any previously
 * set opaque @data, you'll have to do it yourself.
 */
void 
gda_connection_internal_set_provider_data (GdaConnection *cnc, gpointer data, GDestroyNotify destroy_func)
{

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	gda_connection_lock ((GdaLockable*) cnc);
	cnc->priv->provider_data = data;
	cnc->priv->provider_data_destroy_func = destroy_func;
	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_get_provider_data: (skip)
 * @cnc: a #GdaConnection object
 *
 * Get the opaque pointer previously set using gda_connection_internal_set_provider_data().
 * If it's not set, then add a connection event and returns %NULL
 *
 * Returns: (allow-none): the pointer to the opaque structure set using gda_connection_internal_set_provider_data(), or %NULL
 */
gpointer
gda_connection_internal_get_provider_data (GdaConnection *cnc)
{
	return gda_connection_internal_get_provider_data_error (cnc, NULL);
}

/**
 * gda_connection_internal_get_provider_data_error: (skip)
 * @cnc: a #GdaConnection object
 * @error: (allow-none): a place to store errors, or %NULL
 *
 * Get the opaque pointer previously set using gda_connection_internal_set_provider_data().
 * If it's not set, then add a connection event and returns %NULL
 *
 * Returns: (allow-none): the pointer to the opaque structure set using gda_connection_internal_set_provider_data(), or %NULL
 *
 * Since: 5.0.2
 */
gpointer
gda_connection_internal_get_provider_data_error (GdaConnection *cnc, GError **error)
{
	gpointer retval;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	retval = cnc->priv->provider_data;
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
 * Returns: (transfer full): a #GdaMetaStore object
 */
GdaMetaStore *
gda_connection_get_meta_store (GdaConnection *cnc)
{
	GdaMetaStore *store = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	g_mutex_lock (cnc->priv->object_mutex);
	if (!cnc->priv->meta_store) {
		ThreadConnectionData *cdata = NULL;
		if (cnc->priv->is_thread_wrapper) {
			cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
			if (cdata && cdata->sub_connection->priv->meta_store) {
				cnc->priv->meta_store = g_object_ref (cdata->sub_connection->priv->meta_store);
				store = cnc->priv->meta_store;
			}
		}
		if (!store) {
			cnc->priv->meta_store = gda_meta_store_new (NULL);
			if (cnc->priv->is_thread_wrapper)
				cdata->sub_connection->priv->meta_store = 
					g_object_ref (cnc->priv->meta_store);
		}
	}
	store = cnc->priv->meta_store;
	g_mutex_unlock (cnc->priv->object_mutex);
	
	return store;
}

/*
 * This method is useful only in a multi threading environment (it has no effect in a
 * single thread program).
 * Locks @cnc for the current thread. If the lock can't be obtained, then the current thread
 * will be blocked until it can acquire @cnc's lock. 
 *
 * The cases when the connection can't be locked are:
 * <itemizedlist>
 *   <listitem><para>another thread is already using the connection</para></listitem>
 *   <listitem><para>the connection can only be used by a single thread (see the 
 *     <link linkend="GdaConnection--thread-owner">thread-owner</link> property)</para></listitem>
 * </itemizedlist>
 *
 * To avoid the thread being blocked (possibly forever if the single thread which can use the
 * connection is not the current thead), then it is possible to use gda_connection_trylock() instead.
 */
static void
gda_connection_lock (GdaLockable *lockable)
{
	GdaConnection *cnc = (GdaConnection *) lockable;
	
	gda_mutex_lock (cnc->priv->mutex);
	if (cnc->priv->unique_possible_thread && 
	    (cnc->priv->unique_possible_thread != g_thread_self ())) {
		gda_mutex_unlock (cnc->priv->mutex);
		g_mutex_lock (cnc->priv->object_mutex);

		if (!cnc->priv->unique_possible_cond)
			cnc->priv->unique_possible_cond = g_cond_new ();

		while (1) {
			if (cnc->priv->unique_possible_thread &&
			    (cnc->priv->unique_possible_thread != g_thread_self ())) {
#ifdef GDA_DEBUG_CNC_LOCK
				g_print ("Wainting th %p, now %p (cond %p, mutex%p)\n", g_thread_self(),
					 cnc->priv->unique_possible_thread, cnc->priv->unique_possible_cond,
					 cnc->priv->object_mutex);
#endif
				g_cond_wait (cnc->priv->unique_possible_cond,
					     cnc->priv->object_mutex);
			}
			else if (gda_mutex_trylock (cnc->priv->mutex)) {
				g_mutex_unlock (cnc->priv->object_mutex);
				break;
			}
		}
	}
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
	gboolean retval;
	GdaConnection *cnc = (GdaConnection *) lockable;

	retval = gda_mutex_trylock (cnc->priv->mutex);
	if (retval && cnc->priv->unique_possible_thread &&
	    (cnc->priv->unique_possible_thread != g_thread_self ())) {
		retval = FALSE;
		gda_mutex_unlock (cnc->priv->mutex);
	}
	return retval;
}

/*
 * Unlocks @cnc's usage. Any other thread blocked (after having called gda_connection_lock()) gets
 * the opportunity to lock the connection.
 */
static void
gda_connection_unlock  (GdaLockable *lockable)
{
	GdaConnection *cnc = (GdaConnection *) lockable;
	
	gda_mutex_unlock (cnc->priv->mutex);
}

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
		current = get_next_word (next, FALSE, &next);
		if (current && (!strcmp (current, "TABLE") || !strcmp (current, "VIEW")))
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
			tmp = gda_sql_identifier_quote (tname, cnc, cnc->priv->provider_obj,
							TRUE,
							cnc->priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
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
				tmp = gda_sql_identifier_quote (tname, cnc, cnc->priv->provider_obj,
								TRUE,
								cnc->priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
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
	if (! cnc->priv->meta_store ||
	    ! (cnc->priv->options & GDA_CONNECTION_OPTIONS_AUTO_META_DATA))
		return;

	GdaSqlStatementType type;
	type = gda_statement_get_statement_type (stmt);
	if (type == GDA_SQL_STATEMENT_BEGIN) {
		/* initialize cnc->priv->trans_meta_context if meta store's connection is not @cnc */
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (cnc->priv->meta_store);
		if (cnc != mscnc) {
			g_assert (! cnc->priv->trans_meta_context);
			cnc->priv->trans_meta_context = g_array_new (FALSE, FALSE, sizeof (GdaMetaContext*));
		}
		return;
	}
	else if (type == GDA_SQL_STATEMENT_ROLLBACK) {
		/* re-run all the meta store updates started since the BEGIN */
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (cnc->priv->meta_store);
		if (cnc != mscnc) {
			gsize i;
			g_assert (cnc->priv->trans_meta_context);
			for (i = 0; i < cnc->priv->trans_meta_context->len; i++) {
				GdaMetaContext *context;
				GError *lerror = NULL;
				context = g_array_index (cnc->priv->trans_meta_context, GdaMetaContext*, i);
				if (! gda_connection_update_meta_store (cnc, context, &lerror))
					add_connection_event_from_error (cnc, &lerror);
				auto_update_meta_context_free (context);
			}
			g_array_free (cnc->priv->trans_meta_context, TRUE);
			cnc->priv->trans_meta_context = NULL;
		}
		return;
	}
	else if (type == GDA_SQL_STATEMENT_COMMIT) {
		/* get rid of the meta store updates */
		GdaConnection *mscnc;
		mscnc = gda_meta_store_get_internal_connection (cnc->priv->meta_store);
		if (cnc != mscnc) {
			gsize i;
			g_assert (cnc->priv->trans_meta_context);
			for (i = 0; i < cnc->priv->trans_meta_context->len; i++) {
				GdaMetaContext *context;
				context = g_array_index (cnc->priv->trans_meta_context, GdaMetaContext*, i);
				auto_update_meta_context_free (context);
			}
			g_array_free (cnc->priv->trans_meta_context, TRUE);
			cnc->priv->trans_meta_context = NULL;
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
			
			if (cnc->priv->trans_meta_context)
				g_array_prepend_val (cnc->priv->trans_meta_context, context);
			else
				auto_update_meta_context_free (context);
		}
	}
	g_slist_free (clist);
}

void
_gda_connection_signal_meta_table_update (GdaConnection *cnc, const gchar *table_name)
{
	if (! cnc->priv->meta_store ||
	    ! (cnc->priv->options & GDA_CONNECTION_OPTIONS_AUTO_META_DATA))
		return;

	GdaMetaContext *context;
	gchar *tmp;
	/*g_print ("CONTEXT: update for table [%s]\n", tname);*/
	context = g_new0 (GdaMetaContext, 1);
	context->table_name = "_tables";
	context->size = 1;
	context->column_names = g_new0 (gchar *, 1);
	context->column_names[0] = "table_name";
	context->column_values = g_new0 (GValue *, 1);
	tmp = gda_sql_identifier_quote (table_name, cnc, cnc->priv->provider_obj,
					TRUE,
					cnc->priv->options & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
	g_value_take_string ((context->column_values[0] = gda_value_new (G_TYPE_STRING)),
			     tmp);

	GError *lerror = NULL;
	if (! gda_connection_update_meta_store (cnc, context, &lerror))
		add_connection_event_from_error (cnc, &lerror);
	
	if (cnc->priv->trans_meta_context)
		g_array_prepend_val (cnc->priv->trans_meta_context, context);
	else
		auto_update_meta_context_free (context);
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
	gchar *tmp;
	GString *string = NULL;
	gint i;

	g_assert (table_name && *table_name);
	array = gda_sql_identifier_split (table_name);
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
