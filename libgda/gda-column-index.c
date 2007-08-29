/* GDA library
 * Copyright (C) 1998-2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Bas Driessen <bas.driessen@xobas.com>
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

#include <string.h>
#include <libgda/gda-column-index.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaColumnIndexPrivate {
	gchar *column_name;
	gint defined_size;
	GdaSorting sorting;
	gchar *references;
};

static void gda_column_index_class_init (GdaColumnIndexClass *klass);
static void gda_column_index_init       (GdaColumnIndex *dmcia, GdaColumnIndexClass *klass);
static void gda_column_index_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

static void
gda_column_index_class_init (GdaColumnIndexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_column_index_finalize;
}

static void
gda_column_index_init (GdaColumnIndex *dmcia, GdaColumnIndexClass *klass)
{
	g_return_if_fail (GDA_IS_COLUMN_INDEX (dmcia));
	
	dmcia->priv = g_new0 (GdaColumnIndexPrivate, 1);
	dmcia->priv->column_name = NULL;
	dmcia->priv->defined_size = 0;
	dmcia->priv->sorting = GDA_SORTING_ASCENDING;
	dmcia->priv->references = NULL;
}

static void
gda_column_index_finalize (GObject *object)
{
	GdaColumnIndex *dmcia = (GdaColumnIndex *) object;
	
	g_return_if_fail (GDA_IS_COLUMN_INDEX (dmcia));
	
	if (dmcia->priv) {
		if (dmcia->priv->column_name)
			g_free (dmcia->priv->column_name);
		if (dmcia->priv->references)
			g_free (dmcia->priv->references);
		g_free (dmcia->priv);
	}

	parent_class->finalize (object);
}

GType
gda_column_index_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaColumnIndexClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_column_index_class_init,
			NULL,
			NULL,
			sizeof (GdaColumnIndex),
			0,
			(GInstanceInitFunc) gda_column_index_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaColumnIndex", &info, 0);
	}
	
	return type;
}

/**
 * gda_column_index_new
 *
 * Returns: a newly allocated #GdaColumnIndex object.
 */
GdaColumnIndex *
gda_column_index_new (void)
{
	GdaColumnIndex *dmcia;

	dmcia = g_object_new (GDA_TYPE_COLUMN_INDEX, NULL);

	return dmcia;
}

/**
 * gda_column_index_copy
 * @dmcia: attributes to get a copy from.
 *
 * Creates a new #GdaColumnIndex object from an existing one.
 * Returns: a newly allocated #GdaColumnIndex with a copy of the data
 * in @dmcia.
 */
GdaColumnIndex *
gda_column_index_copy (GdaColumnIndex *dmcia)
{
	GdaColumnIndex *dmcia_copy;

	g_return_val_if_fail (GDA_IS_COLUMN_INDEX (dmcia), NULL);

	dmcia_copy = gda_column_index_new ();
	dmcia_copy->priv->column_name = g_strdup (dmcia->priv->column_name);
	dmcia_copy->priv->defined_size = dmcia->priv->defined_size;
	dmcia_copy->priv->sorting = dmcia->priv->sorting;
	dmcia_copy->priv->references = g_strdup (dmcia->priv->references);

	return dmcia_copy;
}

/**
 * gda_column_index_equal:
 * @lhs: a #GdaColumnIndex
 * @rhs: another #GdaColumnIndex
 *
 * Tests whether two field attributes are equal.
 *
 * Return value: %TRUE if the field attributes contain the same information.
 **/
gboolean
gda_column_index_equal (const GdaColumnIndex *lhs,
			const GdaColumnIndex *rhs)
{
	g_return_val_if_fail (GDA_IS_COLUMN_INDEX (lhs), FALSE);
	g_return_val_if_fail (GDA_IS_COLUMN_INDEX (rhs), FALSE);

	/* Compare every struct field: */
	if ((lhs->priv->defined_size != rhs->priv->defined_size) ||
	    (lhs->priv->sorting != rhs->priv->sorting))
		return FALSE;

	/* Check the strings if they have are not null.
	   Then check whether one is null while the other is not, because strcmp can not do that. */
	if ((lhs->priv->column_name && rhs->priv->column_name) && (strcmp (lhs->priv->column_name, rhs->priv->column_name) != 0))
		return FALSE;
    
	if ((lhs->priv->column_name == 0) != (rhs->priv->column_name == 0))
		return FALSE;
    
	if ((lhs->priv->references && rhs->priv->references) && (strcmp (lhs->priv->references, rhs->priv->references) != 0))
		return FALSE;

	if ((lhs->priv->references == 0) != (rhs->priv->references == 0))
		return FALSE;
    
	return TRUE;
}

/**
 * gda_column_index_get_column_name
 * @dmcia: a #GdaColumnIndex.
 *
 * Returns: the column name of @dmcia.
 */
const gchar *
gda_column_index_get_column_name (GdaColumnIndex *dmcia)
{
	g_return_val_if_fail (GDA_IS_COLUMN_INDEX (dmcia), NULL);
	return (const gchar *) dmcia->priv->column_name;
}

/**
 * gda_column_index_set_column_name
 * @dmcia: a #GdaColumnIndex.
 * @column_name: the new name of @dmcia.
 *
 * Sets the name of @dmcia to @column_name.
 */
void
gda_column_index_set_column_name (GdaColumnIndex *dmcia, const gchar *column_name)
{
	g_return_if_fail (GDA_IS_COLUMN_INDEX (dmcia));
	g_return_if_fail (column_name != NULL);

	if (dmcia->priv->column_name != NULL)
		g_free (dmcia->priv->column_name);
	dmcia->priv->column_name = g_strdup (column_name);
}

/**
 * gda_column_index_get_defined_size
 * @dmcia: a @GdaColumnIndex.
 *
 * Returns: the defined size of @dmcia.
 */
glong
gda_column_index_get_defined_size (GdaColumnIndex *dmcia)
{
	g_return_val_if_fail (dmcia != NULL, 0);
	return dmcia->priv->defined_size;
}

/**
 * gda_column_index_set_defined_size
 * @dmcia: a #GdaColumnIndex.
 * @size: the defined size we want to set.
 *
 * Sets the defined size of a #GdaColumnIndex.
 */
void
gda_column_index_set_defined_size (GdaColumnIndex *dmcia, glong size)
{
	g_return_if_fail (GDA_IS_COLUMN_INDEX (dmcia));
	dmcia->priv->defined_size = size;
}

/**
 * gda_column_index_get_sorting
 * @dmcia: a @GdaColumnIndex.
 *
 * Returns: the sorting of @dmcia.
 */
GdaSorting
gda_column_index_get_sorting (GdaColumnIndex *dmcia)
{
	g_return_val_if_fail (dmcia != NULL, 0);
	return dmcia->priv->sorting;
}

/**
 * gda_column_index_set_sorting
 * @dmcia: a #GdaColumnIndex.
 * @sorting: the new sorting of @dmcia.
 *
 * Sets the sorting of a #GdaColumnIndex.
 */
void
gda_column_index_set_sorting (GdaColumnIndex *dmcia, GdaSorting sorting)
{
	g_return_if_fail (GDA_IS_COLUMN_INDEX (dmcia));
	dmcia->priv->sorting = sorting;
}

/**
 * gda_column_index_get_references
 * @dmcia: a #GdaColumnIndex.
 *
 * Returns: @dmcia's references.
 */
const gchar *
gda_column_index_get_references (GdaColumnIndex *dmcia)
{
	g_return_val_if_fail (GDA_IS_COLUMN_INDEX (dmcia), NULL);
	return (const gchar *) dmcia->priv->references;
}

/**
 * gda_column_index_set_references
 * @dmcia: a #GdaColumnIndex.
 * @ref: references.
 *
 * Sets @dmcia's @references.
 */
void
gda_column_index_set_references (GdaColumnIndex *dmcia, const gchar *ref)
{
	g_return_if_fail (GDA_IS_COLUMN_INDEX (dmcia));

	if (dmcia->priv->references != NULL) {
		g_free (dmcia->priv->references);
		dmcia->priv->references = NULL;
	}

	if (ref)
		dmcia->priv->references = g_strdup (ref);
}

