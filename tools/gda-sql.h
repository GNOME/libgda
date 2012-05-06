/*
 * Copyright (C) 2000 - 2001 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2001 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2007 Baris Cicek <bcicek@src.gnome.org>
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_SQL_H_
#define __GDA_SQL_H_

#include <libgda/libgda.h>
#include <tools/gda-threader.h>
#include <sql-parser/gda-sql-parser.h>
#include "tools-favorites.h"

G_BEGIN_DECLS

/*
 * structure representing an opened connection
 */
typedef struct {
	gchar          *name;
	GdaConnection  *cnc;
	GdaSqlParser   *parser;
	GString        *query_buffer;
	ToolsFavorites *favorites;

	GdaThreader    *threader;
	guint           meta_job_id;
} ConnectionSetting;

typedef enum {
	OUTPUT_FORMAT_DEFAULT = 1 << 0,
	OUTPUT_FORMAT_HTML    = 1 << 1,
	OUTPUT_FORMAT_XML     = 1 << 2,
	OUTPUT_FORMAT_CSV     = 1 << 3,

	OUTPUT_FORMAT_COLOR_TERM = 1 << 8
} OutputFormat;

typedef struct {
	gchar *id;
	ConnectionSetting *current;
	OutputFormat output_format;
	GTimeVal  last_time_used;
} SqlConsole;

const GSList            *gda_sql_get_all_connections (void);
const ConnectionSetting *gda_sql_get_connection (const gchar *name);
const ConnectionSetting *gda_sql_get_current_connection (void);

SqlConsole              *gda_sql_console_new (const gchar *id);
void                     gda_sql_console_free (SqlConsole *console);
gchar                   *gda_sql_console_execute (SqlConsole *console, const gchar *command,
						  GError **error, OutputFormat format);

gchar                   *gda_sql_console_compute_prompt (SqlConsole *console, OutputFormat format);

/*
 * color output handling
 */
typedef enum {
	GDA_SQL_COLOR_NORMAL,
	GDA_SQL_COLOR_RESET,
	GDA_SQL_COLOR_BOLD,
	GDA_SQL_COLOR_RED
} GdaSqlColor;

void         color_print (GdaSqlColor color, OutputFormat format, const char *fmt, ...);
gchar       *color_string (GdaSqlColor color, OutputFormat format, const char *fmt, ...);
void         color_append_string (GdaSqlColor color, OutputFormat format, GString *string, const char *fmt, ...);
const gchar *color_s (GdaSqlColor color, OutputFormat format);

G_END_DECLS

#endif
