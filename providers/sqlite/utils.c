/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *         Carlos Perello Marin <carlos@gnome-db.org>
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-sqlite.h"

void
gda_sqlite_set_value (GdaValue *value,
		      GdaValueType type,
		      const gchar *thevalue,
		      gboolean isNull)
{
	if (isNull){
		gda_value_set_null (value);
		return;
	}

	switch (type) {
	case GDA_VALUE_TYPE_STRING :
		gda_value_set_string (value, thevalue);
		break;
	default :
		gda_value_set_string (value, thevalue);
	}
}
