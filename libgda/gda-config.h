/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2003 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2005 Andrew Hill <andru@src.gnome.org>
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

#define GDA_TYPE_CONFIG            (gda_config_get_type())
#define GDA_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONFIG, GdaConfig))
#define GDA_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONFIG, GdaConfigClass))
#define GDA_IS_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CONFIG))
#define GDA_IS_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONFIG))

typedef struct _GdaConfigPrivate GdaConfigPrivate;
typedef struct _GdaDsnInfo GdaDsnInfo;
typedef struct _GdaProviderInfo GdaProviderInfo;

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
struct _GdaProviderInfo {
        gchar             *id;
        gchar             *location;
        gchar             *description;
        GdaSet            *dsn_params;  /* Specs to create a DSN */
	GdaSet            *auth_params; /* Specs to authenticate a client */

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
};

struct _GdaConfig {
	GObject           object;
	GdaConfigPrivate *priv;
};

struct _GdaConfigClass {
	GObjectClass      object_class;

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

GType              gda_config_get_type                 (void) G_GNUC_CONST;
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
