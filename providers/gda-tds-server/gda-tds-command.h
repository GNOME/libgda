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

#if !defined(__gda_tds_command_h__)
#  define __gda_tds_command_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <glib.h>
#include <ctpublic.h>
#include "gda-tds.h"

/*
 * Per-object specific structures
 */

typedef struct _tds_Command {
  CS_COMMAND               *cmd;
} tds_Command;

/*
 * Server implementation prototypes
 */

gboolean gda_tds_command_new (Gda_ServerCommand *cmd);
Gda_ServerRecordset* gda_tds_command_execute (Gda_ServerCommand *cmd,
                                              Gda_ServerError *error,
                                              const GDA_CmdParameterSeq *params,
                                              gulong *affected,
                                              gulong options);
void gda_tds_command_free (Gda_ServerCommand *cmd);

#endif
