/* GDA Firebird Blob
 * Copyright (C) 1998-2004 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-firebird-blob.h"
#include "gda-firebird-provider.h"
#include "gda-firebird-recordset.h"
#include <libgda/gda-intl.h>

typedef struct _FirebirdBlobPrivate FirebirdBlobPrivate;

/* FirebirdBlob private structure */
struct _FirebirdBlobPrivate {
	isc_blob_handle blob_handle;
	isc_tr_handle ftr;
	GdaBlobMode mode;
	ISC_QUAD blob_id;
	GdaConnection *cnc;
};

static void	gda_firebird_blob_free_data (GdaBlob *blob);
static gint	gda_firebird_blob_open (GdaBlob *blob, GdaBlobMode mode);
static gint	gda_firebird_blob_read (GdaBlob *blob, gpointer buf, gint size, gint *bytes_read);
static gint	gda_firebird_blob_close (GdaBlob *blob);


/********************************
 * FirebirdBlob public methods *
 ********************************/

GdaBlob *
gda_firebird_blob_new (GdaConnection *cnc)
{
	FirebirdBlobPrivate *priv;
	GdaBlob *blob;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	blob = g_new0 (GdaBlob, 1);
	blob->priv_data = g_new0 (FirebirdBlobPrivate, 1);
		
	priv = blob->priv_data;
	priv->mode = -1;
	priv->blob_handle = NULL;
	priv->cnc = cnc;
	priv->ftr = NULL;
		
	blob->free_data = gda_firebird_blob_free_data;
	blob->open = gda_firebird_blob_open;
	blob->read = gda_firebird_blob_read;
	blob->close = gda_firebird_blob_close;
	blob->write = NULL;
	blob->lseek = NULL;
	blob->remove = NULL;

	return blob;
}

void
gda_firebird_blob_set_id (GdaBlob *blob,
			  const ISC_QUAD *blob_id)
{
	FirebirdBlobPrivate *priv;

	g_return_if_fail (blob != NULL);

	priv = blob->priv_data;
	
	priv->blob_id.gds_quad_high = blob_id->gds_quad_high;
	priv->blob_id.gds_quad_low = blob_id->gds_quad_low;
}


/********************************
 * FirebirdBlob private methods *
 ********************************/

static void
gda_firebird_blob_free_data (GdaBlob *blob)
{
	FirebirdBlobPrivate *priv;
	ISC_STATUS status[20];

	g_return_if_fail (blob != NULL);
	
	priv = blob->priv_data;

	/* Rollback transaction if active */
	if (priv->ftr)
		isc_rollback_transaction (status, priv->ftr);
	
	g_free (priv);
	
	blob->priv_data = NULL;
}

static gint
gda_firebird_blob_open (GdaBlob *blob, 
			GdaBlobMode mode)
{
	FirebirdBlobPrivate *priv;
	GdaFirebirdConnection *fcnc;
	
	g_return_val_if_fail (blob != NULL, -1);

	priv = blob->priv_data;
	/* Process Blob only if is not oppened */
	if (!priv->blob_handle) {
		
		fcnc = g_object_get_data (G_OBJECT (priv->cnc), CONNECTION_DATA);
		if (!fcnc) {
			gda_connection_add_error_string (priv->cnc, _("Invalid Firebird handle"));
			return -1;
		}
		
		if (isc_start_transaction (fcnc->status, &(priv->ftr), 1, &(fcnc->handle), 0, NULL)) {
			gda_connection_add_error_string (priv->cnc, _("Unable to start transaction"));
			return -1;
		}
		
		priv->mode = mode;
		if (!isc_open_blob2 (fcnc->status, &(fcnc->handle), &(priv->ftr),
				     &(priv->blob_handle), &(priv->blob_id), 0, NULL)) {
			return 0;
		}
	}
	
	gda_connection_add_error_string (priv->cnc, _("Blob already open"));
	
	return -1;
}

static gint 
gda_firebird_blob_read (GdaBlob *blob,
			gpointer buf,
			gint size,
			gint *bytes_read)
{
	GdaFirebirdConnection *fcnc;
	FirebirdBlobPrivate *priv;
	gushort actual_segment_length;
	gint fetch_stat;
	
	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);
	g_return_val_if_fail (bytes_read != NULL, -1);

	priv = blob->priv_data;

	/* Process Blob only if is oppened */
	if (priv->blob_handle) {
	
		/* I don't know if buffer comes initialized, so I initialize */
		if (!buf)
			buf = g_malloc0 (size);
	
		fcnc = g_object_get_data (G_OBJECT (priv->cnc), CONNECTION_DATA);
		if (!fcnc) {
			gda_connection_add_error_string (priv->cnc, _("Invalid Firebird handle"));
			return -1;
		}
	
		fetch_stat = isc_get_segment (
					fcnc->status,
					&(priv->blob_handle),
					&actual_segment_length,
					(gushort) size,
					(gchar *) buf);

		/* If we reach blob EOF */
		if (fetch_stat == isc_segstr_eof) {
			*(bytes_read) = (gint) actual_segment_length;
			return -1;
		}
	
		/* Fetched OK or partially */
		if ((fetch_stat == 0) || (fetch_stat == isc_segment)) {
			*(bytes_read) = (gint) actual_segment_length;
			return 0;
		}
		else
			gda_connection_add_error_string (priv->cnc, _("Error getting blob segment."));

	}
		
	gda_connection_add_error_string (priv->cnc, _("Can't read a closed Blob"));
	
	return -1;
}

static gint 
gda_firebird_blob_close (GdaBlob *blob)
{
	FirebirdBlobPrivate *priv;
	GdaFirebirdConnection *fcnc;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);

	priv = blob->priv_data;

	/* Close Blob if is opened */
	if (priv->blob_handle) {
	
		fcnc = g_object_get_data (G_OBJECT (priv->cnc), CONNECTION_DATA);
		if (!fcnc) {
			gda_connection_add_error_string (priv->cnc, _("Invalid Firebird handle"));
			return -1;
		}
		
		if (!isc_close_blob (fcnc->status, &(priv->blob_handle))) {
			
			/* Rollback transaction if active */
			if (priv->ftr)
				isc_rollback_transaction (fcnc->status, &(priv->ftr));
			
			priv->blob_handle = NULL;
			priv->ftr = NULL;

			return 0;
		}
		else
			gda_connection_add_error_string (priv->cnc, _("Unable to close blob."));
	}

	gda_connection_add_error_string (priv->cnc, _("Blob is closed"));

	return -1;
}



