/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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

#include "gda-row.h"
#include <string.h>
#include "gda-data-model.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaRowPrivate {
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

static void gda_row_class_init (GdaRowClass *klass);
static void gda_row_init       (GdaRow *row, GdaRowClass *klass);
static void gda_row_finalize   (GObject *object);
static void gda_row_dispose    (GObject *object);

static void gda_row_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec);
static void gda_row_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec);

static GObjectClass *parent_class = NULL;

static void
gda_row_class_init (GdaRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_row_finalize;
	object_class->dispose = gda_row_dispose;

	/* Properties */
        object_class->set_property = gda_row_set_property;
        object_class->get_property = gda_row_get_property;

	g_object_class_install_property (object_class, PROP_NB_VALUES,
                                         g_param_spec_int ("nb-values", NULL, "Number of values in the row",
							   1, G_MAXINT, 1, 
							   G_PARAM_WRITABLE));
}

static void
gda_row_init (GdaRow *row, GdaRowClass *klass)
{
	g_return_if_fail (GDA_IS_ROW (row));
	
	row->priv = g_new0 (GdaRowPrivate, 1);
	row->priv->model = NULL;
	row->priv->fields = NULL;
	row->priv->nfields = 0;
}

static void
gda_row_dispose (GObject *object)
{
	GdaRow *row = (GdaRow *) object;
	
	g_return_if_fail (GDA_IS_ROW (row));
		
	parent_class->finalize (object);
}

static void
gda_row_finalize (GObject *object)
{
	GdaRow *row = (GdaRow *) object;
	
	g_return_if_fail (GDA_IS_ROW (row));
	
	if (row->priv) {
		gint i;

		for (i = 0; i < row->priv->nfields; i++)
			gda_value_set_null (&(row->priv->fields [i]));
		g_free (row->priv->fields);

		g_free (row->priv);
		row->priv = NULL;
	}
	
	parent_class->finalize (object);
}

static void
gda_row_set_property (GObject *object,
		      guint param_id,
		      const GValue *value,
		      GParamSpec *pspec)
{
        GdaRow *row;

        row = GDA_ROW (object);
        if (row->priv) {
                switch (param_id) {
		case PROP_NB_VALUES:
			g_return_if_fail (!row->priv->fields);

			row->priv->nfields = g_value_get_int (value);
			row->priv->fields = g_new0 (GValue, row->priv->nfields);			
			break;
		default:
			g_assert_not_reached ();
			break;
                }
        }
}

static void
gda_row_get_property (GObject *object,
		      guint param_id,
		      GValue *value,
		      GParamSpec *pspec)
{
        GdaRow *row;

        row = GDA_ROW (object);
        if (row->priv) {
                switch (param_id) {
		case PROP_NB_VALUES:
			g_value_set_int (value, row->priv->nfields);
			break;
		default:
			g_assert_not_reached ();
			break;
                }
        }
}

GType
gda_row_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaRowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_row_class_init,
			NULL,
			NULL,
			sizeof (GdaRow),
			0,
			(GInstanceInitFunc) gda_row_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "GdaRow", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/**
 * gda_row_new
 * @count: number of #GValue in the new #GdaRow.
 *
 * Creates a #GdaRow which can hold @count #GValue values.
 *
 * Returns: a newly allocated #GdaRow object.
 */
GdaRow *
gda_row_new (gint count)
{
        g_return_val_if_fail (count > 0, NULL);
	
	return (GdaRow*) g_object_new (GDA_TYPE_ROW, "nb-values", count, NULL);
}

/**
 * gda_row_get_value
 * @row: a #GdaRow
 * @num: field index.
 *
 * Gets a pointer to a #GValue stored in a #GdaRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it (modifying is reserved to database provider's implementations).
 *
 * Returns: a pointer to the #GValue in the position @num of @row.
 */
GValue *
gda_row_get_value (GdaRow *row, gint num)
{
        g_return_val_if_fail (GDA_IS_ROW (row), NULL);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, NULL);

        return & (row->priv->fields[num]);
}

/**
 * gda_row_invalidate_value
 * @row: a #GdaRow
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * 
 * Marks @value as being invalid. This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 */
void
gda_row_invalidate_value (GdaRow *row, GValue *value)
{
	gda_value_set_null (value);
	G_VALUE_TYPE (value) = G_TYPE_NONE;
}

/**
 * gda_row_value_is_valid
 * @row: a #GdaRow.
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 *
 * Tells if @value has been marked as being invalid by gda_row_invalidate_value().
 * This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Returns: TRUE if @value is valid
 */
gboolean
gda_row_value_is_valid (GdaRow *row, GValue *value)
{
	return (G_VALUE_TYPE (value) == G_TYPE_NONE) ? FALSE : TRUE;
}

/**
 * gda_row_get_length
 * @row: a #GdaRow.
 *
 * Returns: the number of columns that the @row has.
 */
gint
gda_row_get_length (GdaRow *row)
{
        g_return_val_if_fail (GDA_IS_ROW (row), 0);
	g_return_val_if_fail (row->priv, 0);

        return row->priv->nfields;
}
