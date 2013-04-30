/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <glib/gstdio.h>

#ifndef __JNI_WRAPPER__
#define __JNI_WRAPPER__

/*
 * Conversion method
 */
jlong             jni_cpointer_to_jlong (gconstpointer c_pointer);
gconstpointer     jni_jlong_to_cpointer (jlong value);

/*
 * general functions
 */
typedef jint (*CreateJavaVMFunc) (JavaVM **, void **, void *);

JNIEnv           *jni_wrapper_create_vm (JavaVM **out_jvm, CreateJavaVMFunc create_func,
					 const gchar *lib_path, 
					 const gchar *class_path, GError **error);
void              jni_wrapper_destroy_vm (JavaVM *jvm);
gboolean          jni_wrapper_handle_exception (JNIEnv *jvm, gint *out_error_code, 
						gchar **out_sql_state, GError **error);

jclass            jni_wrapper_class_get (JNIEnv *jvm, const gchar *class_name, GError **error);
GValue           *jni_wrapper_instantiate_object (JNIEnv *jenv, jclass klass,
						  const gchar *signature, GError **error, ...);

/*
 * methods
 */
typedef struct {
	jclass    klass; /* JNI global reference */
	gchar    *ret_type;
	gboolean  is_static;
	jmethodID mid;
} JniWrapperMethod;
JniWrapperMethod *jni_wrapper_method_create (JNIEnv *jenv, jclass jklass, 
					     const gchar *method_name, const gchar *signature,
					     gboolean is_static, GError **error);
GValue           *jni_wrapper_method_call (JNIEnv *jenv, JniWrapperMethod *method, GValue *object, 
					   gint *out_error_code, gchar **out_sql_state, GError **error, ...);
void              jni_wrapper_method_free (JNIEnv *jenv, JniWrapperMethod *method);

/*
 * fields access
 */
typedef struct {
	jclass    klass; /* JNI global reference */
	gchar    *type;
	gboolean  is_static;
	jfieldID  fid;
} JniWrapperField;
JniWrapperField  *jni_wrapper_field_create (JNIEnv *jenv, jclass jklass, 
					     const gchar *field_name, const gchar *signature,
					     gboolean is_static, GError **error);
GValue           *jni_wrapper_field_get (JNIEnv *jenv, JniWrapperField *field, 
					 GValue *object, GError **error);
gboolean          jni_wrapper_field_set (JNIEnv *jenv, JniWrapperField *field, 
					 GValue *object, const GValue *value, GError **error);
void              jni_wrapper_field_free (JNIEnv *jenv, JniWrapperField *field);


/*
 * jobject wrapper in a GValue
 */
#define GDA_TYPE_JNI_OBJECT (gda_jni_object_get_type())
typedef struct {
	JavaVM  *jvm;
	jobject  jobj;
} GdaJniObject;
jobject  gda_value_get_jni_object (const GValue *value);
GValue  *gda_value_new_jni_object (JavaVM *jvm, JNIEnv *env, jobject jni_object);
void     gda_value_set_jni_object (GValue *value, JavaVM *jvm, JNIEnv *env, jobject jni_object);
GType    gda_jni_object_get_type (void) G_GNUC_CONST;
gpointer gda_jni_object_copy (gpointer boxed);
void     gda_jni_object_free (gpointer boxed);

#endif
