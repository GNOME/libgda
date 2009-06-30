/* GDA oracle provider
 * Copyright (C) 2009 The GNOME Foundation.
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

#ifndef __GDA_ORACLE_H__
#define __GDA_ORACLE_H__

/*
 * Provider name
 */
#define ORACLE_PROVIDER_NAME "Oracle"

/* headers necessary for the C or C++ API */
#include <libgda/libgda.h>
#include <oci.h>

/*
 * Provider's specific connection data
 */
typedef struct {
	OCIEnv *henv;
        OCIError *herr;
        OCIServer *hserver;
        OCISvcCtx *hservice;
        OCISession *hsession;
        gchar *schema; /* the same as the username which opened the connection */

	gchar *version;
	guint8 major_version;
	guint8 minor_version;
} OracleConnectionData;

#endif
