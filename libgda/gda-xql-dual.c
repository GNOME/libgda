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

#include "gda-xql-dual.h"

struct _GdaXqlDualPrivate {
	GdaXqlItem * left;
	GdaXqlItem * right;
};

static void gda_xql_dual_init (GdaXqlDual *xqldual);
static void gda_xql_dual_class_init (GdaXqlDualClass *klass);
static void gda_xql_dual_add (GdaXqlItem *parent, GdaXqlItem *child);
static xmlNode *gda_xql_dual_to_dom (GdaXqlItem *xqlitem, xmlNode *parent);
static GdaXqlItem *gda_xql_dual_find_id (GdaXqlItem *parent, gchar *id);

static GdaXqlItemClass *parent_class = NULL;

GType
gda_xql_dual_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlDualClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_dual_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlDual),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_dual_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_ITEM, "GdaXqlDual", &info, 0);
	}

	return type;
}


static void
gda_xql_dual_finalize(GObject *obj_self)
{

	GdaXqlDual *xqldual = GDA_XQL_DUAL (obj_self);

	if(G_OBJECT_CLASS(parent_class)->finalize) \
		(* G_OBJECT_CLASS(parent_class)->finalize)(obj_self);

	if(xqldual->priv->left) {
	  g_object_unref (xqldual->priv->left);
	  xqldual->priv->left = NULL;
	}
	if(xqldual->priv->right) {
	  g_object_unref (xqldual->priv->right);
	  xqldual->priv->right = NULL;
	}

	g_free (xqldual->priv);
}

static void 
gda_xql_dual_init (GdaXqlDual *xqldual)
{
	xqldual->priv = g_new0 (GdaXqlDualPrivate, 1);
	xqldual->priv->left = NULL;
	xqldual->priv->right = NULL;
	return;
}

static void 
gda_xql_dual_class_init (GdaXqlDualClass *klass)
{
        GObjectClass *object_class;
	GdaXqlItemClass *xqlitemclass;

	object_class = G_OBJECT_CLASS (klass);
	xqlitemclass = GDA_XQL_ITEM_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	xqlitemclass->add = gda_xql_dual_add;
	xqlitemclass->to_dom = gda_xql_dual_to_dom;
	xqlitemclass->find_id = gda_xql_dual_find_id;
	object_class->finalize = gda_xql_dual_finalize;
}

GdaXqlItem * 
gda_xql_dual_new (gchar *tag, gchar *sqlfmt, gchar *sqlop)
{
	GdaXqlItem *dual;

        dual = g_object_new (GDA_TYPE_XQL_ITEM, NULL);
        gda_xql_item_set_tag(dual,tag);

	return dual;
}

GdaXqlItem * 
gda_xql_dual_new_with_data (gchar *tag, gchar *sqlfmt, gchar *sqlop, GdaXqlItem *left, GdaXqlItem *right)
{
	GdaXqlItem *dual;

	dual = gda_xql_dual_new (tag, sqlfmt, sqlop);
        gda_xql_item_add (dual, left);
        gda_xql_item_add (dual, right);

	return dual;
}

static void 
gda_xql_dual_add (GdaXqlItem *parent, GdaXqlItem *child)
{
	GdaXqlDual *xqldual;
	g_return_if_fail (!(child) || GDA_IS_XQL_ITEM (child));
	
	xqldual = GDA_XQL_DUAL (parent);

        if (xqldual->priv->left == NULL)
        {
	    xqldual->priv->left = child;
        }
        else if (xqldual->priv->right == NULL)
        {
            xqldual->priv->right = child;
        }
        else {
            g_warning("To many children in GdaXqlDual");
            return;
        }

        gda_xql_item_set_parent (child, parent);
}

GdaXqlItem * 
gda_xql_dual_get_left (GdaXqlDual *xqldual)
{
	g_return_val_if_fail (xqldual != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_DUAL (xqldual), NULL);
	
        return xqldual->priv->left;
}

GdaXqlItem *
gda_xql_dual_get_right (GdaXqlDual *xqldual)
{
	g_return_val_if_fail (xqldual != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_DUAL (xqldual), NULL);
	
        return xqldual->priv->right;
}

static xmlNode*
gda_xql_dual_to_dom (GdaXqlItem *xqlitem, xmlNode *parent)
{
	GdaXqlDual *xqldual;
        xmlNode *node;

	xqldual = GDA_XQL_DUAL (xqlitem);

	node = parent_class->to_dom ? parent_class->to_dom (xqlitem, parent) : NULL;

        TO_DOM(xqldual->priv->left, node);
        TO_DOM(xqldual->priv->right, node);

        return node;
}

GdaXqlItem * 
gda_xql_dual_new_eq (void)
{
        return gda_xql_dual_new ("eq", "(%s = %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_eq_with_data (GdaXqlItem *left, GdaXqlItem *right)
{
        return gda_xql_dual_new_with_data ("eq", "(%s = %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_ne (void)
{
        return gda_xql_dual_new ("ne", "(%s <> %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_ne_with_data (GdaXqlItem *left, GdaXqlItem *right)
{
        return gda_xql_dual_new_with_data ("ne", "(%s <> %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_lt (void)
{
        return gda_xql_dual_new ("lt", "(%s < %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_lt_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("lt", "(%s < %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_le (void)
{
	
        return gda_xql_dual_new ("le", "(%s <= %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_le_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("le", "(%s <= %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_gt (void)
{
        return gda_xql_dual_new ("gt", "(%s > %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_gt_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("gt", "(%s > %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_ge (void)
{
        return gda_xql_dual_new ("ge", "(%s >= %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_ge_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("ge", "(%s >= %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_like (void)
{
        return gda_xql_dual_new ("like", "(%s like %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_like_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("like", "(%s like %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_in (void)
{
        return gda_xql_dual_new ("in", "(%s in %s)", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_in_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("in", "(%s in %s)", NULL, left, right);
}

GdaXqlItem * 
gda_xql_dual_new_set (void)
{
        return gda_xql_dual_new ("set", "%s = %s", NULL);
}

GdaXqlItem * 
gda_xql_dual_new_set_with_data (GdaXqlItem * left, GdaXqlItem * right)
{
        return gda_xql_dual_new_with_data ("set", "%s = %s", NULL, left, right);
}

static GdaXqlItem * 
gda_xql_dual_find_id (GdaXqlItem *parent, gchar * id)
{
        GdaXqlItem *item;
	GdaXqlDual *xqldual;

	xqldual = GDA_XQL_DUAL (parent);

        item = parent_class->find_id ? (parent_class->find_id)(parent, id) : NULL;
        if (item != NULL)
            return item;

        item = gda_xql_item_find_id (gda_xql_dual_get_left (xqldual), id);
        if (item != NULL)
            return item;

        return gda_xql_item_find_id (gda_xql_dual_get_right (xqldual), id);
}
