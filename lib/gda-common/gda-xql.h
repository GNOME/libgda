/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
 *	Gerhard Dieringer <gdieringer@compuserve.com>
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

#if !defined(__gda_xql_h__)
#  define __gda_xql_h__

#include <gda-xml-bin.h>
#include <gda-xml-document.h>
#include <gda-xml-item.h>
#include <gda-xql-column.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Convenience functions for creating XML nodes
 */
#define gda_xql_item_new_union()                  gda_xml_bin_item_new("union")
#define gda_xql_item_new_unionall()               gda_xml_bin_item_new("unionall")
#define gda_xql_item_new_intersect()              gda_xml_bin_item_new("intersect")
#define gda_xql_item_new_minus()                  gda_xml_bin_item_new("minus")
#define gda_xql_item_new_where()                  gda_xml_bin_item_new("where")
#define gda_xql_item_new_where_with_data(_data_)  gda_xml_bin_item_new_with_data("where", (_data_))
#define gda_xql_item_new_having()                 gda_xml_bin_item_new("having")
#define gda_xql_item_new_having_with_data(_data_) gda_xml_bin_item_new_with_data("having", (_data_))
#define gda_xql_item_new_on()                     gda_xml_bin_item_new("on")
#define gda_xql_item_new_not()                    gda_xml_bin_item_new("not")
#define gda_xql_item_new_not_with_data(_data_)    gda_xml_bin_item_new_with_data("not", (_data_))
#define gda_xql_item_new_exists()                 gda_xml_bin_item_new("exists")
#define gda_xql_item_new_null()                   gda_xml_bin_item_new("null")
#define gda_xql_item_new_null_with_data(_data_)   gda_xml_bin_item_new_with_data("null", (_data_))

#define gda_xql_item_new_setlist()                gda_xml_list_item_new("setlist")
#define gda_xql_item_new_sourcelist()             gda_xml_list_item_new("sourcelist")
#define gda_xql_item_new_targetlist()             gda_xml_list_item_new("targetlist")
#define gda_xql_item_new_order()                  gda_xml_list_item_new("order")
#define gda_xql_item_new_dest()                   gda_xml_list_item_new("dest")
#define gda_xql_item_new_arglist()                gda_xml_list_item_new("arglist")
#define gda_xql_item_new_valuelist()              gda_xml_list_item_new("valuelist")
#define gda_xql_item_new_joinlist()               gda_xml_list_item_new("joinlist")
#define gda_xql_item_new_and()                    gda_xml_list_item_new("and")
#define gda_xql_item_new_or()                     gda_xml_list_item_new("or")
#define gda_xql_item_new_group()                  gda_xml_list_item_new("group")

#ifdef __cplusplus
}
#endif

#endif
