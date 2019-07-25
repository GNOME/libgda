/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 Og B. Maciel <ogmaciel@gnome.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-xa-transaction"

#include <glib/gi18n-lib.h>
#include <libgda/gda-xa-transaction.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-value.h>
#include <string.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-debug-macros.h>

static void gda_xa_transaction_class_init (GdaXaTransactionClass *klass);
static void gda_xa_transaction_init       (GdaXaTransaction *xa_trans);
static void gda_xa_transaction_dispose   (GObject *object);

static void gda_xa_transaction_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gda_xa_transaction_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);


typedef struct  {
	GdaXaTransactionId  xid;
	GHashTable         *cnc_hash; /* key = cnc, value = branch qualifier as a GdaBinary */
	GList              *cnc_list;
	GdaConnection      *non_xa_cnc; /* connection which does not support distributed transaction (also in @cnc_list) */
} GdaXaTransactionPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (GdaXaTransaction, gda_xa_transaction, G_TYPE_OBJECT)
/* properties */
enum
{
	PROP_0,
	PROP_FORMAT_ID,
	PROP_TRANSACT_ID
};


/*
 * GdaXaTransaction class implementation
 */

static void
gda_xa_transaction_class_init (GdaXaTransactionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gda_xa_transaction_dispose;


	/* Properties */
        object_class->set_property = gda_xa_transaction_set_property;
        object_class->get_property = gda_xa_transaction_get_property;
        g_object_class_install_property (object_class, PROP_FORMAT_ID,
                                         g_param_spec_uint ("format-id", NULL, NULL,
							    0, G_MAXUINT32, 1,
							    G_PARAM_WRITABLE | G_PARAM_READABLE | 
							    G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_TRANSACT_ID,
                                         g_param_spec_string ("transaction-id", NULL, NULL,
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_READABLE | 
							      G_PARAM_CONSTRUCT_ONLY));
}

static void
gda_xa_transaction_init (GdaXaTransaction *xa_trans)
{
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
	priv->xid.format = 1;
	priv->xid.gtrid_length = 0;
	priv->xid.bqual_length = 0;

	priv->cnc_hash = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)gda_binary_free);
	priv->cnc_list = NULL;
	priv->non_xa_cnc = NULL;
}

static void
gda_xa_transaction_dispose (GObject *object)
{
	GdaXaTransaction *xa_trans = (GdaXaTransaction *) object;

	g_return_if_fail (GDA_IS_XA_TRANSACTION (xa_trans));
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);

	if (priv->cnc_list) {
		GList *list;
		for (list = priv->cnc_list; list; list = list->next) {
			g_object_set_data (G_OBJECT (list->data), "_gda_xa_transaction", NULL);
			g_object_unref (G_OBJECT (list->data));
		}
		g_list_free (priv->cnc_list);
		priv->cnc_list = NULL;
	}
	if (priv->cnc_hash) {
		g_hash_table_destroy (priv->cnc_hash);
		priv->cnc_hash = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_xa_transaction_parent_class)->dispose (object);
}

static void
gda_xa_transaction_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
        GdaXaTransaction *xa_trans;

        xa_trans = GDA_XA_TRANSACTION (object);
        GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
        if (priv) {
                switch (param_id) {
                case PROP_FORMAT_ID:
			priv->xid.format = g_value_get_uint (value);
                        break;
                case PROP_TRANSACT_ID: {
			const gchar *tmp;
			gint len;
		    
			tmp = g_value_get_string (value);
			if (!tmp) {
				gchar *dtmp;
				dtmp = g_strdup_printf ("gda_global_transaction_%p", xa_trans);
				len = strlen (dtmp);
				g_assert (len <= 64);
				priv->xid.gtrid_length = len;
				memcpy (priv->xid.data, dtmp, len); /* Flawfinder: ignore */
				g_free (dtmp);
			}
			else {
				len = strlen (tmp);
				if (len > 64)
					g_warning (_("Global transaction ID can not have more than 64 bytes"));
				else {
					priv->xid.gtrid_length = len;
					memcpy (priv->xid.data, tmp, len); /* Flawfinder: ignore */
				}
			}
                        break;
		}
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
}

static void
gda_xa_transaction_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
        GdaXaTransaction *xa_trans;

        xa_trans = GDA_XA_TRANSACTION (object);
        GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
        if (priv) {
                switch (param_id) {
                case PROP_FORMAT_ID:
			g_value_set_uint (value, priv->xid.format);
                        break;
                case PROP_TRANSACT_ID: {
			gchar *tmp;

			tmp = g_new (gchar, priv->xid.gtrid_length + 1);
			memcpy (tmp, priv->xid.data, priv->xid.gtrid_length); /* Flawfinder: ignore */
			tmp [priv->xid.gtrid_length] = 0;
			g_value_take_string (value, tmp);
                        break;
		}
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
}


/* module error */
GQuark gda_xa_transaction_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_xa_transaction_error");
        return quark;
}

/**
 * gda_xa_transaction_new:
 * @format: a format ID
 * @global_transaction_id: the global transaction ID
 *
 * Creates a new #GdaXaTransaction object, which will control the process of
 * performing a distributed transaction across several connections.
 *
 * Returns: (transfer full): the newly created object.
 */
GdaXaTransaction *
gda_xa_transaction_new (guint32 format, const gchar *global_transaction_id)
{
	g_return_val_if_fail (global_transaction_id && *global_transaction_id, NULL);
	return (GdaXaTransaction*) g_object_new (GDA_TYPE_XA_TRANSACTION, "format", format, 
						 "trans", global_transaction_id, NULL);
}

/**
 * gda_xa_transaction_register_connection:
 * @xa_trans: a #GdaXaTransaction object
 * @cnc: the connection to add to @xa_trans
 * @branch: the branch qualifier
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Registers @cnc to be used by @xa_trans to create a distributed transaction.
 *
 * Note: any #GdaConnection object can only be registered with at most one #GdaXaTransaction object; also
 * some connections may not be registered at all with a #GdaXaTransaction object because the database
 * provider being used does not support it.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_xa_transaction_register_connection  (GdaXaTransaction *xa_trans, GdaConnection *cnc, 
					 const gchar *branch, GError **error)
{
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_XA_TRANSACTION (xa_trans), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (branch && *branch, FALSE);
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
	if (strlen (branch) >= 64) {
		g_set_error (error, GDA_XA_TRANSACTION_ERROR,
			     GDA_XA_TRANSACTION_CONNECTION_BRANCH_LENGTH_ERROR,
			     "%s", _("Connection branch cannot exceed 63 bytes"));
		return FALSE;
	}

	const GdaBinary *ebranch = g_hash_table_lookup (priv->cnc_hash, cnc);
	if (ebranch) {
		bin = gda_binary_new ();
		gda_binary_set_data (bin, (const guchar *)branch, strlen (branch) + 1);
		g_hash_table_insert (priv->cnc_hash, cnc, bin);
		return TRUE;
	}

	/* check that @cnc is not already registered with another GdaXaTransaction object */
	if (g_object_get_data (G_OBJECT (cnc), "_gda_xa_transaction")) {
		g_set_error (error, GDA_XA_TRANSACTION_ERROR,
			     GDA_XA_TRANSACTION_ALREADY_REGISTERED_ERROR,
			     "%s", _("Connection already registered with another GdaXaTransaction object"));
		return FALSE;
	}

	/* check that connection supports distributed transaction, only ONE connection in @xa_trans is allowed
	 * to not support them */
	GdaServerProvider *prov;
	prov = gda_connection_get_provider (cnc);

	if (!gda_server_provider_supports_feature (prov, cnc, GDA_CONNECTION_FEATURE_XA_TRANSACTIONS)) {
		/* if another connection does not support distributed transaction, then there is an error */
		if (priv->non_xa_cnc) {
			g_set_error (error, GDA_XA_TRANSACTION_ERROR,
				     GDA_XA_TRANSACTION_DTP_NOT_SUPPORTED_ERROR,
				     "%s", _("Connection does not support distributed transaction"));
			return FALSE;
		}
		else
			priv->non_xa_cnc = cnc;
	}

	bin = gda_binary_new ();
	gda_binary_set_data (bin, (const guchar *)branch, strlen (branch) + 1);
	priv->cnc_list = g_list_prepend (priv->cnc_list, cnc);
	g_hash_table_insert (priv->cnc_hash, cnc, bin);
	g_object_ref (cnc);
	g_object_set_data (G_OBJECT (cnc), "_gda_xa_transaction", xa_trans);

	return TRUE;
}

/**
 * gda_xa_transaction_unregister_connection:
 * @xa_trans: a #GdaXaTransaction object
 * @cnc: the connection to add to @xa_trans
 *
 * Unregisters @cnc to be used by @xa_trans to create a distributed transaction. This is
 * the opposite of gda_xa_transaction_register_connection().
 */
void
gda_xa_transaction_unregister_connection (GdaXaTransaction *xa_trans, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_XA_TRANSACTION (xa_trans));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);

	if (!g_list_find (priv->cnc_list, cnc)) {
		g_warning (_("Cannot unregister connection not registered with GdaXaTransaction object"));
		return;
	}
	priv->cnc_list = g_list_remove (priv->cnc_list, cnc);
	g_hash_table_remove (priv->cnc_hash, cnc);
	g_object_set_data (G_OBJECT (cnc), "_gda_xa_transaction", NULL);
	g_object_unref (cnc);
}

/**
 * gda_xa_transaction_begin:
 * @xa_trans: a #GdaXaTransaction object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Begins a distributed transaction (managed by @xa_trans). Please note that this phase may fail
 * for some connections if a (normal) transaction is already started (this depends on the database
 * provider being used), so it's better to avoid starting any (normal) transaction on any of the
 * connections registered with @xa_trans.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_xa_transaction_begin  (GdaXaTransaction *xa_trans, GError **error)
{
	GList *list;
	g_return_val_if_fail (GDA_IS_XA_TRANSACTION (xa_trans), FALSE);
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
	
	for (list = priv->cnc_list; list; list = list->next) {
		GdaConnection *cnc;
		GdaServerProvider *prov;
		
		cnc = GDA_CONNECTION (list->data);
		prov = gda_connection_get_provider (cnc);
		if (cnc != priv->non_xa_cnc) {
			GdaBinary *branch;
			branch = g_hash_table_lookup (priv->cnc_hash, cnc);
			memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
				gda_binary_get_data (branch), gda_binary_get_size (branch));
			if (! _gda_server_provider_xa_start (prov, cnc, &(priv->xid), error))
				break;
		}
		else {
			/* do a simple BEGIN */
			if (! gda_connection_begin_transaction (cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN,
								error))
				break;
		}
	}

	if (list) {
		/* something went wrong */
		for (; list; list = list->prev) {
			GdaConnection *cnc;
			GdaServerProvider *prov;
			
			cnc = GDA_CONNECTION (list->data);
			prov = gda_connection_get_provider (cnc);
			if (cnc != priv->non_xa_cnc) {
				GdaBinary *branch;
				branch = g_hash_table_lookup (priv->cnc_hash, cnc);
				memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
					gda_binary_get_data (branch), gda_binary_get_size (branch));
				_gda_server_provider_xa_rollback (prov, cnc, &(priv->xid), NULL);
			}
			else {
				/* do a simple ROLLBACK */
				gda_connection_rollback_transaction (cnc, NULL, NULL);
			}
		}
		return FALSE;
	}
	return TRUE;
}

/**
 * gda_xa_transaction_commit:
 * @xa_trans: a #GdaXaTransaction object
 * @cnc_to_recover: (nullable) (element-type Gda.Connection) (out callee-allocates): a place to store the list of connections for which the commit phase failed, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Commits a distributed transaction (managed by @xa_trans). The commit is composed of two phases:
 * <itemizedlist>
 *   <listitem><para>a PREPARE phase where all the connections are required to store their transaction data to a 
 *     permanent place (to be able to complete the commit should a problem occur afterwards)</para></listitem>
 *   <listitem><para>a COMMIT phase where the transaction data is actually written to the database</para></listitem>
 * </itemizedlist>
 *
 * If the PREPARE phase fails for any of the connection registered with @xa_trans, then the distributed commit
 * fails and FALSE is returned. During the COMMIT phase, some commit may actually fail but the transaction can
 * still be completed because the PREPARE phase succeeded (through the recover method).
 *
 * Returns: TRUE if no error occurred (there may be some connections to recover, though)
 */
gboolean
gda_xa_transaction_commit (GdaXaTransaction *xa_trans, GSList **cnc_to_recover, GError **error)
{
	GList *list;
	if (cnc_to_recover)
		*cnc_to_recover = NULL;

	g_return_val_if_fail (GDA_IS_XA_TRANSACTION (xa_trans), FALSE);
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
	
	/* 
	 * PREPARE phase 
	 */
	for (list = priv->cnc_list; list; list = list->next) {
		GdaConnection *cnc = NULL;
		GdaServerProvider *prov;
		GdaBinary *branch;
		
		if (cnc == priv->non_xa_cnc)
			continue;

		cnc = GDA_CONNECTION (list->data);
		prov = gda_connection_get_provider (cnc);

		branch = g_hash_table_lookup (priv->cnc_hash, cnc);
		memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
			gda_binary_get_data (branch), gda_binary_get_size (branch));

		if (!_gda_server_provider_xa_end (prov, cnc, &(priv->xid), error))
			break;

		if (!_gda_server_provider_xa_prepare (prov, cnc, &(priv->xid), error))
			break;
	}
	if (list) {
		/* something went wrong during the PREPARE phase => rollback everything */
		for (; list; list = list->prev) {
			GdaConnection *cnc = NULL;
			GdaServerProvider *prov;
			
			if (cnc == priv->non_xa_cnc)
				gda_connection_rollback_transaction (cnc, NULL, NULL);
			else {
				GdaBinary *branch;
				cnc = GDA_CONNECTION (list->data);
				prov = gda_connection_get_provider (cnc);
				branch = g_hash_table_lookup (priv->cnc_hash, cnc);
				memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
					gda_binary_get_data (branch), gda_binary_get_size (branch));
				_gda_server_provider_xa_rollback (prov, cnc, &(priv->xid), NULL);
			}
		}
		return FALSE;
	}

	/*
	 * COMMIT phase 
	 */
	if (priv->non_xa_cnc &&
	    ! gda_connection_commit_transaction (priv->non_xa_cnc, NULL, error)) {
		/* something went wrong => rollback everything */
		for (list = priv->cnc_list; list; list = list->next) {
			GdaConnection *cnc = NULL;
			GdaServerProvider *prov;
			
			if (cnc == priv->non_xa_cnc)
				gda_connection_rollback_transaction (cnc, NULL, NULL);
			else {
				GdaBinary *branch;
				cnc = GDA_CONNECTION (list->data);
				prov = gda_connection_get_provider (cnc);
				branch = g_hash_table_lookup (priv->cnc_hash, cnc);
				memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
					gda_binary_get_data (branch), gda_binary_get_size (branch));
				_gda_server_provider_xa_rollback (prov, cnc, &(priv->xid), NULL);
			}
		}
		return FALSE;
	}

	for (list = priv->cnc_list; list; list = list->next) {
		GdaConnection *cnc = NULL;
		GdaServerProvider *prov;
		GdaBinary *branch;
		
		if (cnc == priv->non_xa_cnc)
			continue;

		cnc = GDA_CONNECTION (list->data);
		prov = gda_connection_get_provider (cnc);
		branch = g_hash_table_lookup (priv->cnc_hash, cnc);
		memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
			gda_binary_get_data (branch), gda_binary_get_size (branch));
		if (! _gda_server_provider_xa_commit (prov, cnc, &(priv->xid), error) &&
		    cnc_to_recover)
			*cnc_to_recover = g_slist_prepend (*cnc_to_recover, cnc);
	}
	
	return TRUE;
}

/**
 * gda_xa_transaction_rollback:
 * @xa_trans: a #GdaXaTransaction object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Cancels a distributed transaction (managed by @xa_trans). 
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_xa_transaction_rollback (GdaXaTransaction *xa_trans, GError **error)
{
	GList *list;
	g_return_val_if_fail (GDA_IS_XA_TRANSACTION (xa_trans), FALSE);
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
	
	for (list = priv->cnc_list; list; list = list->next) {
		GdaConnection *cnc;
		GdaServerProvider *prov;

		cnc = GDA_CONNECTION (list->data);
		prov = gda_connection_get_provider (cnc);
		
		if (cnc == priv->non_xa_cnc)
			gda_connection_rollback_transaction (cnc, NULL, NULL);
		else {
			GdaBinary *branch;
			branch = g_hash_table_lookup (priv->cnc_hash, cnc);
			memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
				gda_binary_get_data (branch), gda_binary_get_size (branch));
			GError *lerror = NULL;
			_gda_server_provider_xa_rollback (prov, cnc, &(priv->xid), &lerror);
			if (error && !*error)
				g_propagate_error (error, lerror);
			else
				g_clear_error (&lerror);
		}
	}
	return TRUE;
}

/**
 * gda_xa_transaction_commit_recovered:
 * @xa_trans: a #GdaXaTransaction object
 * @cnc_to_recover: (nullable) (element-type Gda.Connection) (out callee-allocates): a place to store the list of connections for which the there were data to recover and which failed to be actually committed, or %NULL
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Tries to commit the data prepared but which failed to commit (see gda_xa_transaction_commit()). This
 * method allows one to terminate a distributed transaction which succeeded but for which some
 * connections needed to be recovered.
 *
 * Returns: %TRUE if all the data which was still uncommitted has been committed
 */
gboolean
gda_xa_transaction_commit_recovered (GdaXaTransaction *xa_trans, GSList **cnc_to_recover, GError **error)
{
	GList *list;
	gboolean retval = TRUE;
	
	if (cnc_to_recover)
		*cnc_to_recover = NULL;
	g_return_val_if_fail (GDA_IS_XA_TRANSACTION (xa_trans), FALSE);
	GdaXaTransactionPrivate *priv = gda_xa_transaction_get_instance_private (xa_trans);
	
	for (list = priv->cnc_list; list; list = list->next) {
		GdaConnection *cnc;
		GdaServerProvider *prov;

		cnc = GDA_CONNECTION (list->data);
		prov = gda_connection_get_provider (cnc);
		
		if (cnc == priv->non_xa_cnc)
			continue;
		else {
			GList *recov_xid_list;
			
			GdaBinary *branch;
			GList *xlist;
			gboolean commit_needed = FALSE;
				
			recov_xid_list = _gda_server_provider_xa_recover (prov, cnc, error);
			if (!recov_xid_list)
				continue;

			branch = g_hash_table_lookup (priv->cnc_hash, cnc);
			memcpy (priv->xid.data + priv->xid.gtrid_length, /* Flawfinder: ignore */
				gda_binary_get_data (branch), gda_binary_get_size (branch));
			for (xlist = recov_xid_list; xlist; xlist = xlist->next) {
				GdaXaTransactionId *xid = (GdaXaTransactionId*) xlist->data;
				if (!xid)
					/* ignore unknown XID format */
					continue;

				if (!commit_needed &&
				    (xid->format == priv->xid.format) &&
				    (xid->gtrid_length == priv->xid.gtrid_length) &&
				    (xid->bqual_length == priv->xid.bqual_length) &&
				    ! memcmp (priv->xid.data, xid->data, xid->bqual_length + xid->bqual_length))
					/* found a transaction to commit */
					commit_needed = TRUE;
				
				g_free (xid);
			}
			g_list_free (recov_xid_list);

			if (commit_needed) {
				retval = _gda_server_provider_xa_commit (prov, cnc, &(priv->xid), error);
				if (!retval)
					if (cnc_to_recover)
						*cnc_to_recover = g_slist_prepend (*cnc_to_recover, cnc);
			}
		}
	}
	return retval;	
}


GdaXaTransactionId *
gda_xa_transaction_id_copy (GdaXaTransactionId *src) {
	GdaXaTransactionId *cp = g_new0 (GdaXaTransactionId, 1);
	cp->format = src->format;
	cp->gtrid_length = src->gtrid_length;
	cp->bqual_length = src->bqual_length;
	for(int i = 0; i < 128; i++) {
		cp->data[i] = src->data[i];
	}
	return  cp;
}
void
gda_xa_transaction_id_free (GdaXaTransactionId *tid) {
	g_free (tid);
}

G_DEFINE_BOXED_TYPE(GdaXaTransactionId, gda_xa_transaction_id, gda_xa_transaction_id_copy, gda_xa_transaction_id_free)

/**
 * gda_xa_transaction_id_to_string:
 * @xid: a #GdaXaTransactionId pointer
 *
 * Creates a string representation of @xid, in the format &lt;gtrid&gt;,&lt;bqual&gt;,&lt;formatID&gt; the 
 * &lt;gtrid&gt; and &lt;bqual&gt; strings contain alphanumeric characters, and non alphanumeric characters
 * are converted to "%ab" where ab is the hexadecimal representation of the character.
 *
 * Returns: (transfer full): a new string representation of @xid
 */
gchar *
gda_xa_transaction_id_to_string (const GdaXaTransactionId *xid)
{
#define XID_SIZE (128 * 3 +15)
	gchar *str = g_new0 (gchar, XID_SIZE);
	int index = 0, i;
	
	g_return_val_if_fail (xid, NULL);

	/* global transaction ID */
	for (i = 0; i < xid->gtrid_length; i++) {
		if (g_ascii_isalnum (xid->data[i])) {
			str [index] = xid->data[i];
			index++;
		}
		else 
			index += sprintf (str+index, "%%%02x", xid->data[i]); /* Flawfinder: ignore */
	}

	/* branch qualifier */
	str [index++] = ',';
	for (i = 0; i < xid->bqual_length; i++) {
		if (g_ascii_isalnum (xid->data[xid->gtrid_length + i])) {
			str [index] = xid->data[xid->gtrid_length + i];
			index++;
		}
		else 
			index += sprintf (str+index, "%%%02x", xid->data[xid->gtrid_length + i]); /* Flawfinder: ignore */
	}

	/* Format ID */
	str [index++] = ',';
	sprintf (str, "%d", xid->format); /* Flawfinder: ignore */

	return str;
}

/**
 * gda_xa_transaction_string_to_id:
 * @str: a string representation of a #GdaXaTransactionId, in the "gtrid,bqual,formatID" format
 *
 * Creates a new #GdaXaTransactionId structure from its string representation, it's the opposite
 * of gda_xa_transaction_id_to_string().
 *
 * Returns: a new #GdaXaTransactionId structure, or %NULL in @str has a wrong format
 *
 * Free-function: g_free
 */
GdaXaTransactionId *
gda_xa_transaction_string_to_id (const gchar *str)
{
	GdaXaTransactionId *xid;
	const gchar *ptr;
	int index = 0;

	g_return_val_if_fail (str, NULL);

	xid = g_new0 (GdaXaTransactionId, 1);

	/* global transaction ID */
	for (ptr = str; *ptr && (*ptr != ','); ptr++, index++) {
		if (index >= 64)
			goto onerror;

		if (*ptr == '%') {
			ptr++;
			if (*ptr && (((*ptr >= 'a') && (*ptr <= 'f')) ||
				     ((*ptr >= '0') && (*ptr <= '9')))) {
				if ((*ptr >= 'a') && (*ptr <= 'f'))
					xid->data [index] = (*ptr - 'a' + 10) * 16;
				else
					xid->data [index] = (*ptr - '0') * 16;
				ptr++;
				if (*ptr && (((*ptr >= 'a') && (*ptr <= 'f')) ||
					     ((*ptr >= '0') && (*ptr <= '9')))) {
					if ((*ptr >= 'a') && (*ptr <= 'f'))
						xid->data [index] = (*ptr - 'a' + 10);
					else
						xid->data [index] = (*ptr - '0');
				}
				else
					goto onerror;
			}
			else
				goto onerror;
		}
		else if (g_ascii_isalnum (*ptr))
			xid->data [index] = *ptr;
		else
			goto onerror;			 
	}
	xid->gtrid_length = index;

	/* branch qualifier */
	if (*ptr != ',') 
		goto onerror;
	for (ptr++; *ptr && (*ptr != ','); ptr++, index++) {
		if (index >= 128)
			goto onerror;

		if (*ptr == '%') {
			ptr++;
			if (*ptr && (((*ptr >= 'a') && (*ptr <= 'f')) ||
				     ((*ptr >= '0') && (*ptr <= '9')))) {
				if ((*ptr >= 'a') && (*ptr <= 'f'))
					xid->data [index] = (*ptr - 'a' + 10) * 16;
				else
					xid->data [index] = (*ptr - '0') * 16;
				ptr++;
				if (*ptr && (((*ptr >= 'a') && (*ptr <= 'f')) ||
					     ((*ptr >= '0') && (*ptr <= '9')))) {
					if ((*ptr >= 'a') && (*ptr <= 'f'))
						xid->data [index] = (*ptr - 'a' + 10);
					else
						xid->data [index] = (*ptr - '0');
				}
				else
					goto onerror;
			}
			else
				goto onerror;
		}
		else if (g_ascii_isalnum (*ptr))
			xid->data [index] = *ptr;
		else
			goto onerror;
	}
	xid->bqual_length = index - xid->gtrid_length;

	/* Format ID */
	if (*ptr != ',') 
		goto onerror;
	ptr++;
	gchar *endptr;
	gint64 tmp = g_ascii_strtoll (ptr, &endptr, 10);
	if (*endptr || (tmp < 0) || (tmp >= G_MAXUINT32))
		goto onerror;

	xid->format = (guint32) tmp;
	
	return xid;

 onerror:
	g_free (xid);
	return NULL;
}
