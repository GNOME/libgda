/* 
 * $Id$
 *
 * GNOME DB sybase Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Holger Thon
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

#if !defined(__gda_sybase_error_h__)
#  define __gda_sybase_error_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include "gda-sybase.h"

#define GDA_SYBASE_DEBUG_LEVEL 1

#define SYBASE_ERR_NONE      0
#define SYBASE_ERR_CSLIB     1
#define SYBASE_ERR_CLIENT    2
#define SYBASE_ERR_SERVER    3
#define SYBASE_ERR_USERDEF   4
#define SYBASE_ERR_UNDEFINED 5

gint gda_sybase_install_error_handlers(Gda_ServerConnection *);

void gda_sybase_error_make (Gda_ServerError *error,
			       Gda_ServerRecordset *recset,
			       Gda_ServerConnection *cnc,
			       gchar *where);

void     gda_sybase_cleanup(sybase_Connection *, CS_RETCODE, const gchar *);

gboolean gda_sybase_messages_install(Gda_ServerConnection *);
void     gda_sybase_messages_uninstall(Gda_ServerConnection *);

void     sybase_chkerr(Gda_ServerError      *,
                       Gda_ServerRecordset  *,
                       Gda_ServerConnection *,
                       Gda_ServerCommand    *,
                       gchar                *);
CS_RETCODE sybase_exec_chk(CS_RETCODE *,
                           CS_RETCODE,
                           Gda_ServerError *,
                           Gda_ServerRecordset *,
                           Gda_ServerConnection *,
                           Gda_ServerCommand *,
                           gchar *);

#ifdef SYBASE_DEBUG
#define SYB_CHK(retptr, retcode, err, rec, cnc, cmd) \
	sybase_exec_chk(retptr, retcode, err, rec, cnc, cmd, \
                   g_strdup_printf("%s at line %d (%s): ", __FILE__, __LINE__, \
	                __PRETTY_FUNCTION__))
#else
#define SYB_CHK(retptr, retcode, err, rec, cnc, cmd) \
	sybase_exec_chk(retptr, retcode, err, rec, cnc, cmd, \
						 __PRETTY_FUNCTION__)
#endif

void     gda_sybase_messages_handle(Gda_ServerError *,
                                    Gda_ServerRecordset *,
                                    Gda_ServerConnection *,
                                    gchar *where);
void     gda_sybase_messages_handle_clientmsg(Gda_ServerError *,
                                              Gda_ServerRecordset *,
                                              Gda_ServerConnection *,
                                              gchar *where);
void     gda_sybase_messages_handle_servermsg(Gda_ServerError *,
                                              Gda_ServerRecordset *,
                                              Gda_ServerConnection *,
                                              gchar *where);
void     gda_sybase_messages_handle_csmsg(Gda_ServerError *,
                                          Gda_ServerRecordset *,
                                          Gda_ServerConnection *,
                                          gchar *where);

// Don't forget to FREE the results :-)
gchar *g_sprintf_clientmsg(const gchar*, CS_CLIENTMSG *);
gchar *g_sprintf_servermsg(const gchar*, CS_SERVERMSG *);

void     gda_sybase_log_clientmsg(const gchar *, CS_CLIENTMSG *);
void     gda_sybase_log_servermsg(const gchar *, CS_SERVERMSG *);

void     gda_sybase_debug_msg(gint, const gchar *);

#endif
