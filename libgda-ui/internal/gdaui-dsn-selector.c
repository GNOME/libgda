/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include <libgda/gda-config.h>
#include "gdaui-dsn-selector.h"
#include <gtk/gtk.h>

struct _GdauiDsnSelectorPrivate {
	gchar dummy;
};

static void gdaui_dsn_selector_class_init (GdauiDsnSelectorClass *klass);
static void gdaui_dsn_selector_init       (GdauiDsnSelector *selector,
						      GdauiDsnSelectorClass *klass);
static void gdaui_dsn_selector_finalize   (GObject *object);

static void gdaui_dsn_selector_set_property(GObject *object,
                                                       guint param_id,
                                                       const GValue *value,
                                                       GParamSpec *pspec);
static void gdaui_dsn_selector_get_property(GObject *object,
                                                       guint param_id,
                                                       GValue *value,
                                                       GParamSpec *pspec);

enum {
	PROP_0,

	PROP_SOURCE_NAME
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/*
 * GdauiDsnSelector class implementation
 */

static void
gdaui_dsn_selector_class_init (GdauiDsnSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_dsn_selector_finalize;
	object_class->set_property = gdaui_dsn_selector_set_property;
	object_class->get_property = gdaui_dsn_selector_get_property;

	g_object_class_install_property (object_class, PROP_SOURCE_NAME,
	                                 g_param_spec_string ("source-name", NULL, NULL, NULL,
	                                                      G_PARAM_WRITABLE | G_PARAM_READABLE));
}


static void
gdaui_dsn_selector_init (GdauiDsnSelector *selector,
				    G_GNUC_UNUSED GdauiDsnSelectorClass *klass)
{
	GdaDataModel *model;
	gint cols_index[] = {0};

	g_return_if_fail (GDAUI_IS_DSN_SELECTOR (selector));

	selector->priv = g_new0 (GdauiDsnSelectorPrivate, 1);

	model = gda_config_list_dsn ();
	gdaui_combo_set_data (GDAUI_COMBO (selector), model, 1, cols_index);
	g_object_unref (model);
}

static void
gdaui_dsn_selector_finalize (GObject *object)
{
	GdauiDsnSelector *selector = (GdauiDsnSelector *) object;

	g_return_if_fail (GDAUI_IS_DSN_SELECTOR (selector));


	g_free (selector->priv);
	selector->priv = NULL;

	parent_class->finalize (object);
}

static void
gdaui_dsn_selector_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	GdauiDsnSelector *selector;
	GSList *list;
	gint cols_index[] = {0};
	selector = GDAUI_DSN_SELECTOR (object);

	switch (param_id) {
	case PROP_SOURCE_NAME:
		list = g_slist_append (NULL, (gpointer) value);
		_gdaui_combo_set_selected_ext (GDAUI_COMBO (selector), list, cols_index);
		g_slist_free (list);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_dsn_selector_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	GdauiDsnSelector *selector;
	GSList *list;
	gint cols_index[] = {0};
	selector = GDAUI_DSN_SELECTOR (object);

	switch (param_id) {
	case PROP_SOURCE_NAME:
		list = _gdaui_combo_get_selected_ext (GDAUI_COMBO (selector), 1, cols_index);
		if (list && list->data) {
			g_value_set_string (value, g_value_get_string ((GValue*) list->data));
			g_slist_free (list);
		}
		else
			g_value_set_string (value, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

GType
_gdaui_dsn_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDsnSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_dsn_selector_class_init,
			NULL,
			NULL,
			sizeof (GdauiDsnSelector),
			0,
			(GInstanceInitFunc) gdaui_dsn_selector_init,
			0
		};
		type = g_type_from_name ("GdauiDsnSelector");
		if (type == 0)
			type = g_type_register_static (GDAUI_TYPE_COMBO,
						       "GdauiDsnSelector",
						       &info, 0);
	}
	return type;
}

/**
 * _gdaui_dsn_selector_new
 *
 * Create a new #GdauiDsnSelector, which is just a #GtkComboBox
 * which displays, as its items, all the data sources currently
 * configured in the system. It is useful for connection and configuration
 * screens, where the user has to choose a data source to work with.
 *
 * Returns: the newly created widget.
 */
GtkWidget *
_gdaui_dsn_selector_new (void)
{
	return (GtkWidget*) g_object_new (GDAUI_TYPE_DSN_SELECTOR, NULL);
}

/**
 * _gdaui_dsn_selector_get_dsn
 * @name: name of data source to display.
 *
 * Get the Data Source Name (DSN) actualy selected in the #GdauiDsnSelector.
 *
 * Returns: the DSN name actualy selected as a new string.
 */
gchar *
_gdaui_dsn_selector_get_dsn (GdauiDsnSelector *selector)
{
	gchar *dsn;

	g_object_get (G_OBJECT (selector), "source-name", &dsn, NULL);

	return dsn;
}

/**
 * _gdaui_dsn_selector_set_dsn
 * @name: name of data source to display.
 *
 * Set the selected Data Source Name (DSN) in the #GdauiDsnSelector.
 *
 */
void
_gdaui_dsn_selector_set_dsn (GdauiDsnSelector *selector, const gchar *dsn)
{
	g_object_set (G_OBJECT (selector), "source-name", dsn, NULL);
}
