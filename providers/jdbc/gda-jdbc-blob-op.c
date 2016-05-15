/*
 * Copyright (C) 2008 - 2016 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/libgda.h>
#include "gda-jdbc.h"
#include "gda-jdbc-util.h"
#include "gda-jdbc-blob-op.h"
#include <libgda/gda-blob-op-impl.h>
#include "jni-globals.h"

struct _GdaJdbcBlobOpPrivate {
	GdaConnection *cnc;
	GValue        *blob_obj; /* JAVA GdaJBlobOp object */
};

static void gda_jdbc_blob_op_class_init (GdaJdbcBlobOpClass *klass);
static void gda_jdbc_blob_op_init       (GdaJdbcBlobOp *blob,
					 GdaJdbcBlobOpClass *klass);
static void gda_jdbc_blob_op_finalize   (GObject *object);

static glong gda_jdbc_blob_op_get_length (GdaBlobOp *op);
static glong gda_jdbc_blob_op_read       (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size);
static glong gda_jdbc_blob_op_write      (GdaBlobOp *op, GdaBlob *blob, glong offset);

static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
GType
gda_jdbc_blob_op_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaJdbcBlobOpClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_jdbc_blob_op_class_init,
			NULL,
			NULL,
			sizeof (GdaJdbcBlobOp),
			0,
			(GInstanceInitFunc) gda_jdbc_blob_op_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_BLOB_OP, "GdaJdbcBlobOp", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_jdbc_blob_op_init (GdaJdbcBlobOp *op,
			   G_GNUC_UNUSED GdaJdbcBlobOpClass *klass)
{
	g_return_if_fail (GDA_IS_JDBC_BLOB_OP (op));

	op->priv = g_new0 (GdaJdbcBlobOpPrivate, 1);
	op->priv->blob_obj = NULL;
}

static void
gda_jdbc_blob_op_class_init (GdaJdbcBlobOpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaBlobOpClass *blob_class = GDA_BLOB_OP_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_jdbc_blob_op_finalize;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->get_length = gda_jdbc_blob_op_get_length;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->read = gda_jdbc_blob_op_read;
	GDA_BLOB_OP_FUNCTIONS (blob_class->functions)->write = gda_jdbc_blob_op_write;
}

static void
gda_jdbc_blob_op_finalize (GObject * object)
{
	GdaJdbcBlobOp *bop = (GdaJdbcBlobOp *) object;

	g_return_if_fail (GDA_IS_JDBC_BLOB_OP (bop));

	/* free specific information */
	if (bop->priv->blob_obj)
		gda_value_free (bop->priv->blob_obj);
	g_free (bop->priv);
	bop->priv = NULL;

	parent_class->finalize (object);
}

/**
 * gda_jdbc_blob_op_new_with_jblob
 *
 * Steals @blob_op.
 */
GdaBlobOp *
gda_jdbc_blob_op_new_with_jblob (GdaConnection *cnc, JNIEnv *jenv, jobject blob_obj)
{
	GdaJdbcBlobOp *bop;
	JavaVM *jvm;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (blob_obj, NULL);

	if ((*jenv)->GetJavaVM (jenv, &jvm))
		g_error ("Could not attach JAVA virtual machine's current thread");

	bop = g_object_new (GDA_TYPE_JDBC_BLOB_OP, "connection", cnc, NULL);
	bop->priv->cnc = cnc;
	bop->priv->blob_obj = gda_value_new_jni_object (jvm, jenv, blob_obj);
	
	return GDA_BLOB_OP (bop);
}

/*
 * Get length request
 */
static glong
gda_jdbc_blob_op_get_length (GdaBlobOp *op)
{
	GdaJdbcBlobOp *bop;
	GValue *jexec_res;
	gint error_code;
	gchar *sql_state;
	glong retval;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	GError *error = NULL;

	g_return_val_if_fail (GDA_IS_JDBC_BLOB_OP (op), -1);
	bop = GDA_JDBC_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);

	/* fetch JNIEnv */
	jenv = _gda_jdbc_get_jenv (&jni_detach, &error);
	if (!jenv)
		return -1;

	/* get length */
	jexec_res = jni_wrapper_method_call (jenv, GdaJBlobOp__length,
					     bop->priv->blob_obj, &error_code, &sql_state, &error);
	if (!jexec_res) {
		_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
		return -1;
	}

	_gda_jdbc_release_jenv (jni_detach);

	retval = g_value_get_int64 (jexec_res);
	gda_value_free (jexec_res);
	return retval;
}

/*
 * Blob read request
 */
static glong
gda_jdbc_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	GdaJdbcBlobOp *bop;
	GdaBinary *bin;
	gint error_code;
	gchar *sql_state;
	GValue *jexec_res;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	GError *error = NULL;
	jbyteArray bytes;

	g_return_val_if_fail (GDA_IS_JDBC_BLOB_OP (op), -1);
	bop = GDA_JDBC_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);
	if (offset >= G_MAXINT)
		return -1;
	g_return_val_if_fail (blob, -1);
	
	/* fetch JNIEnv */
	jenv = _gda_jdbc_get_jenv (&jni_detach, &error);
	if (!jenv)
		return -1;

	/* get data */
	jexec_res = jni_wrapper_method_call (jenv, GdaJBlobOp__read,
					     bop->priv->blob_obj, &error_code, &sql_state, &error, 
					     (jlong) offset, (jint) size);
	if (!jexec_res) {
		_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
		return -1;
	}

	/* copy data */
	bin = gda_blob_get_binary (blob);
	bytes = (jbyteArray) gda_value_get_jni_object (jexec_res);
	guchar *data;
	glong length;
	length = (*jenv)->GetArrayLength (jenv, bytes);
	data = g_new (guchar, length);
	(*jenv)->GetByteArrayRegion(jenv, bytes, 0, length, (jbyte *) data);
	gda_binary_take_data (bin, data, length);
	_gda_jdbc_release_jenv (jni_detach);
	gda_value_free (jexec_res);

	return length;
}

/*
 * Blob write request
 */
static glong
gda_jdbc_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	GdaJdbcBlobOp *bop;
	GdaBinary *bin;
	gint error_code;
	gchar *sql_state;
	GValue *jexec_res;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;
	GError *error = NULL;
	jbyteArray bytes;
	glong nbwritten;

	g_return_val_if_fail (GDA_IS_JDBC_BLOB_OP (op), -1);
	bop = GDA_JDBC_BLOB_OP (op);
	g_return_val_if_fail (bop->priv, -1);
	g_return_val_if_fail (GDA_IS_CONNECTION (bop->priv->cnc), -1);
	g_return_val_if_fail (blob, -1);

	/* fetch JNIEnv */
	jenv = _gda_jdbc_get_jenv (&jni_detach, &error);
	if (!jenv)
		return -1;

	GdaBlobOp *blob_op;
	blob_op = gda_blob_get_op (blob);
	if (blob_op && (blob_op != op)) {
		/* use data through blob->op */
		#define buf_size 16384
		gint nread = 0;
		GdaBlob *tmpblob;

		nbwritten = 0;
		tmpblob = gda_blob_new ();
		gda_blob_set_op (tmpblob, blob_op);

		for (nread = gda_blob_op_read (blob_op, tmpblob, 0, buf_size);
		     nread > 0;
		     nread = gda_blob_op_read (blob_op, tmpblob, nbwritten, buf_size)) {
			GdaBinary *bin = (GdaBinary *) tmpblob;
			glong tmp_written;

			bytes = (*jenv)->NewByteArray (jenv, nread);
			if (jni_wrapper_handle_exception (jenv, &error_code, &sql_state, &error)) {
				_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
				_gda_jdbc_release_jenv (jni_detach);
				gda_blob_free ((gpointer) tmpblob);
				return -1;
			}

			(*jenv)->SetByteArrayRegion (jenv, bytes, 0, nread, (jbyte*) gda_binary_get_data (bin));
			if (jni_wrapper_handle_exception (jenv, &error_code, &sql_state, &error)) {
				_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
				(*jenv)->DeleteLocalRef (jenv, bytes);
				_gda_jdbc_release_jenv (jni_detach);
				gda_blob_free ((gpointer) tmpblob);
				return -1;
			}
			
			/* write data */
			jexec_res = jni_wrapper_method_call (jenv, GdaJBlobOp__write,
							     bop->priv->blob_obj, &error_code, 
							     &sql_state, &error, (jlong) offset, bytes);
			(*jenv)->DeleteLocalRef (jenv, bytes);
			if (!jexec_res) {
				_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
				_gda_jdbc_release_jenv (jni_detach);
				gda_blob_free ((gpointer) tmpblob);
				return -1;
			}

			tmp_written = g_value_get_int64 (jexec_res);
			gda_value_free (jexec_res);

			g_assert (tmp_written == nread);

			nbwritten += tmp_written;
			if (nread < buf_size)
				/* nothing more to read */
				break;
		}
		gda_blob_free ((gpointer) tmpblob);
	}
	else {
		/* prepare data to be written using bin->data and bin->binary_length */
		bin = (GdaBinary *) blob;
		bytes = (*jenv)->NewByteArray (jenv, gda_binary_get_size (bin));
		if (jni_wrapper_handle_exception (jenv, &error_code, &sql_state, &error)) {
			_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
			_gda_jdbc_release_jenv (jni_detach);
			return -1;
		}
		
		(*jenv)->SetByteArrayRegion (jenv, bytes, 0, gda_binary_get_size (bin), (jbyte*) gda_binary_get_data (bin));
		if (jni_wrapper_handle_exception (jenv, &error_code, &sql_state, &error)) {
			_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
			(*jenv)->DeleteLocalRef (jenv, bytes);
			_gda_jdbc_release_jenv (jni_detach);
			return -1;
		}
		
		/* write data */
		jexec_res = jni_wrapper_method_call (jenv, GdaJBlobOp__write,
						     bop->priv->blob_obj, &error_code, &sql_state, &error, 
						     (jlong) offset, bytes);
		(*jenv)->DeleteLocalRef (jenv, bytes);
		if (!jexec_res) {
			_gda_jdbc_make_error (bop->priv->cnc, error_code, sql_state, error);
			return -1;
		}
		nbwritten = g_value_get_int64 (jexec_res);
		gda_value_free (jexec_res);
	}
	_gda_jdbc_release_jenv (jni_detach);

	return nbwritten;
}
