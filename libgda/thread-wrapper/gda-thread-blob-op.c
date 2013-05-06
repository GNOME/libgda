/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-thread-blob-op.h"

struct _GdaThreadBlobOpPrivate {
	GdaThreadWrapper *wrapper;
	GdaBlobOp        *wrapped_op;
};

static void gda_thread_blob_op_class_init (GdaThreadBlobOpClass *klass);
static void gda_thread_blob_op_init       (GdaThreadBlobOp *blob,
					   GdaThreadBlobOpClass *klass);
static void gda_thread_blob_op_dispose   (GObject *object);

static glong gda_thread_blob_op_get_length (GdaBlobOp *op);
static glong gda_thread_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_thread_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and dispose
 */
GType
_gda_thread_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaThreadBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_thread_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaThreadBlobOp),
			0,
			(GInstanceInitFunc) gda_thread_blob_op_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaThreadBlobOp", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_thread_blob_op_init (GdaThreadBlobOp *op,
			   G_GNUC_UNUSED GdaThreadBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_THREAD_BLOB_OP (op));

	op->priv = g_new0 (GdaThreadBlobOpPrivate, 1);
	op->priv->wrapper = NULL;
	op->priv->wrapped_op = NULL;
}

static void
gda_thread_blob_op_class_init (GdaThreadBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_thread_blob_op_dispose;
	blob_class->get_length = gda_thread_blob_op_get_length;
	blob_class->read = gda_thread_blob_op_read;
	blob_class->write = gda_thread_blob_op_write;
}

static void
gda_thread_blob_op_dispose (GObject * object)
{
	GdaThreadBlobOp *thop = (GdaThreadBlobOp *) object;

	g_return_if_fail (GDA_IS_THREAD_BLOB_OP (thop));

	if (thop->priv) {
		g_object_unref (thop->priv->wrapped_op);
		g_object_unref (thop->priv->wrapper);
		g_free (thop->priv);
		thop->priv = NULL;
	}

	parent_class->dispose (object);
}

GdaBlobOp *
_gda_thread_blob_op_new (GdaThreadWrapper *wrapper, GdaBlobOp *op)
{
	GdaThreadBlobOp *thop;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), NULL);
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), NULL);

	thop = g_object_new (GDA_TYPE_THREAD_BLOB_OP, NULL);
	thop->priv->wrapper = g_object_ref (wrapper);
	thop->priv->wrapped_op = g_object_ref (op);
	
	return GDA_BLOB_OP (thop);
}


/*
 * Virtual functions
 */
static glong *
sub_thread_blob_op_get_length (GdaBlobOp *wrapped_op, G_GNUC_UNUSED GError **error)
{
	/* WARNING: function executed in sub thread! */
	glong *retptr;

	retptr = g_new (glong, 1);
	*retptr = gda_blob_op_get_length (wrapped_op);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %ld\n", __FUNCTION__, *retptr);
#endif

	return retptr;
}

static glong
gda_thread_blob_op_get_length (GdaBlobOp *op)
{
	GdaThreadBlobOp *thop;
	glong *ptr, retval;
	guint jid;
	
	thop = (GdaThreadBlobOp *) op;
	jid = gda_thread_wrapper_execute (thop->priv->wrapper,
					  (GdaThreadWrapperFunc) sub_thread_blob_op_get_length,
					  thop->priv->wrapped_op, NULL, NULL);
	ptr = (glong*) gda_thread_wrapper_fetch_result (thop->priv->wrapper, TRUE, jid, NULL);
	retval = *ptr;
	g_free (ptr);
	return retval;
}

typedef struct {
	GdaBlobOp *op;
	GdaBlob *blob;
	glong offset;
	glong size;
} ThreadData;

static glong *
sub_thread_blob_op_read (ThreadData *td, G_GNUC_UNUSED GError **error)
{
	/* WARNING: function executed in sub thread! */
	glong *retptr;

	retptr = g_new (glong, 1);
	*retptr = gda_blob_op_read (td->op, td->blob, td->offset, td->size);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %ld\n", __FUNCTION__, *retptr);
#endif

	return retptr;
}

static glong
gda_thread_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaThreadBlobOp *thop;
	ThreadData td;
	glong *ptr, retval;
	guint jid;

	thop = (GdaThreadBlobOp *) op;
	td.op = thop->priv->wrapped_op;
	td.blob = blob;
	td.offset = offset;
	td.size = size;

	jid = gda_thread_wrapper_execute (thop->priv->wrapper,
					  (GdaThreadWrapperFunc) sub_thread_blob_op_read,
					  &td, NULL, NULL);
	ptr = (glong*) gda_thread_wrapper_fetch_result (thop->priv->wrapper, TRUE, jid, NULL);
	retval = *ptr;
	g_free (ptr);
	return retval;
}

static glong *
sub_thread_blob_op_write (ThreadData *td, G_GNUC_UNUSED GError **error)
{
	/* WARNING: function executed in sub thread! */
	glong *retptr;

	retptr = g_new (glong, 1);
	*retptr = gda_blob_op_write (td->op, td->blob, td->offset);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %ld\n", __FUNCTION__, *retptr);
#endif

	return retptr;
}

static glong
gda_thread_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaThreadBlobOp *thop;
	ThreadData td;
	glong *ptr, retval;
	guint jid;

	thop = (GdaThreadBlobOp *) op;
	td.op = thop->priv->wrapped_op;
	td.blob = blob;
	td.offset = offset;

	jid = gda_thread_wrapper_execute (thop->priv->wrapper,
					  (GdaThreadWrapperFunc) sub_thread_blob_op_write,
					  &td, NULL, NULL);
	ptr = (glong*) gda_thread_wrapper_fetch_result (thop->priv->wrapper, TRUE, jid, NULL);
	retval = *ptr;
	g_free (ptr);
	return retval;
}
