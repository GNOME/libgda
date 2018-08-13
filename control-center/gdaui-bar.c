/*
 * Copyright (C) 2012 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include "gdaui-bar.h"

struct _GdauiBarPrivate
{
	GtkWidget *content_area;
	GtkWidget *action_area;

	GtkWidget *icon;
        gboolean   show_icon;
        GtkWidget *label;
};

/* Properties */
enum {
        PROP_0,
        PROP_TEXT,
        PROP_SHOW_ICON
};

static GObjectClass *parent_class = NULL;
static GtkCssProvider *css_provider = NULL;

#define ACTION_AREA_DEFAULT_BORDER 2
#define ACTION_AREA_DEFAULT_SPACING 2
#define CONTENT_AREA_DEFAULT_BORDER 5
#define CONTENT_AREA_DEFAULT_SPACING 2

static void gdaui_bar_class_init (GdauiBarClass *klass);
static void gdaui_bar_init (GdauiBar *bar);
static void gdaui_bar_set_property (GObject        *object,
				    guint           prop_id,
				    const GValue   *value,
				    GParamSpec     *pspec);
static void gdaui_bar_get_property (GObject        *object,
				    guint           prop_id,
				    GValue         *value,
				    GParamSpec     *pspec);
static void gdaui_bar_get_preferred_width (GtkWidget *widget,
					   gint      *minimum_width,
					   gint      *natural_width);
static void gdaui_bar_get_preferred_height (GtkWidget *widget,
					    gint *minimum_height,
					    gint *natural_height);
static gboolean gdaui_bar_draw (GtkWidget *widget, cairo_t *cr);
static void gdaui_bar_dispose (GObject *object);

GType
gdaui_bar_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static const GTypeInfo info = {
                        sizeof (GdauiBarClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gdaui_bar_class_init,
                        NULL,
                        NULL,
                        sizeof (GdauiBar),
                        0,
                        (GInstanceInitFunc) gdaui_bar_init,
                        0
                };
                type = g_type_register_static (GTK_TYPE_BOX, "GdauiBar", &info, 0);
        }
        return type;
}

static void
gdaui_bar_class_init (GdauiBarClass *klass)
{
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gdaui_bar_get_property;
	object_class->set_property = gdaui_bar_set_property;
	object_class->dispose = gdaui_bar_dispose;

	widget_class->get_preferred_width = gdaui_bar_get_preferred_width;
	widget_class->get_preferred_height = gdaui_bar_get_preferred_height;
	widget_class->draw = gdaui_bar_draw;

	/* add class properties */
        g_object_class_install_property (object_class, PROP_TEXT,
                                         g_param_spec_string ("text", NULL,
							      "Text showed inside the widget.", NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_SHOW_ICON,
                                         g_param_spec_boolean ("show_icon", NULL, NULL, FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
  if (css_provider == NULL) {
    css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (css_provider, "/gdaui/gdaui.css");
  }
}

static void
gdaui_bar_init (GdauiBar *bar)
{
	GtkWidget *widget = GTK_WIDGET (bar);
	GtkWidget *content_area;
	GtkWidget *action_area;

	bar->priv = g_new0 (GdauiBarPrivate, 1);

	content_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (content_area);
	gtk_box_pack_start (GTK_BOX (bar), content_area, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 6);

	GtkWidget *tmp;
	tmp = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (tmp);
	gtk_widget_set_valign (tmp, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (bar), tmp, FALSE, TRUE, 0);

	action_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (action_area);
	gtk_box_pack_start (GTK_BOX (tmp), action_area, FALSE, TRUE, 0);

	gtk_widget_set_app_paintable (widget, TRUE);
	gtk_widget_set_redraw_on_allocate (widget, TRUE);

	bar->priv->content_area = content_area;
	bar->priv->action_area = action_area;

	/* set default spacings */
	gtk_box_set_spacing (GTK_BOX (bar->priv->action_area), ACTION_AREA_DEFAULT_SPACING);
	gtk_container_set_border_width (GTK_CONTAINER (bar->priv->action_area), ACTION_AREA_DEFAULT_BORDER);
	gtk_box_set_spacing (GTK_BOX (bar->priv->content_area), CONTENT_AREA_DEFAULT_SPACING);
	gtk_container_set_border_width (GTK_CONTAINER (bar->priv->content_area), CONTENT_AREA_DEFAULT_BORDER);

	bar->priv->show_icon = FALSE;
	bar->priv->icon = gtk_image_new ();
	gtk_widget_set_halign (bar->priv->icon, GTK_ALIGN_END);
        gtk_widget_hide (bar->priv->icon);
        gtk_box_pack_end (GTK_BOX (bar->priv->content_area), bar->priv->icon,
			  FALSE, TRUE, 0);

	bar->priv->label = gtk_label_new ("");
        gtk_label_set_selectable (GTK_LABEL (bar->priv->label), FALSE);
	gtk_widget_set_halign (bar->priv->label, GTK_ALIGN_START);
        gtk_box_pack_end (GTK_BOX (bar->priv->content_area), bar->priv->label,
                          TRUE, TRUE, 0);
	gtk_label_set_line_wrap (GTK_LABEL (bar->priv->label), TRUE);
        gtk_widget_show (bar->priv->label);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (bar)), "inline-toolbar");
}

static void
gdaui_bar_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	GdauiBar *bar = GDAUI_BAR (object);

	switch (prop_id) {
	case PROP_TEXT:
		gdaui_bar_set_text (bar, g_value_get_string (value));
		break;
	case PROP_SHOW_ICON:
		gdaui_bar_set_show_icon (bar, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gdaui_bar_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
	GdauiBar *bar = GDAUI_BAR (object);

	switch (prop_id) {
	case PROP_TEXT:
		g_value_set_string (value, gdaui_bar_get_text (bar));
		break;
	case PROP_SHOW_ICON:
		g_value_set_boolean (value, gdaui_bar_get_show_icon (bar));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gdaui_bar_dispose (GObject *object)
{
	GdauiBar *bar = (GdauiBar*) object;
	if (bar->priv) {
		g_free (bar->priv);
		bar->priv = NULL;
	}
	G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
get_padding_and_border (GtkWidget *widget,
                        GtkBorder *border)
{
	GtkStyleContext *context;
	GtkStateFlags state;
	GtkBorder tmp;

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);

	gtk_style_context_get_padding (context, state, border);
	gtk_style_context_get_border (context, state, &tmp);
	border->top += tmp.top;
	border->right += tmp.right;
	border->bottom += tmp.bottom;
	border->left += tmp.left;
}

static void
gdaui_bar_get_preferred_width (GtkWidget *widget,
			       gint      *minimum_width,
			       gint      *natural_width)
{
	GtkBorder border;

	get_padding_and_border (widget, &border);

	GTK_WIDGET_CLASS (parent_class)->get_preferred_width (widget,
							      minimum_width,
							      natural_width);

	if (minimum_width)
		*minimum_width += border.left + border.right;
	if (natural_width)
		*natural_width += border.left + border.right;
}

static void
gdaui_bar_get_preferred_height (GtkWidget *widget,
				gint      *minimum_height,
				gint      *natural_height)
{
	GtkBorder border;

	get_padding_and_border (widget, &border);

	GTK_WIDGET_CLASS (parent_class)->get_preferred_height (widget,
							       minimum_height,
							       natural_height);

	if (minimum_height)
		*minimum_height += border.top + border.bottom;
	if (natural_height)
		*natural_height += border.top + border.bottom;
}

static gboolean
gdaui_bar_draw (GtkWidget *widget,
		cairo_t   *cr)
{
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);

	gtk_render_background (context, cr, 0, 0,
			       gtk_widget_get_allocated_width (widget),
			       gtk_widget_get_allocated_height (widget));
	gtk_render_frame (context, cr, 0, 0,
			  gtk_widget_get_allocated_width (widget),
			  gtk_widget_get_allocated_height (widget));

	GTK_WIDGET_CLASS (parent_class)->draw (widget, cr);

	return FALSE;
}

/**
 * gdaui_bar_new:
 *
 * Creates a new #GdauiBar object.
 *
 * Returns: a new #GdauiBar object
 */
GtkWidget *
gdaui_bar_new (const gchar *text)
{
	return g_object_new (GDAUI_TYPE_BAR, "text", text, NULL);
}

/**
 * gdaui_bar_get_text:
 * @bar: a #GdauiBar widget.
 *
 * Get the text being displayed in the given gray bar widget. This
 * does not include any embedded underlines indicating mnemonics or
 * Pango markup.
 *
 * Returns: the text in the widget.
 */
const gchar *
gdaui_bar_get_text (GdauiBar *bar)
{
        g_return_val_if_fail (GDAUI_IS_BAR (bar), NULL);

	return gtk_label_get_text (GTK_LABEL (bar->priv->label));
}

/**
 * gdaui_bar_set_text
 * @bar: a #GdauiBar widget
 * @text: a string
 *
 * Set the text displayed in the given gray bar widget. This can include
 * embedded underlines indicating mnemonics or Pango markup.
 *
 */
void
gdaui_bar_set_text (GdauiBar *bar, const gchar *text)
{
        g_return_if_fail (GDAUI_IS_BAR (bar));

        gtk_label_set_markup (GTK_LABEL (bar->priv->label), text);
}

/**
 * gdaui_set_icon_from_file
 * @bar: a #GdauiBar widget.
 * @file: filename.
 *
 * Set the icon displayed in the given gray bar widget. This can include
 * embedded underlines indicating mnemonics or Pango markup.
 *
 */
void
gdaui_bar_set_icon_from_file (GdauiBar *bar, const gchar *file)
{
        g_return_if_fail (GDAUI_IS_BAR (bar));

        gtk_image_set_from_file (GTK_IMAGE (bar->priv->icon), file);
        gdaui_bar_set_show_icon (bar, TRUE);
}

/**
 * gdaui_bar_set_icon_from_resource:
 */
void
gdaui_bar_set_icon_from_resource (GdauiBar *bar, const gchar *resource_name)
{
	g_return_if_fail (GDAUI_IS_BAR (bar));
	gtk_image_set_from_resource (GTK_IMAGE (bar->priv->icon), resource_name);
	gdaui_bar_set_show_icon (bar, TRUE);
}

/**
 * gdaui_bar_set_icon_from_pixbuf:
 */
void
gdaui_bar_set_icon_from_pixbuf (GdauiBar *bar, GdkPixbuf *pixbuf)
{
	g_return_if_fail (GDAUI_IS_BAR (bar));
	g_return_if_fail (!pixbuf || GDK_IS_PIXBUF (pixbuf));

	gtk_image_set_from_pixbuf (GTK_IMAGE (bar->priv->icon), pixbuf);
	gdaui_bar_set_show_icon (bar, TRUE);
}

/**
 * gdaui_bar_set_icon_from_icon_name:
 */
void
gdaui_bar_set_icon_from_icon_name (GdauiBar *bar, const gchar *icon_name)
{
	g_return_if_fail (GDAUI_IS_BAR (bar));
	g_return_if_fail (icon_name);

	gtk_image_set_from_icon_name (GTK_IMAGE (bar->priv->icon), icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gdaui_bar_set_show_icon (bar, TRUE);
}

/**
 * gdaui_bar_set_show_icon
 * @bar: a #GdauiBar widget.
 * @show: whether to show the icon or not.
 *
 * Set the icon displaying mode for the given grid.
 */
void
gdaui_bar_set_show_icon (GdauiBar *bar, gboolean show)
{
        g_return_if_fail (GDAUI_IS_BAR (bar));

        if (show) {
                gtk_widget_show (bar->priv->icon);
                bar->priv->show_icon = TRUE;
        } else {
                gtk_widget_hide (bar->priv->icon);
                bar->priv->show_icon = FALSE;
        }
}

/**
 * gdaui_bar_get_show_icon
 * @bar: a #GdauiBar widget.
 *
 * Get whether the icon is being shown for the given gray bar.
 *
 * Returns: TRUE if the icon is shown, FALSE if not.
 */
gboolean
gdaui_bar_get_show_icon (GdauiBar *bar)
{
        g_return_val_if_fail (GDAUI_IS_BAR (bar), FALSE);

        return bar->priv->show_icon;
}

static void
find_icon_pressed_cb (GtkEntry *entry, G_GNUC_UNUSED GtkEntryIconPosition icon_pos,
		      G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
		gtk_entry_set_text (entry, "");
}

/**
 * gdaui_bar_add_search_entry:
 * @bar: a #GdauiBar
 *
 * Returns: (transfer none): the created #GtkEntry
 */
GtkWidget *
gdaui_bar_add_search_entry (GdauiBar *bar)
{
	g_return_val_if_fail (GDAUI_IS_BAR (bar), NULL);

	GtkWidget *vb, *entry;
	vb = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vb), GTK_BUTTONBOX_CENTER);

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (vb), entry, FALSE, FALSE, 0);

	/* CSS theming */
	GtkStyleContext *context;
	context = gtk_widget_get_style_context (vb);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (css_provider), G_MAXUINT);
	gtk_style_context_add_class (context, "gdauibar_entry");

	context = gtk_widget_get_style_context (entry);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (css_provider), G_MAXUINT);
	gtk_style_context_add_class (context, "gdauibar_entry");

	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
					   GTK_ENTRY_ICON_SECONDARY,
					   "edit-clear");
	g_signal_connect (entry, "icon-press",
			  G_CALLBACK (find_icon_pressed_cb), NULL);

	gtk_widget_show_all (vb);
	gdaui_bar_add_widget (bar, vb);

	return entry;
}

/**
 * gdaui_bar_add_widget:
 * @bar: a #GdauiBar
 * @widget: a widget to add to @bar.
 *
 * Adds @widget to @bar.
 */
void
gdaui_bar_add_widget (GdauiBar *bar, GtkWidget *widget)
{
	g_return_if_fail (GDAUI_IS_BAR (bar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_box_pack_start (GTK_BOX (bar->priv->action_area), widget, FALSE, FALSE, 0);
}

/**
 * gdaui_bar_add_button_from_icon_name:
 * @bar: a #GdauiBar
 * @icon_name: an icon name
 *
 * Returns: (transfer none): the created #GtkButton
 */
GtkWidget *
gdaui_bar_add_button_from_icon_name (GdauiBar *bar, const gchar *icon_name)
{
	g_return_val_if_fail (GDAUI_IS_BAR (bar), NULL);

	GtkWidget *vb, *button;

	vb = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vb), GTK_BUTTONBOX_CENTER);
	gtk_box_pack_start (GTK_BOX (bar->priv->action_area), vb, FALSE, FALSE, 0);

	button = gtk_button_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_box_pack_start (GTK_BOX (vb), button, FALSE, FALSE, 0);

	/* CSS theming */
	GtkStyleContext *context;
	context = gtk_widget_get_style_context (vb);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (css_provider), G_MAXUINT);
	gtk_style_context_add_class (context, "gdauibar_button");

	context = gtk_widget_get_style_context (button);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (css_provider), G_MAXUINT);
	gtk_style_context_add_class (context, "gdauibar_button");

	gtk_widget_show_all (vb);
	return button;
}
