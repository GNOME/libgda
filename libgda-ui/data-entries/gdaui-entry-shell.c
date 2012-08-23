/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <gdk/gdkkeysyms.h>
#include "gdaui-entry-shell.h"
#include "gdaui-entry-none.h"
#include <libgda/gda-data-handler.h>
#include <libgda-ui/internal/utility.h>
#include <glib/gi18n-lib.h>
#include "widget-embedder.h"
static void gdaui_entry_shell_class_init (GdauiEntryShellClass *class);
static void gdaui_entry_shell_init (GdauiEntryShell *wid);
static void gdaui_entry_shell_dispose (GObject *object);

static void gdaui_entry_shell_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gdaui_entry_shell_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);


static gint event_cb (GtkWidget *widget, GdkEvent *event, GdauiEntryShell *shell);
static void show_all (GtkWidget *widget);
static void contents_modified_cb (GdauiEntryShell *shell, gpointer unused);
static void gdaui_entry_shell_refresh_status_display (GdauiEntryShell *shell);

/* properties */
enum {
	PROP_0,
	PROP_HANDLER,
	PROP_ACTIONS,
	PROP_IS_CELL_RENDERER
};

struct  _GdauiEntryShellPriv {
        GtkWidget           *embedder;
	GtkWidget           *hbox;
        GtkWidget           *button;
        GdaDataHandler      *data_handler;
	gboolean             show_actions;

	gboolean             value_is_null;
	gboolean             value_is_modified;
	gboolean             value_is_default;
	gboolean             value_is_non_valid;

	gboolean             is_cell_renderer;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;
static GtkCssProvider *css_provider = NULL;

/**
 * gdaui_entry_shell_get_type:
 *
 * Register the GdauiEntryShell class on the GLib type system.
 *
 * Returns: the GType identifying the class.
 */
GType
gdaui_entry_shell_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryShellClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_shell_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryShell),
			0,
			(GInstanceInitFunc) gdaui_entry_shell_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_VIEWPORT, "GdauiEntryShell", &info, 0);
	}
	return type;
}


static void
gdaui_entry_shell_class_init (GdauiEntryShellClass * class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_shell_dispose;
	widget_class->show_all = show_all;

	/* Properties */
	object_class->set_property = gdaui_entry_shell_set_property;
	object_class->get_property = gdaui_entry_shell_get_property;
	g_object_class_install_property (object_class, PROP_HANDLER,
					 g_param_spec_object ("handler", NULL, NULL, GDA_TYPE_DATA_HANDLER,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_ACTIONS,
					 g_param_spec_boolean ("actions", NULL, NULL, TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property (object_class, PROP_IS_CELL_RENDERER,
					 g_param_spec_boolean ("is-cell-renderer", NULL, NULL, TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
show_all (GtkWidget *widget)
{
	if (((GdauiEntryShell*) widget)->priv->show_actions)
		gtk_widget_show (((GdauiEntryShell*) widget)->priv->button);
}

static void
gdaui_entry_shell_init (GdauiEntryShell *shell)
{
	GtkWidget *button, *hbox, *arrow;
	GValue *gval;

	if (!css_provider) {
		css_provider = gtk_css_provider_new ();
                gtk_css_provider_load_from_data (css_provider,
						 "* {\n"
						 "-GtkArrow-arrow-scaling: 0.4;\n"
						 "margin: 0;\n"
						 "padding: 0;\n"
						 "border-style: none;\n"
						 "border-width: 0;\n"
						 "-GtkButton-default-border: 0;\n"
						 "-GtkButton-default-outside-border: 0;\n"
						 "-GtkButton-inner-border: 0}",
						 -1, NULL);
	}

	/* Private structure */
	shell->priv = g_new0 (GdauiEntryShellPriv, 1);
	shell->priv->embedder = NULL;
	shell->priv->button = NULL;
	shell->priv->show_actions = TRUE;
	shell->priv->data_handler = NULL;

	shell->priv->value_is_null = FALSE;
	shell->priv->value_is_modified = FALSE;
	shell->priv->value_is_default = FALSE;
	shell->priv->value_is_non_valid = FALSE;

	shell->priv->is_cell_renderer = FALSE;

	/* Setting the initial layout */
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (shell), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (shell), 0);
	gtk_style_context_add_provider (gtk_widget_get_style_context ((GtkWidget*) shell),
					GTK_STYLE_PROVIDER (css_provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* hbox */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (shell), hbox);
	gtk_widget_show (hbox);
	shell->priv->hbox = hbox;
	gtk_style_context_add_provider (gtk_widget_get_style_context (hbox),
					GTK_STYLE_PROVIDER (css_provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* vbox to insert the real widget to edit data */
	shell->priv->embedder = widget_embedder_new ();
	gtk_style_context_add_provider (gtk_widget_get_style_context (shell->priv->embedder),
					GTK_STYLE_PROVIDER (css_provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_box_pack_start (GTK_BOX (hbox), shell->priv->embedder, TRUE, TRUE, 0);
	gtk_widget_show (shell->priv->embedder);

	/* button to change the entry's state and to display that state */
	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_style_context_add_provider (gtk_widget_get_style_context (arrow),
					GTK_STYLE_PROVIDER (css_provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	button = gtk_button_new ();
	gtk_style_context_add_provider (gtk_widget_get_style_context (button),
					GTK_STYLE_PROVIDER (css_provider),
					G_MAXUINT);

	gtk_container_add (GTK_CONTAINER (button), arrow);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
	shell->priv->button = button;
	gtk_widget_show_all (button);

	g_signal_connect (G_OBJECT (button), "event",
			  G_CALLBACK (event_cb), shell);

	/* focus */
	gval = g_new0 (GValue, 1);
	g_value_init (gval, G_TYPE_BOOLEAN);
	g_value_set_boolean (gval, TRUE);
	g_object_set_property (G_OBJECT (button), "can-focus", gval);
	g_free (gval);
}

static void
gdaui_entry_shell_dispose (GObject   * object)
{
	GdauiEntryShell *shell;

	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (object));

	shell = GDAUI_ENTRY_SHELL (object);

	if (shell->priv) {
		if (shell->priv->data_handler)
			g_object_unref (shell->priv->data_handler);

		g_free (shell->priv);
		shell->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_shell_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	gpointer ptr;
	GdauiEntryShell *shell;

	shell = GDAUI_ENTRY_SHELL (object);
	if (shell->priv) {
		switch (param_id) {
		case PROP_HANDLER:
			ptr = g_value_get_object (value);
			if (shell->priv->data_handler) {
				g_object_unref (shell->priv->data_handler);
				shell->priv->data_handler = NULL;
			}

			if (ptr) {
				shell->priv->data_handler = GDA_DATA_HANDLER (ptr);
				g_object_ref (G_OBJECT (shell->priv->data_handler));
			}
			else
				g_message (_("Widget of class '%s' does not have any associated GdaDataHandler, "
					     "(to be set using the 'handler' property) expect some mis-behaviours"),
					   G_OBJECT_TYPE_NAME (object));
			break;
		case PROP_ACTIONS:
			shell->priv->show_actions = g_value_get_boolean (value);
			if (shell->priv->show_actions)
				gtk_widget_show (shell->priv->button);
			else
				gtk_widget_hide (shell->priv->button);
			break;
		case PROP_IS_CELL_RENDERER:
			if (GTK_IS_CELL_EDITABLE (shell) &&
			    (g_value_get_boolean (value) != shell->priv->is_cell_renderer)) {
				shell->priv->is_cell_renderer = g_value_get_boolean (value);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gdaui_entry_shell_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	GdauiEntryShell *shell;

	shell = GDAUI_ENTRY_SHELL (object);
	if (shell->priv) {
		switch (param_id) {
		case PROP_HANDLER:
			g_value_set_object (value, shell->priv->data_handler);
			break;
		case PROP_ACTIONS:
			g_value_set_boolean (value, shell->priv->show_actions);
			break;
		case PROP_IS_CELL_RENDERER:
			g_value_set_boolean (value, shell->priv->is_cell_renderer);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}


/**
 * gdaui_entry_shell_pack_entry:
 * @shell: a #GdauiEntryShell object
 * @main_widget: a #GtkWidget to pack into @shell
 *
 * Packs a #GTkWidget widget into the GdauiEntryShell.
 */
void
gdaui_entry_shell_pack_entry (GdauiEntryShell *shell, GtkWidget *main_widget)
{
	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));
	g_return_if_fail (main_widget && GTK_IS_WIDGET (main_widget));
	gtk_container_add (GTK_CONTAINER (shell->priv->embedder), main_widget);

	/* signals */
	g_signal_connect (G_OBJECT (shell), "contents-modified",
			  G_CALLBACK (contents_modified_cb), NULL);

	g_signal_connect (G_OBJECT (shell), "status-changed",
			  G_CALLBACK (contents_modified_cb), NULL);
}

static void
contents_modified_cb (GdauiEntryShell *shell, G_GNUC_UNUSED gpointer unused)
{
	gdaui_entry_shell_refresh (shell);
}

static void mitem_activated_cb (GtkWidget *mitem, GdauiEntryShell *shell);
static GdaValueAttribute gdaui_entry_shell_refresh_attributes (GdauiEntryShell *shell);
static gint
event_cb (G_GNUC_UNUSED GtkWidget *widget, GdkEvent *event, GdauiEntryShell *shell)
{
	gboolean done = FALSE;

	if (!shell->priv->show_actions)
		return done;

	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *) event;
		if ((bevent->button == 1) || (bevent->button == 3)) {
			GtkWidget *menu;
			guint attributes;

			attributes = gdaui_entry_shell_refresh_attributes (shell);
			menu = _gdaui_utility_entry_build_actions_menu (G_OBJECT (shell), attributes,
									G_CALLBACK (mitem_activated_cb));
			gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
					bevent->button, bevent->time);
			done = TRUE;
		}
	}

	if (event->type == GDK_KEY_PRESS) {
		GtkWidget *menu;
		GdkEventKey *kevent = (GdkEventKey *) event;

		if (kevent->keyval == GDK_KEY_space) {
			guint attributes;

			attributes = gdaui_entry_shell_refresh_attributes (shell);
			menu = _gdaui_utility_entry_build_actions_menu (G_OBJECT (shell), attributes,
									G_CALLBACK (mitem_activated_cb));

			gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
					0, kevent->time);
			done = TRUE;
		}
		else {
			if (kevent->keyval == GDK_KEY_Tab)
				done = FALSE;
			else
				done = TRUE;
		}
	}

	return done;
}

static void
mitem_activated_cb (GtkWidget *mitem, GdauiEntryShell *shell)
{
	guint action;

	action = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (mitem), "action"));
	gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (shell), action, action);
}

static void
gdaui_entry_shell_refresh_status_display (GdauiEntryShell *shell)
{
	static GdkRGBA **colors = NULL;
	GdkRGBA *normal = NULL, *prelight = NULL;

	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));

	if (!colors)
		colors = _gdaui_utility_entry_build_info_colors_array_a ();

	gtk_widget_set_tooltip_text (shell->priv->button, NULL);

	if (shell->priv->value_is_null) {
		normal = colors[0];
		prelight = colors[1];
		gtk_widget_set_tooltip_text (shell->priv->button, _("Value is NULL"));
	}

	if (shell->priv->value_is_default) {
		normal = colors[2];
		prelight = colors[3];
		gtk_widget_set_tooltip_text (shell->priv->button, _("Value will be determined by default"));
	}

	if (shell->priv->value_is_non_valid) {
		normal = colors[4];
		prelight = colors[5];
		gtk_widget_set_tooltip_text (shell->priv->button, _("Value is invalid"));
	}

	gtk_widget_override_background_color (shell->priv->button, GTK_STATE_FLAG_NORMAL, normal);
	gtk_widget_override_background_color (shell->priv->button, GTK_STATE_FLAG_ACTIVE, normal);
	gtk_widget_override_background_color (shell->priv->button, GTK_STATE_FLAG_PRELIGHT, prelight);
}

static GdaValueAttribute
gdaui_entry_shell_refresh_attributes (GdauiEntryShell *shell)
{
	GdaValueAttribute attrs;

	attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));
	shell->priv->value_is_null = attrs & GDA_VALUE_ATTR_IS_NULL;
	shell->priv->value_is_modified = ! (attrs & GDA_VALUE_ATTR_IS_UNCHANGED);
	shell->priv->value_is_default = attrs & GDA_VALUE_ATTR_IS_DEFAULT;
	shell->priv->value_is_non_valid = attrs & GDA_VALUE_ATTR_DATA_NON_VALID;

	return attrs;
}

/**
 * gdaui_entry_shell_refresh:
 * @shell: the GdauiEntryShell widget to refresh
 *
 * Forces the shell to refresh its display (mainly the color of the
 * button).
 */
void
gdaui_entry_shell_refresh (GdauiEntryShell *shell)
{
	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));
	gdaui_entry_shell_refresh_attributes (shell);
	gdaui_entry_shell_refresh_status_display (shell);
}

/**
 * gdaui_entry_shell_set_unknown:
 * @shell: the #GdauiEntryShell widget to refresh
 * @unknown: set to %TRUE if @shell's contents is unavailable and should not be modified
 *
 * Defines if @shell's contents is in an undefined state (shows or hides @shell's contents)
 */
void
gdaui_entry_shell_set_unknown (GdauiEntryShell *shell, gboolean unknown)
{
	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));

	widget_embedder_set_valid ((WidgetEmbedder*) shell->priv->embedder, !unknown);
}

/**
 * gdaui_entry_shell_set_ucolor:
 * @shell: a #GdauiEntryShell
 * @red: the red component of a color
 * @green: the green component of a color
 * @blue: the blue component of a color
 * @alpha: the alpha component of a color
 *
 * Defines the color to be used when @de displays an invalid value. Any value not
 * between 0. and 1. will result in the default hard coded values to be used (grayish).
 *
 * Since: 5.0.3
 */
void
gdaui_entry_shell_set_ucolor (GdauiEntryShell *shell, gdouble red, gdouble green,
			      gdouble blue, gdouble alpha)
{
	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));
	widget_embedder_set_ucolor ((WidgetEmbedder*) shell->priv->embedder, red, green, blue, alpha);
}
