/* GDA server library
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

#include "gda-server-connection.h"
#include "gda-server-provider.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE
#define CLASS(cnc) (GDA_SERVER_CONNECTION_CLASS (G_OBJECT_GET_CLASS (cnc)))

struct _GdaServerConnectionPrivate {
	GdaServerProvider *provider;
	GList *errors;
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
		      const CORBA_char *cnc_string,
		      const CORBA_char *username,
		      const CORBA_char *password,
		      CORBA_Environment *ev)
{
	gboolean result;
	GdaParameterList *params;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	params = gda_parameter_list_new_from_string (cnc_string);
	result = gda_server_provider_open_connection (cnc->priv->provider,
						      cnc, params, username, password);
	if (!result)
		gda_error_list_to_exception (cnc->priv->errors, ev);
	gda_server_connection_free_error_list (cnc);
	gda_parameter_list_free (params);

	return result;
}

static CORBA_boolean
impl_Connection_close (PortableServer_Servant servant, CORBA_Environment *ev)
{
	gboolean result;
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	result = gda_server_provider_close_connection (cnc->priv->provider, cnc);
	if (!result)
		gda_error_list_to_exception (cnc->priv->errors, ev);
	gda_server_connection_free_error_list (cnc);

	return result;
}

static GNOME_Database_RecordsetList
impl_Connection_executeCommand (PortableServer_Servant servant,
				GNOME_Database_Command *cmd,
				GNOME_Database_ParameterList *params)
{
	GdaServerConnection *cnc = (GdaServerConnection *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
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
	epv->open = impl_Connection_open;
	epv->close = impl_Connection_close;
	epv->executeCommand = impl_Connection_executeCommand;
	epv->beginTransaction = impl_Connection_beginTransaction;
	epv->commitTransaction = impl_Connection_commitTransaction;
	epv->rollbackTransaction = impl_Connection_rollbackTransaction;
}

static void
gda_server_connection_init (GdaServerConnection *cnc, GdaServerConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaServerConnectionPrivate, 1);
	cnc->priv->provider = NULL;
	cnc->priv->errors = NULL;
}

static void
gda_server_connection_finalize (GObject *object)
{
	GdaServerConnection *cnc = (GdaServerConnection *) object;

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	/* free memory */
	gda_error_list_free (cnc->priv->errors);
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
 * gda_server_connection_free_error_list
 */
void
gda_server_connection_free_error_list (GdaServerConnection *cnc)
{
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	gda_error_list_free (cnc->priv->errors);
	cnc->priv->errors = NULL;
}
