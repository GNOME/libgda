/* GDA Berkeley-DB Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Laurent Sansonetti <lrz@gnome.org>  
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

#ifndef __GDA_BDB_H__
#define __GDA_BDB_H__

#if defined(HAVE_CONFIG_H)
#endif

#include <glib/gmacros.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>
#include <db.h>

#include "gda-bdb-provider.h"
#include "gda-bdb-recordset.h"

#define GDA_BDB_PROVIDER_ID          "GDA Berkeley DB provider"

#define BDB_VERSION  (10000*DB_VERSION_MAJOR+100*DB_VERSION_MINOR+DB_VERSION_PATCH)

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaConnectionEvent *gda_bdb_make_error (int ret);

G_END_DECLS

#endif /* __gda_bdb_h__ */
