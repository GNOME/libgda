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

#if !defined(__gda_xql_list_h__)
#  define __gda_xql_list_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-item.h"

#define GDA_TYPE_XQL_LIST	(gda_xql_list_get_type())
#define GDA_XQL_LIST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_list_get_type(), GdaXqlList)
#define GDA_XQL_LIST_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_list_get_type(), GdaXqlList const)
#define GDA_XQL_LIST_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_list_get_type(), GdaXqlListClass)
#define GDA_IS_XQL_LIST(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_list_get_type ())

#define GDA_XQL_LIST_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_list_get_type(), GdaXqlListClass)

typedef struct _GdaXqlList GdaXqlList;
typedef struct _GdaXqlListClass GdaXqlListClass;
typedef struct _GdaXqlListPrivate GdaXqlListPrivate;

struct _GdaXqlList {
	GdaXqlItem xqlitem;
	/*< private >*/
	GdaXqlListPrivate *priv;
};


struct _GdaXqlListClass {
	GdaXqlItemClass parent_class;
};

GType	    gda_xql_list_get_type	(void);
GdaXqlItem *gda_xql_list_new	        (gchar * tag);
void 	    gda_xql_list_unref_list	(GSList * list);
GdaXqlItem *gda_xql_list_new_setlist	(void);
GdaXqlItem *gda_xql_list_new_sourcelist	(void);
GdaXqlItem *gda_xql_list_new_targetlist	(void);
GdaXqlItem *gda_xql_list_new_order	(void);
GdaXqlItem *gda_xql_list_new_dest	(void);
GdaXqlItem *gda_xql_list_new_arglist	(void);
GdaXqlItem *gda_xql_list_new_valuelist	(void);
GdaXqlItem *gda_xql_list_new_joinlist	(void);
GdaXqlItem *gda_xql_list_new_and	(void);
GdaXqlItem *gda_xql_list_new_or	        (void);
GdaXqlItem *gda_xql_list_new_group	(void);

G_END_DECLS

#endif
