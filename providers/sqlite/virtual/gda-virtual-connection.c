/* 
 * GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <sqlite3.h>
#include "gda-virtual-connection.h"

#define PARENT_TYPE GDA_TYPE_CONNECTION
#define CLASS(obj) (GDA_VIRTUAL_CONNECTION_CLASS (G_OBJECT_GET_CLASS (obj)))

struct _GdaVirtualConnectionPrivate {
	
};

/* properties */
enum
{
        PROP_0,
};

static void gda_virtual_connection_class_init (GdaVirtualConnectionClass *klass);
static void gda_virtual_connection_init       (GdaVirtualConnection *prov, GdaVirtualConnectionClass *klass);
static void gda_virtual_connection_finalize   (GObject *object);
static void gda_virtual_connection_set_property (GObject *object,
						 guint param_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void gda_virtual_connection_get_property (GObject *object,
						 guint param_id,
						 GValue *value,
						 GParamSpec *pspec);
static GObjectClass *parent_class = NULL;

/*
 * GdaVirtualConnection class implementation
 */
static void
gda_virtual_connection_class_init (GdaVirtualConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	object_class->finalize = gda_virtual_connection_finalize;

	/* Properties */
        object_class->set_property = gda_virtual_connection_set_property;
        object_class->get_property = gda_virtual_connection_get_property;
}

static void
gda_virtual_connection_init (GdaVirtualConnection *vprov, GdaVirtualConnectionClass *klass)
{
	vprov->priv = g_new (GdaVirtualConnectionPrivate, 1);
}

static void
gda_virtual_connection_finalize (GObject *object)
{
	GdaVirtualConnection *prov = (GdaVirtualConnection *) object;

	g_return_if_fail (GDA_IS_VIRTUAL_CONNECTION (prov));

	/* free memory */
	g_free (prov->priv);
	prov->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_virtual_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVirtualConnectionClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_virtual_connection_class_init,
				NULL, NULL,
				sizeof (GdaVirtualConnection),
				0,
				(GInstanceInitFunc) gda_virtual_connection_init
			};
			
			type = g_type_register_static (PARENT_TYPE, "GdaVirtualConnection", &info, G_TYPE_FLAG_ABSTRACT);
		}
	}

	return type;
}

static void
gda_virtual_connection_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
        GdaVirtualConnection *prov;

        prov = GDA_VIRTUAL_CONNECTION (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			break;
                }
        }
}

static void
gda_virtual_connection_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
        GdaVirtualConnection *prov;

        prov = GDA_VIRTUAL_CONNECTION (object);
        if (prov->priv) {
		switch (param_id) {
		default:
			break;
		}
        }
}
