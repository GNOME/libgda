/* GDA Default Provider
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-default.h"
#include <ctype.h>

typedef GdaServerRecordset* (*schema_ops_fn)(GdaServerError *,
                                              GdaServerConnection *,
                                              GDA_Connection_Constraint *,
                                              gint );

static GdaServerRecordset* schema_tables (GdaServerError *error,
                                           GdaServerConnection *cnc,
                                           GDA_Connection_Constraint *constraints,
                                           gint length);
static GdaServerRecordset* schema_columns (GdaServerError *error,
                                            GdaServerConnection *cnc,
                                            GDA_Connection_Constraint *constraints,
                                            gint length);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = { 0, };

/*
 * Public functions
 */
gboolean
gda_default_connection_new (GdaServerConnection *cnc)
{
	static gboolean     initialized = FALSE;
	DEFAULT_Connection* default_cnc;
	
	/* initialize schema functions */
	if (!initialized) {
		schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
		schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_columns;
	}
	
	default_cnc = g_new0(DEFAULT_Connection, 1);
	gda_server_connection_set_user_data(cnc, (gpointer) default_cnc);
	
	return TRUE;
}

static gchar*
get_value (gchar* ptr)
{
	while (*ptr && *ptr != '=') ptr++;
	if (!*ptr)
		return 0;
	ptr++;
	if (!*ptr)
		return 0;

	while (*ptr && isspace(*ptr)) ptr++;

	return (g_strdup(ptr));
}

gint
gda_default_connection_open (GdaServerConnection *cnc,
							 const gchar *dsn,
							 const gchar *user,
							 const gchar *password)
{
}

void
gda_default_connection_close (GdaServerConnection *cnc)
{
}

gint
gda_default_connection_begin_transaction (GdaServerConnection *cnc)
{
}

gint
gda_default_connection_commit_transaction (GdaServerConnection *cnc)
{
}

gint
gda_default_connection_rollback_transaction (GdaServerConnection *cnc)
{
}

GdaServerRecordset *
gda_default_connection_open_schema (GdaServerConnection *cnc,
                                  GdaServerError *error,
                                  GDA_Connection_QType t,
                                  GDA_Connection_Constraint *constraints,
                                  gint length)
{
	schema_ops_fn fn;
	
	g_return_val_if_fail(cnc != NULL, NULL);
	
	fn = schema_ops[(gint) t];
	if (fn)
		return fn(error, cnc, constraints, length);
	else gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint) t);
	return NULL;
}

glong
gda_default_connection_modify_schema (GdaServerConnection *cnc,
                                    GDA_Connection_QType t,
                                    GDA_Connection_Constraint *constraints,
                                    gint length)
{
	return -1;
}

gint
gda_default_connection_start_logging (GdaServerConnection *cnc, const gchar *filename)
{
	return -1;
}

gint
gda_default_connection_stop_logging (GdaServerConnection *cnc)
{
	return -1;
}

gchar *
gda_default_connection_create_table (GdaServerConnection *cnc,
                                   GDA_RowAttributes *columns)
{
  return NULL;
}

gboolean
gda_default_connection_supports (GdaServerConnection *cnc,
                               GDA_Connection_Feature feature)
{
	g_return_val_if_fail(cnc != NULL, FALSE);
	
	switch (feature) {
		case GDA_Connection_FEATURE_SQL :
		case GDA_Connection_FEATURE_XML_QUERIES :
			return TRUE;
		default :
			return FALSE;
	}

	return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_default_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
}

gshort
gda_default_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
	g_return_val_if_fail(cnc != NULL, -1);
	
	switch (type) {
	}
	return -1;
}

gchar *
gda_default_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
	return NULL;
}

gchar *
gda_default_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
	return NULL;
}

void
gda_default_connection_free (GdaServerConnection *cnc)
{
	DEFAULT_Connection* default_cnc;
	
	g_return_if_fail(cnc != NULL);
	
	default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
	if (default_cnc) {
		g_free((gpointer) default_cnc);
		gda_server_connection_set_user_data(cnc, NULL);
	}
}

void
gda_default_error_make (GdaServerError *error,
						GdaServerRecordset *recset,
						GdaServerConnection *cnc,
						gchar *where)
{
}

/*
 * Schema functions
 */
static GdaServerRecordset *
schema_tables (GdaServerError *error,
               GdaServerConnection *cnc,
               GDA_Connection_Constraint *constraints,
               gint length)
{
}

static GdaServerRecordset *
schema_columns (GdaServerError *error,
                GdaServerConnection *cnc,
                GDA_Connection_Constraint *constraints,
                gint length)
{
}
