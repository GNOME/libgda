/*
 * Copyright (C) 2009 Vivien Malerba
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
#include <glib/gi18n-lib.h>
#include "connection-binding-properties.h"
#include "browser-connection.h"
#include "support.h"
#include <libgda-ui/libgda-ui.h>

/* 
 * Main static functions 
 */
static void connection_binding_properties_class_init (ConnectionBindingPropertiesClass *klass);
static void connection_binding_properties_init (ConnectionBindingProperties *stmt);
static void connection_binding_properties_dispose (GObject *object);

static void update_buttons_sensitiveness (ConnectionBindingProperties *cprop);
static void create_layout (ConnectionBindingProperties *cprop);
static void update_display (ConnectionBindingProperties *cprop);

struct _ConnectionBindingPropertiesPrivate
{
	BrowserVirtualConnectionSpecs *specs;
	GtkTable    *layout_table;
	GtkWidget   *menu;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

GType
connection_binding_properties_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (ConnectionBindingPropertiesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) connection_binding_properties_class_init,
			NULL,
			NULL,
			sizeof (ConnectionBindingProperties),
			0,
			(GInstanceInitFunc) connection_binding_properties_init
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_DIALOG, "ConnectionBindingProperties", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
connection_binding_properties_class_init (ConnectionBindingPropertiesClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = connection_binding_properties_dispose;
}

static void
connection_binding_properties_init (ConnectionBindingProperties *cprop)
{
	cprop->priv = g_new0 (ConnectionBindingPropertiesPrivate, 1);
}

static void
connection_binding_properties_dispose (GObject *object)
{
	ConnectionBindingProperties *cprop;

	g_return_if_fail (object != NULL);
	g_return_if_fail (CONNECTION_IS_BINDING_PROPERTIES (object));

	cprop = CONNECTION_BINDING_PROPERTIES (object);

	if (cprop->priv) {
		if (cprop->priv->specs)
			browser_virtual_connection_specs_free (cprop->priv->specs);
		if (cprop->priv->menu)
			gtk_widget_destroy (cprop->priv->menu);
		g_free (cprop->priv);
		cprop->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

/**
 * connection_binding_properties_new_create
 * @bcnc: a #BrowserConnection
 *
 * Creates a new #ConnectionBindingProperties window. The window will allow a new
 * virtual connection to be opened using tables from @bcnc.
 *
 * Returns: the new object
 */
GtkWidget *
connection_binding_properties_new_create (BrowserConnection *bcnc)
{
	ConnectionBindingProperties *cprop;
	BrowserVirtualConnectionSpecs *specs;
	BrowserVirtualConnectionPart *part;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	specs = g_new0 (BrowserVirtualConnectionSpecs, 1);
	part = g_new0 (BrowserVirtualConnectionPart, 1);
	part->part_type = BROWSER_VIRTUAL_CONNECTION_PART_CNC;
	part->u.cnc.table_schema = g_strdup (browser_connection_get_name (bcnc));
	part->u.cnc.source_cnc = g_object_ref (G_OBJECT (bcnc));
	specs->parts = g_slist_append (NULL, part);

	cprop = CONNECTION_BINDING_PROPERTIES (g_object_new (CONNECTION_TYPE_BINDING_PROPERTIES, NULL));
	cprop->priv->specs = specs;
	gtk_window_set_title (GTK_WINDOW (cprop), _("New virtual connection"));

	create_layout (cprop);
	update_display (cprop);

	gtk_widget_show (gtk_dialog_add_button (GTK_DIALOG (cprop), GTK_STOCK_NEW, GTK_RESPONSE_OK));
	gtk_widget_show (gtk_dialog_add_button (GTK_DIALOG (cprop), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL));

	return (GtkWidget*) cprop;
}

static void
create_layout (ConnectionBindingProperties *cprop)
{
	GtkWidget *sw, *vp, *label, *hbox;
	gchar *str;

	str = g_strdup_printf ("<b>%s:</b>\n<small>%s</small>",
			       _("Virtual connection's properties"),
			       _("Define the sources of data for which tables will\n"
				 "appear in the virtual connection"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cprop)->vbox), label, FALSE, FALSE, 10);

	hbox = gtk_hbox_new (FALSE, 0); /* HIG */
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cprop)->vbox), hbox, TRUE, TRUE, 0);
	label = gtk_label_new ("      ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (hbox), sw, TRUE, TRUE, 0);

	vp = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), vp);

	cprop->priv->layout_table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
	gtk_container_add (GTK_CONTAINER (vp), (GtkWidget*) cprop->priv->layout_table);

	gtk_widget_show_all (GTK_DIALOG (cprop)->vbox);

	gtk_window_set_default_size (GTK_WINDOW (cprop), 340, 300);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (cprop), GTK_RESPONSE_OK, FALSE);
}

static void add_part_clicked_cb (GtkWidget *button, ConnectionBindingProperties *cprop);
static void del_part_clicked_cb (GtkWidget *button, BrowserVirtualConnectionPart *part);

static GtkWidget *create_part_for_model (ConnectionBindingProperties *cprop, BrowserVirtualConnectionModel *pm);
static GtkWidget *create_part_for_cnc (ConnectionBindingProperties *cprop, BrowserVirtualConnectionCnc *cnc);

static void
update_display (ConnectionBindingProperties *cprop)
{
	/* clear any previous setting */
	gtk_container_foreach (GTK_CONTAINER (cprop->priv->layout_table), (GtkCallback) gtk_widget_destroy, NULL);

	/* new contents */
	gint top = 0;
	GtkWidget *button, *label;
	if (cprop->priv->specs) {
		GSList *list;
		for (list = cprop->priv->specs->parts; list; list = list->next, top++) {
			BrowserVirtualConnectionPart *part;
			GtkWidget *display = NULL;

			part = (BrowserVirtualConnectionPart*) list->data;
			switch (part->part_type) {
			case BROWSER_VIRTUAL_CONNECTION_PART_MODEL:
				display = create_part_for_model (cprop, &(part->u.model));
				break;
			case BROWSER_VIRTUAL_CONNECTION_PART_CNC:
				display = create_part_for_cnc (cprop, &(part->u.cnc));
				break;
			default:
				g_assert_not_reached ();
			}

			gtk_table_attach (cprop->priv->layout_table, display, 0, 1, top, top + 1,
					  GTK_EXPAND | GTK_FILL, 0, 0, 10);

			button = gtk_button_new ();
			label = browser_make_tab_label_with_stock (NULL, GTK_STOCK_REMOVE, FALSE, NULL);
			gtk_container_add (GTK_CONTAINER (button), label);
			gtk_table_attach (cprop->priv->layout_table, button, 1, 2, top, top + 1, 0, GTK_FILL, 0, 10);

			g_signal_connect (button, "clicked",
					  G_CALLBACK (del_part_clicked_cb), part);
			g_object_set_data (G_OBJECT (button), "cprop", cprop);
		}
	}

	/* bottom button to add a part */
	button = gtk_button_new ();
	label = browser_make_tab_label_with_stock (_("Add binding"), GTK_STOCK_ADD, FALSE, NULL);
	gtk_container_add (GTK_CONTAINER (button), label);
	GtkWidget *arrow;
	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (label), arrow, FALSE, FALSE, 0);
	g_object_set (G_OBJECT (button), "relief", GTK_RELIEF_NONE, NULL);
	gtk_table_attach (cprop->priv->layout_table, button, 0, 2, top, top + 1, GTK_EXPAND | GTK_FILL, 0, 0, 10);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_part_clicked_cb), cprop);
	
	gtk_widget_show_all ((GtkWidget*) cprop->priv->layout_table);

	update_buttons_sensitiveness (cprop);
}

static void
add_part_mitem_cb (GtkMenuItem *mitem, ConnectionBindingProperties *cprop)
{
	BrowserVirtualConnectionType part_type;
	BrowserVirtualConnectionPart *part;

	part_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (mitem), "part-type"));
	part = g_new0 (BrowserVirtualConnectionPart, 1);
	part->part_type = part_type;
	switch (part_type) {
	case BROWSER_VIRTUAL_CONNECTION_PART_MODEL: {
		BrowserVirtualConnectionModel *pm;
		pm = &(part->u.model);
		pm->table_name = g_strdup ("tab");
		break;
	}
	case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
		BrowserVirtualConnectionCnc *scnc;
		scnc = &(part->u.cnc);
		scnc->table_schema = g_strdup ("sub");
		break;
	}
	default:
		g_assert_not_reached ();
	}

	cprop->priv->specs->parts = g_slist_append (cprop->priv->specs->parts, part);
	update_display (cprop);
}

static void
add_part_clicked_cb (GtkWidget *button, ConnectionBindingProperties *cprop)
{
	if (! cprop->priv->menu) {
		GtkWidget *menu, *entry;
		menu = gtk_menu_new ();
		entry = gtk_menu_item_new_with_label (_("Bind a connection"));
		g_object_set_data (G_OBJECT (entry), "part-type",
				   GINT_TO_POINTER (BROWSER_VIRTUAL_CONNECTION_PART_CNC));
		g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (add_part_mitem_cb), cprop);
		gtk_widget_show (entry);
		gtk_menu_append (GTK_MENU (menu), entry);
		entry = gtk_menu_item_new_with_label (_("Bind a data set"));
		g_object_set_data (G_OBJECT (entry), "part-type",
				   GINT_TO_POINTER (BROWSER_VIRTUAL_CONNECTION_PART_MODEL));
		g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (add_part_mitem_cb), cprop);
		gtk_menu_append (GTK_MENU (menu), entry);
		gtk_widget_show (entry);

		cprop->priv->menu = menu;
	}

	gtk_menu_popup (GTK_MENU (cprop->priv->menu), NULL, NULL, NULL, NULL,
                        0, gtk_get_current_event_time ());
}

static void
del_part_clicked_cb (GtkWidget *button, BrowserVirtualConnectionPart *part)
{
	ConnectionBindingProperties *cprop;
	cprop = g_object_get_data (G_OBJECT (button), "cprop");

	cprop->priv->specs->parts = g_slist_remove (cprop->priv->specs->parts, part);
	browser_virtual_connection_part_free (part);
	update_display (cprop);
}

static GtkWidget *
create_part_for_model (ConnectionBindingProperties *cprop, BrowserVirtualConnectionModel *pm)
{
	GtkWidget *vbox, *label;
	gchar *str;

	vbox = gtk_vbox_new (FALSE, 0);
	label = gtk_label_new ("");
	str = g_markup_printf_escaped ("<b>%s</b>", _("Table from a data set:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);

	GdaSet *set;
	GdaHolder *holder;
	GtkWidget *form;
	set = gda_set_new_inline (2,
				  "SCHEMA", G_TYPE_STRING, pm->table_schema,
				  "NAME", G_TYPE_STRING, pm->table_name);

	holder = gda_holder_new (G_TYPE_POINTER);
	g_object_set (holder, "id", "DATASET", "name", "Data set", NULL);
	g_assert (gda_set_add_holder (set, holder));
	g_object_unref (holder);

	g_object_set (gda_set_get_holder (set, "SCHEMA"), "name", "Table's schema", NULL);
	g_object_set (gda_set_get_holder (set, "NAME"), "name", "Table's name", NULL);
							  
	form = gdaui_basic_form_new (set);
	g_object_unref (set);
	gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

	return vbox;
}

static GError *
part_for_cnc_validate_holder_change_cb (GdaSet *set, GdaHolder *holder, GValue *new_value,
					BrowserVirtualConnectionCnc *cnc)
{
	const gchar *hid;

	hid = gda_holder_get_id (holder);
	g_assert (hid);

	if (!strcmp (hid, "SCHEMA")) {
		const gchar *str, *ptr;
		str = g_value_get_string (new_value);
		for (ptr = str; *ptr; ptr++) {
			if (((ptr == str) && ! g_ascii_isalpha (*ptr)) ||
			    ((ptr != str) && (*ptr != '_') && !g_ascii_isalnum (*ptr))) {
				GError *error = NULL;
				g_set_error (&error, 0, 0,
					     _("Invalid schema name"));
				return error;
			}
		}
	}

	/* no error */
	return NULL;
}

static void
part_for_cnc_holder_changed_cb (GdaSet *set, GdaHolder *holder, BrowserVirtualConnectionCnc *cnc)
{
	const gchar *hid;
	const GValue *value;

	hid = gda_holder_get_id (holder);
	g_assert (hid);
	value = gda_holder_get_value (holder);

	if (!strcmp (hid, "SCHEMA")) {
		g_free (cnc->table_schema);
		cnc->table_schema = g_value_dup_string (value);
	}
	else if (!strcmp (hid, "CNC")) {
		if (cnc->source_cnc)
			g_object_unref (cnc->source_cnc);
		cnc->source_cnc = g_value_get_object (value);
		if (cnc->source_cnc)
			g_object_ref (cnc->source_cnc);
	}
	else
		g_assert_not_reached ();

	ConnectionBindingProperties *cprop;
	cprop = g_object_get_data (G_OBJECT (set), "cprop");
	update_buttons_sensitiveness (cprop);
}

static GtkWidget *
create_part_for_cnc (ConnectionBindingProperties *cprop, BrowserVirtualConnectionCnc *cnc)
{
	GtkWidget *vbox, *label;
	gchar *str;

	vbox = gtk_vbox_new (FALSE, 0);
	label = gtk_label_new ("");
	str = g_markup_printf_escaped ("<b>%s</b>", _("All tables of a connection:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	g_free (str);
	gtk_widget_set_tooltip_text (label, _("Each table in the selected connection will appear\n"
					      "as a table in the virtual connection"));
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);

	GdaSet *set;
	GdaHolder *holder;
	GtkWidget *form;
	GValue *value;
	set = gda_set_new_inline (1,
				  "SCHEMA", G_TYPE_STRING, cnc->table_schema);

	holder = gda_holder_new (BROWSER_TYPE_CONNECTION);
	g_object_set (holder, "id", "CNC", "name", "Connection", "not-null", TRUE, NULL);
	g_assert (gda_set_add_holder (set, holder));

	g_value_set_object ((value = gda_value_new (BROWSER_TYPE_CONNECTION)), cnc->source_cnc);
	g_assert (gda_holder_set_value (holder, value, NULL));
	gda_value_free (value);

	g_assert (gda_holder_set_source_model (holder, browser_get_connections_list (),
					       CNC_LIST_COLUMN_BCNC, NULL));
	g_object_unref (holder);

	holder = gda_set_get_holder (set, "SCHEMA");
	g_object_set (holder, "name", "Table's schema",
		      "description", _("Name of the schema the\ntables will be in"), NULL);
							  
	form = gdaui_basic_form_new (set);
	g_signal_connect (set, "validate-holder-change",
			  G_CALLBACK (part_for_cnc_validate_holder_change_cb), cnc);
	g_object_set_data (G_OBJECT (set), "cprop", cprop);
	g_signal_connect (set, "holder-changed",
			  G_CALLBACK (part_for_cnc_holder_changed_cb), cnc);
	g_object_unref (set);
	gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

	return vbox;
}

static void
update_buttons_sensitiveness (ConnectionBindingProperties *cprop)
{
	gboolean allok = FALSE;
	GSList *list;
	GHashTable *schemas_hash = g_hash_table_new (g_str_hash, g_str_equal);

	if (! cprop->priv->specs->parts)
		goto out;

	for (list = cprop->priv->specs->parts; list; list = list->next) {
		BrowserVirtualConnectionPart *part;
		part = (BrowserVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
			BrowserVirtualConnectionCnc *cnc;
			cnc = &(part->u.cnc);
			if (! cnc->source_cnc || ! cnc->table_schema || ! *cnc->table_schema)
				goto out;

			if (g_hash_table_lookup (schemas_hash, cnc->table_schema))
				goto out;
			g_hash_table_insert (schemas_hash, cnc->table_schema, 0x01);
			break;
		}
		case BROWSER_VIRTUAL_CONNECTION_PART_MODEL:
			TO_IMPLEMENT;
			goto out;
			break;
		}
	}

	allok = TRUE;

 out:
	g_hash_table_destroy (schemas_hash);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (cprop), GTK_RESPONSE_OK, allok);
}

/**
 * connection_binding_properties_get_specs
 * @prop: a #ConnectionBindingProperties widget
 *
 * Returns: a pointer to a read only #BrowserVirtualConnectionSpecs
 */
const BrowserVirtualConnectionSpecs *
connection_binding_properties_get_specs (ConnectionBindingProperties *prop)
{
	g_return_val_if_fail (CONNECTION_IS_BINDING_PROPERTIES (prop), NULL);

	return prop->priv->specs;
}
