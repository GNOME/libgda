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

#include "gdaui-entry-boolean.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_boolean_class_init (GdauiEntryBooleanClass * class);
static void gdaui_entry_boolean_init (GdauiEntryBoolean * srv);
static void gdaui_entry_boolean_dispose (GObject   * object);
static void gdaui_entry_boolean_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryBooleanPrivate
{
	GtkWidget *hbox;
	GtkWidget *check;
};


GType
gdaui_entry_boolean_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryBooleanClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_boolean_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryBoolean),
			0,
			(GInstanceInitFunc) gdaui_entry_boolean_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryBoolean", &info, 0);
	}
	return type;
}

static void
gdaui_entry_boolean_class_init (GdauiEntryBooleanClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_boolean_dispose;
	object_class->finalize = gdaui_entry_boolean_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;
}

static void
gdaui_entry_boolean_init (GdauiEntryBoolean * gdaui_entry_boolean)
{
	gdaui_entry_boolean->priv = g_new0 (GdauiEntryBooleanPrivate, 1);
	gdaui_entry_boolean->priv->hbox = NULL;
	gdaui_entry_boolean->priv->check = NULL;
}

/**
 * gdaui_entry_boolean_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new data entry widget
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_boolean_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryBoolean *mgbool;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_BOOLEAN, "handler", dh, NULL);
	mgbool = GDAUI_ENTRY_BOOLEAN (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgbool), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_boolean_dispose (GObject   * object)
{
	GdauiEntryBoolean *gdaui_entry_boolean;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (object));

	gdaui_entry_boolean = GDAUI_ENTRY_BOOLEAN (object);
	if (gdaui_entry_boolean->priv) {

	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_boolean_finalize (GObject   * object)
{
	GdauiEntryBoolean *gdaui_entry_boolean;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (object));

	gdaui_entry_boolean = GDAUI_ENTRY_BOOLEAN (object);
	if (gdaui_entry_boolean->priv) {

		g_free (gdaui_entry_boolean->priv);
		gdaui_entry_boolean->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *hbox, *cb;
	GdauiEntryBoolean *mgbool;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap), NULL);
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	g_return_val_if_fail (mgbool->priv, NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	mgbool->priv->hbox = hbox;

	cb = gtk_check_button_new ();
	mgbool->priv->check = cb;
	gtk_box_pack_start (GTK_BOX (hbox), cb, FALSE, FALSE, 0);
	gtk_widget_show (cb);

	return hbox;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	g_return_if_fail (mgbool->priv);

	if (value) {
		if (gda_value_is_null ((GValue *) value)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgbool->priv->check), FALSE);
			gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (mgbool->priv->check), TRUE);
		}
		else {
			gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (mgbool->priv->check), FALSE);
			if (g_value_get_boolean ((GValue *) value)) 
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgbool->priv->check), TRUE);
			else 
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgbool->priv->check), FALSE);
		}
	}
	else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgbool->priv->check), FALSE);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (mgbool->priv->check), TRUE);
	}
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryBoolean *mgbool;
	GdaDataHandler *dh;
	const gchar *str;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap), NULL);
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	g_return_val_if_fail (mgbool->priv, NULL);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mgbool->priv->check)))
		str = "TRUE";
	else
		str = "FALSE";
	value = gda_data_handler_get_value_from_sql (dh, str, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));

	return value;
}

static void check_toggled_cb (GtkToggleButton *toggle, GdauiEntryBoolean *mgbool);
static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	g_return_if_fail (mgbool->priv);

	g_signal_connect (G_OBJECT (mgbool->priv->check), "toggled",
			  modify_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgbool->priv->check), "toggled",
			  activate_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgbool->priv->check), "toggled",
                          G_CALLBACK (check_toggled_cb), mgwrap);
}

static void
check_toggled_cb (GtkToggleButton *toggle, G_GNUC_UNUSED GdauiEntryBoolean *mgbool)
{
	gtk_toggle_button_set_inconsistent (toggle, FALSE);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	g_return_if_fail (mgbool->priv);

	gtk_widget_set_sensitive (mgbool->priv->check, editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	g_return_if_fail (mgbool->priv);

	gtk_widget_grab_focus (mgbool->priv->check);
}
