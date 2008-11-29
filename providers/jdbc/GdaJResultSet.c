/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJResultSet.h"
#include "jni-wrapper.h"

JniWrapperMethod *GdaJResultSet__getInfos = NULL;
JniWrapperMethod *GdaJResultSet__declareColumnTypes = NULL;
JniWrapperMethod *GdaJResultSet__fillNextRow = NULL;

JNIEXPORT void
JNICALL Java_GdaJResultSet_initIDs (JNIEnv *env, jclass klass)
{
	gint i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"getInfos", "()LGdaJResultSetInfos;", FALSE, &GdaJResultSet__getInfos},
		{"declareColumnTypes" , "(J[B)V", FALSE, &GdaJResultSet__declareColumnTypes},
		{"fillNextRow", "(J)Z", FALSE, &GdaJResultSet__fillNextRow},
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJResultSet", m->name);
	}
}
