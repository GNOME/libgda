/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2012 Murray Cumming <murrayc@murrayc.com>
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
#include "browser-canvas.h"
#include "browser-canvas-text.h"

static void browser_canvas_text_class_init (BrowserCanvasTextClass * class);
static void browser_canvas_text_init       (BrowserCanvasText *text);
static void browser_canvas_text_dispose    (GObject   * object);
static void browser_canvas_text_finalize   (GObject   * object);

static void browser_canvas_text_set_property    (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void browser_canvas_text_get_property    (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

static gboolean enter_notify_cb (GooCanvasItem *item, GooCanvasItem *target_item, GdkEventCrossing *event, BrowserCanvasText *ct);
static gboolean leave_notify_cb (GooCanvasItem *item, GooCanvasItem *target_item, GdkEventCrossing *event, BrowserCanvasText *ct);

enum
{
	PROP_0,
	PROP_TEXT,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_HIGHLIGHT_COLOR,
	PROP_UNDERLINE,
	PROP_BOLD
};

struct _BrowserCanvasTextPrivate
{
	gchar                *text;
	
	/* properties */
	gboolean              underline;
	gboolean              bold;
	gchar                *highlight_color;

	/* UI building information */
        GooCanvasItem        *bg_item;
        GooCanvasItem        *text_item;

	/* animation */
	guint                 anim_id;
	guint                 current_anim_rgba;
	guint                 end_anim_rgba;
	
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *text_parent_class = NULL;

GType
browser_canvas_text_get_type (void)
{
	static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasTextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_text_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvasText),
			0,
			(GInstanceInitFunc) browser_canvas_text_init,
			0
		};		

		type = g_type_register_static (TYPE_BROWSER_CANVAS_ITEM, "BrowserCanvasText", &info, 0);
	}

	return type;
}	

static void
browser_canvas_text_class_init (BrowserCanvasTextClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	text_parent_class = g_type_class_peek_parent (class);

	object_class->dispose = browser_canvas_text_dispose;
	object_class->finalize = browser_canvas_text_finalize;

	/* Properties */
	object_class->set_property = browser_canvas_text_set_property;
	object_class->get_property = browser_canvas_text_get_property;

	g_object_class_install_property 
		(object_class, PROP_WIDTH,
		 g_param_spec_double ("width", NULL, NULL, 0., G_MAXDOUBLE, 0., G_PARAM_WRITABLE));

	g_object_class_install_property 
		(object_class, PROP_HEIGHT,
		 g_param_spec_double ("height", NULL, NULL, 0., G_MAXDOUBLE, 0., G_PARAM_WRITABLE));

	g_object_class_install_property 
		(object_class, PROP_TEXT,
		 g_param_spec_string ("text", NULL, NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property 
		(object_class, PROP_HIGHLIGHT_COLOR,
		 g_param_spec_string ("highlight_color", NULL, NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property 
		(object_class, PROP_UNDERLINE,
		 g_param_spec_boolean ("text_underline", NULL, NULL, FALSE, (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property 
		(object_class, PROP_BOLD,
		 g_param_spec_boolean ("text_bold", NULL, NULL, FALSE, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
browser_canvas_text_init (BrowserCanvasText *text)
{
	text->priv = g_new0 (BrowserCanvasTextPrivate, 1);
	text->priv->text = NULL;
	text->priv->highlight_color = g_strdup (BROWSER_CANVAS_ENTITY_COLOR);

	g_signal_connect (G_OBJECT (text), "enter-notify-event", 
			  G_CALLBACK (enter_notify_cb), text);
	g_signal_connect (G_OBJECT (text), "leave-notify-event", 
			  G_CALLBACK (leave_notify_cb), text);
}

static void
browser_canvas_text_dispose (GObject *object)
{
	BrowserCanvasText *ct;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_TEXT (object));

	ct = BROWSER_CANVAS_TEXT (object);

	/* animation */
	if (ct->priv->anim_id) {
		g_source_remove (ct->priv->anim_id);
		ct->priv->anim_id = 0;
	}

	/* for the parent class */
	text_parent_class->dispose (object);
}


static void
browser_canvas_text_finalize (GObject *object)
{
	BrowserCanvasText *ct;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_TEXT (object));

	ct = BROWSER_CANVAS_TEXT (object);
	if (ct->priv) {
		g_free (ct->priv->text);

		if (ct->priv->highlight_color)
			g_free (ct->priv->highlight_color);

		g_free (ct->priv);
		ct->priv = NULL;
	}

	/* for the parent class */
	text_parent_class->finalize (object);
}

static void clean_items (BrowserCanvasText *ct);
static void create_items (BrowserCanvasText *ct);

static void
adjust_text_pango_attributes (BrowserCanvasText *ct)
{
	if (! ct->priv->text_item)
		return;

	if (ct->priv->bold || ct->priv->underline) {
		gchar *str;
		if (ct->priv->bold) {
			if (ct->priv->underline)
				str = g_strdup_printf ("<b><u>%s</u></b>", ct->priv->text);
			else
				str = g_strdup_printf ("<b>%s</b>", ct->priv->text);
		}
		else
			str = g_strdup_printf ("<u>%s</u>", ct->priv->text);
		g_object_set (G_OBJECT (ct->priv->text_item),
			      "text", str,
			      "use-markup", TRUE, NULL);
		g_free (str);
	}
	else
		g_object_set (G_OBJECT (ct->priv->text_item),
			      "text", ct->priv->text,
			      "use-markup", FALSE, NULL);
}

static void 
browser_canvas_text_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	BrowserCanvasText *ct = NULL;
	const gchar *cstr = NULL;
	gchar *str;
	gdouble size = 0;
	gboolean bool_value = FALSE;

	ct = BROWSER_CANVAS_TEXT (object);

	switch (param_id) {
	case PROP_TEXT:
		g_free (ct->priv->text);
		ct->priv->text = NULL;
		clean_items (ct);
		ct->priv->text = g_strdup (g_value_get_string (value));
		create_items (ct);
		break;
	case PROP_WIDTH:
		size = g_value_get_double (value);
		if (ct->priv->bg_item)
			g_object_set (G_OBJECT (ct->priv->bg_item),
				      "width", size,
				      NULL);
		break;
	case PROP_HEIGHT:
		size = g_value_get_double (value);
		if (ct->priv->bg_item)
			g_object_set (G_OBJECT (ct->priv->bg_item),
				      "height", size,
				      NULL);
		break;
	case PROP_HIGHLIGHT_COLOR:
		cstr = g_value_get_string (value);
		if (ct->priv->highlight_color) {
			g_free (ct->priv->highlight_color);
			ct->priv->highlight_color = NULL;
		}
		if (cstr) 
			ct->priv->highlight_color = g_strdup (cstr);
		else 
			ct->priv->highlight_color = g_strdup (BROWSER_CANVAS_ENTITY_COLOR);
		break;
	case PROP_UNDERLINE:
		bool_value = g_value_get_boolean (value);
		ct->priv->underline = bool_value;
		adjust_text_pango_attributes (ct);
		if (ct->priv->text_item) {
			if (bool_value) {
				str = g_strdup_printf ("<u>%s</u>", ct->priv->text);
				g_object_set (G_OBJECT (ct->priv->text_item), 
					      "text", str,
					      "use-markup", TRUE, NULL);
				g_free (str);
			}
			else 
				g_object_set (G_OBJECT (ct->priv->text_item), 
					      "text", ct->priv->text,
					      "use-markup", FALSE, NULL);
		}
		break;
	case PROP_BOLD:
		bool_value = g_value_get_boolean (value);
		ct->priv->bold = bool_value;
		adjust_text_pango_attributes (ct);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void 
browser_canvas_text_get_property    (GObject *object,
				    guint param_id,
				    G_GNUC_UNUSED GValue *value,
				    GParamSpec *pspec)
{
	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* 
 * destroy any existing GooCanvasItem obejcts 
 */
static void 
clean_items (BrowserCanvasText *ct)
{
	if (ct->priv->bg_item) {
		goo_canvas_item_remove (GOO_CANVAS_ITEM (ct->priv->bg_item));
		ct->priv->bg_item = NULL;
	}
	if (ct->priv->text_item) {
		goo_canvas_item_remove (GOO_CANVAS_ITEM (ct->priv->text_item));
		ct->priv->text_item = NULL;
	}
}

/*
 * create new GooCanvasItem objects
 */
static void 
create_items (BrowserCanvasText *ct)
{
	GooCanvasItem *item, *text;
	GooCanvasBounds bounds;

	g_object_set (G_OBJECT (ct), 
		      "allow_move", FALSE,
		      NULL);

	/* text: text's name */
	text = goo_canvas_text_new (GOO_CANVAS_ITEM (ct), ct->priv->text,
				    0., 0.,
				    -1, GOO_CANVAS_ANCHOR_NORTH_WEST, 
				    "fill_color", "black",
				    "font", BROWSER_CANVAS_FONT,
				    "alignment", PANGO_ALIGN_RIGHT, 
				    NULL);
	ct->priv->text_item = text;

	/* UI metrics */
	goo_canvas_item_get_bounds (text, &bounds);
	
	/* background */
	item = goo_canvas_rect_new (GOO_CANVAS_ITEM (ct),
				    0., 0., 
				    bounds.x2 - bounds.x1,
				    bounds.y2 - bounds.y1,
				    "fill_color", BROWSER_CANVAS_OBJ_BG_COLOR,
				    "radius-x", 2.,
				    "radius-y", 2.,
				    "stroke-pattern", NULL,
				    NULL);

	ct->priv->bg_item = item;
	goo_canvas_item_lower (item, NULL);

	adjust_text_pango_attributes (ct);	
}

static gboolean
enter_notify_cb (G_GNUC_UNUSED GooCanvasItem *item, G_GNUC_UNUSED GooCanvasItem *target_item,
		 G_GNUC_UNUSED GdkEventCrossing *event, BrowserCanvasText *ct)
{
	browser_canvas_text_set_highlight (ct, TRUE);
	return FALSE;
}

static gboolean 
leave_notify_cb (G_GNUC_UNUSED GooCanvasItem *item, G_GNUC_UNUSED GooCanvasItem *target_item,
		 G_GNUC_UNUSED GdkEventCrossing *event, BrowserCanvasText *ct)
{
	browser_canvas_text_set_highlight (ct, FALSE); 
	return FALSE;
}

static guint
compute_step_value (guint current, guint end)
{
	const guint STEP = 15;
	if (current < end)
		return current + MIN (STEP, (end - current));
	else if (current > end)
		return current - MIN (STEP, (current - end));
	else
		return current;
}

static gboolean
anim_cb (BrowserCanvasText *ct) 
{
	guint current, end, value;
	guint rgba = 0;

	/* red */
	current = (ct->priv->current_anim_rgba >> 24) & 0xFF;
	end = (ct->priv->end_anim_rgba >> 24) & 0xFF;
	value = compute_step_value (current, end) << 24;
	rgba += value;

	/* green */
	current = (ct->priv->current_anim_rgba >> 16) & 0xFF;
	end = (ct->priv->end_anim_rgba >> 16) & 0xFF;
	value = compute_step_value (current, end) << 16;
	rgba += value;

	/* blue */
	current = (ct->priv->current_anim_rgba >> 8) & 0xFF;
	end = (ct->priv->end_anim_rgba >> 8) & 0xFF;
	value = compute_step_value (current, end) << 8;
	rgba += value;

	/* alpha */
	current = ct->priv->current_anim_rgba & 0xFF;
	end = ct->priv->end_anim_rgba & 0xFF;
	value = compute_step_value (current, end);
	rgba += value;

	if (rgba == ct->priv->end_anim_rgba) {
		ct->priv->anim_id = 0;
		return FALSE;
	}
	else {
		g_object_set (G_OBJECT (ct->priv->bg_item),  "fill_color_rgba", rgba, NULL);
		ct->priv->current_anim_rgba = rgba;
		return TRUE;
	}
}

/**
 * browser_canvas_text_set_highlight
 * @ct: a #BrowserCanvasText object
 * @highlight:
 *
 * Turns ON or OFF the highlighting of @ct
 */
void 
browser_canvas_text_set_highlight (BrowserCanvasText *ct, gboolean highlight)
{
	gchar *str_color;
	GdkRGBA gdk_rgba;

	g_return_if_fail (ct && IS_BROWSER_CANVAS_TEXT (ct));
	g_return_if_fail (ct->priv);

	if (! ct->priv->bg_item)
		return;

	if (ct->priv->anim_id) {
		g_source_remove (ct->priv->anim_id);
		ct->priv->anim_id = 0;
	}

	str_color = highlight ? ct->priv->highlight_color : BROWSER_CANVAS_OBJ_BG_COLOR;
	if (gdk_rgba_parse (&gdk_rgba, str_color)) {
		guint col;

		col = ((guint) (gdk_rgba.red * 255.));
		ct->priv->end_anim_rgba = col << 24;
		col = ((guint) (gdk_rgba.green * 255.));
		ct->priv->end_anim_rgba += col << 16;
		col = ((guint) (gdk_rgba.blue * 255.));
		ct->priv->end_anim_rgba += col << 8;
		if (!ct->priv->current_anim_rgba)
			ct->priv->current_anim_rgba = ct->priv->end_anim_rgba;

		if (highlight)
			ct->priv->end_anim_rgba += 255;
		else
			ct->priv->end_anim_rgba += 50;
		
		ct->priv->anim_id = g_timeout_add (10, (GSourceFunc) anim_cb, ct);
	}
	else 
		g_object_set (G_OBJECT (ct->priv->bg_item),  "fill_color", str_color, NULL);
}

/**
 * browser_canvas_text_new
 * @parent: the parent item, or NULL. 
 * @txt: text to display
 * @x: the x coordinate of the text.
 * @y: the y coordinate of the text.
 * @...: optional pairs of property names and values, and a terminating NULL.
 *
 * Creates a new canvas item to display the @txt message
 *
 * Returns: a new #GooCanvasItem object
 */
GooCanvasItem *
browser_canvas_text_new (GooCanvasItem *parent,
		       const gchar *txt,     
		       gdouble x,
		       gdouble y,
		       ...)
{
	GooCanvasItem *item;
	const char *first_property;
	va_list var_args;
		
	item = g_object_new (TYPE_BROWSER_CANVAS_TEXT, NULL);

	if (parent) {
		goo_canvas_item_add_child (parent, item, -1);
		g_object_unref (item);
	}

	va_start (var_args, y);
	first_property = va_arg (var_args, char*);
	if (first_property)
		g_object_set_valist ((GObject*) item, first_property, var_args);
	va_end (var_args);

	g_object_set (item, "text", txt, NULL);
	goo_canvas_item_translate (item, x, y);

	return item;
}
