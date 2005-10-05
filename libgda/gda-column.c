/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <libgda/gda-column.h>
#include <string.h>
#include "gda-marshal.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaColumnPrivate {
	gint         defined_size;
	gchar       *name;
	gchar       *title;

	gchar       *table;
	gchar       *caption;
	gint         scale;

	gchar       *dbms_type;
	GdaValueType gda_type;

	gboolean     allow_null;
	gboolean     primary_key;
	gboolean     unique_key;
	gchar       *references;

	gboolean     auto_increment;
	glong        auto_increment_start;
	glong        auto_increment_step;
	gint         position;
	GdaValue    *default_value;
};

enum {
	NAME_CHANGED,
	GDA_TYPE_CHANGED,
	LAST_SIGNAL
};

static void gda_column_class_init (GdaColumnClass *klass);
static void gda_column_init       (GdaColumn *column, GdaColumnClass *klass);
static void gda_column_finalize   (GObject *object);

static guint gda_column_signals[LAST_SIGNAL] = {0 , 0};
static GObjectClass *parent_class = NULL;

static void
gda_column_class_init (GdaColumnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);
	
	gda_column_signals[NAME_CHANGED] =
		g_signal_new ("name_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaColumnClass, name_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
	gda_column_signals[GDA_TYPE_CHANGED] =
		g_signal_new ("gda_type_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaColumnClass, gda_type_changed),
			      NULL, NULL,
			      gda_marshal_VOID__INT_INT,
			      G_TYPE_NONE,
			      2, G_TYPE_INT, G_TYPE_INT);

	object_class->finalize = gda_column_finalize;
}

static void
gda_column_init (GdaColumn *column, GdaColumnClass *klass)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	
	column->priv = g_new0 (GdaColumnPrivate, 1);
	column->priv->defined_size = 0;
	column->priv->name = NULL;
	column->priv->table = NULL;
	column->priv->title = NULL;
	column->priv->caption = NULL;
	column->priv->scale = 0;
	column->priv->gda_type = GDA_VALUE_TYPE_UNKNOWN;
	column->priv->allow_null = TRUE;
	column->priv->primary_key = FALSE;
	column->priv->unique_key = FALSE;
	column->priv->references = NULL;
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
		if (column->priv->default_value) 
			gda_value_free (column->priv->default_value);
	
		g_free (column->priv);
		column->priv = NULL;
	}
	
	parent_class->finalize (object);
}

GType
gda_column_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaColumnClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_column_class_init,
			NULL,
			NULL,
			sizeof (GdaColumn),
			0,
			(GInstanceInitFunc) gda_column_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaColumn", &info, 0);
	}
	
	return type;
}

/**
 * gda_column_new
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

/** 	 /**
  * gda_column_copy 	 
  * @column: column to get a copy from. 	 
  * 	 
  * Creates a new #GdaColumn object from an existing one. 	 
  * Returns: a newly allocated #GdaColumn with a copy of the data 	 
  * in @column. 	 
  */ 	 
 GdaColumn * 	 
 gda_column_copy (GdaColumn *column) 	 
 { 	 
         GdaColumn *column_copy; 	 
  	 
         g_return_val_if_fail (GDA_IS_COLUMN (column), NULL); 	 
  	 
         column_copy = gda_column_new (); 	 
         column_copy->priv->defined_size = column->priv->defined_size;
	 if (column->priv->name)
		 column_copy->priv->name = g_strdup (column->priv->name);
	 if (column->priv->title)
		 column_copy->priv->name = g_strdup (column->priv->title);
	 if (column->priv->table)
		 column_copy->priv->table = g_strdup (column->priv->table);
	 if (column->priv->caption)
		 column_copy->priv->caption = g_strdup (column->priv->caption);
         column_copy->priv->scale = column->priv->scale;
         column_copy->priv->gda_type = column->priv->gda_type;
         column_copy->priv->allow_null = column->priv->allow_null;
         column_copy->priv->primary_key = column->priv->primary_key;
         column_copy->priv->unique_key = column->priv->unique_key;
	 if (column->priv->references)
		 column_copy->priv->references = g_strdup (column->priv->references);
         column_copy->priv->auto_increment = column->priv->auto_increment;
         column_copy->priv->auto_increment_start = column->priv->auto_increment_start;
         column_copy->priv->auto_increment_step = column->priv->auto_increment_step;
         column_copy->priv->position = column->priv->position;
	 if (column->priv->default_value)
		 column_copy->priv->default_value = gda_value_copy (column->priv->default_value);
  	 
         return column_copy; 	 
 }

/** 	 
 * gda_column_equal: 	 
 * @lhs: a #GdaColumn 	 
 * @rhs: another #GdaColumn 	 
 * 	 
 * Tests whether two colums are equal. 	 
 * 	 
 * Return value: %TRUE if the columns contain the same information. 	 
 */
gboolean
gda_column_equal (const GdaColumn *lhs, const GdaColumn *rhs) 	 
{ 	 
	g_return_val_if_fail (GDA_IS_COLUMN (lhs), FALSE); 	 
	g_return_val_if_fail (GDA_IS_COLUMN (rhs), FALSE); 	 
  	 
	/* Compare every struct field: */ 	 
	if ((lhs->priv->defined_size != rhs->priv->defined_size) || 	 
	    (lhs->priv->scale != rhs->priv->scale) || 	 
	    (lhs->priv->gda_type != rhs->priv->gda_type) || 	 
	    (lhs->priv->allow_null != rhs->priv->allow_null) || 	 
	    (lhs->priv->primary_key != rhs->priv->primary_key) || 	 
	    (lhs->priv->unique_key != rhs->priv->unique_key) || 	 
	    (lhs->priv->auto_increment != rhs->priv->auto_increment) || 	 
	    (lhs->priv->auto_increment_step != rhs->priv->auto_increment_step) || 	 
	    (lhs->priv->position != rhs->priv->position)) 	 
		return FALSE; 	 
  	 
	/* Check the strings if they have are not null. 	 
	   Then check whether one is null while the other is not, because strcmp can not do that. */ 	 
	if ((lhs->priv->name && rhs->priv->name) 	 
	    && (strcmp (lhs->priv->name, rhs->priv->name) != 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->name == 0) != (rhs->priv->name == 0)) 	 
		return FALSE; 	 

	if ((lhs->priv->title && rhs->priv->title) 	 
	    && (strcmp (lhs->priv->title, rhs->priv->title) != 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->title == 0) != (rhs->priv->title == 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->table && rhs->priv->table) 	 
	    && (strcmp (lhs->priv->table, rhs->priv->table) != 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->table == 0) != (rhs->priv->table == 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->caption && rhs->priv->caption) 	 
	    && (strcmp (lhs->priv->caption, rhs->priv->caption) != 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->caption == 0) != (rhs->priv->caption == 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->references && rhs->priv->references) 	 
	    && (strcmp (lhs->priv->references, rhs->priv->references) != 0)) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->references == 0) != (rhs->priv->references == 0)) 	 
		return FALSE; 	 
  	 
	if (lhs->priv->default_value 	 
	    && rhs->priv->default_value 	 
	    && gda_value_compare (lhs->priv->default_value, rhs->priv->default_value) != 0) 	 
		return FALSE; 	 
  	 
	if ((lhs->priv->default_value == 0) != (rhs->priv->default_value == 0)) 	 
		return FALSE; 	 
  	 
	return TRUE; 	 
 }

/**
 * gda_column_get_defined_size
 * @column: a @GdaColumn.
 *
 * Returns: the defined size of @column.
 */
glong
gda_column_get_defined_size (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), 0);
	return column->priv->defined_size;
}

/**
 * gda_column_set_defined_size
 * @column: a #GdaColumn.
 * @size: the defined size we want to set.
 *
 * Sets the defined size of a #GdaColumn.
 */
void
gda_column_set_defined_size (GdaColumn *column, glong size)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	column->priv->defined_size = size;
}

/**
 * gda_column_get_name
 * @column: a #GdaColumn.
 *
 * Returns: the name of @column.
 */
const gchar *
gda_column_get_name (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return (const gchar *) column->priv->name;
}

/**
 * gda_column_set_name
 * @column: a #GdaColumn.
 * @name: the new name of @column.
 *
 * Sets the name of @column to @name.
 */
void
gda_column_set_name (GdaColumn *column, const gchar *name)
{
	gchar *old_name = NULL;

	g_return_if_fail (GDA_IS_COLUMN (column));

	if (column->priv->name) {
		old_name = column->priv->name;
		column->priv->name = NULL;
	}

	if (name)
		column->priv->name = g_strdup (name);

	g_signal_emit (G_OBJECT (column),
		       gda_column_signals[NAME_CHANGED],
		       0, old_name);

	if (old_name)
		g_free (old_name);
}

/**
 * gda_column_get_title
 * @column: a #GdaColumn.
 *
 * Returns: the column's title
 */
const gchar *
gda_column_get_title (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return column->priv->title;;
}

/**
 * gda_column_set_title
 * @column: a #GdaColumn.
 * @title: title name.
 *
 * Sets the column's title
 */
void
gda_column_set_title (GdaColumn *column, const gchar *title)
{
	g_return_if_fail (GDA_IS_COLUMN (column));

	if (column->priv->title != NULL)
		g_free (column->priv->title);
	column->priv->title = g_strdup (title);
}


/**
 * gda_column_get_table
 * @column: a #GdaColumn.
 *
 * Returns: the name of the table to which this column belongs.
 */
const gchar *
gda_column_get_table (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return column->priv->table;;
}

/**
 * gda_column_set_table
 * @column: a #GdaColumn.
 * @table: table name.
 *
 * Sets the name of the table to which the given column belongs.
 */
void
gda_column_set_table (GdaColumn *column, const gchar *table)
{
	g_return_if_fail (GDA_IS_COLUMN (column));

	if (column->priv->table != NULL)
		g_free (column->priv->table);
	column->priv->table = g_strdup (table);
}

/**
 * gda_column_get_caption
 * @column: a #GdaColumn.
 *
 * Returns: @column's caption.
 */
const gchar *
gda_column_get_caption (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return (const gchar *) column->priv->caption;
}

/**
 * gda_column_set_caption
 * @column: a #GdaColumn.
 * @caption: caption.
 *
 * Sets @column's @caption.
 */
void
gda_column_set_caption (GdaColumn *column, const gchar *caption)
{
	g_return_if_fail (GDA_IS_COLUMN (column));

	if (column->priv->caption)
		g_free (column->priv->caption);
	column->priv->caption = g_strdup (caption);
}

/**
 * gda_column_get_scale
 * @column: a #GdaColumn.
 *
 * Returns: the number of decimals of @column.
 */
glong
gda_column_get_scale (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), 0);
	return column->priv->scale;
}

/**
 * gda_column_set_scale
 * @column: a #GdaColumn.
 * @scale: number of decimals.
 *
 * Sets the scale of @column to @scale.
 */
void
gda_column_set_scale (GdaColumn *column, glong scale)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	column->priv->scale = scale;
}

/**
 * gda_column_get_dbms_type
 * @column: a #GdaColumn.
 *
 * Returns: the dbms_type of @column.
 */
const gchar*
gda_column_get_dbms_type (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return (const gchar *) column->priv->dbms_type;
}

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
 * gda_column_get_gdatype
 * @column: a #GdaColumn.
 *
 * Returns: the type of @column.
 */
GdaValueType
gda_column_get_gdatype (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), GDA_VALUE_TYPE_NULL);
	return column->priv->gda_type;
}

/**
 * gda_column_set_gdatype
 * @column: a #GdaColumn.
 * @type: the new type of @column.
 *
 * Sets the type of @column to @type.
 */
void
gda_column_set_gdatype (GdaColumn *column, GdaValueType type)
{
	GdaValueType old_type;

	g_return_if_fail (GDA_IS_COLUMN (column));

	old_type = column->priv->gda_type;
	column->priv->gda_type = type;

	g_signal_emit (G_OBJECT (column),
		       gda_column_signals[GDA_TYPE_CHANGED],
		       0, old_type, type);
}

/**
 * gda_column_get_allow_null
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
 * gda_column_set_allow_null
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
 * gda_column_get_primary_key
 * @column: a #GdaColumn.
 *
 * Returns: whether if the given column is a primary key (%TRUE or %FALSE).
 */
gboolean
gda_column_get_primary_key (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), FALSE);
	return column->priv->primary_key;
}

/**
 * gda_column_set_primary_key
 * @column: a #GdaColumn.
 * @pk: whether if the given column should be a primary key.
 *
 * Sets the 'primary key' flag of the given column.
 */
void
gda_column_set_primary_key (GdaColumn *column, gboolean pk)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	column->priv->primary_key = pk;
}

/**
 * gda_column_get_unique_key
 * @column: a #GdaColumn.
 *
 * Returns: whether if the given column is an unique key (%TRUE or %FALSE).
 */
gboolean
gda_column_get_unique_key (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), FALSE);
	return column->priv->unique_key;
}

/**
 * gda_column_set_unique_key
 * @column: a #GdaColumn.
 * @uk: whether if the given column should be an unique key.
 *
 * Sets the 'unique key' flag of the given column.
 */
void
gda_column_set_unique_key (GdaColumn *column, gboolean uk)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	column->priv->unique_key = uk;
}

/**
 * gda_column_get_references
 * @column: a #GdaColumn.
 *
 * Reference is returned in tablename.fieldname format. Do not free
 * this variable, it is used internally within GdaColumn.
 *
 * Returns: @column's references.
 */
const gchar *
gda_column_get_references (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return (const gchar *) column->priv->references;
}

/**
 * gda_column_set_references
 * @column: a #GdaColumn.
 * @ref: references.
 *
 * Sets @column's @references.
 */
void
gda_column_set_references (GdaColumn *column, const gchar *ref)
{
	g_return_if_fail (GDA_IS_COLUMN (column));

	if (column->priv->references != NULL) {
		g_free (column->priv->references);
		column->priv->references = NULL;
	}

	if (ref)
		column->priv->references = g_strdup (ref);
}

/**
 * gda_column_get_auto_increment
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
 * gda_column_set_auto_increment
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
 * gda_column_get_position
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
 * gda_column_set_position
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
 * gda_column_get_default_value
 * @column: a #GdaColumn.
 *
 * Returns: @column's default value, as a #GdaValue object.
 */
const GdaValue *
gda_column_get_default_value (GdaColumn *column)
{
	g_return_val_if_fail (GDA_IS_COLUMN (column), NULL);
	return (const GdaValue *) column->priv->default_value;
}

/**
 * gda_column_set_default_value
 * @column: a #GdaColumn.
 * @default_value: default #GdaValue for the column
 *
 * Sets @column's default #GdaValue.
 */
void
gda_column_set_default_value (GdaColumn *column, const GdaValue *default_value)
{
	g_return_if_fail (GDA_IS_COLUMN (column));
	g_return_if_fail (default_value != NULL);

	if (column->priv->default_value)
		g_free (column->priv->default_value);
	column->priv->default_value = gda_value_copy ( (GdaValue*)default_value);
}
