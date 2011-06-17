/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 - 2008 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-blob-op.h"
#include "gda-postgres-util.h"

struct _GdaPostgresBlobOpPrivate {
	GdaConnection *cnc;
	Oid            blobid;  /* SQL ID in database */
	gint           fd;      /* to use with lo_read, lo_write, lo_lseek, lo_tell, and lo_close */
};

static void gda_postgres_blob_op_class_init (GdaPostgresBlobOpClass *klass);
static void gda_postgres_blob_op_init       (GdaPostgresBlobOp *blob,
					  GdaPostgresBlobOpClass *klass);
static void gda_postgres_blob_op_finalize   (GObject *object);

static glong gda_postgres_blob_op_get_length (GdaBlobOp *op);
static glong gda_postgres_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_postgres_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_postgres_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaPostgresBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresBlobOp),
			0,
			(GInstanceInitFunc) gda_postgres_blob_op_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaPostgresBlobOp", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_postgres_blob_op_init (GdaPostgresBlobOp *op,
			   G_GNUC_UNUSED GdaPostgresBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_POSTGRES_BLOB_OP (op));

	op->priv = g_new0 (GdaPostgresBlobOpPrivate, 1);
	op->priv->blobid = InvalidOid;
	op->priv->fd = -1;
}

static void
gda_postgres_blob_op_class_init (GdaPostgresBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_postgres_blob_op_finalize;
	blob_class->get_length = gda_postgres_blob_op_get_length;
	blob_class->read = gda_postgres_blob_op_read;
	blob_class->write = gda_postgres_blob_op_write;
}

static PGconn *
get_pconn (GdaConnection *cnc)
{
	PostgresConnectionData *cdata;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;

	return cdata->pconn;
}

static gboolean
blob_op_open (GdaPostgresBlobOp *pgop)
{
	gboolean use_svp = FALSE;

	if (pgop->priv->blobid == InvalidOid)
		return FALSE;
	if (pgop->priv->fd >= 0)
		return TRUE;

	if (gda_connection_get_transaction_status (pgop->priv->cnc)) 
		/* add a savepoint to prevent a blob open failure from rendering the transaction unusable */
		use_svp = TRUE;

	if (use_svp)
		use_svp = gda_connection_add_savepoint (pgop->priv->cnc, "__gda_blob_read_svp", NULL);
	
	pgop->priv->fd = lo_open (get_pconn (pgop->priv->cnc), pgop->priv->blobid, INV_READ | INV_WRITE);
	if (pgop->priv->fd < 0) {
		_gda_postgres_make_error (pgop->priv->cnc, get_pconn (pgop->priv->cnc), NULL, NULL);
		if (use_svp)
			gda_connection_rollback_savepoint (pgop->priv->cnc, "__gda_blob_read_svp", NULL);
		return FALSE;
	}
	if (use_svp)
		gda_connection_delete_savepoint (pgop->priv->cnc, "__gda_blob_read_svp", NULL);
	return TRUE;
}

static void
blob_op_close (GdaPostgresBlobOp *pgop)
{
	lo_close (get_pconn (pgop->priv->cnc), pgop->priv->fd);
	pgop->priv->fd = -1;
}

static void
gda_postgres_blob_op_finalize (GObject * object)
{
	GdaPostgresBlobOp *pgop = (GdaPostgresBlobOp *) object;

	g_return_if_fail (GDA_IS_POSTGRES_BLOB_OP (pgop));

	if (pgop->priv->fd >= 0)
		lo_close (get_pconn (pgop->priv->cnc), pgop->priv->fd);
	g_free (pgop->priv);
	pgop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_postgres_blob_op_new (GdaConnection *cnc)
{
	GdaPostgresBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_POSTGRES_BLOB_OP, NULL);
	pgop->priv->cnc = cnc;
	
	return GDA_BLOB_OP (pgop);
}

GdaBlobOp *
gda_postgres_blob_op_new_with_id (GdaConnection *cnc, const gchar *sql_id)
{
	GdaPostgresBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_POSTGRES_BLOB_OP, NULL);

	pgop->priv->blobid = atoi (sql_id);
	pgop->priv->cnc = cnc;

	return GDA_BLOB_OP (pgop);
}

/**
 * gda_postgres_blob_op_declare_blob
 *
 * Creates a new blob in database if the blob does not yet exist
 */
gboolean
gda_postgres_blob_op_declare_blob (GdaPostgresBlobOp *pgop)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (pgop), FALSE);
	g_return_val_if_fail (pgop->priv, FALSE);

	if (pgop->priv->blobid == InvalidOid) {
		PGconn *pconn = get_pconn (pgop->priv->cnc);
		pgop->priv->blobid = lo_creat (pconn, INV_READ | INV_WRITE);
		if (pgop->priv->blobid == InvalidOid) {
			_gda_postgres_make_error (pgop->priv->cnc, pconn, NULL, NULL);
			return FALSE;
		}
	}
	
	return TRUE;
}

/**
 * gda_postgres_blob_op_get_id
 *
 * Returns: a new string containing the blob id.
 */
gchar *
gda_postgres_blob_op_get_id (GdaPostgresBlobOp *pgop)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (pgop), NULL);
	g_return_val_if_fail (pgop->priv, NULL);

	if (pgop->priv->blobid == InvalidOid)
		return NULL;
	else
		return g_strdup_printf ("%d", pgop->priv->blobid);
}

/**
 * gda_postgres_blob_op_set_id
 */
void
gda_postgres_blob_op_set_id (GdaPostgresBlobOp *pgop, const gchar *sql_id)
{
	g_return_if_fail (GDA_IS_POSTGRES_BLOB_OP (pgop));
	g_return_if_fail (pgop->priv);
	g_return_if_fail (sql_id);

	if (pgop->priv->fd >= 0)
		blob_op_close (pgop);
	pgop->priv->blobid = atoi (sql_id);
}

static gboolean
check_transaction_started (GdaConnection *cnc, gboolean *out_started)
{
        GdaTransactionStatus *trans;

        trans = gda_connection_get_transaction_status (cnc);
        if (!trans) {
		if (!gda_connection_begin_transaction (cnc, NULL,
						       GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL))
			return FALSE;
		else
			*out_started = TRUE;
	}
	return TRUE;
}

/*
 * Virtual functions
 */
static glong
gda_postgres_blob_op_get_length (GdaBlobOp *op)
{
	GdaPostgresBlobOp *pgop;
	PGconn *pconn;
	int pos;
	gboolean transaction_started = FALSE;

	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (op), -1);
	pgop = GDA_POSTGRES_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);

	if (! check_transaction_started (pgop->priv->cnc, &transaction_started))
		return -1;
	
	if (!blob_op_open (pgop))
		goto out_error;

	pconn = get_pconn (pgop->priv->cnc);
	pos = lo_lseek (pconn, pgop->priv->fd, 0, SEEK_END);

	if (pos < 0) {
		_gda_postgres_make_error (pgop->priv->cnc, pconn, NULL, NULL);
		goto out_error;
	}

	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (pgop->priv->cnc, NULL, NULL);

	return pos;

 out_error:
	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (pgop->priv->cnc, NULL, NULL);

	return -1;
}

static glong
gda_postgres_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaPostgresBlobOp *pgop;
	PGconn *pconn;
	GdaBinary *bin;
	gboolean transaction_started = FALSE;

	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (op), -1);
	pgop = GDA_POSTGRES_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	if (! check_transaction_started (pgop->priv->cnc, &transaction_started))
		return -1;

	if (!blob_op_open (pgop))
		goto out_error;

	pconn = get_pconn (pgop->priv->cnc);
	if (lo_lseek (pconn, pgop->priv->fd, offset, SEEK_SET) < 0) {
		_gda_postgres_make_error (pgop->priv->cnc, pconn, NULL, NULL);
		goto out_error;
	}

	bin = (GdaBinary *) blob;
	if (bin->data) 
		g_free (bin->data);
	bin->data = g_new0 (guchar, size);
	bin->binary_length = lo_read (pconn, pgop->priv->fd, (char *) (bin->data), size);

	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (pgop->priv->cnc, NULL, NULL);

	return bin->binary_length;

 out_error:
	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (pgop->priv->cnc, NULL, NULL);

	return -1;
}

static glong
gda_postgres_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaPostgresBlobOp *pgop;
	PGconn *pconn;
	glong nbwritten;
	gboolean transaction_started = FALSE;

	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (op), -1);
	pgop = GDA_POSTGRES_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	if (! check_transaction_started (pgop->priv->cnc, &transaction_started))
		return -1;

	if (!blob_op_open (pgop))
		goto out_error;

	pconn = get_pconn (pgop->priv->cnc);
	if (lo_lseek (pconn, pgop->priv->fd, offset, SEEK_SET) < 0) {
		_gda_postgres_make_error (pgop->priv->cnc, pconn, NULL, NULL);
		goto out_error;
	}

	if (blob->op && (blob->op != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = g_new0 (GdaBlob, 1);
		gda_blob_set_op (tmpblob, blob->op);

		nbwritten = 0;

		for (nread = gda_blob_op_read (tmpblob->op, tmpblob, 0, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (tmpblob->op, tmpblob, nbwritten, buf_size)) {
			GdaBinary *bin = (GdaBinary *) tmpblob;
			glong tmp_written;
			tmp_written = lo_write (pconn, pgop->priv->fd, (char*) bin->data, 
						bin->binary_length);
			if (tmp_written < bin->binary_length) {
				_gda_postgres_make_error (pgop->priv->cnc, pconn, NULL, NULL);
				gda_blob_free ((gpointer) tmpblob);
				goto out_error;
			}
			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}
		gda_blob_free ((gpointer) tmpblob);
	}
	else {
		/* use data in (GdaBinary *) blob */
		GdaBinary *bin = (GdaBinary *) blob;
		nbwritten = lo_write (pconn, pgop->priv->fd, (char*) bin->data, bin->binary_length);
		if (nbwritten == -1) {
			_gda_postgres_make_error (pgop->priv->cnc, pconn, NULL, NULL);
			goto out_error;
		}
	}

	blob_op_close (pgop);
	if (transaction_started)
		if (!gda_connection_commit_transaction (pgop->priv->cnc, NULL, NULL))
			return -1;

	return nbwritten;

 out_error:
	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (pgop->priv->cnc, NULL, NULL);
	return -1;
}
