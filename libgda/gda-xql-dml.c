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

#include "gda-xql-dml.h"
#include "gda-xql-bin.h"
#include "gda-xql-list.h"
#include "gda-xql-atom.h"
#include "gda-xql-target.h"
#include "gda-xql-join.h"

/* here are local prototypes */
static void gda_xql_dml_init (GdaXqlDml *xqldml);
static void gda_xql_dml_class_init (GdaXqlDmlClass *klass);
static gchar *klass_gda_xql_dml_add_target_from_text (GdaXqlDml *self, gchar *name, GdaXqlItem *join);
static void  klass_gda_xql_dml_add_row_condition (GdaXqlDml *self, GdaXqlItem *cond, gchar *type);
static xmlNode *gda_xql_dml_to_dom (GdaXqlItem *parent, xmlNode *parNode);
static GdaXqlItem *klass_gda_xql_dml_find_id (GdaXqlItem *parent, gchar *id);

/* pointer to the class of our parent */
static GdaXqlItemClass *parent_class = NULL;

GType
gda_xql_dml_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlDmlClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_dml_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlDml),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_dml_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_ITEM, "GdaXqlDml", &info, 0);
	}

	return type;
}

static void
gda_xql_dml_finalize (GObject *object)
{
	GdaXqlDml *xqldml = GDA_XQL_DML (object);

	if (G_OBJECT_CLASS(parent_class)->finalize)
	  (G_OBJECT_CLASS(parent_class)->finalize)(object);

	if (xqldml->priv->target) {
	  g_object_unref (xqldml->priv->target);
	  xqldml->priv->target = NULL;
	}
	if (xqldml->priv->valuelist) {
	  g_object_unref (xqldml->priv->valuelist);
	  xqldml->priv->valuelist = NULL;
	}
	if (xqldml->priv->where) {
	  g_object_unref (xqldml->priv->where);
	  xqldml->priv->where = NULL;
	}
	if (xqldml->priv->having) {
	  g_object_unref (xqldml->priv->having);
	  xqldml->priv->having = NULL;
	}
	if (xqldml->priv->group) {
	  g_object_unref (xqldml->priv->group);
	  xqldml->priv->group = NULL;
	}
	if (xqldml->priv->trailer) {
	  g_object_unref (xqldml->priv->trailer);
	  xqldml->priv->trailer = NULL;
	}
	if (xqldml->priv->dest) {
	  g_object_unref (xqldml->priv->dest);
	  xqldml->priv->dest = NULL;
	}
	if (xqldml->priv->source) {
	  g_object_unref (xqldml->priv->source);
	  xqldml->priv->source = NULL;
	}
	if (xqldml->priv->setlist) {
	  g_object_unref (xqldml->priv->setlist);
	  xqldml->priv->setlist = NULL;
	}

	g_free (xqldml->priv);
	xqldml->priv = NULL;
}


static void 
gda_xql_dml_init (GdaXqlDml *xqldml)
{
	xqldml->priv = g_new0 (GdaXqlDmlPrivate, 1);
	xqldml->priv->target = NULL;
	xqldml->priv->valuelist = NULL;
	xqldml->priv->where = NULL;
	xqldml->priv->having = NULL;
	xqldml->priv->group = NULL;
	xqldml->priv->trailer = NULL;
	xqldml->priv->dest = NULL;
	xqldml->priv->source = NULL;
	xqldml->priv->setlist = NULL;
}

static void 
gda_xql_dml_class_init (GdaXqlDmlClass *klass)
{
	GObjectClass *object_class;
	GdaXqlItemClass *gda_xql_item_class;

	gda_xql_item_class = GDA_XQL_ITEM_CLASS (klass);
	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	klass->add_target_from_text = klass_gda_xql_dml_add_target_from_text;
	klass->add_field_from_text = NULL;
	klass->add_const_from_text = NULL;
	klass->add_func = NULL;
	klass->add_query = NULL;
	klass->add_row_condition = klass_gda_xql_dml_add_row_condition;
	gda_xql_item_class->to_dom = gda_xql_dml_to_dom;
	klass->add_group_condition = NULL;
	klass->add_order = NULL;
	klass->add_set = NULL;
	klass->add_set_const = NULL;
	gda_xql_item_class->find_id = klass_gda_xql_dml_find_id;
	object_class->finalize = gda_xql_dml_finalize;
}


gchar * 
gda_xql_dml_add_target_from_text (GdaXqlDml *xqldml, gchar *name, GdaXqlItem *join)
{
	GdaXqlDmlClass *klass;

	g_return_val_if_fail (xqldml != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_DML (xqldml), NULL);

	klass = GDA_XQL_DML_GET_CLASS (xqldml);

	if (klass->add_target_from_text)
		return (*klass->add_target_from_text)(xqldml, name, join);
	else
	        return NULL;
}

static gchar * 
klass_gda_xql_dml_add_target_from_text (GdaXqlDml * self, gchar * name, GdaXqlItem * join)
{
        GdaXqlItem   *table;

        if (self->priv->target != NULL) {
            g_warning("multible targets in %s",
                      gda_xql_item_get_tag(GDA_XQL_ITEM(self)));
            return NULL;
        }
        table = gda_xql_target_new_with_data (NULL,name,join);
        self->priv->target = table;
        return NULL;
}

GdaXqlItem * 
gda_xql_dml_add_field_from_text (GdaXqlDml * self, gchar * id, gchar * name, gchar * alias, gboolean group)
{
	GdaXqlDmlClass *klass;
	g_return_val_if_fail (self != NULL, (GdaXqlItem * )0);
	g_return_val_if_fail (GDA_IS_XQL_DML (self), (GdaXqlItem * )0);
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_field_from_text)
		return (*klass->add_field_from_text)(self,id,name,alias,group);
	else
		return (GdaXqlItem * )(0);
}

GdaXqlItem * 
gda_xql_dml_add_const_from_text (GdaXqlDml * self, gchar * value, gchar * type, gboolean null)
{
	GdaXqlDmlClass *klass;
	g_return_val_if_fail (self != NULL, (GdaXqlItem * )0);
	g_return_val_if_fail (GDA_IS_XQL_DML (self), (GdaXqlItem * )0);
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_const_from_text)
		return (*klass->add_const_from_text)(self,value,type,null);
	else
		return (GdaXqlItem * )(0);
}

void 
gda_xql_dml_add_func (GdaXqlDml * self, GdaXqlItem * item)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_func)
		(*klass->add_func)(self,item);
}

void 
gda_xql_dml_add_query (GdaXqlDml * self, GdaXqlItem * item)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_query)
		(*klass->add_query)(self,item);
}

void 
gda_xql_dml_add_row_condition (GdaXqlDml * self, GdaXqlItem * cond, gchar * type)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_row_condition)
		(*klass->add_row_condition)(self,cond,type);
}
static void 
klass_gda_xql_dml_add_row_condition (GdaXqlDml *xqldml, GdaXqlItem *cond, gchar *type)
{
        GdaXqlItem  *where, *list;
        GdaXqlBin   *bin;
        gchar *tag, *fmt;        

        if (xqldml->priv->where == NULL) {
            xqldml->priv->where = gda_xql_bin_new_where_with_data(cond);
            return;
        }
        bin   = GDA_XQL_BIN(xqldml->priv->where);
        where = gda_xql_bin_get_child(bin);
        tag   = gda_xql_item_get_tag(GDA_XQL_ITEM(where));

        if (!strcmp(tag,type))
            gda_xql_item_add(where,cond);
        else {
            fmt = g_strdup_printf(" %s ",type);
            list = gda_xql_list_new(type);
            g_free(fmt);
            gda_xql_item_add(list,where);
            gda_xql_item_add(list,cond);
            g_object_ref(G_OBJECT(where));
            gda_xql_item_add(GDA_XQL_ITEM(bin),list);
	}
}

static xmlNode * 
gda_xql_dml_to_dom (GdaXqlItem *xqlitem, xmlNode *parent)
{
	GdaXqlDml *xqldml;
        xmlNode *node;

	xqldml = GDA_XQL_DML (parent);

        node = parent_class->to_dom ? parent_class->to_dom (xqlitem, parent) : NULL;
        TO_DOM(xqldml->priv->target, node);
        TO_DOM(xqldml->priv->valuelist, node);
        TO_DOM(xqldml->priv->setlist, node);
        TO_DOM(xqldml->priv->where, node);
        TO_DOM(xqldml->priv->having, node);
        TO_DOM(xqldml->priv->group, node);
        TO_DOM(xqldml->priv->trailer, node);
        TO_DOM(xqldml->priv->dest, node);
        TO_DOM(xqldml->priv->source, node);

        return node;
}

void 
gda_xql_dml_add_group_condition (GdaXqlDml *self, GdaXqlItem *cond, gchar *type)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_group_condition)
		(*klass->add_group_condition)(self,cond,type);
}

void 
gda_xql_dml_add_order (GdaXqlDml *self, gint column, gboolean asc)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_order)
		(*klass->add_order)(self,column,asc);
}

void 
gda_xql_dml_add_set (GdaXqlDml * self, GdaXqlItem * item)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_set)
		(*klass->add_set)(self,item);
}

void 
gda_xql_dml_add_set_const (GdaXqlDml * self, gchar * field, gchar * value, gchar * type, gboolean null)
{
	GdaXqlDmlClass *klass;
	g_return_if_fail (self != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (self));
	klass = GDA_XQL_DML_GET_CLASS(self);

	if(klass->add_set_const)
		(*klass->add_set_const)(self,field,value,type,null);
}

static GdaXqlItem * 
klass_gda_xql_dml_find_id (GdaXqlItem *parent, gchar *id)
{	
        GdaXqlItem *item;
	GdaXqlDml *xqldml;

	xqldml = GDA_XQL_DML (parent);

	item = parent_class->find_id ? parent_class->find_id (parent, id) : NULL;

        if (item != NULL)
            return item;

        if (xqldml->priv->target != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->target,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->valuelist != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->valuelist,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->where != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->where,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->having != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->having,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->group != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->group,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->trailer != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->trailer,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->dest != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->dest,id);
            if (item != NULL)
                return item;
        }
  
        if (xqldml->priv->source != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->source,id);
            if (item != NULL)
                return item;
        }

        if (xqldml->priv->setlist != NULL) {
            item = gda_xql_item_find_id(xqldml->priv->setlist,id);
            if (item != NULL)
                return item;
        }

        return NULL;
}
