/* 
 * Copyright (C) 2007 Vivien Malerba
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

#ifndef _GDA_STATEMENT_STRUCT_PSPEC_H
#define _GDA_STATEMENT_STRUCT_PSPEC_H

#include <glib.h>
#include <glib-object.h>

typedef struct _GdaSqlParamSpec GdaSqlParamSpec;

/*
 * Structure to hold one parameter specification
 */
struct _GdaSqlParamSpec
{
	gchar    *name;
	gchar    *descr;
	gchar    *type;
	gboolean  is_param;
	gboolean  nullok;

	GType     g_type;
	gpointer  validity_meta_dict; /* to be replaced with a pointer to a structure representing a DBMS data type in GdaMetaStruct */
};
GdaSqlParamSpec *gda_sql_param_spec_new        (GValue *simple_spec);
GdaSqlParamSpec *gda_sql_param_spec_copy       (GdaSqlParamSpec *pspec);
void             gda_sql_param_spec_take_name  (GdaSqlParamSpec *pspec, GValue *value);
void             gda_sql_param_spec_take_type  (GdaSqlParamSpec *pspec, GValue *value);
void             gda_sql_param_spec_take_descr (GdaSqlParamSpec *pspec, GValue *value);
void             gda_sql_param_spec_take_nullok(GdaSqlParamSpec *pspec, GValue *value);

void             gda_sql_param_spec_free       (GdaSqlParamSpec *pspec);
gchar           *gda_sql_param_spec_serialize  (GdaSqlParamSpec *pspec);


#endif
