/* GDA ORACLE Provider
 * Copyright (C) 2000-2002 The GNOME Foundation
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

#include "gda-oracle-connection.h"

typedef struct {
	OCIEnv *henv;
	OCIError *herr;
	OCIServer *hserver;
	OCIService *hservice;
	OCISession *hsession;
} GdaOracleConnectionPrivate;

/*
 * Private functions
 */

static void
free_private_data (gpointer data)
{
	GdaOracleConnectionPrivate *priv = (GdaOracleConnectionPrivate *) data;

	g_return_if_fail (priv != NULL);

	if (priv->hsession)
		OCIHandleFree ((dvoid *) priv->hsession, OCI_HTYPE_SESSION);
	if (priv->hservice)
		OCIHandleFree ((dvoid *) priv->hservice, OCI_HTYPE_SVCCTX);
	if (priv->hserver)
		OCIHandleFree ((dvoid *) priv->hserver, OCI_HTYPE_SERVER);
	if (priv->herr)
		OCIHandleFree ((dvoid *) priv->herr, OCI_HTYPE_ERROR);
	if (priv->henv)
		OCIHandleFree ((dvoid *) priv->henv, OCI_HTYPE_ENV);

	g_free (priv);
}

/*
 * Public functions
 */

gboolean
gda_oracle_connection_initialize (GdaServerConnection *cnc)
{
	GdaOracleConnectionPrivate *priv;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	priv = g_new0 (GdaOracleConnectionPrivate, 1);
	g_object_set_data_full (G_OBJECT (cnc), "GNOME_Database_OracleHandle",
				priv, (GDestroyNotify) free_private_data);

	/* initialize Oracle environment */
	if (OCI_SUCCESS !=
	    OCIEnvInit ((OCIEnv **) & priv->henv, OCI_DEFAULT, (size_t) 0,
                        (dvoid **) 0)) {
		gda_server_connection_add_error_string (
			cnc, _("Could not initialize Oracle environment"));
		return FALSE;
	}

	if (OCI_SUCCESS !=
	    OCIHandleAlloc ((dvoid *) priv->henv,
                                    (dvoid **) &priv->herr,
                                    OCI_HTYPE_ERROR, (size_t) 0,
                                    (dvoid **) 0)) {
		gda_server_connection_add_error_string (
			cnc, _("Could not initialize Oracle error handler"));
		return FALSE;
	}

	/* we use the Multiple Sessions/Connections OCI paradigm for this server */
	if (OCI_SUCCESS !=
	    OCIHandleAlloc ((dvoid *) priv->henv,
			    (dvoid **) & priv->hserver,
			    OCI_HTYPE_SERVER, (size_t) 0,
			    (dvoid **) 0)) {
		gda_server_connection_add_error_string (
			cnc, _("Could not initialize Oracle server handle"));
		return FALSE;
	}

	if (OCI_SUCCESS !=
	    OCIHandleAlloc ((dvoid *) priv->henv,
			    (dvoid **) &priv->hservice,
			    OCI_HTYPE_SVCCTX,
			    (size_t) 0,
			    (dvoid **) 0)) {
		gda_server_connection_add_error_string (
			cnc, _("Could not initialize Oracle service context"));
		return FALSE;
	}
}

gboolean
gda_oracle_connection_open (GdaServerConnection *cnc,
			    const gchar *database,
			    const gchar *username,
			    const gchar *password)
{
	GdaOracleConnectionPrivate *priv;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	priv = g_object_get_data (G_OBJECT (cnc), "GNOME_Database_OracleHandle");

	g_assert (priv != NULL);

	/* attach to Oracle server */
	if (OCI_SUCCESS != OCIServerAttach (priv->hserver,
                                            priv->herr,
                                            (text *) database,
                                            strlen (database), OCI_DEFAULT)) {
		gda_oracle_connection_make_error (cnc);
		return FALSE;
	}

	OCIAttrSet ((dvoid *) priv->hservice, OCI_HTYPE_SVCCTX,
		    (dvoid *) priv->hserver, (ub4) 0,
		    OCI_ATTR_SERVER, priv->herr);

	/* create the session handle */
	if (OCI_SUCCESS != OCIHandleAlloc ((dvoid *) priv->henv,
					   (dvoid **) &priv->hsession,
					   OCI_HTYPE_SESSION,
					   (size_t) 0,
					   (dvoid **) 0)) {
		OCIServerDetach (priv->hserver, priv->herr, OCI_DEFAULT);
		gda_oracle_connection_make_error (cnc);
		return FALSE;
	}

	OCIAttrSet ((dvoid *) priv->hsession,
		    OCI_HTYPE_SESSION, (dvoid *) user,
		    (ub4) strlen (user), OCI_ATTR_USERNAME,
		    priv->herr);
	OCIAttrSet ((dvoid *) priv->hsession,
		    OCI_HTYPE_SESSION, (dvoid *) password,
		    (ub4) strlen (password),
		    OCI_ATTR_PASSWORD, priv->herr);

	/* inititalize connection */
	if (OCI_SUCCESS != OCISessionBegin (priv->hservice,
					    priv->herr,
					    priv->hsession,
					    OCI_CRED_RDBMS,
					    OCI_DEFAULT)) {
		OCIServerDetach (priv->hserver, priv->herr, OCI_DEFAULT);
		OCIHandleFree ((dvoid *) priv->hsession, OCI_HTYPE_SESSION);
		priv->hsession = NULL;

		gda_oracle_connection_make_error (cnc);
		return FALSE;
	}

	OCIAttrSet ((dvoid *) priv->hservice,
		    OCI_HTYPE_SVCCTX,
		    (dvoid *) priv->hsession,
		    (ub4) 0, OCI_ATTR_SESSION,
		    priv->herr);
}

gboolean
gda_oracle_connection_close (GdaServerConnection *cnc)
{
	GdaOracleConnectionPrivate *priv;

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	priv = g_object_get_data (G_OBJECT (cnc), "GNOME_Database_OracleHandle");

	g_assert (priv != NULL);

	if (OCI_SUCCESS != OCISessionEnd (priv->hservice,
					  priv->herr,
					  priv->hsession,
					  OCI_DEFAULT)) {
		gda_oracle_connection_make_error (cnc);
		return FALSE;
	}

	OCIHandleFree ((dvoid *) priv->hsession, OCI_HTYPE_SESSION);
	priv->hsession = NULL;

	return TRUE;
}

gboolean
gda_oracle_connection_begin_transaction (GdaServerConnection *cnc,
					 const gchar *trans_id)
{
	GdaOracleConnectionPrivate *priv;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	priv = g_object_get_data (G_OBJECT (cnc), "GNOME_Database_OracleHandle");

	g_assert (priv != NULL);

	if (OCITransStart (priv->hservice, priv->herr, 0, OCI_TRANS_NEW) == OCI_SUCCESS)
		return TRUE;

	gda_oracle_connection_make_error (cnc);

	return FALSE;
}

gboolean
gda_oracle_connection_commit_transaction (GdaServerConnection *cnc,
					  const gchar *trans_id)
{
	GdaOracleConnectionPrivate *priv;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	priv = g_object_get_data (G_OBJECT (cnc), "GNOME_Database_OracleHandle");

	g_assert (priv != NULL);

	if (OCITransCommit (priv->hservice, priv->herr, OCI_DEFAULT) == OCI_SUCCESS)
		return TRUE;

	gda_oracle_connection_make_error (cnc);

	return FALSE;
}

gboolean
gda_oracle_connection_rollback_transaction (GdaServerConnection *cnc,
					    const gchar *trans_id)
{
	GdaOracleConnectionPrivate *priv;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	priv = g_object_get_data (G_OBJECT (cnc), "GNOME_Database_OracleHandle");

	g_assert (priv != NULL);

	if (OCITransRollback (priv->hservice, priv->herr, OCI_DEFAULT) == OCI_SUCCESS)
		return TRUE;

	gda_oracle_connection_make_error (cnc);

	return FALSE;
}

void
gda_oracle_connection_make_error (GdaServerConnection *cnc)
{
	GdaOracleConnectionPrivate *priv;

	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	priv = g_object_get_data (G_OBJECT (cnc), "GNOME_Database_OracleHandle");

	g_assert (priv != NULL);
}
