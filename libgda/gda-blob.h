/* GDA Common Library
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * Authors:
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
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

#if !defined(__gda_blob_h__)
#  define __gda_blob_h__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	GDA_BLOB_MODE_RDONLY,
	GDA_BLOB_MODE_WRONLY,
	GDA_BLOB_MODE_RDWR
} GdaBlobMode;

typedef struct _GdaBlob GdaBlob;

struct _GdaBlob {
	/* FIXME: void* -> GdaConnection* */
	gint (* open) (gpointer connection, GdaBlob *blob,
			GdaBlobMode mode);

	gint (* read) (gpointer connection, GdaBlob *blob,
			gint size, gpointer data, gint *read_length);

	gint (* write) (gpointer connection, GdaBlob *blob,
			gint size, gconstpointer data, gint *written_length);

	gint (* lseek) (gpointer connection, GdaBlob *blob,
			gint offset, gint whence);

	gint (* close) (gpointer connection, GdaBlob *blob);

	gchar * (* stringify) (void);

	gpointer priv_data;
};

gint gda_blob_open (gpointer connection, GdaBlob *blob, 
			GdaBlobMode mode);
gint gda_blob_read (gpointer connection, GdaBlob *blob, gint size, 
			gpointer data, gint *read_length);
gint gda_blob_write (gpointer connection, GdaBlob *blob, gint size, 
			gconstpointer data, gint *written_length);
gint gda_blob_lseek (gpointer connection, GdaBlob *blob, gint offset, 
			gint whence);
gint gda_blob_close (gpointer connection, GdaBlob *blob);

G_END_DECLS

#endif

