/* GDA Postgres blob
 * Copyright (C) 2005 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-blob-op.h"
#include <libpq/libpq-fs.h>

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_BLOB_OP

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

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaPostgresBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresBlobOp),
			0,
			(GInstanceInitFunc) gda_postgres_blob_op_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaPostgresBlobOp", &info, 0);
	}
	return type;
}

static void
gda_postgres_blob_op_init (GdaPostgresBlobOp *op,
			   GdaPostgresBlobOpClass *klass)
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
	GdaPostgresConnectionData *priv_data;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	return priv_data->pconn;
}

static gboolean
blob_op_open (GdaPostgresBlobOp *pgop)
{
	if (pgop->priv->blobid == InvalidOid)
		return FALSE;
	if (pgop->priv->fd >= 0)
		return TRUE;

	/* add a savepoint to prevent a blob open failure from rendering the transaction unuseable */
	gda_connection_add_savepoint (pgop->priv->cnc, "__gda_blob_read_svp", NULL);
	pgop->priv->fd = lo_open (get_pconn (pgop->priv->cnc), pgop->priv->blobid, INV_READ | INV_WRITE);
	if (pgop->priv->fd < 0) {
		gda_connection_rollback_savepoint (pgop->priv->cnc, "__gda_blob_read_svp", NULL);
		return FALSE;
	}
	gda_connection_delete_savepoint (pgop->priv->cnc, "__gda_blob_read_svp", NULL);
	return TRUE;
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
	PGconn *pconn;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	pgop = g_object_new (GDA_TYPE_POSTGRES_BLOB_OP, NULL);

	pconn = get_pconn (cnc);
	pgop->priv->blobid = atoi (sql_id);
	pgop->priv->cnc = cnc;
	blob_op_open (pgop);

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
			gda_postgres_make_error (pgop->priv->cnc, pconn, NULL);
			return FALSE;
		}
	}
	
	if (!blob_op_open (pgop))
		return FALSE;
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

	if (pgop->priv->fd >= 0) {
		lo_close (get_pconn (pgop->priv->cnc), pgop->priv->fd);
		pgop->priv->fd = 0;
	}
	pgop->priv->blobid = atoi (sql_id);
	blob_op_open (pgop);
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

	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (op), -1);
	pgop = GDA_POSTGRES_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	
	if (!blob_op_open (pgop))
		return -1;
	pconn = get_pconn (pgop->priv->cnc);
	pos = lo_lseek (pconn, pgop->priv->fd, 0, SEEK_END);
        if (pos < 0)
                return -1;
        else
                return pos;
}

static glong
gda_postgres_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaPostgresBlobOp *pgop;
	PGconn *pconn;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (op), -1);
	pgop = GDA_POSTGRES_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	if (!blob_op_open (pgop))
		return -1;

	pconn = get_pconn (pgop->priv->cnc);
	if (lo_lseek (pconn, pgop->priv->fd, offset, SEEK_SET) < 0) {
		gda_postgres_make_error (pgop->priv->cnc, pconn, NULL);
		return -1;
	}

	bin = (GdaBinary *) blob;
	if (bin->data) 
		g_free (bin->data);
	bin->data = g_new0 (gchar, size);
	bin->binary_length = lo_read (pconn, pgop->priv->fd, (char *) (bin->data), size);
	return bin->binary_length;
}

static glong
gda_postgres_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaPostgresBlobOp *pgop;
	PGconn *pconn;
	GdaBinary *bin;
	glong nbwritten;

	g_return_val_if_fail (GDA_IS_POSTGRES_BLOB_OP (op), -1);
	pgop = GDA_POSTGRES_BLOB_OP (op);
	g_return_val_if_fail (pgop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (pgop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	if (!blob_op_open (pgop))
		return -1;

	pconn = get_pconn (pgop->priv->cnc);
	if (lo_lseek (pconn, pgop->priv->fd, offset, SEEK_SET) < 0) {
		gda_postgres_make_error (pgop->priv->cnc, pconn, NULL);
		return -1;
	}

	bin = (GdaBinary *) blob;
	nbwritten = lo_write (pconn, pgop->priv->fd, (char*) bin->data, bin->binary_length);
	if (nbwritten == -1) {
		gda_postgres_make_error (pgop->priv->cnc, pconn, NULL);
		return -1;
	}

	return nbwritten;
}
