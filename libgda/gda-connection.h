/* GDA client library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#if !defined(__gda_connection_h__)
#  define __gda_connection_h__

#include <libgda/gda-command.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-error.h>
#include <libgda/gda-parameter.h>

G_BEGIN_DECLS

#define GDA_TYPE_CONNECTION            (gda_connection_get_type())
#define GDA_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION, GdaConnection))
#define GDA_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONNECTION, GdaConnectionClass))
#define GDA_IS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CONNECTION))
#define GDA_IS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION))

typedef struct _GdaServerProvider    GdaServerProvider;
typedef struct _GdaConnection        GdaConnection;
typedef struct _GdaConnectionClass   GdaConnectionClass;
typedef struct _GdaConnectionPrivate GdaConnectionPrivate;

typedef struct _GdaClient GdaClient; /* defined in gda-client.h */

struct _GdaConnection {
	GObject object;
	GdaConnectionPrivate *priv;
};

struct _GdaConnectionClass {
	GObjectClass object_class;

	/* signals */
	void (* error) (GdaConnection *cnc, GList *error_list);
};

GType          gda_connection_get_type (void);
GdaConnection *gda_connection_new (GdaClient *client,
				   GdaServerProvider *provider,
				   const gchar *dsn,
				   const gchar *username,
				   const gchar *password);
gboolean       gda_connection_close (GdaConnection *cnc);
gboolean       gda_connection_is_open (GdaConnection *cnc);

GdaClient     *gda_connection_get_client (GdaConnection *cnc);
void           gda_connection_set_client (GdaConnection *cnc, GdaClient *client);

const gchar   *gda_connection_get_dsn (GdaConnection *cnc);
const gchar   *gda_connection_get_cnc_string (GdaConnection *cnc);
const gchar   *gda_connection_get_provider (GdaConnection *cnc);
const gchar   *gda_connection_get_username (GdaConnection *cnc);
const gchar   *gda_connection_get_password (GdaConnection *cnc);

void           gda_connection_add_error (GdaConnection *cnc, GdaError *error);
void           gda_connection_add_error_string (GdaConnection *cnc, const gchar *str, ...);
void           gda_connection_add_error_list (GdaConnection *cnc, GList *error_list);

gboolean       gda_connection_create_database (GdaConnection *cnc, const gchar *name);
gboolean       gda_connection_drop_database (GdaConnection *cnc, const gchar *name);

GList         *gda_connection_execute_command (GdaConnection *cnc,
					       GdaCommand *cmd,
					       GdaParameterList *params);
GdaDataModel  *gda_connection_execute_single_command (GdaConnection *cnc,
						      GdaCommand *cmd,
						      GdaParameterList *params);
gint           gda_connection_execute_non_query (GdaConnection *cnc,
						 GdaCommand *cmd,
						 GdaParameterList *params);

gboolean       gda_connection_begin_transaction (GdaConnection *cnc, const gchar *id);
gboolean       gda_connection_commit_transaction (GdaConnection *cnc, const gchar *id);
gboolean       gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *id);

typedef enum {
	GDA_CONNECTION_FEATURE_AGGREGATES,
	GDA_CONNECTION_FEATURE_INDEXES,
	GDA_CONNECTION_FEATURE_INHERITANCE,
	GDA_CONNECTION_FEATURE_PROCEDURES,
	GDA_CONNECTION_FEATURE_SEQUENCES,
	GDA_CONNECTION_FEATURE_SQL,
	GDA_CONNECTION_FEATURE_TRANSACTIONS,
	GDA_CONNECTION_FEATURE_TRIGGERS,
	GDA_CONNECTION_FEATURE_USERS,
	GDA_CONNECTION_FEATURE_VIEWS,
	GDA_CONNECTION_FEATURE_XML_QUERIES
} GdaConnectionFeature;

gboolean       gda_connection_supports (GdaConnection *cnc, GdaConnectionFeature feature);

typedef enum {
	GDA_CONNECTION_SCHEMA_AGGREGATES,
	GDA_CONNECTION_SCHEMA_DATABASES,
	GDA_CONNECTION_SCHEMA_FIELDS,
	GDA_CONNECTION_SCHEMA_INDEXES,
	GDA_CONNECTION_SCHEMA_PROCEDURES,
	GDA_CONNECTION_SCHEMA_SEQUENCES,
	GDA_CONNECTION_SCHEMA_TABLES,
	GDA_CONNECTION_SCHEMA_TRIGGERS,
	GDA_CONNECTION_SCHEMA_TYPES,
	GDA_CONNECTION_SCHEMA_USERS,
	GDA_CONNECTION_SCHEMA_VIEWS
} GdaConnectionSchema;

GdaDataModel *gda_connection_get_schema (GdaConnection *cnc,
					 GdaConnectionSchema schema,
					 GdaParameterList *params);

G_END_DECLS

#endif
