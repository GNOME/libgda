/* 
 * $Id$
 *
 * GNOME DB tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
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

#if !defined(__gda_tds_error_h__)
#  define __gda_tds_error_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include "gda-tds.h"

#define GDA_TDS_DEBUG_LEVEL 1

#define TDS_ERR_NONE      0
#define TDS_ERR_CSLIB     1
#define TDS_ERR_CLIENT    2
#define TDS_ERR_SERVER    3
#define TDS_ERR_USERDEF   4
#define TDS_ERR_UNDEFINED 5

gint gda_tds_install_error_handlers(GdaServerConnection *);

void gda_tds_error_make (GdaServerError *error,
                         GdaServerRecordset *recset,
                         GdaServerConnection *cnc,
                         gchar *where);

void     gda_tds_cleanup(tds_Connection *, CS_RETCODE, const gchar *);

// Don't forget to FREE the results :-)
gchar *g_sprintf_clientmsg(const gchar*, CS_CLIENTMSG *);
gchar *g_sprintf_servermsg(const gchar*, CS_SERVERMSG *);

void     gda_tds_log_clientmsg(const gchar *, CS_CLIENTMSG *);
void     gda_tds_log_servermsg(const gchar *, CS_SERVERMSG *);

void     gda_tds_debug_msg(gint, const gchar *);

#endif
