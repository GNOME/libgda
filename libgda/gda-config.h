/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_config_h__)
#  define __gda_config_h__

#include <glib/gmacros.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-parameter.h>

G_BEGIN_DECLS

/*
 * Basic configuration access
 */

gchar   *gda_config_get_string (const gchar *path);
gint     gda_config_get_int (const gchar *path);
gdouble  gda_config_get_float (const gchar *path);
gboolean gda_config_get_boolean (const gchar *path);
void     gda_config_set_string (const gchar *path, const gchar *new_value);
void     gda_config_set_int (const gchar *path, gint new_value);
void     gda_config_set_float (const gchar *path, gdouble new_value);
void     gda_config_set_boolean (const gchar *path, gboolean new_value);

void     gda_config_remove_section (const gchar *path);
void     gda_config_remove_key (const gchar *path);
gboolean gda_config_has_section (const gchar *path);
gboolean gda_config_has_key (const gchar *path);
GList   *gda_config_list_sections (const gchar *path);
GList   *gda_config_list_keys (const gchar *path);
gchar   *gda_config_get_type (const gchar *path);

void     gda_config_free_list (GList *list);

typedef void (* GdaConfigListenerFunc) (const gchar *path, gpointer user_data);

guint    gda_config_add_listener (const gchar *path,
				  GdaConfigListenerFunc func,
				  gpointer user_data);
void     gda_config_remove_listener (guint id);

/*
 * Providers configuration
 */

typedef struct _GdaProviderInfo GdaProviderInfo;

typedef struct {
	char *name;
	char *short_description;
	char *long_description;
	GdaValueType type;
} GdaProviderParameterInfo;

#define GDA_TYPE_PROVIDER_PARAMETER_INFO (gda_provider_parameter_info_get_type ())

GType                     gda_provider_parameter_info_get_type (void);
GdaProviderParameterInfo *gda_provider_parameter_info_new_full (const gchar *name,
								const gchar *short_description,
								const gchar *long_description,
								GdaValueType type);
GdaProviderParameterInfo *gda_provider_parameter_info_copy (GdaProviderParameterInfo *param_info);
void                      gda_provider_parameter_info_free (GdaProviderParameterInfo *param_info);

struct _GdaProviderInfo {
	gchar *id;
	gchar *location;
	gchar *description;
	GList *gda_params; /* A list of GdaProviderParameterInfo pointers */
};

#define GDA_TYPE_PROVIDER_INFO (gda_provider_info_get_type ())

GType            gda_provider_info_get_type (void);
GdaProviderInfo* gda_provider_info_copy (GdaProviderInfo *src);
void             gda_provider_info_free (GdaProviderInfo *provider_info);

GList           *gda_config_get_provider_list (void);
void             gda_config_free_provider_list (GList *list);
GdaProviderInfo *gda_config_get_provider_by_name (const gchar *name);
GdaDataModel    *gda_config_get_provider_model (void);

/*
 * Data sources configuration
 */

typedef struct _GdaDataSourceInfo GdaDataSourceInfo;
 
struct _GdaDataSourceInfo {
	gchar *name;
	gchar *provider;
	gchar *cnc_string;
	gchar *description;
	gchar *username;
	gchar *password;
};

#define GDA_TYPE_DATA_SOURCE_INFO (gda_data_source_info_get_type ())

GType              gda_data_source_info_get_type (void);
GdaDataSourceInfo *gda_config_copy_data_source_info (GdaDataSourceInfo *src);  /* TODO: Rename to gda_data_source_info_copy */
void               gda_config_free_data_source_info (GdaDataSourceInfo *info); /* TODO: Rename to gda_data_source_info_free */

GList             *gda_config_get_data_source_list (void);
GdaDataSourceInfo *gda_config_find_data_source (const gchar *name);

void               gda_config_free_data_source_list (GList *list);

GdaDataModel      *gda_config_get_data_source_model (void);
void               gda_config_save_data_source (const gchar *name,
						const gchar *provider,
						const gchar *cnc_string,
						const gchar *description,
						const gchar *username,
						const gchar *password);
void               gda_config_save_data_source_info (GdaDataSourceInfo *dsn_info);
void               gda_config_remove_data_source (const gchar *name);

/*
 * GDA configuration sections/keys
 */
#define GDA_CONFIG_BASE			     "/apps/libgda"
#define GDA_CONFIG_SECTION_DATASOURCES       "/apps/libgda/Datasources"
#define GDA_CONFIG_SECTION_LAST_CONNECTIONS  "/apps/libgda/LastConnections"

#define GDA_CONFIG_KEY_MAX_LAST_CONNECTIONS  "/apps/libgda/MaxLastConnections"

G_END_DECLS

#endif
