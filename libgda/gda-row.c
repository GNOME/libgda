/* GDA library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
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

#include <libgda/gda-row.h>

/**
 * gda_row_new
 */
GdaRow *
gda_row_new (gint count)
{
	GdaRow *row;

	g_return_val_if_fail (count >= 0, NULL);

	row = GNOME_Database_Row__alloc ();
	CORBA_sequence_set_release (row, TRUE);
	row->_length = count;
	row->_buffer = CORBA_sequence_GNOME_Database_Field_allocbuf (count);

	return row;
}

/**
 * gda_row_free
 */
void
gda_row_free (GdaRow *row)
{
	g_return_if_fail (row != NULL);
	CORBA_free (row);
}
