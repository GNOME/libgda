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

#include "gda-xql-stack.h"
#include "gda-xql-utils.h"

static void
destroy_object_list(GSList *list);


struct _GdaXqlStackPrivate {
	GSList *list;
};

static void gda_xql_stack_init (GdaXqlStack *xqlstack);
static void gda_xql_stack_class_init (GdaXqlStackClass *klass);

static GObjectClass *parent_class = NULL;

GType
gda_xql_stack_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlStackClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_stack_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlStack),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_stack_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, "GdaXqlStack", &info, 0);
	}

	return type;
}


static void
gda_xql_stack_finalize(GObject *object)
{
        GdaXqlStack *xqlstack;

	xqlstack = GDA_XQL_STACK (object);

	if(G_OBJECT_CLASS(parent_class)->finalize) \
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);
	if(xqlstack->priv->list) {
	  destroy_object_list (xqlstack->priv->list);
	  xqlstack->priv->list = NULL;
	}
	g_free (xqlstack->priv);
}

static void 
gda_xql_stack_init (GdaXqlStack *xqlstack)
{
        xqlstack->priv = g_new0 (GdaXqlStackPrivate, 1);
	xqlstack->priv->list = NULL;
}

static void 
gda_xql_stack_class_init (GdaXqlStackClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	g_object_class->finalize = gda_xql_stack_finalize;
}


GdaXqlStack * 
gda_xql_stack_new (void)
{
        return g_object_new (GDA_TYPE_XQL_STACK, NULL);
}

void 
gda_xql_stack_push (GdaXqlStack *xqlstack, GdaXqlItem *item)
{
	g_return_if_fail (xqlstack != NULL);
	g_return_if_fail (GDA_IS_XQL_STACK (xqlstack));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GDA_IS_XQL_ITEM (item));
	
        g_object_ref (G_OBJECT (item));
        xqlstack->priv->list = g_slist_prepend (xqlstack->priv->list, item);
}

GdaXqlItem * 
gda_xql_stack_pop (GdaXqlStack * xqlstack)
{
        GSList     *list;
        GdaXqlItem *item;

	g_return_val_if_fail (xqlstack != NULL, NULL);
	g_return_val_if_fail (GDA_IS_XQL_STACK (xqlstack), NULL);

        list = xqlstack->priv->list;
        g_return_val_if_fail (list != NULL, NULL);
        item = list->data;
        g_object_unref (G_OBJECT(item));
        xqlstack->priv->list = list->next;
        g_slist_free_1(list);

        return item;        
}

gboolean 
gda_xql_stack_empty (GdaXqlStack * xqlstack)
{
	g_return_val_if_fail (xqlstack != NULL, FALSE);
	g_return_val_if_fail (GDA_IS_XQL_STACK (xqlstack), FALSE);
	
        return (xqlstack->priv->list == NULL) ? TRUE : FALSE;
}

static void
destroy_object_list(GSList *list)
{
    g_slist_foreach(list, (GFunc) g_object_unref, NULL);
    g_slist_free (list);
}
