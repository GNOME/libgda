/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2005 Alan Knowles <alan@akbkhome.com>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_MYSQL_H__
#define __GDA_MYSQL_H__

/*
 * Provider name
 */
#define MYSQL_PROVIDER_NAME "MySQL"

#include <libgda/libgda.h>
#include <libgda/gda-connection-private.h>
#ifdef G_OS_WIN32
#include <winsock.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <gda-mysql-reuseable.h>

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	GdaMysqlReuseable *reuseable;
	GdaConnection     *cnc;
	MYSQL             *mysql;	
} MysqlConnectionData;

// Makes back my_bool
#ifdef MYSQL8
#ifndef __MY_BOOL__
#define __MY_BOOL__

typedef _Bool my_bool;
#endif
#endif

#endif
