/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_UNKNOWN_H_
#define _GDA_STATEMENT_STRUCT_UNKNOWN_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>

G_BEGIN_DECLS

/*
 * Structure definition
 */
/**
 * GdaSqlStatementUnknown:
 * @any:
 * @expressions: a list of #GdaSqlExpr pointers
 *
 * Represents any statement which type is not identified (any DDL statement or database specific dialect)
 */
struct _GdaSqlStatementUnknown {
	GdaSqlAnyPart  any;
	GSList        *expressions;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
GdaSqlStatementContentsInfo *_gda_sql_statement_unknown_get_infos (void);

/*
 * Functions used by the parser
 */
void gda_sql_statement_unknown_take_expressions (GdaSqlStatement *stmt, GSList *expressions);

G_END_DECLS

#endif
