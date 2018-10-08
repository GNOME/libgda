/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include "browser-canvas.h"
#include "browser-canvas-column.h"
#include "browser-canvas-table.h"

static void browser_canvas_column_class_init (BrowserCanvasColumnClass * class);
static void browser_canvas_column_init       (BrowserCanvasColumn * drag);
static void browser_canvas_column_dispose   (GObject *object);

static void browser_canvas_column_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void browser_canvas_column_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

static void browser_canvas_column_drag_data_get (BrowserCanvasItem *citem, GdkDragContext *drag_context,
						 GtkSelectionData *data, guint info, guint time);

static void browser_canvas_column_extra_event  (BrowserCanvasItem * citem, GdkEventType event_type);
enum
{
	PROP_0,
	PROP_META_STRUCT,
	PROP_COLUMN,
};

struct _BrowserCanvasColumnPrivate
{
	GdaMetaStruct      *mstruct;
	GdaMetaTableColumn *column;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *column_parent_class = NULL;

GType
browser_canvas_column_get_type (void)
{
	static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasColumnClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_column_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvasColumn),
			0,
			(GInstanceInitFunc) browser_canvas_column_init,
			0
		};		

		type = g_type_register_static (TYPE_BROWSER_CANVAS_TEXT, "BrowserCanvasColumn", &info, 0);
	}

	return type;
}

	

static void
browser_canvas_column_class_init (BrowserCanvasColumnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BrowserCanvasItemClass *iclass = BROWSER_CANVAS_ITEM_CLASS (klass);

	column_parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = browser_canvas_column_dispose;

	iclass->drag_data_get = browser_canvas_column_drag_data_get;
	iclass->extra_event = browser_canvas_column_extra_event;

	/* Properties */
	object_class->set_property = browser_canvas_column_set_property;
	object_class->get_property = browser_canvas_column_get_property;

	g_object_class_install_property
                (object_class, PROP_META_STRUCT,
                 g_param_spec_object ("meta-struct", NULL, NULL, 
				      GDA_TYPE_META_STRUCT,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property
                (object_class, PROP_COLUMN,
                 g_param_spec_pointer ("column", NULL, NULL, 
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
browser_canvas_column_init (BrowserCanvasColumn * column)
{
	column->priv = g_new0 (BrowserCanvasColumnPrivate, 1);
	column->priv->mstruct = NULL;
	column->priv->column = NULL;
}

static void
browser_canvas_column_dispose (GObject   * object)
{
	BrowserCanvasColumn *cf;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_COLUMN (object));

	cf = BROWSER_CANVAS_COLUMN (object);
	if (cf->priv) {
		if (cf->priv->mstruct)
			g_object_unref (cf->priv->mstruct);
		g_free (cf->priv);
		cf->priv = NULL;
	}

	/* for the parent class */
	column_parent_class->dispose (object);
}

static void 
browser_canvas_column_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	BrowserCanvasColumn *cf = NULL;
	GdaMetaTableColumn* column = NULL;
	GString *string = NULL;

	cf = BROWSER_CANVAS_COLUMN (object);

	switch (param_id) {
	case PROP_META_STRUCT:
		cf->priv->mstruct = g_value_dup_object (value);
		break;
	case PROP_COLUMN:
		g_return_if_fail (cf->priv->mstruct);
		column = g_value_get_pointer (value);

		cf->priv->column = column;
		/* column name */
		g_object_set (object, "text", column->column_name, NULL);
		
		/* attributes setting */
		string = g_string_new ("");
		if (column->column_type)
			g_string_append_printf (string, _("Type: %s"), column->column_type);
		
		g_object_set (object, 
			      "highlight_color", BROWSER_CANVAS_DB_TABLE_COLOR, 
			      "text_underline", !column->nullok,
			      "text_bold", column->pkey,
			      NULL);
		
		if (*string->str)
			g_object_set (object, "tip-text", string->str, NULL);
		else
			g_object_set (object, "tip-text", NULL, NULL);
		g_string_free (string, TRUE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void 
browser_canvas_column_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	BrowserCanvasColumn *cf;

	cf = BROWSER_CANVAS_COLUMN (object);

	switch (param_id) {
	case PROP_META_STRUCT:
		g_value_set_object (value, cf->priv->mstruct);
		break;
	case PROP_COLUMN:
		g_value_set_pointer (value, cf->priv->column);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
browser_canvas_column_extra_event  (BrowserCanvasItem *citem, GdkEventType event_type)
{
	if (event_type == GDK_LEAVE_NOTIFY)
		browser_canvas_text_set_highlight (BROWSER_CANVAS_TEXT (citem), FALSE);
}

/**
 * browser_canvas_column_new:
 * @parent: (nullable): the parent item, or %NULL
 * @mstruct: the #GdaMetaStruct @column is from
 * @column: the represented entity's column
 * @x: the x coordinate of the text
 * @y: the y coordinate of the text
 * @...: optional pairs of property names and values, and a terminating %NULL.
 *
 * Creates a new #BrowserCanvasColumn object
 */
GooCanvasItem*
browser_canvas_column_new (GooCanvasItem *parent, GdaMetaStruct *mstruct, GdaMetaTableColumn *column,
			   gdouble x, gdouble y, ...)
{
	GooCanvasItem *item;
	const char *first_property;
	va_list var_args;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);

	item = g_object_new (TYPE_BROWSER_CANVAS_COLUMN, "meta-struct", mstruct, NULL);

	if (parent) {
		goo_canvas_item_add_child (parent, item, -1);
		g_object_unref (item);
	}

	va_start (var_args, y);
	first_property = va_arg (var_args, char*);
	if (first_property)
		g_object_set_valist ((GObject*) item, first_property, var_args);
	va_end (var_args);

	g_object_set (G_OBJECT (item), "column", column, NULL);
	goo_canvas_item_translate (item, x, y);
	
	return item;
}


/**
 * browser_canvas_column_get_column
 * @column: a #BrowserCanvasColumn object
 *
 * Get the #GdaMetaTableColumn which @column represents
 *
 * Returns: the object implementing the #GdaMetaTableColumn interface
 */
GdaMetaTableColumn *
browser_canvas_column_get_column (BrowserCanvasColumn *column)
{
	g_return_val_if_fail (IS_BROWSER_CANVAS_COLUMN (column), NULL);
	g_return_val_if_fail (column->priv, NULL);

	return column->priv->column;
}


/**
 * browser_canvas_column_get_parent_item
 * @column: a #BrowserCanvasColumn object
 *
 * Get the #BrowserCanvasTable in which @column is
 *
 * Returns: the #BrowserCanvasTable in which @column is, or %NULL
 */
BrowserCanvasTable *
browser_canvas_column_get_parent_item (BrowserCanvasColumn *column)
{
	GooCanvasItem *ci;

	g_return_val_if_fail (IS_BROWSER_CANVAS_COLUMN (column), NULL);
	for (ci = goo_canvas_item_get_parent (GOO_CANVAS_ITEM (column));
	     ci && !IS_BROWSER_CANVAS_TABLE (ci);
	     ci = goo_canvas_item_get_parent (ci));

	return (BrowserCanvasTable *) ci;
}

static void
browser_canvas_column_drag_data_get (BrowserCanvasItem *citem, G_GNUC_UNUSED GdkDragContext *drag_context,
				     GtkSelectionData *data, G_GNUC_UNUSED guint info,
				     G_GNUC_UNUSED guint time)
{
	BrowserCanvasColumn *column;
	BrowserCanvasTable *ctable;
	GdaMetaTable *mtable;

	column = BROWSER_CANVAS_COLUMN (citem);
	ctable = browser_canvas_column_get_parent_item (column);
	g_object_get (G_OBJECT (ctable), "table", &mtable, NULL);
	if (!column->priv->column || !mtable)
		return;

	GdaMetaDbObject *dbo;
	gchar *str, *tmp1, *tmp2, *tmp3, *tmp4;

	dbo = GDA_META_DB_OBJECT (mtable);
	tmp1 = gda_rfc1738_encode (dbo->obj_schema);
	tmp2 = gda_rfc1738_encode (dbo->obj_name);
	tmp3 = gda_rfc1738_encode (dbo->obj_short_name);
	tmp4 = gda_rfc1738_encode (column->priv->column->column_name);
	str = g_strdup_printf ("OBJ_TYPE=tablecolumn;OBJ_SCHEMA=%s;OBJ_NAME=%s;OBJ_SHORT_NAME=%s;COL_NAME=%s",
			       tmp1, tmp2, tmp3, tmp4);
	g_free (tmp1);
	g_free (tmp2);
	g_free (tmp3);
	g_free (tmp4);
	gtk_selection_data_set (data, gtk_selection_data_get_target (data), 8, (guchar*) str, strlen (str));
	g_free (str);
}
