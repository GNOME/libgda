/* GdaXql: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
 * deGOBified by Cleber Rodrigues (cleber@gnome-db.org)
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-xql-func.h"
#include "gda-xql-field.h"
#include "gda-xql-const.h"
#include "gda-xql-list.h"

static void gda_xql_func_init (GdaXqlFunc *xqlfunc);
static void gda_xql_func_class_init (GdaXqlFuncClass *klass);

static GdaXqlBinClass *parent_class = NULL;

GType
gda_xql_func_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlFuncClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_func_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlFunc),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_func_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_BIN, "GdaXqlFunc", &info, 0);
	}

	return type;
}

static void 
gda_xql_func_init (GdaXqlFunc *xqlfunc)
{

}

static void 
gda_xql_func_class_init (GdaXqlFuncClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);
}

GdaXqlItem * 
gda_xql_func_new (void)
{
        GdaXqlItem  *func;

	func = g_object_new (GDA_TYPE_XQL_FUNC, NULL);
        gda_xql_item_set_tag (func, "func");

        return func;
}

GdaXqlItem * 
gda_xql_func_new_with_data (gchar *name, gchar *alias, gchar *aggregate)
{
        GdaXqlItem  *func;

        func = gda_xql_func_new();
        if (name != NULL)
            gda_xql_item_set_attrib(func,"name",name);
        if (alias != NULL)
            gda_xql_item_set_attrib(func,"alias",alias);
        if (aggregate != NULL)
            gda_xql_item_set_attrib(func,"aggregate",aggregate);

        return func;
}

void 
gda_xql_func_add_const_from_text (GdaXqlFunc *xqlfunc, gchar *value, gchar *type, gchar *null)
{
	GdaXqlItem  *it_const;
        GdaXqlBin   *bin;

	g_return_if_fail (xqlfunc != NULL);
	g_return_if_fail (GDA_IS_XQL_FUNC (xqlfunc));
	
        bin = GDA_XQL_BIN (xqlfunc);

	if (gda_xql_bin_get_child(bin) == NULL)
	    gda_xql_bin_set_child(bin,gda_xql_list_new_arglist());
	
         it_const  = gda_xql_const_new_with_data(value,NULL,type,null);
         gda_xql_item_add(gda_xql_bin_get_child(bin),it_const);
        
}

void 
gda_xql_func_add_field_from_text (GdaXqlFunc *xqlfunc, gchar *id, gchar *name, gchar *alias)
{
	GdaXqlItem  *field;
        GdaXqlBin   *bin;

	/* There used to be a 'gchar *id = NULL' here, but that shadows the function's
	   own parameter. What's going on here? */

	g_return_if_fail (xqlfunc != NULL);
	g_return_if_fail (GDA_IS_XQL_FUNC (xqlfunc));
	
        bin = GDA_XQL_BIN (xqlfunc);
        
	if (gda_xql_bin_get_child(bin) == NULL)
	    gda_xql_bin_set_child(bin,gda_xql_list_new_arglist());
	
        field = gda_xql_field_new_with_data(id,name,alias);
        gda_xql_item_add(gda_xql_bin_get_child(bin),field);
}
