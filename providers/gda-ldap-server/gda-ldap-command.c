/* GNOME DB LDAP Provider
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

#include "gda-ldap.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_ldap_command_new (GdaServerCommand *cmd) {
	return TRUE;
}

GdaServerRecordset *
gda_ldap_command_execute (GdaServerCommand *cmd,
                          GdaServerError *error,
                          const GDA_CmdParameterSeq *params,
                          gulong *affected,
                          gulong options) {
	GdaServerRecordset* recset = NULL;
	
	switch (gda_server_command_get_cmd_type(cmd)) {
		case GDA_COMMAND_TYPE_TEXT :
		case GDA_COMMAND_TYPE_TABLE :
		case GDA_COMMAND_TYPE_STOREDPROCEDURE :
	}
	return recset;
}

void
gda_ldap_command_free (GdaServerCommand *cmd) {
}
