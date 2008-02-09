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
typedef struct _GdaDataSourceInfo GdaDataSourceInfo;
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

struct _GdaDataSourceInfo {
        gchar    *name;
        gchar    *provider;
        gchar    *cnc_string;
        gchar    *description;
        gchar    *username;
        gchar    *password;
        gboolean  is_system;
};

struct _GdaProviderInfo {
        gchar             *id;
        gchar             *location;
        gchar             *description;
        GdaSet            *gda_params; /* Specs to create a DSN */
        gchar             *dsn_spec; /* XML string with all the parameters required to create a DSN */
};

struct _GdaConfig {
	GObject           object;
	GdaConfigPrivate *priv;
};

struct _GdaConfigClass {
	GObjectClass      object_class;

	/* signals */
	void   (*dsn_added)                 (GdaConfig *conf, GdaDataSourceInfo *new_dsn);
	void   (*dsn_to_be_removed)         (GdaConfig *conf, GdaDataSourceInfo *old_dsn);
	void   (*dsn_removed)               (GdaConfig *conf, GdaDataSourceInfo *old_dsn);
	void   (*dsn_changed)               (GdaConfig *conf, GdaDataSourceInfo *dsn);
};

GType              gda_config_get_type            (void) G_GNUC_CONST;
GdaConfig*         gda_config_get                 (void);

GdaDataSourceInfo *gda_config_get_dsn             (const gchar *dsn_name);
gboolean           gda_config_define_dsn          (const GdaDataSourceInfo *info, GError **error);
gboolean           gda_config_remove_dsn          (const gchar *dsn_name, GError **error);
GdaDataModel      *gda_config_list_dsn            (void);
gboolean           gda_config_can_modify_system_config (void);

gint               gda_config_get_nb_dsn          (void);
gint               gda_config_get_dsn_index       (const gchar *dsn_name);
GdaDataSourceInfo *gda_config_get_dsn_at_index    (gint index);

GdaProviderInfo   *gda_config_get_provider_info   (const gchar *provider_name);
GdaServerProvider *gda_config_get_provider_object (const gchar *provider_name, GError **error);
GdaDataModel      *gda_config_list_providers      (void);

G_END_DECLS

#endif
