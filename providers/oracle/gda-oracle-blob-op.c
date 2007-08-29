/* GDA Postgres blob
 * Copyright (C) 2007 The GNOME Foundation
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
#include "gda-oracle.h"
#include "gda-oracle-blob-op.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_BLOB_OP

struct _GdaOracleBlobOpPrivate {
	GdaConnection *cnc;
	OCILobLocator *lobl;
};

static void gda_oracle_blob_op_class_init (GdaOracleBlobOpClass *klass);
static void gda_oracle_blob_op_init       (GdaOracleBlobOp *blob,
					  GdaOracleBlobOpClass *klass);
static void gda_oracle_blob_op_finalize   (GObject *object);

static glong gda_oracle_blob_op_get_length (GdaBlobOp *op);
static glong gda_oracle_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_oracle_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);
static gboolean gda_oracle_blob_op_write_all (GdaBlobOp *op, GdaBlob *blob);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_oracle_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaOracleBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaOracleBlobOp),
			0,
			(GInstanceInitFunc) gda_oracle_blob_op_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaOracleBlobOp", &info, 0);
	}
	return type;
}

static void
gda_oracle_blob_op_init (GdaOracleBlobOp *op,
			   GdaOracleBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_ORACLE_BLOB_OP (op));

	op->priv = g_new0 (GdaOracleBlobOpPrivate, 1);
	op->priv->lobl = NULL;
}

static void
gda_oracle_blob_op_class_init (GdaOracleBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_oracle_blob_op_finalize;
	blob_class->get_length = gda_oracle_blob_op_get_length;
	blob_class->read = gda_oracle_blob_op_read;
	blob_class->write = gda_oracle_blob_op_write;
	blob_class->write_all = gda_oracle_blob_op_write_all;
}

static GdaOracleConnectionData *
get_conn_data (GdaConnection *cnc)
{
	GdaOracleConnectionData *priv_data;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ORACLE_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
		return NULL;
	}

	return priv_data;
}

static void
gda_oracle_blob_op_finalize (GObject * object)
{
	GdaOracleBlobOp *op = (GdaOracleBlobOp *) object;

	g_return_if_fail (GDA_IS_ORACLE_BLOB_OP (op));

	if (op->priv->lobl) {
		GdaOracleConnectionData *priv_data = get_conn_data (op->priv->cnc);
		g_assert (priv_data);

		OCILobClose (priv_data->hservice, priv_data->herr, op->priv->lobl);
	}
	g_free (op->priv);
	op->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_oracle_blob_op_new (GdaConnection *cnc, OCILobLocator *lobloc)
{
	GdaOracleBlobOp *blob;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (lobloc, NULL);

	blob = g_object_new (GDA_TYPE_ORACLE_BLOB_OP, NULL);
	blob->priv->cnc = cnc;
	blob->priv->lobl = lobloc;
	
	return GDA_BLOB_OP (blob);
}


/*
 * Virtual functions
 */
static glong
gda_oracle_blob_op_get_length (GdaBlobOp *op)
{
	GdaOracleBlobOp *oraop;
	ub4 loblen = 0;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	oraop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (oraop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (oraop->priv->cnc), -1);

	if (oraop->priv->lobl) {
		gint result;
		GdaConnectionEvent *error;
		GdaOracleConnectionData *priv_data = get_conn_data (oraop->priv->cnc);
		g_assert (priv_data);

		result = OCILobGetLength (priv_data->hservice, priv_data->herr, oraop->priv->lobl, &loblen);
		error = gda_oracle_check_result (result, oraop->priv->cnc, priv_data, OCI_HTYPE_ERROR,
						 _("Could not get the length of LOB object"));
		if (!error)
			return loblen;
	}
	return -1;
}

static glong
gda_oracle_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaOracleBlobOp *oraop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	oraop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (oraop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (oraop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	bin = (GdaBinary *) blob;
	if (bin->data) {
		g_free (bin->data);
		bin->data = NULL;
		bin->binary_length = 0;
	}

	if (oraop->priv->lobl) {
		sword result;
		ub4 nbbytes;
		GdaOracleConnectionData *priv_data = get_conn_data (oraop->priv->cnc);
		ub4 loblen = 0;
		ub1 *buf;
		g_assert (priv_data);

		result = OCILobGetLength (priv_data->hservice, priv_data->herr, oraop->priv->lobl, &loblen);
		if (gda_oracle_check_result (result, oraop->priv->cnc, priv_data, OCI_HTYPE_ERROR,
					     _("Could not get the length of LOB object")))
			return -1;

		nbbytes = (size > loblen) ? loblen : size;
		buf = g_new (ub1, nbbytes);
		/*result = OCILobRead2 (priv_data->hservice, priv_data->herr, oraop->priv->lobl, &nbbytes, 0,
				      offset + 1, (dvoid *) buf, nbbytes, OCI_ONE_PIECE, 
				      NULL, NULL, 0, SQLCS_IMPLICIT);*/
		result = OCILobRead (priv_data->hservice, priv_data->herr, oraop->priv->lobl, &nbbytes,
				      offset + 1, (dvoid *) buf, nbbytes, 
				      NULL, NULL, 0, SQLCS_IMPLICIT);
		if (gda_oracle_check_result (result, oraop->priv->cnc, priv_data, OCI_HTYPE_ERROR,
					     _("Could not read from LOB object"))) {
			g_free (buf);
			return -1;
		}
		bin->data = buf;
		bin->binary_length = nbbytes;
		return bin->binary_length;
	}

	return -1;
}

static glong
gda_oracle_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaOracleBlobOp *oraop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	oraop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (oraop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (oraop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	bin = (GdaBinary *) blob;

	if (oraop->priv->lobl) {
		sword result;
		ub4 nbbytes;
		GdaOracleConnectionData *priv_data = get_conn_data (oraop->priv->cnc);
		g_assert (priv_data);

		nbbytes = bin->binary_length;
		result = OCILobWrite (priv_data->hservice, priv_data->herr, oraop->priv->lobl, &nbbytes, (ub4) offset + 1,
				      (dvoid *) bin->data, bin->binary_length,
				      OCI_ONE_PIECE, NULL, NULL, 0, SQLCS_IMPLICIT);
		if (gda_oracle_check_result (result, oraop->priv->cnc, priv_data, OCI_HTYPE_ERROR,
					     _("Could not write to LOB object"))) {
			return -1;
		}
		return nbbytes;
	}

	return -1;
}

static gboolean
gda_oracle_blob_op_write_all (GdaBlobOp *op, GdaBlob *blob)
{
	GdaOracleBlobOp *oraop;
	GdaBinary *bin;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), FALSE);
	oraop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (oraop->priv, FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (oraop->priv->cnc), FALSE);
	g_return_val_if_fail (blob, FALSE);

	bin = (GdaBinary *) blob;
	if (oraop->priv->lobl) {
		sword result;
		GdaOracleConnectionData *priv_data = get_conn_data (oraop->priv->cnc);
		g_assert (priv_data);

		if (gda_oracle_blob_op_write (op, blob, 0) != bin->binary_length)
			return FALSE;
		else {
			/* truncate lob at the end */
			result = OCILobTrim (priv_data->hservice, priv_data->herr, oraop->priv->lobl, bin->binary_length);
			if (gda_oracle_check_result (result, oraop->priv->cnc, priv_data, OCI_HTYPE_ERROR,
						     _("Could not truncate LOB object")))
				return FALSE;
			else
				return TRUE;
		}
	}

	return FALSE;
}
