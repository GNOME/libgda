/* GNOME DB Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include "gda-server-impl.h"

/**
 * gda_server_error_new
 * @cnc: connection object
 */
Gda_ServerError *
gda_server_error_new (void)
{
  return g_new0(Gda_ServerError, 1);
}

/**
 * gda_server_error_get_description
 */
gchar *
gda_server_error_get_description (Gda_ServerError *error)
{
  g_return_val_if_fail(error != NULL, NULL);
  return error->description;
}

/**
 * gda_server_error_set_description
 */
void
gda_server_error_set_description (Gda_ServerError *error, const gchar *description)
{
  g_return_if_fail(error != NULL);

  if (error->description) g_free((gpointer) error->description);
  error->description = description ? g_strdup(description) : NULL;
}

/**
 * gda_server_error_get_number
 */
glong
gda_server_error_get_number (Gda_ServerError *error)
{
  g_return_val_if_fail(error != NULL, -1);
  return error->number;
}

/**
 * gda_server_error_set_number
 */
void
gda_server_error_set_number (Gda_ServerError *error, glong number)
{
  g_return_if_fail(error != NULL);
  error->number = number;
}

/**
 * gda_server_error_set_source
 */
void
gda_server_error_set_source (Gda_ServerError *error, const gchar *source)
{
  g_return_if_fail(error != NULL);

  if (error->source) g_free((gpointer) error->source);
  error->source = source ? g_strdup(source) : NULL;
}

/**
 * gda_server_error_set_help_file
 */
void
gda_server_error_set_help_file (Gda_ServerError *error, const gchar *helpfile)
{
  g_return_if_fail(error != NULL);

  if (error->helpfile) g_free((gpointer) error->helpfile);
  error->helpfile = helpfile ? g_strdup(helpfile) : NULL;
}

/**
 * gda_server_error_set_help_context
 */
void
gda_server_error_set_help_context (Gda_ServerError *error, const gchar *helpctxt)
{
  g_return_if_fail(error != NULL);

  if (error->helpctxt) g_free((gpointer) error->helpctxt);
  error->helpctxt = helpctxt ? g_strdup(helpctxt) : NULL;
}

/**
 * gda_server_error_set_sqlstate
 */
void
gda_server_error_set_sqlstate (Gda_ServerError *error, const gchar *sqlstate)
{
  g_return_if_fail(error != NULL);

  if (error->sqlstate) g_free((gpointer) error->sqlstate);
  error->sqlstate = sqlstate ? g_strdup(sqlstate) : NULL;
}

/**
 * gda_server_error_set_native
 */
void
gda_server_error_set_native (Gda_ServerError *error, const gchar *native)
{
  g_return_if_fail(error != NULL);

  if (error->native) g_free((gpointer) error->native);
  error->native = native ? g_strdup(native) : NULL;
}

/**
 * gda_server_error_free
 * @error: error object
 */
void
gda_server_error_free (Gda_ServerError *error)
{
  g_return_if_fail(error != NULL);

  if (error->description) g_free((gpointer) error->description);
  if (error->source) g_free((gpointer) error->source);
  if (error->helpfile) g_free((gpointer) error->helpfile);
  if (error->helpctxt) g_free((gpointer) error->helpctxt);
  if (error->sqlstate) g_free((gpointer) error->sqlstate);
  if (error->native) g_free((gpointer) error->native);
  g_free((gpointer) error);
}

/**
 * gda_server_error_make
 */
void
gda_server_error_make (Gda_ServerError *error,
		       Gda_ServerRecordset *recset,
		       Gda_ServerConnection *cnc,
		       gchar *where)
{
  Gda_ServerConnection* cnc_to_use = NULL;

  g_return_if_fail(error != NULL);

  if (cnc) cnc_to_use = cnc;
  else if (recset) cnc_to_use = recset->cnc;

  if (!cnc_to_use)
    {
      gda_log_message(_("Could not get pointer to server implementation"));
      return;
    }

  g_return_if_fail(cnc_to_use->server_impl != NULL);
  g_return_if_fail(cnc_to_use->server_impl->functions.error_make != NULL);

  cnc_to_use->server_impl->functions.error_make(error, recset, cnc, where);

  gda_server_connection_add_error(cnc_to_use, error);
}

