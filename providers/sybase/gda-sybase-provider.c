/* GDA Sybase provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *         Holger Thon <holger.thon@gnome-db.org>
 *      based on the MySQL provider by
 *         Michael Lausch <michael@lausch.at>
 *	   Rodrigo Moya <rodrigo@gnome-db.org>
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
#include <locale.h>
#include "gda-sybase.h"
#include "gda-sybase-recordset.h"
#include "gda-sybase-types.h"
#include <libgda/gda-data-model-private.h>

#include <libgda/sql-delimiter/gda-sql-delimiter.h>

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
                                                       const gchar *name, GdaTransactionIsolation level,
						       GError **error);
static gboolean gda_sybase_provider_change_database (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     const gchar *name);
static gboolean gda_sybase_provider_commit_transaction (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        const gchar *name, GError **error);
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
                                                          const gchar *name, GError **error);
static gboolean gda_sybase_provider_supports (GdaServerProvider *provider,
                                              GdaConnection *cnc,
                                              GdaConnectionFeature feature);
static GList* gda_sybase_provider_process_sql_commands(GList         *reclist,
                                                       GdaConnection *cnc,
                                                       const gchar   *sql);
static GdaDataModel *gda_sybase_execute_query (GdaConnection *cnc,
                                               const gchar *sql);
static gboolean gda_sybase_execute_cmd (GdaConnection *cnc, const gchar *sql);

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

	provider_class->get_version = gda_sybase_provider_get_version;
	provider_class->get_server_version = gda_sybase_provider_get_server_version;
	provider_class->get_info = NULL;
	provider_class->supports_feature = gda_sybase_provider_supports;
	provider_class->get_schema = gda_sybase_provider_get_schema;

	provider_class->get_data_handler = NULL;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->create_connection = NULL;
	provider_class->open_connection = gda_sybase_provider_open_connection;
	provider_class->close_connection = gda_sybase_provider_close_connection;
	provider_class->get_database = gda_sybase_provider_get_database;
	provider_class->change_database = gda_sybase_provider_change_database;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_sybase_provider_execute_command;
	provider_class->execute_query = NULL;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_sybase_provider_begin_transaction;
	provider_class->commit_transaction = gda_sybase_provider_commit_transaction;
	provider_class->rollback_transaction = gda_sybase_provider_rollback_transaction;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;

	setlocale (LC_ALL, "C");
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


	g_print("loading sybase provider!\n");

	return GDA_SERVER_PROVIDER (provider);
}

GdaSybaseConnectionData *
gda_sybase_connection_data_new(void)
{
	GdaSybaseConnectionData *sconn = g_new0 (GdaSybaseConnectionData, 1);

	if ( !sconn) 
		return NULL;

	sconn->server_version = NULL;
	sconn->gda_cnc = NULL;
	sconn->context = NULL;
	sconn->cmd = NULL;
	sconn->connection = NULL;
	sconn->mempool = NULL;

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
		if (GDA_IS_CONNECTION (sconn->gda_cnc)) {
			// we do not check if the handle is the same,
			// because it must be identical (connection_open)
			g_object_set_data (G_OBJECT (sconn->gda_cnc), 
					   OBJECT_DATA_SYBASE_HANDLE, NULL);
			// is just a reference copy
			sconn->gda_cnc = NULL;
		}
		if (sconn->server_version) {
			g_free (sconn->server_version);
			sconn->server_version = NULL;
		}
		// drop command structure
		if (sconn->cmd) {
			sconn->ret = ct_cmd_drop (sconn->cmd);
			sconn->cmd = NULL;
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

	//sybase_debug_msg(_("about to open connection"));
	
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

	//sybase_debug_msg ("username: '%s', password '%s'",
	//																	(t_user != NULL) ? t_user : "(NULL)",
	//																		(t_password != NULL) ? "XXXXXXXX" : "(NULL)");
	
	//		sconn = g_new0 (GdaSybaseConnectionData, 1);
	sconn = gda_sybase_connection_data_new();

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
	//sybase_debug_msg(_("Context allocated."));
	/* init client-lib */
	sconn->ret = ct_init (sconn->context, CS_GDA_VERSION);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	//sybase_debug_msg(_("Client library initialized."));

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
	/*  	sconn->ret = cs_config (sconn->context, CS_SET, CS_MESSAGE_CB, */
	/*  																									(CS_VOID *) gda_sybase_csmsg_callback, */
	/*  																									CS_UNUSED, NULL); */
	/*  	if (sconn->ret != CS_SUCCEED) { */
	/*  		sybase_debug_msg(_("Could not initialize cslib callback")); */
	/*  		gda_sybase_connection_data_free (sconn); */
	/*  		return FALSE; */
	/*  	} */
	/*  	sybase_debug_msg(_("CS-lib callback initialized.")); */
	/*   ct-cli */
	/*  	sconn->ret = ct_callback (sconn->context, NULL, CS_SET, */
	/*  																											CS_CLIENTMSG_CB, */
	/*  																											(CS_VOID *) gda_sybase_clientmsg_callback); */
	/*  	if (sconn->ret != CS_SUCCEED) { */
	/*  		sybase_debug_msg(_("Could not initialize ct-client callback")); */
	/*  		gda_sybase_connection_data_free (sconn); */
	/*  		return FALSE; */
	/*  	} */
	/*  	sybase_debug_msg(_("CT-Client callback initialized.")); */
	
	/*   ct-srv */
	/*  	sconn->ret = ct_callback (sconn->context, NULL, CS_SET,  */
	/*  																											CS_SERVERMSG_CB, */
	/*  																											(CS_VOID *) gda_sybase_servermsg_callback); */
	/*  	if (sconn->ret != CS_SUCCEED) { */
	/*  		sybase_debug_msg(_("Could not initialize ct-server callback")); */
	/*  		gda_sybase_connection_data_free (sconn); */
	/*  		return FALSE; */
	/*  	} */
	/*  	sybase_debug_msg(_("CT-Server callback initialized.")); */

	/* allocate the connection structure */
	sconn->ret = ct_con_alloc(sconn->context, &sconn->connection);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	//sybase_debug_msg(_("Connection allocated."));
	/* init inline error handling for cs-lib */
	sconn->ret = cs_diag (sconn->context, 
			      CS_INIT, 
			      CS_UNUSED, 
			      CS_UNUSED,
			      NULL);		
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	//sybase_debug_msg(_("Error handling for cs-lib initialized.")); 
	/* init inline error handling for ct-lib */
	sconn->ret = ct_diag (sconn->connection, 
			      CS_INIT, 
			      CS_UNUSED, 
			      CS_UNUSED,
			      NULL);
	if (sconn->ret != CS_SUCCEED) {
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	//sybase_debug_msg(_("Error handling for ct-lib initialized.")); 

	/* set object data for error handling routines */
	sconn->gda_cnc = cnc; /* circular reference for freeing */

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
		sybase_check_messages(cnc);
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	//sybase_debug_msg(_("Appname set."));

	/* set username */
	sconn->ret = ct_con_props (sconn->connection, 
				   CS_SET, 
				   CS_USERNAME, 
				   (CS_CHAR *) t_user,
				   CS_NULLTERM, 
				   NULL);
	if (sconn->ret != CS_SUCCEED) {
		sybase_check_messages(cnc);
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}
	//sybase_debug_msg(_("Username set."));

	/* assume null-length passwords as passwordless logins */
	if ((t_password != NULL) && (strlen(t_password) > 0)) {
		sconn->ret = ct_con_props (sconn->connection, 
					   CS_SET, 
					   CS_PASSWORD, 
					   (CS_CHAR *) t_password,
					   CS_NULLTERM, 
					   NULL);
		if (sconn->ret != CS_SUCCEED) {
			sybase_check_messages(cnc);
			gda_sybase_connection_data_free (sconn);
			return FALSE;
		}
		//sybase_debug_msg(_("Password set."));
	} else {
		sconn->ret = ct_con_props (sconn->connection, 
					   CS_SET, 
					   CS_PASSWORD, 
					   (CS_CHAR *) "",
					   CS_NULLTERM, 
					   NULL);
		if (sconn->ret != CS_SUCCEED) {
			sybase_check_messages(cnc);
			gda_sybase_connection_data_free (sconn);
			return FALSE;
		}
		//sybase_debug_msg(_("Empty password set."));
	}
	sconn->ret = ct_connect (sconn->connection, 
				 (t_host) ? ((CS_CHAR *) t_host)
				 : ((CS_CHAR *) NULL), 
				 (t_host) ? strlen(t_host) : 0);
	if (sconn->ret != CS_SUCCEED) {
		sybase_check_messages(cnc);
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}

	//sybase_debug_msg(_("Connected."));

	if (t_db) 
		gda_sybase_provider_change_database (provider, cnc, t_db);
	
	/* get the hostname from SQL server as a test */
	sconn->ret = ct_con_props (sconn->connection, 
				   CS_GET, 
				   CS_SERVERNAME, 
				   &buf,
				   CS_MAX_CHAR, 
				   NULL);
	if (sconn->ret != CS_SUCCEED) {
		sybase_check_messages(cnc);
		gda_sybase_connection_data_free (sconn);
		return FALSE;
	}

	//sybase_debug_msg (_("Finally connected."));

	// get rid of the initial change database context to... messages
	sybase_check_messages(cnc);

	//sybase_debug_msg (_("done emptying junk messages."));

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
                                      const gchar *name, GdaTransactionIsolation level,
				      GError **error)
{
	return (gda_sybase_execute_cmd (cnc, "begin transaction"));
}

static gboolean
gda_sybase_provider_change_database (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     const gchar *name)
{
	gchar *sql_cmd = NULL;
	gboolean ret = FALSE;
	GdaSybaseProvider *syb_prov = (GdaSybaseProvider *) provider;

	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (syb_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	sql_cmd = g_strdup_printf("USE %s", name);
	ret = gda_sybase_execute_cmd(cnc, sql_cmd);
	g_free(sql_cmd);

	return ret;
}

static gboolean 
gda_sybase_provider_commit_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        const gchar *name, GError **error)
{
	return (gda_sybase_execute_cmd (cnc, "commit transaction"));		
}

static GList *
gda_sybase_provider_execute_command (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     GdaCommand *cmd,
                                     GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *query = NULL;

	GdaSybaseProvider *syb_prov = (GdaSybaseProvider *) provider;

	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (syb_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL:
		reclist = gda_sybase_provider_process_sql_commands (reclist, cnc, gda_command_get_text (cmd));
		break;
	case GDA_COMMAND_TYPE_TABLE:
		query = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
		reclist = gda_sybase_provider_process_sql_commands (reclist, cnc, query);
		if (reclist && GDA_IS_DATA_MODEL (reclist->data)) 
			g_object_set (G_OBJECT (reclist->data), 
				      "command_text", gda_command_get_text (cmd),
				      "command_type", GDA_COMMAND_TYPE_TABLE, NULL);

		g_free(query);
		query = NULL;
		break;
	case GDA_COMMAND_TYPE_XML:
	case GDA_COMMAND_TYPE_PROCEDURE:
	case GDA_COMMAND_TYPE_SCHEMA:
	case GDA_COMMAND_TYPE_INVALID:
		return reclist;
		break;
	}

	return reclist;
}

static GList *
gda_sybase_provider_process_sql_commands(GList         *reclist,
                                         GdaConnection *cnc,
                                         const gchar   *sql)
{
	GdaSybaseConnectionData *scnc;
	GdaConnectionEvent      *error;
	gchar                   **arr;
	GdaSybaseRecordset      *srecset = NULL;	
	CS_RETCODE              ret;
	CS_INT                  msgcnt = 0;


	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (scnc != NULL, NULL);
	g_return_val_if_fail (scnc->connection != NULL, NULL);

	if (scnc->cmd) {
		sybase_debug_msg(_("Cmd structure already in use. Command will not be executed because of blocked connection."));
		return NULL;
	}

	// FIXME: ; in values bug
	//        e.g. SELECT * from x where y LIKE '%foo; bar%'
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		gint n = 0;
		while (arr[n]) {
			GdaConnectionEvent *event;
			GdaDataModel *recset;

			scnc->ret = ct_cmd_alloc (scnc->connection, &scnc->cmd);
			if (scnc->ret != CS_SUCCEED) {
				sybase_debug_msg(_("Failed allocating a command structure in %s()"),
						 __FUNCTION__);
				sybase_check_messages(cnc);
				return NULL;
			}

			event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
			gda_connection_event_set_description (event, arr[n]);
			gda_connection_add_event (cnc, event);

			// execute _single_ sql query (one stmt)
			scnc->ret = ct_command (scnc->cmd, 
						CS_LANG_CMD,
						arr[n], 
						CS_NULLTERM, 
						CS_UNUSED);
			if (scnc->ret != CS_SUCCEED) {
				error = gda_sybase_make_error (scnc,
							       _("Could not prepare command structure."));
				gda_connection_add_event (cnc, error);
				ct_cmd_drop (scnc->cmd);
				scnc->cmd = NULL;
				sybase_check_messages(cnc);
				return reclist;
			}
			scnc->ret = ct_send(scnc->cmd);
			if (scnc->ret != CS_SUCCEED) {
				error = gda_sybase_make_error (scnc,
							       _("Sending sql-command failed."));
				gda_connection_add_event (cnc, error);
				ct_cmd_drop (scnc->cmd);
				scnc->cmd = NULL;
				sybase_check_messages(cnc);
				return reclist;
			}

			// part 1: process results in while-loop
			while ((scnc->rret = 
				ct_results (scnc->cmd,&scnc->res_type)) == CS_SUCCEED){
				switch (scnc->res_type) {
				case CS_ROW_RESULT:
					// the query returned a result set!
					//sybase_debug_msg (_("CS_ROW_RESULT!"));

					srecset = gda_sybase_process_row_result (cnc, 
										 scnc,
										 TRUE);
					recset = GDA_DATA_MODEL(srecset);
					if (GDA_IS_SYBASE_RECORDSET (recset)) {
						g_object_set (G_OBJECT (recset), 
							      "command_text", arr[n],
							      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
						reclist = g_list_append (reclist, recset);
					}
					else
						sybase_debug_msg (_("GDA_IS_SYBASE_RECORDSET != TRUE"));
					break;
				case CS_STATUS_RESULT:
					// when you execute a stored procedure through openclient, this 
					// part of the result set loop is where you fetch the return status
					// of the executed stored procedure.  You fetch it like a CS_ROW_RESULT
					sybase_debug_msg (_("CS_STATUS_RESULT!"));

					srecset = gda_sybase_process_row_result (cnc, 
										 scnc,
										 TRUE);
					recset = GDA_DATA_MODEL(srecset);
					if (GDA_IS_SYBASE_RECORDSET (recset)) {
						g_object_set (G_OBJECT (recset), 
							      "command_text", arr[n],
							      "command_type", GDA_COMMAND_TYPE_SQL, NULL);

						reclist = g_list_append (reclist, recset);
					}
					else
						sybase_debug_msg (_("GDA_IS_SYBASE_RECORDSET != TRUE"));
				case CS_COMPUTE_RESULT:
					// FIXME - figure this out
					break;
					// you should not hit any of these.  If you do, cancel the result set.
				case CS_CURSOR_RESULT:
				case CS_PARAM_RESULT:
				case CS_DESCRIBE_RESULT:
				case CS_ROWFMT_RESULT:
				case CS_COMPUTEFMT_RESULT:
					sybase_debug_msg (_("Hit unsupported result type"));
					
					scnc->ret = ct_cancel (NULL, scnc->cmd,
							       CS_CANCEL_CURRENT);
					break;
				case CS_CMD_DONE:
					// the command executed successfully.  
					//sybase_debug_msg (_("CS_CMD_DONE!"));
					break;
				case CS_CMD_SUCCEED:
					// this case is when you execute a query, but no rows where returned.
					//sybase_debug_msg (_("CS_CMD_SUCCEED!"));
					
					// wierd workaround...  when you use the transact sql statement
					// 'print' in the console, it does not indicate any results where returned. 
					// it used to go to CS_MSG_RESULT.  handle it here.
					
					// check the number of msgs...
					// see if there is a message
					ret = ct_diag (scnc->connection,
						       CS_STATUS, 
						       CS_ALLMSG_TYPE, 
						       CS_UNUSED,
						       &msgcnt);
					
					if ( ret != CS_SUCCEED ) {
						error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
						g_return_val_if_fail (error != NULL, FALSE);
						gda_connection_event_set_description (error, _("An error occurred when attempting to test if there is a server message for resultset"));
						gda_connection_event_set_code (error, -1);
						gda_connection_event_set_source (error, "gda-sybase");
						gda_connection_add_event (cnc, error);		
						return NULL;
					}			
					// if there are no messages, then the command didn't return any results.
					// create an empty resultset.
					if ( msgcnt == 0 ) {
						srecset = g_object_new (GDA_TYPE_SYBASE_RECORDSET, NULL);
						if ((srecset != NULL) && (srecset->priv != NULL)
						    && (srecset->priv->columns != NULL) 
						    && (srecset->priv->rows != NULL)) {
							srecset->priv->cnc = cnc;
							srecset->priv->scnc = scnc;
						}
					}
					else
						srecset = gda_sybase_process_msg_result(cnc,scnc); 

					recset = GDA_DATA_MODEL(srecset);
					if (GDA_IS_SYBASE_RECORDSET (recset)) {
						g_object_set (G_OBJECT (recset), 
							      "command_text", arr[n],
							      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
						reclist = g_list_append (reclist, recset);
					}
					else
						sybase_debug_msg (_("GDA_IS_SYBASE_RECORDSET != TRUE"));
					break;
				case CS_CMD_FAIL:
					sybase_debug_msg (_("%s returned %s"),
							  "ct_results()", "CS_CMD_FAIL");
					sybase_check_messages(cnc);
					break;
				case CS_MSG_RESULT:
					// when you use the 'print' SQL statement in a stored procedure,
					// you will hit this case. 
					//sybase_debug_msg (_("CS_MSG_RESULT!"));
		
					srecset = gda_sybase_process_msg_result(cnc,scnc);

					recset = GDA_DATA_MODEL(srecset);
					if (GDA_IS_SYBASE_RECORDSET (recset)) {
						g_object_set (G_OBJECT (recset), 
							      "command_text", arr[n],
							      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
						reclist = g_list_append (reclist, recset);
					}
					else
						sybase_debug_msg (_("GDA_IS_SYBASE_RECORDSET != TRUE"));
					break;
				default:
					break;
				} // switch
			} // while ct_results()

			scnc->ret = ct_cmd_drop (scnc->cmd);
			if (scnc->ret != CS_SUCCEED) {
				sybase_debug_msg(_("Failed dropping command structure."));
				sybase_check_messages(cnc);
			} 
			else {
				scnc->cmd = NULL;
			}
			n++;
		} // while arr[n] != NULL
		g_strfreev(arr);
	}
	return reclist;
}

// Executes a resultless sql statement
static gboolean
gda_sybase_execute_cmd (GdaConnection *cnc, const gchar *sql)
{
	GdaSybaseConnectionData *scnc;
	GdaConnectionEvent *error, *event;
	gboolean ret = TRUE;
	CS_INT res_type;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (sql != NULL, FALSE);
	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);

	g_return_val_if_fail (scnc != NULL, FALSE);
	g_return_val_if_fail (scnc->connection != NULL, FALSE);
	
	if (scnc->cmd != NULL) {
		error = gda_sybase_make_error (scnc, _("Command structure already in use. %s failed."),
					       __FUNCTION__);
		gda_connection_add_event (cnc, error);
		return FALSE;
	}

	scnc->ret = ct_cmd_alloc (scnc->connection, &scnc->cmd);
	if (scnc->ret != CS_SUCCEED) {
		error = gda_sybase_make_error (scnc, _("Could not allocate command structure."));
		gda_connection_add_event (cnc, error);
		sybase_check_messages(cnc);
		return FALSE;
	}

	event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
	gda_connection_event_set_description (event, sql);
	gda_connection_add_event (cnc, event);	

	scnc->ret = ct_command (scnc->cmd, CS_LANG_CMD, (CS_CHAR *) sql, 
				CS_NULLTERM, CS_UNUSED);
	if (scnc->ret != CS_SUCCEED) {
		error = gda_sybase_make_error (scnc, _("Could not prepare command structure with %s."), "ct_command()");
		gda_connection_add_event (cnc, error);
		ct_cmd_drop (scnc->cmd);
		sybase_check_messages(cnc);
		scnc->cmd = NULL;
		return FALSE;
	}

	scnc->ret = ct_send (scnc->cmd);
	if (scnc->ret != CS_SUCCEED) {
		error = gda_sybase_make_error (scnc, _("Sending command failed."));
		gda_connection_add_event (cnc, error);
		ct_cmd_drop (scnc->cmd);
		sybase_check_messages(cnc);
		scnc->cmd = NULL;
		return FALSE;
	}

	// proceed result codes, ct_cancel unexpected resultsets
	while ((scnc->ret = ct_results (scnc->cmd, &res_type)) == CS_SUCCEED) {
		switch (res_type) {
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			ret = FALSE;
			break;

		case CS_STATUS_RESULT:
			scnc->ret = ct_cancel (NULL, scnc->cmd,
					       CS_CANCEL_CURRENT);
			if (scnc->ret != CS_SUCCEED) {
				error = gda_sybase_make_error (
					scnc,
					_("%s: %s failed"), __FUNCTION__, "ct_cancel");
				gda_connection_add_event (cnc, error);
				sybase_check_messages(cnc);
				ret = FALSE;
			}
			break;

		default:
			ret = FALSE;
		}
		// cancel all result processing on failure
		if (!ret) {
			scnc->ret = ct_cancel (NULL, scnc->cmd,
					       CS_CANCEL_ALL);
			if (scnc->ret != CS_SUCCEED) {
				error = gda_sybase_make_error (
					scnc,
					_("%s: %s failed"), __FUNCTION__, "ct_cancel");
				gda_connection_add_event (cnc, error);
				sybase_check_messages(cnc);
			}
		}
	}
	
	if (scnc->ret == CS_END_RESULTS) {
		scnc->ret = ct_cmd_drop (scnc->cmd);
		if (scnc->ret != CS_SUCCEED) {
			error = gda_sybase_make_error (scnc,
						       _("%s: %s failed"),
						       __FUNCTION__, "ct_cmd_drop()");
			gda_connection_add_event (cnc, error);
			sybase_check_messages(cnc);
			ret = FALSE;
		} else {
			scnc->cmd = NULL;
		}
	} else {
		ct_cmd_drop (scnc->cmd);
		scnc->cmd = NULL;
		ret = FALSE;
	}

	return ret;
}

static const gchar *
gda_sybase_provider_get_database (GdaServerProvider *provider,
                                  GdaConnection *cnc)
{
	GdaSybaseConnectionData *sconn = NULL;
	CS_RETCODE ret,ret1,ret2,result_type;
	CS_CHAR current_db[256];
	CS_COMMAND     *cmd;
	CS_DATAFMT column;
	CS_INT datalen;
	CS_SMALLINT indicator;
	CS_INT count;

	g_return_val_if_fail (provider != NULL, NULL);
	g_return_val_if_fail (cnc != NULL, NULL);

	memset(current_db,'\0',256);

	sconn = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
							       OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (sconn != NULL, NULL);
	g_return_val_if_fail (sconn->connection != NULL, NULL);
	g_return_val_if_fail (sconn->context != NULL, NULL);

	ret = ct_cmd_alloc(sconn->connection, &cmd);
	
	if ( ret != CS_SUCCEED ) {
		sybase_debug_msg (_("could not allocate cmd structure to find current database."));
		return NULL;
	}
	
	ret = ct_command(cmd, 
			 CS_LANG_CMD,
			 TDS_QUERY_CURRENT_DATABASE,
			 CS_NULLTERM, 
			 CS_UNUSED); 	

	if ( ret != CS_SUCCEED ) {
		sybase_debug_msg (_("could not execute command to get current database."));
		sybase_check_messages(cnc);
		return NULL;
	}

	ret = ct_send(cmd);
	
	if ( ret != CS_SUCCEED ) {
		sybase_debug_msg (_("could not send command to get current database."));
		sybase_check_messages(cnc);
		return NULL;
	}

	while( (ret2 = ct_results(cmd, &result_type)) == CS_SUCCEED)
		{
			switch((int)result_type)
				{
				case CS_ROW_RESULT:
					column.datatype = CS_CHAR_TYPE;
					column.format = CS_FMT_NULLTERM;
					column.maxlength = 255;
					column.count = 1;
					column.locale = NULL;
					ret = ct_bind(cmd, 
						      1, 
						      &column,
						      current_db, 
						      &datalen,
						      &indicator);

					if ( ret != CS_SUCCEED ) {
						sybase_debug_msg (_("could not bind variable to get current database."));
						sybase_check_messages(cnc);
						return NULL;
					}

					ret1 = ct_fetch(cmd, 
							CS_UNUSED, 
							CS_UNUSED,
							CS_UNUSED, 
							&count);


					if ( ret != CS_SUCCEED ) {
						sybase_debug_msg (_("could not fetch data to find current database."));
						sybase_check_messages(cnc);
						return NULL;
					}

					while(ret1 == CS_SUCCEED)
						{
							ret1 = ct_fetch(cmd, 
									CS_UNUSED, 
									CS_UNUSED,
									CS_UNUSED, 
									&count);
						}

					if ( ret != CS_SUCCEED ) {
						sybase_debug_msg (_("could not fetch data to find current database."));
						sybase_check_messages(cnc);
						return NULL;
					}

					break;
				default:
					break;
				}
		}
	
	ret = ct_cmd_drop(cmd);

	if ( ret != CS_SUCCEED ) {
		sybase_debug_msg (_("could not drop cmd structure to find current database."));
		sybase_check_messages(cnc);
		return NULL;
	}

	return ( g_strdup(current_db) );
}


// FIXME : implement this
static GdaDataModel *
gda_sybase_get_fields (GdaConnection *cnc,
                       GdaParameterList *params)
{
	GdaSybaseConnectionData *scnc = NULL;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	scnc = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
							      OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (scnc != NULL, NULL);

	return NULL;
}

static GdaDataModel *
gda_sybase_provider_get_types (GdaConnection *cnc,
                               GdaParameterList *params)
{
	GdaSybaseConnectionData *scnc = NULL;
	GdaDataModelArray       *recset = NULL;
	gint i = 0;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	scnc = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
							      OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (scnc != NULL, NULL);
	
	recset = (GdaDataModelArray *) gda_data_model_array_new (4);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
					 0, _("Type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
					 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
					 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
					 3, _("GDA type"));
	
	for (i = 0; i < GDA_SYBASE_TYPE_CNT; i++) {
		if (gda_sybase_type_list[i].name != NULL) {
			GList *value_list = NULL;
			GValue *tmpval;
			
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), gda_sybase_type_list[i].name);
			value_list = g_list_append (value_list, tmpval);

			// FIXME: owner
			value_list = g_list_append (value_list, gda_value_new_null ());
			value_list = g_list_append (value_list, gda_value_new_null ());

			g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), gda_sybase_type_list[i].g_type);
			value_list = g_list_append (value_list, tmpval);
			
			gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);
			g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
			g_list_free (value_list);
		}
	}
	
	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
gda_sybase_provider_get_schema (GdaServerProvider *provider,
                                GdaConnection *cnc,
                                GdaConnectionSchema schema,
                                GdaParameterList *params)
{
	GdaSybaseProvider *syb_prov = (GdaSybaseProvider *) provider;
	GdaDataModel *recset = NULL;
	
	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (syb_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_DATABASES:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_DATABASES);
		TDS_FIXMODEL_SCHEMA_DATABASES (recset);

		return recset;
		break;
	case GDA_CONNECTION_SCHEMA_FIELDS:
		return gda_sybase_get_fields (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_PROCEDURES);
		TDS_FIXMODEL_SCHEMA_PROCEDURES (recset)
			
			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_TABLES:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_TABLES);
		TDS_FIXMODEL_SCHEMA_TABLES (recset)
			
			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_TYPES:
		recset = gda_sybase_provider_get_types (cnc, params);

		return recset;
		break;
	case GDA_CONNECTION_SCHEMA_USERS:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_USERS);
		TDS_FIXMODEL_SCHEMA_USERS (recset)
				
			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_VIEWS:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_VIEWS);
		TDS_FIXMODEL_SCHEMA_VIEWS (recset)

			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_INDEXES:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_INDEXES);
		TDS_FIXMODEL_SCHEMA_INDEXES (recset);
		
		return recset;
		break;				
	case GDA_CONNECTION_SCHEMA_TRIGGERS:
		recset = gda_sybase_execute_query (cnc, TDS_SCHEMA_TRIGGERS);
		TDS_FIXMODEL_SCHEMA_TRIGGERS (recset);
		
		return recset;
		break;				

		// FIXME: Implement aggregates, sequences 
	case GDA_CONNECTION_SCHEMA_AGGREGATES:
	case GDA_CONNECTION_SCHEMA_PARENT_TABLES:
	case GDA_CONNECTION_SCHEMA_SEQUENCES:
	case GDA_CONNECTION_SCHEMA_LANGUAGES:
	case GDA_CONNECTION_SCHEMA_NAMESPACES:
	case GDA_CONNECTION_SCHEMA_CONSTRAINTS:
	case GDA_CONNECTION_SCHEMA_TABLE_CONTENTS:
		return NULL;
		break;
	}
	
	return NULL;
}

static GdaDataModel *
gda_sybase_execute_query (GdaConnection *cnc,
                          const gchar *sql)
{
	GdaSybaseConnectionData *scnc = NULL;
	GList                   *model_list = NULL;
	GdaDataModel            *model = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sql != NULL, NULL);

	scnc = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
							      OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (scnc != NULL, NULL);

	model_list = gda_sybase_provider_process_sql_commands (NULL, cnc, sql);
	if (model_list) {
		model = GDA_DATA_MODEL (model_list->data);
		g_list_free (model_list);
	}
	return model;
}


static const gchar *
gda_sybase_provider_get_server_version (GdaServerProvider *provider,
                                        GdaConnection *cnc)
{
	GdaDataModel *model;
	GdaSybaseProvider *syb_prov = (GdaSybaseProvider *) provider;
	GdaSybaseConnectionData *scnc = NULL;

	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (syb_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	scnc = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
							      OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (scnc != NULL, NULL);
	if (!scnc->server_version) {
		model = gda_sybase_execute_query (cnc, TDS_QUERY_SERVER_VERSION);
		if (model) {
			if ((gda_data_model_get_n_columns (model) == 1)
			    && (gda_data_model_get_n_rows (model) == 1)) {
				GValue *value;
				
				value = (GValue *) gda_data_model_get_value_at (model, 
										  0, 0);
				scnc->server_version = gda_value_stringify ((GValue *) value);
			}
			g_object_unref (model);
			model = NULL;
		}
	}

	return (const gchar *) (scnc->server_version);
}

static const gchar *
gda_sybase_provider_get_version (GdaServerProvider *provider)
{
	GdaSybaseProvider *syb_prov = (GdaSybaseProvider *) provider;

	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (syb_prov), NULL);
	
	return PACKAGE_VERSION;
}

static gboolean 
gda_sybase_provider_rollback_transaction (GdaServerProvider *provider,
                                          GdaConnection *cnc,
                                          const gchar *name, GError **error)
{
	return (gda_sybase_execute_cmd (cnc, "rollback transaction"));		
}

static gboolean 
gda_sybase_provider_supports (GdaServerProvider *provider,
                              GdaConnection *cnc,
                              GdaConnectionFeature feature)
{
	GdaSybaseProvider *syb_prov = (GdaSybaseProvider *) provider;
	
	g_return_val_if_fail (GDA_IS_SYBASE_PROVIDER (syb_prov), FALSE);

	switch (feature) {
	case GDA_CONNECTION_FEATURE_PROCEDURES:
	case GDA_CONNECTION_FEATURE_USERS:
	case GDA_CONNECTION_FEATURE_VIEWS:
	case GDA_CONNECTION_FEATURE_SQL:
	case GDA_CONNECTION_FEATURE_TRANSACTIONS:
	case GDA_CONNECTION_FEATURE_INDEXES:
	case GDA_CONNECTION_FEATURE_TRIGGERS:
		return TRUE;

	case GDA_CONNECTION_FEATURE_AGGREGATES:
	case GDA_CONNECTION_FEATURE_INHERITANCE:
	case GDA_CONNECTION_FEATURE_SEQUENCES:
	case GDA_CONNECTION_FEATURE_XML_QUERIES:
	case GDA_CONNECTION_FEATURE_NAMESPACES:
	case GDA_CONNECTION_FEATURE_BLOBS:
	case GDA_CONNECTION_FEATURE_UPDATABLE_CURSOR:
		return FALSE;
	}
	
	return FALSE;
}
