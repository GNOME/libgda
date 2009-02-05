/* GDA library
 * Copyright (C) 2007 - 2009 The GNOME Foundation.
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

struct _GdaDsnInfo {
        gchar    *name;        /* plain text, not RFC 1738 encoded */
        gchar    *provider;    /* plain text, not RFC 1738 encoded */
        gchar    *description; /* plain text, not RFC 1738 encoded */
        gchar    *cnc_string;  /* semi-colon separated <key>=<value> list where <key> and <value> are RFC 1738 encoded */
        gchar    *auth_string; /* semi-colon separated <key>=<value> list where <key> and <value> are RFC 1738 encoded */
        gboolean  is_system;

	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
};

struct _GdaProviderInfo {
        gchar             *id;
        gchar             *location;
        gchar             *description;
        GdaSet            *dsn_params;  /* Specs to create a DSN */
	GdaSet            *auth_params; /* Specs to authenticate a client */

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
