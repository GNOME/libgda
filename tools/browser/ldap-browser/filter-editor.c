/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "filter-editor.h"
#include <libgda-ui/gdaui-combo.h>
#include <libgda-ui/gdaui-data-selector.h>

struct _FilterEditorPrivate {
	TConnection *tcnc;
	GtkWidget *base_dn;
	GtkWidget *filter;
	GtkWidget *attributes;
	GtkWidget *scope;

	GdaLdapSearchScope default_scope;
};

static void filter_editor_class_init (FilterEditorClass *klass);
static void filter_editor_init       (FilterEditor *feditor, FilterEditorClass *klass);
static void filter_editor_dispose    (GObject *object);

static GObjectClass *parent_class = NULL;

/* signals */
enum {
        ACTIVATE,
	LAST_SIGNAL
};

gint filter_editor_signals [LAST_SIGNAL] = { 0 };

/*
 * FilterEditor class implementation
 */

static void
filter_editor_class_init (FilterEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	filter_editor_signals [ACTIVATE] =
                g_signal_new ("activate",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (FilterEditorClass, activate),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	klass->activate = NULL;
	object_class->dispose = filter_editor_dispose;
}

static void
filter_editor_init (FilterEditor *feditor, G_GNUC_UNUSED FilterEditorClass *klass)
{
	feditor->priv = g_new0 (FilterEditorPrivate, 1);
	feditor->priv->tcnc = NULL;
	feditor->priv->default_scope = GDA_LDAP_SEARCH_SUBTREE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (feditor), GTK_ORIENTATION_VERTICAL);
}

static void
filter_editor_dispose (GObject *object)
{
	FilterEditor *feditor = (FilterEditor *) object;

	/* free memory */
	if (feditor->priv) {
		if (feditor->priv->tcnc)
			g_object_unref (feditor->priv->tcnc);
		g_free (feditor->priv);
		feditor->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
filter_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (FilterEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) filter_editor_class_init,
			NULL,
			NULL,
			sizeof (FilterEditor),
			0,
			(GInstanceInitFunc) filter_editor_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_BOX, "FilterEditor", &info, 0);
	}
	return type;
}

static void
activated_cb (G_GNUC_UNUSED GtkEntry *entry, FilterEditor *feditor)
{
	g_signal_emit (feditor, filter_editor_signals [ACTIVATE], 0);
}

/**
 * filter_editor_new:
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
filter_editor_new (TConnection *tcnc)
{
	FilterEditor *feditor;
	GtkWidget *grid, *label, *entry;
	GdaDataModel *model;
	GList *values;
	GValue *v1, *v2;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	feditor = FILTER_EDITOR (g_object_new (FILTER_EDITOR_TYPE, NULL));
	feditor->priv->tcnc = T_CONNECTION (g_object_ref ((GObject*) tcnc));

	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
	gtk_box_pack_start (GTK_BOX (feditor), grid, TRUE, TRUE, 0);
	
	label = gtk_label_new (_("Base DN:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	label = gtk_label_new (_("Filter expression:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	label = gtk_label_new (_("Attributes to fetch:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
	label = gtk_label_new (_("Search scope:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
	
	entry = gtk_entry_new ();
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
	feditor->priv->base_dn = entry;
	g_signal_connect (entry, "activate",
			  G_CALLBACK (activated_cb), feditor);

	entry = gtk_entry_new ();
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
	feditor->priv->filter = entry;
	g_signal_connect (entry, "activate",
			  G_CALLBACK (activated_cb), feditor);

	entry = gtk_entry_new ();
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 1, 1);
	feditor->priv->attributes = entry;
	g_signal_connect (entry, "activate",
			  G_CALLBACK (activated_cb), feditor);

	model = gda_data_model_array_new_with_g_types (2, G_TYPE_INT, G_TYPE_STRING);
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "Base (search the base DN only)");
	values = g_list_prepend (NULL, v1);
	g_value_set_int ((v2 = gda_value_new (G_TYPE_INT)), GDA_LDAP_SEARCH_BASE);
	values = g_list_prepend (values, v2);
	g_assert (gda_data_model_append_values (model, values, NULL) >= 0);
	gda_value_free (v1);
	gda_value_free (v2);

	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "Onelevel (search immediate children of base DN only)");
	values = g_list_prepend (NULL, v1);
	g_value_set_int ((v2 = gda_value_new (G_TYPE_INT)), GDA_LDAP_SEARCH_ONELEVEL);
	values = g_list_prepend (values, v2);
	g_assert (gda_data_model_append_values (model, values, NULL) >= 0);
	gda_value_free (v1);
	gda_value_free (v2);

	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "Subtree (search of the base DN and the entire subtree below)");
	values = g_list_prepend (NULL, v1);
	g_value_set_int ((v2 = gda_value_new (G_TYPE_INT)), GDA_LDAP_SEARCH_SUBTREE);
	values = g_list_prepend (values, v2);
	g_assert (gda_data_model_append_values (model, values, NULL) >= 0);
	gda_value_free (v1);
	gda_value_free (v2);

	gint cols[] = {1};
	entry = gdaui_combo_new_with_model (model, 1, cols);
	gtk_widget_set_hexpand (entry, TRUE);
	g_object_unref (model);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 3, 1, 1);
	feditor->priv->scope = entry;
	filter_editor_clear (feditor);

	gtk_widget_show_all (grid);
	return (GtkWidget*) feditor;
}

/**
 * filter_editor_clear:
 */
void
filter_editor_clear (FilterEditor *fedit)
{
	g_return_if_fail (IS_FILTER_EDITOR (fedit));
	filter_editor_set_settings (fedit, NULL, NULL, NULL, fedit->priv->default_scope);
}

/**
 * filter_editor_set_settings:
 */
void
filter_editor_set_settings (FilterEditor *fedit,
			    const gchar *base_dn, const gchar *filter,
			    const gchar *attributes, GdaLdapSearchScope scope)
{
	g_return_if_fail (IS_FILTER_EDITOR (fedit));

	gtk_entry_set_text (GTK_ENTRY (fedit->priv->base_dn), base_dn ? base_dn : "");
	gtk_entry_set_text (GTK_ENTRY (fedit->priv->filter), filter ? filter : "(cn=*)");
	gtk_entry_set_text (GTK_ENTRY (fedit->priv->attributes), attributes ? attributes : "cn");
	gdaui_data_selector_select_row (GDAUI_DATA_SELECTOR (fedit->priv->scope), scope - 1);
}

/**
 * filter_editor_get_settings:
 */
void
filter_editor_get_settings (FilterEditor *fedit,
			    gchar **out_base_dn, gchar **out_filter,
			    gchar **out_attributes, GdaLdapSearchScope *out_scope)
{
	const gchar *tmp;
	g_return_if_fail (IS_FILTER_EDITOR (fedit));
	if (out_base_dn) {
		tmp = gtk_entry_get_text (GTK_ENTRY (fedit->priv->base_dn));
		*out_base_dn = tmp && *tmp ? g_strdup (tmp) : NULL;
	}
	if (out_filter) {
		tmp = gtk_entry_get_text (GTK_ENTRY (fedit->priv->filter));
		if (tmp && *tmp) {
			/* add surrounding parenthesis if not yet there */
			if (*tmp != '(') {
				gint len;
				len = strlen (tmp);
				if (tmp [len-1] != ')')
					*out_filter = g_strdup_printf ("(%s)", tmp);
				else
					*out_filter = g_strdup (tmp);/* may result in an error when executed */	
			}
			else
				*out_filter = g_strdup (tmp);

		}
		else
			*out_filter = NULL;
	}
	if (out_attributes) {
		tmp = gtk_entry_get_text (GTK_ENTRY (fedit->priv->attributes));
		*out_attributes = tmp && *tmp ? g_strdup (tmp) : NULL;
	}
	if (out_scope)
		*out_scope = gtk_combo_box_get_active (GTK_COMBO_BOX (fedit->priv->scope)) + 1;
}
