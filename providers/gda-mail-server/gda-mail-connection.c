/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; fill-column: 160 -*- */
/* GNOME DB Mail Provider
 * Copyright (C) 2000 Akira TAGOH
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

#include "gda-mail.h"
#include <ctype.h>

typedef Gda_ServerRecordset * (*schema_ops_fn)(Gda_ServerError *,
											   Gda_ServerConnection *,
											   GDA_Connection_Constraint *,
											   gint);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] =
{
	0,
};

/*
 * Public functions
 */
gboolean
gda_mail_connection_new (Gda_ServerConnection * cnc)
{
	g_return_val_if_fail(cnc != NULL, FALSE);

	return FALSE;
}

static gchar *
get_value (gchar * ptr)
{
	while (*ptr && *ptr != '=') ptr++;
	if (!*ptr)
		return 0;
	ptr++;
	if (!*ptr)
		return 0;
	while (*ptr && isspace(*ptr))
		ptr++;

	return (g_strdup(ptr));
}

gint
gda_mail_connection_open (Gda_ServerConnection * cnc,
						  const gchar * dsn,
						  const gchar * user,
						  const gchar * password)
{
	g_return_val_if_fail(cnc != NULL, -1);

	return -1;
}

void
gda_mail_connection_close (Gda_ServerConnection * cnc)
{
}

gint
gda_mail_connection_begin_transaction (Gda_ServerConnection * cnc)
{
	return -1;
}

gint
gda_mail_connection_commit_transaction (Gda_ServerConnection * cnc)
{
	return -1;
}

gint
gda_mail_connection_rollback_transaction (Gda_ServerConnection * cnc)
{
	return -1;
}

Gda_ServerRecordset *
gda_mail_connection_open_schema (Gda_ServerConnection * cnc,
								 Gda_ServerError * error,
								 GDA_Connection_QType t,
								 GDA_Connection_Constraint * constraints,
								 gint length)
{
	schema_ops_fn fn;

	g_return_val_if_fail(cnc != NULL, NULL);

	fn = schema_ops[(gint)t];
	if ( fn )
		return fn(error, cnc, constraints, length);
	else
		gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint)t);

	return NULL;
}

gint
gda_mail_connection_start_logging (Gda_ServerConnection * cnc,
								   const gchar * filename)
{
	return -1;
}

gint
gda_mail_connection_stop_logging (Gda_ServerConnection * cnc)
{
	return -1;
}

gchar *
gda_mail_connection_create_table (Gda_ServerConnection * cnc,
								  GDA_RowAttributes * columns)
{
	return NULL;
}

gboolean
gda_mail_connection_supports (Gda_ServerConnection * cnc,
							  GDA_Connection_Feature feature)
{
	g_return_val_if_fail(cnc != NULL, FALSE);

	if (feature == GDA_Connection_FEATURE_TRANSACTIONS) return (FALSE);

	return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_mail_connection_get_gda_type (Gda_ServerConnection * cnc, gulong sql_type)
{
	g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

	switch (sql_type)
    {
    }

	return GDA_TypeNull;
}

gshort
gda_mail_connection_get_c_type (Gda_ServerConnection * cnc, GDA_ValueType type)
{
	g_return_val_if_fail(cnc != NULL, -1);

	//switch (type)
	//  {
	//  }
	return -1;
}

void
gda_mail_connection_free (Gda_ServerConnection * cnc)
{
}

void
gda_mail_error_make (Gda_ServerError * error,
					 Gda_ServerRecordset * recset,
					 Gda_ServerConnection * cnc,
					 gchar * where)
{
}

/*
 * Schema functions
 */
