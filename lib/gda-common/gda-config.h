/* GNOME DB Common Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include <glib.h>
#include <gda-common-defs.h>

G_BEGIN_DECLS

#define GDA_CONFIG_SECTION_DATASOURCES       "/apps/gda/Datasources"
#define GDA_CONFIG_SECTION_LAST_CONNECTIONS  "/apps/gda/LastConnections"
#define GDA_CONFIG_SECTION_LOG               "/apps/gda/Log"

#define GDA_CONFIG_KEY_MAX_LAST_CONNECTIONS  "/apps/gda/MaxLastConnections"

/*
 * Configuration system access
 */
gchar *gda_config_get_string (const gchar * path);
gint gda_config_get_int (const gchar * path);
gdouble gda_config_get_float (const gchar * path);
gboolean gda_config_get_boolean (const gchar * path);
void gda_config_set_string (const gchar * path,
			    const gchar * new_value);
void gda_config_set_int (const gchar * path, gint new_value);
void gda_config_set_float (const gchar * path, gdouble new_value);
void gda_config_set_boolean (const gchar * path, gboolean new_value);

void gda_config_remove_section (const gchar * path);
void gda_config_remove_key (const gchar * path);

gboolean gda_config_has_section (const gchar * path);
gboolean gda_config_has_key (const gchar * path);
void gda_config_commit (void);
void gda_config_rollback (void);

GList *gda_config_list_sections (const gchar * path);
GList *gda_config_list_keys (const gchar * path);
void gda_config_free_list (GList * list);

/*
 * Providers
 */
typedef struct _GdaProvider {
	gchar *name;
	gchar *comment;
	gchar *location;
	gchar *repo_id;
	gchar *type;
	gchar *username;
	gchar *hostname;
	gchar *domain;
	GList *dsn_params;
} GdaProvider;

#define GDA_PROVIDER_TYPE(srv)       ((srv) ? (srv)->type : NULL)
#define GDA_PROVIDER_NAME(srv)       ((srv) ? (srv)->name : NULL)
#define GDA_PROVIDER_COMMENT(srv)    ((srv) ? (srv)->comment : NULL)
#define GDA_PROVIDER_LOCATION(srv)   ((srv) ? (srv)->location : NULL)
#define GDA_PROVIDER_REPO_ID(srv)    ((srv) ? (srv)->repo_id : NULL)
#define GDA_PROVIDER_USERNAME(srv)   ((srv) ? (srv)->username : NULL)
#define GDA_PROVIDER_HOSTNAME(srv)   ((srv) ? (srv)->hostname : NULL)
#define GDA_PROVIDER_DOMAIN(srv)     ((srv) ? (srv)->domain : NULL)
#define GDA_PROVIDER_DSN_PARAMS(srv) ((srv) ? (srv)->dsn_params : NULL)

GdaProvider *gda_provider_new (void);
GdaProvider *gda_provider_copy (GdaProvider * provider);
void gda_provider_free (GdaProvider * provider);

GList *gda_provider_list (void);
void gda_provider_free_list (GList * list);
GdaProvider *gda_provider_find_by_name (const gchar * name);

/*
 * Data sources
 */
GList *gda_list_datasources (void);
GList *gda_list_datasources_for_provider (gchar * name);

typedef struct _GdaDsn {
	gchar *gda_name;
	gchar *provider;
	gchar *dsn_str;
	gchar *description;
	gchar *username;
	gchar *config;
	gboolean is_global;
} GdaDsn;

#define GDA_DSN_GDA_NAME(dsn)    ((dsn) ? (dsn)->gda_name : 0)
#define GDA_DSN_PROVIDER(dsn)    ((dsn) ? (dsn)->provider : 0)
#define GDA_DSN_DSN(dsn)         ((dsn) ? (dsn)->dsn_str : 0)
#define GDA_DSN_DESCRIPTION(dsn) ((dsn) ? (dsn)->description : 0)
#define GDA_DSN_USERNAME(dsn)    ((dsn) ? (dsn)->username : 0)
#define GDA_DSN_CONFIG(dsn)      ((dsn) ? (dsn)->config : 0)
#define GDA_DSN_IS_GLOBAL(dsn)   ((dsn) ? (dsn)->is_global : FALSE)

#define  gda_dsn_new() g_new0(GdaDsn, 1)
void gda_dsn_free (GdaDsn * dsn);
GdaDsn *gda_dsn_copy (GdaDsn * dsn);

GdaDsn *gda_dsn_find_by_name (const gchar * dsn_name);
void gda_dsn_set_name (GdaDsn * dsn, const gchar * name);
void gda_dsn_set_provider (GdaDsn * dsn, const gchar * provider);
void gda_dsn_set_dsn (GdaDsn * dsn, const gchar * dsn_str);
void gda_dsn_set_description (GdaDsn * dsn,
			      const gchar * description);
void gda_dsn_set_username (GdaDsn * dsn, const gchar * username);
void gda_dsn_set_config (GdaDsn * dsn, const gchar * config);
void gda_dsn_set_global (GdaDsn * dsn, gboolean is_global);

gboolean gda_dsn_save (GdaDsn * dsn);
gboolean gda_dsn_remove (GdaDsn * dsn);

GList *gda_dsn_list (void);
void gda_dsn_free_list (GList * list);

/*
 * Private functions
 */
void gda_config_save_last_connection (const gchar * gda_name,
				      const gchar * username);

G_END_DECLS

#endif
