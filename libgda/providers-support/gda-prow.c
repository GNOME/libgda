/* GDA library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Álvaro Peña <alvaropg@telefonica.net>
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

#include "gda-prow.h"
#include <string.h>
#include "gda-data-model.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaPRowPrivate {
	GdaDataModel *model; /* can be NULL */

        GValue       *fields;        /* GValue for each column */
        gint          nfields;
};

/* properties */
enum
{
        PROP_0,
        PROP_NB_VALUES
};

static void gda_prow_class_init (GdaPRowClass *klass);
static void gda_prow_init       (GdaPRow *prow, GdaPRowClass *klass);
static void gda_prow_finalize   (GObject *object);
static void gda_prow_dispose    (GObject *object);

static void gda_prow_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec);
static void gda_prow_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec);

static GObjectClass *parent_class = NULL;

static void
gda_prow_class_init (GdaPRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_prow_finalize;
	object_class->dispose = gda_prow_dispose;

	/* Properties */
        object_class->set_property = gda_prow_set_property;
        object_class->get_property = gda_prow_get_property;

	g_object_class_install_property (object_class, PROP_NB_VALUES,
                                         g_param_spec_int ("nb_values", NULL, NULL,
							   1, G_MAXINT, 1, 
							   G_PARAM_WRITABLE));
}

static void
gda_prow_init (GdaPRow *prow, GdaPRowClass *klass)
{
	g_return_if_fail (GDA_IS_PROW (prow));
	
	prow->priv = g_new0 (GdaPRowPrivate, 1);
	prow->priv->model = NULL;
	prow->priv->fields = NULL;
	prow->priv->nfields = 0;
}

static void
gda_prow_dispose (GObject *object)
{
	GdaPRow *prow = (GdaPRow *) object;
	
	g_return_if_fail (GDA_IS_PROW (prow));
		
	parent_class->finalize (object);
}

static void
gda_prow_finalize (GObject *object)
{
	GdaPRow *prow = (GdaPRow *) object;
	
	g_return_if_fail (GDA_IS_PROW (prow));
	
	if (prow->priv) {
		gint i;

		for (i = 0; i < prow->priv->nfields; i++)
			gda_value_set_null (&(prow->priv->fields [i]));
		g_free (prow->priv->fields);

		g_free (prow->priv);
		prow->priv = NULL;
	}
	
	parent_class->finalize (object);
}

static void
gda_prow_set_property (GObject *object,
		      guint param_id,
		      const GValue *value,
		      GParamSpec *pspec)
{
        GdaPRow *prow;

        prow = GDA_PROW (object);
        if (prow->priv) {
                switch (param_id) {
		case PROP_NB_VALUES:
			g_return_if_fail (!prow->priv->fields);

			prow->priv->nfields = g_value_get_int (value);
			prow->priv->fields = g_new0 (GValue, prow->priv->nfields);			
			break;
		default:
			g_assert_not_reached ();
			break;
                }
        }
}

static void
gda_prow_get_property (GObject *object,
		      guint param_id,
		      GValue *value,
		      GParamSpec *pspec)
{
        GdaPRow *prow;

        prow = GDA_PROW (object);
        if (prow->priv) {
                switch (param_id) {
		case PROP_NB_VALUES:
			g_value_set_int (value, prow->priv->nfields);
			break;
		default:
			g_assert_not_reached ();
			break;
                }
        }
}

GType
gda_prow_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaPRowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_prow_class_init,
			NULL,
			NULL,
			sizeof (GdaPRow),
			0,
			(GInstanceInitFunc) gda_prow_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaPRow", &info, 0);
	}

	return type;
}

/**
 * gda_prow_new
 * @model: the #GdaDataModel this prow belongs to, or %NULL if the prow is outside any data model
 * @count: number of #GValue in the new #GdaPRow.
 *
 * Creates a #GdaPRow which can hold @count #GValue values.
 *
 * The caller of this function is the only owner of a reference to the newly created #GdaPRow
 * object, even if @model is not %NULL (it is recommended to pass %NULL as the @model argument
 * if this function is not called from within a #GdaDataModel implementation).
 *
 * Returns: a newly allocated #GdaPRow object.
 */
GdaPRow *
gda_prow_new (gint count)
{
	GdaPRow *prow = NULL;

        g_return_val_if_fail (count > 0, NULL);

	prow = g_object_new (GDA_TYPE_PROW, "nb_values", count, NULL);

	return prow;
}

/**
 * gda_prow_get_value
 * @prow: a #GdaPRow
 * @num: field index.
 *
 * Gets a pointer to a #GValue stored in a #GdaPRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it!
 *
 * Returns: a pointer to the #GValue in the position @num of @prow.
 */
GValue *
gda_prow_get_value (GdaPRow *prow, gint num)
{
        g_return_val_if_fail (GDA_IS_PROW (prow), NULL);
	g_return_val_if_fail (prow->priv, NULL);
        g_return_val_if_fail (num >= 0 && num < prow->priv->nfields, NULL);

        return & (prow->priv->fields[num]);
}


/**
 * gda_prow_get_length
 * @prow: a #GdaPRow.
 *
 * Returns: the number of columns that the @prow has.
 */
gint
gda_prow_get_length (GdaPRow *prow)
{
        g_return_val_if_fail (GDA_IS_PROW (prow), 0);
	g_return_val_if_fail (prow->priv, 0);

        return prow->priv->nfields;
}
