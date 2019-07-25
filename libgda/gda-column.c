/*
 * Copyright (C) 2005 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2005 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Johannes Schmid <johannes.schmid@openismus.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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
#define G_LOG_DOMAIN "GDA-column"

#include <libgda/gda-column.h>
#include <string.h>
#include "gda-marshal.h"
#include <libgda/gda-custom-marshal.h>

#define PARENT_TYPE G_TYPE_OBJECT

typedef struct {
	gint         defined_size;
	gchar       *id;

	gchar       *dbms_type;
	GType        g_type;

	gboolean     allow_null;

	gboolean     auto_increment;
	glong        auto_increment_start;
	glong        auto_increment_step;
	gint         position;
	GValue      *default_value;
	gchar       *name;
	gchar       *desc;
} GdaColumnPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaColumn,gda_column,G_TYPE_OBJECT)

static void gda_column_class_init (GdaColumnClass *klass);
static void gda_column_init       (GdaColumn *column);
static void gda_column_finalize   (GObject *object);

static void gda_column_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gda_column_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

/* signals */
enum {
	NAME_CHANGED,
	GDA_TYPE_CHANGED,
	LAST_SIGNAL
};

static guint gda_column_signals[LAST_SIGNAL] = {0 , 0};

/* properties */
enum
{
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_DESC
};

static void
gda_column_class_init (GdaColumnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	/* signals */
	/**
	 * GdaColumn::name-changed:
	 * @column: the #GdaColumn object
	 * @old_name: the column's previous name
	 *
	 * Gets emitted whenever @column's name has been changed
	 */
	gda_column_signals[NAME_CHANGED] =
		g_signal_new ("name-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaColumnClass, name_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
	/**
	 * GdaColumn::g-type-changed:
	 * @column: the #GdaColumn object
	 * @old_type: the column's previous type
	 * @new_type: the column's new type
	 *
	 * Gets emitted whenever @column's type has been changed
	 */
	gda_column_signals[GDA_TYPE_CHANGED] =
		g_signal_new ("g-type-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaColumnClass, g_type_changed),
			      NULL, NULL,
			      _gda_marshal_VOID__GTYPE_GTYPE,
			      G_TYPE_NONE,
			      2, G_TYPE_GTYPE, G_TYPE_GTYPE);

	/* properties */
        object_class->set_property = gda_column_set_property;
        object_class->get_property = gda_column_get_property;

	g_object_class_install_property (object_class, PROP_ID,
	                                       g_param_spec_string ("id", NULL,
	                                       "Column's Id (warning: the column's ID is not guaranteed to be unique in a GdaDataModel)",
	                                       NULL, G_PARAM_WRITABLE | G_PARAM_READABLE));


	g_object_class_install_property (object_class, PROP_NAME,
	                                 g_param_spec_string ("name", NULL,
	                                 "Column's name",
	                                 NULL, G_PARAM_WRITABLE | G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_DESC,
	                                 g_param_spec_string ("desc", NULL,
	                                 "Column's description",
	                                 NULL, G_PARAM_WRITABLE | G_PARAM_READABLE));

	object_class->finalize = gda_column_finalize;
}

static void
gda_column_init (GdaColumn *column)
{
	g_return_if_fail (GDA_IS_COLUMN (column));

	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	priv->defined_size = 0;
	priv->id = NULL;
	priv->g_type = GDA_TYPE_NULL;
	priv->allow_null = TRUE;
	priv->auto_increment = FALSE;
	priv->auto_increment_start = 0;
	priv->auto_increment_step = 0;
	priv->position = -1;
	priv->default_value = NULL;
	priv->name = NULL;
	priv->desc = NULL;
}

static void
gda_column_finalize (GObject *object)
{
	GdaColumn *column = (GdaColumn *) object;
	
	g_return_if_fail (GDA_IS_COLUMN (column));
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	
	gda_value_free (priv->default_value);

	if (priv->id != NULL) {
		g_free (priv->id);
		priv->id = NULL;
	}
	if (priv->dbms_type != NULL) {
		g_free (priv->dbms_type);
		priv->dbms_type = NULL;
	}
	if (priv->name != NULL) {
		g_free (priv->name);
		priv->name = NULL;
	}
	if (priv->desc != NULL) {
		g_free (priv->desc);
		priv->desc = NULL;
	}
	
	G_OBJECT_CLASS (gda_column_parent_class)->finalize (object);
}

static void
gda_column_set_property (GObject *object,
                                   guint param_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	GdaColumn *col;

	col = GDA_COLUMN (object);

	GdaColumnPrivate *priv = gda_column_get_instance_private (col);
	switch (param_id) {
	case PROP_ID:
		if (priv->id != NULL) {
			g_free (priv->id);
		}
		if (g_value_get_string (value))
			priv->id = g_strdup (g_value_get_string (value));
		else
			priv->id = NULL;
		break;
	case PROP_NAME:
		if (priv->name != NULL) {
			g_free (priv->name);
		}
		if (g_value_get_string (value))
			priv->name = g_strdup (g_value_get_string (value));
		else
			priv->name = NULL;
		break;
	case PROP_DESC:
		if (priv->desc != NULL) {
			g_free (priv->desc);
		}
		if (g_value_get_string (value))
			priv->desc = g_strdup (g_value_get_string (value));
		else
			priv->desc = NULL;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gda_column_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdaColumn *col;

	col = GDA_COLUMN (object);
	GdaColumnPrivate *priv = gda_column_get_instance_private (col);
	switch (param_id) {
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_DESC:
		g_value_set_string (value, priv->desc);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
		}
}

/**
 * gda_column_new:
 *
 * Returns: a newly allocated #GdaColumn object.
 */
GdaColumn *
gda_column_new (void)
{
	return g_object_new (GDA_TYPE_COLUMN, NULL);
}

/**
 * gda_column_copy:
 * @column: column to get a copy from. 	 
 * 	 
 * Creates a new #GdaColumn object from an existing one.
 *
 * Returns: (transfer full): a newly allocated #GdaColumn with a copy of the data 	 
 * in @column. 	 
 */ 	 
GdaColumn * 	 
gda_column_copy (GdaColumn *column) 	 
{ 	 
	GdaColumn *column_copy; 	 
  	
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
  	
	column_copy = gda_column_new ();
	GdaColumnPrivate *cpriv = gda_column_get_instance_private (column_copy);
	cpriv->defined_size = priv->defined_size;
	if (priv->id)
		cpriv->id = g_strdup (priv->id);
	if (priv->name)
		cpriv->name = g_strdup (priv->name);
	if (priv->desc)
		cpriv->desc = g_strdup (priv->desc);
	cpriv->g_type = priv->g_type;
	cpriv->allow_null = priv->allow_null;
	cpriv->auto_increment = priv->auto_increment;
	cpriv->auto_increment_start = priv->auto_increment_start;
	cpriv->auto_increment_step = priv->auto_increment_step;
	cpriv->position = priv->position;
	if (priv->default_value)
		cpriv->default_value = gda_value_copy (priv->default_value);

	return column_copy;
}

/**
 * gda_column_get_name:
 * @column: a #GdaColumn.
 *
 * Returns: the name of @column.
 */
const gchar *
gda_column_get_name (GdaColumn *column)
{
	g_return_val_if_fail (column != NULL, NULL);
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);

	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return priv->name;
}

/**
 * gda_column_set_name:
 * @column: a #GdaColumn.
 * @name: the new name of @column.
 *
 * Sets the name of @column to @name.
 */
void
gda_column_set_name (GdaColumn *column, const gchar *name)
{
	g_return_if_fail (column != NULL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (GDA_IS_COLUMN (column));

	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	priv->name = g_strdup (name);
}

/**
 * gda_column_get_description:
 * @column: a #GdaColumn.
 *
 * Returns: the column's description, in any
 */
const gchar *
gda_column_get_description (GdaColumn *column)
{
	g_return_val_if_fail (column != NULL, NULL);
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);

	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return priv->desc;
}

/**
 * gda_column_set_description:
 * @column: a #GdaColumn.
 * @descr: description.
 *
 * Sets the column's description
 */
void
gda_column_set_description (GdaColumn *column, const gchar *descr)
{
	g_return_if_fail (column != NULL);
	g_return_if_fail (descr != NULL);
	g_return_if_fail (GDA_IS_COLUMN (column));

	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	priv->desc = g_strdup (descr);
}

/**
 * gda_column_get_dbms_type:
 * @column: a #GdaColumn.
 *
 * Returns: the database type of @column.
 */
const gchar*
gda_column_get_dbms_type (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return (const gchar *) priv->dbms_type;
}

/**
 * gda_column_set_dbms_type:
 * @column: a #GdaColumn
 * @dbms_type: a string
 *
 * Defines @column's database type
 */
void
gda_column_set_dbms_type (GdaColumn *column, const gchar *dbms_type)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	
  g_free (priv->dbms_type);
  priv->dbms_type = NULL;
	
	if (dbms_type)
		priv->dbms_type = g_strdup (dbms_type);
}

/**
 * gda_column_get_g_type:
 * @column: a #GdaColumn.
 *
 * Returns: the type of @column.
 */
GType
gda_column_get_g_type (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), GDA_TYPE_NULL);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return priv->g_type;
}

/**
 * gda_column_set_g_type:
 * @column: a #GdaColumn.
 * @type: the new type of @column.
 *
 * Sets the type of @column to @type.
 */
void
gda_column_set_g_type (GdaColumn *column, GType type)
{
	GType old_type;

	g_return_if_fail (GDA_IS_COLUMN (column));
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);

	old_type = priv->g_type;
	priv->g_type = type;

	g_signal_emit (G_OBJECT (column),
		       gda_column_signals[GDA_TYPE_CHANGED],
		       0, old_type, type);
}

/**
 * gda_column_get_allow_null:
 * @column: a #GdaColumn.
 *
 * Gets the 'allow null' flag of the given column.
 *
 * Returns: whether the given column allows null values or not (%TRUE or %FALSE).
 */
gboolean
gda_column_get_allow_null (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), FALSE);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return priv->allow_null;
}

/**
 * gda_column_set_allow_null:
 * @column: a #GdaColumn.
 * @allow: whether the given column should allows null values or not.
 *
 * Sets the 'allow null' flag of the given column.
 */
void
gda_column_set_allow_null (GdaColumn *column, gboolean allow)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	priv->allow_null = allow;
}

/**
 * gda_column_get_auto_increment:
 * @column: a #GdaColumn.
 *
 * Returns: whether the given column is an auto incremented one (%TRUE or %FALSE).
 */
gboolean
gda_column_get_auto_increment (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), FALSE);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return priv->auto_increment;
}

/**
 * gda_column_set_auto_increment:
 * @column: a #GdaColumn.
 * @is_auto: auto increment status.
 *
 * Sets the auto increment flag for the given column.
 */
void
gda_column_set_auto_increment (GdaColumn *column,
			       gboolean is_auto)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	priv->auto_increment = is_auto;
}

/**
 * gda_column_get_position:
 * @column: a #GdaColumn.
 *
 * Returns: the position of the column refer to in the
 * containing data model.
 */
gint
gda_column_get_position (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), -1);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return priv->position;
}

/**
 * gda_column_set_position:
 * @column: a #GdaColumn.
 * @position: the wanted position of the column in the containing data model.
 *
 * Sets the position of the column refer to in the containing
 * data model.
 */
void
gda_column_set_position (GdaColumn *column, gint position)
{
	g_return_if_fail (column != NULL);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	priv->position = position;
}


/**
 * gda_column_get_default_value:
 * @column: a #GdaColumn.
 *
 * Returns: (nullable): @column's default value, as a #GValue object, or %NULL if column does not have a default value
 */
const GValue *
gda_column_get_default_value (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);
	return (const GValue *) priv->default_value;
}

/**
 * gda_column_set_default_value:
 * @column: a #GdaColumn.
 * @default_value: (nullable): default #GValue for the column
 *
 * Sets @column's default #GValue.
 */
void
gda_column_set_default_value (GdaColumn *column, const GValue *default_value)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	GdaColumnPrivate *priv = gda_column_get_instance_private (column);

	gda_value_free (priv->default_value);
	if (default_value)
		priv->default_value = gda_value_copy ( (GValue*)default_value);
	else
		priv->default_value = NULL;
}

