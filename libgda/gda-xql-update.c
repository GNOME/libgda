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

#include "gda-xql-update.h"
#include "gda-xql-dml.h"
#include "gda-xql-utils.h"
#include "gda-xql-atom.h"
#include "gda-xql-func.h"
#include "gda-xql-field.h"
#include "gda-xql-value.h"
#include "gda-xql-const.h"
#include "gda-xql-dual.h"
#include "gda-xql-list.h"

static void gda_xql_update_init (GdaXqlUpdate *xqlupd);
static void gda_xql_update_class_init (GdaXqlUpdateClass *klass);
static void gda_xql_update_add (GdaXqlItem *parent, GdaXqlItem *child);
static void gda_xql_update_add_set (GdaXqlDml *parent, GdaXqlItem *set);
static void gda_xql_update_add_set_const (GdaXqlDml *parent, gchar *fname, gchar *value, gchar *type, gboolean null);

static GdaXqlDmlClass *parent_class = NULL;

GType
gda_xql_update_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlUpdateClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_update_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlUpdate),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_update_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_DML, "GdaXqlUpdate", &info, 0);
	}

	return type;
}

static void 
gda_xql_update_init (GdaXqlUpdate *xqlupd)
{

}

static void 
gda_xql_update_class_init (GdaXqlUpdateClass *klass)
{
	GdaXqlItemClass *gda_xql_item_class;
	GdaXqlDmlClass *gda_xql_dml_class;

	gda_xql_item_class = GDA_XQL_ITEM_CLASS (klass);
	gda_xql_dml_class = GDA_XQL_DML_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_xql_item_class->add = gda_xql_update_add;
	gda_xql_dml_class->add_set = gda_xql_update_add_set;
	gda_xql_dml_class->add_set_const = gda_xql_update_add_set_const;
}

GdaXqlItem * 
gda_xql_update_new (void)
{
        GdaXqlItem *update;

        update = g_object_new (GDA_TYPE_XQL_UPDATE, NULL);
        gda_xql_item_set_tag (update, "update");

        return update;
}

static void 
gda_xql_update_add (GdaXqlItem *parent, GdaXqlItem *child)
{
        GdaXqlDml *dml;
        gchar *childtag;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (parent));
	g_return_if_fail (child != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (child));	

        dml = GDA_XQL_DML (parent);
        childtag = gda_xql_item_get_tag (child);

        if (!strcmp (childtag, "target")) {
	    if (dml->priv->target != NULL)
	      g_object_unref (G_OBJECT (dml->priv->target));
            dml->priv->target = child;
        } else if (!strcmp (childtag, "setlist")) {
	    if (dml->priv->setlist != NULL)
		g_object_unref (G_OBJECT (dml->priv->setlist));
            dml->priv->setlist = child;
        } else if (!strcmp (childtag, "where")) {
	    if (dml->priv->where != NULL)
		g_object_unref (G_OBJECT (dml->priv->where));
            dml->priv->where = child;
        } else {
            g_warning ("Invalid objecttype in update\n");
            return;
	}

        gda_xql_item_set_parent (child, parent);
}

static void 
gda_xql_update_add_set (GdaXqlDml *parent, GdaXqlItem *set)
{
        GdaXqlItem *item;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (GDA_IS_XQL_DML (parent));
	g_return_if_fail (set != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (set));
	
        item = GDA_XQL_ITEM (parent);        

        if (parent->priv->setlist == NULL)
            parent->priv->setlist = gda_xql_list_new_setlist ();

        gda_xql_item_set_parent (parent->priv->setlist, item);
        gda_xql_item_add (parent->priv->setlist, set);
}

static void 
gda_xql_update_add_set_const (GdaXqlDml *parent, gchar *fname, gchar *value, gchar *type, gboolean null)
{
        GdaXqlItem *field;
	GdaXqlItem *value_it;
	GdaXqlItem *set;
	GdaXqlItem *item;

        item = GDA_XQL_ITEM (parent);

        if (parent->priv->setlist == NULL)
            parent->priv->setlist = gda_xql_list_new_setlist ();

        gda_xql_item_set_parent (parent->priv->setlist, item);

        field = gda_xql_field_new_with_data (NULL, fname, NULL);
        value_it = gda_xql_const_new_with_data (value, NULL, type, null ? "yes" : "no");
        set = gda_xql_dual_new_set_with_data (field, value_it);

        gda_xql_item_add (parent->priv->setlist, set);
}
