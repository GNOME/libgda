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

gboolean
gda_mail_recordset_new (GdaServerRecordset * recset)
{
	return TRUE;
}

gint
gda_mail_recordset_move_next (GdaServerRecordset * recset)
{
	return -1;
}

gint
gda_mail_recordset_move_prev (GdaServerRecordset * recset)
{
	return -1;
}

gint
gda_mail_recordset_close (GdaServerRecordset * recset)
{
	return -1;
}

void
gda_mail_recordset_free (GdaServerRecordset * recset)
{
}
