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

#if !defined(__gda_xql_const_h__)
#  define __gda_xql_const_h__

#ifdef __cplusplus
extern "C" {
#endif

#define GDA_TYPE_XQL_CONST_ITEM            (gda_xql_const_item_get_type ())
#define GDA_XQL_CONST_ITEM(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XQL_CONST_ITEM, GdaXqlConstItem)
#define GDA_XQL_CONST_ITEM_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XQL_CONST_ITEM, GdaXqlConstItemClass)
#define GDA_IS_XQL_CONST_ITEM(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XQL_CONST)
#define GDA_IS_XQL_CONST_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XQL_CONST_ITEM))

typedef struct _GdaXqlConstItem      GdaXqlConstItem;
typedef struct _GdaXqlConstItemClass GdaXqlConstItemClass;

struct _GdaXqlConst {
	GdaXmlAtomItem item;
};

struct _GdaXqlConstClass {
	GdaXmlAtomItemClass parent_class;
};

GtkType     gda_xql_const_get_type      (void);
GdaXmlItem *gda_xql_const_new           (void);
GdaXmlITem *gda_xql_const_new_with_data (const gchar *value,
					 const gchar *alias,
					 const gchar *type,
					 const gchar *null);

#endif
