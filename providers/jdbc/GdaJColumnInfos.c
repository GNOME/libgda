/*
 * This file implements the JNI calls from the JAVA VM to the native
 * C code
 * and also stores Class and Method IDs
 */

#include "GdaJColumnInfos.h"
#include "jni-wrapper.h"

JniWrapperField *GdaJColumnInfos__col_name = NULL;
JniWrapperField *GdaJColumnInfos__col_descr = NULL;
JniWrapperField *GdaJColumnInfos__col_type = NULL;

JNIEXPORT void
JNICALL Java_GdaJColumnInfos_initIDs (JNIEnv *env, jclass klass)
{
	gint i;
	typedef struct {
		const gchar       *name;
		const gchar       *sig;
		gboolean           is_static;
		JniWrapperField **symbol;
	} FieldSignature;

	FieldSignature fields[] = {
		{"col_name", "Ljava/lang/String;", FALSE, &GdaJColumnInfos__col_name},
		{"col_descr", "Ljava/lang/String;", FALSE, &GdaJColumnInfos__col_descr},
		{"col_type", "I", FALSE, &GdaJColumnInfos__col_type},
	};

	for (i = 0; i < sizeof (fields) / sizeof (FieldSignature); i++) {
		FieldSignature *m = & (fields [i]);
		*(m->symbol) = jni_wrapper_field_create (env, klass, m->name, m->sig, m->is_static, NULL);
		if (!*(m->symbol))
			g_error ("Can't find field: %s.%s", "GdaJColumnInfos", m->name);
	}
}
