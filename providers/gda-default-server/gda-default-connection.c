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

#include "gda-server-private.h"
#include "gda-default.h"
#include <ctype.h>

typedef GdaServerRecordset* (*schema_ops_fn)(GdaError *,
                                              GdaServerConnection *,
                                              GDA_Connection_Constraint *,
                                              gint );

static GdaServerRecordset* schema_tables (GdaError *error,
					  GdaServerConnection *cnc,
					  GDA_Connection_Constraint *constraints,
					  gint length);
static GdaServerRecordset* schema_columns (GdaError *error,
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
	DEFAULT_Connection *default_cnc;
	
	/* initialize schema functions */
	if (!initialized) {
		schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
		schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_columns;
		initialized = TRUE;
	}
	
	default_cnc = g_new0(DEFAULT_Connection, 1);
	gda_server_connection_set_user_data(cnc, (gpointer) default_cnc);
	
	return TRUE;
}

static gchar*
get_value (gchar *ptr)
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
	DEFAULT_Connection *default_cnc;
	gint retval = -1;

	g_return_val_if_fail(cnc != NULL, -1);
	g_return_val_if_fail(dsn != NULL, -1);

	default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
	if (default_cnc) {
		gchar **arr;
		gchar *t_dir = NULL;

		/* parse connection string */
		arr = g_strsplit(dsn, ";", 0);
		if (arr) {
			gint n = 0;

			while (arr[n]) {
				if (!strncasecmp(arr[n], "DIRECTORY", strlen("DIRECTORY")))
					t_dir = get_value(arr[n]);
				n++;
			}
			g_strfreev(arr);
		}

		if (t_dir) {
			gchar *errmsg = NULL;

			/* open the database */
			default_cnc->sqlite = sqlite_open(t_dir, 0666, &errmsg);
			if (default_cnc->sqlite) {
				retval = 0;
			}
			else
				gda_server_connection_add_error_string(cnc, errmsg);

			/* free memory */
			g_free((gpointer) t_dir);
			if (errmsg)
				free(errmsg); /* must use free() for this pointer */
		}
		else
			gda_server_connection_add_error_string(cnc, _("You must specify a directory for the database to be stored"));
	}

	return retval;
}

void
gda_default_connection_close (GdaServerConnection *cnc)
{
	DEFAULT_Connection *default_cnc;

	g_return_if_fail(cnc != NULL);

	default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
	if (default_cnc) {
		if (default_cnc->sqlite) {
			sqlite_close(default_cnc->sqlite);
			default_cnc->sqlite = NULL;
		}
	}
}

gint
gda_default_connection_begin_transaction (GdaServerConnection *cnc)
{
	DEFAULT_Connection *default_cnc;
	gint retval = -1;

	g_return_val_if_fail(cnc != NULL, -1);

	default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
	if (default_cnc) {
		gchar *errmsg = NULL;

		if (sqlite_exec(default_cnc->sqlite, "BEGIN", NULL, NULL, &errmsg) == SQLITE_OK)
			retval = 0;
		else
			gda_server_connection_add_error_string(cnc, errmsg);

		if (errmsg)
			free(errmsg);
	}
	return retval;
}

gint
gda_default_connection_commit_transaction (GdaServerConnection *cnc)
{
	DEFAULT_Connection *default_cnc;
	gint retval = -1;

	g_return_val_if_fail(cnc != NULL, -1);

	default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
	if (default_cnc) {
		gchar *errmsg = NULL;

		if (sqlite_exec(default_cnc->sqlite, "COMMIT", NULL, NULL, &errmsg) == SQLITE_OK)
			retval = 0;
		else
			gda_server_connection_add_error_string(cnc, errmsg);

		if (errmsg)
			free(errmsg);
	}
	return retval;
}

gint
gda_default_connection_rollback_transaction (GdaServerConnection *cnc)
{
	DEFAULT_Connection *default_cnc;
	gint retval = -1;

	g_return_val_if_fail(cnc != NULL, -1);

	default_cnc = (DEFAULT_Connection *) gda_server_connection_get_user_data(cnc);
	if (default_cnc) {
		gchar *errmsg = NULL;

		if (sqlite_exec(default_cnc->sqlite, "ROLLBACK", NULL, NULL, &errmsg) == SQLITE_OK)
			retval = 0;
		else
			gda_server_connection_add_error_string(cnc, errmsg);

		if (errmsg)
			free(errmsg);
	}
	return retval;
}

GdaServerRecordset *
gda_default_connection_open_schema (GdaServerConnection *cnc,
				    GdaError *error,
				    GDA_Connection_QType t,
				    GDA_Connection_Constraint *constraints,
				    gint length)
{
	schema_ops_fn fn;
	
	g_return_val_if_fail(cnc != NULL, NULL);
	
	fn = schema_ops[(gint) t];
	if (fn)
		return fn(error, cnc, constraints, length);

	/* we don't support this schema type */
	gda_server_error_make (error, NULL, cnc, __PRETTY_FUNCTION__);
	gda_error_set_description (error, _("Unknown schema type"));

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
	case GDA_Connection_FEATURE_TRANSACTIONS :
		return TRUE;
	default :
		return FALSE;
	}

	return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_default_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
	return GDA_TypeVarchar;
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
gda_default_error_make (GdaError *error,
			GdaServerRecordset *recset,
			GdaServerConnection *cnc,
			gchar *where)
{
	g_return_if_fail(error != NULL);

	gda_error_set_source(error, "[gda-default]");
	gda_error_set_help_url(error, "(null)");
	gda_error_set_help_context(error, "(null)");
	gda_error_set_sqlstate(error, where);
	gda_error_set_native(error, where);
}

/*
 * Schema functions
 */
static GdaServerRecordset *
schema_tables (GdaError *error,
               GdaServerConnection *cnc,
               GDA_Connection_Constraint *constraints,
               gint length)
{
	GString *query;
	GdaServerRecordset *recset = NULL;
	GdaServerCommand *cmd = NULL;
	GDA_Connection_Constraint* ptr;
	gint cnt;
	gulong affected;

	query = g_string_new("SELECT name FROM sqlite_master WHERE type = 'table'");

	/* process constraints */
	ptr = constraints;
	for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
		switch (ptr->ctype) {
		case GDA_Connection_EXTRA_INFO :
			break;
		case GDA_Connection_OBJECT_NAME :
			query = g_string_append(query, " AND name = '");
			query = g_string_append(query, ptr->value);
			query = g_string_append(query, "'");
			break;
		case GDA_Connection_OBJECT_SCHEMA :
			break;
		case GDA_Connection_OBJECT_CATALOG :
			break;
		default :
			gda_error_set_description(error, "Invalid constraint type");
			g_string_free(query, TRUE);
			break;
		}
	}

	/* build the command object */
	cmd = gda_server_command_new(cnc);
	gda_server_command_set_text(cmd, query->str);

	g_string_free(query, TRUE);

	/* execute the command */
	recset = gda_server_command_execute(cmd, error, NULL, &affected, 0);
	gda_server_command_free(cmd);

	return recset;
}

static GdaServerRecordset *
schema_columns (GdaError *error,
                GdaServerConnection *cnc,
                GDA_Connection_Constraint *constraints,
                gint length)
{
	return NULL;
}
