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

#include <string.h>
#include <libgda/libgda.h>
#include "gdaui-provider-auth-editor.h"
#include <libgda-ui/gdaui-basic-form.h>
#include <glib/gi18n-lib.h>

struct _GdauiProviderAuthEditorPrivate {
	gchar           *provider;
	GdaProviderInfo *pinfo;

	GtkWidget       *auth_widget;
	gboolean         auth_needed;

	GtkSizeGroup    *labels_size_group;
	GtkSizeGroup    *entries_size_group;
};

static void gdaui_provider_auth_editor_class_init (GdauiProviderAuthEditorClass *klass);
static void gdaui_provider_auth_editor_init       (GdauiProviderAuthEditor *auth,
						   GdauiProviderAuthEditorClass *klass);
static void gdaui_provider_auth_editor_finalize   (GObject *object);

static void gdaui_provider_auth_editor_set_property (GObject *object,
						     guint param_id,
						     const GValue *value,
						     GParamSpec *pspec);
static void gdaui_provider_auth_editor_get_property (GObject *object,
						     guint param_id,
						     GValue *value,
						     GParamSpec *pspec);

static void gdaui_provider_auth_editor_grab_focus (GtkWidget *widget);

enum {
	PROP_0,
	PROP_PROVIDER
};

enum {
	CHANGED,
	LAST_SIGNAL
};


static gint gdaui_provider_auth_editor_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * GdauiProviderAuthEditor class implementation
 */

static void
gdaui_provider_auth_editor_class_init (GdauiProviderAuthEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_provider_auth_editor_finalize;
	object_class->set_property = gdaui_provider_auth_editor_set_property;
	object_class->get_property = gdaui_provider_auth_editor_get_property;
	klass->changed = NULL;
	GTK_WIDGET_CLASS (klass)->grab_focus = gdaui_provider_auth_editor_grab_focus;

	g_object_class_install_property (object_class, PROP_PROVIDER,
	                                 g_param_spec_string ("provider", NULL, NULL, NULL,
					                      G_PARAM_READWRITE));

	/* add class signals */
	gdaui_provider_auth_editor_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiProviderAuthEditorClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
gdaui_provider_auth_editor_grab_focus (GtkWidget *widget)
{
	GdauiProviderAuthEditor *aed = GDAUI_PROVIDER_AUTH_EDITOR (widget);
	if (aed->priv->auth_widget)
		gdaui_basic_form_entry_grab_focus (GDAUI_BASIC_FORM (aed->priv->auth_widget), NULL);
}

static void
auth_form_changed (G_GNUC_UNUSED GdauiBasicForm *form, G_GNUC_UNUSED GdaHolder *param, gboolean is_user_modif,
		   GdauiProviderAuthEditor *auth)
{
	if (! is_user_modif)
		return;

	g_signal_emit_by_name (auth, "changed");
}

static void
gdaui_provider_auth_editor_init (GdauiProviderAuthEditor *auth,
				 G_GNUC_UNUSED GdauiProviderAuthEditorClass *klass)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (auth), GTK_ORIENTATION_VERTICAL);

	auth->priv = g_new0 (GdauiProviderAuthEditorPrivate, 1);
	auth->priv->provider = NULL;
	auth->priv->auth_needed = FALSE;
}

static void
gdaui_provider_auth_editor_finalize (GObject *object)
{
	GdauiProviderAuthEditor *auth = (GdauiProviderAuthEditor *) object;

	g_return_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth));

	/* free memory */
	if (auth->priv->labels_size_group) {
		g_object_unref (auth->priv->labels_size_group);
		auth->priv->labels_size_group = NULL;
	}
	if (auth->priv->entries_size_group) {
		g_object_unref (auth->priv->entries_size_group);
		auth->priv->entries_size_group = NULL;
	}
	if (auth->priv->provider)
		g_free (auth->priv->provider);

	g_free (auth->priv);
	auth->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
gdaui_provider_auth_editor_set_property (GObject *object,
                                            guint param_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
	GdauiProviderAuthEditor *auth;
	auth = GDAUI_PROVIDER_AUTH_EDITOR (object);

	switch(param_id) {
	case PROP_PROVIDER:
		_gdaui_provider_auth_editor_set_provider (auth, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_provider_auth_editor_get_property (GObject *object,
                                            guint param_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
	GdauiProviderAuthEditor *auth;
	auth = GDAUI_PROVIDER_AUTH_EDITOR (object);

	switch (param_id) {
	case PROP_PROVIDER:
		g_value_set_string (value, auth->priv->provider);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

GType
_gdaui_provider_auth_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiProviderAuthEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_provider_auth_editor_class_init,
			NULL,
			NULL,
			sizeof (GdauiProviderAuthEditor),
			0,
			(GInstanceInitFunc) gdaui_provider_auth_editor_init,
			0
		};
		type = g_type_from_name ("GdauiProviderAuthEditor");
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_BOX,
						       "GdauiProviderAuthEditor",
					       	       &info, 0);
	}
	return type;
}

/*
 * _gdaui_provider_auth_editor_new
 * @provider: the provider to be used 
 *
 * Creates a new #GdauiProviderAuthEditor widget
 *
 * Returns: a new widget
 */
GtkWidget *
_gdaui_provider_auth_editor_new (const gchar *provider)
{
	GdauiProviderAuthEditor *auth;

	auth = g_object_new (GDAUI_TYPE_PROVIDER_AUTH_EDITOR,
	                     "provider", provider, NULL);

	return GTK_WIDGET (auth);
}

/*
 * _gdaui_provider_auth_editor_set_provider
 * @auth: a #GdauiProviderAuthEditor widget
 * @provider: the provider to be used 
 *
 * Updates the displayed fields in @auth to represent the required
 * and possible arguments that a connection to a database through 
 * @provider would require
 */
void
_gdaui_provider_auth_editor_set_provider (GdauiProviderAuthEditor *auth, const gchar *provider)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth));
	g_return_if_fail (auth->priv);

	auth->priv->pinfo = NULL;
	if (auth->priv->provider)
		g_free (auth->priv->provider);
	auth->priv->provider = NULL;
	auth->priv->auth_needed = FALSE;

	if (auth->priv->auth_widget) {
		gtk_widget_destroy (auth->priv->auth_widget);
		auth->priv->auth_widget = NULL;
	}

	if (provider) {
		auth->priv->pinfo = gda_config_get_provider_info (provider);
		if (auth->priv->pinfo) {
			auth->priv->provider = g_strdup (auth->priv->pinfo->id);
			if (auth->priv->pinfo->auth_params && gda_set_get_holders (auth->priv->pinfo->auth_params))
				auth->priv->auth_needed = TRUE;
		}
	}

	if (auth->priv->auth_needed) {
		g_assert (auth->priv->pinfo);
		GdaSet *set;
		
		set = gda_set_copy (auth->priv->pinfo->auth_params);
		auth->priv->auth_widget = gdaui_basic_form_new (set);
		g_signal_connect (G_OBJECT (auth->priv->auth_widget), "holder-changed",
				  G_CALLBACK (auth_form_changed), auth);
		g_object_unref (set);
	}
		
	if (auth->priv->auth_widget) {
		gtk_container_add (GTK_CONTAINER (auth), auth->priv->auth_widget);
		gtk_widget_show (auth->priv->auth_widget);

		if (auth->priv->labels_size_group)
			gdaui_basic_form_add_to_size_group (GDAUI_BASIC_FORM (auth->priv->auth_widget),
							    auth->priv->labels_size_group,
							    GDAUI_BASIC_FORM_LABELS);
		if (auth->priv->entries_size_group)
			gdaui_basic_form_add_to_size_group (GDAUI_BASIC_FORM (auth->priv->auth_widget),
							    auth->priv->entries_size_group,
							    GDAUI_BASIC_FORM_ENTRIES);
	}

	g_signal_emit_by_name (auth, "changed");
}

/*
 * _gdaui_provider_auth_editor_is_valid
 * @auth: a #GdauiProviderAuthEditor widget
 * 
 * Tells if the current information displayed in @auth reauthts the
 * provider's authifications (about non NULL values for example)
 *
 * Returns:
 */
gboolean
_gdaui_provider_auth_editor_is_valid (GdauiProviderAuthEditor *auth)
{
	g_return_val_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth), FALSE);
	g_return_val_if_fail (auth->priv, FALSE);

	if (!auth->priv->pinfo)
		return FALSE;

	if (auth->priv->auth_needed) {
		g_assert (auth->priv->auth_widget);
		return gdaui_basic_form_is_valid (GDAUI_BASIC_FORM (auth->priv->auth_widget));
	}
	else
		return TRUE;
}

static gchar *
params_to_string (GdauiProviderAuthEditor *auth)
{
	GString *string = NULL;
	gchar *str;
	GdaSet *dset;
	GSList *list;

	g_assert (auth->priv->auth_widget);
	if (! GDAUI_IS_BASIC_FORM (auth->priv->auth_widget))
		return NULL;

	dset = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (auth->priv->auth_widget));
	for (list = gda_set_get_holders (dset); list; list = list->next) {
		GdaHolder *param = GDA_HOLDER (list->data);
		if (gda_holder_is_valid (param)) {
			const GValue *value;
			value = gda_holder_get_value (param);
			str = NULL;
			if (value && !gda_value_is_null ((GValue *) value)) {
				GdaDataHandler *dh;
				GType dtype;

				dtype = gda_holder_get_g_type (param);
				dh = gda_data_handler_get_default (dtype);
				str = gda_data_handler_get_str_from_value (dh, value);
			}
			if (str && *str) {
				gchar *name;
				gchar *ename, *evalue;
				if (!string)
					string = g_string_new ("");
				else
					g_string_append_c (string, ';');
				g_object_get (G_OBJECT (list->data), "id", &name, NULL);
				ename = gda_rfc1738_encode (name);
				evalue = gda_rfc1738_encode (str);
				g_string_append_printf (string, "%s=%s", ename, evalue);
				g_free (ename);
				g_free (evalue);
			}
			g_free (str);
		}		
	}

	str = string ? string->str : NULL;
	if (string)
		g_string_free (string, FALSE);
	return str;
}

/*
 * _gdaui_provider_auth_editor_get_auths
 * @auth: a #GdauiProviderAuthEditor widget
 *
 * Get the currently displayed provider's authific
 * connection string
 *
 * Returns: a new string, or %NULL if no provider have been set, or no authentication is necessary
 */
gchar *
_gdaui_provider_auth_editor_get_auth (GdauiProviderAuthEditor *auth)
{
	g_return_val_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth), NULL);
	g_return_val_if_fail (auth->priv, NULL);

	if (!auth->priv->pinfo)
		return NULL;

	if (auth->priv->auth_needed) {
		g_assert (auth->priv->auth_widget);
		return params_to_string (auth);
	}
	else
		return NULL;
}

/*
 * _gdaui_provider_auth_editor_set_auths
 * @auth: a #GdauiProviderAuthEditor widget
 * @auth_string: 
 *
 * Sets the connection string to be displayed in the widget
 */
void
_gdaui_provider_auth_editor_set_auth (GdauiProviderAuthEditor *auth, const gchar *auth_string)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth));
	g_return_if_fail (auth->priv);

	if (!auth->priv->pinfo)
		return;

	if (!auth->priv->auth_needed) {
		if (auth_string && *auth_string)
			g_warning (_("Can't set authentification string: no authentication is needed"));
		return;
	}

	gdaui_basic_form_reset (GDAUI_BASIC_FORM (auth->priv->auth_widget));
	if (auth_string) {
		/* split array in a list of named parameters, and for each parameter value, set the correcponding
		   parameter in @dset */
		GdaSet *dset;
		GSList *params_set = NULL;
		g_assert (auth->priv->auth_widget);

		dset = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (auth->priv->auth_widget));
		gchar **array = NULL;
		array = g_strsplit (auth_string, ";", 0);
		if (array) {
			gint index = 0;
			gchar *tok;
			gchar *value;
			gchar *name;
			
			for (index = 0; array[index]; index++) {
				name = strtok_r (array [index], "=", &tok);
				if (name)
					value = strtok_r (NULL, "=", &tok);
				else
					value = NULL;
				if (name && value) {
					GdaHolder *param;
					gda_rfc1738_decode (name);
					gda_rfc1738_decode (value);
					
					param = gda_set_get_holder (dset, name);
					if (param)
						if (gda_holder_set_value_str (param, NULL, value, NULL))
							params_set = g_slist_prepend (params_set, param);
				}
			}
			
			g_strfreev (array);
		}
	}

	g_signal_emit_by_name (auth, "changed");
}

/*
 * _gdaui_provider_auth_editor_add_to_size_group
 * @auth: a #GdauiProviderAuthEditor widget
 */
void
_gdaui_provider_auth_editor_add_to_size_group (GdauiProviderAuthEditor *auth, GtkSizeGroup *size_group,
					       GdauiBasicFormPart part)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_AUTH_EDITOR (auth));
	g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));

	g_return_if_fail (! ((auth->priv->labels_size_group && (part == GDAUI_BASIC_FORM_LABELS)) ||
			     (auth->priv->entries_size_group && (part == GDAUI_BASIC_FORM_ENTRIES))));
	if (part == GDAUI_BASIC_FORM_LABELS)
		auth->priv->labels_size_group = g_object_ref (size_group);
	else
		auth->priv->entries_size_group = g_object_ref (size_group);

	if (auth->priv->auth_widget)
		gdaui_basic_form_add_to_size_group (GDAUI_BASIC_FORM (auth->priv->auth_widget), size_group, part);
}
