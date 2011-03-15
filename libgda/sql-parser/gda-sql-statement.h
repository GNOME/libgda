/*
 * Copyright (C) 2008 - 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_SQL_STATEMENT_H__
#define __GDA_SQL_STATEMENT_H__

G_BEGIN_DECLS

/**
 * SECTION:gda-sql-statement
 * @short_description: SQL parser
 * @title: GdaSqlParser
 * @stability: Stable
 * @see_also: The #GdaSqlBuilder object which features some easy to use API to build #GdaSqlStatement structures or #GdaStatement objects without having to worry about the details of #GdaSqlStatement's contents.
 *
 * Please note that it is not advised to build #GdaSqlStatement structures directly, but rather
 * it is recommended to use the #GdaSqlBuilder object and its associated API.
 *
 * Every SQL statement can be decomposed in a #GdaSqlStatement structure. This is not a #GObject, but rather just a C structure
 * which can be manipulated directly. The structure is a tree composed of several key structures which are show in the following diagram
 * (even though it does not show, all structures "inherit" the #GdaSqlAnyPart structure which holds some basic information).
 *<mediaobject>
 *  <imageobject role="html">
 *    <imagedata fileref="parts.png" format="PNG"/>
 *  </imageobject>
 *  <caption>
 *    <para>
 *      Main parts of the #GdaSqlStatement structure.
 *    </para>
 *  </caption>
 *</mediaobject>
 *
 * The samples/SqlParserConsole directory of &LIBGDA;'s sources contains a small utility
 * to display statements' structures as a graph (using the GraphViz language). It has been used to
 * provide the examples in this section of the documentation.
 */

#include <sql-parser/gda-statement-struct-select.h>
#include <sql-parser/gda-statement-struct-insert.h>
#include <sql-parser/gda-statement-struct-update.h>
#include <sql-parser/gda-statement-struct-delete.h>
#include <sql-parser/gda-statement-struct-compound.h>
#include <sql-parser/gda-statement-struct-trans.h>
#include <sql-parser/gda-statement-struct-unknown.h>
#include <sql-parser/gda-statement-struct-parts.h>

G_END_DECLS

#endif
