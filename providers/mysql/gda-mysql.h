/* GDA mysql provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Savoretti <csavoretti@gmail.com>
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

#ifndef __GDA_MYSQL_H__
#define __GDA_MYSQL_H__

/*
 * Provider name
 */
#define MYSQL_PROVIDER_NAME "MySQL"

#include <libgda/libgda.h>
#ifdef G_OS_WIN32
#include <winsock.h>
#endif
#include <mysql.h>

/*
 * Provider's specific connection data
 */
typedef struct {
	/* TO_ADD: this structure holds any information necessary to specialize the GdaConnection, usually a connection
	 * handle from the C or C++ API
	 */
	
	GdaConnection    *cnc;
	MYSQL            *mysql;

	/* Backend version (to which we're connected). */
	gchar            *version;
	unsigned long     version_long;
	gchar            *short_version;

	/* specifies how case sensitiveness is */
	gboolean          tables_case_sensitive;
	
} MysqlConnectionData;

#endif
