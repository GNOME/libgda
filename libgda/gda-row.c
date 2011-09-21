/*
 * Copyright (C) 2000 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Cleber Rodrigues <cleberrrjr@bol.com.br>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2003 Xabier Rodríguez Calvar <xrcalvar@igalia.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "gda-row.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-data-model.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaRowPrivate {
	GdaDataModel *model; /* can be NULL */

        GValue       *fields; /* GValues */
	GError      **errors; /* GError for each invalid value at the same position */
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
gda_row_init (GdaRow *row, G_GNUC_UNUSED GdaRowClass *klass)
{
	g_return_if_fail (GDA_IS_ROW (row));
	
	row->priv = g_new0 (GdaRowPrivate, 1);
	row->priv->model = NULL;
	row->priv->fields = NULL;
	row->priv->errors = NULL;
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

		for (i = 0; i < row->priv->nfields; i++) {
			gda_value_set_null (&(row->priv->fields [i]));
			if (row->priv->errors && row->priv->errors [i])
				g_error_free (row->priv->errors [i]);
		}
		g_free (row->priv->fields);
		g_free (row->priv->errors);

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
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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
			(GInstanceInitFunc) gda_row_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "GdaRow", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/**
 * gda_row_new:
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
 * gda_row_get_value:
 * @row: a #GdaRow
 * @num: field index.
 *
 * Gets a pointer to a #GValue stored in a #GdaRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it (modifying is reserved to database provider's implementations).
 *
 * Returns: (transfer none): a pointer to the #GValue in the position @num of @row.
 */
GValue *
gda_row_get_value (GdaRow *row, gint num)
{
        g_return_val_if_fail (GDA_IS_ROW (row), NULL);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, NULL);

        return & (row->priv->fields[num]);
}

/**
 * gda_row_invalidate_value:
 * @row: a #GdaRow
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * 
 * Marks @value as being invalid. This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 */
void
gda_row_invalidate_value (G_GNUC_UNUSED GdaRow *row, GValue *value)
{
	return gda_row_invalidate_value_e (row, value, NULL);
}

/**
 * gda_row_invalidate_value_e:
 * @row: a #GdaRow
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * @error: (allow-none) (transfer full): the error which lead to the invalidation
 * 
 * Marks @value as being invalid. This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Since: 4.2.10
 */
void
gda_row_invalidate_value_e (GdaRow *row, GValue *value, GError *error)
{
	gda_value_set_null (value);
	G_VALUE_TYPE (value) = G_TYPE_NONE;
	if (error) {
		guint i;
		if (! row->priv->errors)
			row->priv->errors = g_new0 (GError*, row->priv->nfields);
		for (i = 0; i < row->priv->nfields; i++) {
			if (& (row->priv->fields[i]) == value) {
				if (row->priv->errors [i])
					g_error_free (row->priv->errors [i]);
				row->priv->errors [i] = error;
				break;
			}
		}
		if (i == row->priv->nfields) {
			g_error_free (error);
			g_warning (_("Value not found in row!"));
		}
	}
	else if (row->priv->errors) {
		guint i;
		for (i = 0; i < row->priv->nfields; i++) {
			if (& (row->priv->fields[i]) == value) {
				if (row->priv->errors [i]) {
					g_error_free (row->priv->errors [i]);
					row->priv->errors [i] = NULL;
				}
				break;
			}
		}
		if (i == row->priv->nfields)
			g_warning (_("Value not found in row!"));
	}
}

/**
 * gda_row_value_is_valid:
 * @row: a #GdaRow.
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 *
 * Tells if @value has been marked as being invalid by gda_row_invalidate_value().
 * This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Returns: %TRUE if @value is valid
 */
gboolean
gda_row_value_is_valid (GdaRow *row, GValue *value)
{
	return gda_row_value_is_valid_e (row, value, NULL);
}

/**
 * gda_row_value_is_valid:
 * @row: a #GdaRow.
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * @error: (allow-none): a place to store the invalid error, or %NULL
 *
 * Tells if @value has been marked as being invalid by gda_row_invalidate_value().
 * This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Returns: %TRUE if @value is valid
 *
 * Since: 4.2.10
 */
gboolean
gda_row_value_is_valid_e (GdaRow *row, GValue *value, GError **error)
{
	gboolean valid;
	valid = (G_VALUE_TYPE (value) == G_TYPE_NONE) ? FALSE : TRUE;
	if (!valid && row->priv->errors && error) {
		guint i;
		for (i = 0; i < row->priv->nfields; i++) {
			if (& (row->priv->fields[i]) == value) {
				if (row->priv->errors [i])
					g_propagate_error (error, g_error_copy (row->priv->errors [i]));
				break;
			}
		}
		if (i == row->priv->nfields)
			g_warning (_("Value not found in row!"));
	}
	return valid;
}

/**
 * gda_row_get_length:
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
