/* GDA Server Library
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

#include "gda-error.h"
#include "gda-server.h"
#include "gda-server-private.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static void gda_server_connection_init       (GdaServerConnection *cnc, GdaServerConnectionClass *klass);
static void gda_server_connection_class_init (GdaServerConnectionClass *klass);
static void gda_server_connection_finalize   (GObject *object);

/*
 * Stub implementations
 */
CORBA_char *
impl_Connection__get_version (PortableServer_Servant servant,
			      CORBA_Environment * ev)
{
	return CORBA_string_dup (VERSION);
}

GNOME_Database_ErrorSeq *
impl_Connection__get_errors (PortableServer_Servant servant,
			     CORBA_Environment * ev)
{
	GdaServerConnection *cnc = GDA_SERVER_CONNECTION (bonobo_x_object (servant));

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), CORBA_OBJECT_NIL);
	return gda_error_list_to_corba_seq (cnc->errors);
}

CORBA_long
impl_Connection_beginTransaction (PortableServer_Servant servant,
				  CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), -1);

	if (gda_server_connection_begin_transaction (cnc) != -1)
		return 0;
	gda_error_list_to_exception (cnc->errors, ev);

	return -1;
}

CORBA_long
impl_Connection_commitTransaction (PortableServer_Servant servant,
				   CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), -1);

	if (gda_server_connection_commit_transaction (cnc) != -1)
		return 0;
	gda_error_list_to_exception (cnc->errors, ev);

	return -1;
}

CORBA_long
impl_Connection_rollbackTransaction (PortableServer_Servant servant,
				     CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), -1);

	if (gda_server_connection_rollback_transaction (cnc) != -1)
		return 0;
	gda_error_list_to_exception (cnc->errors, ev);

	return -1;

}

CORBA_long
impl_Connection_close (PortableServer_Servant servant,
		       CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), -1);

	gda_server_connection_close (cnc);
	gda_server_connection_free (cnc);

	bonobo_object_unref (BONOBO_OBJECT (cnc));
}

CORBA_long
impl_Connection_open (PortableServer_Servant servant,
		      CORBA_char * dsn,
		      CORBA_char * user,
		      CORBA_char * passwd,
		      CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), -1);

	if (gda_server_connection_open (cnc, dsn, user, passwd) != 0) {
		gda_error_list_to_exception (cnc->errors, ev);

		bonobo_object_unref (BONOBO_OBJECT (cnc));
		return -1;
	}

	return 0;
}

GNOME_Database_Recordset
impl_Connection_openSchema (PortableServer_Servant servant,
			    GNOME_Database_Connection_QType t,
			    GNOME_Database_Connection_ConstraintSeq * constraints,
			    CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);
	GdaServerRecordset *recset;
	GdaError *e;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), CORBA_OBJECT_NIL);

	e = gda_error_new ();
	if ((recset = gda_server_connection_open_schema (cnc, e, t,
							 constraints->_buffer,
							 constraints->_length)) == 0) {
		gda_error_to_exception (e, ev);
		gda_error_free (e);
		return CORBA_OBJECT_NIL;
	}

	/* free memory */
	gda_error_free (e);

	return bonobo_object_corba_objref (BONOBO_OBJECT (recset));;
}

CORBA_long
impl_Connection_modifySchema (PortableServer_Servant servant,
			      GNOME_Database_Connection_QType t,
			      GNOME_Database_Connection_ConstraintSeq * constraints,
			      CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), -1);

	if (gda_server_connection_modify_schema (cnc, t,
						 constraints->_buffer,
						 constraints->_length) != 0) {
		gda_error_list_to_exception (cnc->errors, ev);
		return -1;
	}

	return 0;
}

GNOME_Database_Command
impl_Connection_createCommand (PortableServer_Servant servant,
			       CORBA_Environment * ev)
{
	GdaServerCommand *cmd;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), CORBA_OBJECT_NIL);

	cmd = gda_server_command_new (cnc);
	if (!GDA_IS_SERVER_COMMAND (cmd)) {
		gda_error_list_to_exception (cnc->errors, ev);
		return CORBA_OBJECT_NIL;
	}

	return bonobo_object_corba_objref (BONOBO_OBJECT (cmd));
}

GNOME_Database_Recordset
impl_Connection_createRecordset (PortableServer_Servant servant,
				 CORBA_Environment * ev)
{
	GdaServerRecordset *recset;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), CORBA_OBJECT_NIL);

	recset = gda_server_recordset_new (cnc);
	if (!GDA_IS_SERVER_RECORDSET (recset)) {
		gda_error_list_to_exception (cnc->errors, ev);
		return CORBA_OBJECT_NIL;
	}

	return bonobo_object_corba_objref (BONOBO_OBJECT (recset));
}

CORBA_long
impl_Connection_startLogging (PortableServer_Servant servant,
			      CORBA_char * filename,
			      CORBA_Environment * ev)
{
	return -1;
}

CORBA_long
impl_Connection_stopLogging (PortableServer_Servant servant,
			     CORBA_Environment * ev)
{
	return -1;
}

CORBA_char *
impl_Connection_createTable (PortableServer_Servant servant,
			     CORBA_char * name,
			     GNOME_Database_RowAttributes * columns,
			     CORBA_Environment * ev)
{
	CORBA_char *retval;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	retval = gda_server_connection_create_table (cnc, columns);
	if (!retval)
		gda_error_list_to_exception (cnc->errors, ev);
	return retval;
}

CORBA_boolean
impl_Connection_supports (PortableServer_Servant servant,
			  GNOME_Database_Connection_Feature feature,
			  CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);
	return gda_server_connection_supports (cnc, feature);
}

CORBA_char *
impl_Connection_sql2xml (PortableServer_Servant servant,
			 CORBA_char * sql, CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);
	return gda_server_connection_sql2xml (cnc, sql);
}

CORBA_char *
impl_Connection_xml2sql (PortableServer_Servant servant,
			 CORBA_char * xml, CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);
	return gda_server_connection_xml2sql (cnc, xml);
}

void
impl_Connection_addListener (PortableServer_Servant servant,
			     GNOME_Database_Listener listener,
			     CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);
	gda_server_connection_add_listener (cnc, listener);
}

void
impl_Connection_removeListener (PortableServer_Servant servant,
				GNOME_Database_Listener listener,
				CORBA_Environment * ev)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);
	gda_server_connection_remove_listener (cnc, listener);
}

/*
 * Private functions
 */
static void
free_error_list (GList * list)
{
	GList *node;

	g_return_if_fail (list != NULL);

	while ((node = g_list_first (list))) {
		GdaError *error = (GdaError *) node->data;
		list = g_list_remove (list, (gpointer) error);
		gda_error_free (error);
	}
}

/*
 * GdaServerConnection class implementation
 */
BONOBO_TYPE_FUNC_FULL (GdaServerConnection,
		       GNOME_Database_Connection,
		       PARENT_TYPE,
		       gda_server_connection);

static void
gda_server_connection_class_init (GdaServerConnectionClass * klass)
{
	POA_GNOME_Database_Connection__epv *epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_server_connection_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->_get_version = impl_Connection__get_version;
	epv->_get_errors = impl_Connection__get_errors;
	epv->beginTransaction = impl_Connection_beginTransaction;
	epv->commitTransaction = impl_Connection_commitTransaction;
	epv->rollbackTransaction = impl_Connection_rollbackTransaction;
	epv->close = impl_Connection_close;
	epv->open = impl_Connection_open;
	epv->openSchema = impl_Connection_openSchema;
	epv->modifySchema = impl_Connection_modifySchema;
	epv->createCommand = impl_Connection_createCommand;
	epv->createRecordset = impl_Connection_createRecordset;
	epv->startLogging = impl_Connection_startLogging;
	epv->stopLogging = impl_Connection_stopLogging;
	epv->createTable = impl_Connection_createTable;
	epv->supports = impl_Connection_supports;
	epv->sql2xml = impl_Connection_sql2xml;
	epv->xml2sql = impl_Connection_xml2sql;
	epv->addListener = impl_Connection_addListener;
	epv->removeListener = impl_Connection_removeListener;
}

static void
gda_server_connection_init (GdaServerConnection *cnc, GdaServerConnectionClass *klass)
{
	cnc->dsn = NULL;
	cnc->username = NULL;
	cnc->password = NULL;
	cnc->commands = NULL;
	cnc->errors = NULL;
	cnc->listeners = NULL;
	cnc->user_data = NULL;
}

static void
gda_server_connection_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaServerConnection *cnc = (GdaServerConnection *) object;

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	if ((cnc->server_impl != NULL) &&
	    (cnc->server_impl->functions.connection_free != NULL)) {
		cnc->server_impl->functions.connection_free (cnc);
	}

	if (cnc->dsn)
		g_free ((gpointer) cnc->dsn);
	if (cnc->username)
		g_free ((gpointer) cnc->username);
	if (cnc->password)
		g_free ((gpointer) cnc->password);
	//g_list_foreach(cnc->commands, (GFunc) gda_server_command_free, NULL);
	//free_error_list(cnc->errors);

	if (cnc->listeners) {
		GList *l;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		for (l = cnc->listeners; l != NULL; l = g_list_next (l)) {
			GNOME_Database_Listener listener = (GNOME_Database_Listener) l->data;

			if (listener != CORBA_OBJECT_NIL) {
				GNOME_Database_Listener_notifyAction (
					listener,
					_("Connection being closed"),
					GNOME_Database_LISTENER_ACTION_SHUTDOWN,
					_("This connection is being closed, so all listeners are released"),
					&ev);
				//CORBA_Object_release (listener, &ev);
			}
		}

		CORBA_exception_free (&ev);
		g_list_free (cnc->listeners);
	}

	if (cnc->server_impl) {
		cnc->server_impl->connections =
			g_list_remove (cnc->server_impl->connections,
				       (gpointer) cnc);
		if (!cnc->server_impl->connections) {
			/* if no connections left, terminate */
			gda_log_message ("No connections left. Terminating");
			gda_server_stop (cnc->server_impl);
		}
	}

	parent_class = g_type_class_peek (PARENT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

GdaServerConnection *
gda_server_connection_construct (GdaServerConnection * cnc,
				 GdaServer * server_impl)
{
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_SERVER (server_impl), cnc);

	cnc->server_impl = server_impl;

	/* notify 'real' provider */
	cnc->server_impl->connections = g_list_append (cnc->server_impl->connections, (gpointer) cnc);
	if (cnc->server_impl->functions.connection_new != NULL) {
		cnc->server_impl->functions.connection_new (cnc);
	}

	return cnc;
}

/**
 * gda_server_connection_new
 */
GdaServerConnection *
gda_server_connection_new (GdaServer * server_impl)
{
	GdaServerConnection *cnc;

	g_return_val_if_fail (server_impl != NULL, NULL);

	cnc = GDA_SERVER_CONNECTION (g_object_new (GDA_TYPE_SERVER_CONNECTION, NULL));
	return gda_server_connection_construct (cnc, server_impl);
}

/**
 * gda_server_connection_open
 */
gint
gda_server_connection_open (GdaServerConnection * cnc,
			    const gchar * dsn,
			    const gchar * user, const gchar * password)
{
	gint rc;

	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (dsn != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_open != NULL, -1);

	rc = cnc->server_impl->functions.connection_open (cnc, dsn, user, password);
	if (rc != -1) {
		gda_server_connection_set_dsn (cnc, dsn);
		gda_server_connection_set_username (cnc, user);
		gda_server_connection_set_password (cnc, password);
		rc = 0;
	}
	return rc;
}

/**
 * gda_server_connection_close
 */
void
gda_server_connection_close (GdaServerConnection * cnc)
{
	g_return_if_fail (cnc != NULL);
	g_return_if_fail (cnc->server_impl != NULL);
	g_return_if_fail (cnc->server_impl->functions.connection_close != NULL);

	cnc->server_impl->functions.connection_close (cnc);
}

/**
 * gda_server_connection_begin_transaction
 */
gint
gda_server_connection_begin_transaction (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_begin_transaction != NULL, -1);

	return cnc->server_impl->functions.connection_begin_transaction (cnc);
}

/**
 * gda_server_connection_commit_transaction
 */
gint
gda_server_connection_commit_transaction (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_commit_transaction != NULL, -1);

	return cnc->server_impl->functions.connection_commit_transaction (cnc);
}

/**
 * gda_server_connection_rollback_transaction
 */
gint
gda_server_connection_rollback_transaction (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_rollback_transaction != NULL, -1);

	return cnc->server_impl->functions.connection_rollback_transaction (cnc);
}

/**
 * gda_server_connection_open_schema
 */
GdaServerRecordset *
gda_server_connection_open_schema (GdaServerConnection * cnc,
				   GdaError * error,
				   GNOME_Database_Connection_QType t,
				   GNOME_Database_Connection_Constraint * constraints,
				   gint length)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	g_return_val_if_fail (cnc->server_impl != NULL, NULL);
	g_return_val_if_fail (cnc->server_impl->functions.connection_open_schema != NULL, NULL);

	return cnc->server_impl->functions.connection_open_schema (cnc, error,
								   t,
								   constraints,
								   length);
}

/**
 * gda_server_connection_modify_schema
 */
glong
gda_server_connection_modify_schema (GdaServerConnection * cnc,
				     GNOME_Database_Connection_QType t,
				     GNOME_Database_Connection_Constraint * constraints,
				     gint length)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_modify_schema != NULL, -1);

	return cnc->server_impl->functions.connection_modify_schema (cnc, t,
								     constraints,
								     length);
}

/**
 * gda_server_connection_start_logging
 */
gint
gda_server_connection_start_logging (GdaServerConnection * cnc,
				     const gchar * filename)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_start_logging != NULL, -1);

	return cnc->server_impl->functions.connection_start_logging (cnc,
								     filename);
}

/**
 * gda_server_connection_stop_logging
 */
gint
gda_server_connection_stop_logging (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_stop_logging != NULL, -1);

	return cnc->server_impl->functions.connection_stop_logging (cnc);
}

/**
 * gda_server_connection_create_table
 */
gchar *
gda_server_connection_create_table (GdaServerConnection * cnc,
				    GNOME_Database_RowAttributes * columns)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	g_return_val_if_fail (cnc->server_impl != NULL, NULL);
	g_return_val_if_fail (cnc->server_impl->functions.connection_create_table != NULL, NULL);
	g_return_val_if_fail (columns != NULL, NULL);

	return cnc->server_impl->functions.connection_create_table (cnc,
								    columns);
}

/**
 * gda_server_connection_supports
 */
gboolean
gda_server_connection_supports (GdaServerConnection * cnc,
				GNOME_Database_Connection_Feature feature)
{
	g_return_val_if_fail (cnc != NULL, FALSE);
	g_return_val_if_fail (cnc->server_impl != NULL, FALSE);
	g_return_val_if_fail (cnc->server_impl->functions.connection_supports != NULL, FALSE);

	return cnc->server_impl->functions.connection_supports (cnc, feature);
}

/**
 * gda_server_connection_get_gda_type
 */
GNOME_Database_ValueType
gda_server_connection_get_gda_type (GdaServerConnection * cnc,
				    gulong sql_type)
{
	g_return_val_if_fail (cnc != NULL, GNOME_Database_TypeNull);
	g_return_val_if_fail (cnc->server_impl != NULL, GNOME_Database_TypeNull);
	g_return_val_if_fail (
		cnc->server_impl->functions.connection_get_gda_type != NULL,
		GNOME_Database_TypeNull);

	return cnc->server_impl->functions.connection_get_gda_type (cnc,
								    sql_type);
}

/**
 * gda_server_connection_get_c_type
 */
gshort
gda_server_connection_get_c_type (GdaServerConnection * cnc,
				  GNOME_Database_ValueType type)
{
	g_return_val_if_fail (cnc != NULL, -1);
	g_return_val_if_fail (cnc->server_impl != NULL, -1);
	g_return_val_if_fail (cnc->server_impl->functions.connection_get_c_type != NULL, -1);

	return cnc->server_impl->functions.connection_get_c_type (cnc, type);
}

/**
 * gda_server_connection_sql2xml
 */
gchar *
gda_server_connection_sql2xml (GdaServerConnection * cnc, const gchar * sql)
{
	g_return_if_fail (cnc != NULL);
	g_return_if_fail (cnc->server_impl != NULL);
	g_return_if_fail (cnc->server_impl->functions.connection_sql2xml != NULL);

	return cnc->server_impl->functions.connection_sql2xml (cnc, sql);
}

/**
 * gda_server_connection_xml2sql
 */
gchar *
gda_server_connection_xml2sql (GdaServerConnection * cnc, const gchar * xml)
{
	g_return_if_fail (cnc != NULL);
	g_return_if_fail (cnc->server_impl != NULL);
	g_return_if_fail (cnc->server_impl->functions.connection_xml2sql != NULL);

	return cnc->server_impl->functions.connection_xml2sql (cnc, xml);
}

/**
 * gda_server_connection_get_dsn
 */
gchar *
gda_server_connection_get_dsn (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	return cnc->dsn;
}

/**
 * gda_server_connection_set_dsn
 */
void
gda_server_connection_set_dsn (GdaServerConnection * cnc, const gchar * dsn)
{
	g_return_if_fail (cnc != NULL);

	if (cnc->dsn)
		g_free ((gpointer) cnc->dsn);
	if (dsn)
		cnc->dsn = g_strdup (dsn);
	else
		cnc->dsn = NULL;
}

/**
 * gda_server_connection_get_username
 */
gchar *
gda_server_connection_get_username (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	return cnc->username;
}

/**
 * gda_server_connection_set_username
 */
void
gda_server_connection_set_username (GdaServerConnection * cnc,
				    const gchar * username)
{
	g_return_if_fail (cnc != NULL);

	if (cnc->username)
		g_free ((gpointer) cnc->username);
	if (username)
		cnc->username = g_strdup (username);
	else
		cnc->username = NULL;
}

/**
 * gda_server_connection_get_password
 */
gchar *
gda_server_connection_get_password (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	return cnc->password;
}

/**
 * gda_server_connection_set_password
 */
void
gda_server_connection_set_password (GdaServerConnection * cnc,
				    const gchar * password)
{
	g_return_if_fail (cnc != NULL);

	if (cnc->password)
		g_free ((gpointer) cnc->password);
	if (password)
		cnc->password = g_strdup (password);
	else
		cnc->password = NULL;
}

/**
 * gda_server_connection_add_error
 */
void
gda_server_connection_add_error (GdaServerConnection * cnc, GdaError * error)
{
	g_return_if_fail (cnc != NULL);
	g_return_if_fail (error != NULL);

	cnc->errors = g_list_append (cnc->errors, (gpointer) error);
}

/**
 * gda_server_connection_add_error_string
 * @cnc: connection
 * @msg: error message
 *
 * Adds a new error to the given connection from a given error string
 */
void
gda_server_connection_add_error_string (GdaServerConnection * cnc,
					const gchar * msg)
{
	GdaError *error;

	g_return_if_fail (cnc != NULL);
	g_return_if_fail (msg != NULL);

	error = gda_error_new ();
	gda_server_connection_make_error (error, NULL, cnc, __PRETTY_FUNCTION__);
	gda_error_set_description (error, msg);
	gda_error_set_native (error, msg);

	cnc->errors = g_list_append (cnc->errors, (gpointer) error);
}

/**
 * gda_server_connection_make_error
 */
void
gda_server_connection_make_error (GdaError *error,
				  GdaServerRecordset *recset,
				  GdaServerConnection *cnc,
				  gchar *where)
{
	GdaServerConnection *cnc_to_use = NULL;

	g_return_if_fail (error != NULL);

	if (cnc)
		cnc_to_use = cnc;
	else if (recset)
		cnc_to_use = recset->cnc;

	if (!cnc_to_use) {
		gda_log_message (_("Could not get pointer to server implementation"));
		return;
	}

	g_return_if_fail (cnc_to_use->server_impl != NULL);
	g_return_if_fail (cnc_to_use->server_impl->functions.error_make != NULL);

	cnc_to_use->server_impl->functions.error_make (error, recset, cnc,
						       where);

	gda_server_connection_add_error (cnc_to_use, error);
}

/**
 * gda_server_connection_get_user_data
 */
gpointer
gda_server_connection_get_user_data (GdaServerConnection * cnc)
{
	g_return_val_if_fail (cnc != NULL, NULL);
	return cnc->user_data;
}

/**
 * gda_server_connection_set_user_data
 */
void
gda_server_connection_set_user_data (GdaServerConnection * cnc,
				     gpointer user_data)
{
	g_return_if_fail (cnc != NULL);
	cnc->user_data = user_data;
}

/**
 * gda_server_connection_free
 */
void
gda_server_connection_free (GdaServerConnection * cnc)
{
	bonobo_object_unref (BONOBO_OBJECT (cnc));
}

/**
 * gda_server_connection_add_listener
 */
void
gda_server_connection_add_listener (GdaServerConnection * cnc,
				    GNOME_Database_Listener listener)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));
	g_return_if_fail (listener != CORBA_OBJECT_NIL);

	cnc->listeners = g_list_append (cnc->listeners, listener);
}

/**
 * gda_server_connection_remove_listener
 */
void
gda_server_connection_remove_listener (GdaServerConnection * cnc,
				       GNOME_Database_Listener listener)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));
	g_return_if_fail (listener != CORBA_OBJECT_NIL);

	cnc->listeners = g_list_remove (cnc->listeners, listener);
	// CORBA_Object_release (listener, &ev);
}
