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

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>

G_BEGIN_DECLS

#define TDS_QUERY_SERVER_VERSION \
	"SELECT (@@version) AS version"


#define TDS_FIXMODEL_SCHEMA_DATABASES(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("Database")); \
	}
#define TDS_SCHEMA_DATABASES \
	"SELECT name " \
	"  FROM master..sysdatabases " \
	" ORDER BY name"


#define TDS_FIXMODEL_SCHEMA_FIELDS(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("Field Name")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  1, \
		                                 _("Data type")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  2, \
		                                 _("Size")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  3, \
		                                 _("Scale")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  4, \
		                                 _("Not null?")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  5, \
		                                 _("Primary key?")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  6, \
		                                 _("Unique index?")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  7, \
		                                 _("References")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  8, \
		                                 _("Default value")); \
	}
#define TDS_SCHEMA_FIELDS \
	"SELECT c.name, t.name AS typename, " \
	       "c.length, c.scale, " \
	       "(CASE WHEN ((c.status & 0x08) = 0x08) " \
	             "THEN 1 " \
	             "ELSE 0 " \
	       " END " \
	       ") AS nullable, " \
	       "NULL, NULL, NULL, NULL " \
	"  FROM syscolumns c, systypes t " \
	"    WHERE (c.id = OBJECT_ID('%s')) " \
	"      AND (c.usertype = t.usertype) " \
	"  ORDER BY c.colid ASC"
/*
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
*/

#define TDS_FIXMODEL_SCHEMA_PROCEDURES(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("Procedure")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  1, \
		                                 _("Id")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  2, \
		                                 _("Owner")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  3, \
		                                 _("Comments")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  4, \
		                                 _("Return type")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  5, \
		                                 _("Nb args")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  6, \
		                                 _("Args types")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  7, \
		                                 _("Definition")); \
	}
#define TDS_SCHEMA_PROCEDURES \
	"SELECT name, NULL, NULL, NULL, NULL, NULL, NULL, NULL " \
        "  FROM sysobjects " \
	" WHERE (type = 'P') OR " \
	"       (type = 'XP') " \
	" ORDER BY name"


#define TDS_FIXMODEL_SCHEMA_TABLES(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("Table")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  1, \
		                                 _("Owner")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  2, \
		                                 _("Description")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  3, \
		                                 _("Definition")); \
	}
#define TDS_SCHEMA_TABLES \
	"SELECT name, NULL, NULL, NULL " \
	"  FROM sysobjects " \
	" WHERE (type = 'U') AND " \
	"       (name NOT LIKE 'spt_%') AND " \
	"       (name != 'syblicenseslog') " \
	" ORDER BY name"


#define TDS_FIXMODEL_SCHEMA_TYPES(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("Type")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  1, \
		                                 _("Owner")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  2, \
		                                 _("Comments")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  3, \
		                                 _("GDA type")); \
	}
#define TDS_SCHEMA_TYPES \
	"SELECT name, uid, length, type " \
	"  FROM systypes " \
	" ORDER BY name"


#define TDS_FIXMODEL_SCHEMA_USERS(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("Users")); \
	}
#define TDS_SCHEMA_USERS \
	"SELECT name " \
	"  FROM master..syslogins " \
	" ORDER BY name"


#define TDS_FIXMODEL_SCHEMA_VIEWS(model) \
	if (model) { \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  0, \
		                                 _("View")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  1, \
		                                 _("Owner")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  2, \
		                                 _("Comments")); \
		gda_data_model_set_column_title (GDA_DATA_MODEL (model),  3, \
		                                 _("Definition")); \
	}
#define TDS_SCHEMA_VIEWS \
	"SELECT name, NULL, NULL, NULL " \
	"  FROM sysobjects " \
	" WHERE (type = 'V') " \
	" ORDER BY name"

G_END_DECLS

#endif

