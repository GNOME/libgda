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
#include <libgda/gda-debug-macros.h>


typedef struct _GdaConfig GdaConfig;
typedef struct _GdaConfigClass GdaConfigClass;

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

typedef struct _GdaHolder GdaHolder;
typedef struct _GdaHolderClass GdaHolderClass;
typedef struct _GdaHolderPrivate GdaHolderPrivate;

typedef struct _GdaSet GdaSet;
typedef struct _GdaSetClass GdaSetClass;
typedef struct _GdaSetNode GdaSetNode;
typedef struct _GdaSetGroup GdaSetGroup;
typedef struct _GdaSetSource GdaSetSource;
typedef struct _GdaSetPrivate GdaSetPrivate;

typedef struct _GdaBlobOp GdaBlobOp;
typedef struct _GdaBlobOpClass GdaBlobOpClass;

/*
 * Statements & parser
 */

typedef struct _GdaBatch GdaBatch;
typedef struct _GdaBatchClass GdaBatchClass;
typedef struct _GdaBatchPrivate GdaBatchPrivate;

typedef struct _GdaStatement GdaStatement;
typedef struct _GdaStatementClass GdaStatementClass;
typedef struct _GdaStatementPrivate GdaStatementPrivate;

typedef struct _GdaSqlParser GdaSqlParser;
typedef struct _GdaSqlParserClass GdaSqlParserClass;
typedef struct _GdaSqlParserPrivate GdaSqlParserPrivate;

/*
 * Meta data
 */
typedef struct _GdaMetaStore        GdaMetaStore;
typedef struct _GdaMetaStoreClass   GdaMetaStoreClass;
typedef struct _GdaMetaStorePrivate GdaMetaStorePrivate;
typedef struct _GdaMetaStoreClassPrivate GdaMetaStoreClassPrivate;

typedef struct _GdaMetaStruct        GdaMetaStruct;
typedef struct _GdaMetaStructClass   GdaMetaStructClass;
typedef struct _GdaMetaStructPrivate GdaMetaStructPrivate;


/*
 * Win32 adaptations
 */
#ifdef G_OS_WIN32
#define strtok_r(s,d,p) strtok(s,d)
#endif 

#endif
