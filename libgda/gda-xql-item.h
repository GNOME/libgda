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

#if !defined(__gda_xql_item_h__)
#  define __gda_xql_item_h__

#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

#define TO_DOM(_o_,_n_) if (_o_ != NULL) gda_xql_item_to_dom(_o_,_n_)

#define GDA_TYPE_XQL_ITEM	(gda_xql_item_get_type())
#define GDA_XQL_ITEM(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_item_get_type(), GdaXqlItem)
#define GDA_XQL_ITEM_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_item_get_type(), GdaXqlItem const)
#define GDA_XQL_ITEM_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_item_get_type(), GdaXqlItemClass)
#define GDA_IS_XQL_ITEM(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_item_get_type ())
#define GDA_XQL_ITEM_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_item_get_type(), GdaXqlItemClass)

typedef struct _GdaXqlItem GdaXqlItem;
typedef struct _GdaXqlItemClass GdaXqlItemClass;
typedef struct _GdaXqlItemPrivate GdaXqlItemPrivate;

struct _GdaXqlItem {
	GObject object;

	GdaXqlItemPrivate *priv;
};

struct _GdaXqlItemClass {
	GObjectClass parent_class;

	xmlNode * (* to_dom) (GdaXqlItem *xqlitem, xmlNode *parNode);
	void (* add) (GdaXqlItem *xqlitem, GdaXqlItem *child);
	GdaXqlItem * (* find_id) (GdaXqlItem *xqlitem, gchar *id);
	GdaXqlItem * (* find_ref) (GdaXqlItem *xqlitem, gchar *ref);
};



GType	gda_xql_item_get_type	(void) G_GNUC_CONST;
void 	gda_xql_item_set_attrib	(GdaXqlItem *xqlitem,
				 gchar * attrib,
				 gchar * value);

GdaXqlItem *gda_xql_item_get_parent (GdaXqlItem *xqlitem);
void 	    gda_xql_item_set_parent (GdaXqlItem *xqlitem,
				     GdaXqlItem *parent);

gchar      *gda_xql_item_get_attrib (GdaXqlItem *xqlitem,
				     gchar *attrib);
xmlNode    *gda_xql_item_to_dom	    (GdaXqlItem *xqlitem,
				     xmlNode *parent);
void 	    gda_xql_item_add	    (GdaXqlItem *xqlitem,
				     GdaXqlItem *child);
void 	    gda_xql_item_set_tag    (GdaXqlItem *xqlitem,
				     gchar *tag);
gchar      *gda_xql_item_get_tag    (GdaXqlItem *xqlitem);

GdaXqlItem *gda_xql_item_find_root   (GdaXqlItem *xqlitem);
GdaXqlItem *gda_xql_item_find_id     (GdaXqlItem *xqlitem,
				      gchar *id);
GdaXqlItem *gda_xql_item_find_ref    (GdaXqlItem *xqlitem,
				      gchar *ref);
void 	    gda_xql_item_add_id	     (GdaXqlItem *xqlitem,
				      gchar *id);
void 	    gda_xql_item_add_ref     (GdaXqlItem *xqlitem,
				      gchar *ref);
GdaXqlItem *gda_xql_item_get_ref     (GdaXqlItem *xqlitem,
				      gchar *ref);

G_END_DECLS

#endif
