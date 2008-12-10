/* GDA - SQL console
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GDA_SQL_H_
#define __GDA_SQL_H_

#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

/*
 * structure representing an opened connection
 */
typedef struct {
	gchar         *name;
	GdaConnection *cnc;
	GdaSqlParser  *parser;
	GString       *query_buffer;

	GdaThreader   *threader;
	guint          meta_job_id;
} ConnectionSetting;

const GSList            *gda_sql_get_all_connections (void);
const ConnectionSetting *gda_sql_get_connection (const gchar *name);
const ConnectionSetting *gda_sql_get_current_connection (void);

G_END_DECLS

#endif
