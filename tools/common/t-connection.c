/*
 * Copyright (C) 2014 - 2015 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "t-errors.h"
#include "t-connection.h"
#include "t-app.h"
#include "t-context.h"
#include "common-marshal.h"
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-sql-builder.h>
#include <libgda-ui/gdaui-enums.h>
#include "t-config-info.h"
#include "t-virtual-connection.h"
#include <virtual/gda-virtual-connection.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-enum-types.h>
#ifdef G_OS_WIN32
  #include "windows.h"
#else
  #include <termios.h>
#endif
#include <unistd.h>

#define CHECK_RESULTS_SHORT_TIMER 200
#define CHECK_RESULTS_LONG_TIMER 2

struct _TConnectionPrivate {
        GHashTable       *executed_statements; /* key = guint exec ID, value = a StatementResult pointer */

        gulong            meta_store_signal;
        gulong            transaction_status_signal;

        gchar         *name;
	gchar         *query_buffer;
        GdaConnection *cnc;
        gchar         *dict_file_name;
        GdaSqlParser  *parser;

        GdaDsnInfo     dsn_info;
        GdaMetaStruct *mstruct;

        TFavorites    *bfav;

        gboolean       busy;
        gchar         *busy_reason;

        GdaConnection *store_cnc;

        GdaSet        *variables;
	GdaSet        *infos;
};


/* signals */
enum {
	BUSY,
	STATUS_CHANGED,
	META_CHANGED,
	FAV_CHANGED,
	TRANSACTION_STATUS_CHANGED,
	TABLE_COLUMN_PREF_CHANGED,
	NOTICE,
	LAST_SIGNAL
};

gint t_connection_signals [LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0, 0 };

/* 
 * Main static functions 
 */
static void t_connection_class_init (TConnectionClass *klass);
static void t_connection_init (TConnection *tcnc);
static void t_connection_dispose (GObject *object);
static void t_connection_set_property (GObject *object,
				       guint param_id,
				       const GValue *value,
				       GParamSpec *pspec);
static void t_connection_get_property (GObject *object,
				       guint param_id,
				       GValue *value,
				       GParamSpec *pspec);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum {
        PROP_0,
        PROP_GDA_CNC,
	PROP_NAME
};

GType
t_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (TConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) t_connection_class_init,
			NULL,
			NULL,
			sizeof (TConnection),
			0,
			(GInstanceInitFunc) t_connection_init,
			0
		};

		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "TConnection", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
t_connection_class_init (TConnectionClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	t_connection_signals [BUSY] =
		g_signal_new ("busy",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, busy),
                              NULL, NULL,
                              _marshal_VOID__BOOLEAN_STRING, G_TYPE_NONE,
                              2, G_TYPE_BOOLEAN, G_TYPE_STRING);
	t_connection_signals [META_CHANGED] =
		g_signal_new ("meta-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, meta_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                              1, GDA_TYPE_META_STRUCT);
	t_connection_signals [STATUS_CHANGED] =
		g_signal_new ("status-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, status_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__ENUM, G_TYPE_NONE,
                              1, GDA_TYPE_CONNECTION_STATUS);
	t_connection_signals [FAV_CHANGED] =
		g_signal_new ("favorites-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, favorites_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);
	t_connection_signals [TRANSACTION_STATUS_CHANGED] =
		g_signal_new ("transaction-status-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, transaction_status_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);
	t_connection_signals [TABLE_COLUMN_PREF_CHANGED] =
		g_signal_new ("table-column-pref-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, table_column_pref_changed),
                              NULL, NULL,
			      _marshal_VOID__POINTER_POINTER_STRING_STRING, G_TYPE_NONE,
                              4, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);
	t_connection_signals [NOTICE] =
		g_signal_new ("notice",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TConnectionClass, notice),
                              NULL, NULL,
                              _marshal_VOID__STRING, G_TYPE_NONE,
                              1, G_TYPE_STRING);

	klass->busy = NULL;
	klass->meta_changed = NULL;
	klass->favorites_changed = NULL;
	klass->transaction_status_changed = NULL;
	klass->table_column_pref_changed = NULL;

	/* Properties */
        object_class->set_property = t_connection_set_property;
        object_class->get_property = t_connection_get_property;
	g_object_class_install_property (object_class, PROP_GDA_CNC,
                                         g_param_spec_object ("gda-connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_NAME,
                                         g_param_spec_string ("name", NULL, "Connection's name",
                                                              NULL,
                                                              G_PARAM_WRITABLE | G_PARAM_READABLE));

	object_class->dispose = t_connection_dispose;
}

static void
t_connection_init (TConnection *tcnc)
{
	static guint index = 1;
	tcnc->priv = g_new0 (TConnectionPrivate, 1);
	tcnc->priv->executed_statements = NULL;

	tcnc->priv->name = g_strdup_printf (T_CNC_NAME_PREFIX "%u", index++);
	tcnc->priv->cnc = NULL;
	tcnc->priv->parser = NULL;
	tcnc->priv->variables = NULL;
	memset (&(tcnc->priv->dsn_info), 0, sizeof (GdaDsnInfo));
	tcnc->priv->mstruct = NULL;

	tcnc->priv->meta_store_signal = 0;
	tcnc->priv->transaction_status_signal = 0;

	tcnc->priv->bfav = NULL;

	tcnc->priv->store_cnc = NULL;

	tcnc->priv->variables = NULL;
	tcnc->priv->query_buffer = NULL;
	tcnc->priv->infos = gda_set_new (NULL);
  tcnc->priv->dict_file_name = NULL;
}

/* static void */
/* transaction_status_changed_cb (GdaConnection *cnc, TConnection *tcnc) */
/* { */
/* 	g_object_ref (tcnc); */
/* 	g_signal_emit (tcnc, t_connection_signals [TRANSACTION_STATUS_CHANGED], 0); */
/* 	g_object_unref (tcnc); */
/* } */

/*
 * executed in main thread
 */
static void
meta_changed_cb (G_GNUC_UNUSED GdaMetaStore *store, G_GNUC_UNUSED const GSList *changes, TConnection *tcnc)
{
	GError *error = NULL;
	GdaMetaStruct *mstruct;

	mstruct = gda_meta_store_create_struct (gda_connection_get_meta_store (tcnc->priv->cnc),
				       GDA_META_STRUCT_FEATURE_ALL);

	t_connection_set_busy_state (tcnc, TRUE, _("Analysing database schema"));

	gboolean compl;
	compl = gda_meta_struct_complement_all (mstruct, &error);

	t_connection_set_busy_state (tcnc, FALSE, NULL);

	if (compl) {
		if (tcnc->priv->mstruct)
			g_object_unref (tcnc->priv->mstruct);
		tcnc->priv->mstruct = mstruct;

#ifdef GDA_DEBUG_NO
		GSList *all, *list;
		g_print ("%s() %p:\n", __FUNCTION__, tcnc->priv->mstruct);
		all = gda_meta_struct_get_all_db_objects (tcnc->priv->mstruct);
		for (list = all; list; list = list->next) {
			GdaMetaDbObject *dbo = (GdaMetaDbObject *) list->data;
			g_print ("DBO, Type %d: short=>[%s] schema=>[%s] full=>[%s]\n", dbo->obj_type,
				 dbo->obj_short_name, dbo->obj_schema, dbo->obj_full_name);
		}
		g_slist_free (all);
#endif
	}
	else {
		g_object_unref (mstruct);
		gchar *tmp;
		tmp = g_strdup_printf (_("Error while fetching meta data from the connection: %s"),
				       error->message ? error->message : _("No detail"));
		g_clear_error (&error);
		g_signal_emit (tcnc, t_connection_signals [NOTICE], 0, tmp);
		g_free (tmp);
	}
}

/**
 * t_connection_meta_data_changed
 * @tcnc: a #TConnection
 *
 * Call this function if the meta data has been changed directly (ie. for example after
 * declaring or undeclaring a foreign key). This call creates a new #GdaMetaStruct internally used.
 */
void
t_connection_meta_data_changed (TConnection *tcnc)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	meta_changed_cb (NULL, NULL, tcnc);
}

/*
 * executed in tcnc->priv->wrapper's thread
 */
static gboolean
have_meta_store_ready (TConnection *tcnc, GError **error)
{
	GFile *dict_file = NULL;
	gboolean update_store = TRUE;
	GdaMetaStore *store;
	gchar *cnc_string, *cnc_info;

	g_object_get (G_OBJECT (tcnc->priv->cnc),
		      "dsn", &cnc_info,
		      "cnc-string", &cnc_string, NULL);
	dict_file = t_config_info_compute_dict_file_name (cnc_info ? gda_config_get_dsn_info (cnc_info) : NULL,
							       cnc_string);
 	g_free (cnc_string);
	g_message ("Dictionary File: %s", g_file_get_path (dict_file));
	store = gda_meta_store_new_with_file (g_file_get_path (dict_file));
	if (store != NULL) {
		GdaHolder *h;
		h = gda_set_get_holder (tcnc->priv->infos, "meta_filename");
		if (!h) {
			h = gda_holder_new (G_TYPE_STRING, "meta_filename");
			g_object_set (h,
				      "description", _("File containing the meta data associated to the connection"), NULL);
			gda_set_add_holder (tcnc->priv->infos, h);
		}
		g_assert (gda_holder_set_value_str (h, NULL, g_file_get_path (dict_file), NULL));
	}
	else {
		store = gda_meta_store_new (NULL);
	}


	t_config_info_update_meta_store_properties (store, tcnc->priv->cnc);

	tcnc->priv->dict_file_name = g_file_get_path (dict_file);
	g_object_unref (dict_file);
	g_object_set (G_OBJECT (tcnc->priv->cnc), "meta-store", store, NULL);
	if (update_store) {
		gboolean retval;
		g_message ("Updating meta store for connection");
		GdaMetaContext context = {"_tables", 0, NULL, NULL};
		retval = gda_connection_update_meta_store (tcnc->priv->cnc, &context, error);
		if (!retval) {
			g_object_unref (store);
			return FALSE;
		}
	}

	gboolean retval = TRUE;
	GdaMetaStruct *mstruct;
	mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", store, "features", GDA_META_STRUCT_FEATURE_ALL, NULL);
	if (tcnc->priv->mstruct != NULL)
		g_object_unref (tcnc->priv->mstruct = mstruct);
	tcnc->priv->mstruct = mstruct;
	retval = gda_meta_struct_complement_all (mstruct, error);

//#ifdef GDA_DEBUG_NO
	GSList *all, *list;
	g_print ("%s() %p:\n", __FUNCTION__, tcnc->priv->mstruct);
	all = gda_meta_struct_get_all_db_objects (tcnc->priv->mstruct);
	for (list = all; list; list = list->next) {
		GdaMetaDbObject *dbo = (GdaMetaDbObject *) list->data;
		g_print ("DBO, Type %d: short=>[%s] schema=>[%s] full=>[%s]\n", dbo->obj_type,
			 dbo->obj_short_name, dbo->obj_schema, dbo->obj_full_name);
	}
	g_slist_free (all);
//#endif
	g_object_unref (store);
	return retval;
}

static void
cnc_status_changed_cb (GdaConnection *cnc, GdaConnectionStatus status, TConnection *tcnc)
{
	g_object_ref (tcnc);
	g_signal_emit (tcnc, t_connection_signals [STATUS_CHANGED], 0, status);
	g_object_unref (tcnc);
}

static void
t_connection_set_property (GObject *object,
			   guint param_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
        TConnection *tcnc;

        tcnc = T_CONNECTION (object);
        if (tcnc->priv) {
                switch (param_id) {
                case PROP_GDA_CNC:
                        tcnc->priv->cnc = (GdaConnection*) g_value_get_object (value);
                        if (!tcnc->priv->cnc)
				return;

			g_object_ref (tcnc->priv->cnc);
			g_object_set (G_OBJECT (tcnc->priv->cnc), "execution-timer", TRUE, NULL);
			g_signal_connect (tcnc->priv->cnc, "status-changed",
					  G_CALLBACK (cnc_status_changed_cb), tcnc);
			/*
			  tcnc->priv->transaction_status_signal =
			  gda_thread_wrapper_connect_raw (tcnc->priv->wrapper,
			  tcnc->priv->cnc,
			  "transaction-status-changed",
			  FALSE, FALSE,
			  (GdaThreadWrapperCallback) transaction_status_changed_cb,
			  tcnc);
			*/


			/* FIXME: meta store, open it in a sub thread to avoid locking the GTK thread */
			g_message ("Setting Connection");
			GError *lerror = NULL;
			if (! have_meta_store_ready (tcnc, &lerror)) {
				gchar *tmp;
				tmp = g_strdup_printf (_("Error while fetching meta data from the connection: %s"),
						       lerror && lerror->message ? lerror->message : _("No detail"));
				g_clear_error (&lerror);
				g_signal_emit (tcnc, t_connection_signals [NOTICE], 0, tmp);
				g_free (tmp);
			}
                        break;
		case PROP_NAME: {
			const gchar *name;
			name = g_value_get_string (value);
			if (name && *name) {
				g_free (tcnc->priv->name);
				tcnc->priv->name = g_strdup (name);
			}
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}



static void
t_connection_get_property (GObject *object,
			   guint param_id,
			   GValue *value,
			   GParamSpec *pspec)
{
        TConnection *tcnc;

        tcnc = T_CONNECTION (object);
        if (tcnc->priv) {
                switch (param_id) {
                case PROP_GDA_CNC:
                        g_value_set_object (value, tcnc->priv->cnc);
                        break;
		case PROP_NAME:
			g_value_set_string (value, tcnc->priv->name);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
clear_dsn_info (TConnection *tcnc)
{
        g_free (tcnc->priv->dsn_info.name);
        tcnc->priv->dsn_info.name = NULL;

        g_free (tcnc->priv->dsn_info.provider);
        tcnc->priv->dsn_info.provider = NULL;

        g_free (tcnc->priv->dsn_info.description);
        tcnc->priv->dsn_info.description = NULL;

        g_free (tcnc->priv->dsn_info.cnc_string);
        tcnc->priv->dsn_info.cnc_string = NULL;

        g_free (tcnc->priv->dsn_info.auth_string);
        tcnc->priv->dsn_info.auth_string = NULL;
}

static void
fav_changed_cb (G_GNUC_UNUSED TFavorites *bfav, TConnection *tcnc)
{
	g_signal_emit (tcnc, t_connection_signals [FAV_CHANGED], 0);
}

static void
t_connection_dispose (GObject *object)
{
	TConnection *tcnc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (T_IS_CONNECTION (object));

	tcnc = T_CONNECTION (object);
	if (tcnc->priv) {
		if (tcnc->priv->variables)
			g_object_unref (tcnc->priv->variables);

		if (tcnc->priv->store_cnc)
			g_object_unref (tcnc->priv->store_cnc);

		if (tcnc->priv->executed_statements)
			g_hash_table_destroy (tcnc->priv->executed_statements);

		clear_dsn_info (tcnc);
    if (tcnc->priv->dict_file_name != NULL) {
		  g_free (tcnc->priv->dict_file_name);
		  tcnc->priv->dict_file_name = NULL;
    }

		// FIXME: TO_IMPLEMENT;
		/*
		  if (tcnc->priv->meta_store_signal)
		  gda_thread_wrapper_disconnect (tcnc->priv->wrapper,
		  tcnc->priv->meta_store_signal);
		  if (tcnc->priv->transaction_status_signal)
		  gda_thread_wrapper_disconnect (tcnc->priv->wrapper,
		  tcnc->priv->transaction_status_signal);
		*/

		g_free (tcnc->priv->name);
		tcnc->priv->name = NULL;

		if (tcnc->priv->mstruct) {
			g_object_unref (tcnc->priv->mstruct);
			tcnc->priv->mstruct = NULL;
		}

		if (tcnc->priv->cnc) {
			g_signal_handlers_disconnect_by_func (tcnc->priv->cnc,
							      G_CALLBACK (cnc_status_changed_cb), tcnc);
			g_object_unref (tcnc->priv->cnc);
			tcnc->priv->cnc = NULL;
		}

		if (tcnc->priv->parser) {
			g_object_unref (tcnc->priv->parser);
			tcnc->priv->parser = NULL;
		}

		if (tcnc->priv->bfav) {
			g_signal_handlers_disconnect_by_func (tcnc->priv->bfav,
							      G_CALLBACK (fav_changed_cb), tcnc);
			g_object_unref (tcnc->priv->bfav);
			tcnc->priv->bfav = NULL;
		}

		g_free (tcnc->priv->query_buffer);

		t_connection_set_busy_state (tcnc, FALSE, NULL);

		if (tcnc->priv->infos)
			g_object_unref (tcnc->priv->infos);

		g_free (tcnc->priv);
		tcnc->priv = NULL;
		/*g_print ("=== Disposed TConnection %p\n", tcnc);*/
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
user_password_needed (GdaDsnInfo *info, const gchar *real_provider,
		      gboolean *out_need_username, gboolean *out_need_password,
		      gboolean *out_need_password_if_user)
{
	GdaProviderInfo *pinfo = NULL;
	if (out_need_username)
		*out_need_username = FALSE;
	if (out_need_password)
		*out_need_password = FALSE;
	if (out_need_password_if_user)
		*out_need_password_if_user = FALSE;

	if (real_provider)
		pinfo = gda_config_get_provider_info (real_provider);
	else if (info)
		pinfo = gda_config_get_provider_info (info->provider);

	if (pinfo && pinfo->auth_params) {
		if (out_need_password) {
			GdaHolder *h;
			h = gda_set_get_holder (pinfo->auth_params, "PASSWORD");
			if (h && gda_holder_get_not_null (h))
				*out_need_password = TRUE;
		}
		if (out_need_username) {
			GdaHolder *h;
			h = gda_set_get_holder (pinfo->auth_params, "USERNAME");
			if (h && gda_holder_get_not_null (h))
				*out_need_username = TRUE;
		}
		if (out_need_password_if_user) {
			if (gda_set_get_holder (pinfo->auth_params, "PASSWORD") &&
			    gda_set_get_holder (pinfo->auth_params, "USERNAME"))
				*out_need_password_if_user = TRUE;
		}
	}
}

static gchar*
read_hidden_passwd (void)
{
	gchar *p, password [100];

#ifdef HAVE_TERMIOS_H
	int fail;
	struct termios termio;

	fail = tcgetattr (0, &termio);
	if (fail)
		return NULL;

	termio.c_lflag &= ~ECHO;
	fail = tcsetattr (0, TCSANOW, &termio);
	if (fail)
		return NULL;
#else
#ifdef G_OS_WIN32
	HANDLE  t = NULL;
        LPDWORD t_orig = NULL;

	/* get a new handle to turn echo off */
	t_orig = (LPDWORD) malloc (sizeof (DWORD));
	t = GetStdHandle (STD_INPUT_HANDLE);

	/* save the old configuration first */
	GetConsoleMode (t, t_orig);

	/* set to the new mode */
	SetConsoleMode (t, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
#endif
#endif
	p = fgets (password, sizeof (password) - 1, stdin);

#ifdef HAVE_TERMIOS_H
	termio.c_lflag |= ECHO;
	tcsetattr (0, TCSANOW, &termio);
#else
#ifdef G_OS_WIN32
	SetConsoleMode (t, *t_orig);
	fflush (stdout);
	free (t_orig);
#endif
#endif

	if (!p)
		return NULL;

	for (p = password; *p; p++) {
		if (*p == '\n') {
			*p = 0;
			break;
		}
	}
	p = g_strdup (password);
	memset (password, 0, sizeof (password));

	return p;
}

/**
 * t_connection_name_is_valid:
 * @name: a name to ckeck
 *
 * Returns: %TRUE if @name is considered valid
 */
gboolean
t_connection_name_is_valid (const gchar *name)
{
	const gchar *ptr;
	if (!name || !*name)
		return FALSE;
	if (! g_ascii_isalpha (*name) && (*name != '_'))
		return FALSE;
	for (ptr = name; *ptr; ptr++)
		if (!g_ascii_isalnum (*ptr) && (*ptr != '_'))
			return FALSE;
	return TRUE;
}


/**
 * t_connection_open:
 * @cnc_name: (nullable): a name to give to the new connection, or %NULL (a new name is automatically chosen)
 * @cnc_string: a connection string, or DSN name
 * @auth_string: (nullable): an authentication string, or %NULL
 * @use_term: if %TRUE, then standard stdin and stdout are used to read and display missing information (username or password)
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Open a connection.
 *
 * Returns: (transfer none): a #TConnection, or %NULL on error
 */
TConnection *
t_connection_open (const gchar *cnc_name, const gchar *cnc_string, const gchar *auth_string, gboolean use_term, GError **error)
{
	GdaConnection *newcnc = NULL;
	TConnection *tcnc = NULL;

	if (cnc_name && ! t_connection_name_is_valid (cnc_name)) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Connection name '%s' is invalid"), cnc_name);
		return NULL;
	}

	GdaDsnInfo *info;
	gboolean need_user, need_pass, need_password_if_user;
	gchar *user, *pass, *real_cnc, *real_provider, *real_auth_string = NULL;
	gchar *real_cnc_string, *real_cnc_to_split;

	/* if cnc string is a regular file, then use it with SQLite or MSAccess */
	if (g_file_test (cnc_string, G_FILE_TEST_IS_REGULAR)) {
		gchar *path, *file, *e1, *e2;
		const gchar *pname = "SQLite";

		path = g_path_get_dirname (cnc_string);
		file = g_path_get_basename (cnc_string);
		if (g_str_has_suffix (file, ".mdb")) {
			pname = "MSAccess";
			file [strlen (file) - 4] = 0;
		}
		else if (g_str_has_suffix (file, ".db"))
			file [strlen (file) - 3] = 0;
		e1 = gda_rfc1738_encode (path);
		e2 = gda_rfc1738_encode (file);
		g_free (path);
		g_free (file);
		real_cnc_string = g_strdup_printf ("%s://DB_DIR=%s;EXTRA_FUNCTIONS=TRUE;DB_NAME=%s", pname, e1, e2);
		g_free (e1);
		g_free (e2);
	}
	else
		real_cnc_string = g_strdup (cnc_string);

	if (auth_string)
		real_cnc_to_split = g_strdup_printf ("%s;%s", real_cnc_string, auth_string);
	else
		real_cnc_to_split = g_strdup (real_cnc_string);
	gda_connection_string_split (real_cnc_to_split, &real_cnc, &real_provider, &user, &pass);
	g_free (real_cnc_to_split);
	real_cnc_to_split = NULL;

	info = gda_config_get_dsn_info (real_cnc);
	user_password_needed (info, real_provider, &need_user, &need_pass, &need_password_if_user);

	if (!real_cnc) {
		g_free (user);
		g_free (pass);
		g_free (real_provider);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
			     _("Malformed connection string '%s'"), cnc_string);
		g_free (real_cnc_string);
		return NULL;
	}

	if ((!user || !pass) && info && info->auth_string) {
		GdaQuarkList* ql;
		const gchar *tmp;
		ql = gda_quark_list_new_from_string (info->auth_string);
		if (!user) {
			tmp = gda_quark_list_find (ql, "USERNAME");
			if (tmp)
				user = g_strdup (tmp);
		}
		if (!pass) {
			tmp = gda_quark_list_find (ql, "PASSWORD");
			if (tmp)
				pass = g_strdup (tmp);
		}
		gda_quark_list_free (ql);
	}
	if (need_user && ((user && !*user) || !user)) {
		if (use_term) {
			gchar buf[80], *ptr;
			g_print (_("\tUsername for '%s': "), cnc_name);
			if (! fgets (buf, 80, stdin)) {
				g_free (real_cnc);
				g_free (user);
				g_free (pass);
				g_free (real_provider);
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
					     _("No username for '%s'"), cnc_string);
				g_free (real_cnc_string);
				return NULL;
			}
			for (ptr = buf; *ptr; ptr++) {
				if (*ptr == '\n') {
					*ptr = 0;
					break;
				}
			}
			g_free (user);
			user = g_strdup (buf);
		}
		else {
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
				     _("No username for '%s'"), cnc_string);
			g_free (real_cnc_string);
		}
	}
	if (user)
		need_pass = need_password_if_user;
	if (need_pass && ((pass && !*pass) || !pass)) {
		if (use_term) {
			gchar *tmp;
			g_print (_("\tPassword for '%s': "), cnc_name);
			tmp = read_hidden_passwd ();
			g_print ("\n");
			if (! tmp) {
				g_free (real_cnc);
				g_free (user);
				g_free (pass);
				g_free (real_provider);
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
					     _("No password for '%s'"), cnc_string);
				g_free (real_cnc_string);
				return NULL;
			}
			g_free (pass);
			pass = tmp;
		}
		else {
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR,
				     _("No password for '%s'"), cnc_string);
			g_free (real_cnc_string);
			return NULL;
		}
	}

	if (user || pass) {
		GString *string;
		string = g_string_new ("");
		if (user) {
			gchar *enc;
			enc = gda_rfc1738_encode (user);
			g_string_append_printf (string, "USERNAME=%s", enc);
			g_free (enc);
		}
		if (pass) {
			gchar *enc;
			enc = gda_rfc1738_encode (pass);
			if (user)
				g_string_append_c (string, ';');
			g_string_append_printf (string, "PASSWORD=%s", enc);
			g_free (enc);
		}
		real_auth_string = g_string_free (string, FALSE);
	}
	if (info && !real_provider)
		newcnc = gda_connection_open_from_dsn_name (real_cnc_string, real_auth_string,
						       GDA_CONNECTION_OPTIONS_AUTO_META_DATA, error);
	else
		newcnc = gda_connection_open_from_string (NULL, real_cnc_string, real_auth_string,
							  GDA_CONNECTION_OPTIONS_AUTO_META_DATA, error);
	g_free (real_cnc_string);
	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);
	g_free (real_auth_string);

	if (newcnc) {
		gint i;
		g_object_set (G_OBJECT (newcnc), "execution-timer", TRUE, NULL);
		g_object_set (newcnc, "execution-slowdown", 2000000, NULL);

		tcnc = t_connection_new (newcnc);
		g_object_unref (newcnc);

		if (cnc_name && *cnc_name) {
			const gchar *rootname;
			rootname = cnc_name;
			if (t_connection_get_by_name (rootname)) {
				for (i = 1; ; i++) {
					gchar *tmp;
					tmp = g_strdup_printf ("%s%d", rootname, i);
					if (t_connection_get_by_name (tmp))
						g_free (tmp);
					else {
						t_connection_set_name (tcnc, tmp);
						g_free (tmp);
						break;
					}
				}
			}
		}

		/* show date format */
		GDateDMY order[3];
		gchar sep;
		if (gda_connection_get_date_format (t_connection_get_cnc (tcnc),
						    &order[0], &order[1], &order[2], &sep, NULL)) {
			GString *string;
			guint i;
			string = g_string_new ("");
			for (i = 0; i < 3; i++) {
				if (i > 0)
					g_string_append_c (string, sep);
				if (order [i] == G_DATE_DAY)
					g_string_append (string, "DD");
				else if (order [i] == G_DATE_MONTH)
					g_string_append (string, "MM");
				else
					g_string_append (string, "YYYY");
			}
			g_print (_("Date format for this connection will be: %s, where YYYY is the year, MM the month and DD the day\n"),
				 string->str);

			GdaHolder *h;
			h = gda_set_get_holder (tcnc->priv->infos, "date_format");
			if (!h) {
				h = gda_holder_new (G_TYPE_STRING, "date_format");
				g_object_set (h,
					      "description", _("Format of the date used for the connection, where "
							       "YYYY is the year, MM the month and DD the day"), NULL);
				gda_set_add_holder (tcnc->priv->infos, h);
			}
			g_assert (gda_holder_set_value_str (h, NULL, string->str, NULL));
			g_string_free (string, TRUE);
		}
	}

	return tcnc;
}

/**
 * t_connection_new
 * @cnc: a #GdaConnection
 *
 * Creates a new #TConnection object wrapping @cnc. The browser_core_take_connection() method
 * must be called on the new object to make it managed by the browser.
 *
 * To close the new connection, use t_connection_close().
 *
 * Returns: (transfer none): a new object
 */
TConnection*
t_connection_new (GdaConnection *cnc)
{
	TConnection *tcnc;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_message ("Creating TConnection");

	tcnc = T_CONNECTION (g_object_new (T_TYPE_CONNECTION, "gda-connection", cnc, NULL));
	t_app_add_tcnc (tcnc);
	/*g_print ("=== created TCnc %p\n", tcnc);*/

	return tcnc;
}

/**
 * t_connection_close:
 * @tcnc: a #TConnection
 *
 * Closes @tcnc. This leads to the @tcnc object being destroyed
 */
void
t_connection_close (TConnection *tcnc)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	gda_connection_close (tcnc->priv->cnc, NULL);
}

/**
 * t_connection_get_by_name:
 * @main_data: a #TApp
 * @name: a connection's name
 *
 * Find a TConnection using its name, the TConnection must be referenced in @main_data
 *
 * Returns: (transfer none): the requested #TConnection, or %NULL
 */
TConnection *
t_connection_get_by_name (const gchar *name)
{
	g_return_val_if_fail (name, NULL);

	TConnection *tcnc = NULL;
        const GSList *list;
        for (list = t_app_get_all_connections (); list; list = list->next) {
                if (!strcmp (name, t_connection_get_name (T_CONNECTION (list->data)))) {
                        tcnc = (TConnection *) list->data;
                        break;
                }
        }
        return tcnc;
}

/**
 * t_connection_get_cnc:
 * @tcnc: a #TConnection
 *
 * Returns: @tcnc's internal #GdaConnection
 */
GdaConnection *
t_connection_get_cnc (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return tcnc->priv->cnc;
}

/**
 * t_connection_set_name:
 * @tcnc: a #TConnection
 * @name: a new name, not %NULL and not empty
 *
 * Set @tcnc's name
 */
void
t_connection_set_name (TConnection *tcnc, const gchar *name)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	g_object_set (tcnc, "name", name, NULL);
}

/**
 * t_connection_get_name
 * @tcnc: a #TConnection
 *
 * Returns: @tcnc's name
 */
const gchar *
t_connection_get_name (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return tcnc->priv->name;
}

/**
 * t_connection_get_information:
 * @tcnc: a #TConnection
 *
 * Get the information about @tcnc as a string.
 *
 * Returns: a new string
 */
const gchar *
t_connection_get_information (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	GString *title;
	title = g_string_new ("");
	const GdaDsnInfo *dsn;
	dsn = t_connection_get_dsn_information (tcnc);
	if (dsn) {
		if (dsn->name)
			g_string_append_printf (title, "%s '%s'", _("Data source"), dsn->name);
		if (dsn->provider)
			g_string_append_printf (title, " (%s)", dsn->provider);
	}
	return g_string_free (title, FALSE);
}

/**
 * t_connection_get_long_name:
 * @tcnc: a #TConnection
 *
 * Get the "long" name of @tcnc, in the form "Connection <name>, <data source or provider>"
 *
 * Returns: a new string
 */
gchar *
t_connection_get_long_name (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	const gchar *cncname;
	const GdaDsnInfo *dsn;
	GString *title;

	dsn = t_connection_get_dsn_information (tcnc);
	cncname = t_connection_get_name (tcnc);
	title = g_string_new (_("Connection"));
	g_string_append (title, " ");
	g_string_append_printf (title, "'%s'", cncname ? cncname : _("unnamed"));
	if (dsn) {
		if (dsn->name)
			g_string_append_printf (title, ", %s '%s'", _("data source"), dsn->name);
		if (dsn->provider)
			g_string_append_printf (title, " (%s)", dsn->provider);
	}
	return g_string_free (title, FALSE);
}

/**
 * t_connection_get_dsn_information
 * @tcnc: a #TConnection
 *
 * Get some information about the connection
 *
 * Returns: a pointer to the associated #GdaDsnInfo
 */
const GdaDsnInfo *
t_connection_get_dsn_information (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	clear_dsn_info (tcnc);
	if (!tcnc->priv->cnc)
		return NULL;
	
	if (gda_connection_get_provider_name (tcnc->priv->cnc))
		tcnc->priv->dsn_info.provider = g_strdup (gda_connection_get_provider_name (tcnc->priv->cnc));
	if (gda_connection_get_dsn (tcnc->priv->cnc)) {
		tcnc->priv->dsn_info.name = g_strdup (gda_connection_get_dsn (tcnc->priv->cnc));
		if (! tcnc->priv->dsn_info.provider) {
			GdaDsnInfo *cinfo;
			cinfo = gda_config_get_dsn_info (tcnc->priv->dsn_info.name);
			if (cinfo && cinfo->provider)
				tcnc->priv->dsn_info.provider = g_strdup (cinfo->provider);
		}
	}
	if (gda_connection_get_cnc_string (tcnc->priv->cnc))
		tcnc->priv->dsn_info.cnc_string = g_strdup (gda_connection_get_cnc_string (tcnc->priv->cnc));
	if (gda_connection_get_authentication (tcnc->priv->cnc))
		tcnc->priv->dsn_info.auth_string = g_strdup (gda_connection_get_authentication (tcnc->priv->cnc));

	return &(tcnc->priv->dsn_info);
}

/**
 * t_connection_is_virtual
 * @tcnc: a #TConnection
 *
 * Tells if @tcnc is a virtual connection or not
 *
 * Returns: %TRUE if @tcnc is a virtual connection
 */
gboolean
t_connection_is_virtual (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	if (GDA_IS_VIRTUAL_CONNECTION (tcnc->priv->cnc))
		return TRUE;
	else
		return FALSE;
}

/**
 * t_connection_is_busy
 * @tcnc: a #TConnection
 * @out_reason: a pointer to store a copy of the reason @tcnc is busy (will be set 
 *              to %NULL if @tcnc is not busy), or %NULL
 *
 * Tells if @tcnc is currently busy or not.
 *
 * Returns: %TRUE if @tcnc is busy
 */
gboolean
t_connection_is_busy (TConnection *tcnc, gchar **out_reason)
{
	if (out_reason)
		*out_reason = NULL;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);

	if (out_reason && tcnc->priv->busy_reason)
		*out_reason = g_strdup (tcnc->priv->busy_reason);

	return tcnc->priv->busy;
}

/**
 * t_connection_update_meta_data
 * @tcnc: a #TConnection
 *
 * Make @tcnc update its meta store in the background.
 */
void
t_connection_update_meta_data (TConnection *tcnc, GError ** error)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));

  g_message ("Updating meta data for: %s", t_connection_get_name (tcnc));

	t_connection_set_busy_state (tcnc, TRUE, _("Getting database schema information"));

	GdaMetaContext context = {"_tables", 0, NULL, NULL};
	gboolean result;
	result = gda_connection_update_meta_store (tcnc->priv->cnc, &context, error);
  if (!result) {
    gchar *tmp;
    g_warning (_("Error while fetching meta data from the connection: %s"),
				       (*error)->message ? (*error)->message : _("No detail"));
		tmp = g_strdup_printf (_("Error while fetching meta data from the connection: %s"),
				       (*error)->message ? (*error)->message : _("No detail"));
		g_signal_emit (tcnc, t_connection_signals [NOTICE], 0, tmp);
    return;
  }
  t_connection_meta_data_changed (tcnc);
	t_connection_set_busy_state (tcnc, FALSE, NULL);
}

/**
 * t_connection_get_meta_struct
 * @tcnc: a #TConnection
 *
 * Get the #GdaMetaStruct maintained up to date by @tcnc.
 *
 * Returns: a #GdaMetaStruct, the caller does not have any reference to it.
 */
GdaMetaStruct *
t_connection_get_meta_struct (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return tcnc->priv->mstruct;
}

/**
 * t_connection_get_meta_store
 * @tcnc: a #TConnection
 *
 * Returns: @tcnc's #GdaMetaStore, the caller does not have any reference to it.
 */
GdaMetaStore *
t_connection_get_meta_store (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return gda_connection_get_meta_store (tcnc->priv->cnc);
}

/**
 * t_connection_get_dictionary_file
 * @tcnc: a #TConnection
 *
 * Returns: the dictionary file name used by @tcnc, or %NULL
 */
const gchar *
t_connection_get_dictionary_file (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return tcnc->priv->dict_file_name;
}

/**
 * t_connection_get_transaction_status
 * @tcnc: a #TConnection
 *
 * Retuns: the #GdaTransactionStatus of the connection, or %NULL
 */
GdaTransactionStatus *
t_connection_get_transaction_status (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return gda_connection_get_transaction_status (tcnc->priv->cnc);
}

/**
 * t_connection_begin
 * @tcnc: a #TConnection
 * @error: a place to store errors, or %NULL
 *
 * Begins a transaction
 */
gboolean
t_connection_begin (TConnection *tcnc, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	return gda_connection_begin_transaction (tcnc->priv->cnc, NULL,
						 GDA_TRANSACTION_ISOLATION_UNKNOWN, error);
}

/**
 * t_connection_commit
 * @tcnc: a #TConnection
 * @error: a place to store errors, or %NULL
 *
 * Commits a transaction
 */
gboolean
t_connection_commit (TConnection *tcnc, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	return gda_connection_commit_transaction (tcnc->priv->cnc, NULL, error);
}

/**
 * t_connection_rollback
 * @tcnc: a #TConnection
 * @error: a place to store errors, or %NULL
 *
 * Rolls back a transaction
 */
gboolean
t_connection_rollback (TConnection *tcnc, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	return gda_connection_rollback_transaction (tcnc->priv->cnc, NULL, error);
}

/**
 * t_connection_get_favorites
 * @tcnc: a #TConnection
 *
 * Get @tcnc's favorites handler
 *
 * Returns: (transfer none): the #TFavorites used by @tcnc
 */
TFavorites *
t_connection_get_favorites (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	if (!tcnc->priv->bfav && !T_IS_VIRTUAL_CONNECTION (tcnc)) {
		tcnc->priv->bfav = t_favorites_new (gda_connection_get_meta_store (tcnc->priv->cnc));
		g_signal_connect (tcnc->priv->bfav, "favorites-changed",
				  G_CALLBACK (fav_changed_cb), tcnc);
	}
	return tcnc->priv->bfav;
}

/**
 * t_connection_get_completions
 * @tcnc: a #TConnection
 * @sql:
 * @start:
 * @end:
 *
 * See gda_completion_list_get()
 *
 * Returns: a new array of strings, or NULL (use g_strfreev() to free the returned array)
 */
gchar **
t_connection_get_completions (TConnection *tcnc, const gchar *sql,
			      gint start, gint end)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return gda_completion_list_get (tcnc->priv->cnc, sql, start, end);
}


/**
 * t_connection_get_parser:
 * @tcnc: a #TConnection
 *
 * Get the #GdaSqlParser object for @tcnc
 *
 * Returns: (transfer none): a #GdaSqlParser
 */
GdaSqlParser *
t_connection_get_parser (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	if (!tcnc->priv->parser)
		tcnc->priv->parser = t_connection_create_parser (tcnc);

	return tcnc->priv->parser;
}

/**
 * t_connection_create_parser:
 * @tcnc: a #TConnection
 *
 * Create a new #GdaSqlParser object for @tcnc
 *
 * Returns: (transfer full): a new #GdaSqlParser
 */
GdaSqlParser *
t_connection_create_parser (TConnection *tcnc)
{
	GdaSqlParser *parser;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	
	parser = gda_connection_create_parser (tcnc->priv->cnc);
	if (!parser)
		parser = gda_sql_parser_new ();
	return parser;
}

/**
 * t_connection_render_pretty_sql
 * @tcnc: a #TConnection
 * @stmt: a #GdaStatement
 *
 * Renders @stmt as SQL well indented
 *
 * Returns: a new string
 */
gchar *
t_connection_render_pretty_sql (TConnection *tcnc, GdaStatement *stmt)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	return gda_statement_to_sql_extended (stmt, tcnc->priv->cnc, NULL,
					      GDA_STATEMENT_SQL_PRETTY |
					      GDA_STATEMENT_SQL_PARAMS_SHORT,
					      NULL, NULL);
}

/**
 * t_connection_execute_statement
 * @tcnc: a #TConnection
 * @stmt: a #GdaStatement
 * @params: a #GdaSet as parameters, or %NULL
 * @model_usage: how the returned data model (if any) will be used
 * @last_insert_row: (nullable): a place to store a new GdaSet object which contains the values of the last inserted row, or %NULL.
 * @error: a place to store errors, or %NULL
 *
 * Executes @stmt by @tcnc.
 *
 * Returns: (transfer full): a #GObject, or %NULL if an error occurred
 */
GObject *
t_connection_execute_statement (TConnection *tcnc,
				GdaStatement *stmt,
				GdaSet *params,
				GdaStatementModelUsage model_usage,
				GdaSet **last_insert_row,
				GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (!params || GDA_IS_SET (params), NULL);

	GObject *obj;
	obj = gda_connection_statement_execute (tcnc->priv->cnc, stmt, params, model_usage,
						last_insert_row, error);
	if (obj) {
		if (GDA_IS_DATA_MODEL (obj))
			/* force loading of rows if necessary */
			gda_data_model_get_n_rows ((GdaDataModel*) obj);
		else if (last_insert_row)
			g_object_set_data (obj, "__tcnc_last_inserted_row", last_insert_row);
	}
	return obj;
}

/**
 * t_connection_normalize_sql_statement
 * @tcnc: a #TConnection
 * @sqlst: a #GdaSqlStatement
 * @error: a place to store errors, or %NULL
 *
 * See gda_sql_statement_normalize().
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
t_connection_normalize_sql_statement (TConnection *tcnc,
				      GdaSqlStatement *sqlst, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	
	return gda_sql_statement_normalize (sqlst, tcnc->priv->cnc, error);
}

/**
 * t_connection_check_sql_statement_validify
 */
gboolean
t_connection_check_sql_statement_validify (TConnection *tcnc,
					   GdaSqlStatement *sqlst, GError **error)
{
	g_return_val_if_fail (sqlst, FALSE);
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);

	/* check the structure first */
        if (!gda_sql_statement_check_structure (sqlst, error))
                return FALSE;

	return gda_sql_statement_check_validity_m (sqlst, tcnc->priv->mstruct, error);
}



/*
 * DOES emit the "busy" signal
 */
void
t_connection_set_busy_state (TConnection *tcnc, gboolean busy, const gchar *busy_reason)
{
	if (busy && !busy_reason)
		g_warning ("Connection busy, but no reason provided");

	if (tcnc->priv->busy_reason) {
		g_free (tcnc->priv->busy_reason);
		tcnc->priv->busy_reason = NULL;
	}

	tcnc->priv->busy = busy;
	if (busy_reason)
		tcnc->priv->busy_reason = g_strdup (busy_reason);

	g_signal_emit (tcnc, t_connection_signals [BUSY], 0, busy, busy_reason);
}

/*
 *
 * Preferences
 *
 */
#define DBTABLE_PREFERENCES_TABLE_NAME "gda_sql_dbtable_preferences"
#define DBTABLE_PREFERENCES_TABLE_DESC					\
        "<table name=\"" DBTABLE_PREFERENCES_TABLE_NAME "\"> "		\
        "   <column name=\"table_schema\" pkey=\"TRUE\"/>"		\
        "   <column name=\"table_name\" pkey=\"TRUE\"/>"		\
        "   <column name=\"table_column\" nullok=\"TRUE\" pkey=\"TRUE\"/>" \
        "   <column name=\"att_name\"/>"				\
        "   <column name=\"att_value\"/>"				\
        "</table>"

static gboolean
meta_store_addons_init (TConnection *tcnc, GError **error)
{
	GError *lerror = NULL;
	GdaMetaStore *store;

	if (!tcnc->priv->cnc) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Connection not yet opened"));
		return FALSE;
	}
	store = gda_connection_get_meta_store (tcnc->priv->cnc);
	if (!gda_meta_store_schema_add_custom_object (store, DBTABLE_PREFERENCES_TABLE_DESC, &lerror)) {
                g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize dictionary to store table preferences"));
		g_warning ("Can't initialize dictionary to store dbtable_preferences :%s",
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }

	tcnc->priv->store_cnc = g_object_ref (gda_meta_store_get_internal_connection (store));
	return TRUE;
}


/**
 * t_connection_set_table_column_attribute
 * @tcnc:
 * @dbo:
 * @column:
 * @attr_name: attribute name, not %NULL
 * @value: value to set, or %NULL to unset
 * @error:
 *
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
t_connection_set_table_column_attribute (TConnection *tcnc,
					 GdaMetaTable *table,
					 GdaMetaTableColumn *column,
					 const gchar *attr_name,
					 const gchar *value, GError **error)
{
	GdaConnection *store_cnc;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	g_return_val_if_fail (table, FALSE);
	g_return_val_if_fail (column, FALSE);
	g_return_val_if_fail (attr_name, FALSE);

	if (! tcnc->priv->store_cnc &&
	    ! meta_store_addons_init (tcnc, error))
		return FALSE;

	store_cnc = tcnc->priv->store_cnc;
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (store_cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
                return FALSE;
	}

	/* delete existing attribute */
	GdaStatement *stmt;
	GdaSqlBuilder *builder;
	GdaSet *params;
	GdaSqlBuilderId op_ids[4];
	GdaMetaDbObject *dbo = (GdaMetaDbObject *) table;

	params = gda_set_new_inline (5, "schema", G_TYPE_STRING, dbo->obj_schema,
				     "name", G_TYPE_STRING, dbo->obj_name,
				     "column", G_TYPE_STRING, column->column_name,
				     "attname", G_TYPE_STRING, attr_name,
				     "attvalue", G_TYPE_STRING, value);

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (builder, DBTABLE_PREFERENCES_TABLE_NAME);
	op_ids[0] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_schema"),
					      gda_sql_builder_add_param (builder, "schema", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[1] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_name"),
					      gda_sql_builder_add_param (builder, "name", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[2] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_column"),
					      gda_sql_builder_add_param (builder, "column", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[3] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "att_name"),
					      gda_sql_builder_add_param (builder, "attname", G_TYPE_STRING,
									 FALSE), 0);
	gda_sql_builder_set_where (builder,
				   gda_sql_builder_add_cond_v (builder, GDA_SQL_OPERATOR_TYPE_AND,
							       op_ids, 4));
	stmt = gda_sql_builder_get_statement (builder, error);
	g_object_unref (G_OBJECT (builder));
	if (!stmt)
		goto err;
	if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto err;
	}
	g_object_unref (stmt);		

	/* insert new attribute if necessary */
	if (value) {
		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
		gda_sql_builder_set_table (builder, DBTABLE_PREFERENCES_TABLE_NAME);
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "table_schema"),
						    gda_sql_builder_add_param (builder, "schema", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "table_name"),
						    gda_sql_builder_add_param (builder, "name", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "table_column"),
						    gda_sql_builder_add_param (builder, "column", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "att_name"),
						    gda_sql_builder_add_param (builder, "attname", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "att_value"),
						    gda_sql_builder_add_param (builder, "attvalue", G_TYPE_STRING, FALSE));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);
	}

	if (! gda_connection_commit_transaction (store_cnc, NULL, NULL)) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't commit transaction to access favorites"));
		goto err;
	}

	g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	/*
	  g_print ("%s(table=>%s, column=>%s, value=>%s)\n", __FUNCTION__, GDA_META_DB_OBJECT (table)->obj_full_name,
	  column->column_name, value);
	*/
	g_signal_emit (tcnc, t_connection_signals [TABLE_COLUMN_PREF_CHANGED], 0,
		       table, column, attr_name, value);

	return TRUE;

 err:
	g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	gda_connection_rollback_transaction (store_cnc, NULL, NULL);
	return FALSE;
}

/**
 * t_connection_get_table_column_attribute
 * @tcnc:
 * @dbo:
 * @column: may be %NULL
 * @attr_name: attribute name, not %NULL
 * @error:
 *
 *
 * Returns: the requested attribute (as a new string), or %NULL if not set or if an error occurred
 */
gchar *
t_connection_get_table_column_attribute  (TConnection *tcnc,
					  GdaMetaTable *table,
					  GdaMetaTableColumn *column,
					  const gchar *attr_name,
					  GError **error)
{
	GdaConnection *store_cnc;
	gchar *retval = NULL;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	g_return_val_if_fail (table, FALSE);
	g_return_val_if_fail (column, FALSE);
	g_return_val_if_fail (attr_name, FALSE);

	if (! tcnc->priv->store_cnc &&
	    ! meta_store_addons_init (tcnc, error))
		return FALSE;

	store_cnc = tcnc->priv->store_cnc;
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		return FALSE;
	}

	/* SELECT */
	GdaStatement *stmt;
	GdaSqlBuilder *builder;
	GdaSet *params;
	GdaSqlBuilderId op_ids[4];
	GdaDataModel *model = NULL;
	const GValue *cvalue;
	GdaMetaDbObject *dbo = (GdaMetaDbObject *) table;

	params = gda_set_new_inline (4, "schema", G_TYPE_STRING, dbo->obj_schema,
				     "name", G_TYPE_STRING, dbo->obj_name,
				     "column", G_TYPE_STRING, column->column_name,
				     "attname", G_TYPE_STRING, attr_name);

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target_id (builder,
					      gda_sql_builder_add_id (builder, DBTABLE_PREFERENCES_TABLE_NAME),
					      NULL);
	gda_sql_builder_select_add_field (builder, "att_value", NULL, NULL);
	op_ids[0] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_schema"),
					      gda_sql_builder_add_param (builder, "schema", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[1] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_name"),
					      gda_sql_builder_add_param (builder, "name", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[2] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_column"),
					      gda_sql_builder_add_param (builder, "column", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[3] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "att_name"),
					      gda_sql_builder_add_param (builder, "attname", G_TYPE_STRING,
									 FALSE), 0);
	gda_sql_builder_set_where (builder,
				   gda_sql_builder_add_cond_v (builder, GDA_SQL_OPERATOR_TYPE_AND,
							       op_ids, 4));
	stmt = gda_sql_builder_get_statement (builder, error);
	g_object_unref (G_OBJECT (builder));
	if (!stmt)
		goto out;

	model = gda_connection_statement_execute_select (store_cnc, stmt, params, error);
	g_object_unref (stmt);
	if (!model)
		goto out;

	/*gda_data_model_dump (model, NULL);*/
	if (gda_data_model_get_n_rows (model) == 0)
		goto out;

	cvalue = gda_data_model_get_value_at (model, 0, 0, error);
	if (cvalue)
		retval = g_value_dup_string (cvalue);

 out:
	if (model)
		g_object_unref (model);
	g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));

	return retval;
}

/**
 * t_connection_define_ui_plugins_for_batch
 * @tcnc: a #TConnection object
 * @batch: a #GdaBatch
 * @params: a #GdaSet (usually created with gda_batch_get_parameters())
 *
 * Calls t_connection_define_ui_plugins_for_stmt() for each statement in @batch
 */
void
t_connection_define_ui_plugins_for_batch (TConnection *tcnc, GdaBatch *batch, GdaSet *params)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	g_return_if_fail (GDA_IS_BATCH (batch));
	if (!params)
		return;
	g_return_if_fail (GDA_IS_SET (params));

	const GSList *list;
	for (list = gda_batch_get_statements (batch); list; list = list->next)
		t_connection_define_ui_plugins_for_stmt (tcnc, GDA_STATEMENT (list->data), params);
}

/* remark: the current ABI leaves no room to add a
 * validity check to the GdaSqlExpr structure, and the following test
 * should be done in gda_sql_expr_check_validity() once the GdaSqlExpr
 * has the capacity to hold the information (ie. when ABI is broken)
 *
 * The code here is a modification from the gda_sql_select_field_check_validity()
 * adapted for the GdaSqlExpr.
 */
static gboolean
_gda_sql_expr_check_validity (GdaSqlExpr *expr, GdaMetaStruct *mstruct,
			      GdaMetaDbObject **out_validity_meta_object,
			      GdaMetaTableColumn **out_validity_meta_table_column, GError **error)
{
	GdaMetaDbObject *dbo = NULL;
	const gchar *field_name;

	*out_validity_meta_object = NULL;
	*out_validity_meta_table_column = NULL;

	if (! expr->value || (G_VALUE_TYPE (expr->value) != G_TYPE_STRING))
		return TRUE;
	field_name = g_value_get_string (expr->value);

	
	GdaSqlAnyPart *any;
	GdaMetaTableColumn *tcol = NULL;
	GValue value;

	memset (&value, 0, sizeof (GValue));
	for (any = GDA_SQL_ANY_PART(expr)->parent;
	     any && (any->type != GDA_SQL_ANY_STMT_SELECT) && (any->type != GDA_SQL_ANY_STMT_DELETE) &&
		     (any->type != GDA_SQL_ANY_STMT_UPDATE);
	     any = any->parent);
	if (!any) {
		/* not in a structure which can be analysed */
		return TRUE;
	}

	switch (any->type) {
	case GDA_SQL_ANY_STMT_SELECT: {
		/* go through all the SELECT's targets to see if
		 * there is a table with the corresponding field */
		GSList *targets;
		if (((GdaSqlStatementSelect *)any)->from) {
			for (targets = ((GdaSqlStatementSelect *)any)->from->targets;
			     targets;
			     targets = targets->next) {
				GdaSqlSelectTarget *target = (GdaSqlSelectTarget *) targets->data;
				if (!target->validity_meta_object /*&&
								   * commented out in the current context because
								   * t_connection_check_sql_statement_validify() has already been 
								   * called, will need to be re-added when movind to the
								   * gda-statement-struct.c file.
								   *
								   * !gda_sql_select_target_check_validity (target, data, error)*/)
					return FALSE;
				
				g_value_set_string (g_value_init (&value, G_TYPE_STRING), field_name);
				tcol = gda_meta_struct_get_table_column (mstruct,
									 GDA_META_TABLE (target->validity_meta_object),
									 &value);
				g_value_unset (&value);
				if (tcol) {
					/* found a candidate */
					if (dbo) {
						g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
							     _("Could not identify table for field '%s'"), field_name);
						return FALSE;
					}
					dbo = target->validity_meta_object;
				}
			}
		}
		break;
	}
	case GDA_SQL_ANY_STMT_UPDATE: {
		GdaSqlTable *table;
		table = ((GdaSqlStatementUpdate *)any)->table;
		if (!table || !table->validity_meta_object /* ||
							    * commented out in the current context because
							    * t_connection_check_sql_statement_validify() has already been 
							    * called, will need to be re-added when movind to the
							    * gda-statement-struct.c file.
							    *
							    * !gda_sql_select_target_check_validity (target, data, error)*/)
			return FALSE;
		dbo = table->validity_meta_object;
		g_value_set_string (g_value_init (&value, G_TYPE_STRING), field_name);
		tcol = gda_meta_struct_get_table_column (mstruct,
							 GDA_META_TABLE (table->validity_meta_object),
							 &value);
		g_value_unset (&value);
		break;
	}
	case GDA_SQL_ANY_STMT_DELETE: {
		GdaSqlTable *table;
		table = ((GdaSqlStatementDelete *)any)->table;
		if (!table || !table->validity_meta_object /* ||
							    * commented out in the current context because
							    * t_connection_check_sql_statement_validify() has already been 
							    * called, will need to be re-added when movind to the
							    * gda-statement-struct.c file.
							    *
							    * !gda_sql_select_target_check_validity (target, data, error)*/)
			return FALSE;
		dbo = table->validity_meta_object;
		g_value_set_string (g_value_init (&value, G_TYPE_STRING), field_name);
		tcol = gda_meta_struct_get_table_column (mstruct,
							 GDA_META_TABLE (table->validity_meta_object),
							 &value);
		g_value_unset (&value);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	if (!dbo) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			     _("Could not identify table for field '%s'"), field_name);
		return FALSE;
	}
	*out_validity_meta_object = dbo;
	*out_validity_meta_table_column = tcol;
	return TRUE;
}

typedef struct {
	TConnection *tcnc;
	GdaSet *params;
} ParamsData;

/*
 *
 * In this function we try to find for which table's column a parameter is and use
 * preferences to set the GdaHolder's plugin attribute
 */
static gboolean
foreach_ui_plugins_for_params (GdaSqlAnyPart *part, ParamsData *data, G_GNUC_UNUSED GError **error)
{
	if (part->type != GDA_SQL_ANY_EXPR)
		return TRUE;
	GdaSqlExpr *expr = (GdaSqlExpr*) part;
	if (!expr->param_spec)
		return TRUE;

	GdaHolder *holder;
	holder = gda_set_get_holder (data->params, expr->param_spec->name);
	if (! holder)
		return TRUE;

	GdaSqlAnyPart *uppart;
	gchar *plugin = NULL;
	uppart = part->parent;
	if (!uppart)
		return TRUE;
	else if (uppart->type == GDA_SQL_ANY_SQL_OPERATION) {
		GdaSqlOperation *op = (GdaSqlOperation*) uppart;
		/* look into condition */
		GSList *list;
		for (list = op->operands; list; list = list->next) {
			GdaSqlExpr *oexpr = (GdaSqlExpr*) list->data;
			if (oexpr == expr)
				continue;
			
			GdaMetaDbObject    *validity_meta_object;
			GdaMetaTableColumn *validity_meta_table_column;
			if (_gda_sql_expr_check_validity (oexpr,
							  t_connection_get_meta_struct (data->tcnc),
							  &validity_meta_object,
							  &validity_meta_table_column, NULL)) {
				plugin = t_connection_get_table_column_attribute (data->tcnc,
										  GDA_META_TABLE (validity_meta_object),
										  validity_meta_table_column,
										  T_CONNECTION_COLUMN_PLUGIN, NULL);
				break;
			}
		}
	}
	else if (uppart->type == GDA_SQL_ANY_STMT_UPDATE) {
		GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) uppart;
		GdaSqlField *field;
		field = g_slist_nth_data (upd->fields_list, g_slist_index (upd->expr_list, expr));
		if (field)
			plugin = t_connection_get_table_column_attribute (data->tcnc,
									  GDA_META_TABLE (upd->table->validity_meta_object),
									  field->validity_meta_table_column,
									  T_CONNECTION_COLUMN_PLUGIN, NULL);
	}
	else if (uppart->type == GDA_SQL_ANY_STMT_INSERT) {
		GdaSqlStatementInsert *ins = (GdaSqlStatementInsert*) uppart;
		GdaSqlField *field;
		gint expr_index = -1;
		GSList *slist;
		GdaMetaTableColumn *column = NULL;
		for (slist = ins->values_list; slist; slist = slist->next) {
			expr_index = g_slist_index ((GSList*) slist->data, expr);
			if (expr_index >= 0)
				break;
		}
		if (expr_index >= 0) {
			field = g_slist_nth_data (ins->fields_list, expr_index);
			if (field)
				column = field->validity_meta_table_column;
			else {
				/* no field specified => take the table's fields */
				GdaMetaTable *mtable = GDA_META_TABLE (ins->table->validity_meta_object);
				column = g_slist_nth_data (mtable->columns, expr_index);
			}
		}
		if (column)
			plugin = t_connection_get_table_column_attribute (data->tcnc,
									  GDA_META_TABLE (ins->table->validity_meta_object),
									  column,
									  T_CONNECTION_COLUMN_PLUGIN, NULL);
	}

	if (plugin) {
		/*g_print ("Using plugin [%s]\n", plugin);*/
		GValue *value;
		g_value_take_string ((value = gda_value_new (G_TYPE_STRING)), plugin);
		g_object_set (holder, "plugin", value, NULL);
		gda_value_free (value);
	}

	return TRUE;
}

/**
 * t_connection_define_ui_plugins_for_stmt
 * @tcnc: a #TConnection object
 * @stmt: a #GdaStatement
 * @params: a #GdaSet (usually created with gda_statement_get_parameters())
 *
 * Analyses @stmt and assign plugins to each #GdaHolder in @params according to the preferences stored
 * for each table's field, defined at some point using t_connection_set_table_column_attribute().
 */
void
t_connection_define_ui_plugins_for_stmt (TConnection *tcnc, GdaStatement *stmt, GdaSet *params)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	g_return_if_fail (GDA_IS_STATEMENT (stmt));
	if (!params)
		return;
	g_return_if_fail (GDA_IS_SET (params));
	
	GdaSqlStatement *sqlst;
	GdaSqlAnyPart *rootpart;
	g_object_get ((GObject*) stmt, "structure", &sqlst, NULL);
	g_return_if_fail (sqlst);
	switch (sqlst->stmt_type) {
	case GDA_SQL_STATEMENT_INSERT:
	case GDA_SQL_STATEMENT_UPDATE:
	case GDA_SQL_STATEMENT_DELETE:
	case GDA_SQL_STATEMENT_SELECT:
	case GDA_SQL_STATEMENT_COMPOUND:
		rootpart = (GdaSqlAnyPart*) sqlst->contents;
		break;
	default:
		rootpart = NULL;
		break;
	}
	GError *lerror = NULL;
	if (!rootpart || !t_connection_check_sql_statement_validify (tcnc, sqlst, &lerror)) {
		/*g_print ("ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");*/
		g_clear_error (&lerror);
		gda_sql_statement_free (sqlst);
		return;
	}
	
	ParamsData data;
	data.params = params;
	data.tcnc = tcnc;
	gda_sql_any_part_foreach (rootpart, (GdaSqlForeachFunc) foreach_ui_plugins_for_params,
				  &data, NULL);
	
	gda_sql_statement_free (sqlst);

	/* REM: we also need to handle FK tables to propose a drop down list of possible values */
}

/**
 * t_connection_keep_variables
 * @tcnc: a #TConnection object
 * @set: a #GdaSet containing variables for which a copy has to be done
 *
 * Makes a copy of the variables in @set and keep them in @tcnc. Retreive them
 * using t_connection_load_variables()
 */
void
t_connection_keep_variables (TConnection *tcnc, GdaSet *set)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	if (!set)
		return;
	g_return_if_fail (GDA_IS_SET (set));

	if (! tcnc->priv->variables) {
		tcnc->priv->variables = gda_set_copy (set);
		return;
	}

	GSList *list;
	for (list = gda_set_get_holders (set); list; list = list->next) {
		GdaHolder *nh, *eh;
		nh = GDA_HOLDER (list->data);
		eh = gda_set_get_holder (tcnc->priv->variables, gda_holder_get_id (nh));
		if (eh) {
			if (gda_holder_get_g_type (nh) == gda_holder_get_g_type (eh)) {
				const GValue *cvalue;
				cvalue = gda_holder_get_value (nh);
				gda_holder_set_value (eh, cvalue, NULL);
			}
			else {
				gda_set_remove_holder (tcnc->priv->variables, eh);
				eh = gda_holder_copy (nh);
				gda_set_add_holder (tcnc->priv->variables, eh);
				g_object_unref (eh);
			}
		}
		else {
			eh = gda_holder_copy (nh);
			gda_set_add_holder (tcnc->priv->variables, eh);
			g_object_unref (eh);
		}
	}
}

/**
 * t_connection_load_variables
 * @tcnc: a #TConnection object
 * @set: a #GdaSet which will in the end contain (if any) variables stored in @tcnc
 *
 * For each #GdaHolder in @set, set the value if one is available in @tcnc.
 */
void
t_connection_load_variables (TConnection *tcnc, GdaSet *set)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	if (!set)
		return;
	g_return_if_fail (GDA_IS_SET (set));

	if (! tcnc->priv->variables)
		return;

	GSList *list;
	for (list = gda_set_get_holders (set); list; list = list->next) {
		GdaHolder *nh, *eh;
		nh = GDA_HOLDER (list->data);
		eh = gda_set_get_holder (tcnc->priv->variables, gda_holder_get_id (nh));
		if (eh) {
			if (gda_holder_get_g_type (nh) == gda_holder_get_g_type (eh)) {
				const GValue *cvalue;
				cvalue = gda_holder_get_value (eh);
				gda_holder_set_value (nh, cvalue, NULL);
			}
			else if (g_value_type_transformable (gda_holder_get_g_type (eh),
							     gda_holder_get_g_type (nh))) {
				const GValue *evalue;
				GValue *nvalue;
				evalue = gda_holder_get_value (eh);
				nvalue = gda_value_new (gda_holder_get_g_type (nh));
				if (g_value_transform (evalue, nvalue))
					gda_holder_take_value (nh, nvalue, NULL);
				else
					gda_value_free (nvalue);
			}
		}
	}	
}

/**
 * t_connection_is_ldap:
 * @tcnc: a #TConnection
 *
 * Returns: %TRUE if @tcnc proxies an LDAP connection
 */
gboolean
t_connection_is_ldap (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);

#ifdef HAVE_LDAP
	return GDA_IS_LDAP_CONNECTION (tcnc->priv->cnc) ? TRUE : FALSE;
#endif
	return FALSE;
}

#ifdef HAVE_LDAP

/**
 * t_connection_ldap_search:
 *
 * Executes an LDAP search. Wrapper around gda_data_model_ldap_new_with_config()
 *
 * Returns: (transfer full): a new #GdaDataModel, or %NULL on error
 */
GdaDataModel *
t_connection_ldap_search (TConnection *tcnc,
			  const gchar *base_dn, const gchar *filter,
			  const gchar *attributes, GdaLdapSearchScope scope,
			  GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (tcnc->priv->cnc), NULL);

	GdaDataModel *model;
	model = (GdaDataModel*) gda_data_model_ldap_new_with_config (GDA_CONNECTION (tcnc->priv->cnc), base_dn,
								     filter, attributes, scope);
	if (!model) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not execute LDAP search"));
		return NULL;
	}
	else {
		GdaDataModel *wrapped;
		wrapped = gda_data_access_wrapper_new (model);
		g_object_unref (model);
		/* force loading all the LDAP entries in memory to avoid
		 * having the GTK thread lock on LDAP searches */
		gda_data_model_get_n_rows (wrapped);
		return wrapped;
	}
}

/**
 * t_connection_ldap_describe_entry:
 * @tcnc: a #TConnection
 * @dn: the DN of the entry to describe
 * @callback: the callback to execute with the results
 * @cb_data: a pointer to pass to @callback
 * @error: a place to store errors, or %NULL
 *
 * Wrapper around gda_ldap_describe_entry().
 *
 * Returns: (transfer full): a new #GdaLdapEntry, or %NULL
 */
GdaLdapEntry *
t_connection_ldap_describe_entry (TConnection *tcnc, const gchar *dn, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), 0);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (tcnc->priv->cnc), 0);

	return gda_ldap_describe_entry (GDA_LDAP_CONNECTION (tcnc->priv->cnc), dn, error);
}

/**
 * t_connection_ldap_get_entry_children:
 * @tcnc: a #TConnection
 * @dn: the DN of the entry to get children from
 * @callback: the callback to execute with the results
 * @cb_data: a pointer to pass to @callback
 * @error: a place to store errors, or %NULL
 *
 * Wrapper around gda_ldap_get_entry_children().
 *
 * Returns: (transfer full): a %NULL terminated array of #GdaLdapEntry pointers, or %NULL if an error occurred
 */
GdaLdapEntry **
t_connection_ldap_get_entry_children (TConnection *tcnc, const gchar *dn,
				      gchar **attributes, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), 0);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (tcnc->priv->cnc), 0);

	return  gda_ldap_get_entry_children (GDA_LDAP_CONNECTION (tcnc->priv->cnc), dn, attributes, error);
}

/**
 * t_connection_ldap_get_base_dn:
 * @tcnc: a #TConnection
 *
 * wrapper for gda_ldap_connection_get_base_dn()
 *
 * Returns: the base DN or %NULL
 */
const gchar *
t_connection_ldap_get_base_dn (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (tcnc->priv->cnc), NULL);

	return gda_ldap_connection_get_base_dn (GDA_LDAP_CONNECTION (tcnc->priv->cnc));
}

/**
 * t_connection_describe_table:
 * @tcnc: a #TConnection
 * @table_name: a table name, not %NULL
 * @out_base_dn: (nullable) (transfer none): a place to store the LDAP search base DN, or %NULL
 * @out_filter: (nullable) (transfer none): a place to store the LDAP search filter, or %NULL
 * @out_attributes: (nullable) (transfer none): a place to store the LDAP search attributes, or %NULL
 * @out_scope: (nullable) (transfer none): a place to store the LDAP search scope, or %NULL
 * @error: a place to store errors, or %NULL
 */
gboolean
t_connection_describe_table  (TConnection *tcnc, const gchar *table_name,
			      const gchar **out_base_dn, const gchar **out_filter,
			      const gchar **out_attributes,
			      GdaLdapSearchScope *out_scope, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	g_return_val_if_fail (t_connection_is_ldap (tcnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	return gda_ldap_connection_describe_table (GDA_LDAP_CONNECTION (tcnc->priv->cnc),
						   table_name, out_base_dn, out_filter,
						   out_attributes, out_scope, error);
}

/**
 * t_connection_get_class_info:
 * @tcnc: a #TConnection
 * @classname: a class name
 *
 * proxy for gda_ldap_get_class_info.
 */
GdaLdapClass *
t_connection_get_class_info (TConnection *tcnc, const gchar *classname)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (t_connection_is_ldap (tcnc), NULL);

	return gda_ldap_get_class_info (GDA_LDAP_CONNECTION (tcnc->priv->cnc), classname);
}

/**
 * t_connection_get_top_classes:
 * @tcnc: a #TConnection
 *
 * proxy for gda_ldap_get_top_classes.
 */
const GSList *
t_connection_get_top_classes (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (t_connection_is_ldap (tcnc), NULL);

	return gda_ldap_get_top_classes (GDA_LDAP_CONNECTION (tcnc->priv->cnc));
}

/**
 * t_connection_declare_table:
 * @tcnc: a #TConnection
 *
 * Wrapper around gda_ldap_connection_declare_table()
 */
gboolean
t_connection_declare_table (TConnection *tcnc,
			    const gchar *table_name,
			    const gchar *base_dn,
			    const gchar *filter,
			    const gchar *attributes,
			    GdaLdapSearchScope scope,
			    GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	g_return_val_if_fail (t_connection_is_ldap (tcnc), FALSE);

	return gda_ldap_connection_declare_table (GDA_LDAP_CONNECTION (tcnc->priv->cnc),
						  table_name, base_dn, filter,
						  attributes, scope, error);
}

/**
 * t_connection_undeclare_table:
 * @tcnc: a #TConnection
 *
 * Wrapper around gda_ldap_connection_undeclare_table()
 */
gboolean
t_connection_undeclare_table (TConnection *tcnc,
			      const gchar *table_name,
			      GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), FALSE);
	g_return_val_if_fail (t_connection_is_ldap (tcnc), FALSE);

	return gda_ldap_connection_undeclare_table (GDA_LDAP_CONNECTION (tcnc->priv->cnc),
						    table_name, error);
}

#endif /* HAVE_LDAP */

/**
 * t_connection_set_query_buffer:
 * @tcnc: a #TConnection
 * @sql: (nullable): a string, or %NULL
 *
 * Set @tcnc's associated query buffer
 */
void
t_connection_set_query_buffer (TConnection *tcnc, const gchar *sql)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	g_free (tcnc->priv->query_buffer);
	tcnc->priv->query_buffer = sql ? g_strdup (sql) : NULL;
}

/**
 * t_connection_get_query_buffer:
 * @tcnc: a #TConnection
 *
 * Get @tcnc's associated query buffer.
 *
 * Returns: the query buffer
 */
const gchar *
t_connection_get_query_buffer (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	return tcnc->priv->query_buffer;
}


/**
 * t_connection_get_all_infos:
 * @tcnc: a #TConnection
 *
 * Returns: (transfer none): a #GdaSet containing all the names informations anout @tcnc
 */
GdaSet *
t_connection_get_all_infos (TConnection *tcnc)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	/* refresh some information here */
	/* provider name */
	GdaHolder *h;
	h = gda_set_get_holder (tcnc->priv->infos, "db_provider");
	if (!h) {
		h = gda_holder_new (G_TYPE_STRING, "db_provider");
		g_object_set (h,
			      "description", _("Database provider"), NULL);
		gda_set_add_holder (tcnc->priv->infos, h);
	}
	g_assert (gda_holder_set_value_str (h, NULL, gda_connection_get_provider_name (tcnc->priv->cnc), NULL));

	/* database name */
	h = gda_set_get_holder (tcnc->priv->infos, "db_name");
	if (!h) {
		h = gda_holder_new (G_TYPE_STRING, "db_name");
		g_object_set (h,
			      "description", _("Database name"), NULL);
		gda_set_add_holder (tcnc->priv->infos, h);
	}
	const gchar *str;
	str = gda_connection_get_cnc_string (tcnc->priv->cnc);
	GdaQuarkList *ql;
	ql = gda_quark_list_new_from_string (str);
	if (ql) {
		const gchar *name;
		name = gda_quark_list_find (ql, "DB_NAME");
		if (name)
			g_assert (gda_holder_set_value_str (h, NULL, name, NULL));
		else
			gda_holder_force_invalid (h);
		gda_quark_list_free (ql);
	}
	else
		gda_holder_force_invalid (h);

	return tcnc->priv->infos;
}
