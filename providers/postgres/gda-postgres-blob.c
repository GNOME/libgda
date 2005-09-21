/* GDA Postgres blob
 * Copyright (C) 2005 The GNOME Foundation
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

#include <libgda/gda-intl.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-blob.h"
#include <libpq/libpq-fs.h>

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_BLOB

struct _GdaPostgresBlobPrivate {
	GdaConnection *cnc;
	gint           blobid;
	GdaBlobMode    mode;
	gint           fd;
};

static void gda_postgres_blob_class_init (GdaPostgresBlobClass *klass);
static void gda_postgres_blob_init       (GdaPostgresBlob *blob,
					  GdaPostgresBlobClass *klass);
static void gda_postgres_blob_finalize   (GObject *object);

static gint   gda_postgres_blob_open       (GdaBlob *blob, GdaBlobMode mode);
static gint   gda_postgres_blob_read       (GdaBlob *blob, gpointer buf, gint size,
					    gint *bytes_read);
static gint   gda_postgres_blob_write      (GdaBlob *blob, gpointer buf, gint size,
					    gint *bytes_written);
static gint   gda_postgres_blob_lseek      (GdaBlob *blob, gint offset, gint whence);
static gint   gda_postgres_blob_close      (GdaBlob *blob);
static gint   gda_postgres_blob_remove     (GdaBlob *blob);
static gchar *gda_postgres_blob_stringify  (GdaBlob *blob);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_postgres_blob_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaPostgresBlobClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_blob_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresBlob),
			0,
			(GInstanceInitFunc) gda_postgres_blob_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaPostgresBlob", &info, 0);
	}
	return type;
}

static void
gda_postgres_blob_init (GdaPostgresBlob *blob,
			GdaPostgresBlobClass *klass)
{
	g_return_if_fail (GDA_IS_POSTGRES_BLOB (blob));

	blob->priv = g_new0 (GdaPostgresBlobPrivate, 1);
	blob->priv->blobid = -1;
	blob->priv->mode = -1;
	blob->priv->fd = -1;
}

static void
gda_postgres_blob_class_init (GdaPostgresBlobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobClass *blob_class = GDA_BLOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_postgres_blob_finalize;
	blob_class->open = gda_postgres_blob_open;
	blob_class->read = gda_postgres_blob_read;
	blob_class->write = gda_postgres_blob_write;
	blob_class->lseek = gda_postgres_blob_lseek;
	blob_class->close = gda_postgres_blob_close;
	blob_class->remove = gda_postgres_blob_remove;
	blob_class->stringify = gda_postgres_blob_stringify;
}

static void
gda_postgres_blob_finalize (GObject * object)
{
	GdaPostgresBlob *blob = (GdaPostgresBlob *) object;

	g_return_if_fail (GDA_IS_POSTGRES_BLOB (blob));

	g_free (blob->priv);
	blob->priv = NULL;

	parent_class->finalize (object);
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

GdaBlob *
gda_postgres_blob_new (GdaConnection *cnc)
{
	GdaPostgresBlob *blob;
	PGconn *pconn;
	gint oid;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	blob = g_object_new (GDA_TYPE_POSTGRES_BLOB, NULL);

	pconn = get_pconn (cnc);
	oid = lo_creat (pconn, INV_READ | INV_WRITE);
	if (oid == 0) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (cnc, error);
		g_object_unref (blob);

		return NULL;
	}

	blob->priv->blobid = oid;
	blob->priv->cnc = cnc;

	return GDA_BLOB (blob);
}

void
gda_postgres_blob_set_id (GdaPostgresBlob *blob, gint value)
{
	GdaPostgresBlob *pblob;

	g_return_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob));
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_if_fail (pblob->priv);

	pblob->priv->blobid = value;
}

static gint
gda_postgres_blob_open (GdaBlob *blob, GdaBlobMode mode)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;
	gint pg_mode;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), -1);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, -1);

	g_return_val_if_fail (GDA_IS_CONNECTION (pblob->priv->cnc), -1);

	pblob->priv->mode = mode;
	pg_mode = 0;
	if ((mode & GDA_BLOB_MODE_READ) == GDA_BLOB_MODE_READ)
		pg_mode |= INV_READ;

	if ((mode & GDA_BLOB_MODE_WRITE) == GDA_BLOB_MODE_WRITE)
		pg_mode |= INV_WRITE;
	
	pconn = get_pconn (pblob->priv->cnc);
	pblob->priv->fd = lo_open (pconn, pblob->priv->blobid, pg_mode);
	if (pblob->priv->fd < 0) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (pblob->priv->cnc, error);
		return -1;
	}

	return 0;
}

static gint
gda_postgres_blob_read (GdaBlob *blob, gpointer buf, gint size, gint *bytes_read)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), -1);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, -1);
	g_return_val_if_fail (bytes_read, -1);

	g_return_val_if_fail (GDA_IS_CONNECTION (pblob->priv->cnc), -1);

	pconn = get_pconn (pblob->priv->cnc);
	*bytes_read = lo_read (pconn, pblob->priv->fd, (gchar *) buf, size);
	if (*bytes_read == -1) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (pblob->priv->cnc, error);
		return -1;
	}

	return 0;
}

static gint
gda_postgres_blob_write (GdaBlob *blob, gpointer buf, gint size, gint *bytes_written)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), -1);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, -1);

	g_return_val_if_fail (GDA_IS_CONNECTION (pblob->priv->cnc), -1);
	pconn = get_pconn (pblob->priv->cnc);
	*bytes_written = lo_write (pconn, pblob->priv->fd, (gchar *) buf, size);
	if (*bytes_written == -1) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (pblob->priv->cnc, error);
		return -1;
	}

	return 0;
}

static gint
gda_postgres_blob_lseek (GdaBlob *blob, gint offset, gint whence)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;
	gint result;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), -1);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, -1);

	g_return_val_if_fail (GDA_IS_CONNECTION (pblob->priv->cnc), -1);
	g_return_val_if_fail (pblob->priv->fd >= 0, -1);

	pconn = get_pconn (pblob->priv->cnc);
	result = lo_lseek (pconn, pblob->priv->fd, offset, whence);
	if (result == -1) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (pblob->priv->cnc, error);
	}

	return result;
}

static gint
gda_postgres_blob_close (GdaBlob *blob)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;
	gint result;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), -1);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, -1);

	g_return_val_if_fail (GDA_IS_CONNECTION (pblob->priv->cnc), -1);
	g_return_val_if_fail (pblob->priv->fd >= 0, -1);

	pconn = get_pconn (pblob->priv->cnc);
	result = lo_close (pconn, pblob->priv->fd);
	if (result < 0) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (pblob->priv->cnc, error);
	}

	return (result >= 0) ? 0 : -1;
}

static gint
gda_postgres_blob_remove (GdaBlob *blob)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;
	gint result;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), -1);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, -1);

	g_return_val_if_fail (GDA_IS_CONNECTION (pblob->priv->cnc), -1);

	pconn = get_pconn (pblob->priv->cnc);
	result = lo_unlink (pconn, pblob->priv->blobid);
	if (result < 0) {
		GdaConnectionEvent *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_event (pblob->priv->cnc, error);
	}

	return (result >= 0) ? 0 : -1;
}

static gchar *
gda_postgres_blob_stringify (GdaBlob *blob)
{
	GdaPostgresBlob *pblob;
	PGconn *pconn;

	g_return_val_if_fail (blob && GDA_IS_POSTGRES_BLOB (blob), NULL);
	pblob = GDA_POSTGRES_BLOB (blob);
	g_return_val_if_fail (pblob->priv, NULL);

	return g_strdup ("Postgres BLOB to string not implemented");
}
