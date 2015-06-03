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
	gsize i;
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
