/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; fill-column: 160 -*- */
/* GNOME DB Mail Provider
 * Copyright (C) 2000 Akira TAGOH
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

#include "gda-mail.h"

/*
 * Private functions
 */

/*
 * Public functions
 */
gboolean
gda_mail_command_new (Gda_ServerCommand * cmd)
{
	return TRUE;
}

Gda_ServerRecordset *
gda_mail_command_execute (Gda_ServerCommand * cmd,
						  Gda_ServerError * error,
						  const GDA_CmdParameterSeq * params,
						  gulong * affected,
						  gulong options)
{
	Gda_ServerRecordset * recset = NULL;

	return recset;
}

void
gda_mail_command_free (Gda_ServerCommand * cmd)
{
}
