/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <gdk/gdk.h>
#include "gdaui-entry-shell.h"
#include "gdaui-entry-none.h"
#include <libgda/gda-data-handler.h>
#include <libgda-ui/internal/utility.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-debug-macros.h>

/*#define DEBUG*/

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
/*
 * This widget displays a GValue and its attributes (GValueAttribute) in the following way:
 * - if GDA_VALUE_ATTR_DATA_NON_VALID, then:
 *      - if GDA_VALUE_ATTR_READ_ONLY: then a "Invalid" label is shown, and no action is possible
 *    else
 *      - an entry widget is shown
 *      NB: in both cases, the widget is outlined (or colored) in the "invalid color", which can be changed
 *          using gdaui_entry_shell_set_invalid_color()
 * else
 * - if GDA_VALUE_ATTR_IS_NULL or GDA_VALUE_ATTR_IS_DEFAULT then
 *      - a "value is NULL" or "value is default" or "Value is default (NULL)" is shown.
 *        If not GDA_VALUE_ATTR_READ_ONLY, then an "edit" action is possible, which (at least up to when
 *        the focus is lost), displays an entry widget
 *    else
 *      - an entry widget is shown. If not GDA_VALUE_ATTR_READ_ONLY then, depending on
 *        GDA_VALUE_ATTR_CAN_BE_NULL, GDA_VALUE_ATTR_CAN_BE_DEFAULT, or GDA_VALUE_ATTR_HAS_VALUE_ORIG, the
 *        following respective actions are possible: "set to NULL", "set to default" and "reset" (this last one
 *        if not GDA_VALUE_ATTR_IS_UNCHANGED)
 */

#define PAGE_LABEL "L"
#define PAGE_ENTRY "E"

typedef enum {
	MESSAGE_NONE,
	MESSAGE_NULL,
	MESSAGE_DEFAULT,
	MESSAGE_DEFAULT_AND_NULL,
	MESSAGE_INVALID
} MessageType;

static const gchar *raw_messages[] = {
	"",
	N_("Unspecified"),
	N_("Default"),
	N_("Default (unspecified)"),
	N_("Invalid"),
};

static gchar **messages = NULL;

/* properties */
enum {
	PROP_0,
	PROP_HANDLER,
	PROP_IS_CELL_RENDERER
};

#define value_is_null(attr) ((attr) & GDA_VALUE_ATTR_IS_NULL)
#define value_is_modified(attr) (!((attr) & GDA_VALUE_ATTR_IS_UNCHANGED))
#define value_is_default(attr) ((attr) & GDA_VALUE_ATTR_IS_DEFAULT)
#define value_is_valid(attr) (!((attr) & GDA_VALUE_ATTR_DATA_NON_VALID))
struct  _GdauiEntryShellPriv {
	GtkWidget           *stack;
	GtkWidget           *button; /* "..." button */
	GtkWidget           *label;
        GdaDataHandler      *data_handler; /* FIXME: to remove if unused elsewhere */
	gboolean             show_actions;
	gboolean             is_cell_renderer;
	gboolean             editable;
	gboolean             being_edited;
};

static void show_or_hide_actions_button (GdauiEntryShell *shell, GdaValueAttribute attr);
static guint compute_nb_possible_actions (GdauiEntryShell *shell, GdaValueAttribute attr);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

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

		type = g_type_register_static (GTK_TYPE_BOX, "GdauiEntryShell", &info, 0);
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

	/* Properties */
	object_class->set_property = gdaui_entry_shell_set_property;
	object_class->get_property = gdaui_entry_shell_get_property;
	g_object_class_install_property (object_class, PROP_HANDLER,
					 g_param_spec_object ("handler", NULL, NULL, GDA_TYPE_DATA_HANDLER,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property (object_class, PROP_IS_CELL_RENDERER,
					 g_param_spec_boolean ("is-cell-renderer", NULL, NULL, TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
destroy_popover (GtkWidget *child)
{
	/* destroy GtkPopover in which @child is */
	GtkWidget *pop;
	pop = gtk_widget_get_ancestor (GTK_WIDGET (child), GTK_TYPE_POPOVER);
	if (pop)
		gtk_widget_destroy (pop);
}

static void
action_edit_value_cb (GtkButton *button, GdauiEntryShell *shell)
{
	GdaValueAttribute attr;
	attr = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));
	if (! (attr & GDA_VALUE_ATTR_READ_ONLY)) {
		gtk_stack_set_visible_child_name (GTK_STACK (shell->priv->stack), PAGE_ENTRY);
		gdaui_data_entry_grab_focus (GDAUI_DATA_ENTRY (shell));
		show_or_hide_actions_button (shell, attr);
		shell->priv->being_edited = TRUE;
	}
	if (button)
		destroy_popover (GTK_WIDGET (button));
}

static void
action_set_value_to_null_cb (GtkButton *button, GdauiEntryShell *shell)
{
	GdaValueAttribute attr;
	attr = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));
	if (attr & GDA_VALUE_ATTR_CAN_BE_NULL) {
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (shell),
						 GDA_VALUE_ATTR_IS_NULL, GDA_VALUE_ATTR_IS_NULL);
		show_or_hide_actions_button (shell, attr);
		shell->priv->being_edited = FALSE;
	}
	destroy_popover (GTK_WIDGET (button));
}

static void
action_set_value_to_default_cb (GtkButton *button, GdauiEntryShell *shell)
{
	GdaValueAttribute attr;
	attr = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));
	if (attr & GDA_VALUE_ATTR_CAN_BE_DEFAULT) {
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (shell),
						 GDA_VALUE_ATTR_IS_DEFAULT, GDA_VALUE_ATTR_IS_DEFAULT);
		show_or_hide_actions_button (shell, attr);
		shell->priv->being_edited = FALSE;
	}
	destroy_popover (GTK_WIDGET (button));
}

static void
action_reset_value_cb (GtkButton *button, GdauiEntryShell *shell)
{
	GdaValueAttribute attr;
	attr = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));
	if (attr & GDA_VALUE_ATTR_HAS_VALUE_ORIG) {
		gdaui_data_entry_set_attributes (GDAUI_DATA_ENTRY (shell),
						 GDA_VALUE_ATTR_IS_UNCHANGED, GDA_VALUE_ATTR_IS_UNCHANGED);
		shell->priv->being_edited = FALSE;
	}
	destroy_popover (GTK_WIDGET (button));
}

static void
show_or_hide_actions_button (GdauiEntryShell *shell, GdaValueAttribute attr)
{
	gboolean visible = FALSE;
	if (shell->priv->editable) {
		guint nb;
		nb = compute_nb_possible_actions (shell, attr);
		if (nb > 0)
			visible = TRUE;
	}
	gtk_widget_set_visible (shell->priv->button, visible);
}

static guint
compute_nb_possible_actions (GdauiEntryShell *shell, GdaValueAttribute attr)
{
	guint nb = 0;

	if (attr & GDA_VALUE_ATTR_READ_ONLY)
		return 0; /* no action possible */

	if (!strcmp (gtk_stack_get_visible_child_name (GTK_STACK (shell->priv->stack)), PAGE_LABEL))
		nb ++; /* "edit value" action */

	if ((attr & GDA_VALUE_ATTR_CAN_BE_NULL) && !(attr & GDA_VALUE_ATTR_IS_NULL))
		nb++;
	if ((attr & GDA_VALUE_ATTR_CAN_BE_DEFAULT) && !(attr & GDA_VALUE_ATTR_IS_DEFAULT))
		nb++;
	if ((attr & GDA_VALUE_ATTR_HAS_VALUE_ORIG) && ! (attr & GDA_VALUE_ATTR_IS_UNCHANGED))
		nb ++;
	return nb;
}

static void
actions_button_clicked_cb (G_GNUC_UNUSED GtkButton *button, GdauiEntryShell *shell)
{
	GdaValueAttribute attr;
	attr = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));

	if (attr & GDA_VALUE_ATTR_READ_ONLY)
		g_assert_not_reached ();

	shell->priv->being_edited = FALSE;
	g_assert (compute_nb_possible_actions (shell, attr) != 0);

	GtkWidget *pop;
	pop = gtk_popover_new (GTK_WIDGET (button));
	GtkWidget *box;
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (pop), box);

	GtkWidget *action_button;
	if (!strcmp (gtk_stack_get_visible_child_name (GTK_STACK (shell->priv->stack)), PAGE_LABEL)) {
		action_button = gtk_button_new_with_label (_("Edit"));
		g_signal_connect (action_button, "clicked",
				  G_CALLBACK (action_edit_value_cb), shell);
		gtk_box_pack_start (GTK_BOX (box), action_button, FALSE, FALSE, 0);
	}

	if ((attr & GDA_VALUE_ATTR_CAN_BE_NULL) && !(attr & GDA_VALUE_ATTR_IS_NULL)) {
		action_button = gtk_button_new_with_label (_("Define as unspecified"));
		g_signal_connect (action_button, "clicked",
				  G_CALLBACK (action_set_value_to_null_cb), shell);
		gtk_box_pack_start (GTK_BOX (box), action_button, FALSE, FALSE, 0);
	}

	if ((attr & GDA_VALUE_ATTR_CAN_BE_DEFAULT) && !(attr & GDA_VALUE_ATTR_IS_DEFAULT)) {
		action_button = gtk_button_new_with_label (_("Define as default"));
		g_signal_connect (action_button, "clicked",
				  G_CALLBACK (action_set_value_to_default_cb), shell);
		gtk_box_pack_start (GTK_BOX (box), action_button, FALSE, FALSE, 0);
	}

	if ((attr & GDA_VALUE_ATTR_HAS_VALUE_ORIG) && ! (attr & GDA_VALUE_ATTR_IS_UNCHANGED)) {
		action_button = gtk_button_new_with_label (_("Reset"));
		g_signal_connect (action_button, "clicked",
				  G_CALLBACK (action_reset_value_cb), shell);
			gtk_box_pack_start (GTK_BOX (box), action_button, FALSE, FALSE, 0);
	}

	g_signal_connect (pop, "closed", G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show_all (pop);
}

static void
event_after_cb (GtkWidget *widget, GdkEvent *event, GdauiEntryShell *shell)
{
	if (((event->type == GDK_KEY_RELEASE) && (((GdkEventKey*) event)->keyval == GDK_KEY_Escape)) ||
	    ((event->type == GDK_FOCUS_CHANGE) && (((GdkEventFocus*) event)->in == FALSE))) {
		shell->priv->being_edited = FALSE;
#ifdef DEBUG
		g_print ("EVENT AFTER: %s!\n", event->type == GDK_FOCUS_CHANGE ? "FOCUS" : "KEY RELEASE");
#endif
		if (!strcmp (gtk_stack_get_visible_child_name (GTK_STACK (shell->priv->stack)), PAGE_ENTRY)) {
			GdaValueAttribute attr;
			attr = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell));
			_gdaui_entry_shell_attrs_changed (shell, attr);
		}
	}
}

static gboolean
label_event_cb (GtkWidget *widget, GdkEvent *event, GdauiEntryShell *shell)
{
	action_edit_value_cb (NULL, shell);
	return FALSE;
}

static void
gdaui_entry_shell_init (GdauiEntryShell *shell)
{
	GValue *gval;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (shell), GTK_ORIENTATION_HORIZONTAL);

	/* Private structure */
	shell->priv = g_new0 (GdauiEntryShellPriv, 1);
	shell->priv->show_actions = TRUE;
	shell->priv->data_handler = NULL;
	shell->priv->editable = TRUE;
	shell->priv->being_edited = FALSE;

	shell->priv->is_cell_renderer = FALSE;

	/* Setting the initial layout */
	shell->priv->stack = gtk_stack_new ();
	gtk_box_pack_start (GTK_BOX (shell), shell->priv->stack, TRUE, TRUE, 0);
	gtk_widget_show (shell->priv->stack);
	gtk_container_set_border_width (GTK_CONTAINER (shell), 0);

	/* button */
	shell->priv->button = gtk_button_new_with_label ("...");
	gtk_button_set_relief (GTK_BUTTON (shell->priv->button), GTK_RELIEF_NONE);
	gtk_style_context_add_class (gtk_widget_get_style_context (shell->priv->button), "action-entry");
	g_object_set (G_OBJECT (shell->priv->button), "no-show-all", TRUE, NULL);
	gtk_box_pack_start (GTK_BOX (shell), shell->priv->button, FALSE, FALSE, 0);
	gtk_widget_show (shell->priv->button);
	g_signal_connect (shell->priv->button, "clicked",
			  G_CALLBACK (actions_button_clicked_cb), shell);

	/* label */
	if (!messages) {
		guint size, i;
		size = G_N_ELEMENTS (raw_messages);
		messages = g_new (gchar *, size);
		for (i = 0; i < size; i++)
			messages[i] = g_markup_printf_escaped ("<i>%s</i>", raw_messages[i]);
	}
	shell->priv->label = gtk_label_new ("");
	gtk_widget_set_name (shell->priv->label, "invalid-label");
	gtk_label_set_markup (GTK_LABEL (shell->priv->label), messages[MESSAGE_INVALID]);
	gtk_widget_set_halign (shell->priv->label, GTK_ALIGN_START);
	GtkWidget *evbox;
	evbox = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (evbox), shell->priv->label);
	gtk_widget_show_all (evbox);
	gtk_stack_add_named (GTK_STACK (shell->priv->stack), evbox, PAGE_LABEL);

	gdaui_entry_shell_set_invalid_color (shell, 1.0, 0., 0., 0.9);

	g_signal_connect (evbox, "button-press-event",
			  G_CALLBACK (label_event_cb), shell);
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
 * @entry: a #GtkWidget to pack into @shell
 *
 * Packs a #GtkWidget widget into the @shell.
 */
void
gdaui_entry_shell_pack_entry (GdauiEntryShell *shell, GtkWidget *entry)
{
	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));
	g_return_if_fail (GTK_IS_WIDGET (entry));

	/* real packing */
	if (gtk_stack_get_child_by_name (GTK_STACK (shell->priv->stack), PAGE_ENTRY)) {
		g_warning ("Implementation error: %s() has already been called for this GdauiEntryShell.",
			   __FUNCTION__);
		return;
	}
	gtk_stack_add_named (GTK_STACK (shell->priv->stack), entry, PAGE_ENTRY);

	/* signals */
	g_signal_connect_after (G_OBJECT (entry), "event-after",
				G_CALLBACK (event_after_cb), shell);
}

/**
 * gdaui_entry_shell_set_invalid_color:
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
gdaui_entry_shell_set_invalid_color (GdauiEntryShell *shell, gdouble red, gdouble green,
				     gdouble blue, gdouble alpha)
{
	g_return_if_fail (GDAUI_IS_ENTRY_SHELL (shell));
	g_return_if_fail ((red >= 0.) && (red <= 1.));
	g_return_if_fail ((green >= 0.) && (green <= 1.));
	g_return_if_fail ((blue >= 0.) && (blue <= 1.));
	g_return_if_fail ((alpha >= 0.) && (alpha <= 1.));

	static GtkCssProvider *prov = NULL;
	if (!prov) {
		prov = gtk_css_provider_new ();
		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
							   GTK_STYLE_PROVIDER (prov),
							   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

#define BASE_CSS_FORMAT "@define-color gdaui_inv_color lighter (rgba (%u,%u,%u,0.%d));" \
		".invalid-entry #invalid-label {"			\
		"color: darker (@selected_fg_color);"			\
		"background-color: @gdaui_inv_color;"			\
		"border-width:1px;"					\
		"border-style:solid;"					\
		"border-radius:3px;"					\
		"border-color: @gdaui_inv_color;"			\
		"}"							\
		".invalid-entry GtkEntry, .invalid-entry GtkButton, .invalid-entry GtkScrolledWindow, .invalid-entry GtkSwitch {"	\
		"border-radius:3px;"					\
		"border-color: @gdaui_inv_color;"			\
		"border-width:1px;"					\
		"border-style:solid;"					\
		"}"							\
		".invalid-entry GtkLabel {"				\
		"color: @gdaui_inv_color;"							\
		"}"

	gchar *css;
	css = g_strdup_printf (BASE_CSS_FORMAT, (guint) (red * 255), (guint) (green * 255),
			       (guint) (blue * 255), (gint) (alpha * 100));
	gtk_css_provider_load_from_data (prov, css, -1, NULL);
	g_free (css);
}

void
_gdaui_entry_shell_mark_editable (GdauiEntryShell *shell, gboolean editable)
{
	
	if (editable != shell->priv->editable) {
		shell->priv->editable = editable;
		_gdaui_entry_shell_attrs_changed (shell,
						  gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (shell)));
	}
}

/*
 * Call this function to inform @shell that it may need to refresh its UI settings.
 */
void
_gdaui_entry_shell_attrs_changed (GdauiEntryShell *shell, GdaValueAttribute attr)
{

	MessageType msg = MESSAGE_NONE;
	if (value_is_valid (attr)) {
		if (value_is_null (attr)) {
			if (value_is_default (attr))
				msg = MESSAGE_DEFAULT_AND_NULL;
			else
				msg = MESSAGE_NULL;
		}
		else if (value_is_default (attr))
			msg = MESSAGE_DEFAULT;
	}
	else
		msg = MESSAGE_INVALID;

	if (! shell->priv->being_edited) {
		gtk_label_set_markup (GTK_LABEL (shell->priv->label), messages[msg]);
		if ((msg == MESSAGE_NONE) ||
		    ((msg == MESSAGE_INVALID) && ! (attr & GDA_VALUE_ATTR_READ_ONLY)))
			gtk_stack_set_visible_child_name (GTK_STACK (shell->priv->stack), PAGE_ENTRY);
		else
			gtk_stack_set_visible_child_name (GTK_STACK (shell->priv->stack), PAGE_LABEL);
	}

	if (msg == MESSAGE_INVALID)
		gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (shell)),
					     "invalid-entry");
	else
		gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (shell)),
						"invalid-entry");

	show_or_hide_actions_button (shell, attr);
}
