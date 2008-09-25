/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#undef GDA_DISABLE_DEPRECATED
#include <stdio.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-private.h>
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

#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

struct _GdaConnectionPrivate {
	GdaServerProvider    *provider_obj;
	GdaConnectionOptions  options; /* ORed flags */
	gchar                *dsn;
	gchar                *cnc_string;
	gchar                *auth_string;
	gboolean              is_open;

	GdaMetaStore         *meta_store;

	gboolean              auto_clear_events_list; /* TRUE if events_list is cleared before any statement execution */
	GList                *events_list; /* last event is stored as the first node */

	GdaTransactionStatus *trans_status;
	GHashTable           *prepared_stmts;

	gpointer              provider_data;
	GDestroyNotify        provider_data_destroy_func;

	/* multi threading locking */
	GThread              *unique_possible_thread; /* non NULL => only that thread can use this connection */
	GCond                *unique_possible_cond;
	GMutex               *unique_possible_mutex;
	GdaMutex             *mutex;
};

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
static void                 gda_connection_lockable_init (GdaLockableClass *iface);
static void                 gda_connection_lock      (GdaLockable *lockable);
static gboolean             gda_connection_trylock   (GdaLockable *lockable);
static void                 gda_connection_unlock    (GdaLockable *lockable);


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
	PROP_THREAD_OWNER
};

static GObjectClass *parent_class = NULL;

/*
 * GdaConnection class implementation
 * @klass:
 */
static void
gda_connection_class_init (GdaConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_connection_signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_CONNECTION_EVENT);
	gda_connection_signals[CONN_OPENED] =
                g_signal_new ("conn-opened",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_opened),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        gda_connection_signals[CONN_TO_CLOSE] =
                g_signal_new ("conn-to-close",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_to_close),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        gda_connection_signals[CONN_CLOSED] =    /* runs after user handlers */
                g_signal_new ("conn-closed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_closed),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_connection_signals[DSN_CHANGED] =
		g_signal_new ("dsn-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, dsn_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
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
                                         g_param_spec_flags ("options", NULL, _("Options (connection sharing)"),
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

	
	object_class->dispose = gda_connection_dispose;
	object_class->finalize = gda_connection_finalize;
}

static void
gda_connection_lockable_init (GdaLockableClass *iface)
{
	iface->i_lock = gda_connection_lock;
	iface->i_trylock = gda_connection_trylock;
	iface->i_unlock = gda_connection_unlock;
}

static void
gda_connection_init (GdaConnection *cnc, GdaConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaConnectionPrivate, 1);
	cnc->priv->unique_possible_thread = NULL;
	cnc->priv->unique_possible_cond = NULL;
	cnc->priv->unique_possible_mutex = NULL;
	cnc->priv->mutex = gda_mutex_new ();
	cnc->priv->provider_obj = NULL;
	cnc->priv->dsn = NULL;
	cnc->priv->cnc_string = NULL;
	cnc->priv->auth_string = NULL;
	cnc->priv->is_open = FALSE;
	cnc->priv->auto_clear_events_list = TRUE;
	cnc->priv->events_list = NULL;
	cnc->priv->trans_status = NULL; /* no transaction yet */
	cnc->priv->prepared_stmts = NULL;
}

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

	if (cnc->priv->trans_status) {
		g_object_unref (cnc->priv->trans_status);
		cnc->priv->trans_status = NULL;
	}

	if (cnc->priv->meta_store != NULL) {
	        g_object_unref (cnc->priv->meta_store);
	        cnc->priv->meta_store = NULL;
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
	if (cnc->priv->unique_possible_mutex)
		g_mutex_free (cnc->priv->unique_possible_mutex);
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
 * gda_connection_get_type
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
			(GInstanceInitFunc) gda_connection_init
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
			gda_mutex_lock (cnc->priv->mutex);
			cnc->priv->unique_possible_thread = g_value_get_pointer (value);
			if (cnc->priv->unique_possible_cond) 
				g_cond_signal (cnc->priv->unique_possible_cond);
			gda_mutex_unlock (cnc->priv->mutex);
			break;
                case PROP_DSN: {
			const gchar *datasource = g_value_get_string (value);
			GdaDsnInfo *dsn;

			gda_connection_lock ((GdaLockable*) cnc);
			if (cnc->priv->is_open) {
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
			if (cnc->priv->is_open) {
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
			if (cnc->priv->is_open) {
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
			if (cnc->priv->is_open) {
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
                case PROP_OPTIONS:
			gda_mutex_lock (cnc->priv->mutex);
			if (cnc->priv->is_open) {
				g_warning (_("Could not set the '%s' property when the connection is opened"),
					   pspec->name);
				gda_connection_unlock ((GdaLockable*) cnc);
				return;
			}
			cnc->priv->options = g_value_get_flags (value);
			gda_mutex_unlock (cnc->priv->mutex);
			break;
		case PROP_META_STORE:
			gda_mutex_lock (cnc->priv->mutex);
			if (cnc->priv->meta_store) {
				g_object_unref (cnc->priv->meta_store);
				cnc->priv->meta_store = NULL;
			}
			cnc->priv->meta_store = g_value_get_object (value);
			if (cnc->priv->meta_store)
				g_object_ref (cnc->priv->meta_store);
			gda_mutex_unlock (cnc->priv->mutex);
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
		gda_mutex_lock (cnc->priv->mutex);
                switch (param_id) {
                case PROP_DSN:
			g_value_set_string (value, cnc->priv->dsn);
                        break;
                case PROP_CNC_STRING:
			g_value_set_string (value, cnc->priv->cnc_string);
			break;
                case PROP_PROVIDER_OBJ:
			g_value_set_object (value, G_OBJECT (cnc->priv->provider_obj));
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
                }
		gda_mutex_unlock (cnc->priv->mutex);
        }	
}

/**
 * gda_connection_open_from_dsn
 * @dsn: data source name.
 * @auth_string: authentication string, or %NULL
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
 * provider (use gda_config_get_provider_info() to get it). Also one can use the "gda-sql-4.0 -L" command to 
 * list the possible named parameters.
 *
 * Returns: a new #GdaConnection if connection opening was sucessfull or %NULL if there was an error.
 */
GdaConnection *
gda_connection_open_from_dsn (const gchar *dsn, const gchar *auth_string, 
			      GdaConnectionOptions options, GError **error)
{
	GdaConnection *cnc = NULL;
	GdaDsnInfo *dsn_info;
	gchar *user, *pass, *real_dsn;
	gchar *real_auth_string = NULL;

	g_return_val_if_fail (dsn && *dsn, NULL);
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
		GdaServerProvider *prov;

		prov = gda_config_get_provider (dsn_info->provider, error);
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
			
			/* open connection */
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
	}
	else 
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_NOT_FOUND_ERROR, 
			     _("Datasource configuration error: no provider specified"));

	g_free (real_auth_string);
	g_free (real_dsn);
	g_free (user);
	g_free (pass);
	return cnc;
}

/**
 * gda_connection_open_from_string
 * @provider_name: provider ID to connect to, or %NULL
 * @cnc_string: connection string.
 * @auth_string: authentication string, or %NULL
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
 * The possible keys depend on the provider, the "gda-sql-4.0 -L" command
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
 * string, use the "gda-sql-4.0 -L" command to list the possible named parameters.
 *
 * Additionally, it is possible to have the connection string
 * respect the "&lt;provider_name&gt;://&lt;real cnc string&gt;" format, in which case the provider name
 * and the real connection string will be extracted from that string (note that if @provider_name
 * is not %NULL then it will still be used as the provider ID).
 *
 * Returns: a new #GdaConnection if connection opening was sucessfull or %NULL if there was an error.
 */
GdaConnection *
gda_connection_open_from_string (const gchar *provider_name, const gchar *cnc_string, const gchar *auth_string,
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
			     _("No provider specified"));
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
		GdaServerProvider *prov;

		prov = gda_config_get_provider (provider_name ? provider_name : real_provider, error);
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
			
			/* open the connection */
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
	}	

	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);

	return cnc;
}


/**
 * gda_connection_open
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
	g_return_val_if_fail (cnc->priv, FALSE);

	/* don't do anything if connection is already opened */
	if (cnc->priv->is_open)
		return TRUE;

	gda_connection_lock ((GdaLockable*) cnc);

	/* connection string */
	if (cnc->priv->dsn) {
		/* get the data source info */
		dsn_info = gda_config_get_dsn_info (cnc->priv->dsn);
		if (!dsn_info) {
			gda_log_error (_("Data source %s not found in configuration"), cnc->priv->dsn);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NONEXIST_DSN_ERROR,
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
				     _("No DSN or connection string specified"));
			gda_connection_unlock ((GdaLockable*) cnc);
			return FALSE;
		}
		/* try to see if connection string has the <provider>://<rest of the string> format */
	}

	/* provider test */
	if (!cnc->priv->provider_obj) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
			     _("No provider specified"));
		gda_connection_unlock ((GdaLockable*) cnc);
		return FALSE;
	}

	if (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread &&
	    (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread != g_thread_self ())) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_ERROR,
			     _("Provider does not allow usage from this thread"));
		gda_connection_unlock ((GdaLockable*) cnc);
		return FALSE;
	}

	if (!PROV_CLASS (cnc->priv->provider_obj)->open_connection) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_ERROR,
			     _("Internal error: provider does not implement the open_connection() virtual method"));
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

	if (PROV_CLASS (cnc->priv->provider_obj)->open_connection (cnc->priv->provider_obj, cnc, params, auth,
								   NULL, NULL, NULL))
		cnc->priv->is_open = TRUE;
	else {
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
							     gda_connection_event_get_description (event));
				}
			}
		}

		cnc->priv->is_open = FALSE;
	}

	/* free memory */
	gda_quark_list_free (params);
	gda_quark_list_free (auth);
	g_free (real_auth_string);

	if (cnc->priv->is_open) {
#ifdef GDA_DEBUG_signal
		g_print (">> 'CONN_OPENED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_OPENED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'CONN_OPENED' from %s\n", __FUNCTION__);
#endif
	}

	/* limit connection's useable thread */
	if (PROV_CLASS (cnc->priv->provider_obj)->limiting_thread)
		g_object_set (G_OBJECT (cnc), "thread-owner", 
			      PROV_CLASS (cnc->priv->provider_obj)->limiting_thread, NULL);


	gda_connection_unlock ((GdaLockable*) cnc);
	return cnc->priv->is_open;
}


/**
 * gda_connection_close
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source, but first emits the 
 * "conn-to-close" signal.
 */
void
gda_connection_close (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

	if (! cnc->priv->is_open)
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

/**
 * gda_connection_close_no_warning
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source, without emiting any warning signal.
 */
void
gda_connection_close_no_warning (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

	gda_connection_lock ((GdaLockable*) cnc);
	if (! cnc->priv->is_open) {
		gda_connection_unlock ((GdaLockable*) cnc);
		return;
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
	cnc->priv->is_open = FALSE;

	if (cnc->priv->provider_data) {
		if (cnc->priv->provider_data_destroy_func)
			cnc->priv->provider_data_destroy_func (cnc->priv->provider_data);
		else
			g_warning ("Provider did not clean its connection data");
		cnc->priv->provider_data = NULL;
	}

#ifdef GDA_DEBUG_signal
        g_print (">> 'CONN_CLOSED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_CLOSED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'CONN_CLOSED' from %s\n", __FUNCTION__);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}


/**
 * gda_connection_is_opened
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

	return cnc->priv->is_open;
}


/**
 * gda_connection_get_options
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
	g_return_val_if_fail (cnc->priv, -1);

	return cnc->priv->options;
}

/**
 * gda_connection_get_provider
 * @cnc: a #GdaConnection object
 *
 * Get a pointer to the #GdaServerProvider object used to access the database
 *
 * Returns: the #GdaServerProvider (NEVER NULL)
 */
GdaServerProvider *
gda_connection_get_provider (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return cnc->priv->provider_obj;
}

/**
 * gda_connection_get_provider_name
 * @cnc: a #GdaConnection object
 *
 * Get the name (identifier) of the database provider used by @cnc
 *
 * Returns: a non modifiable string
 */
const gchar *
gda_connection_get_provider_name (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!cnc->priv->provider_obj)
		return NULL;

	return gda_server_provider_get_name (cnc->priv->provider_obj);
}

/**
 * gda_connection_get_dsn
 * @cnc: a #GdaConnection object
 *
 * Returns the data source name the connection object is connected
 * to.
 */
const gchar *
gda_connection_get_dsn (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->dsn;
}

/**
 * gda_connection_get_cnc_string
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
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->cnc_string;
}

/**
 * gda_connection_get_authentication
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
	g_return_val_if_fail (cnc->priv, NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->auth_string) 
		str = (const gchar *) cnc->priv->auth_string;
	else 
		str = "";
	gda_connection_unlock ((GdaLockable*) cnc);
	return str;
}

/**
 * gda_connection_add_event
 * @cnc: a #GdaConnection object.
 * @event: is stored internally, so you don't need to unref it.
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
	static gint debug = -1;
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

	gda_connection_lock ((GdaLockable*) cnc);
	if (debug == -1) {
		const gchar *str;
		debug = 0;
		str = getenv ("GDA_CONNECTION_EVENTS_SHOW");
		if (str) {
			gchar **array;
			gint i;
			array = g_strsplit_set (str, " ,/;:", 0);
			for (i = 0; i < g_strv_length (array); i++) {
				if (!g_ascii_strcasecmp (array[i], "notice"))
					debug += 1;
				else if (!g_ascii_strcasecmp (array[i], "warning"))
					debug += 2;
				else if (!g_ascii_strcasecmp (array[i], "error"))
					debug += 4;
				else if (!g_ascii_strcasecmp (array[i], "command"))
					debug += 8;
			}
			g_strfreev (array);
		}
	}

	cnc->priv->events_list = g_list_prepend (cnc->priv->events_list, event);

	if (debug > 0) {
		const gchar *str = NULL;
		switch (gda_connection_event_get_event_type (event)) {
		case GDA_CONNECTION_EVENT_NOTICE:
			if (debug & 1) str = "NOTICE";
			break;
		case GDA_CONNECTION_EVENT_WARNING:
			if (debug & 2) str = "WARNING";
			break;
		case GDA_CONNECTION_EVENT_ERROR:
			if (debug & 4) str = "ERROR";
			break;
		case GDA_CONNECTION_EVENT_COMMAND:
			if (debug & 8) str = "COMMAND";
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
	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_add_event_string
 * @cnc: a #GdaConnection object.
 * @str: a format string (see the printf(3) documentation).
 * @...: the arguments to insert in the error message.
 *
 * Adds a new error to the given connection object. This is just a convenience
 * function that simply creates a #GdaConnectionEvent and then calls
 * #gda_server_connection_add_error.
 *
 * Returns: a new #GdaConnectionEvent object, however the caller does not hold a reference to the returned
 * object, and if need be the caller must call g_object_ref() on it.
 */
GdaConnectionEvent *
gda_connection_add_event_string (GdaConnection *cnc, const gchar *str, ...)
{
	GdaConnectionEvent *error;

	va_list args;
	gchar sz[2048];

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (str != NULL, NULL);

	/* build the message string */
	va_start (args, str);
	vsprintf (sz, str, args);
	va_end (args);
	
	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_description (error, sz);
	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, gda_connection_get_provider_name (cnc));
	gda_connection_event_set_sqlstate (error, "-1");
	
	gda_connection_add_event (cnc, error);

	return error;
}

static void
_clear_events_list (GdaConnection *cnc)
{
	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->events_list != NULL) {
		g_list_foreach (cnc->priv->events_list, (GFunc) g_object_unref, NULL);
		g_list_free (cnc->priv->events_list);
		cnc->priv->events_list =  NULL;
	}
	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_clear_events_list
 * @cnc: a #GdaConnection object.
 *
 * This function lets you clear the list of #GdaConnectionEvent's of the
 * given connection. 
 */
void
gda_connection_clear_events_list (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	_clear_events_list (cnc);
}

/**
 * gda_connection_create_operation
 * @cnc: a #GdaConnection object
 * @type: the type of operation requested
 * @options: an optional list of parameters
 * @error: a place to store an error, or %NULL
 *
 * Creates a new #GdaServerOperation object which can be modified in order 
 * to perform the type type of action. It is a wrapper around the gda_server_provider_create_operation()
 * method.
 *
 * Returns: a new #GdaServerOperation object, or %NULL in the connection's provider does not support the @type type
 * of operation or if an error occurred
 */
GdaServerOperation*
gda_connection_create_operation (GdaConnection *cnc, GdaServerOperationType type, 
                                 GdaSet *options, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	return gda_server_provider_create_operation (cnc->priv->provider_obj, cnc, type, options, error);
}

/**
 * gda_connection_perform_operation
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
	gboolean auto_clear_events, retval;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	auto_clear_events = cnc->priv->auto_clear_events_list;
	cnc->priv->auto_clear_events_list = FALSE;
	retval = gda_server_provider_perform_operation (cnc->priv->provider_obj, cnc, op, error);
	cnc->priv->auto_clear_events_list = auto_clear_events;
	return retval;
}

/**
 * gda_connection_create_parser
 * @cnc: a #GdaConnection object
 *
 * Creates a new parser object able to parse the SQL dialect understood by @cnc. 
 * If the #GdaServerProvider object internally used by @cnc does not have its own parser, 
 * then %NULL is returned, and a general SQL parser can be obtained
 * using gda_sql_parser_new().
 *
 * Returns: a new #GdaSqlParser object, or %NULL
 */
GdaSqlParser *
gda_connection_create_parser (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	return gda_server_provider_create_parser (cnc->priv->provider_obj, cnc);
}

/**
 * gda_connection_batch_execute
 * @cnc: a #GdaConnection object
 * @batch: a #GdaBatch object which contains all the statements to execute
 * @params: a #GdaSet object (which can be obtained using gda_batch_get_parameters()), or %NULL
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
 * Returns: a list of #GObject objects
 */
GSList *
gda_connection_batch_execute (GdaConnection *cnc, GdaBatch *batch, GdaSet *params,
			      GdaStatementModelUsage model_usage, GError **error)
{
	GSList *retlist = NULL, *stmt_list;
	gboolean auto_clear_events;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (GDA_IS_BATCH (batch), NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	auto_clear_events = cnc->priv->auto_clear_events_list;
	cnc->priv->auto_clear_events_list = FALSE;
	for (stmt_list = (GSList*) gda_batch_get_statements (batch); stmt_list; stmt_list = stmt_list->next) {
		GObject *obj;
		obj = gda_connection_statement_execute (cnc, GDA_STATEMENT (stmt_list->data), params,
							model_usage, NULL, error);
		if (!obj)
			break;
		retlist = g_slist_prepend (retlist, obj);
	}
	cnc->priv->auto_clear_events_list = auto_clear_events;
	gda_connection_unlock ((GdaLockable*) cnc);
	
	return g_slist_reverse (retlist);
}


/**
 * gda_connection_statement_to_sql
 * @cnc: a #GdaConnection object
 * @stmt: a #GdaStatement object
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @flags: SQL rendering flags, as #GdaStatementSqlFlag OR'ed values
 * @params_used: a place to store the list of individual #GdaHolder objects within @params which have been used
 * @error: a place to store errors, or %NULL
 *
 * Renders @stmt as an SQL statement, adapted to the SQL dialect used by @cnc
 *
 * Returns: a new string, or %NULL if an error occurred
 */
gchar *
gda_connection_statement_to_sql (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				 GSList **params_used, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
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
 * gda_connection_statement_prepare
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @error: a place to store errors, or %NULL
 *
 * Ask the database accessed through the @cnc connection to prepare the usage of @stmt. This is only usefull
 * if @stmt will be used more than once (however some database providers may always prepare stamements 
 * before executing them).
 *
 * This function is also usefull to make sure @stmt is fully understood by the database before actually executing it.
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->statement_prepare)
		return (PROV_CLASS (cnc->priv->provider_obj)->statement_prepare)(cnc->priv->provider_obj, 
										 cnc, stmt, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     _("Provider does not support statement preparation"));
		return FALSE;
	}
}

static GType *
make_col_types_array (gint init_size, va_list args)
{
	GType *types;
	gint max = 10;
	gint col;

	types = g_new (GType, max + 1);
	for (col = 0; col <= max; col ++)
		types[col] = G_TYPE_NONE;
	for (col = va_arg (args, gint); col >= 0; col = va_arg (args, gint)) {
		if (col > max) {
			gint i;
			types = g_renew (GType, types, col + 5 + 1);
			for (i = max; col <= col + 5; i ++)
				types[i] = G_TYPE_NONE;
			max = col + 5;
		}
		types [col] = va_arg (args, GType);
	}
	return types;
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
	GType *types;
	va_start (ap, error);
	types = make_col_types_array (10, ap);
	va_end (ap);

	gda_connection_lock ((GdaLockable*) cnc);
	if (last_inserted_row) 
		*last_inserted_row = NULL;
	if (cnc->priv->auto_clear_events_list)
		_clear_events_list (cnc);
	obj = PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, cnc, stmt, params, 
								       model_usage, types, last_inserted_row, 
								       NULL, NULL, NULL, error);
	g_free (types);
	gda_connection_unlock ((GdaLockable*) cnc);
	return obj;
}


/**
 * gda_connection_statement_execute
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: in the case where @stmt is a SELECT statement, specifies how the returned data model will be used
 * @last_insert_row: a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Executes @stmt. 
 *
 * As @stmt can, by desing (and if not abused), contain only one SQL statement, the
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
 * Also see the <link linkend="limitations">provider's limitations</link> section.
 *
 * Returns: a #GObject, or %NULL if an error occurred 
 */
GObject *
gda_connection_statement_execute (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
				  GdaStatementModelUsage model_usage, GdaSet **last_inserted_row, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	if (last_inserted_row) 
		*last_inserted_row = NULL;

	return gda_connection_statement_execute_v (cnc, stmt, params, model_usage, last_inserted_row, error, -1);
}


/**
 * gda_connection_statement_execute_non_select
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @last_insert_row: a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store an error, or %NULL
 *
 * Executes a non-selection statement on the given connection. The gda_execute_non_select_command() method can be easier
 * to use if one prefers to use some SQL directly.
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
	g_return_val_if_fail (cnc->priv, -1);
	g_return_val_if_fail (cnc->priv->provider_obj, -1);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), -1);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, -1);

	if ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
	    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is a selection statement"));
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
			     _("Statement is a selection statement"));
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
 * gda_connection_statement_execute_select
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @error: a place to store an error, or %NULL
 *
 * Executes a selection command on the given connection. The gda_execute_select_command() method can be easier
 * to use if one prefers to use some SQL directly.
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
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select (GdaConnection *cnc, GdaStatement *stmt,
					 GdaSet *params, GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	model = (GdaDataModel *) gda_connection_statement_execute_v (cnc, stmt, params, 
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL,
								     error, -1);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_statement_execute_select_fullv
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @error: a place to store an error, or %NULL
 * @...: a (-1 terminated) list of (column number, GType) specifying for each column mentionned the GType
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
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select_fullv (GdaConnection *cnc, GdaStatement *stmt,
					       GdaSet *params, GdaStatementModelUsage model_usage,
					       GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	va_list ap;
	GdaDataModel *model;
	GType *types;
	
	va_start (ap, error);
	types = make_col_types_array (10, ap);
	va_end (ap);

	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->auto_clear_events_list)
		_clear_events_list (cnc);
	model = (GdaDataModel *) PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, 
											  cnc, stmt, params, model_usage, 
											  types, NULL, NULL, 
											  NULL, NULL, error);
	gda_connection_unlock ((GdaLockable*) cnc);
	g_free (types);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_statement_execute_select_full
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @col_types: an array of GType to request each returned #GdaDataModel's column's GType, terminated with the G_TYPE_NONE
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
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select_full (GdaConnection *cnc, GdaStatement *stmt,
					      GdaSet *params, GdaStatementModelUsage model_usage,
					      GType *col_types, GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->auto_clear_events_list)
		_clear_events_list (cnc);
	model = (GdaDataModel *) PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, 
											  cnc, stmt, params, 
											  model_usage, col_types, NULL, 
											  NULL, NULL, NULL, error);
	gda_connection_unlock ((GdaLockable*) cnc);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_begin_transaction
 * @cnc: a #GdaConnection object.
 * @name: the name of the transation to start, or %NULL
 * @level:
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->begin_transaction)
		return PROV_CLASS (cnc->priv->provider_obj)->begin_transaction (cnc->priv->provider_obj, cnc, name, level, error);
	else
		return FALSE;
}

/**
 * gda_connection_commit_transaction
 * @cnc: a #GdaConnection object.
 * @name: the name of the transation to commit, or %NULL
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->commit_transaction)
		return PROV_CLASS (cnc->priv->provider_obj)->commit_transaction (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_rollback_transaction
 * @cnc: a #GdaConnection object.
 * @name: the name of the transation to commit, or %NULL
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->rollback_transaction)
		return PROV_CLASS (cnc->priv->provider_obj)->rollback_transaction (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_add_savepoint
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to add
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	
	if (PROV_CLASS (cnc->priv->provider_obj)->add_savepoint)
		return PROV_CLASS (cnc->priv->provider_obj)->add_savepoint (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_rollback_savepoint
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to rollback to
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);	

	if (PROV_CLASS (cnc->priv->provider_obj)->rollback_savepoint)
		return PROV_CLASS (cnc->priv->provider_obj)->rollback_savepoint (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_delete_savepoint
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to delete
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->delete_savepoint)
		return PROV_CLASS (cnc->priv->provider_obj)->delete_savepoint (cnc->priv->provider_obj, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_connection_get_transaction_status
 * @cnc: a #GdaConnection object
 *
 * Get the status of @cnc regarding transactions. The returned object should not be modified
 * or destroyed; however it may be modified or destroyed by the connection itself.
 *
 * If %NULL is returned, then no transaction has been associated with @cnc
 *
 * Returns: a #GdaTransactionStatus object, or %NULL
 */
GdaTransactionStatus *
gda_connection_get_transaction_status (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return cnc->priv->trans_status;
}

/**
 * gda_connection_supports_feature
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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	return gda_server_provider_supports_feature (cnc->priv->provider_obj, cnc, feature);
}

/* builds a list of #GdaMetaContext contexts templates: contexts which have a non NULL table_name,
 * and empty or partially filled column names and values specifications */
GSList *
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
GSList *
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

#ifdef GDA_DEBUG_NO
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
				g_set_error (error, 0, 0,
					     _("Invalid argument"));
				retval = -1;
			}
		}
	}
	else {
		gchar *str;
		str = meta_context_stringify (context);
		g_set_error (error, 0, 0,
			     _("Missing or wrong arguments for table '%s': %s"), context->table_name, str);
		g_free (str);
	}

	/*g_print ("Check arguments context => found %d\n", retval);*/
	return retval;
}

static gboolean
local_meta_update (GdaServerProvider *provider, GdaConnection *cnc, GdaMetaContext *context, GError **error)
{
#ifdef GDA_DEBUG
#define ASSERT_TABLE_NAME(x,y) g_assert (!strcmp ((x), (y)))
#define WARN_METHOD_NOT_IMPLEMENTED(prov,method) g_warning ("Provider '%s' does not implement the META method '%s()', please report the error to bugzilla.gnome.org", gda_server_provider_get_name (prov), (method))
#define WARN_META_UPDATE_FAILURE(x,method) if (!(x)) g_print ("%s (meta method => %s) ERROR: %s\n", __FUNCTION__, (method), error && *error && (*error)->message ? (*error)->message : "???")
#else
#define ASSERT_TABLE_NAME(x,y)
#define WARN_METHOD_NOT_IMPLEMENTED(prov,method)
#define WARN_META_UPDATE_FAILURE(x,method)
#endif
	const gchar *tname = context->table_name;
	GdaMetaStore *store;
	gboolean retval;

	const GValue *catalog = NULL;
	const GValue *schema = NULL;
	const GValue *name = NULL;
	gint i;


#ifdef GDA_DEBUG_NO
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
		if ((tname[1] == 'r') && (tname[2] == 'i')) {
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
suggest_update_cb_downstream (GdaMetaStore *store, GdaMetaContext *suggest, DownstreamCallbackData *data)
{
#define MAX_CONTEXT_SIZE 10
	if (data->error)
		return data->error;

	GdaMetaContext *templ_context;
	GdaMetaContext loc_suggest;

	/* if there is no context with the same table name in the templates, then exit right now */
	templ_context = g_hash_table_lookup (data->context_templates_hash, suggest->table_name);
	if (!templ_context)
		return NULL;
	
	if (templ_context->size > 0) {
		/* setup @loc_suggest */

		gchar *column_names[MAX_CONTEXT_SIZE];
		GValue *column_values[MAX_CONTEXT_SIZE];
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
		memcpy (loc_suggest.column_names, suggest->column_names, sizeof (gchar *) * suggest->size);
		memcpy (loc_suggest.column_values, suggest->column_values, sizeof (GValue *) * suggest->size);	
		
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
			g_set_error (&lerror, 0, 0,
				     _("Meta update error"));
			data->error = lerror;
		}

		return data->error;
	}

	return NULL;
}

/**
 * gda_connection_update_meta_store
 * @cnc: a #GdaConnection object.
 * @context: description of which part of @cnc's associated #GdaMetaStore should be updated, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Updates @cnc's associated #GdaMetaStore. If @context is not %NULL, then only the parts described by
 * @context will be updated, and if it is %NULL, then the complete meta store will be updated.
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

	/* prepare local context */
	GdaMetaContext lcontext;

	if (context) {
		GSList *list;
		GSList *up_templates;
		GSList *dn_templates;
		GError *lerror = NULL;
		lcontext = *context;
		/* alter local context because "_tables" and "_views" always go together so only
		   "_tables" should be updated and providers should always update "_tables" and "_views"
		*/
		if (!strcmp (lcontext.table_name, "_views"))
			lcontext.table_name = "_tables";

		up_templates = build_upstream_context_templates (store, context, NULL, &lerror);
		if (!up_templates) {
			if (lerror) {
				g_propagate_error (error, lerror);
				gda_connection_unlock ((GdaLockable*) cnc);
				return FALSE;
			}
		}
		dn_templates = build_downstream_context_templates (store, context, NULL, &lerror);
		if (!dn_templates) {
			if (lerror) {
				g_propagate_error (error, lerror);
				gda_connection_unlock ((GdaLockable*) cnc);
				return FALSE;
			}
		}

#ifdef GDA_DEBUG_NO
		g_print ("\n*********** TEMPLATES:\n");
		for (list = up_templates; list; list = list->next) {
			g_print ("UP: ");
			meta_context_dump ((GdaMetaContext*) list->data);
		}
		g_print ("->: ");
		meta_context_dump (context);
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
		cbd.context_templates = g_slist_concat (g_slist_append (up_templates, context), dn_templates);
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
			if (c != context) {
				if (c->size > 0) {
					g_free (c->column_names);
					g_free (c->column_values);
				}
				g_free (c);
			}
		}
		g_slist_free (cbd.context_templates);
		g_hash_table_destroy (cbd.context_templates_hash);

		gda_connection_unlock ((GdaLockable*) cnc);
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
			{"_parameters", "_routine_par", NULL}
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
		g_assert (nb == sizeof (rmeta) / sizeof (RMeta));

		if (! _gda_meta_store_begin_data_reset (store, error)) {
			gda_connection_unlock ((GdaLockable*) cnc);
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
					g_print ("TH %p CNC %p ERROR, prov=%p (%s)\n", g_thread_self(), cnc,
						 gda_connection_get_provider (cnc),
						 gda_connection_get_provider_name (cnc));
					if (error && *error)
						g_warning ("// %s\n", (*error)->message);

					WARN_META_UPDATE_FAILURE (FALSE, rmeta [i].func_name);
					goto onerror;
				}
			}
		}
		retval = _gda_meta_store_finish_data_reset (store, error);
		gda_connection_unlock ((GdaLockable*) cnc);
		return retval;

	onerror:
		_gda_meta_store_cancel_data_reset (store, NULL);
		gda_connection_unlock ((GdaLockable*) cnc);
		return FALSE;
	}
}

/*
 * predefined statements for meta store data retreival
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
	sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type='BASE TABLE'";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TABLES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type='BASE TABLE' AND table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_VIEWS */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_VIEWS;
	sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_VIEWS;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t WHERE table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_FIELDS */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_FIELDS;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT c.column_name, c.data_type, c.gtype, c.numeric_precision, c.numeric_scale, c.is_nullable AS 'Nullable', c.column_default, c.extra FROM _columns as c NATURAL JOIN _tables as t WHERE t.table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	

	return h;
}

/**
 * gda_connection_get_meta_store_data
 * @cnc: a #GdaConnection object.
 * @meta_type: describes which data to get.
 * @error: a place to store errors, or %NULL
 * @nb_filters: the number of filters in the @... argument
 * @...: a list of (filter name (gchar *), filter value (GValue*)) pairs specifying
 * the filter to apply to the returned data model's contents (there must be @nb_filters pairs)
 *
 * Retreives data stored in @cnc's associated #GdaMetaStore object. This method is usefull
 * to easily get some information about the meta-data associated to @cnc, such as the list of
 * tables, views, and other database objects.
 *
 * Note: it's up to the caller to make sure the information contained within @cnc's associated #GdaMetaStore
 * is up to date using gda_connection_update_meta_store() (it can become outdated if the database's schema
 * is accessed from outside of Libgda).
 *
 * For more information about the returned data model's attributes, or about the @meta_type and @... filter arguments,
 * see <link linkend="GdaConnectionMetaTypeHead">this description</link>.
 * 
 * Returns: a #GdaDataModel containing the data required. The caller is responsible
 * for freeing the returned model using g_object_unref().
 */
GdaDataModel *
gda_connection_get_meta_store_data (GdaConnection *cnc,
				    GdaConnectionMetaType meta_type,
				    GError **error, gint nb_filters, ...)
{
	GdaMetaStore *store;
	GdaDataModel *model = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
	static GHashTable *stmt_hash = NULL;
	GdaStatement *stmt;
	GdaSet *set = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	g_assert (store);
	
	/* fetch the statement */
	MetaKey key;
	gint i;
	gchar *fname;

	g_static_mutex_lock (&mutex);
	if (!stmt_hash)
		stmt_hash = prepare_meta_statements_hash ();
	g_static_mutex_unlock (&mutex);

	key.meta_type = meta_type;
	key.nb_filters = nb_filters;
	key.filters = NULL;
	if (nb_filters > 0) {
		va_list ap;
		key.filters = g_new (gchar *, nb_filters);
		va_start (ap, nb_filters);
		for (i = 0; (i < nb_filters); i++) {
			GdaHolder *h;
			GValue *v;
			
			fname = va_arg (ap, gchar*);
			if (!fname)
				break;
			v = va_arg (ap, GValue*);
			if (!v || gda_value_is_null (v))
				continue;
			if (!set)
				set = gda_set_new (NULL);
			h = g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (v), "id", fname, NULL);
			if (! gda_holder_set_value (h, v, error)) {
				g_free (key.filters);
				g_object_unref (set);
				return NULL;
			}
			gda_set_add_holder (set, h);
			g_object_unref (h);
			key.filters[i] = fname;
		}
		va_end (ap);
	}
	stmt = g_hash_table_lookup (stmt_hash, &key);
	g_free (key.filters);
	if (!stmt) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			     _("Wrong filter arguments"));
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
 * gda_connection_get_events
 * @cnc: a #GdaConnection.
 *
 * Retrieves a list of the last errors occurred during the connection. The returned list is
 * chronologically ordered such as that the most recent event is the #GdaConnectionEvent of the first node.
 *
 * Warning: the @cnc object may change the list if connection events occur
 *
 * Returns: a GList of #GdaConnectionEvent objects (the list should not be modified)
 */
const GList *
gda_connection_get_events (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, FALSE);

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
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	/* execute the command on the provider */
	return gda_server_provider_value_to_sql_string (cnc->priv->provider_obj, cnc, from);
}

/*
 * Internal functions to keep track
 * of the transactional status of the connection
 */
void
gda_connection_internal_transaction_started (GdaConnection *cnc, const gchar *parent_trans, const gchar *trans_name, 
					     GdaTransactionIsolation isol_level)
{
	GdaTransactionStatus *parent, *st;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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

void 
gda_connection_internal_transaction_rolledback (GdaConnection *cnc, const gchar *trans_name)
{
	GdaTransactionStatus *st = NULL;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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
		gchar *str;
		str = g_strdup_printf (_("Connection transaction status tracking: no transaction exists for %s"), "ROLLBACK");
		g_warning (str);
		g_free (str);
	}
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

void 
gda_connection_internal_transaction_committed (GdaConnection *cnc, const gchar *trans_name)
{
	GdaTransactionStatus *st = NULL;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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
		gchar *str;
		str = g_strdup_printf (_("Connection transaction status tracking: no transaction exists for %s"), "COMMIT");
		g_warning (str);
		g_free (str);
	}
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

void
gda_connection_internal_savepoint_added (GdaConnection *cnc, const gchar *parent_trans, const gchar *svp_name)
{
	GdaTransactionStatus *st;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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
		gchar *str;
		str = g_strdup_printf (_("Connection transaction status tracking: no transaction exists for %s"), "ADD SAVEPOINT");
		g_warning (str);
		g_free (str);
	}
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

void
gda_connection_internal_savepoint_rolledback (GdaConnection *cnc, const gchar *svp_name)
{
	GdaTransactionStatus *st;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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
		gchar *str;
		str = g_strdup_printf (_("Connection transaction status tracking: no transaction exists for %s"), "ROLLBACK SAVEPOINT");
		g_warning (str);
		g_free (str);
	}
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif	

	gda_connection_unlock ((GdaLockable*) cnc);
}

void
gda_connection_internal_savepoint_removed (GdaConnection *cnc, const gchar *svp_name)
{
	GdaTransactionStatus *st;
	GdaTransactionStatusEvent *ev = NULL;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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
		gchar *str;
		str = g_strdup_printf (_("Connection transaction status tracking: no transaction exists for %s"), "REMOVE SAVEPOINT");
		g_warning (str);
		g_free (str);
	}
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif

	gda_connection_unlock ((GdaLockable*) cnc);
}

void 
gda_connection_internal_statement_executed (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, GdaConnectionEvent *error)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

	if (!error || (error && (gda_connection_event_get_event_type (error) != GDA_CONNECTION_EVENT_ERROR))) {
		GdaSqlStatement *sqlst;
		GdaSqlStatementTransaction *trans;
		g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
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
		gda_sql_statement_free (sqlst);
	}
}

void
gda_connection_internal_change_transaction_state (GdaConnection *cnc,
						  GdaTransactionStatusState newstate)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

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
prepared_stms_foreach_func (GdaStatement *gda_stmt, GdaPStmt *prepared_stmt, GdaConnection *cnc)
{
	g_signal_handlers_disconnect_by_func (gda_stmt, G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);
	g_object_weak_unref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
}

static void
statement_weak_notify_cb (GdaConnection *cnc, GdaStatement *stmt)
{
	gda_connection_lock ((GdaLockable*) cnc);

	g_assert (cnc->priv->prepared_stmts);
	g_hash_table_remove (cnc->priv->prepared_stmts, stmt);

	gda_connection_unlock ((GdaLockable*) cnc);
}


/**
 * gda_connection_add_prepared_statement
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
	g_return_if_fail (cnc->priv);
	g_return_if_fail (GDA_IS_STATEMENT (gda_stmt));
	g_return_if_fail (GDA_IS_PSTMT (prepared_stmt));

	gda_connection_lock ((GdaLockable*) cnc);

	if (!cnc->priv->prepared_stmts)
		cnc->priv->prepared_stmts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	g_hash_table_remove (cnc->priv->prepared_stmts, gda_stmt);
	g_hash_table_insert (cnc->priv->prepared_stmts, gda_stmt, prepared_stmt);
	g_object_ref (prepared_stmt);
	
	/* destroy the prepared statement if gda_stmt is destroyed, or changes */
	g_object_weak_ref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
	g_signal_connect (G_OBJECT (gda_stmt), "reset", G_CALLBACK (prepared_stmts_stmt_reset_cb), cnc);

	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_get_prepared_statement
 * @cnc: a #GdaConnection object
 * @gda_stmt: a #GdaStatement object
 *
 * Retreives a pointer to an object representing a prepared statement for @gda_stmt within @cnc. The
 * association must have been done using gda_connection_add_prepared_statement().
 *
 * Returns: the prepared statement, or %NULL if no association exists
 */
GdaPStmt *
gda_connection_get_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt)
{
	GdaPStmt *retval = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	if (cnc->priv->prepared_stmts) 
		retval = g_hash_table_lookup (cnc->priv->prepared_stmts, gda_stmt);
	gda_connection_unlock ((GdaLockable*) cnc);

	return retval;
}

/**
 * gda_connection_del_prepared_statement
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
	g_return_if_fail (cnc->priv);

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
 * gda_connection_internal_set_provider_data
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
	g_return_if_fail (cnc->priv);

	gda_connection_lock ((GdaLockable*) cnc);
	cnc->priv->provider_data = data;
	cnc->priv->provider_data_destroy_func = destroy_func;
	gda_connection_unlock ((GdaLockable*) cnc);
}

/**
 * gda_connection_internal_get_provider_data
 * @cnc: a #GdaConnection object
 *
 * Get the opaque pointer previously set using gda_connection_internal_set_provider_data().
 * If it's not set, then add a connection event and returns %NULL
 *
 * Returns: the pointer to the opaque structure set using gda_connection_internal_set_provider_data()
 */
gpointer
gda_connection_internal_get_provider_data (GdaConnection *cnc)
{
	gpointer retval;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	
	if (! cnc->priv->provider_data)
		gda_connection_add_event_string (cnc, _("Internal error: invalid provider handle"));
	retval = cnc->priv->provider_data;

	gda_connection_unlock ((GdaLockable*) cnc);
	return retval;
}

/**
 * gda_connection_get_meta_store
 * @cnc: a #GdaConnection object
 *
 * Get or initializes the #GdaMetaStore associated to @cnc
 *
 * Returns: a #GdaMetaStore object
 */
GdaMetaStore *
gda_connection_get_meta_store (GdaConnection *cnc)
{
	GdaMetaStore *store;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	gda_connection_lock ((GdaLockable*) cnc);
	if (!cnc->priv->meta_store)
		cnc->priv->meta_store = gda_meta_store_new (NULL);
	store = cnc->priv->meta_store;
	gda_connection_unlock ((GdaLockable*) cnc);
	
	return store;
}

/*
 * This method is usefull only in a multi threading environment (it has no effect in a
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
	g_return_if_fail (cnc->priv);
	
	gda_mutex_lock (cnc->priv->mutex);
	if (cnc->priv->unique_possible_thread && 
	    (cnc->priv->unique_possible_thread != g_thread_self ())) {
		if (!cnc->priv->unique_possible_mutex)
			cnc->priv->unique_possible_mutex = g_mutex_new ();
		if (!cnc->priv->unique_possible_cond)
			cnc->priv->unique_possible_cond = g_cond_new ();

		g_mutex_lock (cnc->priv->unique_possible_mutex);
		gda_mutex_unlock (cnc->priv->mutex);

		g_cond_wait (cnc->priv->unique_possible_cond, cnc->priv->unique_possible_mutex);
		while (1) {
			if (cnc->priv->unique_possible_thread &&
			    (cnc->priv->unique_possible_thread != g_thread_self ())) {
				g_cond_signal (cnc->priv->unique_possible_cond);
				g_cond_wait (cnc->priv->unique_possible_cond, cnc->priv->unique_possible_mutex);
			}
			else {
				g_mutex_unlock (cnc->priv->unique_possible_mutex);
				gda_mutex_lock (cnc->priv->mutex);
				break;
			}
		}
	}
}

/*
 * Tries to lock @cnc for the exclusive usage of the current thread, as gda_connection_lock(), except
 * that if it can't, then the calling thread is not locked by it simply returns FALSE.
 *
 * Returns: TRUE if sucessfully locked, or FALSE if lock could not be acquired
 */
static gboolean
gda_connection_trylock (GdaLockable *lockable)
{
	gboolean retval;
	GdaConnection *cnc = (GdaConnection *) lockable;

	g_return_val_if_fail (cnc->priv, FALSE);
	
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
	g_return_if_fail (cnc->priv);
	
	gda_mutex_unlock (cnc->priv->mutex);
}
