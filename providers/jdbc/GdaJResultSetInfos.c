/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
