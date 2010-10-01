/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJMeta.h"
#include "jni-wrapper.h"

JniWrapperMethod *GdaJMeta__getCatalog = NULL;
JniWrapperMethod *GdaJMeta__getSchemas = NULL;
JniWrapperMethod *GdaJMeta__getTables = NULL;
JniWrapperMethod *GdaJMeta__getViews = NULL;
JniWrapperMethod *GdaJMeta__getColumns = NULL;

JNIEXPORT void
JNICALL Java_GdaJMeta_initIDs (JNIEnv *env, jclass klass)
{
	gsize i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperMethod **symbol;
	} MethodSignature;

	MethodSignature methods[] = {
		{"getCatalog", "()Ljava/lang/String;", FALSE, &GdaJMeta__getCatalog},
		{"getSchemas", "(Ljava/lang/String;Ljava/lang/String;)LGdaJResultSet;", FALSE, &GdaJMeta__getSchemas},
		{"getTables", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)LGdaJResultSet;", FALSE, &GdaJMeta__getTables},
		{"getViews", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)LGdaJResultSet;", FALSE, &GdaJMeta__getViews},
		{"getColumns", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)LGdaJResultSet;", FALSE, &GdaJMeta__getColumns},
	};

	for (i = 0; i < sizeof (methods) / sizeof (MethodSignature); i++) {
		MethodSignature *m = & (methods [i]);
		*(m->symbol) = jni_wrapper_method_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find method: %s.%s", "GdaJMeta", m->name);
	}
}
