/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
 *	Gerhard Dieringer <gdieringer@compuserve.com>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include "config.h"
#include "gda-xml-atom-item.h"
#include "gda-xml-bin-item.h"
#include "gda-xml-list-item.h"
#include "gda-xql-dml.h"
#include "gda-xql-join.h"
#include "gda-xql-target.h"

struct _GdaXqlDmlPrivate {
	GdaXmlItem *target;
	GdaXmlItem *valuelist;
	GdaXmlItem *where;
	GdaXmlItem *having;
	GdaXmlItem *group;
	GdaXmlItem *trailer;
	GdaXmlItem *dest;
	GdaXmlItem *source;
	GdaXmlItem *setlist;
};

static void gda_xql_dml_class_init (GdaXqlDmlClass *klass);
static void gda_xql_dml_init       (GdaXqlDml *dml);
static void gda_xql_dml_destroy    (GtkObject *object);

/*
 * GdaXqlDml class implementation
 */
static void
gda_xql_dml_class_init (GdaXqlDmlClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	GdaXmlItemClass *item_class = GDA_XML_ITEM_CLASS (klass);

	object_class->destroy = gda_xql_dml_destroy;
	item_class->to_dom = gda_xql_dml_class_to_dom;
	item_class->find_id = gda_xql_dml_class_find_id;
	klass->add_target_from_text = gda_xql_dml_class_add_target_from_text;
	klass->add_field_from_text = NULL;
	klass->add_const_from_text = NULL;
	klass->add_func = NULL;
	klass->add_query = NULL;
	klass->add_row_condition = gda_xql_dml_class_add_row_condition;
	klass->add_group_condition = NULL;
	klass->add_order = NULL;
	klass->add_set = NULL;
	klass->add_set_const = NULL;
}

static void
gda_xql_dml_init (GdaXqlDml *dml)
{
	dml->priv = g_new (GdaXqlDmlPrivate, 1);
	dml->priv->target = NULL;
	dml->priv->valuelist = NULL;
	dml->priv->where = NULL;
	dml->priv->having = NULL;
	dml->priv->group = NULL;
	dml->priv->trailer = NULL;
	dml->priv->dest = NULL;
	dml->priv->source = NULL;
	dml->priv->setlist = NULL;
}

static void
gda_xql_dml_destroy (GtkObject *object)
{
	GtkObjectClass *parent_class;
	GdaXqlDml *dml = (GdaXqlDml *) object;

	g_return_if_fail (GDA_IS_XQL_DML (dml));

	/* free memory */
	gtk_object_unref (GTK_OBJECT (dml->priv->target));
	gtk_object_unref (GTK_OBJECT (dml->priv->valuelist));
	gtk_object_unref (GTK_OBJECT (dml->priv->where));
	gtk_object_unref (GTK_OBJECT (dml->priv->having));
	gtk_object_unref (GTK_OBJECT (dml->priv->group));
	gtk_object_unref (GTK_OBJECT (dml->priv->trailer));
	gtk_object_unref (GTK_OBJECT (dml->priv->dest));
	gtk_object_unref (GTK_OBJECT (dml->priv->source));
	gtk_object_unref (GTK_OBJECT (dml->priv->setlist));
	g_free (dml->priv);
	dml->priv = NULL;

	parent_class = gtk_type_class (gda_xml_item_get_type ());
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}

GtkType
gda_xql_dml_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaXqlDml",
			sizeof (GdaXqlDml),
			sizeof (GdaXqlDmlClass),
			(GtkClassInitFunc) gda_xql_dml_class_init,
			(GtkObjectInitFunc) gda_xql_dml_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL
		};
		type = gtk_type_unique (gda_xml_item_get_type (), &info);
	}

	return type;
}

/**
 * gda_xql_dml_add_target_from_text
 */
gchar *
gda_xql_dml_add_target_from_text (GdaXqlDml *dml,
				  const gchar *name,
				  GdaXmlItem *join)
{
	GdaXqlDmlClass *dml_class;

	dml_class = gtk_type_class (gda_xql_dml_get_type ());
	if (dml_class && dml_class->add_target_from_text)
		return dml_class->add_target_from_text (dml, name, join);

	return NULL;
}

/**
 * gda_xql_dml_add_field_from_text
 */
GdaXmlItem *
gda_xql_dml_add_field_from_text (GdaXqlDml *dml,
				 const gchar *id,
				 const gchar *name,
				 const gchar *alias,
				 gboolean group)
{
	GdaXqlDmlClass *dml_class;

	dml_class = gtk_type_class (gda_xql_dml_get_type ());
	if (dml_class && dml_class->add_field_from_text)
		return dml_class->add_field_from_text (dml, id, name, alias, group);

	return NULL;
}

/**
 * gda_xql_dml_add_const_from_text
 */
GdaXmlItem *
gda_xql_dml_add_const_from_text (GdaXqlDml *dml,
				 const gchar *value,
				 const gchar *type,
				 gboolean null)
{
	GdaXqlDmlClass *dml_class;

	dml_class = gtk_type_class (gda_xql_dml_get_type ());
	if (dml_class && dml_class->add_const_from_text)
		return dml_class->add_const_from_text (dml, value, type, null);

	return NULL;
}
