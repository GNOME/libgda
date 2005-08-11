/* GDA Common Library
 * Copyright (C) 1998-2003 The GNOME Foundation.
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * Authors:
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
 *  Daniel Espinosa Ortiz <esodan@gmail.com> (GdaBlob based on GObject)
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
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GDA_BLOB_MODE_READ = 1,
	GDA_BLOB_MODE_WRITE = 1 << 1,
	GDA_BLOB_MODE_RDWR = 0x03 
} GdaBlobMode;


/* --- type macros --- */
#define G_VALUE_TYPE_BLOB (gda_blob_get_type())
#define G_TYPE_IS_GDABLOB(type)     (G_TYPE_FUNDAMENTAL (type) == GDA_VALUE_TYPE_BLOB)
#define GDA_VALUE_BLOB(object)            (G_TYPE_CHECK_INSTANCE_CAST ((object), G_VALUE_TYPE_BLOB, GdaBlob))
#define GDA_VALUE_BLOB_CLASS(class)       (G_TYPE_CHECK_CLASS_CAST ((class), G_VALUE_TYPE_BLOB, GdaBlobClass))
#define GDA_VALUE_IS_BLOB(object)         (G_TYPE_CHECK_INSTANCE_TYPE ((object), G_VALUE_TYPE_BLOB))
#define GDA_VALUE_IS_BLOB_CLASS(class)    (G_TYPE_CHECK_CLASS_TYPE ((class), G_VALUE_TYPE_BLOB))
#define GDA_VALUE_BLOB_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), G_VALUE_TYPE_BLOB,GdaTimeClass))
#define GDA_VALUE_BLOB_TYPE(object)       (G_TYPE_FROM_INSTANCE (object))
#define GDA_VALUE_BLOB_TYPE_NAME(object)  (g_type_name (GDA_VALUE_BLOB_TYPE (object)))
#define GDA_VALUE_BLOB_CLASS_TYPE(class)  (G_TYPE_FROM_CLASS (class))
#define GDA_VALUE_BLOB_CLASS_NAME(class)  (g_type_name (GDA_VALUE_BLOB_CLASS_TYPE (class)))
#define G_VALUE_HOLDS_GDABLOB(value) (G_TYPE_CHECK_VALUE_TYPE ((value), G_VALUE_TYPE_BLOB))


typedef struct _GdaBlobClass GdaBlobClass;
struct _GdaBlobClass {
	GObjectClass parent_class;
};

typedef struct _GdaBlob GdaBlob;

struct _GdaBlob {
	GObject object;

	/* Private */
	gint (* open) (GdaBlob *blob, GdaBlobMode mode);

	gint (* read) (GdaBlob *blob, gpointer buf, gint size,
			gint *bytes_read);

	gint (* write) (GdaBlob *blob, gpointer buf, gint size,
			gint *bytes_written);

	gint (* lseek) (GdaBlob *blob, gint offset, gint whence);

	gint (* close) (GdaBlob *blob);

	gint (* remove) (GdaBlob *blob);

	gchar * (* stringify) (GdaBlob *blob);

	void (* free_data) (GdaBlob *blob);

	gpointer priv_data;
	/* */

	/* Public */
	gpointer user_data;
};

gint   gda_blob_open (GdaBlob *blob, GdaBlobMode mode);
gint   gda_blob_read (GdaBlob *blob, gpointer buf, gint size, gint *bytes_read);
gint   gda_blob_write (GdaBlob *blob, gpointer buf, gint size, gint *bytes_written);

gint   gda_blob_lseek (GdaBlob *blob, gint offset, gint whence);
gint   gda_blob_close (GdaBlob *blob);

gint   gda_blob_remove (GdaBlob *blob);
void   gda_blob_free_data (GdaBlob *blob);
gchar *gda_blob_stringify(GdaBlob *blob);

G_END_DECLS

#endif

