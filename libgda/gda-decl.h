/* gda-decl.h
 *
 * Copyright (C) 2004 - 2006 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GLOBAL_DECL_H_
#define __GLOBAL_DECL_H_

#include <glib.h>

typedef struct _GdaObject GdaObject;
typedef struct _GdaObjectClass GdaObjectClass;
typedef struct _GdaObjectPrivate GdaObjectPrivate;

typedef struct _GdaConnection        GdaConnection;
typedef struct _GdaConnectionClass   GdaConnectionClass;
typedef struct _GdaConnectionPrivate GdaConnectionPrivate;

typedef struct _GdaConnectionEvent        GdaConnectionEvent;
typedef struct _GdaConnectionEventClass   GdaConnectionEventClass;
typedef struct _GdaConnectionEventPrivate GdaConnectionEventPrivate;

typedef struct _GdaDataHandler      GdaDataHandler;
typedef struct _GdaDataHandlerIface GdaDataHandlerIface;

typedef struct _GdaServerProvider        GdaServerProvider; 
typedef struct _GdaServerProviderClass   GdaServerProviderClass;
typedef struct _GdaServerProviderPrivate GdaServerProviderPrivate;
typedef struct _GdaServerProviderInfo    GdaServerProviderInfo;

typedef struct _GdaServerOperation        GdaServerOperation; 
typedef struct _GdaServerOperationClass   GdaServerOperationClass;
typedef struct _GdaServerOperationPrivate GdaServerOperationPrivate;

typedef struct _GdaClient        GdaClient;
typedef struct _GdaClientClass   GdaClientClass;
typedef struct _GdaClientPrivate GdaClientPrivate;

typedef struct _GdaEntity           GdaEntity;
typedef struct _GdaEntityIface      GdaEntityIface;

typedef struct _GdaEntityField            GdaEntityField;
typedef struct _GdaEntityFieldIface       GdaEntityFieldIface;

typedef struct _GdaDataModelClass   GdaDataModelClass;
typedef struct _GdaDataModel        GdaDataModel;

typedef struct _GdaDataProxy GdaDataProxy;
typedef struct _GdaDataProxyClass GdaDataProxyClass;
typedef struct _GdaDataProxyPrivate GdaDataProxyPrivate;

typedef struct _GdaColumn        GdaColumn;
typedef struct _GdaColumnClass   GdaColumnClass;
typedef struct _GdaColumnPrivate GdaColumnPrivate;

typedef struct _GdaDataModelIter        GdaDataModelIter;
typedef struct _GdaDataModelIterClass   GdaDataModelIterClass;
typedef struct _GdaDataModelIterPrivate GdaDataModelIterPrivate;

typedef struct _GdaDict GdaDict;
typedef struct _GdaDictClass GdaDictClass;
typedef struct _GdaDictPrivate GdaDictPrivate;
extern  GdaDict *default_dict;
#define ASSERT_DICT(x) ((x) ? (x) : default_dict)

typedef struct _GdaDictType GdaDictType;
typedef struct _GdaDictTypeClass GdaDictTypeClass;
typedef struct _GdaDictTypePrivate GdaDictTypePrivate;

#define GDA_FUNC_AGG_TEST_PARAMS_DO_TEST 0
typedef struct _GdaDictFunction GdaDictFunction;
typedef struct _GdaDictFunctionClass GdaDictFunctionClass;
typedef struct _GdaDictFunctionPrivate GdaDictFunctionPrivate;

typedef struct _GdaDictAggregate GdaDictAggregate;
typedef struct _GdaDictAggregateClass GdaDictAggregateClass;
typedef struct _GdaDictAggregatePrivate GdaDictAggregatePrivate;

typedef struct _GdaDictDatabase GdaDictDatabase;
typedef struct _GdaDictDatabaseClass GdaDictDatabaseClass;
typedef struct _GdaDictDatabasePrivate GdaDictDatabasePrivate;

typedef struct _GdaDictTable GdaDictTable;
typedef struct _GdaDictTableClass GdaDictTableClass;
typedef struct _GdaDictTablePrivate GdaDictTablePrivate;

typedef struct _GdaDictField GdaDictField;
typedef struct _GdaDictFieldClass GdaDictFieldClass;
typedef struct _GdaDictFieldPrivate GdaDictFieldPrivate;

typedef struct _GdaDictConstraint GdaDictConstraint;
typedef struct _GdaDictConstraintClass GdaDictConstraintClass;
typedef struct _GdaDictConstraintPrivate GdaDictConstraintPrivate;

typedef struct _GdaParameter GdaParameter;
typedef struct _GdaParameterClass GdaParameterClass;
typedef struct _GdaParameterPrivate GdaParameterPrivate;

typedef struct _GdaParameterList GdaParameterList;
typedef struct _GdaParameterListClass GdaParameterListClass;
typedef struct _GdaParameterListNode GdaParameterListNode;
typedef struct _GdaParameterListGroup GdaParameterListGroup;
typedef struct _GdaParameterListSource GdaParameterListSource;
typedef struct _GdaParameterListPrivate GdaParameterListPrivate;

typedef struct _GdaXmlStorage       GdaXmlStorage;
typedef struct _GdaXmlStorageIface  GdaXmlStorageIface;

typedef struct _GdaRenderer         GdaRenderer;
typedef struct _GdaRendererIface    GdaRendererIface;

typedef struct _GdaReferer          GdaReferer;
typedef struct _GdaRefererIface     GdaRefererIface;

typedef struct _GdaObjectRef GdaObjectRef;
typedef struct _GdaObjectRefClass GdaObjectRefClass;
typedef struct _GdaObjectRefPrivate GdaObjectRefPrivate;

typedef struct _GdaQuery GdaQuery;
typedef struct _GdaQueryClass GdaQueryClass;
typedef struct _GdaQueryPrivate GdaQueryPrivate;

typedef struct _GdaQueryTarget GdaQueryTarget;
typedef struct _GdaQueryTargetClass GdaQueryTargetClass;
typedef struct _GdaQueryTargetPrivate GdaQueryTargetPrivate;

typedef struct _GdaQueryJoin GdaQueryJoin;
typedef struct _GdaQueryJoinClass GdaQueryJoinClass;
typedef struct _GdaQueryJoinPrivate GdaQueryJoinPrivate;

typedef struct _GdaQueryCondition GdaQueryCondition;
typedef struct _GdaQueryConditionClass GdaQueryConditionClass;
typedef struct _GdaQueryConditionPrivate GdaQueryConditionPrivate;

typedef struct _GdaQueryField GdaQueryField;
typedef struct _GdaQueryFieldClass GdaQueryFieldClass;
typedef struct _GdaQueryFieldPrivate GdaQueryFieldPrivate;

typedef struct _GdaQueryFieldAll GdaQueryFieldAll;
typedef struct _GdaQueryFieldAllClass GdaQueryFieldAllClass;
typedef struct _GdaQueryFieldAllPrivate GdaQueryFieldAllPrivate;

typedef struct _GdaQueryFieldField GdaQueryFieldField;
typedef struct _GdaQueryFieldFieldClass GdaQueryFieldFieldClass;
typedef struct _GdaQueryFieldFieldPrivate GdaQueryFieldFieldPrivate;

typedef struct _GdaQueryFieldValue GdaQueryFieldValue;
typedef struct _GdaQueryFieldValueClass GdaQueryFieldValueClass;
typedef struct _GdaQueryFieldValuePrivate GdaQueryFieldValuePrivate;

typedef struct _GdaQueryFieldFunc GdaQueryFieldFunc;
typedef struct _GdaQueryFieldFuncClass GdaQueryFieldFuncClass;
typedef struct _GdaQueryFieldFuncPrivate GdaQueryFieldFuncPrivate;

typedef struct _GdaQueryFieldAgg GdaQueryFieldAgg;
typedef struct _GdaQueryFieldAggClass GdaQueryFieldAggClass;
typedef struct _GdaQueryFieldAggPrivate GdaQueryFieldAggPrivate;

typedef struct _GdaBlobOp GdaBlobOp;
typedef struct _GdaBlobOpClass GdaBlobOpClass;

/*
 * Graphing part
 */
typedef struct _GdaGraph GdaGraph;
typedef struct _GdaGraphClass GdaGraphClass;
typedef struct _GdaGraphPrivate GdaGraphPrivate;

typedef struct _GdaGraphQuery GdaGraphQuery;
typedef struct _GdaGraphQueryClass GdaGraphQueryClass;
typedef struct _GdaGraphQueryPrivate GdaGraphQueryPrivate;

typedef struct _GdaGraphItem GdaGraphItem;
typedef struct _GdaGraphItemClass GdaGraphItemClass;
typedef struct _GdaGraphItemPrivate GdaGraphItemPrivate;

/*
 * Win32 adaptations
 */
#ifdef G_OS_WIN32
#define strtok_r(s,d,p) strtok(s,d)
#endif 

/*
 * Various macros
 */
#ifdef GDA_DEBUG
#define D_COL_NOR "\033[0m"
#define D_COL_H0 "\033[;34;7m"
#define D_COL_H1 "\033[;36;7m"
#define D_COL_H2 "\033[;36;4m"
#define D_COL_OK "\033[;32m"
#define D_COL_ERR "\033[;31;1m"
#define AAA(X) (g_print (D_COL_H1 "DEBUG MARK %d\n" D_COL_NOR, X))
#define DEBUG_HEADER (g_print (D_COL_H0 "====================== %s %s():%d ======================\n" D_COL_NOR, __FILE__, __FUNCTION__, __LINE__))
#define DEBUG_HEADER_STR(str) (g_print (D_COL_H0 "====================== %s %s %s():%d ======================\n" D_COL_NOR, (str), __FILE__, __FUNCTION__, __LINE__))
#endif

#ifndef TO_IMPLEMENT
  #ifdef GDA_DEBUG
    #define TO_IMPLEMENT g_print (D_COL_ERR "Implementation missing:" D_COL_NOR " %s() in %s line %d\n", __FUNCTION__, __FILE__,__LINE__)
  #else
    #define TO_IMPLEMENT g_print ("Implementation missing: %s() in %s line %d\n", __FUNCTION__, __FILE__,__LINE__)
  #endif
#endif

#endif
