/* GDA Xbase Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <xbase.h>
#include "gda-xbase-database.h"
#include "gda-xbase-provider.h"

struct _GdaXbaseDatabase {
	GdaConnection *cnc;
	gchar *filename;
	xbDbf *dbf;
};

static xbXBase xb;  /* initialize xbase library */

extern "C" GdaXbaseDatabase *
gda_xbase_database_open (GdaConnection *cnc, const gchar *filename)
{
	GdaXbaseDatabase *xdb;
	xbShort error;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	xdb = g_new0 (GdaXbaseDatabase, 1);
	xdb->cnc = cnc;
	xdb->filename = g_strdup (filename);

	/* open the database */
	xdb->dbf = new xbDbf (&xb);
	error = xdb->dbf->OpenDatabase (filename);
	if (error != XB_NO_ERROR) {
		gda_xbase_provider_make_error (cnc);
		return NULL;
	}

	return xdb;
}

extern "C" GdaXbaseDatabase *
gda_xbase_database_create (GdaConnection *cnc, const gchar *filename)
{
	GdaXbaseDatabase *xdb;
	xbShort error;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	xdb = g_new0 (GdaXbaseDatabase, 1);
	xdb->cnc = cnc;
	xdb->filename = g_strdup (filename);

	/* create the database */
	/* FIXME */

	return NULL;
}

extern "C" void
gda_xbase_database_close (GdaXbaseDatabase *xdb)
{
	g_return_if_fail (xdb != NULL);

	xdb->dbf->CloseDatabase ();
	delete xdb->dbf;
	xdb->dbf = NULL;

	g_free (xdb->filename);
	xdb->filename = NULL;

	g_free (xdb);
}

extern "C" gboolean
gda_xbase_database_delete_all_records (GdaXbaseDatabase *xdb)
{
	xbShort error;

	g_return_val_if_fail (xdb != NULL, FALSE);

	error = xdb->dbf->DeleteAllRecords ();
	if (error != XB_NO_ERROR) {
		gda_xbase_provider_make_error (xdb->cnc);
		return FALSE;
	}

	return TRUE;
}
