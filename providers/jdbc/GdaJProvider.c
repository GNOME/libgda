/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJProvider.h"
#include "jni-wrapper.h"

jclass GdaJProvider_class = NULL;
JniWrapperMethod *GdaJProvider__getDrivers = NULL;
JniWrapperMethod *GdaJProvider__openConnection = NULL;

JNIEXPORT void
JNICALL Java_GdaJProvider_initIDs (JNIEnv *env, jclass klass)
{
	gsize i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"getDrivers", "()Ljava/lang/String;", TRUE, &GdaJProvider__getDrivers},
		{"openConnection", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)LGdaJConnection;", FALSE, &GdaJProvider__openConnection}
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJProvider", m->name);
	}

	GdaJProvider_class = (*env)->NewGlobalRef (env, klass);
	g_assert (GdaJProvider_class);
}
