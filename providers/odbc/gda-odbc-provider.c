/* GNOME DB ODBC Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Michael Lausch <michael@lausch.at>
 *         Nick Gorham <nick@lurcher.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#include "gda-odbc.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_ODBC_HANDLE "GDA_ODBC_ODBCHandle"

typedef struct {
	SQLHENV henv;
        SQLHDBC hdbc;
} GdaOdbcConnectionPrivate;

static void gda_odbc_provider_class_init (GdaOdbcProviderClass *klass);
static void gda_odbc_provider_init       (GdaOdbcProvider *provider,
					  GdaOdbcProviderClass *klass);
static void gda_odbc_provider_finalize   (GObject *object);

static gboolean gda_odbc_provider_open_connection (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaQuarkList *params,
						   const gchar *username,
						   const gchar *password);
static gboolean gda_odbc_provider_close_connection (GdaServerProvider *provider,
						    GdaConnection *cnc);
static GList *gda_odbc_provider_execute_command (GdaServerProvider *provider,
						 GdaConnection *cnc,
						 GdaCommand *cmd,
						 GdaParameterList *params);

static GObjectClass *parent_class = NULL;

/*
 * GdaOdbcProvider class implementation
 */

static void
gda_odbc_provider_class_init (GdaOdbcProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_odbc_provider_finalize;
	provider_class->open_connection = gda_odbc_provider_open_connection;
	provider_class->close_connection = gda_odbc_provider_close_connection;
	provider_class->execute_command = gda_odbc_provider_execute_command;
}

static void
gda_odbc_provider_init (GdaOdbcProvider *provider, GdaOdbcProviderClass *klass)
{
}

static void
gda_odbc_provider_finalize (GObject *object)
{
	GdaOdbcProvider *provider = (GdaOdbcProvider *) object;

	g_return_if_fail (GDA_IS_ODBC_PROVIDER (provider));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_odbc_provider_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static GTypeInfo info = {
                        sizeof (GdaOdbcProviderClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_odbc_provider_class_init,
                        NULL, NULL,
                        sizeof (GdaOdbcProvider),
                        0,
                        (GInstanceInitFunc) gda_odbc_provider_init
                };
                type = g_type_register_static (PARENT_TYPE, "GdaOdbcProvider", &info, 0);
        }

        return type;
}

GdaServerProvider *
gda_odbc_provider_new (void)
{
	GdaServerProvider *provider;

	provider = g_object_new (gda_odbc_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* open_connection handler for the GdaOdbcProvider class */
static gboolean
gda_odbc_provider_open_connection (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaQuarkList *params,
				   const gchar *username,
				   const gchar *password)
{
	GdaOdbcConnectionPrivate *priv_data;
	const gchar *odbc_string;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	odbc_string = gda_quark_list_find (params, "STRING");

	/* allocate needed structures & handles */
	priv_data = g_new0 (GdaOdbcConnectionPrivate, 1);
	rc = SQLAllocEnv (&priv_data->henv);
	if (!SQL_SUCCEEDED (rc)) {
		/* FIXME: error management */
		g_free (priv_data);
		return FALSE;
	}

	rc = SQLAllocConnect (priv_data->henv, &priv_data->hdbc);
	if (!SQL_SUCCEEDED (rc)) {
		/* FIXME: error management */
		SQLFreeEnv (priv_data->henv);
		g_free (priv_data);
		return FALSE;
	}

	/* open the connection */
	rc = SQLConnect (priv_data->hdbc,
			 (SQLCHAR *) odbc_string, SQL_NTS,
			 (SQLCHAR *) username, SQL_NTS,
			 (SQLCHAR *) password, SQL_NTS);
	if (!SQL_SUCCEEDED (rc)) {
		/* FIXME: error management */
		SQLFreeConnect (priv_data->hdbc);
		SQLFreeEnv (priv_data->henv);
		g_free (priv_data);
		return FALSE;
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE, priv_data);

	return TRUE;
}

/* close_connection handler for the GdaOdbcProvider class */
static gboolean
gda_odbc_provider_close_connection (GdaServerProvider *provider,
				    GdaConnection *cnc)
{
	GdaOdbcConnectionPrivate *priv_data;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
        if (!priv_data)
                return FALSE;

	/* disconnect and free memory */
	SQLDisconnect (priv_data->hdbc);

	SQLFreeConnect (priv_data->hdbc);
        SQLFreeEnv (priv_data->henv);
	g_free (priv_data);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE, NULL);

	return TRUE;
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, GdaCommand *cmd)
{
	GdaOdbcConnectionPrivate *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
        if (!priv_data)
                return NULL;

	return reclist;
}

/* execute_command handler for the GdaOdbcProvider class */
static GList *
gda_odbc_provider_execute_command (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaCommand *cmd,
				   GdaParameterList *params)
{
	GList *reclist = NULL;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);


	/* execute command */
	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
	case GDA_COMMAND_TYPE_TABLE :
		reclist = process_sql_commands (reclist, cnc, cmd);
		break;
	default :
	}

	return reclist;
}
