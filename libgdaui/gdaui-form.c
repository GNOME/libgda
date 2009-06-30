/* gdaui-form.c
 *
 * Copyright (C) 2002 - 2007 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-form.h"
#include "gdaui-data-widget.h"
#include "gdaui-raw-form.h"
#include "gdaui-data-widget-info.h"

static void gdaui_form_class_init (GdauiFormClass * class);
static void gdaui_form_init (GdauiForm *wid);

static void gdaui_form_set_property (GObject *object,
					guint param_id,
					const GValue *value,
					GParamSpec *pspec);
static void gdaui_form_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec);

struct _GdauiFormPriv
{
	GtkWidget *raw_form;
	GtkWidget *info;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum
{
        PROP_0,
        PROP_RAW_FORM,
	PROP_INFO,
	PROP_MODEL
};

GType
gdaui_form_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiFormClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_form_class_init,
			NULL,
			NULL,
			sizeof (GdauiForm),
			0,
			(GInstanceInitFunc) gdaui_form_init
		};		

		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiForm", &info, 0);
	}

	return type;
}

static void
gdaui_form_class_init (GdauiFormClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	/* Properties */
	
        object_class->set_property = gdaui_form_set_property;
        object_class->get_property = gdaui_form_get_property;
	g_object_class_install_property (object_class, PROP_RAW_FORM,
                                         g_param_spec_object ("raw_form", NULL, NULL, GDAUI_TYPE_RAW_FORM,
                                                               G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_INFO,
                                         g_param_spec_object ("widget_info", NULL, NULL, GDAUI_TYPE_DATA_WIDGET_INFO,
							      G_PARAM_READABLE));
	
	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", NULL, NULL,
							      GDA_TYPE_DATA_MODEL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gdaui_form_init (GdauiForm *form)
{
	form->priv = g_new0 (GdauiFormPriv, 1);
	form->priv->raw_form = NULL;
	form->priv->info = NULL;
	
	form->priv->raw_form = gdaui_raw_form_new (NULL);
	gtk_box_pack_start (GTK_BOX (form), form->priv->raw_form, TRUE, TRUE, 0);
	gtk_widget_show (form->priv->raw_form);

	form->priv->info = gdaui_data_widget_info_new (GDAUI_DATA_WIDGET (form->priv->raw_form), 
							  GDAUI_DATA_WIDGET_INFO_CURRENT_ROW |
							  GDAUI_DATA_WIDGET_INFO_ROW_MODIFY_BUTTONS |
							  GDAUI_DATA_WIDGET_INFO_ROW_MOVE_BUTTONS);
	gtk_box_pack_start (GTK_BOX (form), form->priv->info, FALSE, TRUE, 0);
	gtk_widget_show (form->priv->info);

}

/**
 * gdaui_form_new
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiForm widget suitable to display the data in @model
 *
 *  Returns: the new widget
 */
GtkWidget *
gdaui_form_new (GdaDataModel *model)
{
	GdauiForm *form;
	
	g_return_val_if_fail (!model || GDA_IS_DATA_MODEL (model), NULL);
	
	form = (GdauiForm *) g_object_new (GDAUI_TYPE_FORM,
	                                     "model", model, NULL);
	
	return (GtkWidget *) form;
}


static void
gdaui_form_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	GdauiForm *form;
	GdaDataModel *model;

	form = GDAUI_FORM (object);
	
	if (form->priv) {
		switch (param_id) {
		case PROP_MODEL:
			model = GDA_DATA_MODEL (g_value_get_object (value));
			g_object_set (G_OBJECT(form->priv->raw_form),
			              "model", model, NULL);
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gdaui_form_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	GdauiForm *form;
	GdaDataModel *model;

        form = GDAUI_FORM (object);
        if (form->priv) {
		switch (param_id) {
		case PROP_RAW_FORM:
			g_value_set_object (value, form->priv->raw_form);
			break;
		case PROP_INFO:
			g_value_set_object (value, form->priv->info);
			break;
		case PROP_MODEL:
			g_object_get (G_OBJECT (form->priv->raw_form),
			              "model", &model, NULL);
			g_value_take_object (value, G_OBJECT (model));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
        }
}
