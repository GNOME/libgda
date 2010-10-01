/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJConnection.h"
#include "jni-wrapper.h"

JniWrapperMethod *GdaJConnection__close = NULL;
JniWrapperMethod *GdaJConnection__getServerVersion = NULL;
JniWrapperMethod *GdaJConnection__prepareStatement = NULL;
JniWrapperMethod *GdaJConnection__executeDirectSQL = NULL;

JniWrapperMethod *GdaJConnection__begin = NULL;
JniWrapperMethod *GdaJConnection__rollback = NULL;
JniWrapperMethod *GdaJConnection__commit = NULL;
JniWrapperMethod *GdaJConnection__addSavepoint = NULL;
JniWrapperMethod *GdaJConnection__rollbackSavepoint = NULL;
JniWrapperMethod *GdaJConnection__releaseSavepoint = NULL;

JniWrapperMethod *GdaJConnection__getJMeta = NULL;

JNIEXPORT void
JNICALL Java_GdaJConnection_initIDs (JNIEnv *env, jclass klass)
{
	gsize i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"close", "()V", FALSE, &GdaJConnection__close},
		{"getServerVersion", "()Ljava/lang/String;", FALSE, &GdaJConnection__getServerVersion},
		{"prepareStatement", "(Ljava/lang/String;)LGdaJPStmt;", FALSE, &GdaJConnection__prepareStatement},
		{"executeDirectSQL", "(Ljava/lang/String;)LGdaJResultSet;", FALSE, &GdaJConnection__executeDirectSQL},
		{"begin", "()V", FALSE, &GdaJConnection__begin},
		{"rollback", "()V", FALSE, &GdaJConnection__rollback},
		{"commit", "()V", FALSE, &GdaJConnection__commit},
		{"addSavepoint", "(Ljava/lang/String;)V", FALSE, &GdaJConnection__addSavepoint},
		{"rollbackSavepoint", "(Ljava/lang/String;)V", FALSE, &GdaJConnection__rollbackSavepoint},
		{"releaseSavepoint", "(Ljava/lang/String;)V", FALSE, &GdaJConnection__releaseSavepoint},

		{"getJMeta", "()LGdaJMeta;", FALSE, &GdaJConnection__getJMeta}
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJConnection", m->name);
	}
}
