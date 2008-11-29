/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJPStmt.h"
#include "jni-wrapper.h"

JniWrapperMethod *GdaJPStmt__clearParameters = NULL;
JniWrapperMethod *GdaJPStmt__execute = NULL;
JniWrapperMethod *GdaJPStmt__getResultSet = NULL;
JniWrapperMethod *GdaJPStmt__getImpactedRows = NULL;
JniWrapperMethod *GdaJPStmt__declareParamTypes = NULL;
JniWrapperMethod *GdaJPStmt__setParameterValue = NULL;

JNIEXPORT void
JNICALL Java_GdaJPStmt_initIDs (JNIEnv *env, jclass klass)
{
	gint i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"clearParameters", "()V", FALSE, &GdaJPStmt__clearParameters},
		{"execute", "()Z", FALSE, &GdaJPStmt__execute},
		{"getResultSet", "()LGdaJResultSet;", FALSE, &GdaJPStmt__getResultSet},
		{"getImpactedRows", "()I", FALSE, &GdaJPStmt__getImpactedRows},
		{"declareParamTypes", "(J[B)V", FALSE, &GdaJPStmt__declareParamTypes},
		{"setParameterValue", "(IJ)V", FALSE, &GdaJPStmt__setParameterValue}
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJPStmt", m->name);
	}
}
