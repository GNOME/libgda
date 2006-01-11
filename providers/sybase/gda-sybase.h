/* GDA Sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *         Holger Thon <holger.thon@gnome-db.org>
 *      based on the MySQL provider by
 *         Michael Lausch <michael@lausch.at>
 *	        Rodrigo Moya <rodrigo@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
 *         and the pre gnome 2.0 sybase provider
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

#if !defined(__gda_sybase_h__)
#  define __gda_sybase_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>
#include <ctpublic.h>
#include <cspublic.h>
#include "gda-sybase-provider.h"
#include "gda-sybase-schemas.h"

#define GDA_SYBASE_PROVIDER_ID		"GDA sybase provider"

G_BEGIN_DECLS

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_SYBASE_HANDLE "GDA_Sybase_SybaseHandle"
#define GDA_SYBASE_COMPONENT_FACTORY_ID "OAFIID:GNOME_Database_Sybase_ComponentFactory"

/*
 * Utility functions
 */

gboolean sybase_check_messages(GdaConnection *cnc);

CS_RETCODE CS_PUBLIC gda_sybase_csmsg_callback (CS_CONTEXT *context,
                                                CS_CLIENTMSG *msg);
CS_RETCODE CS_PUBLIC gda_sybase_clientmsg_callback (CS_CONTEXT *context,
                                                    CS_CONNECTION *conn,
                                                    CS_CLIENTMSG *msg);
CS_RETCODE CS_PUBLIC gda_sybase_servermsg_callback (CS_CONTEXT *context,
                                                    CS_CONNECTION *conn,
                                                    CS_SERVERMSG *msg);

GdaConnectionEvent *gda_sybase_make_error (GdaSybaseConnectionData *scnc, gchar *fmt, ...);

void sybase_debug_msg(gchar *fmt, ...);
void sybase_error_msg(gchar *fmt, ...);

G_END_DECLS

#endif
