/* GDA library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <libgda/gda-server-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-recordset.h>
#include <bonobo/bonobo-exception.h>

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaServerConnectionPrivate {
	GdaServerProvider *provider;
	GList *errors;
	GList *clients;
	gboolean is_open;
};

static void gda_server_connection_class_init (GdaServerConnectionClass *klass);
static void gda_server_connection_init       (GdaServerConnection *cnc,
					      GdaServerConnectionClass *klass);
static void gda_server_connection_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * CORBA methods implementation
 */

static CORBA_boolean
impl_Connection_open (PortableServer_Servant servant,
		      GNOME_Database_Client client,
		      const CORBA_char *cnc_string,
		      const CORBA_char *username,
		      const CORBA_char *password,
		      CORBA_Environment *ev)
{
	gboolean result;
	GdaQuarkList *params;
	GNOME_Database_Client client_copy;
	CORBA_Environment ev2;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	if (!cnc->priv->is_open) {
		params = gda_quark_list_new_from_string (cnc_string);
		result = gda_server_provider_open_connection (cnc->priv->provider,
							      cnc, params,
							      username, password);
		if (!result) {
			gda_error_list_to_exception (cnc->priv->errors, ev);
			gda_server_connection_free_error_list (cnc);
			gda_quark_list_free (params);
			return FALSE;
		}
		cnc->priv->is_open = TRUE;
	}

	result = TRUE;

	/* add the new client to our list */
	CORBA_exception_init (&ev2);
	client_copy = CORBA_Object_duplicate (client, &ev2);
	if (BONOBO_EX (&ev2))
		gda_log_error (_("Could not duplicate client object. Client won't get notifications"));
	else {
		cnc->priv->clients = g_list_append (cnc->priv->clients, client_copy);
		gda_server_connection_notify_action (
			cnc,
			GNOME_Database_ACTION_CLIENT_CONNECTED,
			NULL);
	}

	return result;
}

static CORBA_boolean
impl_Connection_close (PortableServer_Servant servant,
		       GNOME_Database_Client client,
		       CORBA_Environment *ev)
{
	gboolean result;
	GList *l;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	/* remove the client from our list */
	for (l = g_list_first (cnc->priv->clients); l; l = l->next) {
		GNOME_Database_Client tmp_client;
		gboolean is_equal;
		CORBA_Environment ev2;

		CORBA_exception_init (&ev2);

		tmp_client = (GNOME_Database_Client) l->data;
		is_equal = CORBA_Object_is_equivalent (tmp_client, client, &ev2);
		if (!BONOBO_EX (&ev2) && is_equal) {
			cnc->priv->clients = g_list_remove (cnc->priv->clients, tmp_client);
			bonobo_object_release_unref (tmp_client, NULL);
			gda_server_connection_notify_action (
				cnc,
				GNOME_Database_ACTION_CLIENT_EXITED,
				NULL);
			break;
		}
	}

	if (g_list_length (cnc->priv->clients) > 0)
		return TRUE;

	result = gda_server_provider_close_connection (cnc->priv->provider, cnc);
	if (!result)
		gda_error_list_to_exception (cnc->priv->errors, ev);
	gda_server_connection_free_error_list (cnc);

	return result;
}

static GNOME_Database_RecordsetList *
impl_Connection_executeCommand (PortableServer_Servant servant,
				const GNOME_Database_Command *cmd,
				const GNOME_Database_ParameterList *params,
				CORBA_Environment *ev)
{
	GList *recset_list;
	GList *l;
	gint count;
	GNOME_Database_RecordsetList *seq;
	GdaParameterList *param_list = NULL;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	recset_list = gda_server_provider_execute_command (cnc->priv->provider, cnc,
							   (GdaCommand *) cmd, param_list);
	if (!recset_list) {
		gda_error_list_to_exception (cnc->priv->errors, ev);
		gda_server_connection_free_error_list (cnc);
		return NULL;
	}

	seq = GNOME_Database_RecordsetList__alloc ();
	CORBA_sequence_set_release (seq, TRUE);
	count = g_list_length (recset_list);
	seq->_length = count;
	seq->_buffer = GNOME_Database_RecordsetList_allocbuf (count);

	for (count = 0, l = recset_list; l; count++, l = l->next) {
		GdaServerRecordset *recset = GDA_SERVER_RECORDSET (l->data);

		seq->_buffer[count] = bonobo_object_corba_objref (BONOBO_OBJECT (recset));
	}

	g_list_free (recset_list);

	return seq;
}

static CORBA_boolean
impl_Connection_beginTransaction (PortableServer_Servant servant,
				  GNOME_Database_TransactionId trans_id,
				  CORBA_Environment *ev)
{
	gboolean result;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	result = gda_server_provider_begin_transaction (cnc->priv->provider, cnc, trans_id);
	if (!result)
		gda_error_list_to_exception (cnc->priv->errors, ev);
	else {
		gda_server_connection_notify_action (
			cnc,
			GNOME_Database_ACTION_TRANSACTION_STARTED,
			NULL);
	}

	gda_server_connection_free_error_list (cnc);

	return result;
}

static CORBA_boolean
impl_Connection_commitTransaction (PortableServer_Servant servant,
				   GNOME_Database_TransactionId trans_id,
				   CORBA_Environment *ev)
{
	gboolean result;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	result = gda_server_provider_commit_transaction (cnc->priv->provider, cnc, trans_id);
	if (!result)
		gda_error_list_to_exception (cnc->priv->errors, ev);
	else {
		gda_server_connection_notify_action (
			cnc,
			GNOME_Database_ACTION_TRANSACTION_FINISHED,
			NULL);
	}
	gda_server_connection_free_error_list (cnc);

	return result;
}

static CORBA_boolean
impl_Connection_rollbackTransaction (PortableServer_Servant servant,
				     GNOME_Database_TransactionId trans_id,
				     CORBA_Environment *ev)
{
	gboolean result;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	result = gda_server_provider_rollback_transaction (cnc->priv->provider, cnc, trans_id);
	if (!result)
		gda_error_list_to_exception (cnc->priv->errors, ev);
	else {
		gda_server_connection_notify_action (
			cnc,
			GNOME_Database_ACTION_TRANSACTION_ABORTED,
			NULL);
	}
	gda_server_connection_free_error_list (cnc);

	return result;
}

/*
 * GdaServerConnection class implementation
 */

static void
gda_server_connection_class_init (GdaServerConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Database_Connection__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_connection_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->open = (gpointer) impl_Connection_open;
	epv->close = (gpointer) impl_Connection_close;
	epv->executeCommand = (gpointer) impl_Connection_executeCommand;
	epv->beginTransaction = (gpointer) impl_Connection_beginTransaction;
	epv->commitTransaction = (gpointer) impl_Connection_commitTransaction;
	epv->rollbackTransaction = (gpointer) impl_Connection_rollbackTransaction;
}

static void
gda_server_connection_init (GdaServerConnection *cnc, GdaServerConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaServerConnectionPrivate, 1);
	cnc->priv->provider = NULL;
	cnc->priv->errors = NULL;
	cnc->priv->clients = NULL;
	cnc->priv->is_open = FALSE;
}

static void
gda_server_connection_finalize (GObject *object)
{
	GList *l;
	GdaServerConnection *cnc = (GdaServerConnection *) object;

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	/* free memory */
	gda_error_list_free (cnc->priv->errors);

	for (l = g_list_first (cnc->priv->clients); l; l = l->next) {
		GNOME_Database_Client client;

		client = (GNOME_Database_Client) l->data;
		if (client)
			bonobo_object_release_unref (client, NULL);
	}
	g_list_free (cnc->priv->clients);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_X_TYPE_FUNC_FULL (GdaServerConnection,
			 GNOME_Database_Connection,
			 PARENT_TYPE,
			 gda_server_connection)

/**
 * gda_server_connection_new
 */
GdaServerConnection *
gda_server_connection_new (GdaServerProvider *provider)
{
	GdaServerConnection *cnc;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	cnc = g_object_new (GDA_TYPE_SERVER_CONNECTION, NULL);
	cnc->priv->provider = provider;

	return cnc;
}

/**
 * gda_server_connection_notify_action
 */
void
gda_server_connection_notify_action (GdaServerConnection *cnc,
				     GNOME_Database_ActionId action,
				     GdaParameterList *params)
{
	GList *l;

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	for (l = g_list_first (cnc->priv->clients); l; l = l->next) {
		GNOME_Database_Client client;

		client = (GNOME_Database_Client) l->data;
		if (client != CORBA_OBJECT_NIL) {
			CORBA_Environment ev;

			CORBA_exception_init (&ev);
			GNOME_Database_Client_notifyAction (client, action, NULL, &ev);
			if (BONOBO_EX (&ev)) {
				gda_log_error (_("Could not notify client about action %d"),
					       action);
			}
			CORBA_exception_free (&ev);
		}
	}
}

/**
 * gda_server_connection_add_error
 */
void
gda_server_connection_add_error (GdaServerConnection *cnc, GdaError *error)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_ERROR (error));

	cnc->priv->errors = g_list_append (cnc->priv->errors, error);
}

/**
 * gda_server_connection_add_error_string
 */
void
gda_server_connection_add_error_string (GdaServerConnection *cnc, const gchar *msg)
{
}

/**
 * gda_server_connection_free_error_list
 */
void
gda_server_connection_free_error_list (GdaServerConnection *cnc)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	gda_error_list_free (cnc->priv->errors);
	cnc->priv->errors = NULL;
}
