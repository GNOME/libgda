/*
 * Copyright (C) 2008 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <jni-wrapper.h>

#ifndef __JNI_GLOBALS__
#define __JNI_GLOBALS__

/* GdaJProvider */
extern jclass GdaJProvider_class;
extern JniWrapperMethod *GdaJProvider__getDrivers;
extern JniWrapperMethod *GdaJProvider__openConnection;
extern JniWrapperMethod *GdaJConnection__prepareStatement;

/* GdaJConnection */
extern JniWrapperMethod *GdaJConnection__close;
extern JniWrapperMethod *GdaJConnection__getServerVersion;
extern JniWrapperMethod *GdaJConnection__executeDirectSQL;

extern JniWrapperMethod *GdaJConnection__begin;
extern JniWrapperMethod *GdaJConnection__rollback;
extern JniWrapperMethod *GdaJConnection__commit;
extern JniWrapperMethod *GdaJConnection__addSavepoint;
extern JniWrapperMethod *GdaJConnection__rollbackSavepoint;
extern JniWrapperMethod *GdaJConnection__releaseSavepoint;
extern JniWrapperMethod *GdaJConnection__getJMeta;

/* GdaJPStmt */
extern JniWrapperMethod *GdaJPStmt__clearParameters;
extern JniWrapperMethod *GdaJPStmt__execute;
extern JniWrapperMethod *GdaJPStmt__getResultSet;
extern JniWrapperMethod *GdaJPStmt__getImpactedRows;
extern JniWrapperMethod *GdaJPStmt__declareParamTypes;
extern JniWrapperMethod *GdaJPStmt__setParameterValue;

/* GdaJResultSet */
extern JniWrapperMethod *GdaJResultSet__getInfos;
extern JniWrapperMethod *GdaJResultSet__declareColumnTypes;
extern JniWrapperMethod *GdaJResultSet__fillNextRow;

/* GdaJResultSetInfos */
extern JniWrapperField *GdaJResultSetInfos__ncols;
extern JniWrapperMethod *GdaJResultSetInfos__describeColumn;

/* GdaJColumnInfos */
extern JniWrapperField *GdaJColumnInfos__col_name;
extern JniWrapperField *GdaJColumnInfos__col_descr;
extern JniWrapperField *GdaJColumnInfos__col_type;

/* GdaJBlobOp */
extern JniWrapperMethod *GdaJBlobOp__read;
extern JniWrapperMethod *GdaJBlobOp__write;
extern JniWrapperMethod *GdaJBlobOp__length;

/* GdaInputStream */
extern jclass GdaInputStream__class;

/* GdaJMeta */
extern JniWrapperMethod *GdaJMeta__getCatalog;
extern JniWrapperMethod *GdaJMeta__getSchemas;
extern JniWrapperMethod *GdaJMeta__getTables;
extern JniWrapperMethod *GdaJMeta__getViews;
extern JniWrapperMethod *GdaJMeta__getColumns;
extern JniWrapperMethod *GdaJMeta__getTableConstraints;


/* debug features */
extern gboolean jvm_describe_exceptions;

#endif
