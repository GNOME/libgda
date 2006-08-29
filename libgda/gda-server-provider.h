/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
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

#ifndef __GDA_SERVER_PROVIDER_H__
#define __GDA_SERVER_PROVIDER_H__

#include <libgda/gda-command.h>
#include <libgda/gda-server-operation.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-index.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-transaction.h>
#include <libgda/gda-client.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_PROVIDER            (gda_server_provider_get_type())
#define GDA_SERVER_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_PROVIDER, GdaServerProvider))
#define GDA_SERVER_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_PROVIDER, GdaServerProviderClass))
#define GDA_IS_SERVER_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_PROVIDER))
#define GDA_IS_SERVER_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_PROVIDER))

/* 
 * struct to hold any information specific to the provider used 
 */
struct _GdaServerProviderInfo {
        gchar         *provider_name; /* equal to the return of gda_connection_get_provider() */

        /*
         * TRUE if all comparisons of names can be done on the lower case versions of the objects names
         */
        gboolean       is_case_insensitive;

        /*
         * TRUE to suppose that there are implicit casts available for data types which have
         * the same gda type
         */
        gboolean       implicit_data_types_casts;

        /*
         * TRUE if writing "... FROM mytable AS alias..." is ok, and FALSE if we need to write this as
         * "... FROM mytable alias..."
         */
        gboolean       alias_needs_as_keyword;
};

struct _GdaServerProvider {
	GObject                   object;
	GdaServerProviderPrivate *priv;
};

struct _GdaServerProviderClass {
	GObjectClass parent_class;

	/* signals */
	void                   (* last_connection_gone) (GdaServerProvider *provider);
	gpointer                  reserved1;
	gpointer                  reserved2;

	/* virtual methods */

	/* provider information */
	const gchar           *(* get_version) (GdaServerProvider *provider);
	const gchar           *(* get_server_version) (GdaServerProvider *provider,
						       GdaConnection *cnc);
	GdaServerProviderInfo *(* get_info) (GdaServerProvider *provider,
					     GdaConnection *cnc);
	gboolean               (* supports_feature) (GdaServerProvider *provider,
					             GdaConnection *cnc,
					             GdaConnectionFeature feature);
	
	GdaDataModel          *(* get_schema) (GdaServerProvider *provider,
					       GdaConnection *cnc,
					       GdaConnectionSchema schema,
					       GdaParameterList *params);

	/* types and values manipulation */
	GdaDataHandler        *(* get_data_handler) (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GType gda_type,
						     const gchar *dbms_type);
	GValue                *(* string_to_value) (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *string, 
						    GType prefered_type,
						    gchar **dbms_type);
	const gchar           *(*get_def_dbms_type) (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GType gda_type);

	/* connections management */
	gboolean               (* open_connection) (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
	gboolean               (* close_connection) (GdaServerProvider *provider,
						     GdaConnection *cnc);
	
	const gchar           *(* get_database) (GdaServerProvider *provider,
						 GdaConnection *cnc);
	gboolean               (* change_database) (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *name);
	/* operations */
	gboolean               (* supports_operation) (GdaServerProvider *provider, GdaConnection *cnc, 
						       GdaServerOperationType type, GdaParameterList *options);
	GdaServerOperation    *(* create_operation)   (GdaServerProvider *provider, GdaConnection *cnc, 
						       GdaServerOperationType type, 
						       GdaParameterList *options, GError **error);
	gchar                 *(* render_operation)   (GdaServerProvider *provider, GdaConnection *cnc, 
						       GdaServerOperation *op, GError **error);
	gboolean               (* perform_operation)  (GdaServerProvider *provider, GdaConnection *cnc, 
						       GdaServerOperation *op, GError **error);	

	/* commands */
	GList                  *(* execute_command) (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaCommand *cmd,
						     GdaParameterList *params);
	char                   *(* get_last_insert_id) (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaDataModel *recset);
	
	/* transactions */
	gboolean                (* begin_transaction) (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
	gboolean                (* commit_transaction) (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaTransaction *xaction);
	gboolean                (* rollback_transaction) (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  GdaTransaction *xaction);

	/* blobs */
	GdaBlob                *(* create_blob) (GdaServerProvider *provider,
						 GdaConnection *cnc);

	GdaBlob                *(* fetch_blob) (GdaServerProvider *provider,
						GdaConnection *cnc, const gchar *sql_id);
};

GType                  gda_server_provider_get_type (void);

/* provider information */
const gchar           *gda_server_provider_get_version        (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_server_version (GdaServerProvider *provider,
							       GdaConnection *cnc);
GdaServerProviderInfo *gda_server_provider_get_info           (GdaServerProvider *provider,
							       GdaConnection *cnc);
gboolean               gda_server_provider_supports_feature   (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       GdaConnectionFeature feature);
GdaDataModel          *gda_server_provider_get_schema         (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       GdaConnectionSchema schema,
							       GdaParameterList *params, GError **error);

/* types and values manipulation */
GdaDataHandler        *gda_server_provider_get_data_handler_gtype(GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType for_type);
GdaDataHandler        *gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *for_type);
GValue                *gda_server_provider_string_to_value       (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *string, 
								  GType prefered_type,
								  gchar **dbms_type);
gchar                 *gda_server_provider_value_to_sql_string   (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GValue *from);
const gchar           *gda_server_provider_get_default_dbms_type (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType type);


/* connections management */
gboolean               gda_server_provider_open_connection  (GdaServerProvider *provider,
							     GdaConnection *cnc,
							     GdaQuarkList *params,
							     const gchar *username,
							     const gchar *password);
gboolean               gda_server_provider_reset_connection (GdaServerProvider *provider,
							     GdaConnection *cnc);
gboolean               gda_server_provider_close_connection (GdaServerProvider *provider,
							     GdaConnection *cnc);

const gchar           *gda_server_provider_get_database     (GdaServerProvider *provider,
							     GdaConnection *cnc);
gboolean               gda_server_provider_change_database  (GdaServerProvider *provider,
							     GdaConnection *cnc,
							     const gchar *name);

/* actions with parameters */
gboolean               gda_server_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperationType type, GdaParameterList *options);
GdaServerOperation    *gda_server_provider_create_operation   (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperationType type, 
							       GdaParameterList *options, GError **error);
gchar                 *gda_server_provider_render_operation   (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperation *op, GError **error);
gboolean               gda_server_provider_perform_operation  (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperation *op, GError **error);

/* commands */
GList        *gda_server_provider_execute_command    (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      GdaCommand *cmd,
						      GdaParameterList *params);
gchar        *gda_server_provider_get_last_insert_id (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      GdaDataModel *recset);

/* transactions */
gboolean      gda_server_provider_begin_transaction    (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaTransaction *xaction);
gboolean      gda_server_provider_commit_transaction   (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaTransaction *xaction);
gboolean      gda_server_provider_rollback_transaction (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaTransaction *xaction);

/* blobs */
GdaBlob      *gda_server_provider_create_blob          (GdaServerProvider *provider,
							GdaConnection *cnc);
GdaBlob      *gda_server_provider_fetch_blob_by_id     (GdaServerProvider *provider,
							GdaConnection *cnc, const gchar *sql_id);

G_END_DECLS

#endif
