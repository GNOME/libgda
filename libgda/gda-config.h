/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2003 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2005 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2013, 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_CONFIG_H__
#define __GDA_CONFIG_H__

#include "gda-decl.h"
#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

typedef struct _GdaDsnInfo GdaDsnInfo;

/* error reporting */
extern GQuark gda_config_error_quark (void);
#define GDA_CONFIG_ERROR gda_config_error_quark ()

typedef enum {
	GDA_CONFIG_DSN_NOT_FOUND_ERROR,
	GDA_CONFIG_PERMISSION_ERROR,
	GDA_CONFIG_PROVIDER_NOT_FOUND_ERROR,
	GDA_CONFIG_PROVIDER_CREATION_ERROR
} GdaConfigError;

/**
 * GdaDsnInfo:
 * @name: the (unique) name of the DSN (plain text, not RFC 1738 encoded)
 * @provider: the ID of the database provider to be used (plain text, not RFC 1738 encoded)
 * @description: a descriptive string (plain text, not RFC 1738 encoded), can be %NULL.
 * @cnc_string: the connection string, a semi-colon separated &lt;key>=&lt;value&gt; list where &lt;key&gt; and &lt;value&gt; are RFC 1738 encoded 
 * @auth_string: the authentication string, a semi-colon separated &lt;key>=&lt;value&gt; list where &lt;key&gt; and &lt;value&gt; are RFC 1738 encoded. Can be %NULL.
 * @is_system: %TRUE if the DSN is a system wide defined data source
 *
 * This structure defines the properties of a named data source (DSN).
 */
struct _GdaDsnInfo {
        gchar    *name;
        gchar    *provider;
        gchar    *description;
        gchar    *cnc_string;
        gchar    *auth_string;
        gboolean  is_system;

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
};

#define GDA_TYPE_DSN_INFO (gda_dsn_info_get_type ())

GType            gda_dsn_info_get_type  (void) G_GNUC_CONST;
GdaDsnInfo*      gda_dsn_info_new       (void);
GdaDsnInfo*      gda_dsn_info_copy      (const GdaDsnInfo *source);
void             gda_dsn_info_free      (GdaDsnInfo *dsn);
gboolean         gda_dsn_info_equal     (const GdaDsnInfo *dsn1, const GdaDsnInfo *dsn2);

/**
 * GdaProviderInfo:
 * @id: the unique identifier of the database provider
 * @location: the complete path to the shared library implementing the database provider
 * @description: provider's description
 * @dsn_params: a #GdaSet containing all the parameters which can/must be specified when opening a connection or defining a named data source (DSN)
 * @auth_params: a #GdaSet containing all the authentication parameters
 *
 * This structure holds the information associated to a database provider as discovered by Libgda.
 */
typedef struct {
        gchar             *id;
        gchar             *location;
        gchar             *description;
        GdaSet            *dsn_params;  /* Specs to create a DSN */
	GdaSet            *auth_params; /* Specs to authenticate a client */
	gchar             *icon_id; /* use gdaui_get_icon_for_db_engine(icon_id) to get the actual GdkPixbuf */

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
} GdaProviderInfo;

#define GDA_TYPE_PROVIDER_INFO (gda_provider_info_get_type ())

GType            gda_provider_info_get_type  (void) G_GNUC_CONST;


/**
 * SECTION:gda-config
 * @short_description: Access/Management of libgda configuration
 * @title: Configuration
 * @stability: Stable
 * @see_also:
 *
 * The functions in this section allow applications an easy access to libgda's
 * configuration (the list of data sources and database providers).
 *
 * As soon as a <link linkend="GdaConfig">GdaConfig</link> is needed (for example when requesting information
 * about a data source or about a server provider), a single instance object is created,
 * and no other will need to be created. A pointer to this object can be obtained with
 * <link linkend="gda-config-get">gda_config_get()</link>. Of course one can (right after having called
 * <link linkend="gda-init">gda_init()</link>) force the creation of a GdaConfig object with some
 * specific properties set, using a simple call like:
 * <programlisting>
 *g_object_new (GDA_TYPE_CONFIG, "user-filename", "my_file", NULL);
 * </programlisting>
 * Please note that after that call, the caller has a reference to the newly created object, and should technically
 * call <link linkend="g-object-unref">g_object_unref()</link> when finished using it. It is safe to do this
 * but also pointless since that object should not be destroyed (as no other will be created) as &LIBGDA; also
 * keeps a reference for itself.
 * 
 *Data sources are defined in a per-user configuration file which is by default <filename>${HOME}/.libgda/config</filename> and
 * in a system wide configuration file which is by default <filename>${prefix}/etc/libgda-4.0/config</filename>. Those
 * filenames can be modified by setting the <link linkend="GdaConfig--user-file">user-file</link> and
 * <link linkend="GdaConfig--system-file">system-file</link> properties for the single <link linkend="GdaConfig">GdaConfig</link>
 * instance. Note that setting either of these properties to <literal>NULL</literal> will disable using the corresponding
 * configuration file (DSN will exist only in memory and their definition will be lost when the application finishes).
 *
 * The #GdaConfig object implements its own locking mechanism so it is thread-safe.
 *
 * Note about localization: when the #GdaConfig loads configuration files, it filters the
 * contents based on the current locale, so for example if your current locale is "de" then
 * all the loaded strings (for the ones which are translated) will be in the German language.
 * Changing the locale afterwards will have no effect on the #GdaConfig and the already loaded
 * configuration.
 * The consequence is that you should first call setlocale() youself in your code before using
 * a #GdaConfig object. As a side note you should also call gtk_init() before gdaui_init() because
 * gtk_init() calls setlocale().
 */

G_DECLARE_DERIVABLE_TYPE (GdaConfig, gda_config, GDA, CONFIG, GObject)

#define GDA_TYPE_CONFIG (gda_config_get_type ())

struct _GdaConfigClass {
	GObjectClass      parent_class;

	/* signals */
	void   (*dsn_added)                 (GdaConfig *conf, GdaDsnInfo *new_dsn);
	void   (*dsn_to_be_removed)         (GdaConfig *conf, GdaDsnInfo *old_dsn);
	void   (*dsn_removed)               (GdaConfig *conf, GdaDsnInfo *old_dsn);
	void   (*dsn_changed)               (GdaConfig *conf, GdaDsnInfo *dsn);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GdaConfig*         gda_config_get                      (void);

GdaDsnInfo        *gda_config_get_dsn_info             (const gchar *dsn_name);
gboolean           gda_config_define_dsn               (const GdaDsnInfo *info, GError **error);
gboolean           gda_config_remove_dsn               (const gchar *dsn_name, GError **error);
gboolean           gda_config_dsn_needs_authentication (const gchar *dsn_name);
GdaDataModel      *gda_config_list_dsn                 (void);
gboolean           gda_config_can_modify_system_config (void);

gint               gda_config_get_nb_dsn               (void);
gint               gda_config_get_dsn_info_index       (const gchar *dsn_name);
GdaDsnInfo        *gda_config_get_dsn_info_at_index    (gint index);

GdaProviderInfo   *gda_config_get_provider_info        (const gchar *provider_name);
GdaServerProvider *gda_config_get_provider             (const gchar *provider_name, GError **error);
GdaDataModel      *gda_config_list_providers           (void);

G_END_DECLS

#endif
