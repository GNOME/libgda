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

#include "gda-xql-insert.h"
#include "gda-xql-dml.h"
#include "gda-xql-utils.h"
#include "gda-xql-atom.h"
#include "gda-xql-list.h"
#include "gda-xql-func.h"
#include "gda-xql-field.h"
#include "gda-xql-value.h"
#include "gda-xql-const.h"

static void gda_xql_insert_init (GdaXqlInsert *xqlins);
static void gda_xql_insert_class_init (GdaXqlInsertClass *klass);
static void gda_xql_insert_add (GdaXqlItem * parent, GdaXqlItem * child);
static GdaXqlItem * gda_xql_insert_add_field_from_text (GdaXqlDml * parent, gchar * id, gchar * name, gchar * alias, gboolean group);
static GdaXqlItem * gda_xql_insert_add_const_from_text (GdaXqlDml * parent, gchar * value, gchar * type, gboolean null);
static void gda_xql_insert_add_func (GdaXqlDml * parent, GdaXqlItem * func);
static void gda_xql_insert_add_query (GdaXqlDml * parent, GdaXqlItem * query);

static GdaXqlDmlClass *parent_class = NULL;

GType
gda_xql_insert_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlInsertClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_insert_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlInsert),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_insert_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_DML, "GdaXqlInsert", &info, 0);
	}

	return type;
}

static void 
gda_xql_insert_init (GdaXqlInsert *xqlins)
{

}

static void 
gda_xql_insert_class_init (GdaXqlInsertClass *klass)
{
	GdaXqlDmlClass *gda_xql_dml_class;
	GdaXqlItemClass *gda_xql_item_class;

	gda_xql_dml_class = GDA_XQL_DML_CLASS (klass);
	gda_xql_item_class = GDA_XQL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_xql_item_class->add = gda_xql_insert_add;
	gda_xql_dml_class->add_field_from_text = gda_xql_insert_add_field_from_text;
	gda_xql_dml_class->add_const_from_text = gda_xql_insert_add_const_from_text;
	gda_xql_dml_class->add_func = gda_xql_insert_add_func;
	gda_xql_dml_class->add_query = gda_xql_insert_add_query;
}

GdaXqlItem * 
gda_xql_insert_new (void)
{
        GdaXqlItem *insert;

	insert = g_object_new (GDA_TYPE_XQL_INSERT, NULL);
        gda_xql_item_set_tag (insert, "insert");

        return insert;
}

static void 
gda_xql_insert_add (GdaXqlItem *parent, GdaXqlItem *child)
{
        GdaXqlDml *xqldml;
        gchar *childtag;

	g_return_if_fail (child != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (child));
	
        xqldml = GDA_XQL_DML (parent);
        childtag = gda_xql_item_get_tag (child);

        if (!strcmp (childtag, "target")) {
	    if (xqldml->priv->target != NULL)
		g_object_unref (G_OBJECT(xqldml->priv->target));
            xqldml->priv->target = child;
        } else if (!strcmp (childtag, "dest")) {
	    if (xqldml->priv->dest != NULL)
		g_object_unref (G_OBJECT (xqldml->priv->dest));
            xqldml->priv->dest = child;
        } else if (!strcmp (childtag, "sourcelist")) {
	    if (xqldml->priv->source != NULL)
		g_object_unref (G_OBJECT(xqldml->priv->source));
            xqldml->priv->source = child;
        } else {
            g_warning("Invalid objecttype in insert\n");
            return;
	}

        gda_xql_item_set_parent (child, parent);
}

static GdaXqlItem * 
gda_xql_insert_add_field_from_text (GdaXqlDml *parent, gchar *id, gchar *name, gchar *alias, gboolean group)
{
        GdaXqlDml   *xqldml;
	GdaXqlItem  *field;

        xqldml = GDA_XQL_DML (parent);

         if (xqldml->priv->dest == NULL)
             xqldml->priv->dest = gda_xql_list_new_dest();

         field = gda_xql_field_new_with_data (id, name, alias);
         gda_xql_item_add (xqldml->priv->dest, field);
	 return field;
}

static GdaXqlItem * 
gda_xql_insert_add_const_from_text (GdaXqlDml *parent, gchar *value, gchar *type, gboolean null)
{
        GdaXqlDml   *xqldml;
        GdaXqlItem  *it_const;

        xqldml = GDA_XQL_DML (parent);

        if (xqldml->priv->source == NULL)
            xqldml->priv->source = gda_xql_list_new_sourcelist ();

        it_const  = gda_xql_const_new_with_data (value, NULL, type,
						 null ? "yes" : "no");

        gda_xql_item_add (xqldml->priv->source, it_const);

        return it_const;
}

static void 
gda_xql_insert_add_func (GdaXqlDml *parent, GdaXqlItem *func)
{
        GdaXqlDml *xqldml;

	g_return_if_fail (func != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (func));
	
        xqldml = GDA_XQL_DML (parent);

        if (xqldml->priv->source == NULL)
	  xqldml->priv->source = gda_xql_list_new_sourcelist ();

        gda_xql_item_add (xqldml->priv->source, func);
}

static void 
gda_xql_insert_add_query (GdaXqlDml *parent, GdaXqlItem *query)
{
        GdaXqlDml *xqldml;

	g_return_if_fail (query != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (query));
	
        xqldml = GDA_XQL_DML (parent);

        if (xqldml->priv->source != NULL)
            return;

        xqldml->priv->source = query;
}
