/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2014 Anders Jonsson <anders.jonsson@norsjovallen.se>
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
#include <libgda/gda-config.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdaui-dsn-assistant.h"
#include <libgda-ui/internal/gdaui-provider-spec-editor.h>
#include <libgda-ui/internal/gdaui-provider-auth-editor.h>
#include <libgda-ui/gdaui-provider-selector.h>
#include <libgda-ui/gdaui-server-operation.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/internal/utility.h>
#include <libgda/binreloc/gda-binreloc.h>

enum {
	PAGE_START           = 0,
	PAGE_GENERAL_INFO    = 1,
	PAGE_OPT_CREATE_DB   = 2,
	PAGE_CREATE_DB_INFO  = 3,
	PAGE_CONNECT_INFO    = 4,
	PAGE_AUTH_INFO       = 5,
	PAGE_LAST            = 6
};

struct _GdauiDsnAssistantPrivate {
	GdaDsnInfo  *dsn_info;
	GdaServerOperation *create_db_op;

	/* widgets */
	GtkWidget *general_page;
	GtkWidget *general_name;
	GtkWidget *general_provider;
	GtkWidget *general_description;
	GtkWidget *general_is_system;

	GtkWidget *choose_toggle;

	GtkWidget *newdb_box;
	GtkWidget *newdb_params;

	GtkWidget *cnc_params_page;
	GtkWidget *provider_container;
	GtkWidget *provider_detail;

	GtkWidget *cnc_auth_page;
	GtkWidget *auth_container;
	GtkWidget *auth_detail;

	GtkSizeGroup *size_group;
};

static void gdaui_dsn_assistant_class_init (GdauiDsnAssistantClass *klass);
static void gdaui_dsn_assistant_init       (GdauiDsnAssistant *assistant,
						  GdauiDsnAssistantClass *klass);
static void gdaui_dsn_assistant_finalize   (GObject *object);

enum {
	FINISHED,
	LAST_SIGNAL
};

static guint config_assistant_signals[LAST_SIGNAL] = { 0, };
static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

static void
assistant_cancelled_cb (GtkAssistant *assistant, G_GNUC_UNUSED gpointer data)
{
	g_return_if_fail (GDAUI_IS_DSN_ASSISTANT (assistant));
	g_signal_emit_by_name (G_OBJECT (assistant), "finished", TRUE);
	g_signal_emit_by_name (G_OBJECT (assistant), "close");
}

static void
assistant_applied_cb (GtkAssistant *assist, G_GNUC_UNUSED gpointer data)
{
	gboolean allok = TRUE;
	GString *cnc_string = NULL;
	GdauiDsnAssistant *assistant = (GdauiDsnAssistant *) assist;
  GError *error = NULL;

	g_return_if_fail (GDAUI_IS_DSN_ASSISTANT (assistant));

	/* clear the internal dsn_info */
	if (assistant->priv->dsn_info) {
		gda_dsn_info_free (assistant->priv->dsn_info);
		assistant->priv->dsn_info = NULL;
	}

	/* New database creation first */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (assistant->priv->choose_toggle))) {
		if (!gda_server_operation_is_valid (assistant->priv->create_db_op, NULL, &error)) {
      gchar *msg = "No error details";
      if (error != NULL && error->message != NULL) {
        msg = error->message;
      }
			_gdaui_utility_show_error (NULL, _("Missing mandatory information, to create database: '%s'"), msg);
      if (error != NULL)
        g_error_free (error);
			gtk_assistant_set_current_page (assist, PAGE_CREATE_DB_INFO);
			return;
		}
		else {
			GdaProviderInfo *prov_info;
			GSList *dsn_params;
			GError *error = NULL;

			allok = gda_server_operation_perform_create_database (assistant->priv->create_db_op, NULL, &error);
			if (!allok) {
				gchar *str;
				str = g_strdup_printf (_("Error creating database: %s"), 
						       error && error->message ? error->message : _("Unknown error"));
				_gdaui_utility_show_error (NULL, str);
				g_free (str);
				
				gtk_assistant_set_current_page (assist, PAGE_CREATE_DB_INFO);
				return;
			}
			
			/* make the connection string for the data source */
			prov_info = gda_config_get_provider_info (gdaui_provider_selector_get_provider 
								  (GDAUI_PROVIDER_SELECTOR (assistant->priv->general_provider)));
			g_return_if_fail (prov_info);
			for (dsn_params = gda_set_get_holders (prov_info->dsn_params); dsn_params; dsn_params = dsn_params->next) {
				GdaHolder *param = GDA_HOLDER (dsn_params->data);
				const GValue *value;
				
				value = gda_server_operation_get_value_at (assistant->priv->create_db_op,
									   "/DB_DEF_P/%s",
									   gda_holder_get_id (param));
				if (!value)
					value = gda_server_operation_get_value_at (assistant->priv->create_db_op,
										   "/SERVER_CNX_P/%s",
										   gda_holder_get_id (param));
				
				if (value && !gda_value_is_null ((GValue *) value)) {
					gchar *str;

					if (dsn_params == gda_set_get_holders (prov_info->dsn_params))
						cnc_string = g_string_new ("");
					else
						g_string_append (cnc_string, ";");
					str = gda_value_stringify ((GValue *) value);
					g_string_append_printf (cnc_string, "%s=%s", gda_holder_get_id (param), str);
					g_free (str);
				}
			}
		}
	}

	/* Data source declaration */
	if (allok) {
		assistant->priv->dsn_info = gda_dsn_info_new ();
		assistant->priv->dsn_info->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (assistant->priv->general_name)));
		assistant->priv->dsn_info->provider = g_strdup (
					 gdaui_provider_selector_get_provider (
					 GDAUI_PROVIDER_SELECTOR (assistant->priv->general_provider)));
		if (cnc_string) {
			assistant->priv->dsn_info->cnc_string = cnc_string->str;
			g_string_free (cnc_string, FALSE);
		}
		else
			assistant->priv->dsn_info->cnc_string = _gdaui_provider_spec_editor_get_specs 
				(GDAUI_PROVIDER_SPEC_EDITOR (assistant->priv->provider_detail));
		assistant->priv->dsn_info->description =
			g_strdup (gtk_entry_get_text (GTK_ENTRY (assistant->priv->general_description)));
		assistant->priv->dsn_info->auth_string = NULL;
		if (assistant->priv->auth_detail)
			assistant->priv->dsn_info->auth_string =
				_gdaui_provider_auth_editor_get_auth (GDAUI_PROVIDER_AUTH_EDITOR (assistant->priv->auth_detail));
		if (gda_config_can_modify_system_config ())
			assistant->priv->dsn_info->is_system = gtk_toggle_button_get_active 
				(GTK_TOGGLE_BUTTON (assistant->priv->general_is_system));
		else
			assistant->priv->dsn_info->is_system = FALSE;
	}

	/* notify listeners */
	g_signal_emit (G_OBJECT (assistant), config_assistant_signals[FINISHED], 0, !allok);
}

static GdaServerOperation *
get_specs_database_creation (GdauiDsnAssistant *assistant)
{
	if (! assistant->priv->create_db_op) 
		assistant->priv->create_db_op = 
			gda_server_operation_prepare_create_database (gdaui_provider_selector_get_provider (
						     GDAUI_PROVIDER_SELECTOR (assistant->priv->general_provider)),
						     NULL, NULL);

	return assistant->priv->create_db_op;
}

static void
dsn_spec_changed_cb (GdauiProviderSpecEditor *spec, GdauiDsnAssistant *assistant)
{
	gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant),
					 assistant->priv->cnc_params_page,
					 _gdaui_provider_spec_editor_is_valid (spec));
}

static void
dsn_auth_changed_cb (GdauiProviderAuthEditor *auth, GdauiDsnAssistant *assistant)
{
	gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant),
					 assistant->priv->cnc_auth_page,
					 _gdaui_provider_auth_editor_is_valid (auth));
}

static void
provider_changed_cb (G_GNUC_UNUSED GtkWidget *combo, GdauiDsnAssistant *assistant)
{
	GdaServerOperation *op;
	const gchar *provider;

	/* clean any previous Provider specific stuff */
	if (assistant->priv->newdb_params) {
		gtk_widget_destroy (assistant->priv->newdb_params);
		assistant->priv->newdb_params = NULL;
	}

	if (assistant->priv->create_db_op) {
		g_object_unref (assistant->priv->create_db_op);
		assistant->priv->create_db_op = NULL;
	}

	if (!assistant->priv->size_group)
		assistant->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* is the database creation supported by the chosen provider? */
	op = get_specs_database_creation (assistant);
	if (op) {
		assistant->priv->newdb_params = g_object_new (GDAUI_TYPE_SERVER_OPERATION, 
							      "hide-single-header", TRUE,
							      "server-operation", op, NULL);
		gtk_widget_show (assistant->priv->newdb_params);
		gtk_container_add (GTK_CONTAINER (assistant->priv->newdb_box), 
				   assistant->priv->newdb_params);
		assistant->priv->create_db_op = op;
		gtk_widget_set_sensitive (assistant->priv->choose_toggle, TRUE);
	}
	else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (assistant->priv->choose_toggle),
					      FALSE);
		gtk_widget_set_sensitive (assistant->priv->choose_toggle, FALSE);
	}

	/* dsn spec for the selected provider */
	provider = gdaui_provider_selector_get_provider (GDAUI_PROVIDER_SELECTOR (assistant->priv->general_provider));
	if (provider == NULL) {
    GtkWidget *parent = gtk_widget_get_toplevel(combo);
    if (!GTK_IS_WINDOW (parent)) {
      parent = NULL;
    }
    GtkWidget *msg = gtk_message_dialog_new (GTK_WINDOW (parent),
                        GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        _("No provider exists"));
    gtk_dialog_run (GTK_DIALOG (msg));
    gtk_widget_destroy (msg);
    return;
  }
	if (!assistant->priv->provider_detail) {
		assistant->priv->provider_detail = _gdaui_provider_spec_editor_new (provider);
		gtk_box_pack_start (GTK_BOX (assistant->priv->provider_container),
				    assistant->priv->provider_detail, TRUE, TRUE, 0);
		gtk_widget_show (assistant->priv->provider_detail);
		g_signal_connect (assistant->priv->provider_detail, "changed",
				  G_CALLBACK (dsn_spec_changed_cb), assistant);
		_gdaui_provider_spec_editor_add_to_size_group (GDAUI_PROVIDER_SPEC_EDITOR (assistant->priv->provider_detail),
							       assistant->priv->size_group,
							       GDAUI_BASIC_FORM_LABELS);
	}
	else
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (assistant->priv->provider_detail), provider);

	/* dsn authentication for the selected provider */
	if (!assistant->priv->auth_detail) {
		assistant->priv->auth_detail = _gdaui_provider_auth_editor_new (provider);
		gtk_box_pack_start (GTK_BOX (assistant->priv->auth_container),
				    assistant->priv->auth_detail, TRUE, TRUE, 0);
		gtk_widget_show (assistant->priv->auth_detail);
		g_signal_connect (assistant->priv->auth_detail, "changed",
				  G_CALLBACK (dsn_auth_changed_cb), assistant);
		_gdaui_provider_auth_editor_add_to_size_group (GDAUI_PROVIDER_AUTH_EDITOR (assistant->priv->auth_detail),
							       assistant->priv->size_group,
							       GDAUI_BASIC_FORM_LABELS);
	}
	else
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (assistant->priv->auth_detail), provider);
}

static void
dsn_name_changed_cb (GtkEntry *entry, GdauiDsnAssistant *assistant)
{
	const gchar *name;
	gboolean page_complete = TRUE;
	GdaDsnInfo *dsn_info;

	/* check required fields have values */
	name = gtk_entry_get_text (GTK_ENTRY (assistant->priv->general_name));
	if (!name || strlen (name) < 1) {
		gtk_widget_grab_focus (assistant->priv->general_name);
		page_complete = FALSE;
	}

	dsn_info = gda_config_get_dsn_info (name);
	if (dsn_info) {
		gint i = 2;
		gchar *str = NULL;

		do {
			if (str != NULL)
				g_free (str);
			str = g_strdup_printf ("%s_%d", name, i);
			dsn_info = gda_config_get_dsn_info (str);
			i++;
		} while (dsn_info);

		gtk_entry_set_text (entry, str);
		g_free (str);
		/*gtk_widget_grab_focus (assistant->priv->general_name);*/
	}

	gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), 
					 assistant->priv->general_page,
					 page_complete);
}

/*
 * GdauiDsnAssistant class implementation
 */

static void
gdaui_dsn_assistant_class_init (GdauiDsnAssistantClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	config_assistant_signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDsnAssistantClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	klass->finished = NULL;
	object_class->finalize = gdaui_dsn_assistant_finalize;
}

static gint 
forward_page_function (gint current_page, GdauiDsnAssistant *assistant)
{
	switch (current_page) {
	case PAGE_START:
		return PAGE_GENERAL_INFO;
	case PAGE_GENERAL_INFO:
		if (assistant->priv->newdb_params)
			return PAGE_OPT_CREATE_DB;
		else
			return PAGE_CONNECT_INFO;
	case PAGE_OPT_CREATE_DB:
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (assistant->priv->choose_toggle)))
			return PAGE_CREATE_DB_INFO;
		else
			return PAGE_CONNECT_INFO;
	case PAGE_CREATE_DB_INFO:
		return PAGE_LAST;
	case PAGE_CONNECT_INFO: {
		GdaProviderInfo *pinfo;
		const gchar *provider;
		provider = gdaui_provider_selector_get_provider (GDAUI_PROVIDER_SELECTOR (assistant->priv->general_provider));
		if (provider == NULL) {
      GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(assistant));
      if (!GTK_IS_WINDOW (parent)) {
        parent = NULL;
      }
      GtkWidget *msg = gtk_message_dialog_new (GTK_WINDOW (parent),
                          GTK_DIALOG_MODAL,
                          GTK_MESSAGE_ERROR,
                          GTK_BUTTONS_CLOSE,
                          _("No providers exists"));
      gtk_dialog_run (GTK_DIALOG (msg));
      gtk_widget_destroy (msg);
      return PAGE_CONNECT_INFO;
    }
		pinfo = gda_config_get_provider_info (provider);
    if (pinfo == NULL) {
      GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(assistant));
      if (!GTK_IS_WINDOW (parent)) {
        parent = NULL;
      }
      GtkWidget *msg = gtk_message_dialog_new (GTK_WINDOW (parent),
                          GTK_DIALOG_MODAL,
                          GTK_MESSAGE_ERROR,
                          GTK_BUTTONS_CLOSE,
                          _("No provider's information exists"));
      gtk_dialog_run (GTK_DIALOG (msg));
      gtk_widget_destroy (msg);
      return PAGE_CONNECT_INFO;
    }

		if (gda_set_get_holders (pinfo->auth_params))
			return PAGE_AUTH_INFO;
		else
			return PAGE_LAST;
	}
	case PAGE_AUTH_INFO:
		return PAGE_LAST;
	case PAGE_LAST:
		break;
	default:
		g_assert_not_reached ();
	}
	return -1;
}

static void
gdaui_dsn_assistant_init (GdauiDsnAssistant *assistant,
			     G_GNUC_UNUSED GdauiDsnAssistantClass *klass)
{
	GtkWidget *label, *vbox, *grid;
	GtkAssistant *assist;
	GtkStyleContext *context;

	g_return_if_fail (GDAUI_IS_DSN_ASSISTANT (assistant));

	/* global assistant settings */
	assist = GTK_ASSISTANT (assistant);
	gtk_window_set_title (GTK_WINDOW (assist), _("New data source definition"));
	gtk_container_set_border_width (GTK_CONTAINER (assist), 0);
	g_signal_connect (assist, "cancel", G_CALLBACK (assistant_cancelled_cb), NULL);
	g_signal_connect (assist, "apply", G_CALLBACK (assistant_applied_cb), NULL);
	gtk_assistant_set_forward_page_func (assist, (GtkAssistantPageFunc) forward_page_function, 
					     assistant, NULL);

	/* create private structure */
	assistant->priv = g_new0 (GdauiDsnAssistantPrivate, 1);
	assistant->priv->dsn_info = g_new0 (GdaDsnInfo, 1);
	assistant->priv->provider_detail = NULL;
	assistant->priv->create_db_op = NULL;

	/* 
	 * start page
	 */
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label),
			      _("This assistant will guide you through the process of "
				"creating a new data source, and optionally will allow you to "
				"create a new database.\n\nJust follow the steps!"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	gtk_assistant_append_page (assist, label);
	gtk_assistant_set_page_title (assist, label, _("Add a new data source..."));
	
	gtk_assistant_set_page_type (assist, label, GTK_ASSISTANT_PAGE_INTRO);
	gtk_assistant_set_page_complete (assist, label, TRUE);

	/* 
	 * general info page 
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 0);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label),
			      _("The following fields represent the basic information "
				"items for your new data source. Mandatory fields are marked "
				"with a star. "
				"To create a local database in a file, select the 'SQLite' type "
				"of database."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

	label = gtk_label_new (_("Data source name"));
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "required-label");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	assistant->priv->general_name = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (assistant->priv->general_name), TRUE);
        gtk_widget_show (assistant->priv->general_name);
	gtk_grid_attach (GTK_GRID (grid), assistant->priv->general_name, 1, 1, 1, 1);
	g_signal_connect (assistant->priv->general_name, "changed",
			  G_CALLBACK (dsn_name_changed_cb), assistant);

	if (gda_config_can_modify_system_config ()) {
		label = gtk_label_new (_("System wide data source:"));
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

		assistant->priv->general_is_system = gtk_check_button_new ();
		gtk_grid_attach (GTK_GRID (grid), assistant->priv->general_is_system, 1, 2, 1, 1);
	}
	else
		assistant->priv->general_is_system = NULL;
	label = gtk_label_new (_("Database type"));
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "required-label");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
	
	assistant->priv->general_provider = gdaui_provider_selector_new ();
	gtk_grid_attach (GTK_GRID (grid), assistant->priv->general_provider, 1, 3, 1, 1);

	label = gtk_label_new (_("Description:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);
	
	assistant->priv->general_description = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (assistant->priv->general_description), TRUE);
        gtk_widget_show (assistant->priv->general_description);
	gtk_grid_attach (GTK_GRID (grid), assistant->priv->general_description, 1, 4, 1, 1);
	
	gtk_widget_show_all (vbox);

	gtk_assistant_append_page (assist, vbox);
	gtk_assistant_set_page_title (assist, vbox, _("General Information"));
	gtk_assistant_set_page_type (assist, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	assistant->priv->general_page = vbox;

	/*
	 * Choose between existing database or create a new one
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 0);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);

	label = gtk_label_new (_("This page lets you choose between using an existing database "
				"or to create a new database to use with this new data source"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

	label = gtk_label_new (_("Create a new database:"));
  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "required-label");
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	
	assistant->priv->choose_toggle = gtk_check_button_new ();
	gtk_grid_attach (GTK_GRID (grid), assistant->priv->choose_toggle, 1, 1, 1, 1);

	gtk_widget_show_all (vbox);

	gtk_assistant_append_page (assist, vbox);
	gtk_assistant_set_page_title (assist, vbox, _("Create a new database?"));
	gtk_assistant_set_page_type (assist, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (assist, vbox, TRUE);

	/*
	 * New database information page
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
	
	label = gtk_label_new (_("The following fields represent the information needed "
				"to create a new database "
				"(mandatory fields are marked with a star). "
				"This information is database-specific, so check "
				"the manual for more information."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	
	GtkWidget *sw, *vp;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	vp = gtk_viewport_new (NULL, NULL);
  context = gtk_widget_get_style_context (vp);
  gtk_style_context_add_class (context, "transparent-background");
	gtk_widget_set_name (vp, "gdaui-transparent-background");
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), vp);
	assistant->priv->newdb_box = vp;
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	assistant->priv->newdb_params = NULL;

	gtk_widget_show_all (vbox);

	gtk_assistant_append_page (assist, vbox);
	gtk_assistant_set_page_title (assist, vbox, _("New database definition"));
	gtk_assistant_set_page_type (assist, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (assist, vbox, TRUE);

	/* 
	 * provider parameters to open connection page 
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
	
	label = gtk_label_new (("The following fields represent the information needed "
				"to open a connection (mandatory fields are marked with a star). "
				"This information is database-specific, so check "
				"the manual for more information."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	assistant->priv->provider_container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), assistant->priv->provider_container, TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);

	gtk_assistant_append_page (assist, vbox);
	gtk_assistant_set_page_title (assist, vbox, _("Connection's parameters"));
	gtk_assistant_set_page_type (assist, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	assistant->priv->cnc_params_page = vbox;

	/* 
	 * authentication page 
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
	
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label),
			      _("The following fields represent the authentication information needed "
				"to open a connection."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	assistant->priv->auth_container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), assistant->priv->auth_container, TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);

	gtk_assistant_append_page (assist, vbox);
	gtk_assistant_set_page_title (assist, vbox, _("Authentication parameters"));
	gtk_assistant_set_page_type (assist, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	assistant->priv->cnc_auth_page = vbox;

	/* 
	 * end page 
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label),
			      _("All information needed to create a new data source "
				"has been retrieved. Now, press 'Apply' to close "
				"this dialog."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);

	gtk_assistant_append_page (assist, vbox);
	gtk_assistant_set_page_title (assist, vbox, _("Ready to add a new data source"));
	gtk_assistant_set_page_type (assist, vbox, GTK_ASSISTANT_PAGE_CONFIRM);
	gtk_assistant_set_page_complete (assist, vbox, TRUE);

	/* force correct init */
	provider_changed_cb (assistant->priv->general_provider, assistant);

	g_signal_connect (G_OBJECT (assistant->priv->general_provider), "changed",
			  G_CALLBACK (provider_changed_cb), assistant);
}

static void
gdaui_dsn_assistant_finalize (GObject *object)
{
	GdauiDsnAssistant *assistant = (GdauiDsnAssistant *) object;

	g_return_if_fail (GDAUI_IS_DSN_ASSISTANT (assistant));

	/* free memory */
	gda_dsn_info_free (assistant->priv->dsn_info);

	if (assistant->priv->create_db_op)
		g_object_unref (assistant->priv->create_db_op);

	if (assistant->priv->size_group)
		g_object_unref (assistant->priv->size_group);

	g_free (assistant->priv);
	assistant->priv = NULL;

	parent_class->finalize (object);
}

GType
gdaui_dsn_assistant_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDsnAssistantClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_dsn_assistant_class_init,
			NULL,
			NULL,
			sizeof (GdauiDsnAssistant),
			0,
			(GInstanceInitFunc) gdaui_dsn_assistant_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_ASSISTANT, "GdauiDsnAssistant",
					       &info, 0);
	}
	return type;
}

/**
 * gdaui_dsn_assistant_new
 *
 *
 *
 * Returns:
 */
GtkWidget *
gdaui_dsn_assistant_new (void)
{
	GdauiDsnAssistant *assistant;

	assistant = g_object_new (GDAUI_TYPE_DSN_ASSISTANT, NULL);
	gtk_window_set_default_size (GTK_WINDOW (assistant), 500, -1);
	return GTK_WIDGET (assistant);
}

/**
 * gdaui_dsn_assistant_get_dsn
 * @assistant:
 *
 *
 *
 * Returns:
 */
const GdaDsnInfo *
gdaui_dsn_assistant_get_dsn (GdauiDsnAssistant *assistant)
{
	g_return_val_if_fail (GDAUI_IS_DSN_ASSISTANT (assistant), NULL);
	return (const GdaDsnInfo *) assistant->priv->dsn_info;
}
