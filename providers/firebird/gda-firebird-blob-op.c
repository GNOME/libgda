/* GDA Firebird blob
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
#include "gda-firebird-blob-op.h"
#include "gda-firebird-recordset.h"
#include "gda-firebird-provider.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_BLOB_OP
#define INFO_BUFFER_SIZE 32

struct _GdaFirebirdBlobOpPrivate {
	GdaConnection  *cnc;
	ISC_QUAD        blob_id;
	isc_blob_handle blob_handle; /* NULL if blob not opened */
	
	ISC_LONG        nb_segments;
	ISC_LONG        max_segment_size;
	ISC_LONG        blob_length;
	int             blob_is_stream;

	gpointer        trans_status;  /* initial transaction status when blob was created */
	gboolean        trans_changed; /* if TRUE, then reading BLOB won't be possible anymore */
};

static void gda_firebird_blob_op_class_init (GdaFirebirdBlobOpClass *klass);
static void gda_firebird_blob_op_init       (GdaFirebirdBlobOp *blob,
					  GdaFirebirdBlobOpClass *klass);
static void gda_firebird_blob_op_finalize   (GObject *object);

static glong gda_firebird_blob_op_get_length (GdaBlobOp *op);
static glong gda_firebird_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_firebird_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaFirebirdBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdBlobOp),
			0,
			(GInstanceInitFunc) gda_firebird_blob_op_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaFirebirdBlobOp", &info, 0);
	}
	return type;
}

static void
gda_firebird_blob_op_init (GdaFirebirdBlobOp *op,
			   GdaFirebirdBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op));

	op->priv = g_new0 (GdaFirebirdBlobOpPrivate, 1);
	op->priv->blob_handle = NULL;
	memset (&(op->priv->blob_id), 0, sizeof (ISC_QUAD));
	op->priv->max_segment_size = 0;
	op->priv->blob_length = 0;
	op->priv->nb_segments = 0;
	op->priv->blob_is_stream = FALSE;
	op->priv->trans_status = NULL;
	op->priv->trans_changed = FALSE;
}

static void
gda_firebird_blob_op_class_init (GdaFirebirdBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_firebird_blob_op_finalize;
	blob_class->get_length = gda_firebird_blob_op_get_length;
	blob_class->read = gda_firebird_blob_op_read;
	blob_class->write = NULL; /* to write to a BLOB, one must create another BLOB and do an UPDATE query */
}

static GdaFirebirdConnection *
get_fcnc (GdaConnection *cnc)
{
	GdaFirebirdConnection *fcnc;

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) 
		gda_connection_add_event_string (cnc, _("Invalid FirebirdQL handle"));

	return fcnc;
}

static gboolean
blob_op_open (GdaFirebirdBlobOp *blobop)
{
	GdaFirebirdConnection *fcnc;
	isc_tr_handle *ftr;

	if (blobop->priv->blob_handle)
		return TRUE;

	fcnc = get_fcnc (blobop->priv->cnc);
	ftr = g_object_get_data (G_OBJECT (blobop->priv->cnc), TRANSACTION_DATA);

	if (isc_open_blob2 (fcnc->status, &(fcnc->handle), ftr, &(blobop->priv->blob_handle),
			    &(blobop->priv->blob_id),
			    0,        /* BPB length = 0; no filter will be used */
			    NULL      /* NULL BPB, since no filter will be used */)) {
		gda_firebird_connection_make_error (blobop->priv->cnc, 0);
                return FALSE;
	}

	return TRUE;
}

static void
blob_op_close (GdaFirebirdBlobOp *blobop)
{
	GdaFirebirdConnection *fcnc;
	if (!blobop->priv->blob_handle)
		return;

	fcnc = get_fcnc (blobop->priv->cnc);
	isc_close_blob (fcnc->status, &(blobop->priv->blob_handle));
	blobop->priv->blob_handle = NULL;
}

static gboolean
gda_firebird_blob_op_get_blob_attrs (GdaFirebirdBlobOp *blobop)
{	
	if (blobop->priv->max_segment_size > 0)
		return TRUE; /* attributes already computed */

	if (blobop->priv->trans_changed) {
		gda_connection_add_event_string (blobop->priv->cnc, _("Transaction changed, BLOB can't be accessed"));
		return FALSE;
	}

	if (!blob_op_open (blobop))
		return FALSE;

	char blob_items[]= {isc_info_blob_max_segment, isc_info_blob_total_length, 
			    isc_info_blob_num_segments, isc_info_blob_type };
	char res_buffer[INFO_BUFFER_SIZE], *res_buffer_ptr;
	GdaFirebirdConnection *fcnc;

	fcnc = get_fcnc (blobop->priv->cnc);

	/* Get blob info */
	if (isc_blob_info (fcnc->status, &(blobop->priv->blob_handle),
			   sizeof (blob_items), blob_items,
			   sizeof (res_buffer), res_buffer)) {
		gda_firebird_connection_make_error (blobop->priv->cnc, 0);
		blob_op_close (blobop);
                return FALSE;
	}

	/* Parse blob info data */
	for (res_buffer_ptr= res_buffer; *res_buffer_ptr != isc_info_end; ) {
                char item= *res_buffer_ptr++;
                if (item == isc_info_blob_max_segment) {
			int length= isc_vax_integer (res_buffer_ptr, 2);
			res_buffer_ptr+= 2;
			blobop->priv->max_segment_size = isc_vax_integer (res_buffer_ptr, length);
			res_buffer_ptr+= length;
                }
		else if (item == isc_info_blob_total_length) {
			int length= isc_vax_integer (res_buffer_ptr, 2);
			res_buffer_ptr+= 2;
			blobop->priv->blob_length = isc_vax_integer (res_buffer_ptr, length);
			res_buffer_ptr+= length;
                }
		else if (item == isc_info_blob_num_segments) {
			int length= isc_vax_integer (res_buffer_ptr, 2);
			res_buffer_ptr+= 2;
			blobop->priv->nb_segments = isc_vax_integer (res_buffer_ptr, length);
			res_buffer_ptr+= length;
                }
		else if (item == isc_info_blob_type) {
			int length= isc_vax_integer (res_buffer_ptr, 2);
			res_buffer_ptr+= 2;
			blobop->priv->blob_is_stream = isc_vax_integer (res_buffer_ptr, length) ? TRUE : FALSE;
			res_buffer_ptr+= length;
			if (blobop->priv->blob_is_stream)
				g_warning ("Stream BLOBs are not currently handled");
                }
	}

	blob_op_close (blobop);
	return TRUE;
}

static void
cnc_transaction_status_changed_cb (GdaConnection *cnc, GdaFirebirdBlobOp *blobop)
{
	GdaTransactionStatus *ts;

	ts = gda_connection_get_transaction_status (cnc);
	if (ts != blobop->priv->trans_status)
		blobop->priv->trans_changed = TRUE;
}

static void
gda_firebird_blob_op_finalize (GObject * object)
{
	GdaFirebirdBlobOp *blobop = (GdaFirebirdBlobOp *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_BLOB_OP (blobop));

	g_signal_handlers_disconnect_by_func (G_OBJECT (blobop->priv->cnc),
					      G_CALLBACK (cnc_transaction_status_changed_cb), blobop);
	blob_op_close (blobop);

	g_free (blobop->priv);
	blobop->priv = NULL;

	parent_class->finalize (object);
}

GdaBlobOp *
gda_firebird_blob_op_new (GdaConnection *cnc)
{
	GdaFirebirdBlobOp *blobop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	blobop = g_object_new (GDA_TYPE_FIREBIRD_BLOB_OP, NULL);
	blobop->priv->cnc = cnc;
	blobop->priv->trans_status = gda_connection_get_transaction_status (cnc);

	g_signal_connect (G_OBJECT (cnc), "transaction-status-changed",
			  G_CALLBACK (cnc_transaction_status_changed_cb), blobop);
	
	return GDA_BLOB_OP (blobop);
}

GdaBlobOp *
gda_firebird_blob_op_new_with_id (GdaConnection *cnc, const ISC_QUAD *blob_id)
{
	GdaFirebirdBlobOp *blobop;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	blobop = g_object_new (GDA_TYPE_FIREBIRD_BLOB_OP, NULL);

	blobop->priv->blob_id = *blob_id;
	blobop->priv->cnc = cnc;
	blobop->priv->trans_status = gda_connection_get_transaction_status (cnc);

	g_signal_connect (G_OBJECT (cnc), "transaction-status-changed",
			  G_CALLBACK (cnc_transaction_status_changed_cb), blobop);

	return GDA_BLOB_OP (blobop);
}

/*
 * Virtual functions
 */
static glong
gda_firebird_blob_op_get_length (GdaBlobOp *op)
{
	GdaFirebirdBlobOp *blobop;

	g_return_val_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op), -1);
	blobop = GDA_FIREBIRD_BLOB_OP (op);
	g_return_val_if_fail (blobop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (blobop->priv->cnc), -1);
	
	/* use value if already computed */
	if (blobop->priv->blob_length > 0)
		return blobop->priv->blob_length;

	if (gda_firebird_blob_op_get_blob_attrs (blobop))
		return blobop->priv->blob_length;
	else
		return -1;
}

/*
 * Reads a complete segment from the BLOB
 * Returns: a new GdaBinary structure
 */
static GdaBinary *
blob_op_read_segment (GdaFirebirdBlobOp *blobop, GdaFirebirdConnection *fcnc, unsigned short segment_size)
{
	GdaBinary *bin;
	char *blob_segment;
	unsigned short actual_seg_length;

	bin = g_new0 (GdaBinary, 1);

	blob_segment = g_new0 (char, segment_size);
	if (isc_get_segment (fcnc->status, &(blobop->priv->blob_handle),
			     &actual_seg_length, /* length of segment read */
			     segment_size, /* length of segment buffer */
			     blob_segment/* segment buffer */)) {
		gda_firebird_connection_make_error (blobop->priv->cnc, 0);
		g_free (blob_segment);
		g_free (bin);
		blob_op_close (blobop);
                return NULL;
	}
	if (fcnc->status[1] == isc_segment) {
                /* isc_get_segment returns 0 if a segment was successfully read. */
                /* status_vector[1] is set to isc_segment if only part of a */
                /* segment was read due to the buffer (blob_segment) not being */
		/* large enough. In that case, the following calls to */
		/* isc_get_segment() read the rest of the buffer. */
		/* SHOULD NOT HAPPEN HERE since we should have a big enough buffer */
		TO_IMPLEMENT;
	}
	bin->data = blob_segment;
	bin->binary_length = actual_seg_length;
	return bin;
}


static glong
gda_firebird_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaFirebirdBlobOp *blobop;
	GdaBinary *bin;
	GdaFirebirdConnection *fcnc;

	g_return_val_if_fail (GDA_IS_FIREBIRD_BLOB_OP (op), -1);
	blobop = GDA_FIREBIRD_BLOB_OP (op);
	g_return_val_if_fail (blobop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (blobop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);

	if (blobop->priv->trans_changed) {
		gda_connection_add_event_string (blobop->priv->cnc, _("Transaction changed, BLOB can't be accessed"));
		return -1;
	}

	bin = (GdaBinary *) blob;
	if (bin->data) 
		g_free (bin->data);
	bin->data = NULL;
	bin->binary_length = 0;

	fcnc = get_fcnc (blobop->priv->cnc);	

	/* get the blob's segment size */
	if (!gda_firebird_blob_op_get_blob_attrs (blobop))
		return -1;

	if (!blob_op_open (blobop))
		return -1;

	/* set at offset */
	if (offset > 0) {
		TO_IMPLEMENT;
		blob_op_close (blobop);
		return -1;
	}

	/* read a segment */
	glong remaining_to_read;
	ISC_LONG nb_segments_read = 0;
	for (remaining_to_read = size; 
	     (remaining_to_read > 0) && (nb_segments_read < blobop->priv->nb_segments); 
	     nb_segments_read++) {
		GdaBinary *seg_bin;

		seg_bin = blob_op_read_segment (blobop, fcnc, blobop->priv->max_segment_size);
		if (!seg_bin) {
			if (bin->data) 
				g_free (bin->data);
			bin->data = NULL;
			bin->binary_length = 0;
			return -1;
		}
		if (!bin->data) {
			if (size <= seg_bin->binary_length) {
				bin->data = seg_bin->data;
				bin->binary_length = size;

				seg_bin->data = NULL;
				seg_bin->binary_length = 0;
				gda_binary_free (seg_bin);
				break; /* end of for() */
			}
			else
				bin->data = g_new (guchar, size);
		}
		memcpy (bin->data + (size - remaining_to_read), seg_bin->data, 
			MIN (remaining_to_read, seg_bin->binary_length));
		bin->binary_length += MIN (remaining_to_read, seg_bin->binary_length);
		remaining_to_read -= MIN (remaining_to_read, seg_bin->binary_length);
		gda_binary_free (seg_bin);
	}

	blob_op_close (blobop);
	return bin->binary_length;
}
