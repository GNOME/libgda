/* GNOME DB library
 * Copyright (C) 2005 - 2009 The GNOME Foundation
 *
 * AUTHORS:
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
#include "gdaui-provider-spec-editor.h"
#include <libgda-ui/gdaui-basic-form.h>
#include <glib/gi18n-lib.h>

typedef enum {
	NO_PROVIDER,
	PROVIDER_FORM
} WidgetType;

struct _GdauiProviderSpecEditorPrivate {
	gchar       *provider;

	WidgetType   type;
	GtkWidget   *form; /* what it really is is determined by @type */
	gchar       *cnc_string; /* as it was last updated */
};

static void gdaui_provider_spec_editor_class_init (GdauiProviderSpecEditorClass *klass);
static void gdaui_provider_spec_editor_init       (GdauiProviderSpecEditor *spec,
					  GdauiProviderSpecEditorClass *klass);
static void gdaui_provider_spec_editor_finalize   (GObject *object);
static void gdaui_provider_spec_editor_dispose    (GObject *object);

static void gdaui_provider_spec_editor_set_property (GObject *object,
                                            guint param_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void gdaui_provider_spec_editor_get_property (GObject *object,
                                            guint param_id,
                                            GValue *value,
                                            GParamSpec *pspec);

enum {
	PROP_0,
	PROP_PROVIDER
};

enum {
	CHANGED,
	LAST_SIGNAL
};


static gint gdaui_provider_spec_editor_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * GdauiProviderSpecEditor class implementation
 */

static void
gdaui_provider_spec_editor_class_init (GdauiProviderSpecEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gdaui_provider_spec_editor_dispose;
	object_class->finalize = gdaui_provider_spec_editor_finalize;
	object_class->set_property = gdaui_provider_spec_editor_set_property;
	object_class->get_property = gdaui_provider_spec_editor_get_property;
	klass->changed = NULL;

	g_object_class_install_property (object_class, PROP_PROVIDER,
	                                 g_param_spec_string ("provider", NULL, NULL, NULL,
					                      G_PARAM_READWRITE));

	/* add class signals */
	gdaui_provider_spec_editor_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiProviderSpecEditorClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
update_form_contents (GdauiProviderSpecEditor *spec)
{
	/*g_print ("DSN: %s\n", spec->priv->cnc_string);*/
	switch (spec->priv->type) {
	case PROVIDER_FORM: {
		/* update data set in form */
		GdaSet *dset;
		GSList *params_set = NULL;
		GSList *list;
		g_assert (spec->priv->form);

		dset = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (spec->priv->form));

		/* split array in a list of named parameters, and for each parameter value, set the correcponding
		   parameter in @dset */
		if (spec->priv->cnc_string) {
			gchar **array = NULL;
			array = g_strsplit (spec->priv->cnc_string, ";", 0);
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

		list = dset->holders;
		while (0 && list) {
			if (!params_set || !g_slist_find (params_set, list->data)) {
				/* empty parameter */
				gda_holder_set_value (GDA_HOLDER (list->data), NULL, NULL);
			}
			list = g_slist_next (list);
		}
		g_slist_free (params_set);
		break;
	}
	default:
		/* no change here */
		break;
	}
}

static void
dsn_form_changed (GdauiBasicForm *form, GdaHolder *param, gboolean is_user_modif, GdauiProviderSpecEditor *spec)
{
	if (! is_user_modif)
		return;

	g_signal_emit (spec, gdaui_provider_spec_editor_signals[CHANGED], 0, NULL);
}

static void
adapt_form_widget (GdauiProviderSpecEditor *spec)
{
	/* destroy any previous widget */
	if (spec->priv->form) {
		gtk_container_foreach (GTK_CONTAINER (spec), (GtkCallback) gtk_widget_destroy, NULL);
		spec->priv->form = NULL;
	}
	spec->priv->type = NO_PROVIDER;
	
	if (!spec->priv->provider) 
		return;
	
	/* fetch DSN parameters */
	GdaProviderInfo *pinfo;
	pinfo = gda_config_get_provider_info (spec->priv->provider);
	if (!pinfo) {
		g_warning (_("Unknown provider '%s'"), spec->priv->provider);
		return;
	}
	if (!pinfo->dsn_params) {
		g_warning (_("Provider '%s' does not report the required parameters for DSN"), spec->priv->provider);
		return;
	}

	/* create new widget */	
	GdaSet *dset;
	GtkWidget *wid;	
	dset = gda_set_copy (pinfo->dsn_params);
	if (dset) {
		spec->priv->type = PROVIDER_FORM;
		
		wid = gdaui_basic_form_new (dset);
		gdaui_basic_form_show_entry_actions (GDAUI_BASIC_FORM (wid), FALSE);
		g_object_unref (dset);
		
		spec->priv->form = wid;
		update_form_contents (spec);
		g_signal_connect (G_OBJECT (wid), "param_changed",
				  G_CALLBACK (dsn_form_changed), spec);
	}
	
	gtk_widget_show (wid);
	gtk_container_add (GTK_CONTAINER (spec), wid);
}


static void
gdaui_provider_spec_editor_init (GdauiProviderSpecEditor *spec, GdauiProviderSpecEditorClass *klass)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec));

	spec->priv = g_new0 (GdauiProviderSpecEditorPrivate, 1);
	spec->priv->type = NO_PROVIDER;
}

static void
gdaui_provider_spec_editor_dispose (GObject *object)
{
	GdauiProviderSpecEditor *spec = (GdauiProviderSpecEditor *) object;

	g_return_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec));

	/* free memory */

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gdaui_provider_spec_editor_finalize (GObject *object)
{
	GdauiProviderSpecEditor *spec = (GdauiProviderSpecEditor *) object;

	g_return_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec));

	/* free memory */
	if (spec->priv->cnc_string)
		g_free (spec->priv->cnc_string);
	if (spec->priv->provider)
		g_free (spec->priv->provider);

	g_free (spec->priv);
	spec->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
gdaui_provider_spec_editor_set_property (GObject *object,
                                            guint param_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
	GdauiProviderSpecEditor *spec;
	spec = GDAUI_PROVIDER_SPEC_EDITOR (object);

	switch(param_id) {
	case PROP_PROVIDER:
		_gdaui_provider_spec_editor_set_provider (spec,
		                                g_value_get_string (value));
		break;
	}
}

static void
gdaui_provider_spec_editor_get_property (GObject *object,
                                            guint param_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
	GdauiProviderSpecEditor *spec;
	spec = GDAUI_PROVIDER_SPEC_EDITOR (object);

	switch (param_id) {
	case PROP_PROVIDER:
		g_value_set_string (value, spec->priv->provider);
		break;
	}
}

GType
_gdaui_provider_spec_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiProviderSpecEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_provider_spec_editor_class_init,
			NULL,
			NULL,
			sizeof (GdauiProviderSpecEditor),
			0,
			(GInstanceInitFunc) gdaui_provider_spec_editor_init
		};
		type = g_type_from_name ("GdauiProviderSpecEditor");
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_VBOX,
						       "GdauiProviderSpecEditor",
						       &info, 0);
	}
	return type;
}

/**
 * _gdaui_provider_spec_editor_new
 * @provider: the provider to be used 
 *
 * Creates a new #GdauiProviderSpecEditor widget
 *
 * Returns:
 */
GtkWidget *
_gdaui_provider_spec_editor_new (const gchar *provider)
{
	GdauiProviderSpecEditor *spec;

	spec = g_object_new (GDAUI_TYPE_PROVIDER_SPEC_EDITOR,
	                     "provider", provider, NULL);

	return GTK_WIDGET (spec);
}

static gchar *
params_to_string (GdauiProviderSpecEditor *spec)
{
	GString *string = NULL;
	gchar *str;
	GdaSet *dset;
	GSList *list;

	g_assert (spec->priv->form);
	if (! GDAUI_IS_BASIC_FORM (spec->priv->form))
		return NULL;

	dset = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (spec->priv->form));
	for (list = dset->holders; list; list = list->next) {
		GdaHolder *param = GDA_HOLDER (list->data);
		if (gda_holder_is_valid (param)) {
			const GValue *value;
			value = gda_holder_get_value (param);
			str = NULL;
			if (value && !gda_value_is_null ((GValue *) value)) {
				GdaDataHandler *dh;
				GType dtype;

				dtype = gda_holder_get_g_type (param);
				dh = gda_get_default_handler (dtype);
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

/**
 * _gdaui_provider_spec_editor_set_provider
 * @spec: a #GdauiProviderSpecEditor widget
 * @provider: the provider to be used 
 *
 * Updates the displayed fields in @spec to represent the required
 * and possible arguments that a connection to a database through 
 * @provider would require
 */
void
_gdaui_provider_spec_editor_set_provider (GdauiProviderSpecEditor *spec, const gchar *provider)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec));
	g_return_if_fail (spec->priv);

	if (spec->priv->provider)
		g_free (spec->priv->provider);
	spec->priv->provider = NULL;

	if (provider)
		spec->priv->provider = g_strdup (provider);
	adapt_form_widget (spec);
}

/**
 * _gdaui_provider_spec_editor_is_valid
 * @spec: a #GdauiProviderSpecEditor widget
 * 
 * Tells if the current information displayed in @spec respects the
 * provider's specifications (about non NULL values for example)
 *
 * Returns:
 */
gboolean
_gdaui_provider_spec_editor_is_valid (GdauiProviderSpecEditor *spec)
{
	g_return_val_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec), FALSE);
	g_return_val_if_fail (spec->priv, FALSE);

	switch (spec->priv->type) {
	case PROVIDER_FORM: 
		g_assert (spec->priv->form);
		return gdaui_basic_form_is_valid (GDAUI_BASIC_FORM (spec->priv->form));
	default:
		return FALSE;
	}
}

/**
 * _gdaui_provider_spec_editor_get_specs
 * @spec: a #GdauiProviderSpecEditor widget
 *
 * Get the currently displayed provider's specific
 * connection string
 *
 * Returns: a new string, or %NULL if no provider have been specified
 */
gchar *
_gdaui_provider_spec_editor_get_specs (GdauiProviderSpecEditor *spec)
{
	g_return_val_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec), NULL);
	g_return_val_if_fail (spec->priv, NULL);

	switch (spec->priv->type) {
	case PROVIDER_FORM:
		return params_to_string (spec);
	default:
		return NULL;
	}
}

/**
 * _gdaui_provider_spec_editor_set_specs
 * @spec: a #GdauiProviderSpecEditor widget
 * @specs_string: 
 *
 * Sets the connection string to be displayed in the widget
 */
void
_gdaui_provider_spec_editor_set_specs (GdauiProviderSpecEditor *spec, const gchar *specs_string)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_SPEC_EDITOR (spec));
	g_return_if_fail (spec->priv);

	/* save DSN string */
	if (spec->priv->cnc_string)
		g_free (spec->priv->cnc_string);
	spec->priv->cnc_string = NULL;

	if (specs_string)
		spec->priv->cnc_string = g_strdup (specs_string);

	update_form_contents (spec);
}
