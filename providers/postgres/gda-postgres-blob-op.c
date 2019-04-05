/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/gda-blob-op-impl.h>
#include "gda-postgres-util.h"


static void gda_postgres_blob_op_class_init (GdaPostgresBlobOpClass *klass);
static void gda_postgres_blob_op_init       (GdaPostgresBlobOp *blob);
static void gda_postgres_blob_op_dispose   (GObject *object);

static glong gda_postgres_blob_op_get_length (GdaBlobOp *op);
static glong gda_postgres_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_postgres_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

typedef struct {
	GdaConnection *cnc;
	Oid            blobid;  /* SQL ID in database */
	gint           fd;      /* to use with lo_read, lo_write, lo_lseek, lo_tell, and lo_close */
} GdaPostgresBlobOpPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaPostgresBlobOp, gda_postgres_blob_op, GDA_TYPE_BLOB_OP)

static void
gda_postgres_blob_op_init (GdaPostgresBlobOp *op)
{
	g_return_if_fail (GDA_IS_POSTGRES_BLOB_OP (op));

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (op);
	priv->blobid = InvalidOid;
	priv->fd = -1;
}

static void
gda_postgres_blob_op_class_init (GdaPostgresBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	object_class->dispose = gda_postgres_blob_op_dispose;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_postgres_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_postgres_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_postgres_blob_op_write;
}

static PGconn *
get_pconn (GdaConnection *cnc)
{
	PostgresConnectionData *cdata;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return NULL;

	return cdata->pconn;
}

static gboolean
blob_op_open (GdaPostgresBlobOp *pgop)
{
	gboolean use_svp = FALSE;
  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	if (priv->blobid == InvalidOid)
		return FALSE;
	if (priv->fd >= 0)
		return TRUE;

	if (gda_connection_get_transaction_status (priv->cnc))
		/* add a savepoint to prevent a blob open failure from rendering the transaction unusable */
		use_svp = TRUE;

	if (use_svp)
		use_svp = gda_connection_add_savepoint (priv->cnc, "__gda_blob_read_svp", NULL);
	
	priv->fd = lo_open (get_pconn (priv->cnc), priv->blobid, INV_READ | INV_WRITE);
	if (priv->fd < 0) {
		_gda_postgres_make_error (priv->cnc, get_pconn (priv->cnc), NULL, NULL);
		if (use_svp)
			gda_connection_rollback_savepoint (priv->cnc, "__gda_blob_read_svp", NULL);
		return FALSE;
	}
	if (use_svp)
		gda_connection_delete_savepoint (priv->cnc, "__gda_blob_read_svp", NULL);
	return TRUE;
}

static void
blob_op_close (GdaPostgresBlobOp *pgop)
{
  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);
	lo_close (get_pconn (priv->cnc), priv->fd);
	priv->fd = -1;
}

static void
gda_postgres_blob_op_dispose (GObject * object)
{
	GdaPostgresBlobOp *pgop = (GdaPostgresBlobOp *) object;

	g_return_if_fail (GDA_IS_POSTGRES_BLOB_OP (pgop));
  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	if (priv->fd >= 0) {
		lo_close (get_pconn (priv->cnc), priv->fd);
    if (priv->cnc != NULL) {
      g_object_unref (priv->cnc);
      priv->cnc = NULL;
    }
  }

	G_OBJECT_CLASS(gda_postgres_blob_op_parent_class)->finalize (object);
}

GdaBlobOp *
gda_postgres_blob_op_new (GdaConnection *cnc)
{
	GdaPostgresBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_POSTGRES_BLOB_OP, "connection", cnc, NULL);

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

  priv->cnc = g_object_ref (cnc);
	
	return GDA_BLOB_OP (pgop);
}

GdaBlobOp *
gda_postgres_blob_op_new_with_id (GdaConnection *cnc, const gchar *sql_id)
{
	GdaPostgresBlobOp *pgop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_POSTGRES_BLOB_OP, "connection", cnc, NULL);

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	priv->blobid = atoi (sql_id);
	priv->cnc = g_object_ref (cnc);

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

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	if (priv->blobid == InvalidOid) {
		PGconn *pconn = get_pconn (priv->cnc);
		priv->blobid = lo_creat (pconn, INV_READ | INV_WRITE);
		if (priv->blobid == InvalidOid) {
			_gda_postgres_make_error (priv->cnc, pconn, NULL, NULL);
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

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	if (priv->blobid == InvalidOid)
		return NULL;
	else
		return g_strdup_printf ("%d", priv->blobid);
}

/**
 * gda_postgres_blob_op_set_id
 */
void
gda_postgres_blob_op_set_id (GdaPostgresBlobOp *pgop, const gchar *sql_id)
{
	g_return_if_fail (GDA_IS_POSTGRES_BLOB_OP (pgop));
	g_return_if_fail (sql_id);

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	if (priv->fd >= 0)
		blob_op_close (pgop);
	priv->blobid = atoi (sql_id);
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
  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	if (! check_transaction_started (priv->cnc, &transaction_started))
		return -1;
	
	if (!blob_op_open (pgop))
		goto out_error;

	pconn = get_pconn (priv->cnc);
	pos = lo_lseek (pconn, priv->fd, 0, SEEK_END);

	if (pos < 0) {
		_gda_postgres_make_error (priv->cnc, pconn, NULL, NULL);
		goto out_error;
	}

	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);

	return pos;

 out_error:
	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);

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
  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

  g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	if (! check_transaction_started (priv->cnc, &transaction_started))
		return -1;

	if (!blob_op_open (pgop))
		goto out_error;

	pconn = get_pconn (priv->cnc);
	if (lo_lseek (pconn, priv->fd, offset, SEEK_SET) < 0) {
		_gda_postgres_make_error (priv->cnc, pconn, NULL, NULL);
		goto out_error;
	}

	bin = gda_blob_get_binary (blob);
	gda_binary_reset_data (bin);
	guchar *buffer = g_new0 (guchar, size);
	glong l = lo_read (pconn, priv->fd, (char *) (buffer), size);
	gda_binary_set_data (bin, buffer, l);

	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);

	return gda_binary_get_size (bin);

 out_error:
	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);

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

  GdaPostgresBlobOpPrivate *priv = gda_postgres_blob_op_get_instance_private (pgop);

	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	if (! check_transaction_started (priv->cnc, &transaction_started))
		return -1;

	if (!blob_op_open (pgop))
		goto out_error;

	pconn = get_pconn (priv->cnc);
	if (lo_lseek (pconn, priv->fd, offset, SEEK_SET) < 0) {
		_gda_postgres_make_error (priv->cnc, pconn, NULL, NULL);
		goto out_error;
	}

	if (gda_blob_get_op (blob) && (gda_blob_get_op (blob) != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = gda_blob_new ();
		gda_blob_set_op (tmpblob, gda_blob_get_op (blob));

		nbwritten = 0;

		for (nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, 0, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, nbwritten, buf_size)) {
			GdaBinary *bin = gda_blob_get_binary (tmpblob);
			glong tmp_written;
			tmp_written = lo_write (pconn, priv->fd, (char*) gda_binary_get_data (bin),
						gda_binary_get_size (bin));
			if (tmp_written < gda_binary_get_size (bin)) {
				_gda_postgres_make_error (priv->cnc, pconn, NULL, NULL);
				gda_blob_free (tmpblob);
				goto out_error;
			}
			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}
		gda_blob_free (tmpblob);
	}
	else {
		/* use data in (GdaBinary *) blob */
		GdaBinary *bin = gda_blob_get_binary (blob);
		nbwritten = lo_write (pconn, priv->fd, (char*) gda_binary_get_data (bin), gda_binary_get_size (bin));
		if (nbwritten == -1) {
			_gda_postgres_make_error (priv->cnc, pconn, NULL, NULL);
			goto out_error;
		}
	}

	blob_op_close (pgop);
	if (transaction_started)
		if (!gda_connection_commit_transaction (priv->cnc, NULL, NULL))
			return -1;

	return nbwritten;

 out_error:
	blob_op_close (pgop);
	if (transaction_started)
		gda_connection_rollback_transaction (priv->cnc, NULL, NULL);
	return -1;
}
