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

#include "gda-xql-item.h"
#include "gda-xql-utils.h"

static void
attr_to_dom(gchar *key, gchar *value, xmlNode *node)
{
    gda_xql_new_attr (key, value, node);
}

static void
destroy_hash_table (GHashTable *hashtab)
{
  g_hash_table_foreach_remove(hashtab,
			      (GHRFunc) gda_xql_destroy_hash_pair,
			      g_free);
  g_hash_table_destroy(hashtab);
}

struct _GdaXqlItemPrivate {
	gchar *tag;
	GHashTable *attrlist;
	GHashTable *idlist;
	GHashTable *reflist;
	GdaXqlItem *parent;
};

static void gda_xql_item_init (GdaXqlItem *xqlitem);
static void gda_xql_item_class_init (GdaXqlItemClass *klass);

static GObjectClass *parent_class = NULL;

GType
gda_xql_item_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_item_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlItem),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_item_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, "GdaXqlItem", &info, (GTypeFlags)0);
	}

	return type;
}

static void
gda_xql_item_finalize(GObject *object)
{
	GdaXqlItem *xqlitem = GDA_XQL_ITEM (object);

	if(G_OBJECT_CLASS(parent_class)->finalize) \
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);

	if(xqlitem->priv->tag) {
	  g_free (xqlitem->priv->tag);
	  xqlitem->priv->tag = NULL;
	}
	if(xqlitem->priv->attrlist) {
	  destroy_hash_table (xqlitem->priv->attrlist);
	  xqlitem->priv->attrlist = NULL;
	}
	if(xqlitem->priv->idlist) {
	  destroy_hash_table (xqlitem->priv->idlist);
	  xqlitem->priv->idlist = NULL;
	}
	if(xqlitem->priv->reflist) {
	  destroy_hash_table (xqlitem->priv->reflist);
	  xqlitem->priv->reflist = NULL;
	}
	if(xqlitem->priv->parent) {
	  g_object_unref (xqlitem->priv->parent);
	  xqlitem->priv->reflist = NULL;
	}
	g_free (xqlitem->priv);
}

static void 
gda_xql_item_class_init (GdaXqlItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	klass->to_dom = gda_xql_item_to_dom;
	klass->add = NULL;
	klass->find_id = gda_xql_item_find_id;
	klass->find_ref = gda_xql_item_find_ref;
	object_class->finalize = gda_xql_item_finalize;
}


static void 
gda_xql_item_init (GdaXqlItem *xqlitem)
{
	xqlitem->priv = g_new0 (GdaXqlItemPrivate, 1);
	xqlitem->priv->tag = NULL;
	xqlitem->priv->attrlist = NULL;
	xqlitem->priv->idlist = NULL;
	xqlitem->priv->reflist = NULL;
	xqlitem->priv->parent = NULL;

        xqlitem->priv->attrlist = g_hash_table_new(g_str_hash,g_str_equal);
}

void 
gda_xql_item_set_attrib (GdaXqlItem *xqlitem, gchar *attrib, gchar *value)
{
        gchar *oldval, *oldkey;

	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
	
        if (g_hash_table_lookup_extended(xqlitem->priv->attrlist,
					 attrib,
					 (gpointer *)&oldkey,
					 (gpointer *)&oldval))
        {
            g_hash_table_remove(xqlitem->priv->attrlist, attrib);
            g_free(oldval);
            g_free(oldkey);
        }
        g_hash_table_insert(xqlitem->priv->attrlist,
                            g_strdup(attrib), 
			    g_strdup(value));
}

GdaXqlItem * 
gda_xql_item_get_parent (GdaXqlItem *xqlitem)
{
	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);

        return xqlitem->priv->parent;
}

void 
gda_xql_item_set_parent (GdaXqlItem *xqlitem, GdaXqlItem *parent)
{
	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));

        if (xqlitem->priv->parent != NULL)
            g_object_unref(G_OBJECT(xqlitem->priv->parent));
        xqlitem->priv->parent = parent;
        g_object_ref(G_OBJECT(parent));
}

gchar * 
gda_xql_item_get_attrib (GdaXqlItem *xqlitem, gchar *attrib)
{
        gchar *value;

	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);

        value = (gchar *) g_hash_table_lookup (xqlitem->priv->attrlist, 
					       attrib);
        if (value == NULL)
            value = "";
        return value;
}

xmlNode * 
gda_xql_item_to_dom (GdaXqlItem *xqlitem, xmlNode *parent)
{
	GdaXqlItemClass *klass;
	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	klass = GDA_XQL_ITEM_GET_CLASS(xqlitem);

	if(klass->to_dom)
		return (*klass->to_dom)(xqlitem, parent);
	else
	        return NULL;
}

void 
gda_xql_item_add (GdaXqlItem *xqlitem, GdaXqlItem *child)
{
	GdaXqlItemClass *klass;
	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
	klass = GDA_XQL_ITEM_GET_CLASS(xqlitem);

	if(klass->add)
		(*klass->add) (xqlitem, child);
}

void
gda_xql_item_set_tag (GdaXqlItem *xqlitem, gchar * tag)
{
	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
	
        g_free(xqlitem->priv->tag);
        xqlitem->priv->tag = g_strdup(tag);
}

gchar * 
gda_xql_item_get_tag (GdaXqlItem *xqlitem)
{
	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);

        return xqlitem->priv->tag;
}

GdaXqlItem *
gda_xql_item_find_root (GdaXqlItem *xqlitem)
{
        GdaXqlItem *item;

	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	
        item = xqlitem->priv->parent;
        if (item == NULL)
            return xqlitem;
        return gda_xql_item_find_root(item);
}

GdaXqlItem * 
gda_xql_item_find_id (GdaXqlItem *xqlitem, gchar *id)
{
	GdaXqlItemClass *klass;
	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	klass = GDA_XQL_ITEM_GET_CLASS(xqlitem);

	if(klass->find_id)
		return (*klass->find_id) (xqlitem, id);
	else
		return NULL;
}

GdaXqlItem * 
gda_xql_item_find_ref (GdaXqlItem *xqlitem, gchar *ref)
{
	GdaXqlItemClass *klass;
	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	klass = GDA_XQL_ITEM_GET_CLASS(xqlitem);

	if(klass->find_ref)
		return (*klass->find_ref) (xqlitem, ref);
	else
		return NULL;
}

void 
gda_xql_item_add_id (GdaXqlItem *xqlitem, gchar *id)
{
        GdaXqlItem *root;

	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
	g_return_if_fail (id != NULL);

        root = gda_xql_item_find_root (xqlitem);
        if (root->priv->idlist == NULL)
            root->priv->idlist = g_hash_table_new (g_str_hash,g_str_equal);
        g_hash_table_insert (root->priv->idlist, g_strdup (id), xqlitem);
}

void 
gda_xql_item_add_ref (GdaXqlItem *xqlitem, gchar *ref)
{
        GdaXqlItem *root, *item;

	g_return_if_fail (xqlitem != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (xqlitem));
	g_return_if_fail (ref != NULL);
	
        root = gda_xql_item_find_root (xqlitem);
        item = gda_xql_item_find_id (root, ref);
        if (item == NULL) {
            g_warning("Item with id `%s' not found\n", ref);
            return;
        }
        if (xqlitem->priv->reflist == NULL)
            xqlitem->priv->reflist = g_hash_table_new (g_str_hash,g_str_equal);
        g_hash_table_insert(xqlitem->priv->reflist, g_strdup (ref), item);
        g_object_ref (G_OBJECT (item));
}

GdaXqlItem * 
gda_xql_item_get_ref (GdaXqlItem *xqlitem, gchar *ref)
{
	GdaXqlItem *item;

	g_return_val_if_fail (xqlitem != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_ITEM (xqlitem), NULL);
	g_return_val_if_fail (ref != NULL, NULL);
	
        if (xqlitem->priv->reflist == NULL)
            item = NULL;
	else
	    item = g_hash_table_lookup (xqlitem->priv->reflist, ref);        

        return item;
}


