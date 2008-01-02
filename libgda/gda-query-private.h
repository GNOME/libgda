/* gda-query-private.h
 *
 * Copyright (C) 2004 Vivien Malerba
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

#ifndef __GDA_QUERY_PRIVATE__
#define __GDA_QUERY_PRIVATE__

#ifndef GDA_DISABLE_DEPRECATED

#include "gda-query.h"
#include "sql-delimiter/gda-sql-delimiter.h"

G_BEGIN_DECLS

/* private query structure */
struct _GdaQueryPrivate
{
	GdaQueryType         query_type;
	gboolean             global_distinct;
	GSList              *targets;
	GSList              *joins_flat;
	GSList              *joins_pack;
	GSList              *fields;
	GSList              *sub_queries;

	GSList              *param_sources; /* list of GdaDataModel objects, owned here */

	GdaQueryCondition   *cond;
	GdaQuery            *parent_query;

	gchar                 *sql;     /* non NULL when the query is defined from SQL */
	GdaDelimiterStatement *sql_exprs; /* non NULL when the query is defined from SQL 
					     parsed with libgnomedb's simple parser */
	
	GSList              *fields_order_by;

	guint                serial_target;
	guint                serial_field;
	guint                serial_cond;

	gint                 internal_transaction; /* > 0 to revent emission of the "changed" signal, when
					       * several changes must occur before the query has a stable 
					       * structure again */

	GSList              *all_conds;
	gboolean             auto_clean;

	gboolean             has_limit; /* LIMIT for SELECT queries */
	guint                limit;
	guint                offset;
};

G_END_DECLS

#endif /* GDA_DISABLE_DEPRECATED */

#endif



