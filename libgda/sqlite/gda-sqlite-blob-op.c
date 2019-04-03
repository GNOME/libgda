/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/libgda.h>
#include "gda-sqlite.h"
#include "gda-sqlite-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include "gda-sqlite-util.h"
#include <sql-parser/gda-sql-parser.h>


static void gda_sqlite_blob_op_class_init (GdaSqliteBlobOpClass *klass);
static void gda_sqlite_blob_op_init       (GdaSqliteBlobOp *blob);
static void gda_sqlite_blob_op_finalize   (GObject *object);

static glong gda_sqlite_blob_op_get_length (GdaBlobOp *op);
static glong gda_sqlite_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_sqlite_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

typedef struct {
	GWeakRef      provider;
	sqlite3_blob  *sblob;
} GdaSqliteBlobOpPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaSqliteBlobOp, gda_sqlite_blob_op, GDA_TYPE_BLOB_OP)

static void
gda_sqlite_blob_op_init (GdaSqliteBlobOp *op)
{
	g_return_if_fail (GDA_IS_SQLITE_BLOB_OP (op));
  GdaSqliteBlobOpPrivate *priv = gda_sqlite_blob_op_get_instance_private (op);

	priv->sblob = NULL;
	g_weak_ref_init (&priv->provider, NULL);
}

static void
gda_sqlite_blob_op_class_init (GdaSqliteBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	object_class->finalize = gda_sqlite_blob_op_finalize;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_sqlite_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_sqlite_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_sqlite_blob_op_write;
}

static void
gda_sqlite_blob_op_finalize (GObject * object)
{
	GdaSqliteBlobOp *bop = (GdaSqliteBlobOp *) object;

	g_return_if_fail (GDA_IS_SQLITE_BLOB_OP (bop));
  GdaSqliteBlobOpPrivate *priv = gda_sqlite_blob_op_get_instance_private (bop);

	/* free specific information */
	if (priv->sblob) {
		GdaSqliteProvider *prov = g_weak_ref_get (&priv->provider);
		if (prov != NULL) {
			SQLITE3_CALL (prov, sqlite3_blob_close) (priv->sblob);
			g_object_unref (prov);
		}
#ifdef GDA_DEBUG_NO
		g_print ("CLOSED blob %p\n", bop);
#endif
	}
	g_weak_ref_clear (&priv->provider);

	G_OBJECT_CLASS (gda_sqlite_blob_op_parent_class)->finalize (object);
}

GdaBlobOp *
_gda_sqlite_blob_op_new (GdaConnection *cnc,
			 const gchar *db_name, const gchar *table_name,
			 const gchar *column_name, sqlite3_int64 rowid)
{
	GdaSqliteBlobOp *bop = NULL;
	int rc;
	sqlite3_blob *sblob;
	gchar *db, *table;
	gboolean free_strings = TRUE;
	gboolean transaction_started = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (table_name, NULL);
	g_return_val_if_fail (column_name, NULL);
	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (gda_connection_get_provider (cnc)), NULL);
  GdaSqliteBlobOpPrivate *priv = gda_sqlite_blob_op_get_instance_private (bop);

	if (db_name) {
		db = (gchar *) db_name;
		table = (gchar *) table_name;
		free_strings = FALSE;
	}
	else if (! _split_identifier_string (g_strdup (table_name), &db, &table))
		return NULL;

	SqliteConnectionData *cdata;
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata ||
	    ! _gda_sqlite_check_transaction_started (cnc, &transaction_started, NULL))
		return NULL;

	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (gda_connection_get_provider (cnc));

	rc = SQLITE3_CALL (prov, sqlite3_blob_open) (cdata->connection,
	                                       db ? db : "main",
	                                       table, column_name, rowid,
	                                       1, /* Read & Write */
	                                       &(sblob));
	if (rc != SQLITE_OK) {
#ifdef GDA_DEBUG_NO
		g_print ("ERROR: %s\n", SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection));
#endif
		if (transaction_started)
			gda_connection_rollback_transaction (cnc, NULL, NULL);
		goto out;
	}

	bop = g_object_new (GDA_TYPE_SQLITE_BLOB_OP, "connection", cnc, NULL);
	priv->sblob = sblob;
	g_weak_ref_set (&priv->provider, prov);
#ifdef GDA_DEBUG_NO
	g_print ("OPENED blob %p\n", bop);
#endif

 out:
	if (free_strings) {
		g_free (db);
		g_free (table);
	}
	return (GdaBlobOp*) bop;
}

/*
 * Get length request
 */
static glong
gda_sqlite_blob_op_get_length (GdaBlobOp *op)
{
	GdaSqliteBlobOp *bop;
	int len;

	g_return_val_if_fail (GDA_IS_SQLITE_BLOB_OP (op), -1);
	bop = GDA_SQLITE_BLOB_OP (op);
  GdaSqliteBlobOpPrivate *priv = gda_sqlite_blob_op_get_instance_private (bop);
	g_return_val_if_fail (priv->sblob, -1);
	GdaSqliteProvider *prov = g_weak_ref_get (&priv->provider);
	g_return_val_if_fail (prov != NULL, -1);

	len = SQLITE3_CALL (prov, sqlite3_blob_bytes) (priv->sblob);
	g_object_unref (prov);
	return len >= 0 ? len : 0;
}

/*
 * Blob read request
 */
static glong
gda_sqlite_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaSqliteBlobOp *bop;
	GdaBinary *bin;
	gpointer buffer = NULL;
	int rc;

	g_return_val_if_fail (GDA_IS_SQLITE_BLOB_OP (op), -1);
	bop = GDA_SQLITE_BLOB_OP (op);
  GdaSqliteBlobOpPrivate *priv = gda_sqlite_blob_op_get_instance_private (bop);
	g_return_val_if_fail (priv->sblob, -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	if (offset > G_MAXINT)
		return -1;
	if (size > G_MAXINT)
		return -1;

	GdaSqliteProvider *prov = g_weak_ref_get (&priv->provider);
	g_return_val_if_fail (prov != NULL, -1);

	bin = gda_blob_get_binary (blob);
	gda_binary_set_data (bin, (guchar*) "", 0);

	/* fetch blob data using C API into bin->data, and set bin->binary_length */
	int rsize;
	int len;

	len = SQLITE3_CALL (prov, sqlite3_blob_bytes) (priv->sblob);
	if (len < 0){
		g_object_unref (prov);
		return -1;
	} else if (len == 0) {
		g_object_unref (prov);
		return 0;
	}
		
	rsize = (int) size;
	if (offset >= len) {
		g_object_unref (prov);
		return -1;
	}

	if (len - offset < rsize)
		rsize = len - offset;
  
	rc = SQLITE3_CALL (prov, sqlite3_blob_read) (priv->sblob, buffer, rsize, offset);
	if (rc != SQLITE_OK) {
		gda_binary_reset_data (bin);
		g_object_unref (prov);
		return -1;
	}
	gda_binary_set_data (bin, buffer, rsize);
	g_object_unref (prov);

	return gda_binary_get_size (bin);
}

/*
 * Blob write request
 */
static glong
gda_sqlite_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaSqliteBlobOp *bop;
	GdaBinary *bin;
	glong nbwritten = -1;
	int len;

	g_return_val_if_fail (GDA_IS_SQLITE_BLOB_OP (op), -1);
	bop = GDA_SQLITE_BLOB_OP (op);
  GdaSqliteBlobOpPrivate *priv = gda_sqlite_blob_op_get_instance_private (bop);
	g_return_val_if_fail (priv->sblob, -1);
	g_return_val_if_fail (blob, -1);

	GdaSqliteProvider *prov = g_weak_ref_get (&priv->provider);
	g_return_val_if_fail (prov != NULL, -1);

	len = SQLITE3_CALL (prov, sqlite3_blob_bytes) (priv->sblob);
	if (len < 0) {
		g_object_unref (prov);
		return -1;
	}

	if (gda_blob_get_op (blob) && (gda_blob_get_op (blob) != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob = gda_blob_new ();
		gda_blob_set_op (tmpblob, gda_blob_get_op (blob));

		nbwritten = 0;

		for (nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, nbwritten, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (gda_blob_get_op (tmpblob), tmpblob, nbwritten, buf_size)) {
			int tmp_written;
			int rc;
			int wlen;
			
			if (nread + offset + nbwritten > len)
				wlen = 	len - offset - nbwritten;
			else
				wlen = nread;

			rc = SQLITE3_CALL (prov, sqlite3_blob_write) (priv->sblob,
								gda_binary_get_data (gda_blob_get_binary (tmpblob)), wlen, offset + nbwritten);
			if (rc != SQLITE_OK)
				tmp_written = -1;
			else
				tmp_written = wlen;
			
			if (tmp_written < 0) {
				/* treat error */
				gda_blob_free ((gpointer) tmpblob);
				g_object_unref (prov);
				return -1;
			}
			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}
		gda_blob_free ((gpointer) tmpblob);
	}
	else {
		/* write blob using bin->data and bin->binary_length */
		int rc;
		int wlen;
		bin = gda_blob_get_binary (blob);
		if (gda_binary_get_size (bin) + offset > len)
			wlen = 	len - offset;
		else
			wlen = gda_binary_get_size (bin);

		rc = SQLITE3_CALL (prov, sqlite3_blob_write) (priv->sblob, gda_binary_get_data (bin), wlen, offset);
		if (rc != SQLITE_OK)
			nbwritten = -1;
		else
			nbwritten = wlen;
	}
	g_object_unref (prov);

	return nbwritten;
}
