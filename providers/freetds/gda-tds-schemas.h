/* GDA FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS: Holger Thon <holger.thon@gnome-db.org>
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

#if !defined(__gda_tds_schemas_h__)
#  define __gda_tds_schemas_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

G_BEGIN_DECLS

#define TDS_SCHEMA_DATABASES \
	"SELECT name " \
	"  FROM sysdatabases " \
	" ORDER BY name"

#define TDS_SCHEMA_FIELDS \
	"SELECT c.name, t.name AS typename, " \
	       "c.length, c.prec, c.scale, " \
	       "(CASE WHEN ((c.status & 0x80) = 0x80) " \
	             "THEN 1 " \
	             "ELSE 0 " \
	       " END " \
	       ") AS \"identity\", " \
	       "(CASE WHEN ((c.status & 0x08) = 0x08) " \
	             "THEN 1 " \
	             "ELSE 0 " \
	       " END " \
	       ") AS nullable, " \
	       "c.domain, " \
	       "c.printfmt " \
	"  FROM syscolumns c, systypes t " \
	"    WHERE (c.id = OBJECT_ID('%s')) " \
	"      AND (c.usertype = t.usertype) " \
	"  ORDER BY c.colid ASC"

#define TDS_SCHEMA_PROCEDURES \
	"SELECT name " \
        "  FROM sysobjects " \
	" WHERE (type = 'P') OR " \
	"       (type = 'XP') " \
	" ORDER BY name"

#define TDS_SCHEMA_TABLES \
	"SELECT name " \
	"  FROM sysobjects " \
	" WHERE (type = 'U') AND " \
	"       (name NOT LIKE 'spt_%') AND " \
	"       (name != 'syblicenseslog') " \
	" ORDER BY name"

#define TDS_SCHEMA_TYPES \
	"SELECT name " \
	"  FROM systypes " \
	" ORDER BY name"

#define TDS_SCHEMA_USERS \
	"SELECT name " \
	"  FROM syslogins " \
	" ORDER BY name"

#define TDS_SCHEMA_VIEWS \
	"SELECT name " \
	"  FROM sysobjects " \
	" WHERE (type = 'V') " \
	" ORDER BY name"

G_END_DECLS

#endif

