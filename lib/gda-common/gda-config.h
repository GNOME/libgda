/* GNOME DB Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_config_h__)
#  define __gda_config_h__

#include <glib.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define GDA_CONFIG_SECTION_DATASOURCES "/gda/Datasources"
#define GDA_CONFIG_SECTION_LOG         "/gda/Log"

/*
 * Configuration system access
 */
gchar*   gda_config_get_string     (const gchar *path);
gint     gda_config_get_int        (const gchar *path);
gdouble  gda_config_get_float      (const gchar *path);
gboolean gda_config_get_boolean    (const gchar *path);
void     gda_config_set_string     (const gchar *path, const gchar *new_value);
void     gda_config_set_int        (const gchar *path, gint new_value);
void     gda_config_set_float      (const gchar *path, gdouble new_value);
void     gda_config_set_boolean    (const gchar *path, gboolean new_value);

void     gda_config_remove_section (const gchar *path);
void     gda_config_remove_key     (const gchar *path);

gboolean gda_config_has_section    (const gchar *path);
void     gda_config_commit         (void);
void     gda_config_rollback       (void);

GList*   gda_config_list_sections  (const gchar *path);
GList*   gda_config_list_keys      (const gchar *path);
void     gda_config_free_list      (GList *list);

/*
 * Providers
 */
typedef struct _Gda_Provider
{
  gchar* name;
  gchar* comment;
  gchar* location;
  gchar* repo_id;
  gchar* type;
  gchar* username;
  gchar* hostname;
  gchar* domain;
  GList* dsn_params;
} Gda_Provider;

#define GDA_PROVIDER_TYPE(srv)       ((srv) ? (srv)->type : 0)
#define GDA_PROVIDER_NAME(srv)       ((srv) ? (srv)->name : 0)
#define GDA_PROVIDER_COMMENT(srv)    ((srv) ? (srv)->comment : 0)
#define GDA_PROVIDER_LOCATION(srv)   ((srv) ? (srv)->location : 0)
#define GDA_PROVIDER_REPO_ID(srv)    ((srv) ? (srv)->repo_id : 0)
#define GDA_PROVIDER_USERNAME(srv)   ((srv) ? (srv)->username : 0)
#define GDA_PROVIDER_HOSTNAME(srv)   ((srv) ? (srv)->hostname : 0)
#define GDA_PROVIDER_DOMAIN(srv)     ((srv) ? (srv)->domain : 0)
#define GDA_PROVIDER_DSN_PARAMS(srv) ((srv) ? (srv)->dsn_params : 0)

Gda_Provider* gda_provider_new          (void);
Gda_Provider* gda_provider_copy         (Gda_Provider*);
void          gda_provider_free         (Gda_Provider*);

GList*        gda_provider_list         (void);
void          gda_provider_free_list    (GList* list);
Gda_Provider* gda_provider_find_by_name (const gchar* provider);

/*
 * Data sources
 */
GList*      gda_list_datasources              (void);
GList*      gda_list_datasources_for_provider (gchar* provider);

#if !defined(GDA_CONFIG_DIR)
#  define GDA_CONFIG_DIR "/usr/local/etc/gnome-db"
#endif

typedef struct _Gda_Dsn
{
  gchar*   gda_name;
  gchar*   provider;
  gchar*   dsn_str;
  gchar*   description;
  gchar*   username;
  gchar*   config;
  gboolean is_global;
} Gda_Dsn;

#define GDA_DSN_GDA_NAME(dsn)    ((dsn) ? (dsn)->gda_name : 0)
#define GDA_DSN_PROVIDER(dsn)    ((dsn) ? (dsn)->provider : 0)
#define GDA_DSN_DSN(dsn)         ((dsn) ? (dsn)->dsn_str : 0)
#define GDA_DSN_DESCRIPTION(dsn) ((dsn) ? (dsn)->description : 0)
#define GDA_DSN_USERNAME(dsn)    ((dsn) ? (dsn)->username : 0)
#define GDA_DSN_CONFIG(dsn)      ((dsn) ? (dsn)->config : 0)
#define GDA_DSN_IS_GLOBAL(dsn)   ((dsn) ? (dsn)->is_global : FALSE)

#define  gda_dsn_new() g_new0(Gda_Dsn, 1)
void     gda_dsn_free            (Gda_Dsn *dsn);

Gda_Dsn* gda_dsn_find_by_name    (const gchar *dsn_name);
void     gda_dsn_set_name        (Gda_Dsn *dsn, const gchar *name);
void     gda_dsn_set_provider    (Gda_Dsn *dsn, const gchar *provider);
void     gda_dsn_set_dsn         (Gda_Dsn *dsn, const gchar *dsn_str);
void     gda_dsn_set_description (Gda_Dsn *dsn, const gchar *description);
void     gda_dsn_set_username    (Gda_Dsn *dsn, const gchar *username);
void     gda_dsn_set_config      (Gda_Dsn *dsn, const gchar *config);
void     gda_dsn_set_global      (Gda_Dsn *dsn, gboolean is_global);

gboolean gda_dsn_save            (Gda_Dsn *dsn);
gboolean gda_dsn_remove          (Gda_Dsn *dsn);

GList*   gda_dsn_list            (void);
void     gda_dsn_free_list       (GList *list);

#if defined(__cplusplus)
}
#endif

#endif
