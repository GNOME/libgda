/* GDA Firebird Blob
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Vivien Malerba <malerba@gnome-db.org>
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

/* FirebirdBlob private structure */
struct _GdaFirebirdBlobPrivate {
	isc_blob_handle blob_handle;
	isc_tr_handle ftr;
	GdaBlobMode mode;
	ISC_QUAD blob_id;
	GdaConnection *cnc;
};

static void gda_firebird_blob_class_init (GdaFirebirdBlobClass *klass);
static void gda_firebird_blob_init       (GdaFirebirdBlob *blob,
					  GdaFirebirdBlobClass *klass);
static void gda_firebird_blob_finalize   (GObject *object);

static gint	gda_firebird_blob_open (GdaBlob *blob, GdaBlobMode mode);
static gint	gda_firebird_blob_read (GdaBlob *blob, gpointer buf, gint size, gint *bytes_read);
static gint	gda_firebird_blob_close (GdaBlob *blob);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_firebird_blob_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaFirebirdBlobClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_blob_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdBlob),
			0,
			(GInstanceInitFunc) gda_firebird_blob_init
		};
		type = g_type_register_static (GDA_TYPE_BLOB, "GdaFirebirdBlob", &info, 0);
	}
	return type;
}

static void
gda_firebird_blob_init (GdaFirebirdBlob *blob,
			GdaFirebirdBlobClass *klass)
{
	g_return_if_fail (GDA_IS_FIREBIRD_BLOB (blob));

	blob->priv = g_new0 (GdaFirebirdBlobPrivate, 1);
	blob->priv->mode = -1;
	blob->priv->blob_handle = NULL;
	blob->priv->ftr = NULL;
}

static void
gda_firebird_blob_class_init (GdaFirebirdBlobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobClass *blob_class = GDA_BLOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_firebird_blob_finalize;
	blob_class->open = gda_firebird_blob_open;
	blob_class->read = gda_firebird_blob_read;
	blob_class->write = NULL;
	blob_class->lseek = NULL;
	blob_class->close = gda_firebird_blob_close;
	blob_class->remove = NULL;
	blob_class->stringify = NULL;
}

static void
gda_firebird_blob_finalize (GObject * object)
{
	GdaFirebirdBlob *blob = (GdaFirebirdBlob *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_BLOB (blob));
	g_return_if_fail (blob->priv);
	ISC_STATUS status[20];

	/* Rollback transaction if active */
	if (blob->priv->ftr)
		isc_rollback_transaction (status, blob->priv->ftr);
	g_free (blob->priv);
	blob->priv = NULL;

	parent_class->finalize (object);
}

GdaBlob *
gda_firebird_blob_new (GdaConnection *cnc)
{
	GdaFirebirdBlob *blob;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	blob = g_object_new (GDA_TYPE_FIREBIRD_BLOB, NULL);
	blob->priv->cnc = cnc;
		
	return GDA_BLOB (blob);
}

void
gda_firebird_blob_set_id (GdaFirebirdBlob *blob,
			  const ISC_QUAD *blob_id)
{
	g_return_if_fail (blob && GDA_IS_FIREBIRD_BLOB (blob));
	g_return_if_fail (blob->priv);
	
	blob->priv->blob_id.gds_quad_high = blob_id->gds_quad_high;
	blob->priv->blob_id.gds_quad_low = blob_id->gds_quad_low;
}


static gint
gda_firebird_blob_open (GdaBlob *blob, 
			GdaBlobMode mode)
{
	GdaFirebirdBlob *fblob;
	GdaFirebirdConnection *fcnc;
	
	g_return_val_if_fail (blob && GDA_IS_FIREBIRD_BLOB (blob), -1);
	fblob = GDA_FIREBIRD_BLOB (blob);
	g_return_val_if_fail (fblob->priv, -1);
	
	/* Process Blob only if is not oppened */
	if (!fblob->priv->blob_handle) {
		
		fcnc = g_object_get_data (G_OBJECT (fblob->priv->cnc), CONNECTION_DATA);
		if (!fcnc) {
			gda_connection_add_event_string (fblob->priv->cnc, _("Invalid Firebird handle"));
			return -1;
		}
		
		if (isc_start_transaction (fcnc->status, &(fblob->priv->ftr), 1, &(fcnc->handle), 0, NULL)) {
			gda_connection_add_event_string (fblob->priv->cnc, _("Unable to start transaction"));
			return -1;
		}
		
		fblob->priv->mode = mode;
		if (!isc_open_blob2 (fcnc->status, &(fcnc->handle), &(fblob->priv->ftr),
				     &(fblob->priv->blob_handle), &(fblob->priv->blob_id), 0, NULL)) {
			return 0;
		}
	}
	
	gda_connection_add_event_string (fblob->priv->cnc, _("Blob already open"));
	
	return -1;
}

static gint 
gda_firebird_blob_read (GdaBlob *blob,
			gpointer buf,
			gint size,
			gint *bytes_read)
{
	GdaFirebirdBlob *fblob;
	GdaFirebirdConnection *fcnc;
	gushort actual_segment_length;
	gint fetch_stat;
	
	g_return_val_if_fail (blob && GDA_IS_FIREBIRD_BLOB (blob), -1);
	fblob = GDA_FIREBIRD_BLOB (blob);
	g_return_val_if_fail (fblob->priv, -1);
	g_return_val_if_fail (bytes_read != NULL, -1);

	/* Process Blob only if is oppened */
	if (fblob->priv->blob_handle) {
	
		/* I don't know if buffer comes initialized, so I initialize */
		if (!buf)
			buf = g_malloc0 (size);
	
		fcnc = g_object_get_data (G_OBJECT (fblob->priv->cnc), CONNECTION_DATA);
		if (!fcnc) {
			gda_connection_add_event_string (fblob->priv->cnc, _("Invalid Firebird handle"));
			return -1;
		}
	
		fetch_stat = isc_get_segment (
					fcnc->status,
					&(fblob->priv->blob_handle),
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
			gda_connection_add_event_string (fblob->priv->cnc, _("Error getting blob segment."));

	}
		
	gda_connection_add_event_string (fblob->priv->cnc, _("Can't read a closed Blob"));
	
	return -1;
}

static gint 
gda_firebird_blob_close (GdaBlob *blob)
{
	GdaFirebirdBlob *fblob;
	GdaFirebirdConnection *fcnc;
	
	g_return_val_if_fail (blob && GDA_IS_FIREBIRD_BLOB (blob), -1);
	fblob = GDA_FIREBIRD_BLOB (blob);
	g_return_val_if_fail (fblob->priv, -1);

	/* Close Blob if is opened */
	if (fblob->priv->blob_handle) {
	
		fcnc = g_object_get_data (G_OBJECT (fblob->priv->cnc), CONNECTION_DATA);
		if (!fcnc) {
			gda_connection_add_event_string (fblob->priv->cnc, _("Invalid Firebird handle"));
			return -1;
		}
		
		if (!isc_close_blob (fcnc->status, &(fblob->priv->blob_handle))) {
			
			/* Rollback transaction if active */
			if (fblob->priv->ftr)
				isc_rollback_transaction (fcnc->status, &(fblob->priv->ftr));
			
			fblob->priv->blob_handle = NULL;
			fblob->priv->ftr = NULL;

			return 0;
		}
		else
			gda_connection_add_event_string (fblob->priv->cnc, _("Unable to close blob."));
	}

	gda_connection_add_event_string (fblob->priv->cnc, _("Blob is closed"));

	return -1;
}



