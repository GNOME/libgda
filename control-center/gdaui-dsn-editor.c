/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <string.h>
#include <libgda/libgda.h>
#include "gdaui-dsn-editor.h"
#include <libgda-ui/gdaui-provider-selector.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/internal/gdaui-provider-spec-editor.h>
#include <libgda-ui/internal/gdaui-provider-auth-editor.h>
#include "support.h"
#include "gdaui-login-dialog.h"
#include <libgda-ui/internal/utility.h>

struct _GdauiDsnEditorPrivate {
	gchar     *name;
	GtkWidget *icon;
	GtkWidget *wname;
	GtkWidget *wprovider;
	GtkWidget *wdesc;
	GtkWidget *is_system;
	GtkWidget *warning;

	GtkWidget *stack;
	GtkWidget *dsn_spec;
	GtkWidget *dsn_auth;	

	GdaDsnInfo *dsn_info;
	gboolean    no_change_signal; /* set to %TRUE when we don't want to emit the "changed" signal */
};

static void gdaui_dsn_editor_class_init (GdauiDsnEditorClass *klass);
static void gdaui_dsn_editor_init       (GdauiDsnEditor *config,
					    GdauiDsnEditorClass *klass);
static void gdaui_dsn_editor_finalize   (GObject *object);

enum {
	CHANGED,
	LAST_SIGNAL
};

static gint gdaui_dsn_editor_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * GdauiDsnEditor class implementation
 */
static void
_changed_cb (GdauiDsnEditor *config)
{
	gboolean can_save = FALSE;
	can_save = gdaui_dsn_editor_has_been_changed (config);
	GtkWindow *win;
	win = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (win), "DSNReset");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_save);
}

static void
gdaui_dsn_editor_class_init (GdauiDsnEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_dsn_editor_finalize;
	klass->changed = _changed_cb;

	/* add class signals */
	gdaui_dsn_editor_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDsnEditorClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
field_changed_cb (GtkWidget *widget, GdauiDsnEditor *config)
{
	if (widget == config->priv->wprovider) {
		/* replace the expander's contents */
		const gchar *pname;
		pname = gdaui_provider_selector_get_provider (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider));
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (config->priv->dsn_spec),
							  pname);
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (config->priv->dsn_auth),
							  pname);

		GdaProviderInfo *pinfo;
		pinfo = gda_config_get_provider_info (pname);

		GdkPixbuf *pix;
		pix = support_create_pixbuf_for_provider (pinfo);
		if (pix) {
			gtk_image_set_from_pixbuf (GTK_IMAGE (config->priv->icon), pix);
			g_object_unref (pix);
		}
		else
			gtk_image_clear (GTK_IMAGE (config->priv->icon));

		if (pinfo)
			gtk_widget_hide (config->priv->warning);
		else
			gtk_widget_show (config->priv->warning);
	}

	if (! config->priv->no_change_signal)
		g_signal_emit (config, gdaui_dsn_editor_signals[CHANGED], 0, NULL);
}

static void
field_toggled_cb (G_GNUC_UNUSED GtkWidget *widget, GdauiDsnEditor *config)
{
	if (! config->priv->no_change_signal)
		g_signal_emit (config, gdaui_dsn_editor_signals[CHANGED], 0, NULL);
}

#define PANE_DEFINITION "PANE_DEF"
#define PANE_PARAMS "PANE_PARAMS"
#define PANE_AUTH "PANE_AUTH"

static void dsn_test_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *state, gpointer data);
static void dsn_reset_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *state, gpointer data);
static void dsn_copy_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *state, gpointer data);

static GActionEntry win_entries[] = {
        { "DSNTest", dsn_test_cb, NULL, NULL, NULL },
	{ "DSNReset", dsn_reset_cb, NULL, NULL, NULL },
	{ "DSNCopy", dsn_copy_cb, NULL, NULL, NULL },
};

static void
gdaui_dsn_editor_init (GdauiDsnEditor *config, G_GNUC_UNUSED GdauiDsnEditorClass *klass)
{
	GtkWidget *grid;
	GtkWidget *label;
	gchar *str;

	g_return_if_fail (GDAUI_IS_DSN_EDITOR (config));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (config), GTK_ORIENTATION_VERTICAL);

	/* allocate private structure */
	config->priv = g_new0 (GdauiDsnEditorPrivate, 1);
	config->priv->dsn_info = g_new0 (GdaDsnInfo, 1);
	config->priv->no_change_signal = TRUE;

	/* data source's name and icon */
	GtkWidget *hbox_header;
	hbox_header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (config), hbox_header, FALSE, FALSE, 6);
	config->priv->icon = gtk_image_new ();
	gtk_widget_set_size_request (config->priv->icon, -1, SUPPORT_ICON_SIZE);
	gtk_box_pack_start (GTK_BOX (hbox_header), config->priv->icon, FALSE, FALSE, 0);
	config->priv->wname = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox_header), config->priv->wname, FALSE, FALSE, 10);

	GtkWidget *menu_button;
	menu_button = gtk_menu_button_new ();
	gtk_box_pack_end (GTK_BOX (hbox_header), menu_button, FALSE, FALSE, 0);

	GtkWidget *menu_icon;
        menu_icon = gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU);
        gtk_button_set_image (GTK_BUTTON (menu_button), menu_icon);

	GMenu *smenu;
        smenu = g_menu_new ();
        gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), G_MENU_MODEL (smenu));

	GMenuItem *mitem;
        mitem = g_menu_item_new ("Test data source", "win.DSNTest");
        g_menu_insert_item (smenu, -1, mitem);
        mitem = g_menu_item_new ("Reset data source's changes", "win.DSNReset");
        g_menu_insert_item (smenu, -1, mitem);
        mitem = g_menu_item_new ("Duplicate data source", "win.DSNCopy");
        g_menu_insert_item (smenu, -1, mitem);

	GtkWindow *win;
	win = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
	g_action_map_add_action_entries (G_ACTION_MAP (win), win_entries, G_N_ELEMENTS (win_entries), config);

	/* stack in a scrolled window */
	GtkWidget *sw;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_vexpand (sw, TRUE);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (config), sw, TRUE, TRUE, 6);

	/* Stack */
	config->priv->stack = gtk_stack_new ();
	gtk_stack_set_transition_type (GTK_STACK (config->priv->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
	gtk_container_add (GTK_CONTAINER (sw), config->priv->stack);
	
	/* set up widgets */
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
	gtk_widget_show (grid);

	label = gtk_label_new_with_mnemonic (_("_System wide data source:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_hexpand (label, FALSE);
	g_object_set (G_OBJECT (label), "xalign", 0., NULL);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	config->priv->is_system = gtk_check_button_new ();
	gtk_widget_show (config->priv->is_system);
	g_signal_connect (G_OBJECT (config->priv->is_system), "toggled",
			  G_CALLBACK (field_toggled_cb), config);
	gtk_grid_attach (GTK_GRID (grid), config->priv->is_system, 1, 1, 1, 1);

	str = g_strdup_printf ("%s <span foreground='red' weight='bold'>*</span>", _("_Provider:"));
	label = gtk_label_new ("");
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_hexpand (label, FALSE);
	g_object_set (G_OBJECT (label), "xalign", 0., NULL);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
	config->priv->wprovider = gdaui_provider_selector_new ();
	gtk_widget_set_hexpand (config->priv->wprovider, TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), config->priv->wprovider);
	gtk_widget_show (config->priv->wprovider);
	g_signal_connect (G_OBJECT (config->priv->wprovider), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_grid_attach (GTK_GRID (grid), config->priv->wprovider, 1, 2, 1, 1);

	label = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_hexpand (label, FALSE);
	g_object_set (G_OBJECT (label), "xalign", 0., NULL);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	config->priv->wdesc = gtk_text_view_new ();
	gtk_container_add (GTK_CONTAINER (sw), config->priv->wdesc);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (config->priv->wdesc), TRUE);
	gtk_widget_set_vexpand (config->priv->wdesc, TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), config->priv->wdesc);
	g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (config->priv->wdesc)), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_grid_attach (GTK_GRID (grid), sw, 1, 3, 1, 1);

	config->priv->warning = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (config->priv->warning),
			      _("<span foreground='red'>The database provider used by this data source is not available,\n"
				"editing the data source's attributes is disabled</span>"));
	gtk_widget_set_halign (config->priv->warning, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (config->priv->warning, TRUE);
	g_object_set (G_OBJECT (config->priv->warning), "xalign", 0.5, NULL);
	gtk_label_set_justify (GTK_LABEL (config->priv->warning), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (config->priv->warning), TRUE);
	gtk_grid_attach (GTK_GRID (grid), config->priv->warning, 0, 8, 2, 1);
	gtk_stack_add_named (GTK_STACK (config->priv->stack), grid, PANE_DEFINITION);

	/* connection's spec */
	config->priv->dsn_spec = _gdaui_provider_spec_editor_new (gdaui_provider_selector_get_provider 
								 (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));
	g_signal_connect (G_OBJECT (config->priv->dsn_spec), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_widget_show (config->priv->dsn_spec);
	gtk_stack_add_named (GTK_STACK (config->priv->stack), config->priv->dsn_spec, PANE_PARAMS);

	/* connection's authentication */
	config->priv->dsn_auth = _gdaui_provider_auth_editor_new (gdaui_provider_selector_get_provider 
								 (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));
	g_signal_connect (G_OBJECT (config->priv->dsn_auth), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_widget_show (config->priv->dsn_auth);
	gtk_stack_add_named (GTK_STACK (config->priv->stack), config->priv->dsn_auth, PANE_AUTH);

	config->priv->no_change_signal = FALSE;
}

static void
dsn_test_cb (G_GNUC_UNUSED GSimpleAction *action,
             G_GNUC_UNUSED GVariant *state,
             gpointer data)
{
	GdauiDsnEditor *editor;
	editor = GDAUI_DSN_EDITOR (data);

	GtkWidget *test_dialog = NULL;
	GdauiLogin* login = NULL;
	GdaConnection *cnc = NULL;
	gboolean auth_needed = gdaui_dsn_editor_need_authentication (editor);

	GtkWindow *parent = NULL;
	parent = (GtkWindow*) gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_WINDOW);

	const gchar *dsn;
	dsn = editor->priv->name;
	if (auth_needed) {
		gchar *title;
		title = g_strdup_printf (_("Login for %s"), dsn);
		test_dialog = gdaui_login_dialog_new (title, parent);
		g_free (title);
		login = gdaui_login_dialog_get_login_widget (GDAUI_LOGIN_DIALOG (test_dialog));
		g_object_set (G_OBJECT (login), "dsn", dsn, NULL);
	}

	if (!auth_needed || gdaui_login_dialog_run (GDAUI_LOGIN_DIALOG (test_dialog))) {
		if (test_dialog)
			parent = GTK_WINDOW (test_dialog);

		GtkWidget *msgdialog;
		GError *error = NULL;
		const GdaDsnInfo *cinfo = NULL;

		if (login)
			cinfo = gdaui_login_get_connection_information (login);
		cnc = gda_connection_open_from_dsn_name (dsn,
						    cinfo ? cinfo->auth_string : NULL,
						    GDA_CONNECTION_OPTIONS_NONE, &error);
		if (cnc) {
			msgdialog = gtk_message_dialog_new_with_markup (parent,
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
									"<b>%s</b>",
									_("Connection successfully opened!"));
			gda_connection_close (cnc, NULL);
		}
		else {
			msgdialog = gtk_message_dialog_new_with_markup (parent,
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
									"<b>%s:</b>\n%s",
									_("Could not open connection"),
									error->message ? error->message : _("No detail"));
			if (error)
				g_error_free (error);
		}

		gtk_dialog_run (GTK_DIALOG (msgdialog));
		gtk_widget_destroy (msgdialog);
	}
	if (test_dialog)
		gtk_widget_destroy (test_dialog);
}

static void
dsn_reset_cb (G_GNUC_UNUSED GSimpleAction *action,
              G_GNUC_UNUSED GVariant *state,
              gpointer data)
{
	GdauiDsnEditor *editor;
	editor = GDAUI_DSN_EDITOR (data);
	GdaDsnInfo *orig;
	orig = gda_config_get_dsn_info (editor->priv->name);
	gdaui_dsn_editor_set_dsn (editor, orig);
}

static void
copy_dlg_entry_changed_cb (GtkEntry *entry, GtkDialog *dlg)
{
	gboolean can_create = FALSE;
	const gchar *name;
	name = gtk_entry_get_text (entry);
	if (name && *name && !gda_config_get_dsn_info (name))
		can_create = TRUE;

	GtkWidget *resp;
	resp = gtk_dialog_get_widget_for_response (dlg, GTK_RESPONSE_ACCEPT);
	gtk_widget_set_sensitive (resp, can_create);
}

static void
dsn_copy_cb (G_GNUC_UNUSED GSimpleAction *action,
             G_GNUC_UNUSED GVariant *state,
             gpointer data)
{
	GdauiDsnEditor *editor;
	editor = GDAUI_DSN_EDITOR (data);

	GtkWindow *parent = NULL;
	parent = (GtkWindow*) gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_WINDOW);

	GtkWidget *dialog = NULL;
	dialog = gtk_dialog_new_with_buttons (_("Data source copy"), parent,
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      _("Create data source"), GTK_RESPONSE_ACCEPT,
					      _("_Cancel"), GTK_RESPONSE_REJECT,
					      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

	GtkWidget *grid, *dlg_box, *label, *entry;
	dlg_box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));	

	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (dlg_box), grid, TRUE, TRUE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (grid), 0);
        gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
        gtk_grid_set_column_spacing (GTK_GRID (grid), 5);

	gchar *str;
	str = g_strdup_printf (_("Define the name of the new data source which will be created as a copy "
				 "of '%s':"), editor->priv->name); 
        label = gtk_label_new (str);
	g_free (str);	    
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

        str = _gdaui_utility_markup_title (_("Data source name"), FALSE);
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        g_free (str);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

        entry = gtk_entry_new ();
	guint i;
	for (i = 1; ; i++) {
		str = g_strdup_printf ("%s_%u", editor->priv->name, i);
		if (! gda_config_get_dsn_info (str)) {
			gtk_entry_set_text (GTK_ENTRY (entry), str);
			g_free (str);
			break;
		}
		g_free (str);
	}
        gtk_editable_set_editable (GTK_EDITABLE (entry), TRUE);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (copy_dlg_entry_changed_cb), dialog);
	gtk_widget_set_hexpand (entry, TRUE);
        gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);

	gtk_widget_show_all (grid);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		const gchar *name;
		name = gtk_entry_get_text (GTK_ENTRY (entry));

		GdaDsnInfo *dsn_info = NULL;
		GError *error = NULL;
		dsn_info = gda_config_get_dsn_info (editor->priv->name);
		if (dsn_info) {
			dsn_info = gda_dsn_info_copy (dsn_info);
			g_free (dsn_info->name);
			dsn_info->name = g_strdup (name);
			gda_config_define_dsn (dsn_info, &error);
			gda_dsn_info_free (dsn_info);
		}
		else
			g_set_error (&error, GDA_CONFIG_ERROR, GDA_CONFIG_DSN_NOT_FOUND_ERROR,
				     ("DSN '%s' does not exist anymore"), editor->priv->name);

		if (error) {
			_gdaui_utility_show_error (GTK_WINDOW (dialog), _("Could not create data source: %s"),
						   error->message);
			g_clear_error (&error);
		}
	}
	gtk_widget_destroy (dialog);
}

static void
gdaui_dsn_editor_finalize (GObject *object)
{
	GdauiDsnEditor *config = (GdauiDsnEditor *) object;

	g_return_if_fail (GDAUI_IS_DSN_EDITOR (config));

	/* free memory */
	g_free (config->priv->name); 
	g_free (config->priv->dsn_info->provider); 
	g_free (config->priv->dsn_info->cnc_string); 
	g_free (config->priv->dsn_info->description);
	g_free (config->priv->dsn_info->auth_string);
	g_free (config->priv->dsn_info);
	g_free (config->priv);

	/* chain to parent class */
	parent_class->finalize (object);
}


GType
gdaui_dsn_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDsnEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_dsn_editor_class_init,
			NULL,
			NULL,
			sizeof (GdauiDsnEditor),
			0,
			(GInstanceInitFunc) gdaui_dsn_editor_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "GdauiDsnEditor", &info, 0);
	}
	return type;
}

/**
 * gdaui_dsn_editor_new
 *
 *
 *
 * Returns:
 */
GtkWidget *
gdaui_dsn_editor_new (void)
{
	GdauiDsnEditor *config;

	config = g_object_new (GDAUI_TYPE_DSN_EDITOR, NULL);
	return GTK_WIDGET (config);
}

/**
 * gdaui_dsn_editor_get_dsn
 * @config:
 *
 *
 *
 * Returns: a pointer to the currently configured DSN (do not modify)
 */
const GdaDsnInfo *
gdaui_dsn_editor_get_dsn (GdauiDsnEditor *config)
{
	GdaDsnInfo *dsn_info;

	g_return_val_if_fail (GDAUI_IS_DSN_EDITOR (config), NULL);
	dsn_info = config->priv->dsn_info;

	g_free (dsn_info->provider); dsn_info->provider = NULL;
	g_free (dsn_info->cnc_string); dsn_info->cnc_string = NULL;
	g_free (dsn_info->description); dsn_info->description = NULL;
	g_free (dsn_info->auth_string); dsn_info->auth_string = NULL;
	g_free (dsn_info->name); dsn_info->name = NULL;
	dsn_info->name = g_strdup (config->priv->name);
	dsn_info->provider = g_strdup (gdaui_provider_selector_get_provider 
				       (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));
	dsn_info->cnc_string = _gdaui_provider_spec_editor_get_specs (GDAUI_PROVIDER_SPEC_EDITOR (config->priv->dsn_spec));
	GtkTextBuffer *buf;
	GtkTextIter start, end;
	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (config->priv->wdesc));
	gtk_text_buffer_get_start_iter (buf, &start);
	gtk_text_buffer_get_end_iter (buf, &end);
	dsn_info->description = gtk_text_buffer_get_text (buf, &start, &end, FALSE);
	dsn_info->auth_string = _gdaui_provider_auth_editor_get_auth (GDAUI_PROVIDER_AUTH_EDITOR (config->priv->dsn_auth));
	dsn_info->is_system = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (config->priv->is_system));

	return dsn_info;
}

/**
 * gdaui_dsn_editor_set_dsn
 * @editor: a #GdauiDsnEditor widget
 * @dsn_info: (nullable): a #GdaDsnInfo pointer or %NULL
 *
 *
 * Requests that @editor update its contents with @dsn_info's contents
 */
void
gdaui_dsn_editor_set_dsn (GdauiDsnEditor *editor, const GdaDsnInfo *dsn_info)
{
	g_return_if_fail (GDAUI_IS_DSN_EDITOR (editor));
	editor->priv->no_change_signal = TRUE;

	if (dsn_info) {
		GdaProviderInfo *pinfo;
		pinfo = gda_config_get_provider_info (dsn_info->provider);

		GdkPixbuf *pix;
		pix = support_create_pixbuf_for_provider (pinfo);
		if (pix) {
			gtk_image_set_from_pixbuf (GTK_IMAGE (editor->priv->icon), pix);
			g_object_unref (pix);
		}
		else
			gtk_image_clear (GTK_IMAGE (editor->priv->icon));

		if (pinfo)
			gtk_widget_hide (editor->priv->warning);
		else
			gtk_widget_show (editor->priv->warning);

		gchar *tmp;
		tmp = g_markup_printf_escaped ("<big><b>%s</b></big>", dsn_info->name);
		gtk_label_set_markup (GTK_LABEL (editor->priv->wname), tmp);
		g_free (tmp);

		g_free (editor->priv->name);
		editor->priv->name = g_strdup (dsn_info->name);
		gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (editor->priv->wprovider), 
						      dsn_info->provider);
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (editor->priv->dsn_spec),
							 dsn_info->provider);
		_gdaui_provider_spec_editor_set_specs (GDAUI_PROVIDER_SPEC_EDITOR (editor->priv->dsn_spec),
						      dsn_info->cnc_string);
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->wdesc)),
					  dsn_info->description ? dsn_info->description : "", -1);
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (editor->priv->dsn_auth),
							 dsn_info->provider);
		_gdaui_provider_auth_editor_set_auth (GDAUI_PROVIDER_AUTH_EDITOR (editor->priv->dsn_auth),
						     dsn_info->auth_string);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->priv->is_system), dsn_info->is_system);
		
		if (dsn_info->is_system && !gda_config_can_modify_system_config ()) {
			gtk_widget_set_sensitive (editor->priv->wprovider, FALSE);
			gtk_text_view_set_editable (GTK_TEXT_VIEW (editor->priv->wdesc), pinfo ? TRUE : FALSE);
			gtk_widget_set_sensitive (editor->priv->dsn_spec, FALSE);
			gtk_widget_set_sensitive (editor->priv->dsn_auth, FALSE);
			gtk_widget_set_sensitive (editor->priv->is_system, FALSE);
		}
		else {
			gtk_widget_set_sensitive (editor->priv->wprovider, pinfo ? TRUE : FALSE);
			gtk_text_view_set_editable (GTK_TEXT_VIEW (editor->priv->wdesc), pinfo ? TRUE : FALSE);
			gtk_widget_set_sensitive (editor->priv->dsn_spec, TRUE);
			gtk_widget_set_sensitive (editor->priv->dsn_auth, TRUE);
			gtk_widget_set_sensitive (editor->priv->is_system,
						  pinfo && gda_config_can_modify_system_config () ? TRUE : FALSE);
		}
	}
	else {
		gtk_image_clear (GTK_IMAGE (editor->priv->icon));
		gtk_label_set_text (GTK_LABEL (editor->priv->wname), "");
		gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (editor->priv->wprovider), NULL);
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (editor->priv->dsn_spec), NULL);
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->wdesc)), "", -1);
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (editor->priv->dsn_auth), NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->priv->is_system), FALSE);

		gtk_widget_set_sensitive (editor->priv->wprovider, FALSE);
		gtk_text_view_set_editable (GTK_TEXT_VIEW (editor->priv->wdesc), FALSE);
		gtk_widget_set_sensitive (editor->priv->dsn_spec, FALSE);
		gtk_widget_set_sensitive (editor->priv->dsn_auth, FALSE);
		gtk_widget_set_sensitive (editor->priv->is_system, FALSE);
	}

	editor->priv->no_change_signal = FALSE;
	g_signal_emit (editor, gdaui_dsn_editor_signals[CHANGED], 0, NULL);
}

/**
 * gdaui_dsn_editor_has_been_changed:
 * @config: a #GdauiDsnEditor
 *
 * Tells if the user has some changes to the DSN being edited, which can be comitted
 *
 * Returns: %TRUE if some changes can be comitted
 */
gboolean
gdaui_dsn_editor_has_been_changed (GdauiDsnEditor *config)
{
	g_return_val_if_fail (GDAUI_IS_DSN_EDITOR (config), FALSE);
	GdaDsnInfo *orig;
	orig = gda_config_get_dsn_info (config->priv->name);

	return ! gda_dsn_info_equal (gdaui_dsn_editor_get_dsn (config), orig);
}

/**
 * gdaui_dsn_editor_show_pane:
 */
void
gdaui_dsn_editor_show_pane (GdauiDsnEditor *config, GdauiDsnEditorPaneType type)
{
	switch (type) {
	case GDAUI_DSN_EDITOR_PANE_DEFINITION:
		gtk_stack_set_visible_child_name (GTK_STACK (config->priv->stack), PANE_DEFINITION);
		break;
	case GDAUI_DSN_EDITOR_PANE_PARAMS:
		gtk_stack_set_visible_child_name (GTK_STACK (config->priv->stack), PANE_PARAMS);
		break;
	case GDAUI_DSN_EDITOR_PANE_AUTH:
		gtk_stack_set_visible_child_name (GTK_STACK (config->priv->stack), PANE_AUTH);
		break;
	default:
		g_assert_not_reached ();
	}
}

/**
 * gdaui_dsn_editor_need_authentication:
 */
gboolean
gdaui_dsn_editor_need_authentication (GdauiDsnEditor *config)
{
	const GdaDsnInfo *dsn_info;
	dsn_info = gdaui_dsn_editor_get_dsn (config);

	GdaProviderInfo *pinfo;
	pinfo = gda_config_get_provider_info (dsn_info->provider);
	if (pinfo && pinfo->auth_params && gda_set_get_holders (pinfo->auth_params))
		return TRUE;
	else
		return FALSE;
}
