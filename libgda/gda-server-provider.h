/* GDA library
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

#if !defined(__gda_server_provider_h__)
#  define __gda_server_provider_h__

#include <libgda/gda-command.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-transaction.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_PROVIDER            (gda_server_provider_get_type())
#define GDA_SERVER_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_PROVIDER, GdaServerProvider))
#define GDA_SERVER_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_PROVIDER, GdaServerProviderClass))
#define GDA_IS_SERVER_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_PROVIDER))
#define GDA_IS_SERVER_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_PROVIDER))

typedef struct _GdaServerProviderClass   GdaServerProviderClass;
typedef struct _GdaServerProviderPrivate GdaServerProviderPrivate;

struct _GdaServerProvider {
	GObject object;
	GdaServerProviderPrivate *priv;
};

struct _GdaServerProviderClass {
	GObjectClass parent_class;

	/* signals */
	void (* last_connection_gone) (GdaServerProvider *provider);

	/* virtual methods */
	const gchar * (* get_version) (GdaServerProvider *provider);

	gboolean (* open_connection) (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaQuarkList *params,
				      const gchar *username,
				      const gchar *password);
	gboolean (* close_connection) (GdaServerProvider *provider,
				       GdaConnection *cnc);

	const gchar * (* get_server_version) (GdaServerProvider *provider,
					      GdaConnection *cnc);
	const gchar * (* get_database) (GdaServerProvider *provider,
					GdaConnection *cnc);
	gboolean (* change_database) (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name);
	gboolean (* create_database) (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name);
	gboolean (* drop_database) (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const gchar *name);
	gboolean (* create_table) (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   const gchar *table_name,
				   const GdaFieldAttributes *attributes[]);
	gboolean (* drop_table) (GdaServerProvider *provider,
				 GdaConnection *cnc,
				 const gchar *table_name);

	GList * (* execute_command) (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params);
	char * (* get_last_insert_id) (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaDataModel *recset);

	gboolean (* begin_transaction) (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaTransaction *xaction);
	gboolean (* commit_transaction) (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaTransaction *xaction);
	gboolean (* rollback_transaction) (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GdaTransaction *xaction);

	gboolean (* supports) (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionFeature feature);

	GdaDataModel * (* get_schema) (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaConnectionSchema schema,
				       GdaParameterList *params);

	gboolean (* create_blob) (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaBlob *blob);
	
	gboolean (* escape_string) (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    gchar *to,
				    const gchar *from,
				    unsigned long length);
				  
				  
};

GType    gda_server_provider_get_type (void);
const gchar *gda_server_provider_get_version (GdaServerProvider *provider);
gboolean gda_server_provider_open_connection (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      GdaQuarkList *params,
					      const gchar *username,
					      const gchar *password);
gboolean gda_server_provider_close_connection (GdaServerProvider *provider,
					       GdaConnection *cnc);

const gchar *gda_server_provider_get_server_version (GdaServerProvider *provider,
						     GdaConnection *cnc);
const gchar *gda_server_provider_get_database (GdaServerProvider *provider,
					       GdaConnection *cnc);
gboolean gda_server_provider_change_database (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      const gchar *name);
gboolean gda_server_provider_create_database (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      const gchar *name);
gboolean gda_server_provider_drop_database (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    const gchar *name);
gboolean gda_server_provider_create_table (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   const gchar *table_name,
					   const GdaFieldAttributes *attributes[]);
gboolean gda_server_provider_drop_table (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 const gchar *table_name);

GList   *gda_server_provider_execute_command (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      GdaCommand *cmd,
					      GdaParameterList *params);
gchar   *gda_server_provider_get_last_insert_id (GdaServerProvider *provider,
						 GdaConnection *cnc,
						 GdaDataModel *recset);

gboolean gda_server_provider_begin_transaction (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaTransaction *xaction);
gboolean gda_server_provider_commit_transaction (GdaServerProvider *provider,
						 GdaConnection *cnc,
						 GdaTransaction *xaction);
gboolean gda_server_provider_rollback_transaction (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaTransaction *xaction);

gboolean gda_server_provider_supports (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaConnectionFeature feature);

GdaDataModel *gda_server_provider_get_schema (GdaServerProvider *provider,
					      GdaConnection *cnc,
					      GdaConnectionSchema schema,
					      GdaParameterList *params);

gboolean gda_server_provider_create_blob (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaBlob *blob);

gboolean gda_server_provider_escape_string (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    gchar *to,
					    const gchar *from,
					    unsigned long length);			  
	

G_END_DECLS

#endif
