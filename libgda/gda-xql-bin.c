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

#include <libgda/gda-xql-bin.h>

struct _GdaXqlBinPrivate {
	GdaXqlItem *child;
};

static void gda_xql_bin_init (GdaXqlBin *xqlbin);
static void gda_xql_bin_class_init (GdaXqlBinClass *klass);
static void gda_xql_bin_add (GdaXqlItem *xqlitem, GdaXqlItem *child);
static xmlNode *gda_xql_bin_to_dom (GdaXqlItem *xqlitem, xmlNode *parent);
static GdaXqlItem *gda_xql_bin_find_id (GdaXqlItem *xqlitem, gchar *id);

static GdaXqlItemClass *parent_class = NULL;

GType
gda_xql_bin_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlBinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_bin_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlBin),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_bin_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_ITEM, "GdaXqlBin", &info, 0);
	}

	return type;
}

static void
gda_xql_bin_finalize(GObject *object)
{
	GObjectClass *klass;
  	GdaXqlBin *xqlbin;

	g_return_if_fail (G_IS_OBJECT (object));

	klass = G_OBJECT_GET_CLASS (object);
	xqlbin = GDA_XQL_BIN (object);

	if (klass->finalize)
	  klass->finalize (object);

	if (xqlbin->priv->child) {
	  g_object_unref (xqlbin->priv->child);
	  xqlbin->priv->child = NULL;
	}
	g_free (xqlbin->priv);
}

static void 
gda_xql_bin_init (GdaXqlBin *xqlbin)
{
	xqlbin->priv = g_new0 (GdaXqlBinPrivate, 1);
	xqlbin->priv->child = NULL;
}

static void 
gda_xql_bin_class_init (GdaXqlBinClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaXqlItemClass *gda_xql_item_class = GDA_XQL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_xql_item_class->add = gda_xql_bin_add;
	gda_xql_item_class->to_dom = gda_xql_bin_to_dom;
	gda_xql_item_class->find_id = gda_xql_bin_find_id;
	object_class->finalize = gda_xql_bin_finalize;
}

GdaXqlItem * 
gda_xql_bin_new (gchar *tag, gchar *sqlfmt, gchar *sqlop)
{
	GdaXqlItem *bin;

	bin = g_object_new (GDA_TYPE_XQL_BIN, NULL);

        gda_xql_item_set_tag (bin, tag);

	return bin;
}

GdaXqlItem * 
gda_xql_bin_new_with_data (gchar *tag, gchar *sqlfmt, gchar *sqlop, GdaXqlItem *child)
{
	GdaXqlItem *bin;

	bin = gda_xql_bin_new(tag,sqlfmt,sqlop);
        gda_xql_item_add(bin,child);

	return bin;
}

static void 
gda_xql_bin_add (GdaXqlItem *xqlitem, GdaXqlItem *child)
{
	GdaXqlBin *xqlbin = GDA_XQL_BIN (xqlitem);

	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
	g_return_if_fail (child != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));

        if (xqlbin->priv->child != NULL)
	    g_object_unref (G_OBJECT(xqlbin->priv->child));
        xqlbin->priv->child = child;
 
        gda_xql_item_set_parent (child, xqlitem);
}


GdaXqlItem * 
gda_xql_bin_get_child (GdaXqlBin *xqlbin)
{
	g_return_val_if_fail (xqlbin != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_BIN (xqlbin), NULL);

        return xqlbin->priv->child;
}

void 
gda_xql_bin_set_child (GdaXqlBin *xqlbin, GdaXqlItem *xqlitem)
{
	g_return_if_fail (xqlbin != NULL);
	g_return_if_fail (GDA_IS_XQL_BIN (xqlbin));
	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));

	if (xqlbin->priv->child != NULL)
            g_object_unref (G_OBJECT (xqlbin->priv->child));
        xqlbin->priv->child = xqlitem;
}

static xmlNode * 
gda_xql_bin_to_dom (GdaXqlItem *xqlitem, xmlNode *parnode)
{
	GdaXqlBin *xqlbin = GDA_XQL_BIN (xqlitem);
        xmlNode *node;

        node = (parent_class->to_dom ? (parent_class->to_dom) (xqlitem, parnode) : NULL);
        TO_DOM(xqlbin->priv->child, node);

        return node;
}

GdaXqlItem * 
gda_xql_bin_new_union (void)
{
        return gda_xql_bin_new ("union", "union %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_unionall (void)
{
        return gda_xql_bin_new ("unionall", "unionall %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_intersect (void)
{
        return gda_xql_bin_new ("intersect", "intersect %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_minus (void)
{
	
        return gda_xql_bin_new ("minus", "minus %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_where (void)
{	
        return gda_xql_bin_new ("where", "where %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_where_with_data (GdaXqlItem *data)
{
        GdaXqlItem *where;

        where = gda_xql_bin_new ("where", "where %s", NULL);
        gda_xql_item_add (where, data);

	return where;
}

GdaXqlItem * 
gda_xql_bin_new_having (void)
{	
        return gda_xql_bin_new ("having", "having %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_having_with_data (GdaXqlItem *data)
{
	
        GdaXqlItem *having;

        having = gda_xql_bin_new ("having", "having %s", NULL);
        gda_xql_item_add (having, data);

        return having;
}

GdaXqlItem * 
gda_xql_bin_new_on (void)
{
        return gda_xql_bin_new ("on", "on %s", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_not (void)
{
        return gda_xql_bin_new ("not", "(not %s)", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_not_with_data (GdaXqlItem * data)
{
        GdaXqlItem *not;

        not = gda_xql_bin_new ("not", "(not %s)", NULL);
        gda_xql_item_add (not, data);

        return not;
}

GdaXqlItem * 
gda_xql_bin_new_exists (void)
{
        return gda_xql_bin_new ("exists", "(exists %s)", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_null (void)
{
	
        return gda_xql_bin_new("null", "(%s is null)", NULL);
}

GdaXqlItem * 
gda_xql_bin_new_null_with_data (GdaXqlItem * data)
{
        GdaXqlItem *null;

        null = gda_xql_bin_new ("null", "(%s is null)", NULL);
        gda_xql_item_add (null, data);
        return null;
}

static GdaXqlItem * 
gda_xql_bin_find_id (GdaXqlItem *xqlitem, gchar *id)
{
        GdaXqlItem *item;

	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	item = (parent_class->find_id ? (parent_class->find_id) (xqlitem, id) : NULL);
        if (item != NULL)
            return item;
        return gda_xql_item_find_id (gda_xql_bin_get_child(GDA_XQL_BIN(xqlitem)),id);
}
