/* GDA Default Provider
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include "gda-default.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_default_command_new (GdaServerCommand *cmd)
{
	return TRUE;
}

GdaServerRecordset *
gda_default_command_execute (GdaServerCommand *cmd,
                           GdaServerError *error,
                           const GDA_CmdParameterSeq *params,
                           gulong *affected,
                           gulong options)
{
}

void
gda_default_command_free (GdaServerCommand *cmd)
{
}
