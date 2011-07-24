/*
 * Copyright (C) 2005 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2005 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Johannes Schmid <johannes.schmid@openismus.com>
 * Copyright (C) 2008 PrzemysÅ‚aw Grzegorczyk <pgrzegorczyk@gmail.com>
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

#include <libgda/gda-column.h>
#include <string.h>
#include "gda-marshal.h"
#include <libgda/gda-attributes-manager.h>
#include <libgda/gda-custom-marshal.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaColumnPrivate {
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
};

static void gda_column_class_init (GdaColumnClass *klass);
static void gda_column_init       (GdaColumn *column, GdaColumnClass *klass);
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
};

static GObjectClass *parent_class = NULL;
GdaAttributesManager *gda_column_attributes_manager;

static void
gda_column_class_init (GdaColumnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);
	
	/* signals */
	/**
	 * GdaColumn::name-changed
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
	 * GdaColumn::g-type-changed
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
							      "Column's Id (warning: the column's ID is not "
							      "guaranteed to be unique in a GdaDataModel)",
                                                              NULL, G_PARAM_WRITABLE | G_PARAM_READABLE));

	object_class->finalize = gda_column_finalize;

	/* extra */
	gda_column_attributes_manager = gda_attributes_manager_new (TRUE, NULL, NULL);
}

static void
gda_column_init (GdaColumn *column, G_GNUC_UNUSED GdaColumnClass *klass)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	
	column->priv = g_new0 (GdaColumnPrivate, 1);
	column->priv->defined_size = 0;
	column->priv->id = NULL;
	column->priv->g_type = G_TYPE_INVALID;
	column->priv->allow_null = TRUE;
	column->priv->auto_increment = FALSE;
	column->priv->auto_increment_start = 0;
	column->priv->auto_increment_step = 0;
	column->priv->position = -1;
	column->priv->default_value = NULL;
}

static void
gda_column_finalize (GObject *object)
{
	GdaColumn *column = (GdaColumn *) object;
	
	g_return_if_fail (GDA_IS_COLUMN (column));
	
	if (column->priv) {
		gda_value_free (column->priv->default_value);
	
		g_free (column->priv->id);
		g_free (column->priv->dbms_type);

		g_free (column->priv);
		column->priv = NULL;
	}
	
	parent_class->finalize (object);
}

GType
gda_column_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaColumnClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_column_class_init,
			NULL,
			NULL,
			sizeof (GdaColumn),
			0,
			(GInstanceInitFunc) gda_column_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "GdaColumn", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	
	return type;
}

static void
gda_column_set_property (GObject *object,
                                   guint param_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
        GdaColumn *col;

        col = GDA_COLUMN (object);
	if (col->priv) {
		switch (param_id) {
                case PROP_ID:
			g_free (col->priv->id);
			if (g_value_get_string (value))
				col->priv->id = g_strdup (g_value_get_string (value));
			else
				col->priv->id = NULL;
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
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
	if (col->priv) {
		switch (param_id) {
                case PROP_ID:
			g_value_set_string (value, col->priv->id);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
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
	GdaColumn *column;

	column = g_object_new (GDA_TYPE_COLUMN, NULL);

	return column;
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
  	
	column_copy = gda_column_new (); 	 
	column_copy->priv->defined_size = column->priv->defined_size;
	if (column->priv->id)
		column_copy->priv->id = g_strdup (column->priv->id);
	column_copy->priv->g_type = column->priv->g_type;
	column_copy->priv->allow_null = column->priv->allow_null;
	column_copy->priv->auto_increment = column->priv->auto_increment;
	column_copy->priv->auto_increment_start = column->priv->auto_increment_start;
	column_copy->priv->auto_increment_step = column->priv->auto_increment_step;
	column_copy->priv->position = column->priv->position;
	if (column->priv->default_value)
		column_copy->priv->default_value = gda_value_copy (column->priv->default_value);
	gda_attributes_manager_copy (gda_column_attributes_manager, (gpointer) column, gda_column_attributes_manager, (gpointer) column_copy);

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
	const GValue *cvalue;
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	cvalue = gda_column_get_attribute (column, GDA_ATTRIBUTE_NAME);
	if (cvalue)
		return g_value_get_string (cvalue);
	else
		return NULL;
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
	gchar *old_name = NULL;
	GValue *value = NULL;

	g_return_if_fail (GDA_IS_COLUMN (column));

	old_name = (gchar *) gda_column_get_name (column);
	if (old_name)
		old_name = g_strdup (old_name);

	if (name)
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), name);
	gda_column_set_attribute_static (column, GDA_ATTRIBUTE_NAME, value);
	if (value)
		gda_value_free (value);

	g_signal_emit (G_OBJECT (column),
		       gda_column_signals[NAME_CHANGED],
		       0, old_name);

	g_free (old_name);
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
	const GValue *cvalue;
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	cvalue = gda_column_get_attribute (column, GDA_ATTRIBUTE_DESCRIPTION);
	if (cvalue)
		return g_value_get_string (cvalue);
	else
		return NULL;
}

/**
 * gda_column_set_description:
 * @column: a #GdaColumn.
 * @title: title name.
 *
 * Sets the column's description
 */
void
gda_column_set_description (GdaColumn *column, const gchar *descr)
{
	GValue *value = NULL;

	g_return_if_fail (GDA_IS_COLUMN (column));

	if (descr)
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), descr);
	gda_column_set_attribute_static (column, GDA_ATTRIBUTE_DESCRIPTION, value);
	if (value)
		gda_value_free (value);
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
	return (const gchar *) column->priv->dbms_type;
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
	
	if (column->priv->dbms_type != NULL) {
		g_free (column->priv->dbms_type);
		column->priv->dbms_type = NULL;
	}
	
	if (dbms_type)
		column->priv->dbms_type = g_strdup (dbms_type);
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
	return column->priv->g_type;
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

	old_type = column->priv->g_type;
	column->priv->g_type = type;

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
	return column->priv->allow_null;
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
	column->priv->allow_null = allow;
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
	return column->priv->auto_increment;
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
	column->priv->auto_increment = is_auto;
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
	return column->priv->position;
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
	column->priv->position = position;
}


/**
 * gda_column_get_default_value:
 * @column: a #GdaColumn.
 *
 * Returns: (allow-none): @column's default value, as a #GValue object, or %NULL if column does not have a default value
 */
const GValue *
gda_column_get_default_value (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return (const GValue *) column->priv->default_value;
}

/**
 * gda_column_set_default_value:
 * @column: a #GdaColumn.
 * @default_value: (allow-none): default #GValue for the column
 *
 * Sets @column's default #GValue.
 */
void
gda_column_set_default_value (GdaColumn *column, const GValue *default_value)
{
	g_return_if_fail (GDA_IS_COLUMN (column));

	gda_value_free (column->priv->default_value);
	if (default_value)
		column->priv->default_value = gda_value_copy ( (GValue*)default_value);
	else
		column->priv->default_value = NULL;
}

/**
 * gda_column_get_attribute:
 * @column: a #GdaColumn
 * @attribute: attribute name as a string
 *
 * Get the value associated to a named attribute.
 *
 * Attributes can have any name, but Libgda proposes some default names, see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * Returns: a read-only #GValue, or %NULL if not attribute named @attribute has been set for @column
 */
const GValue *
gda_column_get_attribute (GdaColumn *column, const gchar *attribute)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return gda_attributes_manager_get (gda_column_attributes_manager, column, attribute);
}

/**
 * gda_column_set_attribute:
 * @column: a #GdaColumn
 * @attribute: attribute name as a static string
 * @value: a #GValue, or %NULL
 * @destroy: a function to be called when @attribute is not needed anymore, or %NULL
 *
 * Set the value associated to a named attribute. The @attribute string is 'stolen' by this method, and
 * the memory it uses will be freed using the @destroy function when no longer needed (if @destroy is %NULL,
 * then the string will not be freed at all).
 *
 * Attributes can have any name, but Libgda proposes some default names, 
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * If there is already an attribute named @attribute set, then its value is replaced with the new value (@value is
 * copied), except if @value is %NULL, in which case the attribute is removed.
 *
 * For example one would use it as:
 *
 * <code>
 * gda_column_set_attribute (holder, g_strdup (my_attribute), g_free, my_value);
 * gda_column_set_attribute (holder, GDA_ATTRIBUTE_NAME, NULL, my_value);
 * </code>
 *
 * Note: this method does not modify in any way the contents of the data model for which @column is a column (nor
 * does it modify the table definition of the tables used by a SELECT statement is the model was created from a
 * SELECT statement).
 */
void
gda_column_set_attribute (GdaColumn *column, const gchar *attribute, const GValue *value, GDestroyNotify destroy)
{
	const GValue *cvalue;
	g_return_if_fail (GDA_IS_COLUMN (column));

	cvalue = gda_attributes_manager_get (gda_column_attributes_manager, column, attribute);
	if ((value && cvalue && !gda_value_differ (cvalue, value)) ||
	    (!value && !cvalue))
		return;

	gda_attributes_manager_set_full (gda_column_attributes_manager, column, attribute, value, destroy);
}
