/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 Daniel Espinosa <despinosa@src.gnome.org>
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

#ifndef __GLOBAL_DECL_H_
#define __GLOBAL_DECL_H_

#include <glib.h>

typedef struct _GdaConfig GdaConfig;
typedef struct _GdaConfigClass GdaConfigClass;

typedef struct _GdaConnection        GdaConnection;
typedef struct _GdaConnectionClass   GdaConnectionClass;

typedef struct _GdaServerProvider        GdaServerProvider;
typedef struct _GdaServerProviderClass   GdaServerProviderClass;

typedef struct _GdaDataModelIface   GdaDataModelIface;
typedef struct _GdaDataModel        GdaDataModel;

typedef struct _GdaColumn        GdaColumn;
typedef struct _GdaColumnClass   GdaColumnClass;

typedef struct _GdaHolder GdaHolder;
typedef struct _GdaHolderClass GdaHolderClass;

typedef struct _GdaBlobOp GdaBlobOp;
typedef struct _GdaBlobOpClass GdaBlobOpClass;

/*
 * Statements & parser
 */

typedef struct _GdaStatement GdaStatement;
typedef struct _GdaStatementClass GdaStatementClass;

typedef struct _GdaRepetitiveStatement GdaRepetitiveStatement;
typedef struct _GdaRepetitiveStatementClass GdaRepetitiveStatementClass;

typedef struct _GdaSqlParser GdaSqlParser;
typedef struct _GdaSqlParserClass GdaSqlParserClass;

/*
 * Meta data
 */
typedef struct _GdaMetaStruct        GdaMetaStruct;
typedef struct _GdaMetaStructClass   GdaMetaStructClass;

/*
 * Determines if @word is a reserved SQL keyword
 */
typedef gboolean (*GdaSqlReservedKeywordsFunc) (const gchar *word);

typedef struct _GdaTreeNode GdaTreeNode;
typedef struct _GdaTreeNodeClass GdaTreeNodeClass;

#endif

