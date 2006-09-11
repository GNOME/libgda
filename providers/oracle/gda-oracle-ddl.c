/* GNOME DB Oracle Provider
 * Copyright (C) 2006 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Bas Driessen <bas.driessen@xobas.com>
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

#include "gda-oracle-ddl.h"
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-handler.h>

gchar *
gda_oracle_render_CREATE_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	return NULL;
}

gchar *
gda_oracle_render_DROP_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			   GdaServerOperation *op, GError **error)
{
	return NULL;
}


gchar *
gda_oracle_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	return NULL;
}

gchar *
gda_oracle_render_DROP_TABLE   (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP TABLE ");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_oracle_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	return NULL;
}


gchar *
gda_oracle_render_ADD_COLUMN   (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	return NULL;
}

gchar *
gda_oracle_render_DROP_COLUMN  (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	return NULL;
}


gchar *
gda_oracle_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	return NULL;
}

gchar *
gda_oracle_render_DROP_INDEX   (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

