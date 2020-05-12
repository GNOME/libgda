/*
 * Copyright (C) 2000 Akira Tagoh <tagoh@src.gnome.org>
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 - 2004 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2003 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2004 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2005 Stanislav Brabec <sbrabec@suse.de>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2013, 2018 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
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
#define G_LOG_DOMAIN "GDA-config"

#include <unistd.h>
#include <stdio.h>
#include <gmodule.h>
#include <libgda/gda-config.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include "gda-marshal.h"
#include <string.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-dsn-list.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/sqlite/gda-sqlite-provider.h>

#ifdef HAVE_GIO
  #include <gio/gio.h>
#else
#endif
#ifdef G_OS_WIN32 
#include <io.h>
#endif

#ifdef HAVE_LIBSECRET
  #include <libsecret/secret.h>
  const SecretSchema *_gda_secret_get_schema (void) G_GNUC_CONST;
  #define GDA_SECRET_SCHEMA _gda_secret_get_schema ()
  #define SECRET_LABEL "DSN"
  const SecretSchema *
  _gda_secret_get_schema (void)
  {
	  static const SecretSchema the_schema = {
		  "org.gnome-db.DSN", SECRET_SCHEMA_NONE,
		  {
			  {  "DSN", SECRET_SCHEMA_ATTRIBUTE_STRING },
			  {  "NULL", 0 },
		  }
	  };
	  return &the_schema;
  }
#else
  #ifdef HAVE_GNOME_KEYRING
  #include <gnome-keyring.h>
  #define SECRET_LABEL "server"
  #endif
#endif

/*
   Register GdaDsnInfo type
*/
GType
gda_dsn_info_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		if (type == 0)
			type = g_boxed_type_register_static ("GdaDsnInfo",
							     (GBoxedCopyFunc) gda_dsn_info_copy,
							     (GBoxedFreeFunc) gda_dsn_info_free);
	}

	return type;
}

/**
 * gda_dsn_info_new:
 *
 * Creates a new empty #GdaDsnInfo struct.
 *
 * Returns: (transfer full): a new #GdaDsnInfo struct.
 *
 * Since: 5.2
 */
GdaDsnInfo*
gda_dsn_info_new (void)
{
	GdaDsnInfo *dsn = g_new0 (GdaDsnInfo, 1);
	dsn->name = NULL;
	dsn->provider = NULL;
	dsn->description = NULL;
	dsn->cnc_string = NULL;
	dsn->auth_string = NULL;
	dsn->is_system = FALSE;
	return dsn;
}

/**
 * gda_dsn_info_copy:
 * @source: a #GdaDsnInfo to copy from
 *
 * Copy constructor.
 *
 * Returns: (transfer full): a new #GdaDsnInfo
 *
 * Since: 5.2
 */
GdaDsnInfo *
gda_dsn_info_copy (const GdaDsnInfo *source)
{
	GdaDsnInfo *n;
	g_return_val_if_fail (source, NULL);
	n = gda_dsn_info_new ();
	n->name = source->name ? g_strdup (source->name) : NULL;
	n->provider = source->provider ? g_strdup (source->provider) : NULL;
	n->description = source->description ? g_strdup (source->description) : NULL;
	n->cnc_string = source->cnc_string ? g_strdup (source->cnc_string) : NULL;
	n->auth_string = source->auth_string ? g_strdup (source->auth_string) : NULL;
	n->is_system = source->is_system;
	return n;
}

/**
 * gda_dsn_info_free:
 * @dsn: (nullable): a #GdaDsnInfo struct to free
 *
 * Frees any resources taken by @dsn struct. If @dsn is %NULL, then nothing happens.
 *
 * Since: 5.2
 */
void
gda_dsn_info_free (GdaDsnInfo *dsn)
{
	if (dsn) {
		g_free (dsn->name);
		g_free (dsn->provider);
		g_free (dsn->description);
		g_free (dsn->cnc_string);
		g_free (dsn->auth_string);
		g_free (dsn);
	}
}

static
gboolean string_equal (const gchar *s1, const gchar *s2)
{
	if (s1) {
		if (s2)
			return !strcmp (s1, s2);
		else
			return FALSE;
	}
	else
		return s2 ? FALSE : TRUE;
}

static void gda_quark_process (gpointer key,
			       gpointer value,
			       gpointer string)
{
  gchar *evalue;

  g_string_append_c ((GString *)string, ',');
  evalue = gda_rfc1738_encode ((const gchar *)value);
  g_string_append_printf ((GString *)string, ",%s=%s", (gchar *)key, evalue);
  g_free (evalue);
}

static gchar *
make_cmp_string (const gchar *key_values_string)
{
	if (!key_values_string)
		return NULL;

	GdaQuarkList *ql;
	GSList *list, *sorted_list = NULL;
	GString *string = g_string_new("");
	ql = gda_quark_list_new_from_string (key_values_string);
	gda_quark_list_foreach (ql, gda_quark_process, string);
	gda_quark_list_free (ql);
	if (string)
		return g_string_free (string, FALSE);
	else
		return NULL;
}

/**
 * gda_dsn_info_equal:
 * @dsn1: (nullable): a #GdaDsnInfo
 * @dsn2: (nullable): a #GdaDsnInfo
 *
 * Compares @dsn1 and @dsn2.
 *
 * If both @dsn1 and @dsn2 are %NULL, then the function returns %TRUE.
 * If only one of @dsn1 or @dsn2 is %NULL, then the function return %FALSE.
 *
 * Returns: %TRUE if they are equal.
 */
gboolean
gda_dsn_info_equal (const GdaDsnInfo *dsn1, const GdaDsnInfo *dsn2)
{
	if (dsn1) {
		if (dsn2) {
			if ((dsn1->is_system == dsn2->is_system) &&
			    string_equal (dsn1->name, dsn2->name) &&
			    string_equal (dsn1->provider, dsn2->provider) &&
			    string_equal (dsn1->description, dsn2->description)) {
				gboolean eq;
				gchar *s1, *s2;
				s1 = make_cmp_string (dsn1->cnc_string);
				s2 = make_cmp_string (dsn2->cnc_string);
				eq = string_equal (s1, s2);
				g_free (s1);
				g_free (s2);
				if (eq) {
					s1 = make_cmp_string (dsn1->auth_string);
					s2 = make_cmp_string (dsn2->auth_string);
					eq = string_equal (s1, s2);
					g_free (s1);
					g_free (s2);
				}
				return eq;
			}
			else
				return FALSE;
		}
		else
			return FALSE;
	}
	else
		return dsn2 ? FALSE : TRUE;
}

/* GdaProviderInfo definitions */
GdaProviderInfo*
gda_provider_info_copy (GdaProviderInfo *src)
{
  GdaProviderInfo *dst = g_new0(GdaProviderInfo, 1);
  dst->id = g_strdup (src->id);
  dst->location = g_strdup (src->location);
  dst->description = g_strdup (src->description);
  dst->dsn_params = gda_set_copy (src->dsn_params);
  dst->auth_params = gda_set_copy (src->auth_params);
  dst->icon_id = g_strdup (src->icon_id);
	return dst;
}

void
gda_provider_info_free (GdaProviderInfo *info)
{
  g_free (info->id);
  g_free (info->location);
  g_free (info->description);
  g_object_unref (info->dsn_params);
  g_object_unref (info->auth_params);
  g_free (info->icon_id);
  g_free (info);
}

G_DEFINE_BOXED_TYPE (GdaProviderInfo, gda_provider_info, gda_provider_info_copy, gda_provider_info_free)

/* GdaInternalProvider */
typedef struct {
	GdaProviderInfo    pinfo;
	GModule           *handle;
	GdaServerProvider *instance;
} InternalProvider;

typedef struct {
	gchar *user_file;
	gchar *system_file;
	gboolean system_config_allowed;
	GSList *dsn_list; /* list of GdaDsnInfo structures */
	GSList *prov_list; /* list of InternalProvider structures */
	gboolean providers_loaded; /* TRUE if providers list has already been scanned */

	gboolean emit_signals;
} GdaConfigPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaConfig, gda_config, G_TYPE_OBJECT)

static GObject *gda_config_constructor (GType type,
					guint n_construct_properties,
					GObjectConstructParam *construct_properties);
static void gda_config_dispose    (GObject *object);
static void gda_config_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gda_config_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);
static GdaConfig *unique_instance = NULL;
#ifdef HAVE_LIBSECRET
static gboolean sync_keyring = FALSE;
#else
  #ifdef HAVE_GNOME_KEYRING
static gboolean sync_keyring = FALSE;
  #endif
#endif

static gint data_source_info_compare (GdaDsnInfo *infoa, GdaDsnInfo *infob);
static void internal_provider_free (InternalProvider *ip);
static void load_config_file (const gchar *file, gboolean is_system);
static void save_config_file (gboolean is_system);
static void load_all_providers (void);
static void reload_dsn_configuration (void);


enum {
	DSN_ADDED,
	DSN_TO_BE_REMOVED,
	DSN_REMOVED,
	DSN_CHANGED,
	LAST_SIGNAL
};

static gint gda_config_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };

/* properties */
enum {
	PROP_0,
	PROP_USER_FILE,
	PROP_SYSTEM_FILE
};

#ifdef HAVE_GIO
/*
 * GIO static variables
 */
static GFileMonitor *mon_conf_user = NULL;
static GFileMonitor *mon_conf_global = NULL;
gulong user_notify_changes = 0;
gulong global_notify_changes = 0;

static void conf_file_changed (GFileMonitor *mon, GFile *file, GFile *other_file,
			       GFileMonitorEvent event_type, gpointer data);
static void lock_notify_changes (void);
static void unlock_notify_changes (void);
#endif

static GRecMutex gda_rmutex;
#define GDA_CONFIG_LOCK() g_rec_mutex_lock(&gda_rmutex)
#define GDA_CONFIG_UNLOCK() g_rec_mutex_unlock(&gda_rmutex)

/* GdaServerProvider for SQLite as a shortcut, available
 * even if the SQLite provider is not installed
 */
GdaServerProvider *_gda_config_sqlite_provider = NULL;

/*
 * GdaConfig class implementation
 * @klass:
 */
static void
gda_config_class_init (GdaConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * GdaConfig::dsn-added:
	 * @conf: the #GdaConfig object
	 * @new_dsn: a #GdaDsnInfo
	 *
	 * Gets emitted whenever a new DSN has been defined
	 */
	gda_config_signals[DSN_ADDED] =
                g_signal_new ("dsn-added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_added),
                              NULL, NULL,
                              _gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	/**
	 * GdaConfig::dsn-to-be-removed:
	 * @conf: the #GdaConfig object
	 * @old_dsn: a #GdaDsnInfo
	 *
	 * Gets emitted whenever a DSN is about to be removed
	 */
	gda_config_signals[DSN_TO_BE_REMOVED] =
                g_signal_new ("dsn-to-be-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_to_be_removed),
                              NULL, NULL,
                              _gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	/**
	 * GdaConfig::dsn-removed:
	 * @conf: the #GdaConfig object
	 * @old_dsn: a #GdaDsnInfo
	 *
	 * Gets emitted whenever a DSN has been removed
	 */
	gda_config_signals[DSN_REMOVED] =
                g_signal_new ("dsn-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_removed),
                              NULL, NULL,
                              _gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	/**
	 * GdaConfig::dsn-changed:
	 * @conf: the #GdaConfig object
	 * @dsn: a #GdaDsnInfo
	 *
	 * Gets emitted whenever a DSN's definition has been changed
	 */
	gda_config_signals[DSN_CHANGED] =
                g_signal_new ("dsn-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_changed),
                              NULL, NULL,
                              _gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);

	/* Properties */
        object_class->set_property = gda_config_set_property;
        object_class->get_property = gda_config_get_property;

	/**
	 * GdaConfig:user-filename:
	 *
	 * File to use for per-user DSN list. When changed, the whole list of DSN will be reloaded.
	 */
	/* To translators: DSN stands for Data Source Name, it's a named connection string defined in $XDG_DATA_HOME/libgda/config */
	g_object_class_install_property (object_class, PROP_USER_FILE,
                                         g_param_spec_string ("user-filename", NULL, 
							      "File to use for per-user DSN list", 
							      NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/**
	 * GdaConfig:system-filename:
	 *
	 * File to use for system-wide DSN list. When changed, the whole list of DSN will be reloaded.
	 */
	/* To translators: DSN stands for Data Source Name, it's a named connection string defined in $PREFIX/etc/libgda-6.0/config */
	g_object_class_install_property (object_class, PROP_SYSTEM_FILE,
                                         g_param_spec_string ("system-filename", NULL,
							      "File to use for system-wide DSN list", 
							      NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
	object_class->constructor = gda_config_constructor;
	object_class->dispose = gda_config_dispose;
}

static void
gda_config_init (GdaConfig *conf)
{
	g_return_if_fail (GDA_IS_CONFIG (conf));
	GdaConfigPrivate *priv = gda_config_get_instance_private (conf);
	priv->user_file = NULL;
	priv->system_file = NULL;
	priv->system_config_allowed = FALSE;
	priv->prov_list = NULL;
	priv->dsn_list = NULL;
	priv->providers_loaded = FALSE;
	priv->emit_signals = TRUE;
}

#ifdef HAVE_LIBSECRET
static void
secret_password_found_cb (GObject *source_object, GAsyncResult *res, gchar *dsnname)
{
	gchar *auth;
	GError *error = NULL;
	auth = secret_password_lookup_finish (res, &error);
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
	if (auth) {
		GdaDsnInfo *dsn;
		dsn = gda_config_get_dsn_info (dsnname);
		if (dsn) {
			if (dsn->auth_string && auth && !strcmp (dsn->auth_string, auth))
				return;

			g_free (dsn->auth_string);
			dsn->auth_string = g_strdup (auth);
		}
		/*g_print ("Loaded auth info for '%s'\n", dsnname);*/
		if (priv->emit_signals)
			g_signal_emit (unique_instance, gda_config_signals [DSN_CHANGED], 0, dsn);
		g_free (auth);
	}
	else if (error) {
		gda_log_message (_("Error loading authentication information for '%s' DSN: %s"),
				 dsnname, error->message ? error->message : _("No detail"));
		g_clear_error (&error);
	}
	g_free (dsnname);
}
#else
  #ifdef HAVE_GNOME_KEYRING
static void
password_found_cb (GnomeKeyringResult res, const gchar *password, const gchar *dsnname)
{
        if (res == GNOME_KEYRING_RESULT_OK) {
		GdaDsnInfo *dsn;
		dsn = gda_config_get_dsn_info (dsnname);
		if (dsn) {
			if (dsn->auth_string && password && !strcmp (dsn->auth_string, password))
				return;

			g_free (dsn->auth_string);
			dsn->auth_string = g_strdup (password);
		}
		/*g_print ("Loaded auth info for '%s'\n", dsnname);*/
		if (priv->emit_signals)
			g_signal_emit (unique_instance, gda_config_signals [DSN_CHANGED], 0, dsn);
	}
	else if (res != GNOME_KEYRING_RESULT_NO_MATCH)
		gda_log_message (_("Error loading authentication information for '%s' DSN: %s"),
				 dsnname, gnome_keyring_result_to_message (res));
}
  #endif
#endif

static void
load_config_file (const gchar *file, gboolean is_system)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	if (!g_file_test (file, G_FILE_TEST_EXISTS))
		return;

	doc = xmlParseFile (file);
	if (!doc) 
		return;

	root = xmlDocGetRootElement (doc);
	if (root) {
		xmlNodePtr node;
		for (node = root->children; node; node = node->next) { /* iter over the <section> tags */
			if (strcmp ((gchar *) node->name, "section"))
				continue;

			xmlChar *prop;
			gchar *ptr;
			GdaDsnInfo *info;
			gboolean is_new = FALSE;
			xmlNodePtr entry;
			
			prop = xmlGetProp (node, BAD_CAST "path");
			if (!prop)
				continue;
			for (ptr = ((gchar *) prop) + strlen ((gchar *) prop) - 1; ptr >= (gchar *) prop; ptr--) {
				if (*ptr == '/') {
					ptr++;
					break;
				}
			}
			info = gda_config_get_dsn_info (ptr);
			if (!info) {
				info = g_new0 (GdaDsnInfo, 1);
				info->name = g_strdup (ptr);
				is_new = TRUE;
			}
			else {
				g_free (info->provider); info->provider = NULL;
				g_free (info->cnc_string); info->cnc_string = NULL;
				g_free (info->description); info->description = NULL;
				g_free (info->auth_string); info->auth_string = NULL;
			}
			info->is_system = is_system;
			xmlFree (prop);
			
			gchar *username = NULL, *password = NULL;
			for (entry = node->children; entry; entry = entry->next) { /* iter over the <entry> tags */
				xmlChar *value;
				if (strcmp ((gchar *)entry->name, "entry"))
					continue;
				prop = xmlGetProp (entry, BAD_CAST "name");
				if (!prop)
					continue;
				value = xmlGetProp (entry, BAD_CAST "value");
				if (!value) {
					xmlFree (prop);
					continue;
				}
				if (!strcmp ((gchar *) prop, "DSN")) 
					info->cnc_string = g_strdup ((gchar *)value);
				else if (!strcmp ((gchar *) prop, "Provider")) {
					GdaProviderInfo *pinfo;
					pinfo = gda_config_get_provider_info ((gchar *)value);
					if (pinfo)
						info->provider = g_strdup (pinfo->id);
					else
						info->provider = g_strdup ((gchar *)value);
				}
				else if (!strcmp ((gchar *) prop, "Description"))
					info->description = g_strdup ((gchar *)value);
				if (!strcmp ((gchar *) prop, "Auth"))
					info->auth_string = g_strdup ((gchar *)value);
				else if (!strcmp ((gchar *) prop, "Username"))
					username = g_strdup ((gchar*) value);
				else if (!strcmp ((gchar *) prop, "Password"))
					password =  g_strdup ((gchar*) value);
				xmlFree (prop);
				xmlFree (value);
			}
			
			if (username && *username) {
				if (!info->auth_string) {
					/* migrate username/password to auth_string */
					gchar *s1;
					s1 = gda_rfc1738_encode (username);
					if (password) {
						gchar *s2;
						s2 = gda_rfc1738_encode (password);
						info->auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", s1, s2);
						g_free (s2);
					}
					else
						info->auth_string = g_strdup_printf ("USERNAME=%s", s1);
					g_free (s1);
				}
			}
			g_free (username);
			g_free (password);

#ifdef HAVE_LIBSECRET
			if (! is_system) {
				if (sync_keyring) {
					GError *error = NULL;
					gchar *auth = NULL;
					auth = secret_password_lookup_sync (GDA_SECRET_SCHEMA,
									    NULL, &error,
									    SECRET_LABEL, info->name, NULL);
					if (auth) {
						/*g_print ("Loaded sync. auth info for '%s': %s\n", info->name, auth);*/
						info->auth_string = g_strdup (auth);
						g_free (auth);
					}
					else if (error) {
						gda_log_message (_("Error loading authentication information for '%s' DSN: %s"),
								 info->name, error->message ? error->message : _("No detail"));
						g_clear_error (&error);
					}
				}
				else {
					secret_password_lookup (GDA_SECRET_SCHEMA,
								NULL, (GAsyncReadyCallback) secret_password_found_cb,
								g_strdup (info->name),
								SECRET_LABEL, info->name, NULL);
				}
			}
#else
  #ifdef HAVE_GNOME_KEYRING
			if (! is_system) {
				if (sync_keyring) {
					GnomeKeyringResult res;
					gchar *auth = NULL;
					res = gnome_keyring_find_password_sync (GNOME_KEYRING_NETWORK_PASSWORD, &auth,
										SECRET_LABEL, info->name, NULL);
					if (res == GNOME_KEYRING_RESULT_OK) {
						/*g_print ("Loaded sync. auth info for '%s': %s\n", info->name, auth);*/
						info->auth_string = g_strdup (auth);
					}
					else if (res != GNOME_KEYRING_RESULT_NO_MATCH)
						gda_log_message (_("Error loading authentication information for '%s' DSN: %s"),
								 info->name, gnome_keyring_result_to_message (res));
					if (auth)
						gnome_keyring_free_password (auth);
				}
				else {
					gchar *tmp = g_strdup (info->name);
					gnome_keyring_find_password (GNOME_KEYRING_NETWORK_PASSWORD,
								     (GnomeKeyringOperationGetStringCallback) password_found_cb,
								     tmp, g_free,
								     SECRET_LABEL, tmp, NULL);
				}
			}
  #endif
#endif
			/* signals */
			if (is_new) {
				priv->dsn_list = g_slist_insert_sorted (priv->dsn_list, info,
											 (GCompareFunc) data_source_info_compare);
				if (priv->emit_signals)
					g_signal_emit (unique_instance, gda_config_signals[DSN_ADDED], 0, info);
			}
			else if (priv->emit_signals)
				g_signal_emit (unique_instance, gda_config_signals[DSN_CHANGED], 0, info);
		}
	}
	xmlFreeDoc (doc);
}

static void
save_config_file (gboolean is_system)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GSList *list;

	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
	
	if ((!is_system && !priv->user_file) ||
	    (is_system && !priv->system_file)) {
		return;
	}

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewDocNode (doc, NULL, BAD_CAST "libgda-config", NULL);
        xmlDocSetRootElement (doc, root);
	for (list = priv->dsn_list; list; list = list->next) {
		GdaDsnInfo *info = (GdaDsnInfo*) list->data;
		if (info->is_system != is_system)
			continue;

		xmlNodePtr section, entry;
		gchar *prop;
		section = xmlNewChild (root, NULL, BAD_CAST "section", NULL);
		prop = g_strdup_printf ("/apps/libgda/Datasources/%s", info->name);
		xmlSetProp (section, BAD_CAST "path", BAD_CAST prop);
		g_free (prop);

		/* provider */
		entry = xmlNewChild (section, NULL, BAD_CAST "entry", NULL);
		xmlSetProp (entry, BAD_CAST "name", BAD_CAST "Provider");
		xmlSetProp (entry, BAD_CAST "type", BAD_CAST "string");
		xmlSetProp (entry, BAD_CAST "value", BAD_CAST (info->provider));

		/* DSN */
		entry = xmlNewChild (section, NULL, BAD_CAST "entry", NULL);
		xmlSetProp (entry, BAD_CAST "name", BAD_CAST "DSN");
		xmlSetProp (entry, BAD_CAST "type", BAD_CAST "string");
		xmlSetProp (entry, BAD_CAST "value", BAD_CAST (info->cnc_string));

#if !defined (HAVE_GNOME_KEYRING) && !defined (HAVE_LIBSECRET)
		if (! is_system) {
			/* auth */
			entry = xmlNewChild (section, NULL, BAD_CAST "entry", NULL);
			xmlSetProp (entry, BAD_CAST "name", BAD_CAST "Auth");
			xmlSetProp (entry, BAD_CAST "type", BAD_CAST "string");
			xmlSetProp (entry, BAD_CAST "value", BAD_CAST (info->auth_string));
		}
#endif

		/* description */
		entry = xmlNewChild (section, NULL, BAD_CAST "entry", NULL);
		xmlSetProp (entry, BAD_CAST "name", BAD_CAST "Description");
		xmlSetProp (entry, BAD_CAST "type", BAD_CAST "string");
		xmlSetProp (entry, BAD_CAST "value", BAD_CAST (info->description));
	}

#ifdef HAVE_GIO
	lock_notify_changes ();
#endif
	if (!is_system && priv->user_file) {
		if (xmlSaveFormatFile (priv->user_file, doc, TRUE) == -1)
                        gda_log_error ("Error saving config data to '%s'", priv->user_file);
	}
	else if (is_system && priv->system_file) {
		if (xmlSaveFormatFile (priv->system_file, doc, TRUE) == -1)
                        gda_log_error ("Error saving config data to '%s'", priv->system_file);
	}
	fflush (NULL);
#ifdef HAVE_GIO
	unlock_notify_changes ();
#endif
	xmlFreeDoc (doc);
}

static GObject *
gda_config_constructor (GType type,
			guint n_construct_properties,
			GObjectConstructParam *construct_properties)
{
	GObject *object;
  
	if (!unique_instance) {
#if defined(HAVE_LIBSECRET) || defined(HAVE_GNOME_KEYRING)
		if (g_getenv ("GDA_CONFIG_SYNCHRONOUS"))
			sync_keyring = TRUE;
#endif

		guint i;
		gboolean user_file_set = FALSE, system_file_set = FALSE;

		object = G_OBJECT_CLASS (gda_config_parent_class)->constructor (type,
								     n_construct_properties,
								     construct_properties);
		for (i = 0; i< n_construct_properties; i++) {
			GObjectConstructParam *prop = &(construct_properties[i]);
			if (!strcmp (g_param_spec_get_name (prop->pspec), "user-filename")) {
				user_file_set = TRUE;
				/*g_print ("GdaConfig user dir set\n");*/
			}
			else if (!strcmp (g_param_spec_get_name (prop->pspec), "system-filename")) {
				system_file_set = TRUE;
				/*g_print ("GdaConfig system dir set\n");*/
			}
		}

		unique_instance = GDA_CONFIG (object);
		GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
		g_object_ref (object); /* keep one reference for the library */

		/* define user and system dirs. if not already defined */
		if (!user_file_set) {
			gchar *confdir, *conffile;
			gboolean setup_ok = TRUE;
			confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "libgda", NULL);
			conffile = g_build_filename (confdir, "config", NULL);

			if (!g_file_test (confdir, G_FILE_TEST_EXISTS)) {
				gchar *old_path;
				old_path = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".libgda", NULL); /* Flawfinder: ignore */
				if (g_file_test (old_path, G_FILE_TEST_EXISTS)) {
					/* using $HOME/.libgda because it exists */
					g_free (confdir);
					confdir = old_path;
					g_free (conffile);
					conffile = g_build_filename (confdir, "config", NULL);
				}
				else {
					g_free (old_path);
					if (g_mkdir_with_parents (confdir, 0700)) {
						setup_ok = FALSE;
						gda_log_error (_("Error creating user specific "
								 "configuration directory '%s'"), 
							       confdir);
					}
					if (setup_ok) {
						gchar *str;
						gchar *full_file;
						gsize len;
#define DB_FILE "sales_test.db"
#define DEFAULT_CONFIG \
"<?xml version=\"1.0\"?>\n" \
"<libgda-config>\n" \
"    <section path=\"/apps/libgda/Datasources/SalesTest\">\n" \
"        <entry name=\"DSN\" type=\"string\" value=\"DB_DIR=%s;DB_NAME=sales_test.db\"/>\n" \
"        <entry name=\"Description\" type=\"string\" value=\"Test database for a sales department\"/>\n" \
"        <entry name=\"Provider\" type=\"string\" value=\"SQLite\"/>\n" \
"    </section>\n" \
"</libgda-config>\n"
#define DEFAULT_CONFIG_EMPTY \
"<?xml version=\"1.0\"?>\n" \
"<libgda-config>\n" \
"    <!-- User specific data sources go here -->\n" \
"</libgda-config>\n"

						str = gda_gbr_get_file_path (GDA_ETC_DIR, 
									     LIBGDA_ABI_NAME, DB_FILE, NULL);
						if (g_file_get_contents (str, &full_file, &len, NULL)) {
							gchar *dbfile;
							
							/* copy the Sales test database */
							dbfile = g_build_filename (confdir, DB_FILE, NULL);
							if (g_file_set_contents (dbfile, full_file, len, NULL)) {
								gchar *str2;
								str2 = g_strdup_printf (DEFAULT_CONFIG, confdir);
								g_file_set_contents (conffile, str2, -1, NULL);
								g_free (str2);
							}
							else
								g_file_set_contents (conffile, DEFAULT_CONFIG_EMPTY, -1, NULL);
							g_free (dbfile);
							g_free (full_file);
						}
						else 
							g_file_set_contents (conffile, DEFAULT_CONFIG_EMPTY, -1, NULL);
						g_free (str);
					}
				}
				if (setup_ok && !g_file_test (confdir, G_FILE_TEST_IS_DIR)) {
					setup_ok = FALSE;
					gda_log_message (_("User specific "
							   "configuration directory '%s' exists and is not a directory"), 
							 confdir);
				}
				g_free (confdir);

				if (setup_ok)
					priv->user_file = conffile;
				else
					g_free (conffile);
			}
			else {
				if (!g_file_test (confdir, G_FILE_TEST_IS_DIR)) {
					gda_log_message (_("User specific "
							   "configuration directory '%s' exists and is not a directory"), 
							 confdir);
					g_free (conffile);
				}
				else
					priv->user_file = conffile;
				g_free (confdir);
			}
		}
		if (!system_file_set) 
			priv->system_file = gda_gbr_get_file_path (GDA_ETC_DIR,
										    LIBGDA_ABI_NAME, "config", NULL);
		priv->system_config_allowed = FALSE;
		if (priv->system_file) {
#ifdef G_OS_WIN32

			FILE *file;
                        file = fopen (priv->system_file, "a");  /* Flawfinder: ignore */
                        if (file) {
                                priv->system_config_allowed = TRUE;
                                fclose (file);
                        }
#else
			struct stat stbuf;
			if (stat (priv->system_file, &stbuf) == 0) {
				/* use effective user and group IDs */
				uid_t euid;
				gid_t egid;
				euid = geteuid ();
				egid = getegid ();
				if (euid == stbuf.st_uid) {
					if ((stbuf.st_mode & S_IWUSR) && (stbuf.st_mode & S_IRUSR))
						priv->system_config_allowed = TRUE;
				}
				else if (egid == stbuf.st_gid) {
					if ((stbuf.st_mode & S_IWGRP) && (stbuf.st_mode & S_IRGRP))
						priv->system_config_allowed = TRUE;
				}
				else if ((stbuf.st_mode & S_IWOTH) && (stbuf.st_mode & S_IROTH))
					priv->system_config_allowed = TRUE;
			}
#endif
		}

		/* Setup file monitoring */
#ifdef HAVE_GIO
		if (priv->user_file) {
			GFile *gf;
			gf = g_file_new_for_path (priv->user_file);
			mon_conf_user = g_file_monitor_file (gf, G_FILE_MONITOR_NONE, NULL, NULL);
			if (mon_conf_user)
				g_signal_connect (G_OBJECT (mon_conf_user), "changed",
						  G_CALLBACK (conf_file_changed), NULL);
			g_object_unref (gf);
		}

		if (priv->system_file) {
			GFile *gf;
			gf = g_file_new_for_path (priv->system_file);
			mon_conf_global = g_file_monitor_file (gf, G_FILE_MONITOR_NONE, NULL, NULL);
			if (mon_conf_user)
				g_signal_connect (G_OBJECT (mon_conf_global), "changed",
						  G_CALLBACK (conf_file_changed), NULL);
			g_object_unref (gf);
		}
#endif
		/* load existing DSN definitions */
		if (priv->system_file)
			load_config_file (priv->system_file, TRUE);
		if (priv->user_file)
			load_config_file (priv->user_file, FALSE);
	}
	else
		object = g_object_ref (G_OBJECT (unique_instance));

	return object;
}

static void
gda_config_dispose (GObject *object)
{
	GdaConfig *conf = (GdaConfig *) object;

	g_return_if_fail (GDA_IS_CONFIG (conf));
	GdaConfigPrivate *priv = gda_config_get_instance_private (conf);

	if (priv) {
		if (priv->user_file) {
			g_free (priv->user_file);
			priv->user_file = NULL;
		}
		if (priv->system_file) {
			g_free (priv->system_file);
			priv->system_file = NULL;
		}

		if (priv->dsn_list) {
			g_slist_free_full (priv->dsn_list, (GDestroyNotify) gda_dsn_info_free);
			priv->dsn_list = NULL;
		}
		if (priv->prov_list) {
			g_slist_free_full (priv->prov_list, (GDestroyNotify) internal_provider_free);
			priv->prov_list = NULL;
		}
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_config_parent_class)->dispose (object);
}


/* module error */
GQuark gda_config_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_config_error");
        return quark;
}


static void
gda_config_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdaConfig *conf;
	const gchar *cstr;

	conf = GDA_CONFIG (object);
	GdaConfigPrivate *priv = gda_config_get_instance_private (conf);
	switch (param_id) {
		case PROP_USER_FILE:
			cstr = g_value_get_string (value);
			if ((cstr && priv->user_file &&
			     !strcmp (cstr, priv->user_file)) ||
			    (! cstr && !priv->user_file)) {
				/* nothing to do */
				break;
			}
			g_free (priv->user_file);
			priv->user_file = NULL;
			if (g_value_get_string (value))
				priv->user_file = g_strdup (cstr);
			reload_dsn_configuration ();
			break;
                case PROP_SYSTEM_FILE:
			cstr = g_value_get_string (value);
			if ((cstr && priv->system_file &&
			     !strcmp (cstr, priv->system_file)) ||
			    (! cstr && !priv->system_file)) {
				/* nothing to do */
				break;
			}
			g_free (priv->system_file);
			priv->system_file = NULL;
			if (g_value_get_string (value))
				priv->system_file = g_strdup (cstr);
			reload_dsn_configuration ();
                        break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_config_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdaConfig *conf;
	
	conf = GDA_CONFIG (object);
	GdaConfigPrivate *priv = gda_config_get_instance_private (conf);
	switch (param_id) {
		case PROP_USER_FILE:
			g_value_set_string (value, priv->user_file);
			break;
		case PROP_SYSTEM_FILE:
			g_value_set_string (value, priv->system_file);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_config_get:
 * 
 * Get a pointer to the global (unique) #GdaConfig object. This functions increments
 * the reference count of the object, so you need to call g_object_unref() on it once finished.
 *
 * Returns: (transfer full): a non %NULL pointer to the unique #GdaConfig
 */
GdaConfig*
gda_config_get (void)
{
	GDA_CONFIG_LOCK ();
	g_object_new (GDA_TYPE_CONFIG, NULL);
	g_assert (unique_instance);
	GDA_CONFIG_UNLOCK ();
	return unique_instance;
}

/**
 * gda_config_get_dsn_info:
 * @dsn_name: the name of the DSN to look for
 *
 * Get information about the DSN named @dsn_name. 
 *
 * @dsn_name's format is "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" (if &lt;username&gt;
 * and optionally &lt;password&gt; are provided, they are ignored). Also see the gda_dsn_split() utility
 * function.
 *
 * Returns: (transfer none): a pointer to read-only #GdaDsnInfo structure, or %NULL if not found
 */
GdaDsnInfo *
gda_config_get_dsn_info (const gchar *dsn_name)
{
	GSList *list;

	g_return_val_if_fail (dsn_name, NULL);

	gchar *user, *pass, *real_dsn;
        gda_dsn_split (dsn_name, &real_dsn, &user, &pass);
	g_free (user);
	g_free (pass);
        if (!real_dsn) {
		gda_log_message (_("Malformed data source name '%s'"), dsn_name);
                return NULL;
	}

	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	for (list = priv->dsn_list; list; list = list->next)
		if (!strcmp (((GdaDsnInfo*) list->data)->name, real_dsn)) {
			GDA_CONFIG_UNLOCK ();
			g_free (real_dsn);
			return (GdaDsnInfo*) list->data;
		}
	GDA_CONFIG_UNLOCK ();
	g_free (real_dsn);
	return NULL;
}

#ifdef HAVE_LIBSECRET
static void
secret_password_stored_cb (GObject *source_object, GAsyncResult *res, gchar *dsnname)
{
	GError *error = NULL;
	if (! secret_password_store_finish (res, &error)) {
		gda_log_error (_("Couldn't save authentication information for DSN '%s': %s"),
			       dsnname, error && error->message ? error->message : _("No detail"));
		g_clear_error (&error);
	}
	g_free (dsnname);
}
#else
  #ifdef HAVE_GNOME_KEYRING
static void
password_stored_cb (GnomeKeyringResult res, const gchar *dsnname)
{
        if (res != GNOME_KEYRING_RESULT_OK)
                gda_log_error (_("Couldn't save authentication information for DSN '%s': %s"), dsnname,
			       gnome_keyring_result_to_message (res));
}
  #endif
#endif

/**
 * gda_config_define_dsn:
 * @info: a pointer to a filled GdaDsnInfo structure
 * @error: a place to store errors, or %NULL
 *
 * Add or update a DSN from the definition in @info.
 *
 * This method may fail with a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_define_dsn (const GdaDsnInfo *info, GError **error)
{
	GdaDsnInfo *einfo;
	gboolean save_user = FALSE;
	gboolean save_system = FALSE;

	g_return_val_if_fail (info, FALSE);
	g_return_val_if_fail (info->name, FALSE);

	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	if (info->is_system && !priv->system_config_allowed) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PERMISSION_ERROR,
			      "%s", _("Can't manage system-wide configuration"));
		GDA_CONFIG_UNLOCK ();
		return FALSE;
	}

	if (info->is_system)
		save_system = TRUE;
	else
		save_user = TRUE;
	einfo = gda_config_get_dsn_info (info->name);
	if (einfo) {
		g_free (einfo->provider); einfo->provider = NULL;
		g_free (einfo->cnc_string); einfo->cnc_string = NULL;
		g_free (einfo->description); einfo->description = NULL;
		g_free (einfo->auth_string); einfo->auth_string = NULL;
		if (info->provider)
			einfo->provider = g_strdup (info->provider);
		if (info->cnc_string)
			einfo->cnc_string = g_strdup (info->cnc_string);
		if (info->description)
			einfo->description = g_strdup (info->description);
		if (info->auth_string)
			einfo->auth_string = g_strdup (info->auth_string);
		
		if (info->is_system != einfo->is_system) {
			save_system = TRUE;
			save_user = TRUE;
			einfo->is_system = info->is_system ? TRUE : FALSE;
		}
		if (priv->emit_signals)
			g_signal_emit (unique_instance, gda_config_signals[DSN_CHANGED], 0, einfo);
	}
	else {
		einfo = g_new0 (GdaDsnInfo, 1);
		einfo->name = g_strdup (info->name);
		if (info->provider)
			einfo->provider = g_strdup (info->provider);
		if (info->cnc_string)
			einfo->cnc_string = g_strdup (info->cnc_string);
		if (info->description)
			einfo->description = g_strdup (info->description);
		if (info->auth_string)
			einfo->auth_string = g_strdup (info->auth_string);
		einfo->is_system = info->is_system ? TRUE : FALSE;

		priv->dsn_list = g_slist_insert_sorted (priv->dsn_list, einfo,
									 (GCompareFunc) data_source_info_compare);
		if (priv->emit_signals)
			g_signal_emit (unique_instance, gda_config_signals[DSN_ADDED], 0, einfo);
	}

#ifdef HAVE_LIBSECRET
	if (! info->is_system && info->auth_string) {
		/* save to keyring */
		gchar *tmp;
		tmp = g_strdup_printf (_("Gnome-DB: authentication for the '%s' DSN"), info->name);
		if (sync_keyring) {
			gboolean res;
			GError *error = NULL;
			res = secret_password_store_sync (GDA_SECRET_SCHEMA,
							  SECRET_COLLECTION_DEFAULT,
							  tmp, info->auth_string,
							  NULL, &error,
							  SECRET_LABEL, info->name, NULL);
			if (!res) {
				gda_log_error (_("Couldn't save authentication information for DSN '%s': %s"),
					       info->name,
					       error && error->message ? error->message : _("No detail"));
				g_clear_error (&error);
			}
		}
		else {
			secret_password_store (GDA_SECRET_SCHEMA,
					       SECRET_COLLECTION_DEFAULT,
					       tmp, info->auth_string,
					       NULL,
					       (GAsyncReadyCallback) secret_password_stored_cb,
					       g_strdup (info->name),
					       SECRET_LABEL, info->name, NULL);
		}
		g_free (tmp);
	}
#else
  #ifdef HAVE_GNOME_KEYRING
	if (! info->is_system && info->auth_string) {
		/* save to keyring */
		gchar *tmp;
		tmp = g_strdup_printf (_("Gnome-DB: authentication for the '%s' DSN"), info->name);
		if (sync_keyring) {
			GnomeKeyringResult res;
			res = gnome_keyring_store_password_sync (GNOME_KEYRING_NETWORK_PASSWORD, GNOME_KEYRING_DEFAULT,
								 tmp, info->auth_string,
								 SECRET_LABEL, info->name, NULL);
			password_stored_cb (res, info->name);
		}
		else {
			gchar *tmp1;
			tmp1 = g_strdup (info->name);
			gnome_keyring_store_password (GNOME_KEYRING_NETWORK_PASSWORD,
						      GNOME_KEYRING_DEFAULT,
						      tmp, info->auth_string,
						      (GnomeKeyringOperationDoneCallback) password_stored_cb, tmp1, g_free,
						      SECRET_LABEL, info->name, NULL);
		}
		g_free (tmp);
	}
  #endif
#endif
	
	if (save_system)
		save_config_file (TRUE);
	if (save_user)
		save_config_file (FALSE);

	GDA_CONFIG_UNLOCK ();
	return TRUE;
}

#ifdef HAVE_LIBSECRET
static void
secret_password_deleted_cb (GObject *source_object, GAsyncResult *res, gchar *dsnname)
{
	GError *error = NULL;
	if (! secret_password_clear_finish (res, &error)) {
                gda_log_error (_("Couldn't delete authentication information for DSN '%s': %s"), dsnname,
			       error && error->message ? error->message : _("No detail"));
		g_clear_error (&error);
	}
	g_free (dsnname);
}
#else
  #ifdef HAVE_GNOME_KEYRING
static void
password_deleted_cb (GnomeKeyringResult res, const gchar *dsnname)
{
	if (res != GNOME_KEYRING_RESULT_OK)
                gda_log_error (_("Couldn't delete authentication information for DSN '%s': %s"), dsnname,
			       gnome_keyring_result_to_message (res));
}
  #endif
#endif

/**
 * gda_config_remove_dsn:
 * @dsn_name: the name of the DSN to remove
 * @error: a place to store errors, or %NULL
 *
 * Remove the DSN named @dsn_name.
 *
 * This method may fail with a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_config_remove_dsn (const gchar *dsn_name, GError **error)
{
	GdaDsnInfo *info;
	gboolean save_user = FALSE;
	gboolean save_system = FALSE;

	g_return_val_if_fail (dsn_name, FALSE);
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();

	info = gda_config_get_dsn_info (dsn_name);
	if (!info) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_DSN_NOT_FOUND_ERROR,
			     _("Unknown DSN '%s'"), dsn_name);
		GDA_CONFIG_UNLOCK ();
		return FALSE;
	}
	if (info->is_system && !priv->system_config_allowed) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PERMISSION_ERROR,
			      "%s", _("Can't manage system-wide configuration"));
		GDA_CONFIG_UNLOCK ();
		return FALSE;
	}

	if (info->is_system)
		save_system = TRUE;
	else
		save_user = TRUE;

	if (priv->emit_signals)
		g_signal_emit (unique_instance, gda_config_signals[DSN_TO_BE_REMOVED], 0, info);
	priv->dsn_list = g_slist_remove (priv->dsn_list, info);
	if (priv->emit_signals)
		g_signal_emit (unique_instance, gda_config_signals[DSN_REMOVED], 0, info);

#ifdef HAVE_LIBSECRET
	if (! info->is_system) {
		/* remove from keyring */
		if (sync_keyring) {
			GError *error = NULL;
			if (! secret_password_clear_sync (GDA_SECRET_SCHEMA,
							  NULL, &error,
							  SECRET_LABEL, info->name, NULL)) {
				gda_log_error (_("Couldn't delete authentication information for DSN '%s': %s"),
					       info->name,
					       error && error->message ? error->message : _("No detail"));
				g_clear_error (&error);
			}
		}
		else {
			secret_password_clear (GDA_SECRET_SCHEMA,
					       NULL, (GAsyncReadyCallback) secret_password_deleted_cb,
					       g_strdup (info->name),
					       SECRET_LABEL, info->name, NULL);
		}
	}
#else
  #ifdef HAVE_GNOME_KEYRING
	if (! info->is_system) {
		/* remove from keyring */
		if (sync_keyring) {
			GnomeKeyringResult res;
			res = gnome_keyring_delete_password_sync (GNOME_KEYRING_NETWORK_PASSWORD, GNOME_KEYRING_DEFAULT,
								  SECRET_LABEL, info->name, NULL);
			password_deleted_cb (res, info->name);
		}
		else {
			gchar *tmp;
			tmp = g_strdup (dsn_name);
			gnome_keyring_delete_password (GNOME_KEYRING_NETWORK_PASSWORD,
						       (GnomeKeyringOperationDoneCallback) password_deleted_cb,
						       tmp, g_free,
						       SECRET_LABEL, info->name, NULL);
		}
	}
  #endif
#endif
	gda_dsn_info_free (info);

	if (save_system)
		save_config_file (TRUE);
	if (save_user)
		save_config_file (FALSE);

	GDA_CONFIG_UNLOCK ();
	return TRUE;
}

/**
 * gda_config_dsn_needs_authentication:
 * @dsn_name: the name of a DSN, in the "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" format
 * 
 * Tells if the data source identified as @dsn_name needs any authentication. If a &lt;username&gt;
 * and optionally a &lt;password&gt; are specified, they are ignored.
 *
 * Returns: TRUE if an authentication is needed
 */
gboolean
gda_config_dsn_needs_authentication (const gchar *dsn_name)
{
	GdaDsnInfo *info;
	GdaProviderInfo *pinfo;

	info = gda_config_get_dsn_info (dsn_name);
	if (!info)
		return FALSE;
	pinfo = gda_config_get_provider_info (info->provider);
	if (!pinfo) {
		gda_log_message (_("Provider '%s' not found"), info->provider);
		return FALSE;
	}
	if (pinfo->auth_params && gda_set_get_holders (pinfo->auth_params))
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_config_list_dsn:
 * 
 * Get a #GdaDataModel representing all the configured DSN, and keeping itself up to date with
 * the changes in the declared DSN.
 *
 * The returned data model is composed of the following columns:
 * <itemizedlist>
 *  <listitem><para>DSN name</para></listitem>
 *  <listitem><para>Provider name</para></listitem>
 *  <listitem><para>Description</para></listitem>
 *  <listitem><para>Connection string</para></listitem>
 *  <listitem><para>Username if it exists</para></listitem>
 * </itemizedlist>
 *
 * Returns: (transfer full): a new #GdaDataModel
 */
GdaDataModel *
gda_config_list_dsn (void)
{
	GdaDataModel *model;
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	
	model = GDA_DATA_MODEL (g_object_new (GDA_TYPE_DATA_MODEL_DSN_LIST, NULL));
	GDA_CONFIG_UNLOCK ();
	return model;
}

/**
 * gda_config_get_nb_dsn:
 *
 * Get the number of defined DSN
 *
 * Returns: the number of defined DSN
 */
gint
gda_config_get_nb_dsn (void)
{
	gint ret;
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	ret = g_slist_length (priv->dsn_list);
	GDA_CONFIG_UNLOCK ();
	return ret;
}

/**
 * gda_config_get_dsn_info_index:
 * @dsn_name: a DSN
 * 
 * Get the index (starting at 0) of the DSN named @dsn_name
 * 
 * Returns: the index or -1 if not found
 */
gint
gda_config_get_dsn_info_index (const gchar *dsn_name)
{
	GdaDsnInfo *info;
	gint ret = -1;

	g_return_val_if_fail (dsn_name, -1);
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	info = gda_config_get_dsn_info (dsn_name);
	if (info)
		ret = g_slist_index (priv->dsn_list, info);

	GDA_CONFIG_UNLOCK ();
	return ret;
}

/**
 * gda_config_get_dsn_info_at_index:
 * @index: an index
 *
 * Get a pointer to a read-only #GdaDsnInfo at the @index position
 *
 * Returns: (transfer none):the pointer or %NULL if no DSN exists at position @index
 */
GdaDsnInfo *
gda_config_get_dsn_info_at_index (gint index)
{
	GdaDsnInfo *ret;
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
	ret = g_slist_nth_data (priv->dsn_list, index);
	GDA_CONFIG_UNLOCK ();
	return ret;
}

/**
 * gda_config_can_modify_system_config:
 *
 * Tells if the global (system) configuration can be modified (considering
 * system permissions and settings)
 *
 * Returns: TRUE if system-wide configuration can be modified
 */
gboolean
gda_config_can_modify_system_config (void)
{
	gboolean retval;
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
	retval = priv->system_config_allowed;
	GDA_CONFIG_UNLOCK ();
	return retval;
}

/**
 * gda_config_get_provider_info:
 * @provider_name: a database provider
 *
 * Get some information about the a database provider (adapter) named 
 *
 * Returns: (transfer none): a pointer to read-only #GdaProviderInfo structure, or %NULL if not found
 */
GdaProviderInfo *
gda_config_get_provider_info (const gchar *provider_name)
{
	GSList *list;
	g_return_val_if_fail (provider_name, NULL);
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	if (!priv->providers_loaded)
		load_all_providers ();

	if (!g_ascii_strcasecmp (provider_name, "MS Access")) {
		GDA_CONFIG_UNLOCK ();
		return gda_config_get_provider_info ("MSAccess");
	}

	for (list = priv->prov_list; list; list = list->next)
		if (!g_ascii_strcasecmp (((GdaProviderInfo*) list->data)->id, provider_name)) {
			GDA_CONFIG_UNLOCK ();
			return (GdaProviderInfo*) list->data;
		}
	GDA_CONFIG_UNLOCK ();
	return NULL;
}

/**
 * gda_config_get_provider:
 * @provider_name: a database provider
 * @error: a place to store errors, or %NULL
 *
 * Get a pointer to the session-wide #GdaServerProvider for the
 * provider named @provider_name. The caller must not call g_object_unref() on the
 * returned object.
 *
 * This method may fail with a %GDA_CONFIG_ERROR domain error (see the #GdaConfigError error codes).
 *
 * Returns: (transfer none): a pointer to the #GdaServerProvider, or %NULL if an error occurred
 */
GdaServerProvider *
gda_config_get_provider (const gchar *provider_name, GError **error)
{
	InternalProvider *ip;
	GdaServerProvider  *(*plugin_create_provider) (void);
	GdaServerProvider  *(*plugin_create_sub_provider) (const gchar *provider_name);

	g_return_val_if_fail (provider_name, NULL);
	GDA_CONFIG_LOCK ();
	ip = (InternalProvider *) gda_config_get_provider_info (provider_name);
	if (!ip) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
			     _("No provider '%s' installed"), provider_name);
		GDA_CONFIG_UNLOCK ();
		return NULL;
	}
	if (ip->instance) {
		GDA_CONFIG_UNLOCK ();
		return ip->instance;
	}
	/* need to actually create the provider object */
	if (!ip->handle) {
		GdaProviderInfo *info = (GdaProviderInfo*) ip;
		ip->handle = g_module_open (info->location, G_MODULE_BIND_LAZY);
		if (!ip->handle) {
			g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_CREATION_ERROR,
				     _("Can't load provider: %s"), g_module_error ());
			return NULL;
		}

		void (*plugin_init) (void);
		if (g_module_symbol (ip->handle, "plugin_init", (gpointer *) &plugin_init)) {
			plugin_init ();
		}
	}

	g_module_symbol (ip->handle, "plugin_create_provider", (gpointer) &plugin_create_provider);
	if (plugin_create_provider) {
	  ip->instance = plugin_create_provider ();
  } else {
		g_module_symbol (ip->handle, "plugin_create_sub_provider", (gpointer) &plugin_create_sub_provider);
		if (plugin_create_sub_provider)
			ip->instance = plugin_create_sub_provider (provider_name);
	}
	
	if (!ip->instance) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_CREATION_ERROR,
			     _("Can't instantiate provider '%s'"), provider_name);
		GDA_CONFIG_UNLOCK ();
		return NULL;
	}

	GDA_CONFIG_UNLOCK ();
	return ip->instance;
}
 
/**
 * gda_config_list_providers:
 * 
 * Get a #GdaDataModel representing all the installed database providers.
 *
 * The returned data model is composed of the following columns:
 * <itemizedlist>
 *  <listitem><para>Provider name</para></listitem>
 *  <listitem><para>Description</para></listitem>
 *  <listitem><para>DSN parameters</para></listitem>
 *  <listitem><para>Authentication parameters</para></listitem>
 *  <listitem><para>File name of the plugin</para></listitem>
 * </itemizedlist>
 *
 * Returns: (transfer full): a new #GdaDataModel
 */
GdaDataModel *
gda_config_list_providers (void)
{
	GSList *list;
	GdaDataModel *model;
	
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

	if (!priv->providers_loaded)
		load_all_providers ();

	model = gda_data_model_array_new_with_g_types (5,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Provider"));
	gda_data_model_set_column_title (model, 1, _("Description"));
	gda_data_model_set_column_title (model, 2, _("DSN parameters"));
	gda_data_model_set_column_title (model, 3, _("Authentication"));
	gda_data_model_set_column_title (model, 4, _("File"));
	g_object_set_data (G_OBJECT (model), "name", _("List of installed providers"));

	for (list = priv->prov_list; list; list = list->next) {
		GdaProviderInfo *info = (GdaProviderInfo *) list->data;
		GValue *value;
		gint row;

		row = gda_data_model_append_row (model, NULL);

		value = gda_value_new_from_string (info->id, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 0, row, value, NULL);
		gda_value_free (value);

		if (info->description) 
			value = gda_value_new_from_string (info->description, G_TYPE_STRING);
		else
			value = gda_value_new_null ();
		gda_data_model_set_value_at (model, 1, row, value, NULL);
		gda_value_free (value);

		if (info->dsn_params) {
			GSList *params;
			GString *string = g_string_new ("");
			for (params = gda_set_get_holders (info->dsn_params);
			     params; params = params->next) {
				const gchar *id;

				id = gda_holder_get_id (GDA_HOLDER (params->data));
				if (params != gda_set_get_holders (info->dsn_params))
					g_string_append (string, ",\n");
				g_string_append (string, id);
			}
			value = gda_value_new_from_string (string->str, G_TYPE_STRING);
			g_string_free (string, TRUE);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);
		}

		if (info->auth_params) {
			GSList *params;
			GString *string = g_string_new ("");
			for (params = gda_set_get_holders (info->auth_params);
			     params; params = params->next) {
				const gchar *id;

				id = gda_holder_get_id (GDA_HOLDER (params->data));
				if (params != gda_set_get_holders (info->auth_params))
					g_string_append (string, ",\n");
				g_string_append (string, id);
			}
			value = gda_value_new_from_string (string->str, G_TYPE_STRING);
			g_string_free (string, TRUE);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
		}

		value = gda_value_new_from_string (info->location, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 4, row, value, NULL);
		gda_value_free (value);
	}
	g_object_set (G_OBJECT (model), "read-only", TRUE, NULL);

	GDA_CONFIG_UNLOCK ();
	return model;
}

static gint
internal_provider_sort_func (InternalProvider *a, InternalProvider *b)
{
	return strcmp (a->pinfo.id, b->pinfo.id);
}

static void load_providers_from_dir (const gchar *dirname, gboolean recurs);
static void
load_all_providers (void)
{
	const gchar *dirname;
	g_assert (unique_instance);
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
	GError *error = NULL;

	dirname = g_getenv ("GDA_TOP_BUILD_DIR");
	if (dirname) {
		gchar *pdir;
		pdir = g_build_path (G_DIR_SEPARATOR_S, dirname, "providers", NULL);
		load_providers_from_dir (pdir, TRUE);
		g_free (pdir);
	}
	else {
		gchar *str;
		str = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "providers", NULL);
		load_providers_from_dir (str, FALSE);
		g_free (str);
	}
	priv->providers_loaded = TRUE;

	/* find SQLite provider, and instantiate it if not installed */
	_gda_config_sqlite_provider = gda_config_get_provider ("SQLite", &error);
	if (!_gda_config_sqlite_provider) {
		g_message (_("No default SQLite Provider Trying to create a new one. Error was: %s"),
		          error && error->message ? error->message: _("No detail"));
		g_clear_error (&error);
		_gda_config_sqlite_provider = (GdaServerProvider*) 
			g_object_new (GDA_TYPE_SQLITE_PROVIDER, NULL);
	}

	/* sort providers by name */
	priv->prov_list = g_slist_sort (priv->prov_list,
							 (GCompareFunc) internal_provider_sort_func);
}

static InternalProvider *
create_internal_provider (const gchar *path,
			  const gchar *prov_name, const gchar *prov_descr,
			  gchar *dsn_spec, gchar *auth_spec, const gchar *icon_id)
{
	g_return_val_if_fail (prov_name, NULL);

	InternalProvider *ip;
	GdaProviderInfo *info;

	ip = g_new0 (InternalProvider, 1);
	ip->handle = NULL;
	info = (GdaProviderInfo*) ip;
	info->location = g_strdup (path);

	info->id = g_strdup (prov_name);
	info->description = g_strdup (prov_descr);

	/* DSN parameters */
	info->dsn_params = NULL;
	if (dsn_spec) {
		GError *error = NULL;
		info->dsn_params = gda_set_new_from_spec_string (dsn_spec, &error);
		if (!info->dsn_params) {
			g_warning ("Invalid format for provider '%s' DSN spec : %s",
					 info->id,
					 error ? error->message : "Unknown error");
			g_clear_error (&error);
#ifdef GDA_DEBUG
			g_print ("Dump DSN spec:\n%s\n", dsn_spec);
#endif

			/* there may be traces of the provider installed but some parts are missing,
			   forget about that provider... */
			internal_provider_free (ip);
			g_free (dsn_spec);
			return NULL;
		}
		g_free (dsn_spec);
	}
	else
		gda_log_message ("Provider '%s' does not provide a DSN spec", info->id);

	/* Authentication parameters */
	info->auth_params = NULL;
	if (auth_spec) {
		GError *error = NULL;

		info->auth_params = gda_set_new_from_spec_string (auth_spec, &error);
		if (!info->auth_params) {
			g_warning ("Invalid format for provider '%s' AUTH spec : %s",
					 info->id,
				   error ? error->message : "Unknown error");
			if (error)
				g_error_free (error);
		}

		if (!info->auth_params) {
			/* there may be traces of the provider installed but some parts are missing,
			   forget about that provider... */
			internal_provider_free (ip);
			g_free (auth_spec);
			return NULL;
		}
		g_free (auth_spec);
	}
	else {
		/* default to username/password */
		GdaHolder *h;
		info->auth_params = gda_set_new_inline (2, "USERNAME", G_TYPE_STRING, NULL,
							"PASSWORD", G_TYPE_STRING, NULL);
		h = gda_set_get_holder (info->auth_params, "USERNAME");
		g_object_set (G_OBJECT (h), "name", _("Username"), "not-null", TRUE, NULL);
		h = gda_set_get_holder (info->auth_params, "PASSWORD");
		g_object_set (G_OBJECT (h), "name", _("Password"), "not-null", TRUE, NULL);

		GValue *value;
#define GDAUI_ATTRIBUTE_PLUGIN "__gdaui_attr_plugin"
		value = gda_value_new_from_string ("string:HIDDEN=true", G_TYPE_STRING);
		g_object_set (h, "plugin", value, NULL);
		gda_value_free (value);
	}

	info->icon_id = icon_id ? g_strdup (icon_id) : g_strdup (prov_name);

	return ip;
}

static void 
load_providers_from_dir (const gchar *dirname, gboolean recurs)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *name;

	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);
	/* read the plugin directory */
#ifdef GDA_DEBUG
	g_print ("Loading providers in %s\n", dirname);
#endif
	dir = g_dir_open (dirname, 0, &err);
	if (err) {
		gda_log_error (err->message);
		g_error_free (err);
		return;
	}
	
	while ((name = g_dir_read_name (dir))) {
		GModule *handle;
		gchar *path;

		/* initialization method */
		void (*plugin_init) (const gchar *);

		/* methods for shared libraries which provide only one type of provider */
		const gchar * (* plugin_get_name) (void);
		const gchar * (* plugin_get_description) (void);
		gchar * (* plugin_get_dsn_spec) (void);
		gchar * (* plugin_get_auth_spec) (void);
		const gchar * (*plugin_get_icon_id) (void);
		
		/* methods for shared libraries which provide several types of providers (ODBC, JDBC, ...) */
		const gchar ** (* plugin_get_sub_names) (void);
		const gchar * (* plugin_get_sub_description) (const gchar *name);
		gchar * (* plugin_get_sub_dsn_spec) (const gchar *name);
		gchar * (* plugin_get_sub_auth_spec) (const gchar *name);
		const gchar * (*plugin_get_sub_icon_id) (const gchar *name);

		if (recurs) {
			gchar *cname;
			cname = g_build_filename (dirname, name, NULL);
			if (g_file_test (cname, G_FILE_TEST_IS_DIR)) 
				load_providers_from_dir (cname, TRUE);
			g_free (cname);
		}
		if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX))
			continue;
#ifdef G_WITH_CYGWIN
		if (!g_str_has_prefix (name, "cyggda"))
#else
		if (!g_str_has_prefix (name, "libgda"))
#endif
			continue;

#ifdef GDA_DEBUG
		g_print ("File's name checking for provider: %s\n", name);
#endif
		path = g_build_path (G_DIR_SEPARATOR_S, dirname, name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY);
		if (!handle) {
			if (g_getenv ("GDA_SHOW_PROVIDER_LOADING_ERROR"))
				gda_log_message (_("Error loading provider '%s': %s"), path, g_module_error ());
#ifdef GDA_DEBUG
		g_print ("Error loading provider's module: %s : %s\n", path, g_module_error ());
#endif
			g_free (path);
			continue;
		}

		if (g_module_symbol (handle, "plugin_init", (gpointer *) &plugin_init)) {
			plugin_init (dirname);
		}
		else {
			g_module_close (handle);
			g_free (path);
			continue;
		}
		g_module_symbol (handle, "plugin_get_name",
				 (gpointer *) &plugin_get_name);
		g_module_symbol (handle, "plugin_get_description",
				 (gpointer *) &plugin_get_description);
		g_module_symbol (handle, "plugin_get_dsn_spec",
				 (gpointer *) &plugin_get_dsn_spec);
		g_module_symbol (handle, "plugin_get_auth_spec",
				 (gpointer *) &plugin_get_auth_spec);
		g_module_symbol (handle, "plugin_get_icon_id",
				 (gpointer *) &plugin_get_icon_id);
		g_module_symbol (handle, "plugin_get_sub_names",
				 (gpointer *) &plugin_get_sub_names);
		g_module_symbol (handle, "plugin_get_sub_description",
				 (gpointer *) &plugin_get_sub_description);
		g_module_symbol (handle, "plugin_get_sub_dsn_spec",
				 (gpointer *) &plugin_get_sub_dsn_spec);
		g_module_symbol (handle, "plugin_get_sub_auth_spec",
				 (gpointer *) &plugin_get_sub_auth_spec);
		g_module_symbol (handle, "plugin_get_sub_icon_id",
				 (gpointer *) &plugin_get_sub_icon_id);

		if (plugin_get_sub_names) {
			const gchar **subnames = plugin_get_sub_names ();
			const gchar **ptr;
			for (ptr = subnames; ptr && *ptr; ptr++) {
				InternalProvider *ip;
								
				ip = create_internal_provider (path, *ptr,
							       plugin_get_sub_description ? 
							       plugin_get_sub_description (*ptr) : NULL,
							       plugin_get_sub_dsn_spec ? 
							       plugin_get_sub_dsn_spec (*ptr) : NULL,
							       plugin_get_sub_auth_spec ?
							       plugin_get_sub_auth_spec (*ptr) : NULL,
							       plugin_get_sub_icon_id ?
							       plugin_get_sub_icon_id (*ptr) : NULL);
				if (ip) {
					priv->prov_list =
						g_slist_prepend (priv->prov_list, ip);
#ifdef GDA_DEBUG
					g_print ("Loaded '%s' sub-provider\n", ((GdaProviderInfo*) ip)->id);
#endif
				}
#ifdef GDA_DEBUG
				else {
					g_print ("Error Loading sub-provider\n");
				}
#endif
			}
		}
		else {
			InternalProvider *ip;
			ip = create_internal_provider (path,
						       plugin_get_name ? plugin_get_name () : name,
						       plugin_get_description ? plugin_get_description () : NULL,
						       plugin_get_dsn_spec ? plugin_get_dsn_spec () : NULL,
						       plugin_get_auth_spec ? plugin_get_auth_spec () : NULL,
						       plugin_get_icon_id ? plugin_get_icon_id () : NULL);
			if (ip) {
				priv->prov_list =
					g_slist_prepend (priv->prov_list, ip);
#ifdef GDA_DEBUG
				g_print ("Loaded '%s' provider\n", ((GdaProviderInfo*) ip)->id);
#endif
			}
#ifdef GDA_DEBUG
				else {
					g_print ("Error Loading provider\n");
				}
#endif
		}
		g_free (path);
		g_module_close (handle);
	}

	/* free memory */
	g_dir_close (dir);
	GDA_CONFIG_UNLOCK ();
}



/* sorting function */
static gint
data_source_info_compare (GdaDsnInfo *infoa, GdaDsnInfo *infob)
{
	if (!infoa && !infob)
		return 0;
	if (infoa) {
		if (!infob)
			return 1;
		else {
			gchar *u8a, *u8b;
			gint res;
			u8a = g_utf8_casefold (infoa->name, -1);
			u8b = g_utf8_casefold (infob->name, -1);
			res = strcmp (u8a, u8b);
			g_free (u8a);
			g_free (u8b);
			return res;
		}
	}
	else
		return -1;
}

static void
internal_provider_free (InternalProvider *ip)
{
	GdaProviderInfo *info = (GdaProviderInfo*) ip;
	if (ip->instance)
		g_object_unref (ip->instance);
	if (ip->handle) 
		g_module_close (ip->handle);
	
	g_free (info->id);
	g_free (info->location);
	g_free (info->description);
	if (info->dsn_params)
		g_object_unref (info->dsn_params);
	g_free (ip);
}

static gboolean
str_equal (const gchar *str1, const gchar *str2)
{
	if (str1 && str2) {
		if (!strcmp (str1, str2))
			return TRUE;
		else
			return FALSE;
	}
	else if (!str1 && !str2)
		return TRUE;
	return FALSE;
}

static void
reload_dsn_configuration (void)
{
	GSList *list, *current_dsn_list, *new_dsn_list;
	if (!unique_instance) {
		/* object not yet created */
		return;
	}
	GdaConfigPrivate *priv = gda_config_get_instance_private (unique_instance);

        GDA_CONFIG_LOCK ();
#ifdef GDA_DEBUG_NO
	gboolean is_system = (mon == mon_conf_global) ? TRUE : FALSE;
	g_print ("Reloading config files (%s config has changed)\n", is_system ? "global" : "user");
	for (list = priv->dsn_list; list; list = list->next) {
		GdaDsnInfo *info = (GdaDsnInfo *) list->data;
		g_print ("[info %p]: %s/%s\n", info, info->provider, info->name);
	}
#endif
	current_dsn_list = priv->dsn_list;
	priv->dsn_list = NULL;

	priv->emit_signals = FALSE;
#ifdef HAVE_GIO
	lock_notify_changes ();
#endif
	if (priv->system_file)
		load_config_file (priv->system_file, TRUE);
	if (priv->user_file)
		load_config_file (priv->user_file, FALSE);
#ifdef HAVE_GIO
	unlock_notify_changes ();
#endif
	priv->emit_signals = TRUE;

	new_dsn_list = priv->dsn_list;
	priv->dsn_list = current_dsn_list;
	current_dsn_list = NULL;

	/* handle new or updated DSN */
	GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (list = new_dsn_list; list; list = list->next) {
		GdaDsnInfo *ninfo, *oinfo;
		ninfo = (GdaDsnInfo *) list->data;
		oinfo = gda_config_get_dsn_info (ninfo->name);
		if (!oinfo) {
			/* add ninfo */
			priv->dsn_list = g_slist_insert_sorted (priv->dsn_list, ninfo,
										 (GCompareFunc) data_source_info_compare);
			if (priv->emit_signals)
				g_signal_emit (unique_instance, gda_config_signals[DSN_ADDED], 0, ninfo);
			g_hash_table_insert (hash, ninfo->name, (gpointer) 0x1);
		}
		else {
			/* signal changed if updated */
			if (str_equal (oinfo->provider, ninfo->provider) && 
			    str_equal (oinfo->description, ninfo->description) && 
			    str_equal (oinfo->cnc_string, ninfo->cnc_string) && 
#if !defined (HAVE_GNOME_KEYRING) && !defined (HAVE_LIBSECRET)
			    str_equal (oinfo->auth_string, ninfo->auth_string) && 
#endif
			    (oinfo->is_system == ninfo->is_system)) {
				/* no change for this DSN */
				gda_dsn_info_free (ninfo);
			}
			else {
				GdaDsnInfo tmp;
				tmp = *oinfo;
				*oinfo = *ninfo;
				*ninfo = tmp;
				if (priv->emit_signals)
					g_signal_emit (unique_instance, gda_config_signals[DSN_CHANGED], 0, oinfo);
				gda_dsn_info_free (ninfo);
			}
			g_hash_table_insert (hash, oinfo->name, (gpointer) 0x1);
		}
	}
	g_slist_free (new_dsn_list);

	/* remove old DSN */
	for (list = priv->dsn_list; list; ) {
		GdaDsnInfo *info;
		info = (GdaDsnInfo *) list->data;
		list = list->next;
		if (g_hash_table_lookup (hash, info->name))
			continue;

		if (priv->emit_signals)
			g_signal_emit (unique_instance, gda_config_signals[DSN_TO_BE_REMOVED], 0, info);
		priv->dsn_list = g_slist_remove (priv->dsn_list, info);
		if (priv->emit_signals)
			g_signal_emit (unique_instance, gda_config_signals[DSN_REMOVED], 0, info);
		gda_dsn_info_free (info);
	}
	g_hash_table_destroy (hash);

        GDA_CONFIG_UNLOCK ();
}

/*
 * File monitoring actions
 */
#ifdef HAVE_GIO

static void
conf_file_changed (G_GNUC_UNUSED GFileMonitor *mon, G_GNUC_UNUSED GFile *file,
		   G_GNUC_UNUSED GFile *other_file, GFileMonitorEvent event_type,
		   G_GNUC_UNUSED gpointer data)
{
	if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
		return;
	reload_dsn_configuration ();
}

static void
lock_notify_changes (void)
{
	if (user_notify_changes != 0)
		g_signal_handler_block (mon_conf_user, user_notify_changes);
	if (global_notify_changes != 0)
		g_signal_handler_block (mon_conf_global, global_notify_changes);
}

static void
unlock_notify_changes (void)
{
	if (user_notify_changes != 0)
		g_signal_handler_unblock (mon_conf_user, user_notify_changes);
	if (global_notify_changes != 0)
		g_signal_handler_unblock (mon_conf_global, global_notify_changes);
}

#endif
