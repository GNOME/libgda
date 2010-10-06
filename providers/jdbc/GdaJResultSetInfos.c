/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJResultSetInfos.h"
#include "jni-wrapper.h"

JniWrapperField *GdaJResultSetInfos__ncols = NULL;
JniWrapperMethod *GdaJResultSetInfos__describeColumn = NULL;

JNIEXPORT void
JNICALL Java_GdaJResultSetInfos_initIDs (JNIEnv *env, jclass klass)
{
	gsize i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperField **symbol;
	} FieldSignature;

	FieldSignature fields[] = {
		{"ncols", "I", FALSE, &GdaJResultSetInfos__ncols}
	};

	/* fields */
	for (i = 0; i < sizeof (fields) / sizeof (FieldSignature); i++) {
		FieldSignature *m = & (fields [i]);
		*(m->symbol) = jni_wrapper_field_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find field: %s.%s", "GdaJResultSetInfos", m->name);
	}

	/* methods */
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"describeColumn", "(I)LGdaJColumnInfos;", FALSE, &GdaJResultSetInfos__describeColumn},
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJResultSetInfos", m->name);
	}
}
