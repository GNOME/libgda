/* GdaXql: the xml query object
 * Copyright (C) 2000-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Gerhard Dieringer <gdieringer@compuserve.com>
 * 	Cleber Rodrigues <cleber@gnome-db.org>
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

#if !defined(__gda_xql_stack_h__)
#  define __gda_xql_stack_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-item.h"

#define GDA_TYPE_XQL_STACK	(gda_xql_stack_get_type())
#define GDA_XQL_STACK(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_stack_get_type(), GdaXqlStack)
#define GDA_XQL_STACK_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_stack_get_type(), GdaXqlStack const)
#define GDA_XQL_STACK_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_stack_get_type(), GdaXqlStackClass)
#define GDA_IS_XQL_STACK(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_stack_get_type ())

#define GDA_XQL_STACK_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_stack_get_type(), GdaXqlStackClass)

typedef struct _GdaXqlStack GdaXqlStack;
typedef struct _GdaXqlStackClass GdaXqlStackClass;
typedef struct _GdaXqlStackPrivate GdaXqlStackPrivate;


struct _GdaXqlStack {
	GObject gobject;

	/*< private >*/
	GdaXqlStackPrivate *priv;
};

struct _GdaXqlStackClass {
	GObjectClass parent_class;
};


GType	     gda_xql_stack_get_type  (void);
GdaXqlStack *gda_xql_stack_new	     (void);
void 	     gda_xql_stack_push	     (GdaXqlStack *xqlstack,
				      GdaXqlItem *item);
GdaXqlItem  *gda_xql_stack_pop	     (GdaXqlStack *xqlstack);
gboolean     gda_xql_stack_empty     (GdaXqlStack *xqlstack);

G_END_DECLS

#endif
