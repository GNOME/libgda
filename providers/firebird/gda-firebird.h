c/* GDA firebird provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_FIREBIRD_H__
#define __GDA_FIREBIRD_H__

/*
 * Provider name
 */
#define FIREBIRD_PROVIDER_NAME "Firebird"

#include <ibase.h>

/*
 * Provider's specific connection data
 */
typedef struct {
	gchar         *dbname;
	gchar         *server_version;
        isc_db_handle  handle;
        ISC_STATUS     status[20];
        gchar          dpb_buffer[128];
        gshort         dpb_length;

	/* transaction */
	isc_tr_handle *ftr;
} FirebirdConnectionData;

#endif
