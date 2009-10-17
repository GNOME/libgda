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

static void create_layout (ConnectionBindingProperties *cprop);
static void update_display (ConnectionBindingProperties *cprop);

struct _ConnectionBindingPropertiesPrivate
{
	BrowserVirtualConnectionSpecs *specs;
	GtkTable    *layout_table;
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
	GtkWidget *label, *hbox;
	gchar *str;

	str = g_strdup_printf ("<b>%s:</b>\n<small>%s</small>",
			       _("Virtual connection's properties"),
			       _("Define the sources of data for which tables will\n"
				 "appear in the virtual connection"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cprop)->vbox), label, FALSE, FALSE, 5);

	hbox = gtk_hbox_new (FALSE, 0); /* HIG */
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cprop)->vbox), hbox, TRUE, TRUE, 0);
	label = gtk_label_new ("      ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	cprop->priv->layout_table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
	gtk_box_pack_start (GTK_BOX (hbox), (GtkWidget*) cprop->priv->layout_table, TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_DIALOG (cprop)->vbox);
}

static void add_part_clicked_cb (GtkWidget *button, ConnectionBindingProperties *cprop);
static void del_part_clicked_cb (GtkWidget *button, BrowserVirtualConnectionPart *part);

static GtkWidget *create_part_for_model (BrowserVirtualConnectionModel *pm);
static GtkWidget *create_part_for_cnc (BrowserVirtualConnectionCnc *cnc);

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
				display = create_part_for_model (&(part->u.model));
				break;
			case BROWSER_VIRTUAL_CONNECTION_PART_CNC:
				display = create_part_for_cnc (&(part->u.cnc));
				break;
			default:
				g_assert_not_reached ();
			}

			gtk_table_attach (cprop->priv->layout_table, display, 0, 1, top, top + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

			button = gtk_button_new ();
			label = browser_make_tab_label_with_stock (NULL, GTK_STOCK_REMOVE, FALSE, NULL);
			gtk_container_add (GTK_CONTAINER (button), label);
			gtk_table_attach (cprop->priv->layout_table, button, 1, 2, top, top + 1, 0, GTK_FILL, 0, 0);

			g_signal_connect (button, "clicked",
					  G_CALLBACK (del_part_clicked_cb), part);
		}
	}

	/* bottom button to add a part */
	button = gtk_button_new ();
	label = browser_make_tab_label_with_stock (_("Add part"), GTK_STOCK_ADD, FALSE, NULL);
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_table_attach (cprop->priv->layout_table, button, 0, 2, top, top + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_part_clicked_cb), cprop);
	
	gtk_widget_show_all ((GtkWidget*) cprop->priv->layout_table);
}

static void
add_part_clicked_cb (GtkWidget *button, ConnectionBindingProperties *cprop)
{
	TO_IMPLEMENT;
}

static void
del_part_clicked_cb (GtkWidget *button, BrowserVirtualConnectionPart *part)
{
	TO_IMPLEMENT;
}

static GtkWidget *
create_part_for_model (BrowserVirtualConnectionModel *pm)
{
	GtkWidget *vbox, *label;
	gchar *str;

	vbox = gtk_vbox_new (FALSE, 0);
	label = gtk_label_new ("");
	str = g_markup_printf_escaped ("<b>%s</b>", _("Table from a data set:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

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
			    (*ptr != '_') ||
			    ! g_ascii_isalnum (*ptr)) {
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
}

static GtkWidget *
create_part_for_cnc (BrowserVirtualConnectionCnc *cnc)
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
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

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
	g_signal_connect (set, "holder-changed",
			  G_CALLBACK (part_for_cnc_holder_changed_cb), cnc);

	g_object_unref (set);
	gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

	return vbox;
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
