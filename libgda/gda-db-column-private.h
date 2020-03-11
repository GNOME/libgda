/* gda-db-column-private.h
 *
 * Copyright (C) 2020 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef GDA_DB_COLUMN_PRIVATE_H
#define GDA_DB_COLUMN_PRIVATE_H

#include "gda-meta-struct.h"
#include "gda-db-column.h"

GdaDbColumn*    gda_db_column_new_from_meta    (GdaMetaTableColumn *column);

#endif

