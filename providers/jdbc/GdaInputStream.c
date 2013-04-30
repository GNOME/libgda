/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include "GdaInputStream.h"
#include "jni-wrapper.h"
#include <libgda/libgda.h>
#include <libgda/gda-blob-op.h>
#include <glib/gi18n-lib.h>

jclass GdaInputStream__class = NULL;

JNIEXPORT void
JNICALL Java_GdaInputStream_initIDs (JNIEnv *env, jclass klass)
{
	GdaInputStream__class = (*env)->NewGlobalRef (env, klass);
}

JNIEXPORT jintArray
JNICALL Java_GdaInputStream_readData (JNIEnv *jenv, G_GNUC_UNUSED jobject obj,
				      jlong gda_blob_pointer, jlong offset, jlong size)
{
	GdaBlob *blob = (GdaBlob*) jni_jlong_to_cpointer (gda_blob_pointer);
	jintArray jdata;
	if (!blob) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	guchar *raw_data;
	jint *data;
	GdaBlob *nblob = NULL;
	gint real_size;
	if (blob->op) {
		nblob = g_new0 (GdaBlob, 1);
		gda_blob_set_op (nblob, blob->op);
		real_size =  gda_blob_op_read (nblob->op, nblob, offset, size);
		if (real_size < 0) {
			/* throw an exception */
			jclass cls;
			cls = (*jenv)->FindClass (jenv, "java/sql/SQLException");
			if (!cls) {
				/* Unable to find the exception class */
				return NULL;
			}
			(*jenv)->ThrowNew (jenv, cls, _("Can't read BLOB"));
			return NULL;
		}
		raw_data = ((GdaBinary*) nblob)->data;
	}
	else {
		GdaBinary *bin = (GdaBinary *) blob;
		if (offset + size > bin->binary_length)
			real_size = bin->binary_length - offset;
		else
			real_size = size;
		raw_data = bin->data + offset;
	}

	/* convert bin->data to a jintArray */
	int i;
	data = g_new (jint, real_size);
	for (i = 0; i < real_size; i++) 
		data[i] = (jint) (raw_data[i]);

	jdata = (*jenv)->NewIntArray (jenv, real_size);
	if ((*jenv)->ExceptionCheck (jenv)) {
		jdata = NULL;
		goto out;
	}
	
	(*jenv)->SetIntArrayRegion (jenv, jdata, 0, real_size, data);
	if ((*jenv)->ExceptionCheck (jenv)) {
		jdata = NULL;
		(*jenv)->DeleteLocalRef (jenv, jdata);
		goto out;
	}

 out:
	g_free (data);
	if (nblob)
		gda_blob_free ((gpointer) nblob);

	return jdata;
}

JNIEXPORT jbyteArray JNICALL
Java_GdaInputStream_readByteData (JNIEnv *jenv, G_GNUC_UNUSED jobject obj,
				      jlong gda_blob_pointer, jlong offset, jlong size)
{
	GdaBlob *blob = (GdaBlob*) jni_jlong_to_cpointer (gda_blob_pointer);
	jbyteArray jdata;
	if (!blob) {
		jclass cls;
		cls = (*jenv)->FindClass (jenv, "java/lang/IllegalArgumentException");
		if (!cls) {
			/* Unable to find the exception class */
			return NULL;
		}
		(*jenv)->ThrowNew (jenv, cls, _("Invalid argument: NULL"));
		return NULL;
	}

	guchar *raw_data;
	GdaBlob *nblob = NULL;
	gint real_size;
	if (blob->op) {
		nblob = g_new0 (GdaBlob, 1);
		gda_blob_set_op (nblob, blob->op);
		real_size =  gda_blob_op_read (nblob->op, nblob, offset, size);
		if (real_size < 0) {
			/* throw an exception */
			jclass cls;
			cls = (*jenv)->FindClass (jenv, "java/sql/SQLException");
			if (!cls) {
				/* Unable to find the exception class */
				return NULL;
			}
			(*jenv)->ThrowNew (jenv, cls, _("Can't read BLOB"));
			return NULL;
		}
		raw_data = ((GdaBinary*) nblob)->data;
	}
	else {
		GdaBinary *bin = (GdaBinary *) blob;
		if (offset + size > bin->binary_length)
			real_size = bin->binary_length - offset;
		else
			real_size = size;
		raw_data = bin->data + offset;
	}

	/* convert bin->data to a jintArray */
	jdata = (*jenv)->NewByteArray (jenv, real_size);
	if ((*jenv)->ExceptionCheck (jenv)) {
		jdata = NULL;
		goto out;
	}
	
	(*jenv)->SetByteArrayRegion (jenv, jdata, 0, real_size, (const jbyte *) raw_data);
	if ((*jenv)->ExceptionCheck (jenv)) {
		jdata = NULL;
		(*jenv)->DeleteLocalRef (jenv, jdata);
		goto out;
	}

 out:
	if (nblob)
		gda_blob_free ((gpointer) nblob);

	return jdata;
}
