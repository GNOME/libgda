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

#include "gda-xql-select.h"
#include "gda-xql-dml.h"
#include "gda-xql-utils.h"
#include "gda-xql-list.h"
#include "gda-xql-dual.h"
#include "gda-xql-target.h"
#include "gda-xql-field.h"
#include "gda-xql-const.h"
#include "gda-xql-func.h"
#include "gda-xql-value.h"
#include "gda-xql-join.h"
#include "gda-xql-column.h"
#include "gda-xql-item.h"

static void gda_xql_select_init (GdaXqlSelect *xqlsel);
static void gda_xql_select_class_init (GdaXqlSelectClass *klass);
static void gda_xql_select_add (GdaXqlItem *parent, GdaXqlItem *child);
static GdaXqlItem *gda_xql_select_add_value (GdaXqlSelect *xqlsel, GdaXqlItem *valueitem);
static void gda_xql_select_add_group (GdaXqlSelect *xqlsel, GdaXqlItem *group);
static gchar *gda_xql_select_add_target_from_text (GdaXqlDml *parent, gchar *name, GdaXqlItem *join);
static GdaXqlItem *gda_xql_select_add_field_from_text (GdaXqlDml *parent, gchar *id, gchar *name, gchar *alias, gboolean group);
static GdaXqlItem *gda_xql_select_add_const_from_text (GdaXqlDml *parent, gchar *value, gchar *type, gboolean null);
static void gda_xql_select_add_func (GdaXqlDml *parent, GdaXqlItem *func);
static void gda_xql_select_add_group_condition (GdaXqlDml *parent, GdaXqlItem *cond, gchar *type);
static void gda_xql_select_add_order (GdaXqlDml *parent, gint col, gboolean asc);

static GdaXqlDmlClass *parent_class = NULL;

GType
gda_xql_select_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlSelectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_select_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlSelect),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_select_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_DML, "GdaXqlSelect", &info, 0);
	}

	return type;
}

static void 
gda_xql_select_init (GdaXqlSelect *xqlsel)
{

}

static void 
gda_xql_select_class_init (GdaXqlSelectClass *klass)
{
        GdaXqlItemClass *gda_xql_item_class;
	GdaXqlDmlClass *gda_xql_dml_class;

	gda_xql_item_class = GDA_XQL_ITEM_CLASS (klass);
	gda_xql_dml_class = GDA_XQL_DML_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_xql_item_class->add = gda_xql_select_add;
	gda_xql_dml_class->add_target_from_text = gda_xql_select_add_target_from_text;
	gda_xql_dml_class->add_field_from_text = gda_xql_select_add_field_from_text;
	gda_xql_dml_class->add_const_from_text = gda_xql_select_add_const_from_text;
	gda_xql_dml_class->add_func = gda_xql_select_add_func;
	gda_xql_dml_class->add_group_condition = gda_xql_select_add_group_condition;
	gda_xql_dml_class->add_order = gda_xql_select_add_order;
}

GdaXqlItem * 
gda_xql_select_new (void)
{
        GdaXqlItem *select;

	select = g_object_new (GDA_TYPE_XQL_SELECT, NULL);
        gda_xql_item_set_tag (select, "select");

        return select;
}

static void 
gda_xql_select_add (GdaXqlItem *parent, GdaXqlItem *child)
{
        GdaXqlDml *dml;
        gchar *childtag;

	g_return_if_fail (GDA_IS_XQL_ITEM (parent));
	g_return_if_fail (parent != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (child));
	g_return_if_fail (parent != NULL);

        dml = GDA_XQL_DML (parent);
        childtag = gda_xql_item_get_tag (child);

        if (!strcmp (childtag, "targetlist")) {
	    if (dml->priv->target != NULL)
	      g_object_unref (G_OBJECT (dml->priv->target));
            dml->priv->target = child;

        } else if (!strcmp (childtag, "valuelist")) {
	  if (dml->priv->valuelist != NULL)
	    g_object_unref (G_OBJECT (dml->priv->valuelist));
	  dml->priv->valuelist = child;

        } else if (!strcmp (childtag, "where")) {
	  if (dml->priv->where != NULL)
	    g_object_unref (G_OBJECT (dml->priv->where));
	  dml->priv->where = child;

        } else if (!strcmp (childtag, "having")) {
	    if (dml->priv->having != NULL)
	      g_object_unref (G_OBJECT (dml->priv->having));
            dml->priv->having = child;

        } else if (!strcmp(childtag,"group")) {
	    if (dml->priv->group != NULL)
		g_object_unref(G_OBJECT(dml->priv->group));
            dml->priv->group = child;

        } else if (!strcmp (childtag,"union") ||
                   !strcmp (childtag,"unionall") ||
                   !strcmp (childtag,"intersect") ||
                   !strcmp (childtag,"minus") ||
                   !strcmp (childtag,"order")) {
	    if (dml->priv->trailer != NULL)
		g_object_unref (G_OBJECT (dml->priv->trailer));
            dml->priv->trailer = child;
        } else {
	  g_warning ("Invalid objecttype in select\n");
        }

        gda_xql_item_set_parent (child, parent);
}

static GdaXqlItem * 
gda_xql_select_add_value (GdaXqlSelect *xqlsel, GdaXqlItem *valueitem)
{
        GdaXqlItem   *value;
        GdaXqlDml    *dml;
        gchar        *id;

	g_return_val_if_fail (xqlsel != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_SELECT (xqlsel), NULL);

        dml = GDA_XQL_DML (xqlsel);

        if (dml->priv->valuelist == NULL)
            dml->priv->valuelist = gda_xql_list_new_valuelist();

        id = gda_xql_gensym ("v");
        value = gda_xql_value_new_with_data (id);
        g_free(id);
        gda_xql_item_add(value,valueitem);
        gda_xql_item_add(dml->priv->valuelist,value);

        return value;
}

static void 
gda_xql_select_add_group (GdaXqlSelect *xqlsel, GdaXqlItem *group)
{
        GdaXqlDml    *dml;

	g_return_if_fail (xqlsel != NULL);
	g_return_if_fail (GDA_IS_XQL_SELECT (xqlsel));
	
        dml = GDA_XQL_DML (xqlsel);

	if (dml->priv->group == NULL)
            dml->priv->group = gda_xql_list_new_group ();
        gda_xql_item_add (dml->priv->group, group);
}

static gchar * 
gda_xql_select_add_target_from_text (GdaXqlDml *parent, gchar *name, GdaXqlItem *join)
{
        GdaXqlItem   *target;
        gchar          *id;

        id = gda_xql_gensym ("t");
        if (parent->priv->target == NULL)
            parent->priv->target = gda_xql_list_new_targetlist ();
        target = gda_xql_target_new_with_data (id, name, join);
        gda_xql_item_add (parent->priv->target, target);

	return id;
}

static GdaXqlItem * 
gda_xql_select_add_field_from_text (GdaXqlDml *parent, gchar *id, gchar *name, gchar *alias, gboolean group)
{
	GdaXqlItem *field;
	GdaXqlItem *value;

        g_return_val_if_fail (id != NULL, NULL);

        field = gda_xql_field_new_with_data (id, name, alias);
        value = gda_xql_select_add_value ( GDA_XQL_SELECT (parent), field);
	/* strange code! deserves a FIXME! */
	if (group) {

	}

	return value;
}

static GdaXqlItem * 
gda_xql_select_add_const_from_text (GdaXqlDml *parent, gchar *value, gchar *type, gboolean null)
{
        GdaXqlItem   *it_const, *it_value;
        gchar        *null_strg;

        null_strg = (null) ? "yes" : "no";

        it_const  = gda_xql_const_new_with_data (value, NULL, type, null_strg);
        it_value = gda_xql_select_add_value (GDA_XQL_SELECT (parent), it_const);

	return it_value;
}

static void 
gda_xql_select_add_func (GdaXqlDml *parent, GdaXqlItem *func)
{
        gda_xql_select_add_value(GDA_XQL_SELECT (parent), func);
}

static void 
gda_xql_select_add_group_condition (GdaXqlDml *parent, GdaXqlItem *cond, gchar *type)
{
        GdaXqlItem  *having, *list;
        GdaXqlBin   *bin;
        gchar *tag, *fmt;        

        if (parent->priv->having == NULL) {
            parent->priv->having = gda_xql_bin_new_having_with_data (cond);
            return;
        }

        bin   = GDA_XQL_BIN (parent->priv->having);
        having = gda_xql_bin_get_child (bin);
        tag   = gda_xql_item_get_tag (GDA_XQL_ITEM (having));

        if (!strcmp (tag, type))
	  gda_xql_item_add (having, cond);
        else {
	  fmt = g_strdup_printf (" %s ", type);
	  list = gda_xql_list_new (type);
	  g_free (fmt);
	  gda_xql_item_add (list, having);
	  gda_xql_item_add (list, cond);
	  g_object_ref (G_OBJECT (having));
	  gda_xql_item_add (GDA_XQL_ITEM (bin), list);
	  g_object_unref (G_OBJECT (having));
        }
}

static void 
gda_xql_select_add_order (GdaXqlDml * parent, gint col, gboolean asc)
{
        GdaXqlDml    *dml;
        GdaXqlItem   *column;

        dml = GDA_XQL_DML (parent);

        if (dml->priv->trailer == NULL)
            dml->priv->trailer = gda_xql_list_new_order ();

        if (!GDA_IS_XQL_LIST (dml->priv->trailer))
	  return;

        column = gda_xql_column_new_with_data (col, asc);
        gda_xql_item_add (dml->priv->trailer, column);
}
