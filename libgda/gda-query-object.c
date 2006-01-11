/* gda-query-object.c
 *
 * Copyright (C) 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <libgda/gda-query-object.h>
#include "gda-marshal.h"

extern GdaDict *default_dict;

/* 
 * Main static functions 
 */
static void gda_query_object_class_init (GdaQueryObjectClass * class);
static void gda_query_object_init (GdaQueryObject * srv);
static void gda_query_object_dispose (GObject   * object);
static void gda_query_object_finalize (GObject   * object);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
        NUMID_CHANGED,
        LAST_SIGNAL
};
static gint gda_query_object_signals[LAST_SIGNAL] = { 0 };

struct _GdaQueryObjectPrivate
{
	guint id;
};

GType
gda_query_object_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaQueryObjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_query_object_class_init,
			NULL,
			NULL,
			sizeof (GdaQueryObject),
			0,
			(GInstanceInitFunc) gda_query_object_init
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaQueryObject", &info, 0);
	}
	return type;
}

static void
gda_query_object_class_init (GdaQueryObjectClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_query_object_signals [NUMID_CHANGED] = 
		g_signal_new ("numid_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaQueryObjectClass, numid_changed),
                              NULL, NULL,
                              gda_marshal_VOID__VOID, G_TYPE_NONE,
                              0);

	class->numid_changed = NULL;

	/* virtual functions */
	class->set_int_id = NULL;

	object_class->dispose = gda_query_object_dispose;
	object_class->finalize = gda_query_object_finalize;
}

static void
gda_query_object_init (GdaQueryObject *gdaobj)
{
	gdaobj->priv = g_new0 (GdaQueryObjectPrivate, 1);
}

static void
gda_query_object_dispose (GObject *object)
{
	GdaQueryObject *gdaobj;

	g_return_if_fail (GDA_IS_OBJECT (object));

	gdaobj = GDA_QUERY_OBJECT (object);
	if (gdaobj->priv) {
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_query_object_finalize (GObject   * object)
{
	GdaQueryObject *gdaobj;

	g_return_if_fail (GDA_IS_OBJECT (object));

	gdaobj = GDA_QUERY_OBJECT (object);
	if (gdaobj->priv) {
		g_free (gdaobj->priv);
		gdaobj->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

/**
 * gda_query_object_set_int_id
 * @qobj:
 * @id:
 *
 * Sets the integer ID of @qobj; this also triggers a change in the string ID 
 * (which can be obtained using gda_object_get_id()) of the object.
 */
void
gda_query_object_set_int_id (GdaQueryObject *qobj, guint id)
{
	g_return_if_fail (GDA_IS_QUERY_OBJECT (qobj));
	g_return_if_fail (qobj->priv);

	g_assert (GDA_QUERY_OBJECT_CLASS (G_OBJECT_GET_CLASS (qobj))->set_int_id);

	if (qobj->priv->id == id)
		return;

	qobj->priv->id = id;
	GDA_QUERY_OBJECT_CLASS (G_OBJECT_GET_CLASS (qobj))->set_int_id (qobj, id);

#ifdef debug_signal
	g_print (">> 'NUMID_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (qobj), gda_query_object_signals[NUMID_CHANGED], 0);
#ifdef debug_signal
	g_print ("<< 'NUMID_CHANGED' from %s\n", __FUNCTION__);
#endif
}

/**
 * gda_query_object_get_int_id 
 * @qobj:
 * @id:
 *
 * Get the integer ID of @qobj
 *
 * Return: the integer ID
 */
guint
gda_query_object_get_int_id (GdaQueryObject *qobj)
{
	g_return_val_if_fail (GDA_IS_QUERY_OBJECT (qobj), 0);
	g_return_val_if_fail (qobj->priv, 0);
	
	return qobj->priv->id;
}
