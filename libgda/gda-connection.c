/* GDA library
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

#include <bonobo/bonobo-i18n.h>
#include <libgda/gda-client.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-log.h>
#include <libgda/gda-recordset.h>
#include <bonobo/bonobo-exception.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaConnectionPrivate {
	GdaClient *client;
	GNOME_Database_Connection corba_cnc;
	gchar *dsn;
	gchar *username;
	gchar *password;
	gboolean is_open;
	GList *error_list;
	GList *recset_list;
};

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc, GdaConnectionClass *klass);
static void gda_connection_finalize   (GObject *object);

enum {
	ERROR,
	LAST_SIGNAL
};

static gint gda_connection_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

static void
recset_weak_cb (gpointer user_data, GObject *object)
{
	GdaRecordset *recset = (GdaRecordset *) object;
	GdaConnection *cnc = (GdaConnection *) user_data;

	g_return_if_fail (GDA_IS_RECORDSET (recset));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	cnc->priv->recset_list = g_list_remove (cnc->priv->recset_list, recset);
}

/*
 * GdaConnection class implementation
 */

static void
gda_connection_class_init (GdaConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_connection_signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	object_class->finalize = gda_connection_finalize;
}

static void
gda_connection_init (GdaConnection *cnc, GdaConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaConnectionPrivate, 1);
	cnc->priv->client = NULL;
	cnc->priv->corba_cnc = CORBA_OBJECT_NIL;
	cnc->priv->dsn = NULL;
	cnc->priv->username = NULL;
	cnc->priv->password = NULL;
	cnc->priv->is_open = FALSE;
	cnc->priv->error_list = NULL;
	cnc->priv->recset_list = NULL;
}

static void
gda_connection_finalize (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	if (cnc->priv->is_open) {
		CORBA_Environment ev;

		/* close the connection to the provider */
		CORBA_exception_init (&ev);
		GNOME_Database_Connection_close (
			cnc->priv->corba_cnc,
			bonobo_object_corba_objref (BONOBO_OBJECT (cnc->priv->client)),
			&ev);
		CORBA_Object_release (cnc->priv->corba_cnc, &ev);
		CORBA_exception_free (&ev);

		cnc->priv->corba_cnc = CORBA_OBJECT_NIL;
	}

	g_free (cnc->priv->dsn);
	g_free (cnc->priv->username);
	g_free (cnc->priv->password);

	g_list_foreach (cnc->priv->error_list, (GFunc) gda_error_free, NULL);
	g_list_free (cnc->priv->error_list);

	g_list_foreach (cnc->priv->recset_list, (GFunc) g_object_unref, NULL);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_connection_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaConnectionClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_connection_class_init,
				NULL, NULL,
				sizeof (GdaConnection),
				0,
				(GInstanceInitFunc) gda_connection_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaConnection", &info, 0);
		}
	}

	return type;
}

/**
 * gda_connection_new
 * @client: a #GdaClient object.
 * @corba_cnc: a CORBA GNOME::Database::Connection object.
 * @dsn: GDA data source to connect to.
 * @username: user name to use to connect.
 * @password: password for @username.
 *
 * This function creates a new #GdaConnection object. It is not
 * intended to be used directly by applications (use
 * #gda_client_open_connection instead).
 *
 * Returns: a newly allocated #GdaConnection object.
 */
GdaConnection *
gda_connection_new (GdaClient *client,
		    GNOME_Database_Connection corba_cnc,
		    const gchar *dsn,
		    const gchar *username,
		    const gchar *password)
{
	GdaConnection *cnc;
	CORBA_Environment ev;
	CORBA_boolean corba_res;
	GdaDataSourceInfo *dsn_info;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	g_return_val_if_fail (corba_cnc != CORBA_OBJECT_NIL, NULL);

	/* get the data source info */
	dsn_info = gda_config_find_data_source (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	cnc = g_object_new (GDA_TYPE_CONNECTION, NULL);

	gda_connection_set_client (cnc, client);
	cnc->priv->corba_cnc = corba_cnc;
	cnc->priv->dsn = g_strdup (dsn);
	cnc->priv->username = g_strdup (username);
	cnc->priv->password = g_strdup (password);

	/* try to open the connection */
	CORBA_exception_init (&ev);

	corba_res = GNOME_Database_Connection_open (
		cnc->priv->corba_cnc,
		bonobo_object_corba_objref (BONOBO_OBJECT (cnc->priv->client)),
		(const CORBA_char *) dsn_info->cnc_string,
		(const CORBA_char *) cnc->priv->username,
		(const CORBA_char *) cnc->priv->password, &ev);
	gda_config_free_data_source_info (dsn_info);

	if (!corba_res || BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		g_object_unref (G_OBJECT (cnc));
		return NULL;
	}

	cnc->priv->is_open = TRUE;

	return cnc;
}

/**
 * gda_connection_close
 */
gboolean
gda_connection_close (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	g_object_unref (G_OBJECT (cnc));
	return TRUE;
}

gboolean
gda_connection_is_open (GdaConnection *cnc)
{
        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return cnc->priv->is_open;
}

/**
 * gda_connection_get_client
 */
GdaClient *
gda_connection_get_client (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return cnc->priv->client;
}

/**
 * gda_connection_set_client
 */
void
gda_connection_set_client (GdaConnection *cnc, GdaClient *client)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	cnc->priv->client = client;
}

/**
 * gda_connection_get_dsn
 * @cnc: A #GdaConnection object
 *
 * Returns the data source name the connection object is connected
 * to.
 */
const gchar *
gda_connection_get_dsn (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->dsn;
}

/**
 * gda_connection_get_username
 */
const gchar *
gda_connection_get_username (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->username;
}

/**
 * gda_connection_get_password
 */
const gchar *
gda_connection_get_password (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->password;
}

/**
 * gda_connection_add_error
 */
void
gda_connection_add_error (GdaConnection *cnc, GdaError *error)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_ERROR (error));

	cnc->priv->error_list = g_list_append (cnc->priv->error_list, error);

	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, cnc->priv->error_list);
	cnc->priv->error_list = NULL;
}

/**
 * gda_connection_add_error_list
 */
void
gda_connection_add_error_list (GdaConnection *cnc, GList *error_list)
{
	GList *l;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (error_list != NULL);

	for (l = error_list; l; l = l->next) {
		GdaError *error = GDA_ERROR (l->data);
		cnc->priv->error_list = g_list_append (cnc->priv->error_list, error);
	}

	/* notify errors */
	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, cnc->priv->error_list);
	cnc->priv->error_list = NULL;

	g_list_free (error_list);
}

/**
 * gda_connection_execute_command
 */
GList *
gda_connection_execute_command (GdaConnection *cnc,
				GdaCommand *cmd,
				GdaParameterList *params)
{
	GNOME_Database_RecordsetList *corba_reclist;
	GNOME_Database_ParameterList *corba_params;
	CORBA_Environment ev;
	GList *reclist = NULL;
	gint n;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	corba_params = gda_parameter_list_to_corba (params);

	/* call the CORBA method */
	CORBA_exception_init (&ev);

	corba_reclist = GNOME_Database_Connection_executeCommand (
		cnc->priv->corba_cnc, cmd, corba_params, &ev);

	CORBA_free (corba_params);

	if (BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);

		return NULL;
	}

	/* convert the RecordsetList in a GList of GdaRecordset objects */
	for (n = 0; n < corba_reclist->_length; n++) {
		GdaRecordset *recset;

		recset = gda_recordset_new (cnc, corba_reclist->_buffer[n]);
		if (!GDA_IS_RECORDSET (recset))
			continue;

		reclist = g_list_append (reclist, recset);

		cnc->priv->recset_list = g_list_append (cnc->priv->recset_list, recset);
		g_object_weak_ref (G_OBJECT (recset), (GWeakNotify) recset_weak_cb, cnc);
	}

	return reclist;
}

/**
 * gda_connection_begin_transaction
 */
gboolean
gda_connection_begin_transaction (GdaConnection *cnc, const gchar *id)
{
	CORBA_boolean corba_res;
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	CORBA_exception_init (&ev);

	corba_res = GNOME_Database_Connection_beginTransaction (
		cnc->priv->corba_cnc, id ? id : "", &ev);
	if (!corba_res || BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

/**
 * gda_connection_commit_transaction
 */
gboolean
gda_connection_commit_transaction (GdaConnection *cnc, const gchar *id)
{
	CORBA_boolean corba_res;
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	CORBA_exception_init (&ev);

	corba_res = GNOME_Database_Connection_commitTransaction (
		cnc->priv->corba_cnc, id ? id : "", &ev);
	if (!corba_res || BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

/**
 * gda_connection_rollback_transaction
 */
gboolean
gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *id)
{
	CORBA_boolean corba_res;
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	CORBA_exception_init (&ev);

	corba_res = GNOME_Database_Connection_rollbackTransaction (
		cnc->priv->corba_cnc, id ? id : "", &ev);
	if (!corba_res || BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

/**
 * gda_connection_supports
 */
gboolean
gda_connection_supports (GdaConnection *cnc, GdaConnectionFeature feature)
{
	CORBA_Environment ev;
	CORBA_boolean corba_res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	CORBA_exception_init (&ev);

	corba_res = GNOME_Database_Connection_supports (cnc->priv->corba_cnc,
							feature, &ev);
	if (BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return corba_res;
}

/**
 * gda_connection_get_schema
 */
GdaDataModel *
gda_connection_get_schema (GdaConnection *cnc,
			   GdaConnectionSchema schema,
			   GdaParameterList *params)
{
	GNOME_Database_ParameterList *corba_params;
	GNOME_Database_Recordset corba_recset;
	GdaRecordset *recset;
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	corba_params = gda_parameter_list_to_corba (params);

	/* call the CORBA method */
	CORBA_exception_init (&ev);

	corba_recset = GNOME_Database_Connection_getSchema (cnc->priv->corba_cnc,
							    schema,
							    corba_params,
							    &ev);
	CORBA_free (corba_params);

	if (BONOBO_EX (&ev)) {
		gda_connection_add_error_list (cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	recset = gda_recordset_new (cnc, corba_recset);

	return GDA_DATA_MODEL (recset);
}
