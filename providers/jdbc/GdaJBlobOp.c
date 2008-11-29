/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJBlobOp.h"
#include "jni-wrapper.h"

JniWrapperMethod *GdaJBlobOp__read = NULL;
JniWrapperMethod *GdaJBlobOp__write = NULL;
JniWrapperMethod *GdaJBlobOp__length = NULL;

JNIEXPORT void
JNICALL Java_GdaJBlobOp_initIDs (JNIEnv *env, jclass klass)
{
	gint i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"read", "(JI)[B", FALSE, &GdaJBlobOp__read},
		{"write", "(J[B)I", FALSE, &GdaJBlobOp__write},
		{"length", "()J", FALSE, &GdaJBlobOp__length},
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJBlobOp", m->name);
	}
}
