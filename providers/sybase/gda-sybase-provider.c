/* GDA Sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *         Holger Thon <holger.thon@gnome-db.org>
 *      based on the MySQL provider by
 *         Michael Lausch <michael@lausch.at>
 *	        Rodrigo Moya <rodrigo@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif
#include <string.h>
#include "gda-sybase.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

static void gda_sybase_provider_class_init (GdaSybaseProviderClass *klass);
static void gda_sybase_provider_init (GdaSybaseProvider *provider,
                                      GdaSybaseProviderClass *klass);
static void gda_sybase_provider_finalize (GObject *object);

static gboolean gda_sybase_provider_open_connection (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaQuarkList *params,
						     const gchar *username,
						     const gchar *password);
static gboolean gda_sybase_provider_close_connection (GdaServerProvider *provider,
						      GdaConnection *cnc);


static gboolean gda_sybase_provider_begin_transaction (GdaServerProvider *provider,
                                                       GdaConnection *cnc,
                                                       GdaTransaction *xaction);
static gboolean gda_sybase_provider_change_database (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     const gchar *name);
static gboolean gda_sybase_provider_commit_transaction (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        GdaTransaction *xaction);
static gboolean gda_sybase_provider_create_database (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     const gchar *name);
static gboolean gda_sybase_provider_drop_database (GdaServerProvider *provider,
                                                   GdaConnection *cnc,
                                                   const gchar *name);
static GList *gda_sybase_provider_execute_command (GdaServerProvider *provider,
                                                   GdaConnection *cnc,
                                                   GdaCommand *cmd,
                                                   GdaParameterList *params);
static const gchar *gda_sybase_provider_get_database (GdaServerProvider *provider,
                                                      GdaConnection *cnc);
static GdaDataModel *gda_sybase_provider_get_schema (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     GdaConnectionSchema schema,
                                                     GdaParameterList *params);
static const gchar *gda_sybase_provider_get_server_version (GdaServerProvider *provider,
                                        GdaConnection *cnc);
static const gchar *gda_sybase_provider_get_version (GdaServerProvider *provider);
static gboolean gda_sybase_provider_rollback_transaction (GdaServerProvider *provider,
                                                          GdaConnection *cnc,
                                                          GdaTransaction *xaction);
static gboolean gda_sybase_provider_supports (GdaServerProvider *provider,
                                              GdaConnection *cnc,
                                              GdaConnectionFeature feature);


static GObjectClass *parent_class = NULL;

/*
 * GdaSybaseProvider class implementation
 */

static void
gda_sybase_provider_class_init (GdaSybaseProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sybase_provider_finalize;
	
	provider_class->begin_transaction = gda_sybase_provider_begin_transaction;
	provider_class->change_database = gda_sybase_provider_change_database;
	provider_class->close_connection = gda_sybase_provider_close_connection;
	provider_class->commit_transaction = gda_sybase_provider_commit_transaction;
	provider_class->create_database = gda_sybase_provider_create_database;
	provider_class->drop_database = gda_sybase_provider_drop_database;
	provider_class->execute_command = gda_sybase_provider_execute_command;
	provider_class->get_database = gda_sybase_provider_get_database;
	provider_class->get_schema = gda_sybase_provider_get_schema;
	provider_class->get_server_version = gda_sybase_provider_get_server_version;
	provider_class->get_version = gda_sybase_provider_get_version;
	provider_class->open_connection = gda_sybase_provider_open_connection;
	provider_class->rollback_transaction = gda_sybase_provider_rollback_transaction;
	provider_class->supports = gda_sybase_provider_supports;

	setlocale(LC_ALL, "C");
}

static void
gda_sybase_provider_init (GdaSybaseProvider *myprv, 
			  GdaSybaseProviderClass *klass)
{
}

static void
gda_sybase_provider_finalize (GObject *object)
{
	GdaSybaseProvider *myprv = (GdaSybaseProvider *) object;

	g_return_if_fail (GDA_IS_SYBASE_PROVIDER (myprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_sybase_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaSybaseProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sybase_provider_class_init,
			NULL, NULL,
			sizeof (GdaSybaseProvider),
			0,
			(GInstanceInitFunc) gda_sybase_provider_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaSybaseProvider",
					       &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_sybase_provider_new (void)
{
	GdaSybaseProvider *provider;

	provider = g_object_new (gda_sybase_provider_get_type (), NULL);

	return GDA_SERVER_PROVIDER (provider);
}

GdaSybaseConnectionData *
gda_sybase_connection_data_new(void)
{
	GdaSybaseConnectionData *sconn = g_new0 (GdaSybaseConnectionData, 1);

	return sconn;
}

/* this function just frees the connection structure. 
 * make sure, connection is already closed [ct_close(sconn->connection);] */
void 
gda_sybase_connection_data_free(GdaSybaseConnectionData *sconn)
{
	if (sconn) {
		// if a GdaConnection is associated with the data,
		// clear the handle
		if (GDA_IS_CONNECTION (sconn->cnc)) {
			// we do not check if the handle is the same,
			// because it must be identical (connection_open)
			g_object_set_data (G_OBJECT (sconn->cnc), 
			                   OBJECT_DATA_SYBASE_HANDLE, NULL);
			// is just a reference copy
			sconn->cnc = NULL;
		}
		// drop connection
		if (sconn->connection) {
			sconn->ret = ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		// exit library and drop context
		if (sconn->context) {
			sconn->ret = ct_exit (sconn->context, CS_UNUSED);
			sconn->ret = cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		sconn->ret = CS_SUCCEED;
		g_free (sconn);
		sconn = NULL;
	}
}

/* open_connection handler for the GdaSybaseProvider class */
static gboolean
gda_sybase_provider_open_connection (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
		
	GdaSybaseConnectionData *sconn = NULL;
	CS_LOCALE   *locale = NULL;
	const gchar *t_host = NULL;
	const gchar *t_db = NULL;
	const gchar *t_user = NULL;
	const gchar *t_password = NULL;
	const gchar *t_locale = NULL;
	CS_CHAR buf[CS_MAX_CHAR + 1];

	memset(&buf, 0, sizeof(buf));
		
	/* the logic to connect to the database */

	sybase_debug_msg("about to open connection");
	
	if (username)
		t_user = g_strdup(username);
	else
		t_user = gda_quark_list_find (params, "USERNAME");

		
	if (password)
		t_password = g_strdup(password);
	else
		t_password = gda_quark_list_find (params, "PASSWORD");

	t_db = gda_quark_list_find (params, "DATABASE");
	t_host = gda_quark_list_find (params, "HOSTNAME");
	if (!t_host)
		t_host = gda_quark_list_find (params, "HOST");

	t_locale = gda_quark_list_find (params, "LOCALE");

	sybase_debug_msg ("username: '%s', password '%s'",
	                  (t_user != NULL) ? t_user : "(NULL)",
	                  (t_password != NULL) ? t_password : "(NULL)");
	
	sconn = g_new0 (GdaSybaseConnectionData, 1);
	if (!sconn) {
		sybase_debug_msg (_("Out of memory. Allocating connection structure failed."));
		return FALSE;
	}

	/* CS_GDA_VERSION is set to the latest CS_VERSION available
	 * alloc context */
	sconn->ret = cs_ctx_alloc (CS_GDA_VERSION, &sconn->context);
	if (sconn->ret != CS_SUCCEED) {
		g_free (sconn);
		sconn = NULL;
		return FALSE;
	}
	sybase_debug_msg(_("Context allocated."));
	/* init client-lib */
	sconn->ret = ct_init (sconn->context, CS_GDA_VERSION);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Client library initialized."));

	/* apply locale before connection */
	if (t_locale) {
		sconn->ret = cs_loc_alloc (sconn->context, &locale);
		/* locale setting should not make connection fail
		 * 
		 * avoid nested if clauses because of same condition */
		if (sconn->ret == CS_SUCCEED) {
			sconn->ret = cs_locale (sconn->context,
			                        CS_SET,
			                        locale,
			                        CS_LC_ALL,
			                        (CS_CHAR *) t_locale,
			                        CS_NULLTERM,
			                        NULL);
		}
		if (sconn->ret == CS_SUCCEED) {
			sconn->ret = cs_config (sconn->context,
			                        CS_SET,
			                        CS_LOC_PROP,
			                        locale,
			                        CS_UNUSED,
			                        NULL);
		}
		if (sconn->ret == CS_SUCCEED) {
			sybase_debug_msg (_("Locale set to '%s'."), t_locale);
		}
		if (locale) {
			sconn->ret = cs_loc_drop (sconn->context,
			                          locale);
			locale = NULL;
		}
	}

	/* Initialize callback handlers */
	/*   cs */
	sconn->ret = cs_config (sconn->context, CS_SET, CS_MESSAGE_CB,
	                        (CS_VOID *) gda_sybase_csmsg_callback,
	                         CS_UNUSED, NULL);
	if (sconn->ret != CS_SUCCEED) {
		sybase_debug_msg(_("Could not initialize cslib callback"));
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("CS-lib callback initialized."));
	/*   ct-cli */
	sconn->ret = ct_callback (sconn->context, NULL, CS_SET,
	                          CS_CLIENTMSG_CB,
	                          (CS_VOID *) gda_sybase_clientmsg_callback);
	if (sconn->ret != CS_SUCCEED) {
		sybase_debug_msg(_("Could not initialize ct-client callback"));
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("CT-Client callback initialized."));
	
	/*   ct-srv */
	sconn->ret = ct_callback (sconn->context, NULL, CS_SET, 
	                          CS_SERVERMSG_CB,
	                          (CS_VOID *) gda_sybase_servermsg_callback);
	if (sconn->ret != CS_SUCCEED) {
		sybase_debug_msg(_("Could not initialize ct-server callback"));
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("CT-Server callback initialized."));

	/* allocate the connection structure */
	sconn->ret = ct_con_alloc(sconn->context, &sconn->connection);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Connection allocated."));
	/* init inline error handling for cs-lib */
/*	sconn->ret = cs_diag (sconn->context, 
	                      CS_INIT, 
	                      CS_UNUSED, 
	                      CS_UNUSED,
	                      NULL);		
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Error handling for cs-lib initialized.")); */
	/* init inline error handling for ct-lib */
/*	sconn->ret = ct_diag (sconn->connection, 
	                      CS_INIT, 
	                      CS_UNUSED, 
	                      CS_UNUSED,
	                      NULL);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Error handling for ct-lib initialized.")); */

	/* set object data for error handling routines */
	sconn->cnc = cnc; /* circular reference for freeing */
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE, sconn);
	/* now first check possible, do so */
	
	/* set appname */
	sconn->ret = ct_con_props (sconn->connection, 
	                           CS_SET, 
	                           CS_APPNAME, 
	                           (CS_CHAR *) "gda-sybase provider", 
	                           CS_NULLTERM, 
	                           NULL);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Appname set."));

	/* set username */
	sconn->ret = ct_con_props (sconn->connection, 
	                           CS_SET, 
	                           CS_USERNAME, 
	                           (CS_CHAR *) t_user,
	                           CS_NULLTERM, 
	                           NULL);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Username set."));

	/* assume null-length passwords as passwordless logins */
	if ((t_password != NULL) && (strlen(t_password) > 0)) {
		sconn->ret = ct_con_props (sconn->connection, 
		                           CS_SET, 
		                           CS_PASSWORD, 
		                           (CS_CHAR *) t_password,
		                           CS_NULLTERM, 
		                           NULL);
		if (sconn->ret != CS_SUCCEED) {
			gda_sybase_connection_data_free (sconn);
			return FALSE;
		}
		sybase_debug_msg(_("Password set."));
	} else {
		sconn->ret = ct_con_props (sconn->connection, 
		                           CS_SET, 
		                           CS_PASSWORD, 
		                           (CS_CHAR *) NULL,
		                           CS_NULLTERM, 
		                           NULL);
		if (sconn->ret != CS_SUCCEED) {
			gda_sybase_connection_data_free (sconn);
			return FALSE;
		}
		sybase_debug_msg(_("Empty password set."));
	}
	sconn->ret = ct_connect (sconn->connection, 
	                         (t_host) ? ((CS_CHAR *) t_host)
	                                  : ((CS_CHAR *) NULL), 
	                         (t_host) ? strlen(t_host) : 0);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	sybase_debug_msg(_("Connected."));

	/* get the hostname from SQL server as a test */
	sconn->ret = ct_con_props (sconn->connection, 
	                           CS_GET, 
	                           CS_SERVERNAME, 
	                           &buf,
	                           CS_MAX_CHAR, 
	                           NULL);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		
		return FALSE;
	}
//	return TRUE;

	sybase_debug_msg (_("Finally connected."));

	return TRUE;
}

/* close_connection handler for the GdaSybaseProvider class */
static gboolean
gda_sybase_provider_close_connection (GdaServerProvider *provider, 
				      GdaConnection *cnc)
{
	GdaSybaseConnectionData *sconn;
	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	sconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (sconn != NULL, FALSE);

	// close connection before freeing data
	if (sconn->connection != NULL) {
		sconn->ret = ct_close(sconn->connection, CS_UNUSED);
	}
	gda_sybase_connection_data_free (sconn);
	
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE, NULL);
	return TRUE;
}

static gboolean
gda_sybase_provider_begin_transaction(GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_sybase_provider_change_database (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     const gchar *name)
{
	return FALSE;
}

static gboolean 
gda_sybase_provider_commit_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean 
gda_sybase_provider_create_database (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     const gchar *name)
{
	return FALSE;
}

static gboolean 
gda_sybase_provider_drop_database (GdaServerProvider *provider,
                                   GdaConnection *cnc,
                                   const gchar *name)
{
	return FALSE;
}

static GList *
gda_sybase_provider_execute_command (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     GdaCommand *cmd,
                                     GdaParameterList *params)
{
	return NULL;
}

static const gchar *
gda_sybase_provider_get_database (GdaServerProvider *provider,
                                  GdaConnection *cnc)
{
	return NULL;
}

static GdaDataModel *
gda_sybase_provider_get_schema (GdaServerProvider *provider,
                                GdaConnection *cnc,
                                GdaConnectionSchema schema,
                                GdaParameterList *params)
{
	return NULL;
}

static const gchar *
gda_sybase_provider_get_server_version (GdaServerProvider *provider,
                                        GdaConnection *cnc)
{
	return NULL;
}

static const gchar *
gda_sybase_provider_get_version (GdaServerProvider *provider)
{
	return NULL;
}

static gboolean 
gda_sybase_provider_rollback_transaction (GdaServerProvider *provider,
                                          GdaConnection *cnc,
                                          GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean 
gda_sybase_provider_supports (GdaServerProvider *provider,
                              GdaConnection *cnc,
                              GdaConnectionFeature feature)
{
	return FALSE;
}

