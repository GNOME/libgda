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

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "browser-canvas.h"
#include "browser-canvas-priv.h"
#include "browser-canvas-table.h"
#include "browser-canvas-column.h"
#include <glib/gi18n-lib.h>
#include <string.h>

static void browser_canvas_table_class_init (BrowserCanvasTableClass *class);
static void browser_canvas_table_init       (BrowserCanvasTable *drag);
static void browser_canvas_table_dispose    (GObject *object);
static void browser_canvas_table_finalize   (GObject *object);

static void browser_canvas_table_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void browser_canvas_table_get_property (GObject *object,
					       guint param_id,
					       GValue *value,
					       GParamSpec *pspec);

static void browser_canvas_table_drag_data_get (BrowserCanvasItem *citem, GdkDragContext *drag_context,
						GtkSelectionData *data, guint info, guint time);
static void browser_canvas_table_set_selected (BrowserCanvasItem *citem, gboolean selected);

static xmlNodePtr browser_canvas_table_serialize (BrowserCanvasItem *citem);

enum
{
	PROP_0,
	PROP_META_STRUCT,
	PROP_TABLE,
	PROP_MENU_FUNC
};

struct _BrowserCanvasTablePrivate
{
	GdaMetaStruct      *mstruct;
	GdaMetaTable       *table;

	/* UI building information */
        GSList             *column_items; /* list of GooCanvasItem for the columns */
	GSList             *other_items; /* list of GooCanvasItem for other purposes */
	gdouble            *column_ypos; /* array for each column's Y position in this canvas group */
	GtkWidget          *(*popup_menu_func) (BrowserCanvasTable *ce);

	GooCanvasItem      *selection_mark;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *table_parent_class = NULL;

GType
browser_canvas_table_get_type (void)
{
	static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasTableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_table_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvasTable),
			0,
			(GInstanceInitFunc) browser_canvas_table_init,
			0
		};		

		type = g_type_register_static (TYPE_BROWSER_CANVAS_ITEM, "BrowserCanvasTable", &info, 0);
	}

	return type;
}

	
static void
browser_canvas_table_class_init (BrowserCanvasTableClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	BrowserCanvasItemClass *iclass = BROWSER_CANVAS_ITEM_CLASS (class);

	table_parent_class = g_type_class_peek_parent (class);
	iclass->drag_data_get = browser_canvas_table_drag_data_get;
	iclass->set_selected = browser_canvas_table_set_selected;
	iclass->serialize = browser_canvas_table_serialize;

	object_class->dispose = browser_canvas_table_dispose;
	object_class->finalize = browser_canvas_table_finalize;

	/* Properties */
	object_class->set_property = browser_canvas_table_set_property;
	object_class->get_property = browser_canvas_table_get_property;

	g_object_class_install_property
                (object_class, PROP_META_STRUCT,
                 g_param_spec_object ("meta-struct", NULL, NULL, 
				      GDA_TYPE_META_STRUCT,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property
                (object_class, PROP_TABLE,
                 g_param_spec_pointer ("table", NULL, NULL,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property 
		(object_class, PROP_MENU_FUNC,
                 g_param_spec_pointer ("popup_menu_func", "Popup menu function", 
				       "Function to create a popup menu on each BrowserCanvasTable", 
				       G_PARAM_WRITABLE));
}

static gboolean button_press_event_cb (BrowserCanvasTable *ce, GooCanvasItem *target_item, GdkEventButton *event,
				       gpointer unused_data);

static void
browser_canvas_table_init (BrowserCanvasTable *table)
{
	table->priv = g_new0 (BrowserCanvasTablePrivate, 1);
	table->priv->mstruct = NULL;
	table->priv->table = NULL;
	table->priv->column_ypos = NULL;
	table->priv->popup_menu_func = NULL;

	table->priv->selection_mark = NULL;

	g_signal_connect (G_OBJECT (table), "button-press-event",
			  G_CALLBACK (button_press_event_cb), NULL);
}

static void clean_items (BrowserCanvasTable *ce);
static void create_items (BrowserCanvasTable *ce);

static void
browser_canvas_table_dispose (GObject *object)
{
	BrowserCanvasTable *ce;

	g_return_if_fail (IS_BROWSER_CANVAS_TABLE (object));

	ce = BROWSER_CANVAS_TABLE (object);

	/* REM: let the GooCanvas library destroy the items itself */
	ce->priv->table = NULL;
	if (ce->priv->mstruct) {
		g_object_unref (ce->priv->mstruct);
		ce->priv->mstruct = NULL;
	}

	/* for the parent class */
	table_parent_class->dispose (object);
}


static void
browser_canvas_table_finalize (GObject *object)
{
	BrowserCanvasTable *ce;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_TABLE (object));

	ce = BROWSER_CANVAS_TABLE (object);
	if (ce->priv) {
		g_slist_free (ce->priv->column_items);
		g_slist_free (ce->priv->other_items);
		if (ce->priv->column_ypos)
			g_free (ce->priv->column_ypos);

		g_free (ce->priv);
		ce->priv = NULL;
	}

	/* for the parent class */
	table_parent_class->finalize (object);
}

static void 
browser_canvas_table_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	BrowserCanvasTable *ce = NULL;

	ce = BROWSER_CANVAS_TABLE (object);

	switch (param_id) {
	case PROP_META_STRUCT:
		ce->priv->mstruct = g_value_dup_object (value);
		break;
	case PROP_TABLE: {
		GdaMetaTable *table;
		table = g_value_get_pointer (value);
		if (table && (table == ce->priv->table))
			return;

		if (ce->priv->table) {
			ce->priv->table = NULL;
			clean_items (ce);
		}

		if (table) {
			ce->priv->table = (GdaMetaTable*) table;
			create_items (ce);
		}
		break;
	}
	case PROP_MENU_FUNC:
		ce->priv->popup_menu_func = (GtkWidget *(*) (BrowserCanvasTable *ce)) g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void 
browser_canvas_table_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	BrowserCanvasTable *ce = NULL;

	ce = BROWSER_CANVAS_TABLE (object);

	switch (param_id) {
	case PROP_META_STRUCT:
		g_value_set_object (value, ce->priv->mstruct);
		break;
	case PROP_TABLE:
		g_value_set_pointer (value, ce->priv->table);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* 
 * destroy any existing GooCanvasItem obejcts 
 */
static void 
clean_items (BrowserCanvasTable *ce)
{
	GSList *list;
	/* destroy all the items in the group */
	while (ce->priv->column_items)
		g_object_unref (G_OBJECT (ce->priv->column_items->data));

	for (list = ce->priv->other_items; list; list = list->next)
		g_object_unref (G_OBJECT (list->data));
	g_slist_free (ce->priv->other_items);
	ce->priv->other_items = NULL;

	/* free the columns positions */
	if (ce->priv->column_ypos) {
		g_free (ce->priv->column_ypos);
		ce->priv->column_ypos = NULL;
	}
}

/*
 * create new GooCanvasItem objects
 */
static void 
create_items (BrowserCanvasTable *ce)
{
	GooCanvasItem *item, *frame, *title;
        gdouble y, ysep;
#define HEADER_Y_PAD 3.
#define Y_PAD 0.
#define X_PAD 3.
#define RADIUS_X 5.
#define RADIUS_Y 5.
#define MIN_HEIGHT 70.
#define SELECTION_SIZE 4.
        GooCanvasBounds border_bounds;
        GooCanvasBounds bounds;
	const gchar *cstr;
	gchar *tmpstr = NULL;
	GSList *columns, *list;
	gint column_nb;
	gdouble column_width;

	clean_items (ce);
	g_assert (ce->priv->table);

        /* title */
	cstr = GDA_META_DB_OBJECT (ce->priv->table)->obj_short_name;
	if (cstr)
		tmpstr = g_markup_printf_escaped ("<b>%s</b>", cstr);
	else
		tmpstr = g_strdup_printf ("<b>%s</b>", _("No name"));	

	y = RADIUS_Y;
        title = goo_canvas_text_new  (GOO_CANVAS_ITEM (ce), tmpstr,
				      RADIUS_X + X_PAD, y, 
				      -1, GOO_CANVAS_ANCHOR_NORTH_WEST,
				      "font", "Sans 11",
				      "use-markup", TRUE, NULL);

	g_free (tmpstr);
        goo_canvas_item_get_bounds (title, &bounds);
        border_bounds = bounds;
        border_bounds.x1 = 0.;
        border_bounds.y1 = 0.;
        y += bounds.y2 - bounds.y1 + HEADER_Y_PAD;

	/* separator's placeholder */
        ysep = y;
        y += HEADER_Y_PAD;

	/* columns' vertical position */
	columns = ce->priv->table->columns;
	ce->priv->column_ypos = g_new0 (gdouble, g_slist_length (columns) + 1);

	/* columns */
	for (column_nb = 0, list = columns; list; list = list->next, column_nb++) {
		ce->priv->column_ypos [column_nb] = y;
		item = browser_canvas_column_new (GOO_CANVAS_ITEM (ce),
						  ce->priv->mstruct,
						  GDA_META_TABLE_COLUMN (list->data),
						  X_PAD, ce->priv->column_ypos [column_nb], NULL);
		ce->priv->column_items = g_slist_append (ce->priv->column_items, item);
		goo_canvas_item_get_bounds (item, &bounds);
		border_bounds.x1 = MIN (border_bounds.x1, bounds.x1);
                border_bounds.x2 = MAX (border_bounds.x2, bounds.x2);
                border_bounds.y1 = MIN (border_bounds.y1, bounds.y1);
                border_bounds.y2 = MAX (border_bounds.y2, bounds.y2);

                y += bounds.y2 - bounds.y1 + Y_PAD;
	}
	if (!columns && (border_bounds.y2 - border_bounds.y1 < MIN_HEIGHT))
		border_bounds.y2 += MIN_HEIGHT - (border_bounds.y2 - border_bounds.y1);

	/* border */
	column_width = border_bounds.x2 - border_bounds.x1;
        border_bounds.y2 += RADIUS_Y;
        border_bounds.x2 += RADIUS_X;
        frame = goo_canvas_rect_new (GOO_CANVAS_ITEM (ce), border_bounds.x1, border_bounds.y1, 
				     border_bounds.x2, border_bounds.y2,
				     "radius-x", RADIUS_X,
				     "radius-y", RADIUS_Y,
				     "fill-color", "#f8f8f8",
				     NULL);		
	ce->priv->other_items = g_slist_prepend (ce->priv->other_items, frame);

	ce->priv->selection_mark = goo_canvas_rect_new (GOO_CANVAS_ITEM (ce), border_bounds.x1 - SELECTION_SIZE,
							border_bounds.y1 - SELECTION_SIZE, 
							border_bounds.x2 + 2 * SELECTION_SIZE,
							border_bounds.y2 + 2 * SELECTION_SIZE,
							"radius-x", RADIUS_X,
							"radius-y", RADIUS_Y,
							"fill-color", "#11d155",//"#ffea08",
							"stroke-color", "#11d155",//"#ffea08",
							NULL);
	g_object_set (G_OBJECT (ce->priv->selection_mark), "visibility", GOO_CANVAS_ITEM_HIDDEN, NULL);

	/* title's background */
	gchar *cpath;
	cpath = g_strdup_printf ("M %d %d H %d V %d H %d Z",
				 (gint) border_bounds.x1, (gint) border_bounds.y1,
				 (gint) border_bounds.x2, (gint) ysep,
				 (gint) border_bounds.x1);
	item = goo_canvas_rect_new (GOO_CANVAS_ITEM (ce), border_bounds.x1, border_bounds.y1, 
				    border_bounds.x2, ysep + RADIUS_X,
				    "clip_path", cpath,
				    "radius-x", RADIUS_X,
				    "radius-y", RADIUS_Y,
				    "fill-color", "#aaaaff",
				    NULL);
	g_free (cpath);
	goo_canvas_item_lower (item, NULL);

	/* separator */
        item = goo_canvas_polyline_new_line (GOO_CANVAS_ITEM (ce), border_bounds.x1, ysep, border_bounds.x2, ysep,
					     "close-path", FALSE,
					     "line-width", .7, NULL);
	ce->priv->other_items = g_slist_prepend (ce->priv->other_items, item);

	goo_canvas_item_lower (frame, NULL);
	goo_canvas_item_lower (ce->priv->selection_mark, NULL);

	/* setting the columns' background width to be the same for all */
	for (list = ce->priv->column_items; list; list = list->next) 
		g_object_set (G_OBJECT (list->data), "width", column_width, NULL);
}

static gboolean
button_press_event_cb (BrowserCanvasTable *ce, G_GNUC_UNUSED GooCanvasItem  *target_item,
		       GdkEventButton *event,
		       G_GNUC_UNUSED gpointer data)
{
	if ((event->button == 3) && ce->priv->popup_menu_func) {
		GtkWidget *menu;
		menu = ce->priv->popup_menu_func (ce);
		gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
		return TRUE;
	}

	return FALSE;	
}

/**
 * browser_canvas_table_get_column_item
 * @ce: a #BrowserCanvasTable object
 * @column: a #GdaMetaTableColumn object
 *
 * Get the #BrowserCanvasColumn object representing @column
 * in @ce.
 *
 * Returns: the corresponding #BrowserCanvasColumn
 */
BrowserCanvasColumn *
browser_canvas_table_get_column_item (BrowserCanvasTable *ce, GdaMetaTableColumn *column)
{
	gint pos;

	g_return_val_if_fail (ce && IS_BROWSER_CANVAS_TABLE (ce), NULL);
	g_return_val_if_fail (ce->priv, NULL);
	g_return_val_if_fail (ce->priv->table, NULL);

	pos = g_slist_index (ce->priv->table->columns, column);
	g_return_val_if_fail (pos >= 0, NULL);

	return g_slist_nth_data (ce->priv->column_items, pos);
}


/**
 * browser_canvas_table_get_column_ypos
 * @ce: a #BrowserCanvasTable object
 * @column: a #GdaMetaTableColumn object
 *
 * Get the Y position of the middle of the #BrowserCanvasColumn object representing @column
 * in @ce, in @ce's coordinates.
 *
 * Returns: the Y coordinate.
 */
gdouble
browser_canvas_table_get_column_ypos (BrowserCanvasTable *ce, GdaMetaTableColumn *column)
{
	gint pos;

	g_return_val_if_fail (ce && IS_BROWSER_CANVAS_TABLE (ce), 0.);
	g_return_val_if_fail (ce->priv, 0.);
	g_return_val_if_fail (ce->priv->table, 0.);
	g_return_val_if_fail (ce->priv->column_ypos, 0.);

	pos = g_slist_index (ce->priv->table->columns, column);
	g_return_val_if_fail (pos >= 0, 0.);
	return (0.75 * ce->priv->column_ypos[pos+1] + 0.25 * ce->priv->column_ypos[pos]);
}


/**
 * browser_canvas_table_new
 * @parent: the parent item, or NULL. 
 * @table: a #GdaMetaTable to display
 * @x: the x coordinate
 * @y: the y coordinate
 * @...: optional pairs of property names and values, and a terminating NULL.
 *
 * Creates a new canvas item to display the @table table
 *
 * Returns: a new #GooCanvasItem object
 */
GooCanvasItem *
browser_canvas_table_new (GooCanvasItem *parent, GdaMetaStruct *mstruct, GdaMetaTable *table, 
			 gdouble x, gdouble y, ...)
{
	GooCanvasItem *item;
	const char *first_property;
	va_list var_args;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);

	item = g_object_new (TYPE_BROWSER_CANVAS_TABLE, "meta-struct", mstruct, 
			     "allow-move", TRUE,
			     "allow-select", TRUE, NULL);

	if (parent) {
		goo_canvas_item_add_child (parent, item, -1);
		g_object_unref (item);
	}

	g_object_set (item, "table", table, NULL);

	va_start (var_args, y);
	first_property = va_arg (var_args, char*);
	if (first_property)
		g_object_set_valist ((GObject*) item, first_property, var_args);
	va_end (var_args);

	goo_canvas_item_translate (item, x, y);

	return item;
}

static void
browser_canvas_table_drag_data_get (BrowserCanvasItem *citem, G_GNUC_UNUSED GdkDragContext *drag_context,
				    GtkSelectionData *data, G_GNUC_UNUSED guint info,
				    G_GNUC_UNUSED guint time)
{
	BrowserCanvasTable *ctable;

	ctable = BROWSER_CANVAS_TABLE (citem);
	if (!ctable->priv->table)
		return;

	GdaMetaDbObject *dbo;
	gchar *str, *tmp1, *tmp2, *tmp3;

	dbo = GDA_META_DB_OBJECT (ctable->priv->table);
	tmp1 = gda_rfc1738_encode (dbo->obj_schema);
	tmp2 = gda_rfc1738_encode (dbo->obj_name);
	tmp3 = gda_rfc1738_encode (dbo->obj_short_name);
	str = g_strdup_printf ("OBJ_TYPE=table;OBJ_SCHEMA=%s;OBJ_NAME=%s;OBJ_SHORT_NAME=%s", tmp1, tmp2, tmp3);
	g_free (tmp1);
	g_free (tmp2);
	g_free (tmp3);
	gtk_selection_data_set (data, gtk_selection_data_get_target (data), 8, (guchar*) str, strlen (str));
	g_free (str);
}

static void
browser_canvas_table_set_selected (BrowserCanvasItem *citem, gboolean selected)
{
	g_object_set (G_OBJECT (BROWSER_CANVAS_TABLE (citem)->priv->selection_mark),
		      "visibility", selected ? GOO_CANVAS_ITEM_VISIBLE : GOO_CANVAS_ITEM_HIDDEN, NULL);
}

static xmlNodePtr
browser_canvas_table_serialize (BrowserCanvasItem *citem)
{
	BrowserCanvasTable *ctable;

	ctable = BROWSER_CANVAS_TABLE (citem);
	if (!ctable->priv->table)
		return NULL;

	GdaMetaDbObject *dbo;
	xmlNodePtr node;
	GooCanvasBounds bounds;
	gchar *str;

	dbo = GDA_META_DB_OBJECT (ctable->priv->table);
	node = xmlNewNode (NULL, BAD_CAST "table");
	xmlSetProp (node, BAD_CAST "schema", BAD_CAST (dbo->obj_schema));
	xmlSetProp (node, BAD_CAST "name", BAD_CAST (dbo->obj_name));
	goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (citem), &bounds);
	str = g_strdup_printf ("%.1f", bounds.x1);
	xmlSetProp (node, BAD_CAST "x", BAD_CAST str);
	g_free (str);
	str = g_strdup_printf ("%.1f", bounds.y1);
	xmlSetProp (node, BAD_CAST "y", BAD_CAST str);
	g_free (str);
	
	return node;
}

/**
 * browser_canvas_table_get_anchor_bounds
 *
 * Get the bounds to be used to compute anchors, ie. without the selection mark or any other
 * artefact not part of the table's rectangle.
 */
void
browser_canvas_table_get_anchor_bounds (BrowserCanvasTable *ce, GooCanvasBounds *bounds)
{
	g_return_if_fail (IS_BROWSER_CANVAS_TABLE (ce));
	g_return_if_fail (bounds);

	goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ce), bounds);
	bounds->x1 += SELECTION_SIZE;
	bounds->y1 += SELECTION_SIZE;
	bounds->x2 -= SELECTION_SIZE;
	bounds->y2 -= SELECTION_SIZE;
}
