/* GDA ORACLE Provider
 * Copyright (C) 2000-2001 The GNOME Foundation
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-oracle.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

static void gda_mysql_provider_class_init (GdaOracleProviderClass *klass);
static void gda_oracle_provider_init       (GdaOracleProvider *provider,
					    GdaOracleProviderClass *klass);
static void gda_oracle_provider_finalize   (GObject *object);

static gboolean gda_oracle_provider_open_connection (GdaServerProvider *provider,
						     GdaServerConnection *cnc,
						     GdaQuarkList *params,
						     const gchar *username,
						     const gchar *password);
static gboolean gda_oracle_provider_close_connection (GdaServerProvider *provider,
						      GdaServerConnection *cnc);
static GList *gda_oracle_provider_execute_command (GdaServerProvider *provider,
						   GdaServerConnection *cnc,
						   GdaCommand *cmd,
						   GdaParameterList *params);
static gboolean gda_oracle_provider_begin_transaction (GdaServerProvider *provider,
						       GdaServerConnection *cnc,
						       const gchar *trans_id);
static gboolean gda_oracle_provider_commit_transaction (GdaServerProvider *provider,
							GdaServerConnection *cnc,
							const gchar *trans_id);

static gboolean gda_oracle_provider_rollback_transaction (GdaServerProvider *provider,
							  GdaServerConnection *cnc,
							  const gchar *trans_id);
static GObjectClass *parent_class = NULL;

/*
 * GdaOracleProvider class implementation
 */

static void
gda_oracle_provider_class_init (GdaOracleProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = gda_oracle_provider_finalize;
	provider_class->open_connection = gda_oracle_provider_open_connection;
	provider_class->close_connection = gda_oracle_provider_close_connection;
	provider_class->execute_command = gda_oracle_provider_execute_command;
	provider_class->begin_transaction = gda_oracle_provider_begin_transaction;
	provider_class->commit_transaction = gda_oracle_provider_commit_transaction;
	provider_class->rollback_transaction = gda_oracle_provider_rollback_transaction;
}

static void
gda_oracle_provider_init (GdaOracleProvider *orap, GdaOracleProviderClass *klass)
{
}

static void
gda_oracle_provider_finalize (GObject *object)
{
	GdaOracleProvider *orap = (GdaOracleProvider *) object;

	g_return_if_fail (GDA_IS_ORACLE_PROVIDER (orap));

	parent_class->finalize (object);
}

GType
gda_mysql_provider_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static GTypeInfo info = {
                        sizeof (GdaOracleProviderClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_oracle_provider_class_init,
                        NULL, NULL,
                        sizeof (GdaOracleProvider),
                        0,
                        (GInstanceInitFunc) gda_oracle_provider_init
                };
                type = g_type_register_static (PARENT_TYPE,
                                               "GdaOracleProvider",
                                               &info, 0);
        }

        return type;
}

/* open_connection handler for the Oracle provider */
static gboolean
gda_oracle_provider_open_connection (GdaServerProvider *provider,
				     GdaServerConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
	gchar *database;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	if (!gda_oracle_connection_initialize (cnc))
		return FALSE;

	/* open the connection */
	database = gda_quark_list_find (params, "DATABASE");

	return gda_oracle_connection_open (cnc, database, username, password);
}

/* close_connection handler for the Oracle provider */
static gboolean
gda_oracle_provider_close_connection (GdaServerProvider *provider, GdaServerConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	return gda_oracle_connection_close (cnc);
}

/* execute_command handler for the Oracle provider */
static GList *
gda_oracle_provider_execute_command (GdaServerProvider *provider,
				     GdaServerConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params)
{
}

/* begin_transaction handler for the Oracle provider */
static gboolean
gda_oracle_provider_begin_transaction (GdaServerProvider *provider,
				       GdaServerConnection *cnc,
				       const gchar *trans_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	return gda_oracle_connection_begin_transaction (cnc, trans_id);
}

/* commit_transaction handler for the Oracle provider */
static gboolean
gda_oracle_provider_commit_transaction (GdaServerProvider *provider,
					GdaServerConnection *cnc,
					const gchar *trans_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	return gda_oracle_connection_commit_transaction (cnc, trans_id);
}

/* rollback_transaction handler for the Oracle provider */
static gboolean
gda_oracle_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaServerConnection *cnc,
					  const gchar *trans_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	return gda_oracle_connection_rollback_transaction (cnc, trans_id);
}
