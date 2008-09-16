/* GDA library
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
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
#ifdef HAVE_FAM
#include <fam.h>
#include <glib/giochannel.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef G_OS_WIN32 
#include <io.h>
#endif

typedef struct {
	GdaProviderInfo    pinfo;
	GModule           *handle;
	GdaServerProvider *instance;
} InternalProvider;

struct _GdaConfigPrivate {
	gchar *user_file;
	gchar *system_file;
	gboolean system_config_allowed;
	GSList *dsn_list; /* list of GdaDsnInfo structures */
	GSList *prov_list; /* list of InternalProvider structures */
};

static void gda_config_class_init (GdaConfigClass *klass);
static GObject *gda_config_constructor (GType type,
					guint n_construct_properties,
					GObjectConstructParam *construct_properties);
static void gda_config_init       (GdaConfig *conf, GdaConfigClass *klass);
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

static gint data_source_info_compare (GdaDsnInfo *infoa, GdaDsnInfo *infob);
static void data_source_info_free (GdaDsnInfo *info);
static void internal_provider_free (InternalProvider *ip);
static void load_config_file (const gchar *file, gboolean is_system);
static void save_config_file (gboolean is_system);
static void load_all_providers (void);

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

static GObjectClass *parent_class = NULL;

#ifdef HAVE_FAM
/*
 * FAM delcarations and static variables
 */
static FAMConnection *fam_connection = NULL;
static gint           fam_watch_id = 0;
static gboolean       lock_fam = FALSE;
static FAMRequest    *fam_conf_user = NULL;
static FAMRequest    *fam_conf_global = NULL;
static time_t         last_mtime = 0;
static time_t         last_ctime = 0;
static off_t          last_size = 0;

static gboolean       fam_callback (GIOChannel *source, GIOCondition condition, gpointer data);
static void           fam_lock_notify ();
static void           fam_unlock_notify ();
#endif

static GStaticRecMutex gda_mutex = G_STATIC_REC_MUTEX_INIT;
#define GDA_CONFIG_LOCK() g_static_rec_mutex_lock(&gda_mutex)
#define GDA_CONFIG_UNLOCK() g_static_rec_mutex_unlock(&gda_mutex)

/*
 * GdaConfig class implementation
 * @klass:
 */
static void
gda_config_class_init (GdaConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_config_signals[DSN_ADDED] =
                g_signal_new ("dsn-added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_config_signals[DSN_TO_BE_REMOVED] =
                g_signal_new ("dsn-to-be-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_to_be_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_config_signals[DSN_REMOVED] =
                g_signal_new ("dsn-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_config_signals[DSN_CHANGED] =
                g_signal_new ("dsn-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConfigClass, dsn_changed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);

	/* Properties */
        object_class->set_property = gda_config_set_property;
        object_class->get_property = gda_config_get_property;

	/* To translators: DSN stands for Data Source Name, it's a named connection string defined in ~/.libgda/config */
	g_object_class_install_property (object_class, PROP_USER_FILE,
                                         g_param_spec_string ("user-filename", _("File to use for per-user DSN list"), 
							      NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/* To translators: DSN stands for Data Source Name, it's a named connection string defined in $PREFIX/etc/libgda-4.0/config */
	g_object_class_install_property (object_class, PROP_USER_FILE,
                                         g_param_spec_string ("system-filename", _("File to use for system-wide DSN list"), 
							      NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
	object_class->constructor = gda_config_constructor;
	object_class->dispose = gda_config_dispose;
}

static void
gda_config_init (GdaConfig *conf, GdaConfigClass *klass)
{
	g_return_if_fail (GDA_IS_CONFIG (conf));

	conf->priv = g_new0 (GdaConfigPrivate, 1);
	conf->priv->user_file = NULL;
	conf->priv->system_file = NULL;
	conf->priv->system_config_allowed = FALSE;
	conf->priv->prov_list = NULL;
	conf->priv->dsn_list = NULL;
}

static void
load_config_file (const gchar *file, gboolean is_system)
{
	xmlDocPtr doc;
	xmlNodePtr root;

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


			/* signals */
			if (is_new) {
				unique_instance->priv->dsn_list = g_slist_insert_sorted (unique_instance->priv->dsn_list, info,
											 (GCompareFunc) data_source_info_compare);
				g_signal_emit (unique_instance, gda_config_signals[DSN_ADDED], 0, info);
			}
			else
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
	
	if ((!is_system && !unique_instance->priv->user_file) ||
	    (is_system && !unique_instance->priv->system_file)) {
		return;
	}

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewDocNode (doc, NULL, BAD_CAST "libgda-config", NULL);
        xmlDocSetRootElement (doc, root);
	for (list = unique_instance->priv->dsn_list; list; list = list->next) {
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

		/* auth */
		entry = xmlNewChild (section, NULL, BAD_CAST "entry", NULL);
		xmlSetProp (entry, BAD_CAST "name", BAD_CAST "Auth");
		xmlSetProp (entry, BAD_CAST "type", BAD_CAST "string");
		xmlSetProp (entry, BAD_CAST "value", BAD_CAST (info->auth_string));

		/* description */
		entry = xmlNewChild (section, NULL, BAD_CAST "entry", NULL);
		xmlSetProp (entry, BAD_CAST "name", BAD_CAST "Description");
		xmlSetProp (entry, BAD_CAST "type", BAD_CAST "string");
		xmlSetProp (entry, BAD_CAST "value", BAD_CAST (info->description));
	}

#ifdef HAVE_FAM
        fam_lock_notify ();
#endif
	if (!is_system && unique_instance->priv->user_file) {
		if (xmlSaveFormatFile (unique_instance->priv->user_file, doc, TRUE) == -1)
                        g_warning ("Error saving config data to '%s'", unique_instance->priv->user_file);
	}
	else if (is_system && unique_instance->priv->system_file) {
		if (xmlSaveFormatFile (unique_instance->priv->system_file, doc, TRUE) == -1)
                        g_warning ("Error saving config data to '%s'", unique_instance->priv->system_file);
	}
	fflush (NULL);
#ifdef HAVE_FAM
        fam_unlock_notify ();
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
		gint i;
		gboolean user_file_set = FALSE, system_file_set = FALSE;

		object = G_OBJECT_CLASS (parent_class)->constructor (type,
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
		g_object_ref (object); /* keep one reference for the library */

		/* define user and system dirs. if not already defined */
		if (!user_file_set) {
			if (g_get_home_dir ()) {
				gchar *confdir, *conffile;
				gboolean setup_ok = TRUE;
				confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".libgda", NULL);
				conffile = g_build_filename (g_get_home_dir (), ".libgda", "config", NULL);

				if (!g_file_test (confdir, G_FILE_TEST_EXISTS)) {
					if (g_mkdir (confdir, 0700))
						{
							setup_ok = FALSE;
							g_warning (_("Error creating user specific "
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
				else if (!g_file_test (confdir, G_FILE_TEST_IS_DIR)) {
					setup_ok = FALSE;
					g_warning (_("User specific "
						     "configuration directory '%s' exists and is not a directory"), 
						   confdir);
				}
				g_free (confdir);

				if (setup_ok)
					unique_instance->priv->user_file = conffile;
				else
					g_free (conffile);
			}
		}
		if (!system_file_set) 
			unique_instance->priv->system_file = gda_gbr_get_file_path (GDA_ETC_DIR, 
										   LIBGDA_ABI_NAME, "config", NULL);
		unique_instance->priv->system_config_allowed = FALSE;
		if (unique_instance->priv->system_file) {
			FILE *file;
                        file = fopen (unique_instance->priv->system_file, "a");
                        if (file) {
                                unique_instance->priv->system_config_allowed = TRUE;
                                fclose (file);
                        }
		}

		/* Setup file monitoring */
#ifdef HAVE_FAM
		if (!fam_connection) {
			/* FAM init */
			GIOChannel *ioc;
			int res;
			
#ifdef GDA_DEBUG_NO
			g_print ("Using FAM to monitor configuration files changes.\n");
#endif
			fam_connection = g_malloc0 (sizeof (FAMConnection));
			if (FAMOpen2 (fam_connection, "libgnomedb user") != 0) {
				g_print ("FAMOpen failed, FAMErrno=%d\n", FAMErrno);
				g_free (fam_connection);
				fam_connection = NULL;
			}
			else {
				ioc = g_io_channel_unix_new (FAMCONNECTION_GETFD(fam_connection));
				fam_watch_id = g_io_add_watch (ioc,
							       G_IO_IN | G_IO_HUP | G_IO_ERR,
							       fam_callback, NULL);
				
				if (unique_instance->priv->system_file) {
					fam_conf_global = g_new0 (FAMRequest, 1);
					res = FAMMonitorFile (fam_connection, unique_instance->priv->system_file, 
							      fam_conf_global,
							      GINT_TO_POINTER (TRUE));
#ifdef GDA_DEBUG_NO
					g_print ("Monitoring changes on file %s: %s\n", 
						 unique_instance->priv->system_file, 
						 res ? "ERROR" : "Ok");
#endif
				}
				
				if (unique_instance->priv->user_file) {
					fam_conf_user = g_new0 (FAMRequest, 1);
					res = FAMMonitorFile (fam_connection, unique_instance->priv->user_file, 
							      fam_conf_user,
							      GINT_TO_POINTER (FALSE));
#ifdef GDA_DEBUG_NO
					g_print ("Monitoring changes on file %s: %s\n",
						 unique_instance->priv->user_file, 
						 res ? "ERROR" : "Ok");
#endif
				}
			}
			
		}
#endif
		/* load existing DSN definitions */
		if (unique_instance->priv->system_file)
			load_config_file (unique_instance->priv->system_file, TRUE);
		if (unique_instance->priv->user_file)
			load_config_file (unique_instance->priv->user_file, FALSE);
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

	if (conf->priv) {
		g_free (conf->priv->user_file);
		g_free (conf->priv->system_file);

		if (conf->priv->dsn_list) {
			g_slist_foreach (conf->priv->dsn_list, (GFunc) data_source_info_free, NULL);
			g_slist_free (conf->priv->dsn_list);
		}
		if (conf->priv->prov_list) {
			g_slist_foreach (conf->priv->prov_list, (GFunc) internal_provider_free, NULL);
			g_slist_free (conf->priv->prov_list);
		}
		g_free (conf->priv);
		conf->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}


/* module error */
GQuark gda_config_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_config_error");
        return quark;
}

/**
 * gda_config_get_type
 * 
 * Registers the #GdaConfig class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 */
GType
gda_config_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GTypeInfo info = {
			sizeof (GdaConfigClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_config_class_init,
			NULL, NULL,
			sizeof (GdaConfig),
			0,
			(GInstanceInitFunc) gda_config_init
		};
		GDA_CONFIG_LOCK ();
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaConfig", &info, 0);
		GDA_CONFIG_UNLOCK ();
	}

	return type;
}

static void
gda_config_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdaConfig *conf;

        conf = GDA_CONFIG (object);
        if (conf->priv) {
                switch (param_id) {
		case PROP_USER_FILE:
			g_free (conf->priv->user_file);
			conf->priv->user_file = NULL;
			if (g_value_get_string (value))
				conf->priv->user_file = g_strdup (g_value_get_string (value));
			break;
                case PROP_SYSTEM_FILE:
			g_free (conf->priv->system_file);
			conf->priv->system_file = NULL;
			if (g_value_get_string (value))
				conf->priv->system_file = g_strdup (g_value_get_string (value));
                        break;
		}	
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
	if (conf->priv) {
		switch (param_id) {
		case PROP_USER_FILE:
			g_value_set_string (value, conf->priv->user_file);
			break;
		case PROP_SYSTEM_FILE:
			g_value_set_string (value, conf->priv->system_file);
			break;
		}
	}	
}

/**
 * gda_config_get
 * 
 * Get a pointer to the global GdaConfig object
 *
 * Returns: a non %NULL pointer to a #GdaConfig
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
 * gda_config_get_dsn_info
 * @dsn_name: the name of the DSN to look for
 *
 * Get information about the DSN named @dsn_name. 
 *
 * @dsn_name's format is "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" (if &lt;username&gt;
 * and optionaly &lt;password&gt; are provided, they are ignored). Also see the gda_dsn_split() utility
 * function.
 *
 * Returns: a a pointer to read-only #GdaDsnInfo structure, or %NULL if not found
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
		g_warning (_("Malformed data source name '%s'"), dsn_name);
                return NULL;
	}

	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();

	for (list = unique_instance->priv->dsn_list; list; list = list->next)
		if (!strcmp (((GdaDsnInfo*) list->data)->name, real_dsn)) {
			GDA_CONFIG_UNLOCK ();
			g_free (real_dsn);
			return (GdaDsnInfo*) list->data;
		}
	GDA_CONFIG_UNLOCK ();
	g_free (real_dsn);
	return NULL;
}

/**
 * gda_config_define_dsn
 * @info: a pointer to a filled GdaDsnInfo structure
 * @error: a place to store errors, or %NULL
 *
 * Add or update a DSN from the definition in @info
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

	if (info->is_system && !unique_instance->priv->system_config_allowed) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PERMISSION_ERROR,
			     _("Can't manage system-wide configuration"));
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
			einfo->is_system = info->is_system;
		}
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
		einfo->is_system = info->is_system;

		unique_instance->priv->dsn_list = g_slist_insert_sorted (unique_instance->priv->dsn_list, einfo,
									 (GCompareFunc) data_source_info_compare);
		g_signal_emit (unique_instance, gda_config_signals[DSN_ADDED], 0, einfo);
	}
	
	if (save_system)
		save_config_file (TRUE);
	if (save_user)
		save_config_file (FALSE);

	GDA_CONFIG_UNLOCK ();
	return TRUE;
}

/**
 * gda_config_remove_dsn
 * @dsn_name: the name of the DSN to remove
 * @error: a place to store errors, or %NULL
 *
 * Add or update a DSN from the definition in @info
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
	if (info->is_system && !unique_instance->priv->system_config_allowed) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PERMISSION_ERROR,
			     _("Can't manage system-wide configuration"));
		GDA_CONFIG_UNLOCK ();
		return FALSE;
	}

	if (info->is_system)
		save_system = TRUE;
	else
		save_user = TRUE;

	g_signal_emit (unique_instance, gda_config_signals[DSN_TO_BE_REMOVED], 0, info);
	unique_instance->priv->dsn_list = g_slist_remove (unique_instance->priv->dsn_list, info);
	g_signal_emit (unique_instance, gda_config_signals[DSN_REMOVED], 0, info);
	data_source_info_free (info);
	
	if (save_system)
		save_config_file (TRUE);
	if (save_user)
		save_config_file (FALSE);

	GDA_CONFIG_UNLOCK ();
	return TRUE;
}

/**
 * gda_config_dsn_needs_auth
 * @dsn_name: the name of a DSN, in the "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" format
 * 
 * Tells if the data source identified as @dsn_name needs any authentication. If a &lt;username&gt;
 * and optionaly a &lt;password&gt; are specified, they are ignored.
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
		g_warning (_("Provider '%s' not found"), info->provider);
		return FALSE;
	}
	if (pinfo->auth_params && pinfo->auth_params->holders)
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_config_list_dsn
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
 * Returns: a new #GdaDataModel
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
 * gda_config_get_nb_dsn
 *
 * Get the number of defined DSN
 *
 * Return: the number of defined DSN
 */
gint
gda_config_get_nb_dsn (void)
{
	gint ret;
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();

	ret = g_slist_length (unique_instance->priv->dsn_list);
	GDA_CONFIG_UNLOCK ();
	return ret;
}

/**
 * gda_config_get_dsn_info_index
 * @dsn_name:
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

	info = gda_config_get_dsn_info (dsn_name);
	if (info)
		ret = g_slist_index (unique_instance->priv->dsn_list, info);

	GDA_CONFIG_UNLOCK ();
	return ret;
}

/**
 * gda_config_get_dsn_info_at_index
 * @index:
 *
 * Get a pointer to a read-only #GdaDsnInfo at the @index position
 *
 * Returns: the pointer or %NULL if no DSN exists at position @index
 */
GdaDsnInfo *
gda_config_get_dsn_info_at_index (gint index)
{
	GdaDsnInfo *ret;
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();
	ret = g_slist_nth_data (unique_instance->priv->dsn_list, index);
	GDA_CONFIG_UNLOCK ();
	return ret;
}

/**
 * gda_config_can_modify_system_config
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
	retval = unique_instance->priv->system_config_allowed;
	GDA_CONFIG_UNLOCK ();
	return retval;
}

/**
 * gda_config_get_provider_info
 * @provider_name:
 *
 * Get some information about the a database provider (adaptator) named 
 * @provider_name
 *
 * Returns: a pointer to read-only #GdaProviderInfo structure, or %NULL if not found
 */
GdaProviderInfo *
gda_config_get_provider_info (const gchar *provider_name)
{
	GSList *list;
	g_return_val_if_fail (provider_name, NULL);
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();

	if (!unique_instance->priv->prov_list) 
		load_all_providers ();

	if (!g_ascii_strcasecmp (provider_name, "MS Access")) {
		GDA_CONFIG_UNLOCK ();
		return gda_config_get_provider_info ("MSAccess");
	}

	for (list = unique_instance->priv->prov_list; list; list = list->next)
		if (!g_ascii_strcasecmp (((GdaProviderInfo*) list->data)->id, provider_name)) {
			GDA_CONFIG_UNLOCK ();
			return (GdaProviderInfo*) list->data;
		}
	GDA_CONFIG_UNLOCK ();
	return NULL;
}

/**
 * gda_config_get_provider
 * @provider_name:
 * @error: a place to store errors, or %NULL
 *
 * Get a pointer to the session-wide #GdaServerProvider for the
 * provider named @provider_name. The caller must not call g_object_unref() on the
 * returned object.
 *
 * Returns: a pointer to the #GdaServerProvider, or %NULL if an error occurred
 */
GdaServerProvider *
gda_config_get_provider (const gchar *provider_name, GError **error)
{
	InternalProvider *ip;
	GdaServerProvider  *(*plugin_create_provider) (void);

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
	g_assert (ip->handle);
	g_module_symbol (ip->handle, "plugin_create_provider", (gpointer) &plugin_create_provider);
	if (!plugin_create_provider) {
		g_set_error (error, GDA_CONFIG_ERROR, GDA_CONFIG_PROVIDER_CREATION_ERROR,
			     _("Can't instantiate provider '%s'"), provider_name);
		GDA_CONFIG_UNLOCK ();
		return NULL;
	}
	ip->instance = plugin_create_provider ();
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
 * gda_config_list_providers
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
 * Returns: a new #GdaDataModel
 */
GdaDataModel *
gda_config_list_providers (void)
{
	GSList *list;
	GdaDataModel *model;
	
	GDA_CONFIG_LOCK ();
	if (!unique_instance)
		gda_config_get ();

	if (!unique_instance->priv->prov_list) 
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

	for (list = unique_instance->priv->prov_list; list; list = list->next) {
		GdaProviderInfo *info = (GdaProviderInfo *) list->data;
		GValue *value;
		gint row;

		row = gda_data_model_append_row (model, NULL);

		value = gda_value_new_from_string (info->id, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 0, row, value, NULL);
		gda_value_free (value);

		value = gda_value_new_from_string (info->description, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 1, row, value, NULL);
		gda_value_free (value);

		if (info->dsn_params) {
			GSList *params;
			GString *string = g_string_new ("");
			for (params = info->dsn_params->holders;
			     params; params = params->next) {
				const gchar *id;

				id = gda_holder_get_id (GDA_HOLDER (params->data));
				if (params != info->dsn_params->holders)
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
			for (params = info->auth_params->holders;
			     params; params = params->next) {
				const gchar *id;

				id = gda_holder_get_id (GDA_HOLDER (params->data));
				if (params != info->auth_params->holders)
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
 
static void load_providers_from_dir (const gchar *dirname, gboolean recurs);
static void
load_all_providers (void)
{
	const gchar *dirname;
	g_assert (unique_instance);

	dirname = g_getenv ("GDA_TOP_BUILD_DIR");
	if (dirname) {
		gchar *pdir;
		pdir = g_build_path (G_DIR_SEPARATOR_S, dirname, "providers", NULL);
		load_providers_from_dir (pdir, TRUE);
	}
	else {
		gchar *str;
		str = gda_gbr_get_file_path (GDA_LIB_DIR, LIBGDA_ABI_NAME, "providers", NULL);
		load_providers_from_dir (str, FALSE);
		g_free (str);
	}
}

static void 
load_providers_from_dir (const gchar *dirname, gboolean recurs)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *name;

	/* read the plugin directory */
#ifdef GDA_DEBUG_NO
	g_print ("Loading providers in %s\n", dirname);
#endif
	dir = g_dir_open (dirname, 0, &err);
	if (err) {
		gda_log_error (err->message);
		g_error_free (err);
		return;
	}

	while ((name = g_dir_read_name (dir))) {
		InternalProvider *ip;
		GdaProviderInfo *info;
		GModule *handle;
		gchar *path;
		void (*plugin_init) (const gchar *);
		const gchar * (* plugin_get_name) (void);
		const gchar * (* plugin_get_description) (void);
		gchar * (* plugin_get_dsn_spec) (void);
		gchar * (* plugin_get_auth_spec) (void);

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

		path = g_build_path (G_DIR_SEPARATOR_S, dirname,
				     name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY);
		if (!handle) {
			g_warning (_("Error: %s"), g_module_error ());
			g_free (path);
			continue;
		}

		if (g_module_symbol (handle, "plugin_init", (gpointer *) &plugin_init))
			plugin_init (dirname);
		g_module_symbol (handle, "plugin_get_name", (gpointer *) &plugin_get_name);
		g_module_symbol (handle, "plugin_get_description", (gpointer *) &plugin_get_description);
		g_module_symbol (handle, "plugin_get_dsn_spec", (gpointer *) &plugin_get_dsn_spec);
		g_module_symbol (handle, "plugin_get_auth_spec", (gpointer *) &plugin_get_auth_spec);

		ip = g_new0 (InternalProvider, 1);
		ip->handle = handle;
		info = (GdaProviderInfo*) ip;
		info->location = path;

		if (plugin_get_name != NULL)
			info->id = g_strdup (plugin_get_name ());
		else
			info->id = g_strdup (name);

		if (plugin_get_description != NULL)
			info->description = g_strdup (plugin_get_description ());
		else
			info->description = NULL;

		/* DSN parameters */
		info->dsn_params = NULL;
		if (plugin_get_dsn_spec) {
			GError *error = NULL;
			gchar *dsn_spec;

			dsn_spec = plugin_get_dsn_spec ();
			if (dsn_spec) {
				info->dsn_params = gda_set_new_from_spec_string (dsn_spec, &error);
				if (!info->dsn_params) {
					g_warning ("Invalid format for provider '%s' DSN spec : %s",
						   info->id,
						   error ? error->message : "Unknown error");
					if (error)
						g_error_free (error);
				}
				g_free (dsn_spec);
			}
			if (!info->dsn_params) {
				/* there may be traces of the provider installed but some parts are missing,
				   forget about that provider... */
				internal_provider_free (ip);
				ip = NULL;
				continue;
			}
		}
		else
			g_warning ("Provider '%s' does not provide a DSN spec", info->id);

		/* Authentication parameters */
		info->auth_params = NULL;
		if (plugin_get_auth_spec) {
			GError *error = NULL;
			gchar *auth_spec;

			auth_spec = plugin_get_auth_spec ();
			if (auth_spec) {
				info->auth_params = gda_set_new_from_spec_string (auth_spec, &error);
				if (!info->auth_params) {
					g_warning ("Invalid format for provider '%s' AUTH spec : %s",
						   info->id,
						   error ? error->message : "Unknown error");
					if (error)
						g_error_free (error);
				}
				g_free (auth_spec);
			}
			if (!info->auth_params) {
				/* there may be traces of the provider installed but some parts are missing,
				   forget about that provider... */
				internal_provider_free (ip);
				ip = NULL;
				continue;
			}
		}
		else {
			/* default to username/password */
			GdaHolder *h;
			info->auth_params = gda_set_new_inline (2, "USERNAME", G_TYPE_STRING, NULL,
								"PASSWORD", G_TYPE_STRING, NULL);
			h = gda_set_get_holder (info->auth_params, "USERNAME");
			g_object_set (G_OBJECT (h), "name", _("Username"), NULL);
			h = gda_set_get_holder (info->auth_params, "PASSWORD");
			g_object_set (G_OBJECT (h), "name", _("Password"), NULL);
		}

		if (ip) {
			unique_instance->priv->prov_list = g_slist_prepend (unique_instance->priv->prov_list, ip);
#ifdef GDA_DEBUG_NO
			g_print ("Loaded '%s' provider\n", info->id);
#endif
		}
	}

	/* free memory */
	g_dir_close (dir);
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
data_source_info_free (GdaDsnInfo *info)
{
	g_free (info->provider); 
	g_free (info->cnc_string); 
	g_free (info->description);
	g_free (info->auth_string);
	g_free (info);
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

/*
 * File monitoring actions
 */
#ifdef HAVE_FAM
static gboolean
fam_callback (GIOChannel *source, GIOCondition condition, gpointer data)
{
        gboolean res = TRUE;

	if (!unique_instance)
		return TRUE;

        GDA_CONFIG_LOCK ();
        while (fam_connection && FAMPending (fam_connection)) {
                FAMEvent ev;
                gboolean is_system;

                if (FAMNextEvent (fam_connection, &ev) != 1) {
                        FAMClose (fam_connection);
                        g_free (fam_connection);
                        g_source_remove (fam_watch_id);
                        fam_watch_id = 0;
                        fam_connection = NULL;
                        GDA_CONFIG_UNLOCK ();
                        return FALSE;
                }

                if (lock_fam)
                        continue;

                is_system = GPOINTER_TO_INT (ev.userdata);
                switch (ev.code) {
                case FAMChanged: {
                        struct stat stat;
                        if (lstat (ev.filename, &stat))
                                break;
                        if ((stat.st_mtime != last_mtime) ||
                            (stat.st_ctime != last_ctime) ||
                            (stat.st_size != last_size)) {
                                last_mtime = stat.st_mtime;
                                last_ctime = stat.st_ctime;
                                last_size = stat.st_size;
                        }
                        else
                                break;
                }
                case FAMDeleted:
                case FAMCreated:
#ifdef GDA_DEBUG_NO
                        g_print ("Reloading config files (%s config has changed)\n", is_system ? "global" : "user");
			GSList *list;
			for (list = unique_instance->priv->dsn_list; list; list = list->next) {
				GdaDsnInfo *info = (GdaDsnInfo *) list->data;
				g_print ("[info %p]: %s/%s\n", info, info->provider, info->name);
			}
#endif
			while (unique_instance->priv->dsn_list) {
				GdaDsnInfo *info = (GdaDsnInfo *) unique_instance->priv->dsn_list->data;
				g_signal_emit (unique_instance, gda_config_signals[DSN_TO_BE_REMOVED], 0, info);
				unique_instance->priv->dsn_list = g_slist_remove (unique_instance->priv->dsn_list, info);
				g_signal_emit (unique_instance, gda_config_signals[DSN_REMOVED], 0, info);
				data_source_info_free (info);
			}
			
			if (unique_instance->priv->system_file)
				load_config_file (unique_instance->priv->system_file, TRUE);
			if (unique_instance->priv->user_file)
				load_config_file (unique_instance->priv->user_file, FALSE);
                        break;
                case FAMAcknowledge:
                case FAMStartExecuting:
                case FAMStopExecuting:
                case FAMExists:
                case FAMEndExist:
                case FAMMoved:
                        /* Not supported */
                        break;
                }
        }

        GDA_CONFIG_UNLOCK ();
        return res;
}

static void
fam_lock_notify ()
{
        lock_fam = TRUE;
}

static void
fam_unlock_notify ()
{
        lock_fam = FALSE;
}

#endif
