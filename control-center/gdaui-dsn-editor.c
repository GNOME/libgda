/* GNOME DB library
 * Copyright (C) 1999 - 2009 The GNOME Foundation
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <string.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <libgda/libgda.h>
#include "gdaui-dsn-editor.h"
#include <libgda-ui/gdaui-provider-selector.h>
#include <libgda-ui/gdaui-basic-form.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/internal/gdaui-provider-spec-editor.h>
#include <libgda-ui/internal/gdaui-provider-auth-editor.h>

struct _GdauiDsnEditorPrivate {
	GtkWidget *wname;
	GtkWidget *wprovider;
	GtkWidget *wdesc;
	GtkWidget *is_system;
	GtkWidget *warning;

	GtkWidget *dsn_spec_expander;
	GtkWidget *dsn_spec;

	GtkWidget *dsn_auth_expander;
	GtkWidget *dsn_auth;	

	GdaDsnInfo *dsn_info;
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
gdaui_dsn_editor_class_init (GdauiDsnEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_dsn_editor_finalize;
	klass->changed = NULL;

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
	if (widget == config->priv->wprovider) 
		/* replace the expander's contents */
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (config->priv->dsn_spec),
						gdaui_provider_selector_get_provider 
						(GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));

	g_signal_emit (config, gdaui_dsn_editor_signals[CHANGED], 0, NULL);
}

static void
field_toggled_cb (GtkWidget *widget, GdauiDsnEditor *config)
{
	g_signal_emit (config, gdaui_dsn_editor_signals[CHANGED], 0, NULL);
}

static void
gdaui_dsn_editor_init (GdauiDsnEditor *config, GdauiDsnEditorClass *klass)
{
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *exp;
	gchar *str;

	g_return_if_fail (GDAUI_IS_DSN_EDITOR (config));

	/* allocate private structure */
	config->priv = g_new0 (GdauiDsnEditorPrivate, 1);
	config->priv->dsn_info = g_new0 (GdaDsnInfo, 1);

	/* set up widgets */
	table = gtk_table_new (8, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (config), table, TRUE, TRUE, 0);

	str = g_strdup_printf ("%s <span foreground='red' weight='bold'>*</span>", _("Data source _name:"));
	label = gtk_label_new ("");
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0, -1.);
	g_free (str);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	config->priv->wname = gtk_entry_new ();
        gtk_editable_set_editable (GTK_EDITABLE (config->priv->wname), FALSE);
	g_object_set (G_OBJECT (config->priv->wname), "can-focus", FALSE, NULL);

        gtk_widget_show (config->priv->wname);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), config->priv->wname);
	gtk_widget_show (config->priv->wname);
	gtk_editable_set_editable (GTK_EDITABLE (config->priv->wname), FALSE);
	g_signal_connect (G_OBJECT (config->priv->wname), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_table_attach (GTK_TABLE (table), config->priv->wname, 1, 2, 0, 1,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	label = gtk_label_new_with_mnemonic (_("_System wide data source:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, -1.);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	config->priv->is_system = gtk_check_button_new ();
	gtk_widget_show (config->priv->is_system);
	g_signal_connect (G_OBJECT (config->priv->is_system), "toggled",
			  G_CALLBACK (field_toggled_cb), config);
	gtk_table_attach (GTK_TABLE (table), config->priv->is_system, 1, 2, 1, 2,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	str = g_strdup_printf ("%s <span foreground='red' weight='bold'>*</span>", _("_Provider:"));
	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0, -1.);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	config->priv->wprovider = gdaui_provider_selector_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), config->priv->wprovider);
	gtk_widget_show (config->priv->wprovider);
	g_signal_connect (G_OBJECT (config->priv->wprovider), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_table_attach (GTK_TABLE (table), config->priv->wprovider, 1, 2, 2, 3,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	label = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, -1.);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
	config->priv->wdesc = gtk_entry_new ();
        gtk_editable_set_editable (GTK_EDITABLE (config->priv->wdesc), TRUE);
        gtk_widget_show (config->priv->wdesc);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), config->priv->wdesc);
	gtk_widget_show (config->priv->wdesc);
	g_signal_connect (G_OBJECT (config->priv->wdesc), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_table_attach (GTK_TABLE (table), config->priv->wdesc, 1, 2, 3, 4,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	config->priv->warning = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (config->priv->warning),
			      _("<span foreground='red'>The database provider used by this data source is not available,\n"
				"editing the data source's attributes is disabled</span>"));
	gtk_misc_set_alignment (GTK_MISC (config->priv->warning), 0.5, -1);
	gtk_label_set_justify (GTK_LABEL (config->priv->warning), GTK_JUSTIFY_CENTER);
	gtk_table_attach (GTK_TABLE (table), config->priv->warning, 0, 2, 8, 9,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	/* connection's spec */
	exp = gtk_expander_new (_("Connection's parameters"));
	config->priv->dsn_spec_expander = exp;
	gtk_widget_show (exp);
	gtk_table_attach (GTK_TABLE (table), config->priv->dsn_spec_expander, 0, 2, 6, 7,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
	config->priv->dsn_spec = _gdaui_provider_spec_editor_new (gdaui_provider_selector_get_provider 
								 (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));
	g_signal_connect (G_OBJECT (config->priv->dsn_spec), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_container_add (GTK_CONTAINER (config->priv->dsn_spec_expander), config->priv->dsn_spec);
	gtk_widget_show (config->priv->dsn_spec);

	/* connection's authentication */
	exp = gtk_expander_new (_("Authentication"));
	config->priv->dsn_auth_expander = exp;
	gtk_widget_show (exp);
	gtk_table_attach (GTK_TABLE (table), config->priv->dsn_auth_expander, 0, 2, 7, 8,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
	config->priv->dsn_auth = _gdaui_provider_auth_editor_new (gdaui_provider_selector_get_provider 
								 (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));
	g_signal_connect (G_OBJECT (config->priv->dsn_auth), "changed",
			  G_CALLBACK (field_changed_cb), config);
	gtk_container_add (GTK_CONTAINER (config->priv->dsn_auth_expander), config->priv->dsn_auth);
	gtk_widget_show (config->priv->dsn_auth);
}

static void
gdaui_dsn_editor_finalize (GObject *object)
{
	GdauiDsnEditor *config = (GdauiDsnEditor *) object;

	g_return_if_fail (GDAUI_IS_DSN_EDITOR (config));

	/* free memory */
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
			(GInstanceInitFunc) gdaui_dsn_editor_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiDsnEditor", &info, 0);
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
	dsn_info->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (config->priv->wname)));
	dsn_info->provider = g_strdup (gdaui_provider_selector_get_provider 
				       (GDAUI_PROVIDER_SELECTOR (config->priv->wprovider)));
	dsn_info->cnc_string = _gdaui_provider_spec_editor_get_specs (GDAUI_PROVIDER_SPEC_EDITOR (config->priv->dsn_spec));
	dsn_info->description = g_strdup (gtk_entry_get_text (GTK_ENTRY (config->priv->wdesc)));
	dsn_info->auth_string = _gdaui_provider_auth_editor_get_auth (GDAUI_PROVIDER_AUTH_EDITOR (config->priv->dsn_auth));
	dsn_info->is_system = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (config->priv->is_system));

	return dsn_info;
}

/**
 * gdaui_dsn_editor_set_dsn
 * @editor: a #GdauiDsnEditor widget
 * @dsn_info: a #GdaDsnInfo pointer or %NULL
 *
 *
 * Requests that @editor update its contents with @dsn_info's contents
 */
void
gdaui_dsn_editor_set_dsn (GdauiDsnEditor *editor, const GdaDsnInfo *dsn_info)
{
	g_return_if_fail (GDAUI_IS_DSN_EDITOR (editor));

	if (dsn_info) {
		GdaProviderInfo *pinfo;

		pinfo = gda_config_get_provider_info (dsn_info->provider);
		
		gtk_entry_set_text (GTK_ENTRY (editor->priv->wname), dsn_info->name);
		gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (editor->priv->wprovider), 
						      dsn_info->provider);
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (editor->priv->dsn_spec),
							 dsn_info->provider);
		_gdaui_provider_spec_editor_set_specs (GDAUI_PROVIDER_SPEC_EDITOR (editor->priv->dsn_spec),
						      dsn_info->cnc_string);
		gtk_entry_set_text (GTK_ENTRY (editor->priv->wdesc),
				    dsn_info->description ? dsn_info->description : "");
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (editor->priv->dsn_auth),
							 dsn_info->provider);
		_gdaui_provider_auth_editor_set_auth (GDAUI_PROVIDER_AUTH_EDITOR (editor->priv->dsn_auth),
						     dsn_info->auth_string);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->priv->is_system), dsn_info->is_system);
		
		if (dsn_info->is_system && !gda_config_can_modify_system_config ()) {
			gtk_editable_set_editable (GTK_EDITABLE (editor->priv->wname), FALSE);
			gtk_widget_set_sensitive (editor->priv->wprovider, FALSE);
			gtk_editable_set_editable (GTK_EDITABLE (editor->priv->wdesc), pinfo ? TRUE : FALSE);
			gtk_widget_set_sensitive (editor->priv->dsn_spec, FALSE);
			gtk_widget_set_sensitive (editor->priv->dsn_auth, FALSE);
			gtk_widget_set_sensitive (editor->priv->is_system, FALSE);
		}
		else {
			gtk_editable_set_editable (GTK_EDITABLE (editor->priv->wname), FALSE);
			gtk_widget_set_sensitive (editor->priv->wprovider, pinfo ? TRUE : FALSE);
			gtk_editable_set_editable (GTK_EDITABLE (editor->priv->wdesc), pinfo ? TRUE : FALSE);
			gtk_widget_set_sensitive (editor->priv->dsn_spec, TRUE);
			gtk_widget_set_sensitive (editor->priv->dsn_auth, TRUE);
			gtk_widget_set_sensitive (editor->priv->is_system,
						  pinfo && gda_config_can_modify_system_config () ? TRUE : FALSE);
		}
		if (pinfo) {
			gtk_widget_hide (editor->priv->warning);
			gtk_widget_show (editor->priv->dsn_spec_expander);
		}
		else {
			gtk_widget_show (editor->priv->warning);
			gtk_widget_hide (editor->priv->dsn_spec_expander);
		}

		if (pinfo && gda_config_dsn_needs_authentication (dsn_info->name))
			gtk_widget_show (editor->priv->dsn_auth_expander);
		else
			gtk_widget_hide (editor->priv->dsn_auth_expander);
	}
	else {
		gtk_entry_set_text (GTK_ENTRY (editor->priv->wname), "");
		gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (editor->priv->wprovider), NULL);
		_gdaui_provider_spec_editor_set_provider (GDAUI_PROVIDER_SPEC_EDITOR (editor->priv->dsn_spec), NULL);
		gtk_entry_set_text (GTK_ENTRY (editor->priv->wdesc), "");
		_gdaui_provider_auth_editor_set_provider (GDAUI_PROVIDER_AUTH_EDITOR (editor->priv->dsn_auth), NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->priv->is_system), FALSE);
		
		gtk_editable_set_editable (GTK_EDITABLE (editor->priv->wname), FALSE);
		gtk_widget_set_sensitive (editor->priv->wprovider, FALSE);
		gtk_editable_set_editable (GTK_EDITABLE (editor->priv->wdesc), FALSE);
		gtk_widget_set_sensitive (editor->priv->dsn_spec, FALSE);
		gtk_widget_set_sensitive (editor->priv->dsn_auth, FALSE);
		gtk_widget_set_sensitive (editor->priv->is_system, FALSE);

		gtk_widget_hide (editor->priv->dsn_spec_expander);
		gtk_widget_hide (editor->priv->dsn_auth_expander);
	}
}
