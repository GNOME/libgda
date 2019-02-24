/*
 * Copyright (C) 2007 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Nathan <nbinont@yahoo.ca>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2012 Daniel Mustieles <daniel.mustieles@gmail.com>
 * Copyright (C) 2014 Anders Jonsson <anders.jonsson@norsjovallen.se>
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

#include "t-app.h"
#include "t-context.h"
#include "t-term-context.h"
#include "t-connection.h"
#include "t-utils.h"
#include "t-config-info.h"
#include "web-server.h"
#include <unistd.h>
#include <sys/types.h>
#ifndef G_OS_WIN32
   #include <pwd.h>
#endif
#include <errno.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <base/base-tool-input.h>
#include <sql-parser/gda-sql-parser.h>
#include <virtual/libgda-virtual.h>
#include <libgda/gda-blob-op.h>
#ifdef HAVE_GTK_CLASSES
  #include <browser/login-dialog.h>
  #include <browser/auth-dialog.h>
  #include <browser/browser-window.h>
#endif

#define T_APP_LOCK(self) g_rec_mutex_lock(& ((TApp*)(self))->priv->rmutex)
#define T_APP_UNLOCK(self) g_rec_mutex_unlock(& ((TApp*)(self))->priv->rmutex)

/*
 * Main static functions
 */
static void t_app_class_init (TAppClass *klass);
static void t_app_init (TApp *app);
static void t_app_dispose (GObject *object);

#ifdef HAVE_GTK_CLASSES
static void t_app_shutdown (GApplication *application);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* pointer to the singleton */
TApp *global_t_app = NULL;

/* signals */
enum {
	CNC_ADDED,
	CNC_REMOVED,
	QUIT_REQUESTED,
	LAST_SIGNAL
};

static gint t_app_signals[LAST_SIGNAL] = { 0, 0, 0 };
struct _TAppPrivate {
	TAppFeatures features;

	/* if T_APP_TERM_CONSOLE is featured */
	TContext *term_console;

	/* if T_APP_T  is featured */

	/* if T_APP_WEB_SERVER is featured */
#ifdef HAVE_LIBSOUP
	WebServer *server;
#endif

	GRecMutex   rmutex;
	GSList     *tcnc_list; /* list all the TConnection */
	GSList     *tcontext_list; /* list of all the TContext */

	GHashTable *parameters; /* key = name, value = G_TYPE_STRING GdaHolder */
	GdaSet     *options;
	GHashTable *mem_data_models; /* key = name, value = the named GdaDataModel, ref# kept in hash table */
};

GType
t_app_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (TAppClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) t_app_class_init,
                        NULL,
                        NULL,
                        sizeof (TApp),
                        0,
                        (GInstanceInitFunc) t_app_init,
                        0
                };


                g_mutex_lock (&registering);
                if (type == 0) {
#ifdef HAVE_GTK_CLASSES
			type = g_type_register_static (GTK_TYPE_APPLICATION, "TApp", &info, 0);
#else
                        type = g_type_register_static (G_TYPE_APPLICATION, "TApp", &info, 0);
#endif
		}
                g_mutex_unlock (&registering);
        }
        return type;
}

static void
t_app_class_init (TAppClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        parent_class = g_type_class_peek_parent (klass);

        t_app_signals[CNC_ADDED] =
                g_signal_new ("connection-added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TAppClass, connection_added),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, T_TYPE_CONNECTION);

        t_app_signals[CNC_REMOVED] =
                g_signal_new ("connection-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TAppClass, connection_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, T_TYPE_CONNECTION);

        t_app_signals[QUIT_REQUESTED] =
                g_signal_new ("quit-requested",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (TAppClass, quit_requested),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

        klass->connection_added = NULL;
        klass->connection_removed = NULL;
        klass->quit_requested = NULL;

#ifdef HAVE_GTK_CLASSES
	GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
	application_class->shutdown = t_app_shutdown;
#endif

        object_class->dispose = t_app_dispose;
}

static void
t_app_init (TApp *self)
{
        self->priv = g_new0 (TAppPrivate, 1);
	g_rec_mutex_init (& self->priv->rmutex);
}

static void
t_app_dispose (GObject *object)
{
        TApp *self;

        g_return_if_fail (IS_T_APP (object));

        self = T_APP (object);
        if (self->priv) {
		if (self->priv->mem_data_models)
			g_hash_table_destroy (self->priv->mem_data_models);

		while (self->priv->tcontext_list)
			g_object_unref (G_OBJECT (self->priv->tcontext_list->data));

		while (self->priv->tcnc_list) {
			TConnection *tcnc;
			tcnc = T_CONNECTION (self->priv->tcnc_list->data);
			self->priv->tcnc_list = g_slist_delete_link (self->priv->tcnc_list, self->priv->tcnc_list);
			g_signal_emit (self, t_app_signals[CNC_REMOVED], 0, tcnc);
			g_object_unref (G_OBJECT (tcnc));
		}

		g_rec_mutex_clear (& self->priv->rmutex);
                g_free (self->priv);
                self->priv = NULL;
        }
	global_t_app = NULL;

        /* parent class */
        parent_class->dispose (object);


}
#ifdef HAVE_GTK_CLASSES
static void
t_app_shutdown (GApplication *application)
{
	GList *windows;
	windows = gtk_application_get_windows (GTK_APPLICATION (application));
	if (windows) {
		GList *list;
		windows = g_list_copy (windows);
		for (list = windows; list; list = list->next)
			gtk_widget_destroy (GTK_WIDGET (list->data));
		g_list_free (windows);
	}

	G_APPLICATION_CLASS (parent_class)->shutdown (application);
}
#endif

static void build_commands (TApp *self, TAppFeatures features);

/**
 * t_app_setup:
 * @features: a set of features which will be used by some consoles
 *
 * Sets up the unique #TApp object. Use the t_app_add_feature() to add more features
 */
void
t_app_setup (TAppFeatures features)
{
	if (global_t_app == NULL) {
		global_t_app = g_object_new (T_APP_TYPE, "flags", G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_NON_UNIQUE, NULL);

		/* options */
		global_t_app->priv->options = gda_set_new_inline (3,
								  "csv_names_on_first_line", G_TYPE_BOOLEAN, FALSE,
								  "csv_quote", G_TYPE_STRING, "\"",
								  "csv_separator", G_TYPE_STRING, ",");

		GdaHolder *opt;
		GValue *value;
		value = gda_value_new (G_TYPE_STRING);
		g_value_set_string (value, _("Set to TRUE when the 1st line of a CSV file holds column names"));
		opt = gda_set_get_holder (global_t_app->priv->options, "csv_names_on_first_line");
		g_object_set (opt, "description", value, NULL);
		g_object_set_data ((GObject*) opt, "csv", "NAMES_ON_FIRST_LINE");

		g_value_set_string (value, _("Quote character for CSV format"));
		opt = gda_set_get_holder (global_t_app->priv->options, "csv_quote");
		g_object_set (opt, "description", value, NULL);
		g_object_set_data ((GObject*) opt, "csv", "QUOTE");

		g_value_set_string (value, _("Separator character for CSV format"));
		opt = gda_set_get_holder (global_t_app->priv->options, "csv_separator");
		g_object_set (opt, "description", value, NULL);
		g_object_set_data ((GObject*) opt, "csv", "SEPARATOR");
		gda_value_free (value);

#ifdef HAVE_LDAP
		opt = gda_holder_new_string ("ldap_dn", "dn");
		g_object_set (opt, "description", _("Defines how the DN column is handled for LDAP searched (among \"dn\", \"rdn\" and \"none\")"), NULL);
		gda_set_add_holder (T_APP (global_t_app)->priv->options, opt);
		g_object_unref (opt);

		opt = gda_holder_new_string ("ldap_attributes", T_DEFAULT_LDAP_ATTRIBUTES);
		g_object_set (opt, "description", _("Defines the LDAP attributes which are fetched by default by LDAP commands"), NULL);
		gda_set_add_holder (T_APP (global_t_app)->priv->options, opt);
		g_object_unref (opt);
#endif

		global_t_app->priv->mem_data_models = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

		/* parameters */
		global_t_app->priv->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

		/* building commands lists */
		build_commands (global_t_app, features);

		t_app_add_feature (features);
	}
	else
		g_assert_not_reached ();
}

#ifdef HAVE_GTK_CLASSES
static gboolean
idle_create_window (GApplication *app)
{
	GSList *cnclist;
	cnclist = t_app_get_all_connections ();
	g_assert (cnclist);

	TConnection *tcnc;
	tcnc = T_CONNECTION (cnclist->data);

	GtkWidget *bwin;
	bwin = (GtkWidget*) browser_window_new (tcnc, NULL);
	gtk_window_set_default_size ((GtkWindow*) bwin, 640, 480);
	gtk_widget_show (bwin);

	g_application_release (app); /* release now that the 1st window has been created, cf. t_app_add_feature() where
				      * g_application_hold() is called */

	g_object_unref (G_OBJECT (global_t_app));

	return FALSE;
}
#endif


/**
 * t_app_add_feature:
 * @feature: a feature to add
 *
 * Changes the global application configuration to remove a feature
 */
void
t_app_add_feature (TAppFeatures feature)
{
	/* specific features */
	if ((feature & T_APP_TERM_CONSOLE) &&
	    !(global_t_app->priv->features & T_APP_TERM_CONSOLE)) {
		g_assert (! global_t_app->priv->term_console);

		global_t_app->priv->term_console = T_CONTEXT (t_term_context_new ("TERM"));
		t_utils_term_compute_color_attribute ();

		g_application_hold (G_APPLICATION (global_t_app));

		global_t_app->priv->features |= T_APP_TERM_CONSOLE;
	}

#ifdef HAVE_GTK_CLASSES
	if ((feature & T_APP_BROWSER) &&
	    !(global_t_app->priv->features & T_APP_BROWSER)) {
		g_application_hold (G_APPLICATION (global_t_app)); /* up until the 1st window is created */
		g_object_ref (G_OBJECT (global_t_app));
		g_idle_add ((GSourceFunc) idle_create_window, global_t_app);
		global_t_app->priv->features |= T_APP_BROWSER;
	}
#endif

#ifdef HAVE_LIBSOUP
	if (feature & T_APP_WEB_SERVER) {
		TO_IMPLEMENT;
	}
#endif
}

/**
 * t_app_remove_feature:
 * @feature: a feature to remove
 *
 * Changes the global application configuration to remove a feature
 */
void
t_app_remove_feature (TAppFeatures feature)
{
	if (feature & T_APP_TERM_CONSOLE) {
		g_object_unref (global_t_app->priv->term_console);
		global_t_app->priv->term_console = NULL;

		global_t_app->priv->features &= ~T_APP_TERM_CONSOLE;
		g_application_release (G_APPLICATION (global_t_app));
	}

	if (feature & T_APP_BROWSER) {
		TO_IMPLEMENT;
		global_t_app->priv->features &= ~T_APP_BROWSER;
	}

	if (global_t_app->priv->features == T_APP_NO_FEATURE)
		t_app_request_quit ();
}

/**
 * t_app_cleanup:
 *
 * Cleans up everything!
 */
void
t_app_cleanup (void)
{
	g_assert (global_t_app);
	g_object_unref (global_t_app);
	global_t_app = NULL;
}

/**
 * t_app_get:
 *
 * Obtain a pointer to the unique #TApp
 *
 * Returns: (transfer none): the pointer
 */
TApp *
t_app_get (void)
{
	g_assert (global_t_app);
	return global_t_app;
}

static gboolean
end_requested (GApplication *app)
{
	g_application_quit (app);
	return FALSE; /* remove idle */
}

/**
 * t_app_request_quit:
 *
 * Requests that the application quit; this function initiates the quiting sequence and returns.
 */
void
t_app_request_quit (void)
{
	g_assert (global_t_app);

	g_signal_emit (global_t_app, t_app_signals[QUIT_REQUESTED], 0);
	g_idle_add ((GSourceFunc) end_requested, G_APPLICATION (global_t_app));
}

/**
 * t_app_lock:
 * 
 * Locks the global #TApp object for the current thread.
 */
void
t_app_lock (void)
{
	g_assert (global_t_app);
	T_APP_LOCK (global_t_app);
}

/**
 * t_app_unlock:
 * 
 * Locks the global #TApp object for the current thread.
 */
void
t_app_unlock (void)
{
	g_assert (global_t_app);
	T_APP_UNLOCK (global_t_app);
}

static void
conn_status_changed_cb (TConnection *tcnc, GdaConnectionStatus status, gpointer data)
{
	g_assert (global_t_app);

	if (status == GDA_CONNECTION_STATUS_CLOSED) {
		T_APP_LOCK (global_t_app);
#ifdef HAVE_GTK_CLASSES
		GList *windows;
		windows = gtk_application_get_windows (GTK_APPLICATION (global_t_app));
		if (windows) {
			GList *list;
			windows = g_list_copy (windows);
			for (list = windows; list; list = list->next) {
				if (browser_window_get_connection (BROWSER_WINDOW (list->data)) == tcnc)
					gtk_widget_destroy (GTK_WIDGET (list->data));
			}
			g_list_free (windows);
		}

		if (! gtk_application_get_windows (GTK_APPLICATION (global_t_app))) {
			/* call t_app_remove_feature when there are no more opened windows*/
			t_app_remove_feature (T_APP_BROWSER);
		}
#endif

		global_t_app->priv->tcnc_list = g_slist_remove (global_t_app->priv->tcnc_list, tcnc);
		g_signal_emit (global_t_app, t_app_signals[CNC_REMOVED], 0, tcnc);
		g_object_unref (tcnc);
		T_APP_UNLOCK (global_t_app);
	}
}

/**
 * t_app_add_tcnc:
 * @tcnc: (transfer full): a #TConnection
 *
 * Make @self handle @tcnc. This function is thread-safe.
 */
void
t_app_add_tcnc (TConnection *tcnc)
{
	g_return_if_fail (T_IS_CONNECTION (tcnc));
	g_assert (global_t_app);

	T_APP_LOCK (global_t_app);
	if (g_slist_find (global_t_app->priv->tcnc_list, tcnc))
		g_warning ("TConnection is already known to TApp!");
	else {
		global_t_app->priv->tcnc_list = g_slist_append (global_t_app->priv->tcnc_list, tcnc);
		g_signal_connect (tcnc, "status-changed",
				  G_CALLBACK (conn_status_changed_cb), NULL);
		g_signal_emit (global_t_app, t_app_signals[CNC_ADDED], 0, tcnc);
	}
	T_APP_UNLOCK (global_t_app);
}

/**
 * t_app_open_connections:
 * @argc: the size of @argv
 * @argv: an array of strings each representing a connection to open
 * @error: a place to store errors, or %NULL
 *
 * Opens the connections:
 * - listed in @argv
 * - from the GDA_SQL_CNC environment variable
 *
 * Returns: %TRUE if all the connections have been opened
 */
gboolean
t_app_open_connections (gint argc, const gchar *argv[], GError **error)
{
	g_assert (argc >= 0);
#ifdef HAVE_GTK_CLASSES
	if (argc == 0) {
		LoginDialog *dlg;
		dlg = login_dialog_new (NULL);
		TConnection *tcnc = NULL;
		tcnc = login_dialog_run_open_connection (dlg, TRUE, error);
		gtk_widget_destroy (GTK_WIDGET (dlg));

		return tcnc ? TRUE : FALSE;
	}
	else {
		AuthDialog *dlg;
		dlg = auth_dialog_new (NULL);
		gint i;
		for (i = 0; i < argc; i++) {
			if (! auth_dialog_add_cnc_string (AUTH_DIALOG (dlg), argv[i], error)) {
				gtk_widget_destroy (GTK_WIDGET (dlg));
				return FALSE;
			}
		}
		gboolean result;
		result = auth_dialog_run (AUTH_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));
		return result;
	}
#else
	TContext *term_console;
	FILE *ostream = NULL;
	term_console = t_app_get_term_console ();
	if (term_console)
		ostream = t_context_get_output_stream (term_console, NULL);

	guint i;
	for (i = 0; i < (guint) argc; i++) {
		/* open connection */
		TConnection *tcnc;
		GdaDsnInfo *info = NULL;
		gchar *str;

		/* Use argv[i] to open a connection */
                info = gda_config_get_dsn_info (argv[i]);
		if (info)
			str = g_strdup (info->name);
		else
			str = g_strdup_printf (T_CNC_NAME_PREFIX "%u", i);
		if (!ostream) {
			gchar *params, *prov, *user;
			gda_connection_string_split (argv[i], &params, &prov, &user, NULL);
			g_print (_("Opening connection '%s' for: "), str);
			if (prov)
				g_print ("%s://", prov);
			if (user)
				g_print ("%s@", user);
			if (params)
				g_print ("%s", params);
			g_print ("\n");
			g_free (params);
			g_free (prov);
			g_free (user);
		}
		tcnc = t_connection_open (str, argv[i], NULL, TRUE, error);
		g_free (str);
		if (tcnc)
			t_context_set_connection (term_console, tcnc);
		else
			return FALSE;
	}

	return TRUE;
#endif
}

/**
 * t_app_get_all_connections:
 *
 * This function is thread-safe.
 *
 * Returns: (transfer none) (element-type TConnection): a read-only list of all the #TConnection object known to @self
 */
GSList *
t_app_get_all_connections (void)
{
	g_assert (global_t_app);

	GSList *retlist;
	T_APP_LOCK (global_t_app);
	retlist = global_t_app->priv->tcnc_list;
	T_APP_UNLOCK (global_t_app);
	return retlist;
}

static void
model_connection_added_cb (G_GNUC_UNUSED TApp *tapp, TConnection *tcnc, GdaDataModel *model)
{
	GList *values;
	GValue *value;
	
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), t_connection_get_name (tcnc));
	values = g_list_prepend (NULL, value);
	g_value_set_object ((value = gda_value_new (T_TYPE_CONNECTION)), tcnc);
	values = g_list_prepend (values, value);

	g_assert (gda_data_model_append_values (model, values, NULL) >= 0);

	g_list_foreach (values, (GFunc) gda_value_free, NULL);
	g_list_free (values);
}

static void
model_connection_removed_cb (G_GNUC_UNUSED TApp *tapp, TConnection *tcnc, GdaDataModel *model)
{
	gint i, nrows;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		value = gda_data_model_get_value_at (model, 0, i, NULL);
		g_assert (value);
		if (g_value_get_object (value) == tcnc) {
			g_assert (gda_data_model_remove_row (model, i, NULL));
			break;
		}
	}
}
/**
 * t_app_get_all_connections_m:
 *
 * Creates a #GdaDataModel with 2 columns: T_TYPE_CONNECTION (#TConnection) and G_TYPE_STRING (connection's name)
 *
 * Returns: (transfer none): a #GdaDataModel
 */
GdaDataModel *
t_app_get_all_connections_m (void)
{
	static GdaDataModel *model = NULL;
	if (!model) {
		model = gda_data_model_array_new_with_g_types (2,
							       T_TYPE_CONNECTION,
							       G_TYPE_STRING);
		/* initial filling */
		GSList *list;
		for (list = t_app_get_all_connections (); list; list = list->next)
			model_connection_added_cb (t_app_get(), T_CONNECTION (list->data), model);

		g_signal_connect (t_app_get (), "connection-added",
				  G_CALLBACK (model_connection_added_cb), model);
		g_signal_connect (t_app_get (), "connection-removed",
				  G_CALLBACK (model_connection_removed_cb), model);
	}

	return model;
}

/**
 * t_app_get_term_console:
 *
 * Returns: the term console, or %NULL if no term console exists
 */
TContext *
t_app_get_term_console (void)
{
	g_assert (global_t_app);
	return global_t_app->priv->term_console;
}

/**
 * t_app_get_options:
 *
 * Returns: (transfer none): the options #GdaSet
 */
GdaSet *
t_app_get_options (void)
{
	g_assert (global_t_app);
	return global_t_app->priv->options;
}

/**
 * t_app_store_data_model:
 * @model: (transfer none): a #GdaDataModel
 * @name: (transfer none): the name to store it as
 *
 * Stores a #GdaDataModel, indexing it as @name
 * If a data model with the same name already exists, then it is first removed
 */
void
t_app_store_data_model (GdaDataModel *model, const gchar *name)
{
	g_assert (global_t_app);
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (name && *name);

	g_hash_table_insert (global_t_app->priv->mem_data_models, g_strdup (name), g_object_ref (model));
}

/**
 * t_app_fetch_data_model:
 * @name: a name
 *
 * Retreives a data model previously stored using t_app_store_data_model()
 *
 * Returns: (transfer none): the #GdaDataModel, or %NULL if not found
 */
GdaDataModel *
t_app_fetch_data_model (const gchar *name)
{
	g_assert (global_t_app);
	g_return_val_if_fail (name && *name, NULL);

	return g_hash_table_lookup (global_t_app->priv->mem_data_models, name);
}

/*
 * _t_app_add_context:
 * @console: (transfer full):
 */
void
_t_app_add_context (TContext *console)
{
	g_assert (global_t_app);
	g_return_if_fail (console);

	T_APP_LOCK (global_t_app);
	if (g_slist_find (global_t_app->priv->tcontext_list, console))
		g_warning ("TContext is already known to TApp!");
	else
		global_t_app->priv->tcontext_list = g_slist_append (global_t_app->priv->tcontext_list, console);
	T_APP_UNLOCK (global_t_app);
}

void
_t_app_remove_context (TContext *console)
{
	g_assert (global_t_app);
	g_return_if_fail (console);

	T_APP_LOCK (global_t_app);
	if (!g_slist_find (global_t_app->priv->tcontext_list, console))
		g_warning ("TContext is not known to TApp!");
	else
		global_t_app->priv->tcontext_list = g_slist_remove (global_t_app->priv->tcontext_list, console);
	T_APP_UNLOCK (global_t_app);
}

/**
 * t_app_get_parameter:
 * @name: the name of the parameter requested
 *
 * Get the #GValue of a parameter
 *
 * Returns: (transfer full): a new #GValue, or %NULL if the parameter has not been found
 */ 
GValue *
t_app_get_parameter_value (const gchar *name)
{
	g_assert (global_t_app);
	g_return_val_if_fail (name, NULL);

	T_APP_LOCK (global_t_app);
	GdaHolder *h;
	GValue *value = NULL;
	h = g_hash_table_lookup (global_t_app->priv->parameters, name);
	if (h) {
		const GValue *cvalue;
		cvalue = gda_holder_get_value (h);
		if (cvalue)
			value = gda_value_copy (cvalue);
	}
	T_APP_UNLOCK (global_t_app);
	return value;
}

static ToolCommandResult *gda_internal_command_history (ToolCommand *command, guint argc, const gchar **argv,
							TContext *console, GError **error);
static ToolCommandResult *gda_internal_command_dict_sync (ToolCommand *command, guint argc, const gchar **argv,
							  TContext *console, GError **error);
static ToolCommandResult *gda_internal_command_list_tables (ToolCommand *command, guint argc, const gchar **argv,
							    TContext *console, GError **error);
static ToolCommandResult *gda_internal_command_list_views (ToolCommand *command, guint argc, const gchar **argv,
							   TContext *console, GError **error);
static ToolCommandResult *gda_internal_command_list_schemas (ToolCommand *command, guint argc, const gchar **argv,
							     TContext *console, GError **error);
static ToolCommandResult *gda_internal_command_detail (ToolCommand *command, guint argc, const gchar **argv,
						       TContext *console, GError **error);

static ToolCommandResult *extra_command_copyright (ToolCommand *command, guint argc, const gchar **argv,
						   TContext *console, GError **error);
static ToolCommandResult *extra_command_option (ToolCommand *command, guint argc, const gchar **argv,
						TContext *console, GError **error);
static ToolCommandResult *extra_command_info (ToolCommand *command, guint argc, const gchar **argv,
					      TContext *console, GError **error);
static ToolCommandResult *extra_command_quit (ToolCommand *command, guint argc, const gchar **argv,
					      TContext *console, GError **error);
static ToolCommandResult *extra_command_cd (ToolCommand *command, guint argc, const gchar **argv,
					    TContext *console, GError **error);
static ToolCommandResult *extra_command_set_output (ToolCommand *command, guint argc, const gchar **argv,
						    TContext *console, GError **error);
static ToolCommandResult *extra_command_set_output_format (ToolCommand *command, guint argc, const gchar **argv,
							   TContext *console, GError **error);
static ToolCommandResult *extra_command_set_input (ToolCommand *command, guint argc, const gchar **argv,
						   TContext *console, GError **error);
static ToolCommandResult *extra_command_echo (ToolCommand *command, guint argc, const gchar **argv,
					      TContext *console, GError **error);
static ToolCommandResult *extra_command_qecho (ToolCommand *command, guint argc, const gchar **argv,
					       TContext *console, GError **error);
static ToolCommandResult *extra_command_list_dsn (ToolCommand *command, guint argc, const gchar **argv,
						  TContext *console, GError **error);
static ToolCommandResult *extra_command_create_dsn (ToolCommand *command, guint argc, const gchar **argv,
						    TContext *console, GError **error);
static ToolCommandResult *extra_command_remove_dsn (ToolCommand *command, guint argc, const gchar **argv,
						    TContext *console, GError **error);
static ToolCommandResult *extra_command_list_providers (ToolCommand *command, guint argc, const gchar **argv,
							TContext *console, GError **error);
static ToolCommandResult *extra_command_manage_cnc (ToolCommand *command, guint argc, const gchar **argv,
						    TContext *console, GError **error);
static gchar **extra_command_manage_cnc_compl (const gchar *text, gpointer user_data);
static ToolCommandResult *extra_command_close_cnc (ToolCommand *command, guint argc, const gchar **argv,
						   TContext *console, GError **error);
static ToolCommandResult *extra_command_bind_cnc (ToolCommand *command, guint argc, const gchar **argv,
						  TContext *console, GError **error);
static ToolCommandResult *extra_command_edit_buffer (ToolCommand *command, guint argc, const gchar **argv,
						     TContext *console, GError **error);
static ToolCommandResult *extra_command_reset_buffer (ToolCommand *command, guint argc, const gchar **argv,
						      TContext *console, GError **error);
static ToolCommandResult *extra_command_show_buffer (ToolCommand *command, guint argc, const gchar **argv,
						     TContext *console, GError **error);
static ToolCommandResult *extra_command_exec_buffer (ToolCommand *command, guint argc, const gchar **argv,
						     TContext *console, GError **error);
static ToolCommandResult *extra_command_write_buffer (ToolCommand *command, guint argc, const gchar **argv,
						      TContext *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_to_dict (ToolCommand *command, guint argc, const gchar **argv,
							      TContext *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_from_dict (ToolCommand *command, guint argc, const gchar **argv,
								TContext *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_list_dict (ToolCommand *command, guint argc, const gchar **argv,
								TContext *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_delete_dict (ToolCommand *command, guint argc, const gchar **argv,
								  TContext *console, GError **error);
static ToolCommandResult *extra_command_set (ToolCommand *command, guint argc, const gchar **argv,
					     TContext *console, GError **error);
static ToolCommandResult *extra_command_unset (ToolCommand *command, guint argc, const gchar **argv,
					       TContext *console, GError **error);
static ToolCommandResult *extra_command_data_sets_list (ToolCommand *command, guint argc, const gchar **argv,
							TContext *console, GError **error);
static ToolCommandResult *extra_command_data_set_move (ToolCommand *command, guint argc, const gchar **argv,
						       TContext *console, GError **error);
static ToolCommandResult *extra_command_data_set_grep (ToolCommand *command, guint argc, const gchar **argv,
						       TContext *console, GError **error);
static ToolCommandResult *extra_command_data_set_show (ToolCommand *command, guint argc, const gchar **argv,
						       TContext *console, GError **error);

static ToolCommandResult *extra_command_data_set_rm (ToolCommand *command, guint argc, const gchar **argv,
						     TContext *console, GError **error);
static ToolCommandResult *extra_command_data_set_import (ToolCommand *command, guint argc, const gchar **argv,
							 TContext *console, GError **error);

static ToolCommandResult *extra_command_graph (ToolCommand *command, guint argc, const gchar **argv,
					       TContext *console, GError **error);
#ifdef HAVE_LIBSOUP
static ToolCommandResult *extra_command_httpd (ToolCommand *command, guint argc, const gchar **argv,
					       TContext *console, GError **error);
#endif
#ifdef NONE
static ToolCommandResult *extra_command_lo_update (ToolCommand *command, guint argc, const gchar **argv,
						   TContext *console, GError **error);
#endif
static ToolCommandResult *extra_command_export (ToolCommand *command, guint argc, const gchar **argv,
						TContext *console, GError **error);
static ToolCommandResult *extra_command_set2 (ToolCommand *command, guint argc, const gchar **argv,
					      TContext *console, GError **error);
static ToolCommandResult *extra_command_pivot (ToolCommand *command, guint argc, const gchar **argv,
					       TContext *console, GError **error);

static ToolCommandResult *extra_command_declare_fk (ToolCommand *command, guint argc, const gchar **argv,
						    TContext *console, GError **error);

static ToolCommandResult *extra_command_undeclare_fk (ToolCommand *command, guint argc, const gchar **argv,
						      TContext *console, GError **error);
#ifdef HAVE_LDAP
static ToolCommandResult *extra_command_ldap_search (ToolCommand *command, guint argc, const gchar **argv,
						     TContext *console, GError **error);
static ToolCommandResult *extra_command_ldap_descr (ToolCommand *command, guint argc, const gchar **argv,
						    TContext *console, GError **error);
static ToolCommandResult *extra_command_ldap_mv (ToolCommand *command, guint argc, const gchar **argv,
						 TContext *console, GError **error);
static ToolCommandResult *extra_command_ldap_mod (ToolCommand *command, guint argc, const gchar **argv,
						  TContext *console, GError **error);
#endif

/*
 * Rem: there is a @self to allow TApp's reinitialization
 */
static void
build_commands (TApp *self, TAppFeatures features)
{
	g_assert (self);
#ifdef HAVE_LIBSOUP
	self->web_commands = base_tool_command_group_new ();
#endif
	self->term_commands = base_tool_command_group_new ();

	ToolCommand *c;

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [<FILE>]"), "s");
	c->description = _("Show commands history, or save it to file");
	c->command_func = (ToolCommandFunc) gda_internal_command_history; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<META DATA TYPE>]"), "meta");
	c->description = _("Force reading the database meta data (or part of the meta data, ex:\"tables\")");
	c->command_func = (ToolCommandFunc) gda_internal_command_dict_sync;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s <fkname> <tableA>(<colA>,...) <tableB>(<colB>,...)"), "fkdeclare");
	c->description = _("Declare a new foreign key (not actually in database): tableA references tableB");
	c->command_func = (ToolCommandFunc) extra_command_declare_fk;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s <fkname> <tableA> <tableB>"), "fkundeclare");
	c->description = _("Un-declare a foreign key (not actually in database)");
	c->command_func = (ToolCommandFunc) extra_command_undeclare_fk;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<TABLE>]"), "dt");
	c->description = _("List all tables (or named table)");
	c->command_func = (ToolCommandFunc) gda_internal_command_list_tables;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<VIEW>]"), "dv");
	c->description = _("List all views (or named view)");
	c->command_func = (ToolCommandFunc) gda_internal_command_list_views;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<SCHEMA>]"), "dn");
	c->description = _("List all schemas (or named schema)");
	c->command_func = (ToolCommandFunc) gda_internal_command_list_schemas;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<OBJ_NAME>|<SCHEMA>.*]"), "d");
	c->description = _("Describe object or full list of objects");
	c->command_func = (ToolCommandFunc) gda_internal_command_detail;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<TABLE1> [<TABLE2>...]]"), "graph");
	c->description = _("Create a graph of all or the listed tables");
	c->command_func = (ToolCommandFunc) extra_command_graph; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

#ifdef HAVE_LIBSOUP
	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [<port> [<authentication token>]]"), "http");
	c->description = _("Start/stop embedded HTTP server (on given port or on 12345 by default)");
	c->command_func = (ToolCommandFunc) extra_command_httpd;
	base_tool_command_group_add (self->term_commands, c);
#endif

	/* specific commands */
	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [[<CNC_NAME>] [<DSN>|<CONNECTION STRING>]]"), "c");
	c->description = _("Opens a new connection or lists opened connections");
	c->command_func = (ToolCommandFunc) extra_command_manage_cnc;
	c->completion_func = extra_command_manage_cnc_compl;
	c->completion_func_data = self;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [<CNC_NAME>]"), "close");
	c->description = _("Close a connection");
	c->command_func = (ToolCommandFunc) extra_command_close_cnc;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s <CNC NAME> <OBJ NAME> [<OBJ NAME> ...]"), "bind");
	c->description = _("Bind connections or datasets (<OBJ NAME>) into a single new one (allowing SQL commands to be executed across multiple connections and/or datasets)");
	c->command_func = (ToolCommandFunc) extra_command_bind_cnc;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s [<DSN>]"), "l");
	c->description = _("List all DSN (or specified DSN's attributes)");
	c->command_func = (ToolCommandFunc) extra_command_list_dsn;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s <DSN_NAME> <DSN_DEFINITION> [<DESCRIPTION>]"), "lc");
	c->description = _("Create (or modify) a DSN");
	c->command_func = (ToolCommandFunc) extra_command_create_dsn;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s <DSN_NAME> [<DSN_NAME>...]"), "lr");
	c->description = _("Remove a DSN");
	c->command_func = (ToolCommandFunc) extra_command_remove_dsn;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s [<PROVIDER>]"), "L");
	c->description = _("List all installed database providers (or named one's attributes)");
	c->command_func = (ToolCommandFunc) extra_command_list_providers;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s <FILE>"), "i");
	c->description = _("Execute commands from file");
	c->command_func = (ToolCommandFunc) extra_command_set_input; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [<FILE>]"), "o");
	c->description = _("Send output to a file or |pipe");
	c->command_func = (ToolCommandFunc) extra_command_set_output; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [<TEXT>]"), "echo");
	c->description = _("Print TEXT or an empty line to standard output");
	c->command_func = (ToolCommandFunc) extra_command_echo;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [<TEXT>]"), "qecho");
	c->description = _("Send TEXT or an empty line to current output stream");
	c->command_func = (ToolCommandFunc) extra_command_qecho;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = "q";
	c->description = _("Quit");
	c->command_func = (ToolCommandFunc) extra_command_quit; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [<DIR>]"), "cd");
	c->description = _("Change the current working directory");
	c->command_func = (ToolCommandFunc) extra_command_cd;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = "copyright";
	c->description = _("Show usage and distribution terms");
	c->command_func = (ToolCommandFunc) extra_command_copyright;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [<NAME> [<VALUE>]]"), "option");
	c->description = _("Set or show an option, or list all options ");
	c->command_func = (ToolCommandFunc) extra_command_option;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [<NAME>]"), "info");
	c->description = _("Show a piece of information, or all information about the connection");
	c->command_func = (ToolCommandFunc) extra_command_info;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s [<FILE>]"), "e");
	c->description = _("Edit the query buffer (or file) with external editor");
	c->command_func = (ToolCommandFunc) extra_command_edit_buffer; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s [<FILE>]"), "qr");
	c->description = _("Reset the query buffer (or load file into query buffer)");
	c->command_func = (ToolCommandFunc) extra_command_reset_buffer; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = "qp";
	c->description = _("Show the contents of the query buffer");
	c->command_func = (ToolCommandFunc) extra_command_show_buffer; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s [<FAVORITE_NAME>]"), "g");
	c->description = _("Execute contents of query buffer, or execute specified query favorite");
	c->command_func = (ToolCommandFunc) extra_command_exec_buffer; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s <FILE>"), "qw");
	c->description = _("Write query buffer to file");
	c->command_func = (ToolCommandFunc) extra_command_write_buffer; /* only for term console */
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s <FAVORITE_NAME>"), "qs");
	c->description = _("Save query buffer as favorite");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_to_dict;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s <FAVORITE_NAME>"), "ql");
	c->description = _("Load a query favorite into query buffer");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_from_dict;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s <FAVORITE_NAME>"), "qd");
	c->description = _("Delete a query favorite");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_delete_dict;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s"), "qa");
	c->description = _("List all query favorites");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_list_dict;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->name = "H [HTML|XML|CSV|DEFAULT]";
	c->description = _("Set output format");
	c->command_func = (ToolCommandFunc) extra_command_set_output_format;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->name = g_strdup_printf (_("%s [<NAME>|<TABLE> <COLUMN> <ROW_CONDITION>] <FILE>"), "export");
	c->description = _("Export internal parameter or table's value to the FILE file");
	c->command_func = (ToolCommandFunc) extra_command_export;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->name = g_strdup_printf (_("%s [<NAME> [<VALUE>|_null_]]"), "set");
	c->description = _("Set or show internal parameter, or list all if no parameter specified ");
	c->command_func = (ToolCommandFunc) extra_command_set;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->name = g_strdup_printf (_("%s [<NAME>]"), "unset");
	c->description = _("Unset (delete) internal named parameter (or all parameters)");
	c->command_func = (ToolCommandFunc) extra_command_unset;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->name = g_strdup_printf (_("%s <NAME> [<FILE>|<TABLE> <COLUMN> <ROW_CONDITION>]"), "setex");
	c->description = _("Set internal parameter as the contents of the FILE file or from an existing table's value");
	c->command_func = (ToolCommandFunc) extra_command_set2;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->name = g_strdup_printf (_("%s <SELECT> <ROW_FIELDS> [<COLUMN_FIELDS> [<DATA_FIELDS> ...]]"), "pivot");
	c->description = _("Performs a statistical analysis on the data from SELECT, "
			   "using ROW_FIELDS and COLUMN_FIELDS criteria and optionally DATA_FIELDS for the data");
	c->command_func = (ToolCommandFunc) extra_command_pivot;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s"), "ds_list");
	c->description = _("Lists all the datasets kept in memory for reference");
	c->command_func = (ToolCommandFunc) extra_command_data_sets_list;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> <PATTERN>"), "ds_grep");
	c->description = _("Show a dataset's contents where lines match a regular expression");
	c->command_func = (ToolCommandFunc) extra_command_data_set_grep;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> [<COLUMN> [<COLUMN> ...]]"), "ds_show");
	c->description = _("Show a dataset's contents, showing only the specified columns if any specified");
	c->command_func = (ToolCommandFunc) extra_command_data_set_show;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> [<DATASET NAME> ...]"), "ds_rm");
	c->description = _("Remove one or more datasets");
	c->command_func = (ToolCommandFunc) extra_command_data_set_rm;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> <DATASET NAME>"), "ds_mv");
	c->description = _("Rename a dataset, useful to rename the '_' dataset to keep it");
	c->command_func = (ToolCommandFunc) extra_command_data_set_move;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s CSV <FILE NAME>"), "ds_import");
	c->description = _("Import a dataset from a file");
	c->command_func = (ToolCommandFunc) extra_command_data_set_import;
#ifdef HAVE_LIBSOUP
	base_tool_command_group_add (self->web_commands, c);
#endif
	base_tool_command_group_add (self->term_commands, c);

#ifdef HAVE_LDAP
	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <filter> [<base|onelevel|subtree> [<base DN>]]"), "ldap_search");
	c->description = _("Search LDAP entries");
	c->command_func = (ToolCommandFunc) extra_command_ldap_search;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <DN> [\"all\"|\"set\"|\"unset\"]"), "ldap_descr");
	c->description = _("Shows attributes for the entry identified by its DN. If the "
			   "\"set\" 2nd parameter is passed, then all set attributes are shown, if "
			   "the \"all\" 2nd parameter is passed, then the unset attributes are "
			   "also shown, and if the \"unset\" 2nd parameter "
			   "is passed, then only non set attributes are shown.");
	c->command_func = (ToolCommandFunc) extra_command_ldap_descr;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <DN> <new DN>"), "ldap_mv");
	c->description = _("Renames an LDAP entry");
	c->command_func = (ToolCommandFunc) extra_command_ldap_mv;
	base_tool_command_group_add (self->term_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <DN> <OPERATION> [<ATTR>[=<VALUE>]] [<ATTR>=<VALUE> ...]"), "ldap_mod");
	c->description = _("Modifies an LDAP entry's attributes; <OPERATION> may be DELETE, REPLACE or ADD");
	c->command_func = (ToolCommandFunc) extra_command_ldap_mod;
	base_tool_command_group_add (self->term_commands, c);
#endif
}

ToolCommandResult *
gda_internal_command_history (ToolCommand *command, guint argc, const gchar **argv,
			      TContext *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_TXT;

	GString *string;
#ifdef HAVE_HISTORY
	if (argv[0]) {
		if (!base_tool_input_save_history (argv[0], error)) {
			g_free (res);
			res = NULL;
		}
		else
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}
	else {
		gchar *tmp;
		tmp = base_tool_input_get_history ();
		string = g_string_new (tmp);
		g_free (tmp);
		res->u.txt = string;
	}
#else
	string = g_string_new (_("History is not supported"));
	res->u.txt = string;
#endif
	
	return res;
}

ToolCommandResult *
gda_internal_command_dict_sync (ToolCommand *command, guint argc, const gchar **argv,
				TContext *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;

	if (argv[0] && *argv[0]) {
		GdaMetaContext context;
		memset (&context, 0, sizeof (context));
		if (*argv[0] == '_')
			context.table_name = (gchar*) argv[0];
		else
			context.table_name = g_strdup_printf ("_%s", argv[0]);
		if (!gda_connection_update_meta_store (t_connection_get_cnc (t_context_get_connection (console)), &context, error)) {
			g_free (res);
			res = NULL;
		}
		if (*argv[0] != '_')
			g_free (context.table_name);
	}
	else if (!gda_connection_update_meta_store (t_connection_get_cnc (t_context_get_connection (console)), NULL, error)) {
		g_free (res);
		res = NULL;
	}

	return res;
}

ToolCommandResult *
gda_internal_command_list_tables (ToolCommand *command, guint argc, const gchar **argv,
				  TContext *console, GError **error)
{
	ToolCommandResult *res;
	GdaDataModel *model;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		GValue *v;
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_short_name=##tname::string AND "
			"table_type LIKE '%TABLE%' "
			"ORDER BY table_schema, table_name";

		gchar *tmp = gda_sql_identifier_prepare_for_compare (g_strdup (argv[0]));
		g_value_take_string (v = gda_value_new (G_TYPE_STRING), tmp);
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type LIKE '%TABLE%' "
			"ORDER BY table_schema, table_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error);
	}
	if (!model)
		return NULL;

	g_object_set_data (G_OBJECT (model), "name", _("List of tables"));
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

ToolCommandResult *
gda_internal_command_list_views (ToolCommand *command, guint argc, const gchar **argv,
				 TContext *console, GError **error)
{
	ToolCommandResult *res;
	GdaDataModel *model;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		GValue *v;
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_short_name=##tname::string AND "
			"table_type = 'VIEW' "
			"ORDER BY table_schema, table_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), argv[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type='VIEW' "
			"ORDER BY table_schema, table_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error);
	}
	if (!model)
		return NULL;

	g_object_set_data (G_OBJECT (model), "name", _("List of views"));
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

ToolCommandResult *
gda_internal_command_list_schemas (ToolCommand *command, guint argc, const gchar **argv,
				   TContext *console, GError **error)
{
	ToolCommandResult *res;
	GdaDataModel *model;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		GValue *v;
		const gchar *sql = "SELECT schema_name AS Schema, schema_owner AS Owner, "
			"CASE WHEN schema_internal THEN 'yes' ELSE 'no' END AS Internal "
			"FROM _schemata WHERE schema_name=##sname::string "
			"ORDER BY schema_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), argv[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error, "sname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT schema_name AS Schema, schema_owner AS Owner, "
			"CASE WHEN schema_internal THEN 'yes' ELSE 'no' END AS Internal "
			"FROM _schemata ORDER BY schema_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error);
	}
	if (!model)
		return NULL;

	g_object_set_data (G_OBJECT (model), "name", _("List of schemas"));
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

GdaMetaStruct *
gda_internal_command_build_meta_struct (GdaConnection *cnc, const gchar **argv, GError **error)
{
	GdaMetaStruct *mstruct;
	gint index;
	const gchar *arg;
	GdaMetaStore *store;
	GSList *objlist;

	store = gda_connection_get_meta_store (cnc);
	mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", store, "features", GDA_META_STRUCT_FEATURE_ALL, NULL);

	if (!argv[0]) {
		GSList *list;
		/* use all tables or views visible by default */
		if (!gda_meta_struct_complement_default (mstruct, error))
			goto onerror;
		list = gda_meta_struct_get_all_db_objects (mstruct);
		if (!list) {
			/* use all tables or views visible or not by default */
			if (!gda_meta_struct_complement_all (mstruct, error))
				goto onerror;
		}
		else
			g_slist_free (list);
	}

	for (index = 0, arg = argv[0]; arg; index++, arg = argv[index]) {
		GValue *v;
		g_value_set_string (v = gda_value_new (G_TYPE_STRING), arg);

		/* see if we have the form <schema_name>.*, to list all the objects in a given schema */
		if (g_str_has_suffix (arg, ".*") && (*arg != '.')) {
			gchar *str;

			str = g_strdup (arg);
			str[strlen (str) - 2] = 0;
			g_value_take_string (v, str);

			if (!gda_meta_struct_complement_schema (mstruct, NULL, v, error))
				goto onerror;
		}
		else {
			/* try to find it as a table or view */
			if (g_str_has_suffix (arg, "=") && (*arg != '=')) {
				GdaMetaDbObject *dbo;
				gchar *str;
				str = g_strdup (arg);
				str[strlen (str) - 1] = 0;
				g_value_take_string (v, str);
				dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, 
								  NULL, NULL, v, error);
				if (dbo)
					gda_meta_struct_complement_depend (mstruct, dbo, error);
				else
					goto onerror;
			}
			else if (!gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, NULL, NULL, v, error))
				goto onerror;
		}
	}

	objlist = gda_meta_struct_get_all_db_objects (mstruct);
	if (!objlist) {
		g_set_error (error, T_ERROR, T_OBJECT_NOT_FOUND_ERROR,
			     "%s", _("No object found"));
		goto onerror;
	}
	g_slist_free (objlist);
	gda_meta_struct_sort_db_objects (mstruct, GDA_META_SORT_ALHAPETICAL, NULL);
	return mstruct;

 onerror:
	g_object_unref (mstruct);
	return NULL;
}

ToolCommandResult *
gda_internal_command_detail (ToolCommand *command, guint argc, const gchar **argv,
			     TContext *console, GError **error)
{
	ToolCommandResult *res;
	GdaDataModel *model;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	if (!argv[0] || !*argv[0]) {
		/* FIXME: include indexes and sequences when they are present in the information schema */
		/* displays all tables, views, indexes and sequences which are "directly visible" */
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner FROM _tables WHERE table_short_name = table_name "
			"ORDER BY table_schema, table_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
						sql, error, NULL);

		if (model) {
			/* if no row, then return all the objects from all the schemas */
			if (gda_data_model_get_n_rows (model) == 0) {
				g_object_unref (model);
				sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
					"table_owner as Owner FROM _tables "
					"ORDER BY table_schema, table_name";
				model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
								sql, error, NULL);
			}
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
		}
		else {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
		return res;
	}

	GdaMetaStruct *mstruct;
	GSList *dbo_list, *tmplist;
	mstruct = gda_internal_command_build_meta_struct (t_connection_get_cnc (t_context_get_connection (console)), argv, error);
	if (!mstruct)
		return NULL;
	
	/* compute the number of known database objects */
	gint nb_objects = 0;
	tmplist = gda_meta_struct_get_all_db_objects (mstruct);
	for (dbo_list = tmplist; dbo_list; dbo_list = dbo_list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (dbo_list->data);
		if (dbo->obj_type != GDA_META_DB_UNKNOWN)
			nb_objects++;
	}

	/* if more than one object, then show a list */
	if (nb_objects > 1) {
		model = gda_data_model_array_new (4);
		gda_data_model_set_column_title (model, 0, _("Schema"));
		gda_data_model_set_column_title (model, 1, _("Name"));
		gda_data_model_set_column_title (model, 2, _("Type"));
		gda_data_model_set_column_title (model, 3, _("Owner"));
		for (dbo_list = tmplist; dbo_list; dbo_list = dbo_list->next) {
			GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (dbo_list->data);
			GList *values = NULL;
			GValue *val;
			
			if (dbo->obj_type == GDA_META_DB_UNKNOWN)
				continue;

			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
			values = g_list_append (values, val);

			val = gda_value_new (G_TYPE_STRING);
			switch (dbo->obj_type) {
			case GDA_META_DB_TABLE:
				g_value_set_string (val, "TABLE");
				break;
			case GDA_META_DB_VIEW:
				g_value_set_string (val, "VIEW");
				break;
			default:
				g_value_set_string (val, "???");
				TO_IMPLEMENT;
			}
			values = g_list_append (values, val);
			if (dbo->obj_owner)
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), dbo->obj_owner);
			else
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), "");
			values = g_list_append (values, val);
			gda_data_model_append_values (model, values, NULL);
			g_list_foreach (values, (GFunc) gda_value_free, NULL);
			g_list_free (values);
		}
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		g_slist_free (tmplist);
		return res;
	}
	else if (nb_objects == 0) {
		g_set_error (error, T_ERROR, T_OBJECT_NOT_FOUND_ERROR,
			     "%s", _("No object found"));
		g_slist_free (tmplist);
		return NULL;
	}

	/* 
	 * Information about a single object
	 */
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_MULTIPLE;
	res->u.multiple_results = NULL;
	GdaMetaDbObject *dbo;

	for (dbo_list = tmplist; dbo_list; dbo_list = dbo_list->next) {
		dbo = GDA_META_DB_OBJECT (dbo_list->data);
		if (dbo->obj_type == GDA_META_DB_UNKNOWN)
			dbo = NULL;
		else
			break;
	}
	g_assert (dbo);
	g_slist_free (tmplist);

	if ((dbo->obj_type == GDA_META_DB_VIEW) || (dbo->obj_type == GDA_META_DB_TABLE)) {
		ToolCommandResult *subres;
		GdaMetaTable *mt = GDA_META_TABLE (dbo);

		if (mt->columns) {
			GSList *list;
			model = gda_data_model_array_new (5);
			gda_data_model_set_column_title (model, 0, _("Column"));
			gda_data_model_set_column_title (model, 1, _("Type"));
			gda_data_model_set_column_title (model, 2, _("Nullable"));
			gda_data_model_set_column_title (model, 3, _("Default"));
			gda_data_model_set_column_title (model, 4, _("Extra"));
			if (dbo->obj_type == GDA_META_DB_VIEW)
				g_object_set_data_full (G_OBJECT (model), "name", 
							g_strdup_printf (_("List of columns for view '%s'"), 
									 dbo->obj_short_name), 	g_free);
			else
				g_object_set_data_full (G_OBJECT (model), "name", 
							g_strdup_printf (_("List of columns for table '%s'"), 
									 dbo->obj_short_name), g_free);
			for (list = mt->columns; list; list = list->next) {
				GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
				GList *values = NULL;
				GValue *val;

				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->column_name);
				values = g_list_append (values, val);
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->column_type);
				values = g_list_append (values, val);
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->nullok ? _("yes") : _("no"));
				values = g_list_append (values, val);
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->default_value);
				values = g_list_append (values, val);

				if (tcol->auto_incement) {
					g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), _("Auto increment"));
				} else  {
					val = gda_value_new_null ();
				}
				values = g_list_append (values, val);

				gda_data_model_append_values (model, values, NULL);
				g_list_foreach (values, (GFunc) gda_value_free, NULL);
				g_list_free (values);
			}
		
			subres = g_new0 (ToolCommandResult, 1);
			subres->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
			subres->u.model = model;
			res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
		}
		else {
			subres = g_new0 (ToolCommandResult, 1);
			subres->type = BASE_TOOL_COMMAND_RESULT_TXT;
			subres->u.txt = g_string_new ("");
			if (dbo->obj_type == GDA_META_DB_VIEW)
				g_string_append_printf (subres->u.txt,
							_("Could not determine columns of view '%s'"),
							dbo->obj_short_name);
			else
				g_string_append_printf (subres->u.txt,
							_("Could not determine columns of table '%s'"),
							dbo->obj_short_name);

			res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
		}
		
		if (dbo->obj_type == GDA_META_DB_VIEW) {
			/* VIEW specific */
			GdaMetaView *mv = GDA_META_VIEW (dbo);
			
			subres = g_new0 (ToolCommandResult, 1);
			subres->type = BASE_TOOL_COMMAND_RESULT_TXT;
			subres->u.txt = g_string_new ("");
			g_string_append_printf (subres->u.txt, _("View definition: %s"), mv->view_def);
			res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
		}
		else {
			/* TABLE specific */
			GValue *catalog, *schema, *name;
			gint i, nrows;
			const gchar *sql = "SELECT constraint_type, constraint_name  "
				"FROM _table_constraints WHERE table_catalog = ##tc::string "
				"AND table_schema = ##ts::string AND table_name = ##tname::string "
				"AND constraint_type != 'FOREIGN KEY' "
				"ORDER BY constraint_type DESC, constraint_name";

			g_value_set_string ((catalog = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
			g_value_set_string ((schema = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
			g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
			model = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
							sql, error, 
							"tc", catalog, "ts", schema, "tname", name, NULL);
			nrows = gda_data_model_get_n_rows (model);
			for (i = 0; i < nrows; i++) {
				GString *string = NULL;
				const GValue *cvalue;
				const gchar *str = NULL;

				cvalue = gda_data_model_get_value_at (model, 0, i, error);
				if (!cvalue) {
					base_tool_command_result_free (res);
					res = NULL;
					goto out;
				}
				str = g_value_get_string (cvalue);
				if (*str == 'P') {
					/* primary key */
					GdaDataModel *cols;
					cvalue = gda_data_model_get_value_at (model, 1, i, error);
					if (!cvalue) {
						base_tool_command_result_free (res);
						res = NULL;
						goto out;
					}
					string = g_string_new (_("Primary key"));
					g_string_append_printf (string, " '%s'", g_value_get_string (cvalue));
					str = "SELECT column_name, ordinal_position "
						"FROM _key_column_usage WHERE table_catalog = ##tc::string "
						"AND table_schema = ##ts::string AND table_name = ##tname::string AND "
						"constraint_name = ##cname::string "
						"ORDER BY ordinal_position";
					
					cols = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
								       str, error, 
								       "tc", catalog, "ts", schema, "tname", name, "cname", cvalue, 
								       NULL);
					if (cols) {
						gint j, cnrows;
						cnrows = gda_data_model_get_n_rows (cols);
						if (cnrows > 0) {
							g_string_append (string, " (");
							for (j = 0; j < cnrows; j++) {
								if (j > 0)
									g_string_append (string, ", ");
								cvalue = gda_data_model_get_value_at (cols, 0, j, error);
								if (!cvalue) {
									base_tool_command_result_free (res);
									res = NULL;
									g_object_unref (cols);
									g_string_free (string, TRUE);
									goto out;
								}
								g_string_append (string, g_value_get_string (cvalue));
							}
							g_string_append_c (string, ')');
						}
						g_object_unref (cols);
					}
				}
				else if (*str == 'F') {
					/* foreign key, not handled here */
				}
				else if (*str == 'U') {
					/* Unique constraint */
					GdaDataModel *cols;
					cvalue = gda_data_model_get_value_at (model, 1, i, error);
					if (!cvalue) {
						base_tool_command_result_free (res);
						res = NULL;
						goto out;
					}
					string = g_string_new (_("Unique"));
					g_string_append_printf (string, " '%s'", g_value_get_string (cvalue));
					str = "SELECT column_name, ordinal_position "
						"FROM _key_column_usage WHERE table_catalog = ##tc::string "
						"AND table_schema = ##ts::string AND table_name = ##tname::string AND "
						"constraint_name = ##cname::string "
						"ORDER BY ordinal_position";
					
					cols = gda_meta_store_extract (gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console))),
								       str, error, 
								       "tc", catalog, "ts", schema, "tname", name, "cname", cvalue, 
								       NULL);
					if (cols) {
						gint j, cnrows;
						cnrows = gda_data_model_get_n_rows (cols);
						if (cnrows > 0) {
							g_string_append (string, " (");
							for (j = 0; j < cnrows; j++) {
								if (j > 0)
									g_string_append (string, ", ");
								cvalue = gda_data_model_get_value_at (cols, 0, j, error);
								if (!cvalue) {
									base_tool_command_result_free (res);
									res = NULL;
									g_object_unref (cols);
									g_string_free (string, TRUE);
									goto out;
								}
								g_string_append (string, g_value_get_string (cvalue));
							}
							g_string_append_c (string, ')');
						}
						g_object_unref (cols);
					}
				}

				if (string) {
					subres = g_new0 (ToolCommandResult, 1);
					subres->type = BASE_TOOL_COMMAND_RESULT_TXT;
					subres->u.txt = string;
					res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
				}
			}

			gda_value_free (catalog);
			gda_value_free (schema);
			gda_value_free (name);

			g_object_unref (model);

			/* foreign key constraints */
			GSList *list;
			for (list = mt->fk_list; list; list = list->next) {
				GString *string;
				GdaMetaTableForeignKey *fk;
				gint i;
				fk = (GdaMetaTableForeignKey*) list->data;
				if (GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (fk))
					string = g_string_new (_("Declared foreign key"));
				else
					string = g_string_new (_("Foreign key"));
				g_string_append_printf (string, " '%s' (", fk->fk_name);
				for (i = 0; i < fk->cols_nb; i++) {
					if (i != 0)
						g_string_append (string, ", ");
					g_string_append (string, fk->fk_names_array [i]);
				}
				g_string_append (string, ") ");
				if (fk->depend_on->obj_short_name)
					/* To translators: the term "references" is the verb
					 * "to reference" in the context of foreign keys where
					 * "table A REFERENCES table B" */
					g_string_append_printf (string, _("references %s"),
								fk->depend_on->obj_short_name);
				else
					/* To translators: the term "references" is the verb
					 * "to reference" in the context of foreign keys where
					 * "table A REFERENCES table B" */
					g_string_append_printf (string, _("references %s.%s"),
								fk->depend_on->obj_schema,
								fk->depend_on->obj_name);
				g_string_append (string, " (");
				for (i = 0; i < fk->cols_nb; i++) {
					if (i != 0)
						g_string_append (string, ", ");
					g_string_append (string, fk->ref_pk_names_array [i]);
				}
				g_string_append (string, ")");

				g_string_append (string, "\n  ");
				g_string_append (string, _("Policy on UPDATE"));
				g_string_append (string, ": ");
				g_string_append (string, t_utils_fk_policy_to_string (GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY (fk)));
				g_string_append (string, "\n  ");
				g_string_append (string, _("Policy on DELETE"));
				g_string_append (string, ": ");
				g_string_append (string, t_utils_fk_policy_to_string (GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY (fk)));

				subres = g_new0 (ToolCommandResult, 1);
				subres->type = BASE_TOOL_COMMAND_RESULT_TXT;
				subres->u.txt = string;
				res->u.multiple_results = g_slist_append (res->u.multiple_results,
									  subres);
			}

			return res;
		}
	}
	else 
		TO_IMPLEMENT;

 out:
	g_object_unref (mstruct);
	return res;
}

static ToolCommandResult *
extra_command_set_output (ToolCommand *command, guint argc, const gchar **argv,
			  TContext *console, GError **error)
{
	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (t_context_set_output_file (console, argv[0], error)) {
		ToolCommandResult *res;
		
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static ToolCommandResult *
extra_command_set_output_format (ToolCommand *command, guint argc, const gchar **argv,
				 TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *fmt = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0])
		fmt = argv[0];
	
	t_context_set_output_format (console, BASE_TOOL_OUTPUT_FORMAT_DEFAULT);

	if (fmt) {
		if ((*fmt == 'X') || (*fmt == 'x'))
			t_context_set_output_format (console, BASE_TOOL_OUTPUT_FORMAT_XML);
		else if ((*fmt == 'H') || (*fmt == 'h'))
			t_context_set_output_format (console, BASE_TOOL_OUTPUT_FORMAT_HTML);
		else if ((*fmt == 'D') || (*fmt == 'd'))
			t_context_set_output_format (console, BASE_TOOL_OUTPUT_FORMAT_DEFAULT);
		else if ((*fmt == 'C') || (*fmt == 'c'))
			t_context_set_output_format (console, BASE_TOOL_OUTPUT_FORMAT_CSV);
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("Unknown output format: '%s', reset to default"), fmt);
			goto out;
		}
	}

	if (!t_context_get_output_stream (console, NULL)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new ("");
		switch (t_context_get_output_format (console)) {
		case BASE_TOOL_OUTPUT_FORMAT_DEFAULT:
			g_string_assign (res->u.txt, ("Output format is default\n"));
			break;
		case BASE_TOOL_OUTPUT_FORMAT_HTML:
			g_string_assign (res->u.txt, ("Output format is HTML\n"));
			break;
		case BASE_TOOL_OUTPUT_FORMAT_XML:
			g_string_assign (res->u.txt, ("Output format is XML\n"));
			break;
		case BASE_TOOL_OUTPUT_FORMAT_CSV:
			g_string_assign (res->u.txt, ("Output format is CSV\n"));
			break;
		default:
			TO_IMPLEMENT;
		}
	}
	else {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}

 out:
	t_utils_term_compute_color_attribute ();
	
	return res;
}

static ToolCommandResult *
extra_command_copyright (ToolCommand *command, guint argc, const gchar **argv,
			 TContext *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	g_assert (global_t_app);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new ("This program is free software; you can redistribute it and/or modify\n"
				   "it under the terms of the GNU General Public License as published by\n"
				   "the Free Software Foundation; either version 2 of the License, or\n"
				   "(at your option) any later version.\n\n"
				   "This program is distributed in the hope that it will be useful,\n"
				   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
				   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
				   "GNU General Public License for more details.\n");
	return res;
}

static ToolCommandResult *
extra_command_option (ToolCommand *command, guint argc, const gchar **argv,
		      TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *oname = NULL;
	const gchar *value = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0]) {
		oname = argv[0];
		if (argv[1] && *argv[1])
			value = argv[1];
	}

	if (oname) {
		GdaHolder *opt = gda_set_get_holder (global_t_app->priv->options, oname);
		if (opt) {
			if (value) {
				if (! gda_holder_set_value_str (opt, NULL, value, error))
					return NULL;
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
			}
			else {
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_SET;
				res->u.set = gda_set_new (NULL);
				gda_set_add_holder (res->u.set, gda_holder_copy (opt));
			}
		}
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("No option named '%s'"), oname);
			return NULL;
		}
	}
	else {
		/* list parameter's values */
		GdaDataModel *model;
		GSList *list;
		model = gda_data_model_array_new_with_g_types (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Value"));
		gda_data_model_set_column_title (model, 2, _("Description"));
		g_object_set_data (G_OBJECT (model), "name", _("List of options"));
		for (list = gda_set_get_holders (global_t_app->priv->options); list; list = list->next) {
			gint row;
			gchar *str;
			GValue *value;
			GdaHolder *opt;
			opt = GDA_HOLDER (list->data);
			if (!gda_holder_is_valid (opt))
				continue;
			row = gda_data_model_append_row (model, NULL);

			value = gda_value_new_from_string (gda_holder_get_id (opt), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);

			str = gda_holder_get_value_str (opt, NULL);
			value = gda_value_new_from_string (str ? str : "(NULL)", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			value = gda_value_new (G_TYPE_STRING);
			g_object_get_property ((GObject*) opt, "description", value);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);
		}

		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}

	return res;
}

static ToolCommandResult *
extra_command_info (ToolCommand *command, guint argc, const gchar **argv,
		    TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *oname = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0])
		oname = argv[0];

	if (oname) {
		GdaSet *infos;
		infos = t_connection_get_all_infos (t_context_get_connection (console));
		GdaHolder *opt = gda_set_get_holder (infos, oname);
		if (opt) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_SET;
			res->u.set = gda_set_new (NULL);
			gda_set_add_holder (res->u.set, gda_holder_copy (opt));
		}
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("No option named '%s'"), oname);
			return NULL;
		}
	}
	else {
		/* list parameter's values */
		GdaDataModel *model;
		GSList *list;
		model = gda_data_model_array_new_with_g_types (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Value"));
		gda_data_model_set_column_title (model, 2, _("Description"));
		g_object_set_data (G_OBJECT (model), "name", _("Current connection's information"));
		for (list = gda_set_get_holders (t_connection_get_all_infos (t_context_get_connection (console)));
		     list;
		     list = list->next) {
			gint row;
			gchar *str;
			GValue *value;
			GdaHolder *info;
			info = GDA_HOLDER (list->data);
			if (!gda_holder_is_valid (info))
				continue;
			row = gda_data_model_append_row (model, NULL);

			value = gda_value_new_from_string (gda_holder_get_id (info), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);

			str = gda_holder_get_value_str (info, NULL);
			value = gda_value_new_from_string (str ? str : "(NULL)", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			value = gda_value_new (G_TYPE_STRING);
			g_object_get_property ((GObject*) info, "description", value);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);
		}

		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}

	return res;
}


static ToolCommandResult *
extra_command_quit (ToolCommand *command, guint argc, const gchar **argv,
		    TContext *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	g_assert (global_t_app);

	g_assert (console == global_t_app->priv->term_console);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EXIT;
	return res;
}

static ToolCommandResult *
extra_command_cd (ToolCommand *command, guint argc, const gchar **argv,
		  TContext *console, GError **error)
{
	const gchar *dir = NULL;
#define DIR_LENGTH 256
	static char start_dir[DIR_LENGTH];
	static gboolean init_done = FALSE;

	g_assert (console);
	g_assert (global_t_app);

	if (!init_done) {
		init_done = TRUE;
		memset (start_dir, 0, DIR_LENGTH);
		if (getcwd (start_dir, DIR_LENGTH) != NULL) {
			/* default to $HOME */
#ifdef G_OS_WIN32
			TO_IMPLEMENT;
			strncpy (start_dir, "/", 2);
#else
			struct passwd *pw;
			
			pw = getpwuid (geteuid ());
			if (!pw) {
				g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
					     _("Could not get home directory: %s"), strerror (errno));
				return NULL;
			}
			else {
				gsize l = strlen (pw->pw_dir);
				if (l > DIR_LENGTH - 1)
					strncpy (start_dir, "/", 2);
				else
					strncpy (start_dir, pw->pw_dir, l + 1);
			}
#endif
		}
	}

	if (argv[0]) 
		dir = argv[0];
	else 
		dir = start_dir;

	if (dir) {
		if (chdir (dir) == 0) {
			ToolCommandResult *res;

			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_TXT_STDOUT;
			res->u.txt = g_string_new ("");
			g_string_append_printf (res->u.txt, _("Working directory is now: %s"), dir);
			return res;
		}
		else
			g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     _("Could not change working directory to '%s': %s"),
				     dir, strerror (errno));
	}

	return NULL;
}

static ToolCommandResult *
extra_command_set_input (ToolCommand *command, guint argc, const gchar **argv,
			 TContext *console, GError **error)
{
	g_assert (console);
	g_assert (global_t_app);

	g_assert (console == global_t_app->priv->term_console);

	if (t_term_context_set_input_file (T_TERM_CONTEXT (console), argv[0], error)) {
		ToolCommandResult *res;
		
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static ToolCommandResult *
extra_command_echo (ToolCommand *command, guint argc, const gchar **argv,
		    TContext *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	g_assert (global_t_app);
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_TXT_STDOUT;
	res->u.txt = g_string_new (argv[0]);
	if (argv[0][strlen (argv[0]) - 1] != '\n')
		g_string_append_c (res->u.txt, '\n');
	return res;
}


static ToolCommandResult *
extra_command_qecho (ToolCommand *command, guint argc, const gchar **argv,
		     TContext *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	g_assert (global_t_app);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (argv[0]);
	return res;
}

static ToolCommandResult *
extra_command_list_dsn (ToolCommand *command, guint argc, const gchar **argv,
			G_GNUC_UNUSED TContext *console, GError **error)
{
	ToolCommandResult *res;
	GList *list = NULL;
	GdaDataModel *dsn_list = NULL, *model = NULL;

	g_assert (global_t_app);

	if (argv[0]) {
		/* details about a DSN */
		GdaDataModel *model;
		model = t_config_info_detail_dsn (argv[0], error);
		if (model) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
			return res;
		}
		else
			return NULL;
	}
	else {
		gint i, nrows;
		
		dsn_list = gda_config_list_dsn ();
		nrows = gda_data_model_get_n_rows (dsn_list);

		model = gda_data_model_array_new_with_g_types (3,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("DSN"));
		gda_data_model_set_column_title (model, 1, _("Description"));
		gda_data_model_set_column_title (model, 2, _("Provider"));
		g_object_set_data (G_OBJECT (model), "name", _("DSN list"));
		
		for (i =0; i < nrows; i++) {
			const GValue *value;
			list = NULL;
			value = gda_data_model_get_value_at (dsn_list, 0, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (NULL, gda_value_copy (value));
			value = gda_data_model_get_value_at (dsn_list, 2, i, error);
			if (!value) 
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			value = gda_data_model_get_value_at (dsn_list, 1, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			
			if (gda_data_model_append_values (model, list, error) == -1)
				goto onerror;
			
			g_list_foreach (list, (GFunc) gda_value_free, NULL);
			g_list_free (list);
		}
		
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		g_object_unref (dsn_list);
		
		return res;
	}

 onerror:
	if (list) {
		g_list_foreach (list, (GFunc) gda_value_free, NULL);
		g_list_free (list);
	}
	g_object_unref (dsn_list);
	g_object_unref (model);
	return NULL;
}

static ToolCommandResult *
extra_command_create_dsn (ToolCommand *command, guint argc, const gchar **argv,
			  G_GNUC_UNUSED TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaDsnInfo newdsn;
	gchar *real_cnc, *real_provider, *user, *pass;

	g_assert (global_t_app);

	if (!argv[0] || !argv[1]) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing arguments"));
		return NULL;
	}

	newdsn.name = (gchar *) argv [0];
	gda_connection_string_split ((gchar *) argv[1], &real_cnc, &real_provider, &user, &pass);
	newdsn.provider = real_provider;
	newdsn.description = (gchar*) argv[2];
	
	newdsn.cnc_string = real_cnc;
	newdsn.auth_string = NULL;
	GString *auth = NULL;
	if (user) {
		gchar *tmp;
		tmp = gda_rfc1738_encode (user);
		auth = g_string_new ("USERNAME=");
		g_string_append (auth, tmp);
		g_free (tmp);
	}
	if (pass) {
		gchar *tmp;
		tmp = gda_rfc1738_encode (pass);
		if (auth)
			g_string_append (auth, ";PASSWORD=");
		else
			auth = g_string_new ("PASSWORD=");
		g_string_append (auth, tmp);
		g_free (tmp);
	}
	if (auth)
		newdsn.auth_string = g_string_free (auth, FALSE);

	newdsn.is_system = FALSE;

	if (!newdsn.provider) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing provider name"));
	}
	else if (gda_config_define_dsn (&newdsn, error)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}

	g_free (real_cnc);
	g_free (real_provider);
	g_free (user);
	g_free (pass);

	return res;
}

static ToolCommandResult *
extra_command_remove_dsn (ToolCommand *command, guint argc, const gchar **argv,
			  G_GNUC_UNUSED TContext *console, GError **error)
{
	ToolCommandResult *res;
	gint i;

	g_assert (global_t_app);

	if (!argv[0]) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing DSN name"));
		return NULL;
	}
	for (i = 0; argv [i]; i++) {
		if (! gda_config_remove_dsn (argv[i], error))
			return NULL;
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	return res;	
}

static ToolCommandResult *
extra_command_list_providers (ToolCommand *command, guint argc, const gchar **argv,
			      G_GNUC_UNUSED TContext *console, GError **error)
{
	ToolCommandResult *res;
	GdaDataModel *model;

	g_assert (global_t_app);

	if (argv[0])
		model = t_config_info_detail_provider (argv[0], error);
	else
		model = t_config_info_list_all_providers ();
		
	if (model) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
	return NULL;
}

static void
vconnection_hub_foreach_cb (G_GNUC_UNUSED GdaConnection *cnc, const gchar *ns, GString *string)
{
	if (string->len > 0)
		g_string_append_c (string, '\n');
	g_string_append_printf (string, "namespace %s", ns);
}

static gchar **
extra_command_manage_cnc_compl (const gchar *text, gpointer user_data)
{
	TApp *main_data = (TApp*) user_data;

	GArray *array = NULL;
	gsize len;
	gint i, ndsn;
	len = strlen (text);

	/* complete with DSN list to open connections */
	ndsn = gda_config_get_nb_dsn ();
	for (i = 0; i < ndsn; i++) {
		GdaDsnInfo *info;
		info = gda_config_get_dsn_info_at_index (i);
		if (!len || !strncmp (info->name, text, len)) {
			if (!array)
				array = g_array_new (TRUE, FALSE, sizeof (gchar*));
			gchar *tmp;
			tmp = g_strdup (info->name);
			g_array_append_val (array, tmp);
		}
	}

	/* complete with opened connections */
	GSList *list;
	for (list = main_data->priv->tcnc_list; list; list = list->next) {
		TConnection *tcnc = (TConnection *) list->data;
		if (!len || !strncmp (t_connection_get_name (tcnc), text, len)) {
			if (!array)
				array = g_array_new (TRUE, FALSE, sizeof (gchar*));
			gchar *tmp;
			tmp = g_strdup (t_connection_get_name (tcnc));
			g_array_append_val (array, tmp);
		}
	}

	if (array)
		return (gchar**) g_array_free (array, FALSE);
	else
		return NULL;
}

static 
ToolCommandResult *
extra_command_manage_cnc (ToolCommand *command, guint argc, const gchar **argv,
			  TContext *console, GError **error)
{
	g_assert (console);
	g_assert (global_t_app);

	if (argc > 2) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Too many arguments"));
		return NULL;
	}

	/* arguments:
	 * 0 = connection name
	 * 1 = DSN or direct string
	 */
	if (argv[0]) {
		const gchar *dsn = NULL;
		if (!argv[1]) {
			/* try to switch to an existing connection */
			TConnection *tcnc;

			tcnc = t_connection_get_by_name (argv[0]);
			if (tcnc) {
				ToolCommandResult *res;
				
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
				t_context_set_connection (console, tcnc);
				return res;
			}
			else {
				if (*argv [0] == '~') {
					/* find connection for which we want the meta store's connection */
					if (*(argv[0] + 1)) {
						tcnc = t_connection_get_by_name (argv[0] + 1);
						if (!tcnc) {
							g_set_error (error, T_ERROR,
								     T_NO_CONNECTION_ERROR,
								     _("No connection named '%s' found"), argv[0] + 1);
							return NULL;
						}
					}
					else
						tcnc = t_context_get_connection (console);

					if (!tcnc) {
						g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
							     "%s", _("No current connection"));
						return NULL;
					}

					/* find if requested connection already exists */
					TConnection *ntcnc = NULL;
					if (* (t_connection_get_name (tcnc)) == '~')
						ntcnc = t_connection_get_by_name (t_connection_get_name (t_context_get_connection (console)) + 1);
					else {
						gchar *tmp;
						tmp = g_strdup_printf ("~%s", t_connection_get_name (tcnc));
						ntcnc = t_connection_get_by_name (tmp);
						g_free (tmp);
					}
					if (ntcnc) {
						ToolCommandResult *res;
						
						res = g_new0 (ToolCommandResult, 1);
						res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
						t_context_set_connection (console, ntcnc);
						return res;
					}

					/* open a new connection */
					GdaMetaStore *store;
					ToolCommandResult *res;
					

					store = gda_connection_get_meta_store (t_connection_get_cnc (tcnc));
					ntcnc = t_connection_new (gda_meta_store_get_internal_connection (store));
					gchar *tmp;
					tmp = g_strdup_printf ("~%s", t_connection_get_name (tcnc));
					t_connection_set_name (ntcnc, tmp);
					g_free (tmp);

					t_context_set_connection (console, ntcnc);

					GError *lerror = NULL;
					FILE *output_stream;
					output_stream = t_context_get_output_stream (console, NULL);
					if (!output_stream) {
						g_print (_("Getting database schema information, "
							   "this may take some time... "));
						fflush (stdout);
					}
					if (!gda_connection_update_meta_store (t_connection_get_cnc (ntcnc),
									       NULL, &lerror)) {
						if (!output_stream) 
							g_print (_("error: %s\n"), 
								 lerror && lerror->message ? lerror->message : _("No detail"));
						if (lerror)
							g_error_free (lerror);
					}
					else
						if (!output_stream) 
							g_print (_("Done.\n"));
					
					res = g_new0 (ToolCommandResult, 1);
					res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
					return res;
				}

				dsn = argv[0];
			}
		}
		else
			dsn = argv[1];

		if (dsn) {
			/* open a new connection */
			TConnection *tcnc;

			tcnc = t_connection_get_by_name (argv[0]);
			if (tcnc) {
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     _("A connection named '%s' already exists"), argv[0]);
				return NULL;
			}

			tcnc = t_connection_open (argv[0], dsn, NULL, (console == t_app_get_term_console ()) ? TRUE : FALSE, error);
			if (tcnc) {
				t_context_set_connection (console, tcnc);

				ToolCommandResult *res;
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
				return res;
			}
			else
				return NULL;
		}
		else
			return NULL;
	}
	else {
		/* list opened connections */
		GdaDataModel *model;
		GSList *list;
		ToolCommandResult *res;

		if (! global_t_app->priv->tcnc_list) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_TXT;
			res->u.txt = g_string_new (_("No opened connection"));
			return res;
		}

		model = gda_data_model_array_new_with_g_types (4,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Provider"));
		gda_data_model_set_column_title (model, 2, _("DSN or connection string"));
		gda_data_model_set_column_title (model, 3, _("Username"));
		g_object_set_data (G_OBJECT (model), "name", _("List of opened connections"));

		for (list = global_t_app->priv->tcnc_list; list; list = list->next) {
			TConnection *tcnc = (TConnection *) list->data;
			GValue *value;
			gint row;
			const gchar *cstr;
			GdaServerProvider *prov;
			
			row = gda_data_model_append_row (model, NULL);
			
			value = gda_value_new_from_string (t_connection_get_name (tcnc), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);
			
			prov = gda_connection_get_provider (t_connection_get_cnc (tcnc));
			if (GDA_IS_VPROVIDER_HUB (prov))
				value = gda_value_new_from_string ("", G_TYPE_STRING);
			else
				value = gda_value_new_from_string (gda_connection_get_provider_name (t_connection_get_cnc (tcnc)), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			if (GDA_IS_VPROVIDER_HUB (prov)) {
				GString *string = g_string_new ("");
				gda_vconnection_hub_foreach (GDA_VCONNECTION_HUB (t_connection_get_cnc (tcnc)), 
							     (GdaVConnectionHubFunc) vconnection_hub_foreach_cb, string);
				value = gda_value_new_from_string (string->str, G_TYPE_STRING);
				g_string_free (string, TRUE);
			}
			else {
				cstr = gda_connection_get_dsn (t_connection_get_cnc (tcnc));
				if (cstr)
					value = gda_value_new_from_string (cstr, G_TYPE_STRING);
				else
					value = gda_value_new_from_string (gda_connection_get_cnc_string (t_connection_get_cnc (tcnc)), G_TYPE_STRING);
			}
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);

			/* only get USERNAME from the the authentication string */
			GdaQuarkList* ql;
			cstr = gda_connection_get_authentication (t_connection_get_cnc (tcnc));
			ql = gda_quark_list_new_from_string (cstr);
			cstr = gda_quark_list_find (ql, "USERNAME");
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_quark_list_free (ql);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
		}
 
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
}

static 
ToolCommandResult *
extra_command_close_cnc (ToolCommand *command, guint argc, const gchar **argv,
			 TContext *console, GError **error)
{
	g_assert (console);
	g_assert (global_t_app);
	
	if (argc == 0) {
		TConnection *tcnc = NULL;
		tcnc = t_context_get_connection (console);
		if (!tcnc) {
			g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
				     _("No connection specified"));
			return NULL;
		}
		else
			t_connection_close (tcnc);
	}
	else {
		guint i;
		for (i = 0; i < argc; i++) {
			TConnection *tcnc = NULL;
			if (argv[i] && *argv[i])
				tcnc = t_connection_get_by_name (argv[0]);
			if (!tcnc) {
				g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
					     _("No connection named '%s' found"), argv[0]);
				return NULL;
			}
			t_connection_close (tcnc);
		}
	}
	
	ToolCommandResult *res;
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	return res;
}

static ToolCommandResult *
extra_command_bind_cnc (ToolCommand *command, guint argc, const gchar **argv,
			TContext *console, GError **error)
{
	TConnection *tcnc = NULL;
	gint i, nargv = g_strv_length ((gchar **) argv);
	static GdaVirtualProvider *vprovider = NULL;
	GdaConnection *virtual;
	GString *string;

	g_assert (console);
	g_assert (global_t_app);

	if (nargv < 2) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing required connection names"));
		return NULL;
	}

	/* check for connections existance */
	tcnc = t_connection_get_by_name (argv[0]);
	if (tcnc) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("A connection named '%s' already exists"), argv[0]);
		return NULL;
	}
	if (! t_connection_name_is_valid (argv[0])) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Connection name '%s' is invalid"), argv[0]);
		return NULL;
	}
	for (i = 1; i < nargv; i++) {
		tcnc = t_connection_get_by_name (argv[i]);
		if (!tcnc) {
			GdaDataModel *src;
			src = g_hash_table_lookup (global_t_app->priv->mem_data_models, argv[i]);
			if (!src) {
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     _("No connection or dataset named '%s' found"), argv[i]);
				return NULL;
			}
		}
	}

	/* actual virtual connection creation */
	if (!vprovider) 
		vprovider = gda_vprovider_hub_new ();
	g_assert (vprovider);

	virtual = gda_virtual_connection_open (vprovider, GDA_CONNECTION_OPTIONS_AUTO_META_DATA, NULL);
	if (!virtual) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not create virtual connection"));
		return NULL;
	}
	g_object_set (G_OBJECT (virtual), "execution-timer", TRUE, NULL);
	gda_connection_get_meta_store (virtual); /* force create of meta store */

	/* add existing connections to virtual connection */
	string = g_string_new (_("Connections were bound as:"));
	for (i = 1; i < nargv; i++) {
		tcnc = t_connection_get_by_name (argv[i]);
		if (tcnc) {
			if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual),
						      t_connection_get_cnc (tcnc), argv[i], error)) {
				g_object_unref (virtual);
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, "\n   ");
			/* Translators: this string indicates that all the tables in connection named in the
			 * 1st "%s" will appear in the SQL namespace named as the 2nd "%s" */
			g_string_append_printf (string, _("%s in the '%s' namespace"), argv[i], argv[i]);
		}
		else {
			GdaDataModel *src;
			src = g_hash_table_lookup (global_t_app->priv->mem_data_models, argv[i]);
			if (! gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual),
								    src, argv[i], error)) {
				g_object_unref (virtual);
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, "\n   ");
			/* Translators: this string indicates that the dataset named in the 1st "%s"
			 * will appear as the table named as the 2nd "%s" */
			g_string_append_printf (string, _("%s mapped to the %s table"), argv[i],
						gda_vconnection_data_model_get_table_name (GDA_VCONNECTION_DATA_MODEL (virtual), src));
		}
        }

	tcnc = t_connection_new (virtual);
	t_connection_set_name (tcnc, argv[0]);
	t_context_set_connection (console, tcnc);

	ToolCommandResult *res;
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_TXT;
	res->u.txt = string;
	return res;
}

static ToolCommandResult *
extra_command_edit_buffer (ToolCommand *command, guint argc, const gchar **argv,
			   TContext *console, GError **error)
{
	gchar *filename = NULL;
	static gchar *editor_name = NULL;
	gchar *edit_command = NULL;
	gint systemres;
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		goto end_of_command;
	}

	if (argv[0] && *argv[0])
		filename = (gchar *) argv[0];
	else {
		/* use a temp file */
		gint fd;
		fd = g_file_open_tmp (NULL, &filename, error);
		if (fd < 0)
			goto end_of_command;

		const gchar *tmp;
		tmp = t_connection_get_query_buffer (t_context_get_connection (console));
		if (tmp) {
			ssize_t len;
			len = strlen (tmp);
			if (write (fd, tmp, len) != len) {
				g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
					     _("Could not write to temporary file '%s': %s"),
					     filename, strerror (errno));
				close (fd);
				goto end_of_command;
			}
		}
		close (fd);
	}

	if (!editor_name) {
		editor_name = getenv("GDA_SQL_EDITOR");
		if (!editor_name)
			editor_name = getenv("EDITOR");
		if (!editor_name)
			editor_name = getenv("VISUAL");
		if (!editor_name) {
#ifdef G_OS_WIN32
			editor_name = "notepad.exe";
#else
			editor_name = "vi";
#endif
		}
	}
#ifdef G_OS_WIN32
#ifndef __CYGWIN__
#define SYSTEMQUOTE "\""
#else
#define SYSTEMQUOTE ""
#endif
	edit_command = g_strdup_printf ("%s\"%s\" \"%s\"%s", SYSTEMQUOTE, editor_name, filename, SYSTEMQUOTE);
#else
	edit_command = g_strdup_printf ("exec %s '%s'", editor_name, filename);
#endif

	systemres = system (edit_command);
	if (systemres == -1) {
                g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("could not start editor '%s'"), editor_name);
		goto end_of_command;
	}
        else if (systemres == 127) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not start /bin/sh"));
		goto end_of_command;
	}
	else {
		if (! argv[0]) {
			gchar *str;
			
			if (!g_file_get_contents (filename, &str, NULL, error))
				goto end_of_command;
			t_connection_set_query_buffer (t_context_get_connection (console), str);
			g_free (str);
		}
	}
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;

 end_of_command:

	g_free (edit_command);
	if (! argv[0]) {
		g_unlink (filename);
		g_free (filename);
	}

	return res;
}


static ToolCommandResult *
extra_command_reset_buffer (ToolCommand *command, guint argc, const gchar **argv,
			    TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0]) {
		const gchar *filename = NULL;
		gchar *str;

		filename = argv[0];
		if (!g_file_get_contents (filename, &str, NULL, error))
			return NULL;

		t_connection_set_query_buffer (t_context_get_connection (console), str);
		g_free (str);
	}
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

static ToolCommandResult *
extra_command_show_buffer (ToolCommand *command, guint argc, const gchar **argv,
			   TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (t_connection_get_query_buffer (t_context_get_connection (console)));

	return res;
}

static ToolCommandResult *
extra_command_exec_buffer (ToolCommand *command, guint argc, const gchar **argv,
			   TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		/* load named query buffer first */
		res = extra_command_query_buffer_from_dict (command, argc, argv, console, error);
		if (!res)
			return NULL;
		base_tool_command_result_free (res);
		res = NULL;
	}

	const gchar *tmp;
	tmp = t_connection_get_query_buffer (t_context_get_connection (console));
	if (tmp && (*tmp != 0))
		res = t_context_command_execute (console, tmp,
						 GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
	else {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}

	return res;
}

static ToolCommandResult *
extra_command_write_buffer (ToolCommand *command, guint argc, const gchar **argv,
			    TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!argv[0]) 
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing FILE to write to"));
	else {
		const gchar *tmp;
		tmp = t_connection_get_query_buffer (t_context_get_connection (console));
		if (g_file_set_contents (argv[0], tmp ? tmp : "", -1, error)) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
	}

	return res;
}

#define QUERY_BUFFERS_TABLE_NAME "gda_sql_query_buffers"
#define QUERY_BUFFERS_TABLE_SELECT					\
	"SELECT name, sql FROM " QUERY_BUFFERS_TABLE_NAME " ORDER BY name"
#define QUERY_BUFFERS_TABLE_SELECT_ONE					\
	"SELECT sql FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"
#define QUERY_BUFFERS_TABLE_DELETE					\
	"DELETE FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"

static ToolCommandResult *
extra_command_query_buffer_list_dict (ToolCommand *command, guint argc, const gchar **argv,
				      TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaDataModel *retmodel;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	/* initialize returned data model */
	retmodel = gda_data_model_array_new_with_g_types (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gda_data_model_set_column_title (retmodel, 0, _("Favorite name"));
	gda_data_model_set_column_title (retmodel, 1, _("Comments"));
	gda_data_model_set_column_title (retmodel, 2, _("SQL"));

	GdaMetaStore *mstore;
	mstore = gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console)));


	GSList *favlist, *list;
	GError *lerror = NULL;
	favlist = t_favorites_list (t_connection_get_favorites (t_context_get_connection (console)), 0, T_FAVORITES_QUERIES,
				    ORDER_KEY_QUERIES, &lerror);
	if (lerror) {
		g_propagate_error (error, lerror);
		g_object_unref (retmodel);
		return NULL;
	}
	for (list = favlist; list; list = list->next) {
		TFavoritesAttributes *att = (TFavoritesAttributes*) list->data;
		gint i;
		GValue *value = NULL;
		i = gda_data_model_append_row (retmodel, error);
		if (i == -1)
			goto cleanloop;
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), att->name);
		if (! gda_data_model_set_value_at (retmodel, 0, i, value, error))
			goto cleanloop;
		g_value_set_string (value, att->descr);
		if (! gda_data_model_set_value_at (retmodel, 1, i, value, error))
			goto cleanloop;
		g_value_set_string (value, att->contents);
		if (! gda_data_model_set_value_at (retmodel, 2, i, value, error))
			goto cleanloop;
		gda_value_free (value);
		continue;
	cleanloop:
		gda_value_free (value);
		t_favorites_free_list (favlist);
		g_object_unref (retmodel);
		return NULL;
	}
	if (favlist)
		t_favorites_free_list (favlist);

	/* Use query buffer which used to be stored differently in previous versions of GdaSql:
	 * in the "gda_sql_query_buffers" table */
	GdaStatement *sel_stmt = NULL;
	GdaDataModel *model;
	sel_stmt = gda_sql_parser_parse_string (t_connection_get_parser (t_context_get_connection (console)),
						QUERY_BUFFERS_TABLE_SELECT, NULL, NULL);
	g_assert (sel_stmt);
	GdaConnection *store_cnc;
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	model = gda_connection_statement_execute_select (store_cnc, sel_stmt, NULL, NULL);
	g_object_unref (sel_stmt);
	if (model) {
		gint r, nrows;
		nrows = gda_data_model_get_n_rows (model);
		for (r = 0; r < nrows; r++) {
			const GValue *cvalue = NULL;
			gint i;
			i = gda_data_model_append_row (retmodel, NULL);
			if (i == -1)
				break;
			cvalue = gda_data_model_get_value_at (model, 0, r, NULL);
			if (!cvalue)
				break;
			gda_data_model_set_value_at (retmodel, 0, i, cvalue, NULL);

			cvalue = gda_data_model_get_value_at (model, 1, r, NULL);
			if (!cvalue)
				break;
			gda_data_model_set_value_at (retmodel, 2, i, cvalue, NULL);
		}
		g_object_unref (model);
	}
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = retmodel;

	return res;
}

static ToolCommandResult *
extra_command_query_buffer_to_dict (ToolCommand *command, guint argc, const gchar **argv,
				    TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	const gchar *tmp;
	tmp = t_connection_get_query_buffer (t_context_get_connection (console));
	if (tmp && (*tmp != 0)) {
		/* find a suitable name */
		gchar *qname;
		if (argv[0] && *argv[0]) 
			qname = g_strdup ((gchar *) argv[0]);
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("Missing query buffer name"));
			return NULL;
		}

		/* Use tools favorites */
		TFavoritesAttributes att;
		att.id = -1;
		att.type = T_FAVORITES_QUERIES;
		att.name = qname;
		att.descr = NULL;
		att.contents = (gchar*) tmp;

		if (! t_favorites_add (t_connection_get_favorites (t_context_get_connection (console)), 0,
				       &att, ORDER_KEY_QUERIES, G_MAXINT, error)) {
			g_free (qname);
			return NULL;
		}
		g_free (qname);

		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Query buffer is empty"));

	return res;
}

static ToolCommandResult *
extra_command_query_buffer_from_dict (ToolCommand *command, guint argc, const gchar **argv,
				      TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		/* Use tools favorites */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console)));

		TFavoritesAttributes att;
		gint favid;
		favid = t_favorites_find_by_name (t_connection_get_favorites (t_context_get_connection (console)), 0,
						  T_FAVORITES_QUERIES,
						  argv[0], &att, NULL);      
		if (favid >= 0) {
			t_connection_set_query_buffer (t_context_get_connection (console), att.contents);
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
		else {
			/* query retrieval */
			static GdaStatement *sel_stmt = NULL;
			static GdaSet *sel_params = NULL;
			GdaDataModel *model;
			const GValue *cvalue;
			GError *lerror = NULL;

			g_set_error (&lerror, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     "%s", _("Could not find favorite"));
			if (!sel_stmt) {
				sel_stmt = gda_sql_parser_parse_string (t_connection_get_parser (t_context_get_connection (console)), 
									QUERY_BUFFERS_TABLE_SELECT_ONE, NULL, NULL);
				g_assert (sel_stmt);
				g_assert (gda_statement_get_parameters (sel_stmt, &sel_params, NULL));
			}

			if (! gda_set_set_holder_value (sel_params, error, "name", argv[0])) {
				g_propagate_error (error, lerror);
				return NULL;
			}

			GdaConnection *store_cnc;
			store_cnc = gda_meta_store_get_internal_connection (mstore);
			model = gda_connection_statement_execute_select (store_cnc, sel_stmt, sel_params, NULL);
			if (!model) {
				g_propagate_error (error, lerror);
				return NULL;
			}

			if ((gda_data_model_get_n_rows (model) == 1) &&
			    (cvalue = gda_data_model_get_value_at (model, 0, 0, NULL))) {
				t_connection_set_query_buffer (t_context_get_connection (console), g_value_get_string (cvalue));
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
			}
			g_object_unref (model);
		}
	}
	else
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing query buffer name"));
		
	return res;
}

static ToolCommandResult *
extra_command_query_buffer_delete_dict (ToolCommand *command, guint argc, const gchar **argv,
					TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		/* Use tools favorites */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console)));

		TFavoritesAttributes att;
		GError *lerror = NULL;
		att.id = -1;
		att.type = T_FAVORITES_QUERIES;
		att.name = (gchar*) argv[0];
		att.descr = NULL;
		att.contents = NULL;

		if (! t_favorites_delete (t_connection_get_favorites (t_context_get_connection (console)), 0,
					  &att, &lerror)) {
			/* query retrieval */
			static GdaStatement *del_stmt = NULL;
			static GdaSet *del_params = NULL;
			if (!del_stmt) {
				del_stmt = gda_sql_parser_parse_string (t_connection_get_parser (t_context_get_connection (console)), 
									QUERY_BUFFERS_TABLE_DELETE, NULL, NULL);
				g_assert (del_stmt);
				g_assert (gda_statement_get_parameters (del_stmt, &del_params, NULL));
			}

			if (! gda_set_set_holder_value (del_params, NULL, "name", argv[0])) {
				g_propagate_error (error, lerror);
				return NULL;
			}

			GdaConnection *store_cnc;
			store_cnc = gda_meta_store_get_internal_connection (mstore);
			if (gda_connection_statement_execute_non_select (store_cnc, del_stmt, del_params,
									 NULL, NULL) > 0)
				g_clear_error (&lerror);
			else {
				g_propagate_error (error, lerror);
				return NULL;
			}
		}
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing query buffer name"));
		
	return res;
}

static void foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model);
static ToolCommandResult *
extra_command_set (ToolCommand *command, guint argc, const gchar **argv,
		   TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *value = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
                g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

	if (argv[0] && *argv[0]) {
		pname = argv[0];
		if (argv[1] && *argv[1])
			value = argv[1];
	}

	if (pname) {
		T_APP_LOCK (global_t_app);
		GdaHolder *param = g_hash_table_lookup (global_t_app->priv->parameters, pname);
		if (param) {
			if (value) {
				/* set param's value */
				if (!strcmp (value, "_null_")) {
					if (! gda_holder_set_value (param, NULL, error)) {
						T_APP_UNLOCK (global_t_app);
						return NULL;
					}
				}
				else {
					GdaDataHandler *dh;
					GValue *gvalue;

					if (t_context_get_connection (console)) {
						GdaConnection *icnc;
						GdaServerProvider *prov;
						icnc = t_connection_get_cnc (t_context_get_connection (console));
						prov = gda_connection_get_provider (icnc);
						dh = gda_server_provider_get_data_handler_g_type (prov, icnc,
												  gda_holder_get_g_type (param));
					}
					else
						dh = gda_data_handler_get_default (gda_holder_get_g_type (param));
					gvalue = gda_data_handler_get_value_from_str (dh, value,
										      gda_holder_get_g_type (param));
					if (! gda_holder_take_value (param, gvalue, error)) {
						T_APP_UNLOCK (global_t_app);
						return NULL;
					}
				}

				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
			}
			else {
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_SET;
				res->u.set = gda_set_new (NULL);
				gda_set_add_holder (res->u.set, gda_holder_copy (param));
			}
		}
		else {
			if (value) {
				/* create parameter */
				if (!strcmp (value, "_null_"))
					value = NULL;
				param = gda_holder_new_string (pname, value);
				g_hash_table_insert (global_t_app->priv->parameters, g_strdup (pname), param);
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
			}
			else 
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     _("No parameter named '%s' defined"), pname);
		}
		T_APP_UNLOCK (global_t_app);
	}
	else {
		/* list parameter's values */
		GdaDataModel *model;
		model = gda_data_model_array_new_with_g_types (2,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Value"));
		g_object_set_data (G_OBJECT (model), "name", _("List of defined parameters"));
		T_APP_LOCK (global_t_app);
		g_hash_table_foreach (global_t_app->priv->parameters, (GHFunc) foreach_param_set, model);
		T_APP_UNLOCK (global_t_app);
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}

	return res;
}

static void
foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model)
{
	gint row;
	gchar *str;
	GValue *value;
	row = gda_data_model_append_row (model, NULL);

	value = gda_value_new_from_string (pname, G_TYPE_STRING);
	gda_data_model_set_value_at (model, 0, row, value, NULL);
	gda_value_free (value);
			
	str = gda_holder_get_value_str (param, NULL);
	value = gda_value_new_from_string (str ? str : "(NULL)", G_TYPE_STRING);
	gda_data_model_set_value_at (model, 1, row, value, NULL);
	gda_value_free (value);
}

static ToolCommandResult *
extra_command_unset (ToolCommand *command, guint argc, const gchar **argv,
		     TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *pname = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0]) 
		pname = argv[0];

	T_APP_LOCK (global_t_app);
	if (pname) {
		GdaHolder *param = g_hash_table_lookup (global_t_app->priv->parameters, pname);
		if (param) {
			g_hash_table_remove (global_t_app->priv->parameters, pname);
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
		else 
			g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     _("No parameter named '%s' defined"), pname);
	}
	else {
		g_hash_table_destroy (global_t_app->priv->parameters);
		global_t_app->priv->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}
	T_APP_UNLOCK (global_t_app);
		
	return res;
}

static void foreach_data_model (const gchar *name, GdaDataModel *keptmodel, GdaDataModel *model);
static ToolCommandResult *
extra_command_data_sets_list (ToolCommand *command, guint argc, const gchar **argv,
			      TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaDataModel *model;

	g_assert (console);
	g_assert (global_t_app);

	model = gda_data_model_array_new_with_g_types (2,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("dimensions (columns x rows)"));
	g_object_set_data (G_OBJECT (model), "name", _("List of kept data"));
	g_hash_table_foreach (global_t_app->priv->mem_data_models, (GHFunc) foreach_data_model, model);
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

static void
foreach_data_model (const gchar *name, GdaDataModel *keptmodel, GdaDataModel *model)
{
	gint row;
	gchar *str;
	GValue *value;
	row = gda_data_model_append_row (model, NULL);

	value = gda_value_new_from_string (name, G_TYPE_STRING);
	gda_data_model_set_value_at (model, 0, row, value, NULL);
	gda_value_free (value);

	str = g_strdup_printf ("%d x %d",
			       gda_data_model_get_n_columns (keptmodel),
			       gda_data_model_get_n_rows (keptmodel));
	value = gda_value_new_from_string (str, G_TYPE_STRING);
	g_free (str);
	gda_data_model_set_value_at (model, 1, row, value, NULL);
	gda_value_free (value);
}


static ToolCommandResult *
extra_command_data_set_move (ToolCommand *command, guint argc, const gchar **argv,
			     TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *old_name = NULL;
	const gchar *new_name = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0]) {
		old_name = argv[0];
		if (argv[1] && *argv[1])
			new_name = argv[1];
	}
	if (!old_name || !*old_name || !new_name || !*new_name) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *src;

	src = g_hash_table_lookup (global_t_app->priv->mem_data_models, old_name);
	if (!src) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Could not find dataset named '%s'"), old_name);
		return NULL;
	}

	g_hash_table_insert (global_t_app->priv->mem_data_models, g_strdup (new_name), g_object_ref (src));
	if (strcmp (old_name, T_LAST_DATA_MODEL_NAME))
		g_hash_table_remove (global_t_app->priv->mem_data_models, old_name);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

static ToolCommandResult *
extra_command_data_set_grep (ToolCommand *command, guint argc, const gchar **argv,
			     TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *model_name = NULL;
	const gchar *pattern = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0]) {
		model_name = argv[0];
		if (argv[1] && *argv[1])
			pattern = argv[1];
	}
	if (!model_name) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *src, *model;
	gint nbfields, nbrows, i;

	src = g_hash_table_lookup (global_t_app->priv->mem_data_models, model_name);
	if (!src) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Could not find dataset named '%s'"), model_name);
		return NULL;
	}
	nbfields = gda_data_model_get_n_columns (src);
	nbrows = gda_data_model_get_n_rows (src);
        model = gda_data_model_array_new (nbfields);
	if (g_object_get_data (G_OBJECT (src), "name"))
                g_object_set_data_full (G_OBJECT (model), "name", g_strdup (g_object_get_data (G_OBJECT (src), "name")), g_free);
        if (g_object_get_data (G_OBJECT (src), "descr"))
                g_object_set_data_full (G_OBJECT (model), "descr", g_strdup (g_object_get_data (G_OBJECT (src), "descr")), g_free);
        for (i = 0; i < nbfields; i++) {
		GdaColumn *copycol, *srccol;
                gchar *colid;

                srccol = gda_data_model_describe_column (src, i);
                copycol = gda_data_model_describe_column (model, i);

                g_object_get (G_OBJECT (srccol), "id", &colid, NULL);
                g_object_set (G_OBJECT (copycol), "id", colid, NULL);
                g_free (colid);
                gda_column_set_description (copycol, gda_column_get_description (srccol));
                gda_column_set_name (copycol, gda_column_get_name (srccol));
                gda_column_set_dbms_type (copycol, gda_column_get_dbms_type (srccol));
                gda_column_set_g_type (copycol, gda_column_get_g_type (srccol));
                gda_column_set_position (copycol, gda_column_get_position (srccol));
                gda_column_set_allow_null (copycol, gda_column_get_allow_null (srccol));
	}

	for (i = 0; i < nbrows; i++) {
		gint nrid, j;
		gboolean add_row = TRUE;
		const GValue *cvalue;
		GRegex *regex = NULL;

		if (pattern && *pattern) {
			if (!regex) {
				GRegexCompileFlags flags = G_REGEX_OPTIMIZE | G_REGEX_CASELESS;
				regex = g_regex_new ((const gchar*) pattern, flags, 0, error);
				if (!regex) {
					g_object_unref (model);
					return NULL;
				}
			}

			add_row = FALSE;
			for (j = 0; j < nbfields; j++) {
				cvalue = gda_data_model_get_value_at (src, j, i, error);
				if (!cvalue) {
					if (regex)
						g_regex_unref (regex);
					g_object_unref (model);
					return NULL;
				}
				gchar *str;
				str = gda_value_stringify (cvalue);
				if (g_regex_match (regex, str, 0, NULL)) {
					add_row = TRUE;
					g_free (str);
					break;
				}
				g_free (str);
			}
		}
		if (!add_row)
			continue;

		nrid = gda_data_model_append_row (model, error);
		if (nrid < 0) {
			if (regex)
				g_regex_unref (regex);
			g_object_unref (model);
			return NULL;
		}

		for (j = 0; j < nbfields; j++) {
			cvalue = gda_data_model_get_value_at (src, j, i, error);
			if (!cvalue) {
				if (regex)
					g_regex_unref (regex);
				g_object_unref (model);
				return NULL;
			}
			if (! gda_data_model_set_value_at (model, j, nrid, cvalue, error)) {
				if (regex)
					g_regex_unref (regex);
				g_object_unref (model);
				return NULL;
			}
		}
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

static ToolCommandResult *
extra_command_data_set_show (ToolCommand *command, guint argc, const gchar **argv,
			      TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *model_name = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0])
		model_name = argv[0];

	if (!model_name) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *src;
	src = g_hash_table_lookup (global_t_app->priv->mem_data_models, model_name);
	if (!src) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Could not find dataset named '%s'"), model_name);
		return NULL;
	}

	if (argv[1] && *argv[1]) {
		GArray *cols;
		gint i;
		cols = g_array_new (FALSE, FALSE, sizeof (gint));
		for (i = 1; argv[i] && *argv[i]; i++) {
			const gchar *cname = argv[i];
			gint pos;
			pos = gda_data_model_get_column_index (src, cname);
			if (pos < 0) {
				long int li;
				char *end;
				li = strtol (cname, &end, 10);
				if (!*end && (li >= 0) && (li < G_MAXINT))
					pos = (gint) li;
			}
			if (pos < 0) {
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     _("Could not identify column '%s'"), cname);
				g_array_free (cols, TRUE);
				return NULL;
			}
			g_array_append_val (cols, pos);
		}

		GdaDataModel *model;
		model = (GdaDataModel*) gda_data_model_array_copy_model_ext (src, cols->len, (gint*) cols->data,
									     error);
		g_array_free (cols, TRUE);
		if (model) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
		}
		else
			return NULL;
	}
	else {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = g_object_ref (src);
	}

	return res;
}

static ToolCommandResult *
extra_command_data_set_rm (ToolCommand *command, guint argc, const gchar **argv,
			   TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	guint i;

	g_assert (console);
	g_assert (global_t_app);

	if (!argv[0] ||  !(*argv[0])) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}
	for (i = 0; argv[i]; i++) {
		GdaDataModel *src;
		src = g_hash_table_lookup (global_t_app->priv->mem_data_models, argv[i]);
		if (!src) {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("Could not find dataset named '%s'"), argv[i]);
			return NULL;
		}
	}

	for (i = 0; argv[i]; i++)
		g_hash_table_remove (global_t_app->priv->mem_data_models, argv[i]);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

static GdaSet *
make_options_set_from_gdasql_options (const gchar *context)
{
	GdaSet *expopt = NULL;
	GSList *list, *nlist = NULL;
	for (list = gda_set_get_holders (global_t_app->priv->options); list; list = list->next) {
		GdaHolder *param = GDA_HOLDER (list->data);
		gchar *val;
		val = g_object_get_data ((GObject*) param, context);
		if (val == NULL)
			continue;

		GdaHolder *nparam;
		const GValue *cvalue2;
		cvalue2 = gda_holder_get_value (param);
		nparam = gda_holder_new (G_VALUE_TYPE (cvalue2), val);
		g_assert (gda_holder_set_value (nparam, cvalue2, NULL));
		nlist = g_slist_append (nlist, nparam);
	}
	if (nlist) {
		expopt = gda_set_new (nlist);
		g_slist_free (nlist);
	}
	return expopt;
}

static ToolCommandResult *
extra_command_data_set_import (ToolCommand *command, guint argc, const gchar **argv,
			       TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *type = NULL, *file_name = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (argv[0] && *argv[0]) {
		type = argv[0];
		if (g_ascii_strcasecmp (type, "csv")) {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("Unknown import format '%s'"), argv[0]);
			return NULL;
		}
		if (argv[1] && *argv[1])
			file_name = argv[1];
	}

	if (!type || !file_name) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *model;
	GdaSet *impopt;
	impopt = make_options_set_from_gdasql_options ("csv");
	model = gda_data_model_import_new_file (file_name, TRUE, impopt);
	if (impopt)
		g_object_unref (impopt);
	if (!model) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("Could not import file '%s'"), file_name);
		return NULL;
	}
	GSList *list;
	list = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model));
	if (list) {
		g_propagate_error (error, g_error_copy ((GError*) list->data));
		return NULL;
	}

	g_hash_table_insert (global_t_app->priv->mem_data_models, g_strdup (T_LAST_DATA_MODEL_NAME), model);
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

#define DO_UNLINK(x) g_unlink(x)
static void
graph_func_child_died_cb (GPid pid, G_GNUC_UNUSED gint status, gchar *fname)
{
	DO_UNLINK (fname);
	g_free (fname);
	g_spawn_close_pid (pid);
}

static gchar *
create_graph_from_meta_struct (G_GNUC_UNUSED GdaConnection *cnc, GdaMetaStruct *mstruct, GError **error)
{
#define FNAME "graph.dot"
	gchar *graph;

	graph = gda_meta_struct_dump_as_graph (mstruct, GDA_META_GRAPH_COLUMNS, error);
	if (!graph)
		return NULL;

	/* do something with the graph */
	gchar *result = NULL;
	if (g_file_set_contents (FNAME, graph, -1, error)) {
		const gchar *viewer;
		const gchar *format;
		
		viewer = g_getenv ("GDA_SQL_VIEWER_PNG");
		if (viewer)
			format = "png";
		else {
			viewer = g_getenv ("GDA_SQL_VIEWER_PDF");
			if (viewer)
				format = "pdf";
		}
		if (viewer) {
			static gint counter = 0;
			gchar *tmpname, *suffix;
			gchar *argv[] = {"dot", NULL, "-o",  NULL, FNAME, NULL};
			guint eventid = 0;

			suffix = g_strdup_printf (".gda_graph_tmp-%d", counter++);
			tmpname = g_build_filename (g_get_tmp_dir (), suffix, NULL);
			g_free (suffix);
			argv[1] = g_strdup_printf ("-T%s", format);
			argv[3] = tmpname;
			if (g_spawn_sync (NULL, argv, NULL,
					  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 
					  NULL, NULL,
					  NULL, NULL, NULL, NULL)) {
				gchar *vargv[3];
				GPid pid;
				vargv[0] = (gchar *) viewer;
				vargv[1] = tmpname;
				vargv[2] = NULL;
				if (g_spawn_async (NULL, vargv, NULL,
						   G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | 
						   G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD, 
						   NULL, NULL,
						   &pid, NULL)) {
					eventid = g_child_watch_add (pid, (GChildWatchFunc) graph_func_child_died_cb, 
								     tmpname);
				}
			}
			if (eventid == 0) {
				DO_UNLINK (tmpname);
				g_free (tmpname);
			}
			g_free (argv[1]);
			result = g_strdup_printf (_("Graph written to '%s'\n"), FNAME);
		}
		else 
			result = g_strdup_printf (_("Graph written to '%s'\n"
						    "Use 'dot' (from the GraphViz package) to create a picture, for example:\n"
						    "\tdot -Tpng -o graph.png %s\n"
						    "Note: set the GDA_SQL_VIEWER_PNG or GDA_SQL_VIEWER_PDF environment "
						    "variables to view the graph\n"), FNAME, FNAME);
	}
	g_free (graph);
	return result;
}

static ToolCommandResult *
extra_command_graph (ToolCommand *command, guint argc, const gchar **argv,
		     TContext *console, GError **error)
{
	gchar *result;
	GdaMetaStruct *mstruct;

	g_assert (console);
	g_assert (global_t_app);
	g_assert (console == global_t_app->priv->term_console);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	GdaConnection *icnc;
	icnc = t_connection_get_cnc (t_context_get_connection (console));
	mstruct = gda_internal_command_build_meta_struct (icnc, argv, error);
	if (!mstruct)
		return NULL;
	
	result = create_graph_from_meta_struct (icnc, mstruct, error);
	g_object_unref (mstruct);
	if (result) {
		ToolCommandResult *res;
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (result);
		g_free (result);
		return res;
	}
	else 
		return NULL;
}

#ifdef HAVE_LIBSOUP
gboolean
t_app_start_http_server (gint port, const gchar *auth_token, GError **error)
{
	gboolean retval = TRUE;
	if (port > 0) {
		global_t_app->priv->server = web_server_new (port, auth_token);
		if (!global_t_app->priv->server) {
			g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     "%s", _("Could not start HTTPD server"));
			retval = FALSE;
		}
	}
	return retval;
}

static ToolCommandResult *
extra_command_httpd (ToolCommand *command, guint argc, const gchar **argv,
		     TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (global_t_app->priv->server) {
		/* stop server */
		g_object_unref (global_t_app->priv->server);
		global_t_app->priv->server = NULL;
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (_("HTTPD server stopped"));
	}
	else {
		/* start new server */
		gint port = 12345;
		const gchar *auth_token = NULL;
		if (argv[0] && *argv[0]) {
			gchar *ptr;
			port = (gint) strtol (argv[0], &ptr, 10);
			if (ptr && *ptr)
				port = -1;
			if (argv[1] && *argv[1]) {
				auth_token = argv[1];
			}
		}
		if (port > 0) {
			if (t_app_start_http_server (port, auth_token, error)) {
				res = g_new0 (ToolCommandResult, 1);
				res->type = BASE_TOOL_COMMAND_RESULT_TXT_STDOUT;
				res->u.txt = g_string_new (_("HTTPD server started"));
			}
		}
		else
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("Invalid port specification"));
	}

	return res;
}
#endif

static const GValue *
get_table_value_at_cell (TContext *console, GError **error,
			 const gchar *table, const gchar *column, const gchar *row_cond,
			 GdaDataModel **out_model_of_value)
{
	const GValue *retval = NULL;

	*out_model_of_value = NULL;

	/* prepare executed statement */
	gchar *sql;
	gchar *rtable, *rcolumn;
	
	GdaConnection *icnc;
	icnc = t_connection_get_cnc (t_context_get_connection (console));
	rtable = gda_sql_identifier_quote (table, icnc, NULL, FALSE, FALSE);
	rcolumn = gda_sql_identifier_quote (column, icnc, NULL, FALSE, FALSE);
	sql = g_strdup_printf ("SELECT %s FROM %s WHERE %s", rcolumn, rtable, row_cond);
	g_free (rtable);
	g_free (rcolumn);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (t_connection_get_parser (t_context_get_connection (console)), sql, &remain, error);
	if (!stmt) {
		g_free (sql);
		return NULL;
	}
	if (remain) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong row condition"));
		g_free (sql);
		return NULL;
	}
	g_object_unref (stmt);

	/* execute statement */
	ToolCommandResult *tmpres;
	tmpres = t_context_command_execute (console, sql,
					    GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
	g_free (sql);
	if (!tmpres)
		return NULL;
	gboolean errorset = FALSE;
	if (tmpres->type == BASE_TOOL_COMMAND_RESULT_DATA_MODEL) {
		GdaDataModel *model;
		model = tmpres->u.model;
		if (gda_data_model_get_n_rows (model) == 1) {
			retval = gda_data_model_get_value_at (model, 0, 0, error);
			if (!retval)
				errorset = TRUE;
			else
				*out_model_of_value = g_object_ref (model);
		}
	}
	base_tool_command_result_free (tmpres);

	if (!retval && !errorset)
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("No unique row identified"));

	return retval;
}

static ToolCommandResult *
extra_command_export (ToolCommand *command, guint argc, const gchar **argv,
		      TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	const gchar *pname = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;
	gint whichargv = 0;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
                g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

        if (argv[0] && *argv[0]) {
                table = argv[0];
                pname = argv[0];
                if (argv[1] && *argv[1]) {
                        column = argv[1];
			filename = argv[1];
			if (argv[2] && *argv[2]) {
				row_cond = argv[2];
				if (argv[3] && *argv[3]) {
					filename = argv[3];
					if (argv [4]) {
						g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
							     "%s", _("Too many arguments"));
						return NULL;
					}
					else
						whichargv = 1;
				}
			}
			else {
				whichargv = 2;
			}
		}
        }

	const GValue *value = NULL;
	GdaDataModel *model = NULL;

	TApp *main_data;
	main_data = T_APP (global_t_app);
	T_APP_LOCK (main_data);

	if (whichargv == 1) 
		value = get_table_value_at_cell (console, error, table, column,
						 row_cond, &model);
	else if (whichargv == 2) {
		GdaHolder *param = g_hash_table_lookup (global_t_app->priv->parameters, pname);
		if (!pname) 
			g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     _("No parameter named '%s' defined"), pname);
		else
			value = gda_holder_get_value (param);
	}
	else 
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong number of arguments"));

	if (value) {
		/* to file through this blob */
		gboolean done = FALSE;

		if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			GdaBlob *fblob = g_value_get_boxed (value);
			if (gda_blob_op_write (gda_blob_get_op (tblob), (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
					     "%s", _("Could not write file"));
			else
				done = TRUE;
			gda_value_free (vblob);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			GdaBlob *fblob = gda_blob_new ();
			GdaBinary *fbin = g_value_get_boxed (value);
			gda_binary_set_data (gda_blob_get_binary (fblob),
			                    gda_binary_get_data (fbin),
			                    gda_binary_get_size (fbin));
			if (gda_blob_op_write (gda_blob_get_op (tblob), (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
					     "%s", _("Could not write file"));
			else
				done = TRUE;
			gda_blob_free (fblob);
			gda_value_free (vblob);
		}
		else {
			gchar *str;
			str = gda_value_stringify (value);
			if (g_file_set_contents (filename, str, -1, error))
				done = TRUE;
			g_free (str);
		}
		
		if (done) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
	}
	if (model)
		g_object_unref (model);
	T_APP_UNLOCK (main_data);
				
	return res;
}

static ToolCommandResult *
extra_command_set2 (ToolCommand *command, guint argc, const gchar **argv,
		    TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *filename = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *row_cond = NULL;
	gint whichargv = 0;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
                g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

        if (argv[0] && *argv[0]) {
                pname = argv[0];
                if (argv[1] && *argv[1]) {
			if (argv[2] && *argv[2]) {
				table = argv[1];
				column = argv[2];
				if (argv[3] && *argv[3]) {
					row_cond = argv[3];
					if (argv [4]) {
						g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
							     "%s", _("Too many arguments"));
						return NULL;
					}
					whichargv = 1;
				}
			}
			else {
				filename = argv[1];
				whichargv = 2;
			}
		}
        }

	TApp *main_data;
	main_data = T_APP (global_t_app);

	if (whichargv == 1) {
		/* param from an existing blob */
		const GValue *value;
		GdaDataModel *model = NULL;
		value = get_table_value_at_cell (console, error, table,
						 column, row_cond, &model);
		if (value) {
			T_APP_LOCK (main_data);
			GdaHolder *param = g_hash_table_lookup (global_t_app->priv->parameters, pname);
			if (param) 
				g_hash_table_remove (global_t_app->priv->parameters, pname);
		
			param = gda_holder_new (G_VALUE_TYPE (value), "blob");
			g_assert (gda_holder_set_value (param, value, NULL));
			g_hash_table_insert (global_t_app->priv->parameters, g_strdup (pname), param);
			T_APP_UNLOCK (main_data);

			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
		if (model)
			g_object_unref (model);
	}
	else if (whichargv == 2) {
		/* param from filename */
		T_APP_LOCK (main_data);
		GdaHolder *param = g_hash_table_lookup (global_t_app->priv->parameters, pname);
		GValue *bvalue;
		if (param) 
			g_hash_table_remove (global_t_app->priv->parameters, pname);
		
		param = gda_holder_new (GDA_TYPE_BLOB, "file");
		bvalue = gda_value_new_blob_from_file (filename);
		g_assert (gda_holder_take_value (param, bvalue, NULL));
		g_hash_table_insert (global_t_app->priv->parameters, g_strdup (pname), param);
		T_APP_UNLOCK (main_data);

		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}
	else 
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong number of arguments"));

	return res;
}

static ToolCommandResult *
extra_command_pivot (ToolCommand *command, guint argc, const gchar **argv,
		     TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	TConnection *tcnc;

	g_assert (console);
	g_assert (global_t_app);

	tcnc = t_context_get_connection (console);
	if (!tcnc) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	const gchar *select = NULL;
	const gchar *row_fields = NULL;
	const gchar *column_fields = NULL;

        if (argv[0] && *argv[0]) {
                select = argv[0];
                if (argv[1] && *argv[1]) {
			row_fields = argv [1];
			if (argv[2] && *argv[2])
				column_fields = argv[2];
		}
        }

	if (!select) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing data on which to operate"));
		return NULL;
	}
	if (!row_fields) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing row fields specifications"));
		return NULL;
	}
	if (column_fields && !strcmp (column_fields, "-"))
		column_fields = NULL;

	/* execute SELECT */
	gboolean was_in_trans;
	ToolCommandResult *tmpres;

	was_in_trans = gda_connection_get_transaction_status (t_connection_get_cnc (tcnc)) ? TRUE : FALSE;

	tmpres = t_context_command_execute (console, select, GDA_STATEMENT_MODEL_CURSOR_FORWARD, error);
	if (!tmpres)
		return NULL;
	if (tmpres->type != BASE_TOOL_COMMAND_RESULT_DATA_MODEL) {
		base_tool_command_result_free (tmpres);
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong SELECT argument"));
		return NULL;
	}

	GdaDataPivot *pivot;
	gdouble etime = 0.;
	//g_object_get ((GObject*) tmpres->u.model, "execution-delay", &etime, NULL);
	pivot = (GdaDataPivot*) gda_data_pivot_new (tmpres->u.model);
	base_tool_command_result_free (tmpres);

	if (! gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_ROW,
					row_fields, NULL, error)) {
		g_object_unref (pivot);
		return NULL;
	}

	if (column_fields &&
	    ! gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_COLUMN,
					column_fields, NULL, error)) {
		g_object_unref (pivot);
		return NULL;
	}

	GTimer *timer;
	timer = g_timer_new ();

	gint i;
	for (i = 3; argv[i] && *argv[i]; i++) {
		const gchar *df = argv[i];
		const gchar *alias = "count";
		GdaDataPivotAggregate agg = GDA_DATA_PIVOT_COUNT;
		if (*df == '[') {
			const gchar *tmp;
			for (tmp = df; *tmp && *tmp != ']'; tmp++);
			if (!*tmp) {
				g_timer_destroy (timer);
				g_object_unref (pivot);
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     "%s", _("Wrong data field argument"));
				return NULL;
			}
			df++;
			if (! g_ascii_strncasecmp (df, "sum", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_SUM;
				alias = "sum";
			}
			else if (! g_ascii_strncasecmp (df, "avg", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_AVG;
				alias = "avg";
			}
			else if (! g_ascii_strncasecmp (df, "max", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_MAX;
				alias = "max";
			}
			else if (! g_ascii_strncasecmp (df, "min", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_MIN;
				alias = "min";
			}
			else if (! g_ascii_strncasecmp (df, "count", 5) && (df[5] == ']')) {
				agg = GDA_DATA_PIVOT_COUNT;
				alias = "count";
			}
			else {
				g_timer_destroy (timer);
				g_object_unref (pivot);
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     "%s", _("Wrong data field argument"));
				return NULL;
			}
			df = tmp+1;
		}
		gchar *tmp;
		tmp = g_strdup_printf ("%s_%s", alias, df);
		if (! gda_data_pivot_add_data (pivot, agg, df, tmp, error)) {
			g_free (tmp);
			g_timer_destroy (timer);
			g_object_unref (pivot);
			return NULL;
		}
		g_free (tmp);
	}

	if (! gda_data_pivot_populate (pivot, error)) {
		g_timer_destroy (timer);
		g_object_unref (pivot);
		return NULL;
	}

	etime += g_timer_elapsed (timer, NULL);
	g_timer_destroy (timer);
	//g_object_set (pivot, "execution-delay", etime, NULL);

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->was_in_transaction_before_exec = was_in_trans;
	res->u.model = (GdaDataModel*) pivot;
	return res;
}

typedef struct {
	gchar *fkname;
	gchar *table;
	gchar *ref_table;
	GArray *columns;
	GArray *ref_columns;
} FkDeclData;

static void
fk_decl_data_free (FkDeclData *data)
{
	g_free (data->fkname);
	g_free (data->table);
	g_free (data->ref_table);
	if (data->columns) {
		guint i;
		for (i = 0; i < data->columns->len; i++)
			g_free (g_array_index (data->columns, gchar*, i));
		g_array_free (data->columns, TRUE);
	}
	if (data->ref_columns) {
		guint i;
		for (i = 0; i < data->ref_columns->len; i++)
			g_free (g_array_index (data->ref_columns, gchar*, i));
		g_array_free (data->ref_columns, TRUE);
	}
	g_free (data);
}

static FkDeclData *
parse_fk_decl_spec (const gchar *spec, gboolean columns_required, GError **error)
{
	FkDeclData *decldata;
	decldata = g_new0 (FkDeclData, 1);

	gchar *ptr, *dspec, *start, *subptr, *substart;

	if (!spec || !*spec) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing foreign key declaration specification"));
		return NULL;
	}
	dspec = g_strstrip (g_strdup (spec));

	/* FK name */
	for (ptr = dspec; *ptr && !g_ascii_isspace (*ptr); ptr++) {
		if (! g_ascii_isalnum (*ptr) && (*ptr != '_'))
			goto onerror;
	}
	if (!*ptr)
		goto onerror;
	*ptr = 0;
	decldata->fkname = g_strstrip (g_strdup (dspec));
#ifdef GDA_DEBUG_NO
	g_print ("KFNAME [%s]\n", decldata->fkname);
#endif

	/* table name */
	start = ++ptr;
	if (columns_required)
		for (ptr = start; *ptr && (*ptr != '('); ptr++);
	else {
		for (ptr = start; *ptr && g_ascii_isspace (*ptr); ptr++);
		start = ptr;
		for (; *ptr && !g_ascii_isspace (*ptr); ptr++);
	}
	if (!*ptr)
		goto onerror;
	*ptr = 0;
	if (!*start)
		goto onerror;
	decldata->table = g_strstrip (g_strdup (start));
#ifdef GDA_DEBUG_NO
	g_print ("TABLE [%s]\n", decldata->table);
#endif

	if (columns_required) {
		/* columns names */
		start = ++ptr;
		for (ptr = start; *ptr && (*ptr != ')'); ptr++);
		if (!*ptr)
			goto onerror;
		*ptr = 0;
#ifdef GDA_DEBUG_NO
		g_print ("COLS [%s]\n", start);
#endif
		substart = start;
		for (substart = start; substart < ptr; substart = ++subptr) {
			gchar *tmp;
			for (subptr = substart; *subptr && (*subptr != ','); subptr++);
			*subptr = 0;
			if (!decldata->columns)
				decldata->columns = g_array_new (FALSE, FALSE, sizeof (gchar*));
			tmp = g_strstrip (g_strdup (substart));
			g_array_append_val (decldata->columns, tmp);
#ifdef GDA_DEBUG_NO
			g_print ("  COL [%s]\n", tmp);
#endif
		}
		if (! decldata->columns)
			goto onerror;
	}

	/* ref table */
	start = ++ptr;
	if (columns_required)
		for (ptr = start; *ptr && (*ptr != '('); ptr++);
	else {
		for (ptr = start; *ptr && g_ascii_isspace (*ptr); ptr++);
		start = ptr;
		for (; *ptr && !g_ascii_isspace (*ptr); ptr++);
	}
	*ptr = 0;
	if (!*start)
		goto onerror;
	decldata->ref_table = g_strstrip (g_strdup (start));
#ifdef GDA_DEBUG_NO
	g_print ("REFTABLE [%s]\n", decldata->ref_table);
#endif

	if (columns_required) {
		/* ref columns names */
		start = ++ptr;
		for (ptr = start; *ptr && (*ptr != ')'); ptr++);
		if (!*ptr)
			goto onerror;
		*ptr = 0;
#ifdef GDA_DEBUG_NO
		g_print ("REF COLS [%s]\n", start);
#endif
		for (substart = start; substart < ptr; substart = ++subptr) {
			gchar *tmp;
			for (subptr = substart; *subptr && (*subptr != ','); subptr++);
			*subptr = 0;
			if (!decldata->ref_columns)
				decldata->ref_columns = g_array_new (FALSE, FALSE, sizeof (gchar*));
			tmp = g_strstrip (g_strdup (substart));
			g_array_append_val (decldata->ref_columns, tmp);
#ifdef GDA_DEBUG_NO
			g_print ("  COL [%s]\n", tmp);
#endif
		}
		if (! decldata->ref_columns)
			goto onerror;
	}

	g_free (dspec);

	if (columns_required && (decldata->columns->len != decldata->ref_columns->len))
		goto onerror;

	return decldata;

 onerror:
	fk_decl_data_free (decldata);
	g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
		     "%s", _("Malformed foreign key declaration specification"));
	return NULL;
}

/*
 * decomposes @table and sets the the out* variables
 */
static gboolean
fk_decl_analyse_table_name (const gchar *table, GdaMetaStore *mstore,
			    gchar **out_catalog, gchar **out_schema, gchar **out_table,
			    GError **error)
{
	gchar **id_array;
	gint size = 0, l;

	id_array = gda_sql_identifier_split (table);
	if (id_array) {
		size = g_strv_length (id_array) - 1;
		g_assert (size >= 0);
		l = size;
		*out_table = g_strdup (id_array[l]);
		l--;
		if (l >= 0) {
			*out_schema = g_strdup (id_array[l]);
			l--;
			if (l >= 0)
				*out_catalog = g_strdup (id_array[l]);
		}
		g_strfreev (id_array);
	}
	else {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Malformed table name specification '%s'"), table);
		return FALSE;
	}
	return TRUE;
}


static ToolCommandResult *
extra_command_declare_fk (ToolCommand *command, guint argc, const gchar **argv,
			  TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		FkDeclData *decldata;
		GdaMetaStore *mstore;
		gchar *catalog = NULL, *schema = NULL, *table = NULL;
		gchar *ref_catalog = NULL, *ref_schema = NULL, *ref_table = NULL;

		mstore = gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console)));
		if (! (decldata = parse_fk_decl_spec (argv[0], TRUE, error)))
			return NULL;
		
		/* table name */
		if (!fk_decl_analyse_table_name (decldata->table, mstore,
						 &catalog, &schema, &table, error)) {
			fk_decl_data_free (decldata);
			return NULL;
		}
		
		/* ref table name */
		if (!fk_decl_analyse_table_name (decldata->ref_table, mstore,
						 &ref_catalog, &ref_schema, &ref_table, error)) {
			fk_decl_data_free (decldata);
			g_free (catalog);
			g_free (schema);
			g_free (table);
			return NULL;
		}

#ifdef GDA_DEBUG_NO
		g_print ("NOW: %s.%s.%s REFERENCES %s.%s.%s\n",
			 gda_value_stringify (gda_set_get_holder_value (params, "tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tname")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tname")));
#endif

		gchar **colnames, **ref_colnames;
		guint l;
		gboolean allok;
		colnames = g_new0 (gchar *, decldata->columns->len);
		ref_colnames = g_new0 (gchar *, decldata->columns->len);
		for (l = 0; l < decldata->columns->len; l++) {
			colnames [l] = g_array_index (decldata->columns, gchar*, l);
			ref_colnames [l] = g_array_index (decldata->ref_columns, gchar*, l);
		}

		allok = gda_meta_store_declare_foreign_key (mstore, NULL,
							    decldata->fkname,
							    catalog, schema, table,
							    ref_catalog, ref_schema, ref_table,
							    decldata->columns->len,
							    colnames, ref_colnames, error);
		g_free (catalog);
		g_free (schema);
		g_free (table);
		g_free (ref_catalog);
		g_free (ref_schema);
		g_free (ref_table);
		g_free (colnames);
		g_free (ref_colnames);
		fk_decl_data_free (decldata);

		if (allok) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
	}
	else
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing foreign key name argument"));
	return res;
}

static ToolCommandResult *
extra_command_undeclare_fk (ToolCommand *command, guint argc, const gchar **argv,
			    TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	g_assert (global_t_app);

	if (!t_context_get_connection (console)) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		FkDeclData *decldata;
		GdaMetaStore *mstore;
		gchar *catalog = NULL, *schema = NULL, *table = NULL;
		gchar *ref_catalog = NULL, *ref_schema = NULL, *ref_table = NULL;

		mstore = gda_connection_get_meta_store (t_connection_get_cnc (t_context_get_connection (console)));
		if (! (decldata = parse_fk_decl_spec (argv[0], FALSE, error)))
			return NULL;
		
		/* table name */
		if (!fk_decl_analyse_table_name (decldata->table, mstore,
						 &catalog, &schema, &table, error)) {
			fk_decl_data_free (decldata);
			return NULL;
		}
		
		/* ref table name */
		if (!fk_decl_analyse_table_name (decldata->ref_table, mstore,
						 &ref_catalog, &ref_schema, &ref_table, error)) {
			fk_decl_data_free (decldata);
			g_free (catalog);
			g_free (schema);
			g_free (table);
			return NULL;
		}

#ifdef GDA_DEBUG_NO
		g_print ("NOW: %s.%s.%s REFERENCES %s.%s.%s\n",
			 gda_value_stringify (gda_set_get_holder_value (params, "tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tname")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tname")));
#endif

		gboolean allok;
		allok = gda_meta_store_undeclare_foreign_key (mstore, NULL,
							      decldata->fkname,
							      catalog, schema, table,
							      ref_catalog, ref_schema, ref_table,
							      error);
		g_free (catalog);
		g_free (schema);
		g_free (table);
		g_free (ref_catalog);
		g_free (ref_schema);
		g_free (ref_table);
		fk_decl_data_free (decldata);

		if (allok) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		}
	}
	else
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing foreign key name argument"));
	return res;
}

#ifdef HAVE_LDAP
static ToolCommandResult *
extra_command_ldap_search (ToolCommand *command, guint argc, const gchar **argv,
			   TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	TConnection *tcnc;
	GdaDataModel *model, *wrapper;

	g_assert (console);
	g_assert (global_t_app);

	tcnc = t_context_get_connection (console);
	if (!tcnc) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (t_connection_get_cnc (tcnc))) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *filter = NULL;
	gchar *lfilter = NULL;
	const gchar *scope = NULL;
	const gchar *base_dn = NULL;
	GdaLdapSearchScope lscope = GDA_LDAP_SEARCH_SUBTREE;

        if (argv[0] && *argv[0]) {
                filter = argv[0];
                if (argv[1]) {
			scope = argv [1];
			if (!g_ascii_strcasecmp (scope, "base"))
				lscope = GDA_LDAP_SEARCH_BASE;
			else if (!g_ascii_strcasecmp (scope, "onelevel"))
				lscope = GDA_LDAP_SEARCH_ONELEVEL;
			else if (!g_ascii_strcasecmp (scope, "subtree"))
				lscope = GDA_LDAP_SEARCH_SUBTREE;
			else {
				g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     _("Unknown search scope '%s'"), scope);
				return NULL;
			}
			if (argv[2])
				base_dn = argv[2];
		}
        }

	if (!filter) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing filter which to operate"));
		return NULL;
	}

	if (*filter != '(')
		lfilter = g_strdup_printf ("(%s)", filter);

	GdaHolder *param;
	const gchar *ldap_attributes;
	ldap_attributes = g_value_get_string (gda_set_get_holder_value (global_t_app->priv->options, "ldap_attributes"));
	model = GDA_DATA_MODEL (gda_data_model_ldap_new_with_config (t_connection_get_cnc (tcnc), base_dn, lfilter != NULL ?  lfilter : filter,
					 ldap_attributes ? ldap_attributes : T_DEFAULT_LDAP_ATTRIBUTES,
					 lscope));
	g_free (lfilter);
	param = gda_set_get_holder (global_t_app->priv->options, "ldap_dn");
	wrapper = gda_data_access_wrapper_new (model);
	g_object_unref (model);

	if (param) {
		const GValue *cvalue;
		gchar *tmp = NULL;
		cvalue = gda_holder_get_value (param);
		if (cvalue)
			tmp = gda_value_stringify (cvalue);
		if (tmp) {
			if (!g_ascii_strcasecmp (tmp, "rdn"))
				g_object_set ((GObject *) model, "use-rdn", TRUE, NULL);
			else if (!g_ascii_strncasecmp (tmp, "no", 2)) {
				gint nbcols;
				nbcols = gda_data_model_get_n_columns (wrapper);
				if (nbcols > 1) {
					gint *map, i;
					map = g_new (gint, nbcols - 1);
					for (i = 0; i < nbcols - 1; i++)
						map [i] = i+1;
					gda_data_access_wrapper_set_mapping (GDA_DATA_ACCESS_WRAPPER (wrapper),
									     map, nbcols - 1);
					g_free (map);
				}
			}
			g_free (tmp);
		}
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = wrapper;
	return res;
}

static ToolCommandResult *
extra_command_ldap_mv (ToolCommand *command, guint argc, const gchar **argv,
		       TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	TConnection *tcnc;

	g_assert (console);
	g_assert (global_t_app);

	tcnc = t_context_get_connection (console);
	if (!tcnc) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (t_connection_get_cnc (tcnc))) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *current_dn = NULL;
	const gchar *new_dn = NULL;

        if (argv[0] && *argv[0]) {
                current_dn = argv[0];
                if (argv[1])
			new_dn = argv [1];
        }

	if (!current_dn || !new_dn) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing current DN or new DN specification"));
		return NULL;
	}

	if (gda_ldap_rename_entry (GDA_LDAP_CONNECTION (t_connection_get_cnc (tcnc)), current_dn, new_dn, error)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

/*
 * parses text as <att_name>[=<att_value>]:
 *  - if value_req is %TRUE then the format has to be <att_name>=<att_value>
 *  - if value_req is %TRUE then the format has to be <att_name>=<att_value> or <att_name>
 *
 * if returned value is %TRUE, then @out_attr_name and @out_value are allocated
 */
static gboolean
parse_ldap_attr (const gchar *spec, gboolean value_req,
		 gchar **out_attr_name, GValue **out_value)
{
	const gchar *ptr;
	g_return_val_if_fail (spec && *spec, FALSE);
	*out_attr_name = NULL;
	*out_value = NULL;

	for (ptr = spec; *ptr && (*ptr != '='); ptr++);
	if (!*ptr) {
		if (value_req)
			return FALSE;
		else {
			*out_attr_name = g_strstrip (g_strdup (spec));
			return TRUE;
		}
	}
	else {
		/* copy attr name part */
		*out_attr_name = g_new (gchar, ptr - spec + 1);
		memcpy (*out_attr_name, spec, ptr - spec);
		(*out_attr_name) [ptr - spec] = 0;
		g_strstrip (*out_attr_name);

		/* move on to the attr value */
		ptr++;
		if (!*ptr) {
			/* no value ! */
			g_free (*out_attr_name);
			*out_attr_name = NULL;
			return FALSE;
		}
		else {
			gchar *tmp;
			*out_value = gda_value_new (G_TYPE_STRING);
			tmp = g_strdup (ptr);
			g_value_take_string (*out_value, tmp);
			return TRUE;
		}
	}
}

static ToolCommandResult *
extra_command_ldap_mod (ToolCommand *command, guint argc, const gchar **argv,
			TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	TConnection *tcnc;

	g_assert (console);
	g_assert (global_t_app);

	tcnc = t_context_get_connection (console);
	if (!tcnc) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (t_connection_get_cnc (tcnc))) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *dn = NULL;
	const gchar *op = NULL;
	GdaLdapModificationType optype;
        if (argv[0] && *argv[0]) {
                dn = argv[0];
                if (argv[1])
			op = argv [1];
        }

	if (!dn) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing DN of LDAP entry"));
		return NULL;
	}
	if (!op) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing operation to perform on LDAP entry's attributes"));
		return NULL;
	}

	if (! g_ascii_strncasecmp (op, "DEL", 3))
		optype = GDA_LDAP_MODIFICATION_ATTR_DEL;
	else if (! g_ascii_strncasecmp (op, "REPL", 4))
		optype = GDA_LDAP_MODIFICATION_ATTR_REPL;
	else if (! g_ascii_strncasecmp (op, "ADD", 3))
		optype = GDA_LDAP_MODIFICATION_ATTR_ADD;
	else {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Unknown operation '%s' to perform on LDAP entry's attributes"), op);
		return NULL;
	}

	GdaLdapEntry *entry;
	guint i;

	entry = gda_ldap_entry_new (dn);
	for (i = 2; argv[i]; i++) {
		gchar *att_name;
		GValue *att_value;
		gboolean vreq = TRUE;
		if (optype == GDA_LDAP_MODIFICATION_ATTR_DEL)
			vreq = FALSE;
		if (parse_ldap_attr (argv[i], vreq, &att_name, &att_value)) {
			gda_ldap_entry_add_attribute (entry, TRUE, att_name, 1, &att_value);
			g_free (att_name);
			gda_value_free (att_value);
		}
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("Wrong attribute value specification '%s'"), argv[i]);
			return NULL;
		}
	}

	if (gda_ldap_modify_entry (GDA_LDAP_CONNECTION (t_connection_get_cnc (tcnc)), optype, entry, NULL, error)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
	}
	gda_ldap_entry_free (entry);
	return res;
}


typedef struct {
	GValue *attr_name;
	GValue *req;
	GArray *values; /* array of #GValue */
} AttRow;

static gint
att_row_cmp (AttRow *r1, AttRow *r2)
{
	return strcmp (g_value_get_string (r1->attr_name), g_value_get_string (r2->attr_name));
}

static ToolCommandResult *
extra_command_ldap_descr (ToolCommand *command, guint argc, const gchar **argv,
			  TContext *console, GError **error)
{
	ToolCommandResult *res = NULL;
	TConnection *tcnc;

	g_assert (console);
	g_assert (global_t_app);

	tcnc = t_context_get_connection (console);
	if (!tcnc) {
		g_set_error (error, T_ERROR, T_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (t_connection_get_cnc (tcnc))) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *dn = NULL;
	GHashTable *attrs_hash = NULL;
	gint options = 0; /* 0 => show attributes specified with ".option ldap_attributes"
			   * 1 => show all attributes
			   * 2 => show non set attributes only
			   * 3 => show all set attributes only
			   */

        if (!argv[0] || !*argv[0]) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing DN (Distinguished name) argument"));		
		return NULL;
        }
	dn = argv[0];
	if (argv [1] && *argv[1]) {
		if (!g_ascii_strcasecmp (argv [1], "all"))
			options = 1;
		else if (!g_ascii_strcasecmp (argv [1], "unset"))
			options = 2;
		else if (!g_ascii_strcasecmp (argv [1], "set"))
			options = 3;
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     _("Unknown '%s' argument"), argv[1]);
			return NULL;
		}
	}

	GdaLdapEntry *entry;
	GdaDataModel *model;
	entry = gda_ldap_describe_entry (GDA_LDAP_CONNECTION (t_connection_get_cnc (tcnc)), dn, error);
	if (!entry) {
		if (error && !*error)
			g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
				     _("Could not find entry with DN '%s'"), dn);
		return NULL;
	}

	model = gda_data_model_array_new_with_g_types (3,
						       G_TYPE_STRING,
						       G_TYPE_BOOLEAN,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Attribute"));
	gda_data_model_set_column_title (model, 1, _("Required?"));
	gda_data_model_set_column_title (model, 2, _("Value"));
	g_object_set_data (G_OBJECT (model), "name", _("LDAP entry's Attributes"));

	/* parse attributes to use */
	const gchar *ldap_attributes;
	ldap_attributes = g_value_get_string (gda_set_get_holder_value (global_t_app->priv->options,
									"ldap_attributes"));
	if ((options == 0) && ldap_attributes) {
		gchar **array;
		guint i;
		array = g_strsplit (ldap_attributes, ",", 0);
		for (i = 0; array [i]; i++) {
			gchar *tmp;
			for (tmp = array [i]; *tmp && (*tmp != ':'); tmp++);
			*tmp = 0;
			g_strstrip (array [i]);
			if (!attrs_hash)
				attrs_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
								    NULL);
			g_hash_table_insert (attrs_hash, array [i], (gpointer) 0x01);
		}
		g_free (array);
	}

	/* Prepare rows */
	GSList *rows_list = NULL; /* sorted list of #AttRow, used to create the data model */
	GHashTable *rows_hash; /* key = att name, value = #AttRow */	
	rows_hash = g_hash_table_new (g_str_hash, g_str_equal);

	/* 1st pass: all actual entry's attributes */
	if (options != 2) {
		guint i;
		for (i = 0; i < entry->nb_attributes; i++) {
			GdaLdapAttribute *attr;
			guint j;
			attr = (GdaLdapAttribute*) entry->attributes[i];

			if (attrs_hash && !g_hash_table_lookup (attrs_hash, attr->attr_name))
				continue;

			AttRow *row;
			row = g_new0 (AttRow, 1);
			g_value_set_string ((row->attr_name = gda_value_new (G_TYPE_STRING)),
					    attr->attr_name);
			row->req = NULL;
			row->values = g_array_new (FALSE, FALSE, sizeof (GValue*));
			for (j = 0; j < attr->nb_values; j++) {
				GValue *copy;
				copy = gda_value_copy (attr->values [j]);
				g_array_append_val (row->values, copy);
			}

			g_hash_table_insert (rows_hash, (gpointer) g_value_get_string (row->attr_name), row);
			rows_list = g_slist_insert_sorted (rows_list, row, (GCompareFunc) att_row_cmp);
		}
	}

	/* 2nd pass: use the class's attributes */
	GSList *attrs_list, *list;
	attrs_list = gda_ldap_entry_get_attributes_list (GDA_LDAP_CONNECTION (t_connection_get_cnc (tcnc)), entry);
	for (list = attrs_list; list; list = list->next) {
		GdaLdapAttributeDefinition *def;
		def = (GdaLdapAttributeDefinition*) list->data;
		GdaLdapAttribute *vattr;
		vattr = g_hash_table_lookup (entry->attributes_hash, def->name);
		if (! (((options == 0) && vattr) ||
		       ((options == 3) && vattr) ||
		       (options == 1) ||
		       ((options == 2) && !vattr)))
			continue;

		if (attrs_hash && !g_hash_table_lookup (attrs_hash, def->name))
			continue;

		AttRow *row;
		row = g_hash_table_lookup (rows_hash, def->name);
		if (row) {
			if (!row->req)
				g_value_set_boolean ((row->req = gda_value_new (G_TYPE_BOOLEAN)),
						     def->required);
		}
		else {
			row = g_new0 (AttRow, 1);
			g_value_set_string ((row->attr_name = gda_value_new (G_TYPE_STRING)),
					    def->name);
			g_value_set_boolean ((row->req = gda_value_new (G_TYPE_BOOLEAN)),
					     def->required);
			row->values = NULL;
			g_hash_table_insert (rows_hash, (gpointer) g_value_get_string (row->attr_name), row);
			rows_list = g_slist_insert_sorted (rows_list, row, (GCompareFunc) att_row_cmp);
		}
	}
	gda_ldap_attributes_list_free (attrs_list);

	if (attrs_hash)
		g_hash_table_destroy (attrs_hash);

	/* store all rows in data model */
	GValue *nvalue = NULL;
	nvalue = gda_value_new_null ();
	for (list = rows_list; list; list = list->next) {
		AttRow *row;
		GList *vlist;
		guint i;
		row = (AttRow*) list->data;
		vlist = g_list_append (NULL, row->attr_name);
		if (row->req)
			vlist = g_list_append (vlist, row->req);
		else
			vlist = g_list_append (vlist, nvalue);
		if (row->values) {
			for (i = 0; i < row->values->len; i++) {
				GValue *value;
				value = g_array_index (row->values, GValue*, i);
				vlist = g_list_append (vlist, value);
				g_assert (gda_data_model_append_values (model, vlist, NULL) != -1);
				gda_value_free (value);
				vlist = g_list_remove (vlist, value);
			}
			g_array_free (row->values, TRUE);
		}
		else {
			vlist = g_list_append (vlist, nvalue);
			g_assert (gda_data_model_append_values (model, vlist, NULL) != -1);
		}
		g_list_free (vlist);
		gda_value_free (row->attr_name);
		gda_value_free (row->req);
		g_free (row);
	}
	g_slist_free (rows_list);


	gda_ldap_entry_free (entry);
	res = g_new0 (ToolCommandResult, 1);
	res->type = BASE_TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

#endif /* HAVE_LDAP */
