/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_PSPEC_H
#define _GDA_STATEMENT_STRUCT_PSPEC_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GdaSqlParamSpec GdaSqlParamSpec;

/*
 * Structure to hold one parameter specification
 */
struct _GdaSqlParamSpec
{
	gchar    *name;
	gchar    *descr;
	gboolean  is_param;
	gboolean  nullok;

	GType     g_type;
	gpointer  validity_meta_dict; /* to be replaced with a pointer to a structure representing a DBMS data type in GdaMetaStruct */

	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};
#define GDA_TYPE_SQL_PARAM_SPEC gda_sql_param_get_type()
GType gda_sql_param_spec_get_type(void) G_GNUC_CONST;

GdaSqlParamSpec *gda_sql_param_spec_new        (GValue *simple_spec);
GdaSqlParamSpec *gda_sql_param_spec_copy       (GdaSqlParamSpec *pspec);
void             gda_sql_param_spec_take_name  (GdaSqlParamSpec *pspec, GValue *value);
void             gda_sql_param_spec_take_type  (GdaSqlParamSpec *pspec, GValue *value);
void             gda_sql_param_spec_take_descr (GdaSqlParamSpec *pspec, GValue *value);
void             gda_sql_param_spec_take_nullok(GdaSqlParamSpec *pspec, GValue *value);

void             gda_sql_param_spec_free       (GdaSqlParamSpec *pspec);
gchar           *gda_sql_param_spec_serialize  (GdaSqlParamSpec *pspec);

G_END_DECLS

#endif
