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
}

static void
gda_sybase_provider_init (GdaSybaseProvider *myprv, 
			  GdaSybaseProviderClass *klass)
{
	setlocale (LC_ALL, "");
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

/* open_connection handler for the GdaSybaseProvider class */
static gboolean
gda_sybase_provider_open_connection (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
		
	sybase_connection *sconn;
	CS_RETCODE ret;
	const gchar *t_host = NULL;
	const gchar *t_db = NULL;
	const gchar *t_user = NULL;
	const gchar *t_password = NULL;		
	CS_CHAR buf[500];
		
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

	sybase_debug_msg ("username: '%s', password '%s'",
	                  (t_user != NULL) ? t_user : "(NULL)",
	                  (t_password != NULL) ? t_password : "(NULL)");

	sconn = g_new(sybase_connection,1);
	sconn->context = NULL;
	sconn->connection = NULL;

	ret = cs_ctx_alloc (CS_VERSION_100, &sconn->context);
	if (ret != CS_SUCCEED) {
		g_free((gpointer)sconn);
		return FALSE;
	}
	ret = ct_init (sconn->context, CS_VERSION_100);
	if (ret != CS_SUCCEED) {
		if (sconn->context) {
			ct_con_drop (sconn->connection);
		}
		g_free((gpointer)sconn);
		return FALSE;
	}				
	/* allocate the connection structure */
	ret = ct_con_alloc(sconn->context, &sconn->connection);
	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL) {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}						
	ret = cs_diag (sconn->context, 
		       CS_INIT, 
		       CS_UNUSED, 
		       CS_UNUSED,
		       NULL);		
	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL)  {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}						
	ret = ct_diag (sconn->connection, 
		       CS_INIT, 
		       CS_UNUSED, 
		       CS_UNUSED,
		       NULL);
	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL) {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}								
	ret = ct_con_props (sconn->connection, 
			    CS_SET, 
			    CS_APPNAME, 
			    (CS_CHAR *) "libgda test", 
			    CS_NULLTERM, 
			    NULL);		
	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL) {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}						


	ret = ct_con_props(sconn->connection, 
			   CS_SET, 
			   CS_USERNAME, 
			   (CS_CHAR *) t_user,
			   CS_NULLTERM, 
			   NULL);

	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL) {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}								
	ret = ct_con_props (sconn->connection, 
			    CS_SET, 
			    CS_PASSWORD, 
			    (CS_CHAR *) t_password,
			    CS_NULLTERM, 
			    NULL);

	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL) {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}						
	ret = ct_connect (sconn->connection, 
			  (CS_CHAR *) NULL, 
			  0);
	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL) {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}						
	/* get the hostname from SQL server as a test */
	ret = ct_con_props (sconn->connection, 
			    CS_GET, 
			    CS_SERVERNAME, 
			    &buf, 
			    CS_MAX_CHAR, 
			    NULL);		
	if (ret != CS_SUCCEED) {
		if (sconn->connection != NULL)  {
			ct_con_drop (sconn->connection);
			sconn->connection = NULL;
		}
		if (sconn->context != NULL) {
			ct_exit (sconn->context, CS_FORCE_EXIT);
			cs_ctx_drop (sconn->context);
			sconn->context = NULL;
		}
		return FALSE;
	}						
//	return TRUE;

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE, sconn);

	return TRUE;
}

/* close_connection handler for the GdaSybaseProvider class */
static gboolean
gda_sybase_provider_close_connection (GdaServerProvider *provider, 
				      GdaConnection *cnc)
{
	sybase_connection *sconn;
	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	sconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);
	if (sconn->connection != NULL) {
		ct_con_drop (sconn->connection);
		sconn->connection = NULL;
	}
	if (sconn->context != NULL) {
		ct_exit (sconn->context, CS_FORCE_EXIT);
		cs_ctx_drop (sconn->context);
		sconn->context = NULL;
	}
	g_free (sconn);
	sconn = NULL;
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

