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

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_oracle_blob_op_get_type (void)
{
	static GType type = 0;

	if (!type) {
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
}

static void
gda_oracle_blob_op_finalize (GObject * object)
{
	GdaOracleBlobOp *op = (GdaOracleBlobOp *) object;

	g_return_if_fail (GDA_IS_ORACLE_BLOB_OP (op));

	g_free (op->priv);
	op->priv = NULL;

	parent_class->finalize (object);
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


GdaBlobOp *
gda_oracle_blob_op_new (GdaConnection *cnc)
{
	GdaOracleBlobOp *blob;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	blob = g_object_new (GDA_TYPE_ORACLE_BLOB_OP, NULL);
	blob->priv->cnc = cnc;
	
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

	return -1;
}

static glong
gda_oracle_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaOracleBlobOp *oraop;
	GdaBinary *bin;
	glong nbwritten;

	g_return_val_if_fail (GDA_IS_ORACLE_BLOB_OP (op), -1);
	oraop = GDA_ORACLE_BLOB_OP (op);
	g_return_val_if_fail (oraop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (oraop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	return -1;
}
