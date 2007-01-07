/* GDA client library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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

#ifndef __GDA_CONNECTION_H__
#define __GDA_CONNECTION_H__

#include "gda-decl.h"
#include <libgda/gda-object.h>
#include <libgda/gda-command.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-index.h>
#include <libgda/gda-connection-event.h>
#include <libgda/gda-parameter.h>
#include <libgda/gda-transaction-status.h>

G_BEGIN_DECLS

#define GDA_TYPE_CONNECTION            (gda_connection_get_type())
#define GDA_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION, GdaConnection))
#define GDA_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONNECTION, GdaConnectionClass))
#define GDA_IS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CONNECTION))
#define GDA_IS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION))

/* error reporting */
extern GQuark gda_connection_error_quark (void);
#define GDA_CONNECTION_ERROR gda_connection_error_quark ()

typedef enum {
        GDA_CONNECTION_CONN_OPEN_ERROR,
        GDA_CONNECTION_DO_QUERY_ERROR,
	GDA_CONNECTION_NONEXIST_DSN_ERROR,
	GDA_CONNECTION_NO_CNC_SPEC_ERROR,
	GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
	GDA_CONNECTION_OPEN_ERROR,
	GDA_CONNECTION_EXECUTE_COMMAND_ERROR
} GdaConnectionError;

struct _GdaConnection {
	GObject               object;
	GdaConnectionPrivate *priv;
};

struct _GdaConnectionClass {
	GObjectClass          object_class;

	/* signals */
	void   (*error)                     (GdaConnection *cnc, GdaConnectionEvent *error);
        void   (*conn_opened)               (GdaConnection *obj);
        void   (*conn_to_close)             (GdaConnection *obj);
        void   (*conn_closed)               (GdaConnection *obj);
	void   (*dsn_changed)               (GdaConnection *obj);
	void   (*transaction_status_changed)(GdaConnection *obj);
};

typedef enum {
	GDA_CONNECTION_OPTIONS_READ_ONLY = 1 << 0,
	GDA_CONNECTION_OPTIONS_DONT_SHARE = 2 << 0
} GdaConnectionOptions;

typedef enum {
	GDA_CONNECTION_FEATURE_AGGREGATES,
	GDA_CONNECTION_FEATURE_BLOBS,
	GDA_CONNECTION_FEATURE_INDEXES,
	GDA_CONNECTION_FEATURE_INHERITANCE,
	GDA_CONNECTION_FEATURE_NAMESPACES,
	GDA_CONNECTION_FEATURE_PROCEDURES,
	GDA_CONNECTION_FEATURE_SEQUENCES,
	GDA_CONNECTION_FEATURE_SQL,
	GDA_CONNECTION_FEATURE_TRANSACTIONS,
	GDA_CONNECTION_FEATURE_SAVEPOINTS,
	GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE,
	GDA_CONNECTION_FEATURE_TRIGGERS,
	GDA_CONNECTION_FEATURE_UPDATABLE_CURSOR,
	GDA_CONNECTION_FEATURE_USERS,
	GDA_CONNECTION_FEATURE_VIEWS,
	GDA_CONNECTION_FEATURE_XML_QUERIES
} GdaConnectionFeature;

typedef enum {
	GDA_CONNECTION_SCHEMA_AGGREGATES,
	GDA_CONNECTION_SCHEMA_DATABASES,
	GDA_CONNECTION_SCHEMA_FIELDS,
	GDA_CONNECTION_SCHEMA_INDEXES,
	GDA_CONNECTION_SCHEMA_LANGUAGES,
	GDA_CONNECTION_SCHEMA_NAMESPACES,
	GDA_CONNECTION_SCHEMA_PARENT_TABLES,
	GDA_CONNECTION_SCHEMA_PROCEDURES,
	GDA_CONNECTION_SCHEMA_SEQUENCES,
	GDA_CONNECTION_SCHEMA_TABLES,
	GDA_CONNECTION_SCHEMA_TRIGGERS,
	GDA_CONNECTION_SCHEMA_TYPES,
	GDA_CONNECTION_SCHEMA_USERS,
	GDA_CONNECTION_SCHEMA_VIEWS,
	GDA_CONNECTION_SCHEMA_CONSTRAINTS,
	GDA_CONNECTION_SCHEMA_TABLE_CONTENTS
} GdaConnectionSchema;


GType                gda_connection_get_type            (void);
GdaConnection       *gda_connection_new                 (GdaClient *client,
							 GdaServerProvider *provider,
							 const gchar *dsn,
							 const gchar *username,
							 const gchar *password,
							 guint options);
gboolean             gda_connection_open                 (GdaConnection *cnc, GError **error);
void                 gda_connection_close                (GdaConnection *cnc);
void                 gda_connection_close_no_warning     (GdaConnection *cnc);
gboolean             gda_connection_is_opened            (GdaConnection *cnc);

GdaClient           *gda_connection_get_client           (GdaConnection *cnc);

const gchar         *gda_connection_get_provider         (GdaConnection *cnc);
GdaServerProvider   *gda_connection_get_provider_obj     (GdaConnection *cnc);
GdaServerProviderInfo *gda_connection_get_infos          (GdaConnection *cnc);
guint                gda_connection_get_options          (GdaConnection *cnc);

const gchar         *gda_connection_get_server_version   (GdaConnection *cnc);
const gchar         *gda_connection_get_database         (GdaConnection *cnc);
const gchar         *gda_connection_get_dsn              (GdaConnection *cnc);
gboolean             gda_connection_set_dsn              (GdaConnection *cnc, const gchar *datasource);
const gchar         *gda_connection_get_cnc_string       (GdaConnection *cnc);
const gchar         *gda_connection_get_username         (GdaConnection *cnc);
gboolean             gda_connection_set_username         (GdaConnection *srv, const gchar *username);
const gchar         *gda_connection_get_password         (GdaConnection *cnc);
gboolean             gda_connection_set_password         (GdaConnection *srv, const gchar *password);

void                 gda_connection_add_event            (GdaConnection *cnc, GdaConnectionEvent *error);
GdaConnectionEvent  *gda_connection_add_event_string     (GdaConnection *cnc, const gchar *str, ...);
void                 gda_connection_add_events_list      (GdaConnection *cnc, GList *events_list);
void                 gda_connection_clear_events_list    (GdaConnection *cnc);
const GList         *gda_connection_get_events           (GdaConnection *cnc);

gboolean             gda_connection_change_database      (GdaConnection *cnc, const gchar *name);

GdaDataModel        *gda_connection_execute_select_command (GdaConnection *cnc, GdaCommand *cmd,
							    GdaParameterList *params, GError **error);
gint                 gda_connection_execute_non_select_command (GdaConnection *cnc, GdaCommand *cmd,
								GdaParameterList *params, GError **error);
GList               *gda_connection_execute_command      (GdaConnection *cnc, GdaCommand *cmd,
							  GdaParameterList *params, GError **error);
gchar               *gda_connection_get_last_insert_id   (GdaConnection *cnc, GdaDataModel *recset);

gboolean             gda_connection_begin_transaction    (GdaConnection *cnc, const gchar *name, 
							  GdaTransactionIsolation level, GError **error);
gboolean             gda_connection_commit_transaction   (GdaConnection *cnc, const gchar *name, GError **error);
gboolean             gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *name, GError **error);

gboolean             gda_connection_add_savepoint        (GdaConnection *cnc, const gchar *name, GError **error);
gboolean             gda_connection_rollback_savepoint   (GdaConnection *cnc, const gchar *name, GError **error);
gboolean             gda_connection_delete_savepoint     (GdaConnection *cnc, const gchar *name, GError **error);

GdaTransactionStatus *gda_connection_get_transaction_status (GdaConnection *cnc);

gchar               *gda_connection_value_to_sql_string  (GdaConnection *cnc, GValue *from);

gboolean             gda_connection_supports_feature     (GdaConnection *cnc, GdaConnectionFeature feature);
GdaDataModel        *gda_connection_get_schema           (GdaConnection *cnc, GdaConnectionSchema schema,
							  GdaParameterList *params, GError **error);

G_END_DECLS

#endif
