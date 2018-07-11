/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "data-console.h"
#include "data-widget.h"
#include "xml-spec-editor.h"
#include "ui-spec-editor.h"
#include "../dnd.h"
#include "../ui-support.h"
#include "../ui-customize.h"
#include "../gdaui-bar.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "../browser-perspective.h"
#include <libgda-ui/internal/popup-container.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda-ui/libgda-ui.h>
#include "data-source-manager.h"
#include <gdk/gdkkeysyms.h>
#include "analyser.h"
#include <libgda/gda-debug-macros.h>

#define MAIN_PAGE_EDITORS 0
#define MAIN_PAGE_DATA 1

#define EDITOR_PAGE_XML 1
#define EDITOR_PAGE_UI 0

typedef enum {
	LAYOUT_HORIZ,
	LAYOUT_VERT
} LayoutType;

struct _DataConsolePrivate {
	DataSourceManager *mgr;

	GdauiBar *header;
	LayoutType layout_type;
	TConnection *tcnc;

	GtkWidget *main_notebook; /* 2 pages: MAIN_PAGE_EDITORS & MAIN_PAGE_DATA */
	GtkWidget *editors_notebook; /* 2 pages: EDITOR_PAGE_XML & EDITOR_PAGE_UI */

	GtkWidget *data_box; /* in main_notebook */
	GtkWidget *data;

	GtkWidget *xml_sped; /* in editors_notebook */
	GtkWidget *ui_sped; /* in editors_notebook */

	gboolean toggling;
	GtkToggleButton *params_toggle;
	GtkWidget *params_top;
	GtkWidget *params_form_box;
	GtkWidget *params_form;

	gint fav_id; /* diagram's ID as a favorite, -1=>not a favorite */
	GtkWidget *save_button;
        GtkWidget *popup_container; /* to enter canvas's name */
        GtkWidget *name_entry;
        GtkWidget *real_save_button;

	GtkWidget *clear_xml_button;
	GtkWidget *add_source_button;
	GtkWidget *add_source_menu;
	gpointer   add_source_menu_index; /* used to know if @add_source_menu needs to be rebuilt */
	GSList    *customized_menu_items_list; /* list of GtkMenuItem in @add_source_menu */
};

static void data_console_class_init (DataConsoleClass *klass);
static void data_console_init       (DataConsole *dconsole, DataConsoleClass *klass);
static void data_console_dispose    (GObject *object);
static void data_console_show_all   (GtkWidget *widget);
static void data_console_grab_focus (GtkWidget *widget);
static gboolean key_press_event     (GtkWidget *widget, GdkEventKey *event);


static void data_source_mgr_changed_cb (DataSourceManager *mgr, DataConsole *dconsole);
static void data_source_source_changed_cb (DataSourceManager *mgr, DataSource *source, DataConsole *dconsole);

/* BrowserPage interface */
static void                 data_console_page_init (BrowserPageIface *iface);
static void                 data_console_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header);
static GtkWidget           *data_console_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static GObjectClass *parent_class = NULL;

/*
 * DataConsole class implementation
 */

static void
data_console_class_init (DataConsoleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	GTK_WIDGET_CLASS (klass)->show_all = data_console_show_all;
	GTK_WIDGET_CLASS (klass)->grab_focus = data_console_grab_focus;
	GTK_WIDGET_CLASS (klass)->key_press_event = key_press_event;
	object_class->dispose = data_console_dispose;
}

static void
data_console_show_all (GtkWidget *widget)
{
	DataConsole *dconsole = (DataConsole *) widget;
	GTK_WIDGET_CLASS (parent_class)->show_all (widget);

	if (!gtk_toggle_button_get_active (dconsole->priv->params_toggle))
		gtk_widget_hide (dconsole->priv->params_top);
}

static void
data_console_grab_focus (GtkWidget *widget)
{
	DataConsole *dconsole;

	dconsole = DATA_CONSOLE (widget);
	gtk_widget_grab_focus (GTK_WIDGET (dconsole->priv->xml_sped));
}

static gboolean
key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	DataConsole *dconsole;
	dconsole = DATA_CONSOLE (widget);
	if ((event->keyval == GDK_KEY_Escape) &&
	    (gtk_notebook_get_current_page (GTK_NOTEBOOK (dconsole->priv->main_notebook)) == MAIN_PAGE_DATA)) {
		GAction *action;
		action = customization_data_get_action (G_OBJECT (dconsole), "ComposeMode");
		if (action) {
			GVariant *value;
			value = g_variant_new_boolean (TRUE);
			g_simple_action_set_state (G_SIMPLE_ACTION (action), value);
		}
		return TRUE;
	}

	/* parent class */
	return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}

static void
data_console_page_init (BrowserPageIface *iface)
{
	iface->i_customize = data_console_customize;
	iface->i_uncustomize = NULL;
	iface->i_get_tab_label = data_console_get_tab_label;
}

static void
data_console_init (DataConsole *dconsole, G_GNUC_UNUSED DataConsoleClass *klass)
{
	dconsole->priv = g_new0 (DataConsolePrivate, 1);
	dconsole->priv->layout_type = LAYOUT_HORIZ;
	dconsole->priv->fav_id = -1;
	dconsole->priv->popup_container = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (dconsole), GTK_ORIENTATION_VERTICAL);
}

static void
data_console_dispose (GObject *object)
{
	DataConsole *dconsole = (DataConsole *) object;

	/* free memory */
	if (dconsole->priv) {
		if (dconsole->priv->add_source_menu)
			gtk_widget_destroy (dconsole->priv->add_source_menu);
		if (dconsole->priv->customized_menu_items_list)
			g_slist_free (dconsole->priv->customized_menu_items_list);
		if (dconsole->priv->params_form)
			gtk_widget_destroy (dconsole->priv->params_form);
		if (dconsole->priv->popup_container)
                        gtk_widget_destroy (dconsole->priv->popup_container);
		if (dconsole->priv->tcnc)
			g_object_unref (dconsole->priv->tcnc);
		if (dconsole->priv->mgr) {
			g_signal_handlers_disconnect_by_func (dconsole->priv->mgr,
							      G_CALLBACK (data_source_mgr_changed_cb),
							      dconsole);
			g_signal_handlers_disconnect_by_func (dconsole->priv->mgr,
							      G_CALLBACK (data_source_source_changed_cb),
							      dconsole);
			g_object_unref (dconsole->priv->mgr);
		}
		g_free (dconsole->priv);
		dconsole->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
data_console_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo console = {
			sizeof (DataConsoleClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_console_class_init,
			NULL,
			NULL,
			sizeof (DataConsole),
			0,
			(GInstanceInitFunc) data_console_init,
			0
		};

		static GInterfaceInfo page_console = {
                        (GInterfaceInitFunc) data_console_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "DataConsole", &console, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_console);
	}
	return type;
}

/**
 * data_console_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
data_console_new_with_fav_id (TConnection *tcnc, gint fav_id)
{
	GtkWidget *dconsole;
	dconsole = data_console_new (tcnc);
	data_console_set_fav_id (DATA_CONSOLE (dconsole), fav_id, NULL);

	return dconsole;
}

static void editor_clear_clicked_cb (GtkButton *button, DataConsole *dconsole);
static void add_source_clicked_cb (GtkButton *button, DataConsole *dconsole);
static void variables_clicked_cb (GtkToggleButton *button, DataConsole *dconsole);
static void execute_clicked_cb (GtkButton *button, DataConsole *dconsole);
#ifdef HAVE_GDU
static void help_clicked_cb (GtkButton *button, DataConsole *dconsole);
#endif
static void spec_editor_toggled_cb (GtkToggleButton *button, DataConsole *dconsole);
static void save_clicked_cb (GtkWidget *button, DataConsole *dconsole);

/**
 * data_console_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
data_console_new (TConnection *tcnc)
{
	DataConsole *dconsole;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	dconsole = DATA_CONSOLE (g_object_new (DATA_CONSOLE_TYPE, NULL));

	dconsole->priv->tcnc = g_object_ref (tcnc);
	dconsole->priv->mgr = data_source_manager_new (tcnc);
	g_signal_connect (dconsole->priv->mgr, "list-changed",
			  G_CALLBACK (data_source_mgr_changed_cb), dconsole);
	g_signal_connect (dconsole->priv->mgr, "source-changed",
			  G_CALLBACK (data_source_source_changed_cb), dconsole);
	
	/* header */
        GtkWidget *hbox, *label, *wid;
	gchar *str;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (dconsole), hbox, FALSE, FALSE, 0);

	str = g_strdup_printf ("<b>%s</b>\n%s", _("Data Manager"), _("Unsaved"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        gtk_widget_show (label);
	dconsole->priv->header = GDAUI_BAR (label);

	wid = gdaui_bar_add_button_from_icon_name (GDAUI_BAR (label), "document-save");
	dconsole->priv->save_button = wid;

	g_signal_connect (wid, "clicked",
			  G_CALLBACK (save_clicked_cb), dconsole);

        gtk_widget_show_all (hbox);

	/* main container */
	GtkWidget *hpaned, *nb;
	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (dconsole), hpaned, TRUE, TRUE, 0);

	/* variables */
        GtkWidget *vbox, *sw;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	dconsole->priv->params_top = vbox;
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, FALSE, TRUE);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Variables' values:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	dconsole->priv->params_form_box = gtk_viewport_new (NULL, NULL);
	gtk_widget_set_name (dconsole->priv->params_form_box, "gdaui-transparent-background");
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (dconsole->priv->params_form_box), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), dconsole->priv->params_form_box);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (dconsole->priv->params_form_box, 250, -1);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), VARIABLES_HELP);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_container_add (GTK_CONTAINER (dconsole->priv->params_form_box), label);
	dconsole->priv->params_form = label;

	/* main contents: 1 page for editors and 1 for execution widget */
	nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_paned_pack2 (GTK_PANED (hpaned), nb, TRUE, FALSE);
	dconsole->priv->main_notebook = nb;

	/* editors page */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (dconsole->priv->main_notebook), vbox, NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	nb = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (hbox), nb, TRUE, TRUE, 0);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (nb), FALSE);
	dconsole->priv->editors_notebook = nb;

	dconsole->priv->ui_sped = ui_spec_editor_new (dconsole->priv->mgr);
	gtk_widget_show (dconsole->priv->ui_sped);
	gtk_notebook_append_page (GTK_NOTEBOOK (dconsole->priv->editors_notebook),
				  dconsole->priv->ui_sped, NULL);

	dconsole->priv->xml_sped = xml_spec_editor_new (dconsole->priv->mgr);
	gtk_widget_show (dconsole->priv->xml_sped);
	gtk_notebook_append_page (GTK_NOTEBOOK (dconsole->priv->editors_notebook),
				  dconsole->priv->xml_sped, NULL);

	/* buttons */
	GtkWidget *bbox, *button;
	bbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, FALSE, 5);

	button = ui_make_small_button (FALSE, FALSE, _("Reset"), "edit-clear",
				       _("Reset the editor's\ncontents"));
	dconsole->priv->clear_xml_button = button;
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (editor_clear_clicked_cb), dconsole);

	button = ui_make_small_button (FALSE, TRUE, _("Add"), "list-add",
				       _("Add a new data source"));
	dconsole->priv->add_source_button = button;
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_source_clicked_cb), dconsole);

	button = ui_make_small_button (TRUE, FALSE, _("Variables"), NULL, _("Show variables needed"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	dconsole->priv->params_toggle = GTK_TOGGLE_BUTTON (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (variables_clicked_cb), dconsole);

	button = ui_make_small_button (FALSE, FALSE, _("Execute"), "system-run",
				       _("Execute specified\ndata manager"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (execute_clicked_cb), dconsole);

	button = ui_make_small_button (TRUE, FALSE, _("View XML"), NULL,
				       _("View specifications\nas XML (advanced)"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	spec_editor_toggled_cb (GTK_TOGGLE_BUTTON (button), dconsole);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (spec_editor_toggled_cb), dconsole);

#ifdef HAVE_GDU
	button = ui_make_small_button (FALSE, FALSE, _("Help"), "help-browser", _("Help"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (help_clicked_cb), dconsole);
#endif


	/* data contents page */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	dconsole->priv->data_box = vbox;
	gtk_notebook_append_page (GTK_NOTEBOOK (dconsole->priv->main_notebook), vbox, NULL);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("BBB:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);

	gtk_box_pack_start (GTK_BOX (dconsole->priv->data_box), wid, TRUE, TRUE, 0);
	dconsole->priv->data = wid;

	/* show everything */
        gtk_widget_show_all (hpaned);
	gtk_widget_hide (dconsole->priv->params_top);

	/* initial contents for tests */
#define DEFAULT_XML \
"<data>\n" \
"    <!-- This is an example of XML code, this editor will be replaced by a UI editing,\n" \
"         but will still be visible and useable for quick access\n" \
"    -->\n" \
"\n    <!-- specifies that we want the contents of the 'customers' table -->\n" \
"    <table name=\"customers\"/>\n" \
"\n    <!-- specifies that we want the contents of the 'orders' table, where the order ID \n" \
"         depends on the selected customer's ID -->\n"						\
"    <table name=\"orders\">\n" \
"        <depend foreign_key_table=\"customers\"/>\n" \
"    </table>\n" \
"\n    <!-- specifies that we want the result of the execution of a SELECT query, notice the WHERE clause\n" \
"          which enables to filter on the previously selected order ID-->\n" \
"    <query title=\"Order's products\" id=\"products\">\n" \
"        SELECT p.ref, p.name FROM products p INNER JOIN order_contents o ON (o.product_ref=p.ref) WHERE o.order_id=##orders@id::int\n" \
"    </query>\n" \
"\n    <!-- a more complicated query this time giving all the orders (for any customer) containing the \n" \
"         selected product from the previous SELECT query-->\n" \
"    <query title=\"All orders with this product\" id=\"aorders\">\n" \
"        SELECT distinct c.name, o.creation_date, o.delivery_date FROM customers c INNER JOIN orders o ON (o.customer=c.id) INNER JOIN order_contents oc ON (oc.order_id=o.id) INNER JOIN products p ON (oc.product_ref=##products@ref::string)\n" \
"    </query>\n" \
"</data>"
#undef DEFAULT_XML


#define DEFAULT_XML \
"<data>\n" \
"    <!--\n\n" \
"    <table name=\"\"/>\n" \
"        <depend foreign_key_table=\"\"/>\n" \
"    </table>\n" \
"    <query title=\"\" id=\"\">\n" \
"        SELECT ...\n" \
"    </query>\n\n" \
"    -->\n" \
"</data>"

	xml_spec_editor_set_xml_text (XML_SPEC_EDITOR (dconsole->priv->xml_sped), DEFAULT_XML);

	gtk_widget_grab_focus (dconsole->priv->xml_sped);
	return (GtkWidget*) dconsole;
}

/**
 * data_console_set_fav_id
 *
 * Sets the favorite ID of @dconsole: ensure every displayed information is up to date
 */
void
data_console_set_fav_id (DataConsole *dconsole, gint fav_id, GError **error)
{
	g_return_if_fail (IS_DATA_CONSOLE (dconsole));
	TFavoritesAttributes fav;

	if ((fav_id >=0) &&
	    t_favorites_get (t_connection_get_favorites (dconsole->priv->tcnc),
			     fav_id, &fav, error)) {
		gchar *str, *tmp;
		tmp = g_markup_printf_escaped (_("'%s' data manager"), fav.name);
		str = g_strdup_printf ("<b>%s</b>\n%s", _("Data manager"), tmp);
		g_free (tmp);
		gdaui_bar_set_text (dconsole->priv->header, str);
		g_free (str);
		
		dconsole->priv->fav_id = fav.id;
		
		t_favorites_reset_attributes (&fav);
	}
	else {
		gchar *str;
		str = g_strdup_printf ("<b>%s</b>\n%s", _("Data manager"), _("Unsaved"));
		gdaui_bar_set_text (dconsole->priv->header, str);
		g_free (str);
		dconsole->priv->fav_id = -1;
	}
}

/*
 * POPUP
 */
static void
real_save_clicked_cb (GtkWidget *button, DataConsole *dconsole)
{
	gchar *str;

	str = xml_spec_editor_get_xml_text (XML_SPEC_EDITOR (dconsole->priv->xml_sped));

	GError *lerror = NULL;
	TFavorites *bfav;
	TFavoritesAttributes favatt;

	memset (&favatt, 0, sizeof (TFavoritesAttributes));
	favatt.id = dconsole->priv->fav_id;
	favatt.type = T_FAVORITES_DATA_MANAGERS;
	favatt.name = gtk_editable_get_chars (GTK_EDITABLE (dconsole->priv->name_entry), 0, -1);
	if (!*favatt.name) {
		g_free (favatt.name);
		favatt.name = g_strdup (_("Data manager"));
	}
	favatt.contents = str;
	
	gtk_widget_hide (dconsole->priv->popup_container);
	
	bfav = t_connection_get_favorites (dconsole->priv->tcnc);
	if (! t_favorites_add (bfav, 0, &favatt, ORDER_KEY_DATA_MANAGERS, G_MAXINT, &lerror)) {
		ui_show_error ((GtkWindow*) gtk_widget_get_toplevel (button),
			       "<b>%s:</b>\n%s",
			       _("Could not save data manager"),
			       lerror && lerror->message ? lerror->message : _("No detail"));
		if (lerror)
			g_error_free (lerror);
	}

	data_console_set_fav_id (dconsole, favatt.id, NULL);

	g_free (favatt.name);
	g_free (str);
}

static void
save_clicked_cb (GtkWidget *button, DataConsole *dconsole)
{
	gchar *str;

	if (!dconsole->priv->popup_container) {
		GtkWidget *window, *wid, *hbox;

		window = popup_container_new (button);
		dconsole->priv->popup_container = window;

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_container_add (GTK_CONTAINER (window), hbox);
		wid = gtk_label_new ("");
		str = g_strdup_printf ("%s:", _("Data manager's name"));
		gtk_label_set_markup (GTK_LABEL (wid), str);
		g_free (str);
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

		wid = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 5);
		dconsole->priv->name_entry = wid;
		if (dconsole->priv->fav_id > 0) {
			TFavoritesAttributes fav;
			if (t_favorites_get (t_connection_get_favorites (dconsole->priv->tcnc),
					     dconsole->priv->fav_id, &fav, NULL)) {
				gtk_entry_set_text (GTK_ENTRY (wid), fav.name);
				t_favorites_reset_attributes (&fav);
			}
		}

		g_signal_connect (wid, "activate",
				  G_CALLBACK (real_save_clicked_cb), dconsole);

		wid = gtk_button_new_with_label (_("Save"));
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
		g_signal_connect (wid, "clicked",
				  G_CALLBACK (real_save_clicked_cb), dconsole);
		dconsole->priv->real_save_button = wid;

		gtk_widget_show_all (hbox);
	}

        gtk_widget_show (dconsole->priv->popup_container);
}

static void
execute_clicked_cb (G_GNUC_UNUSED GtkButton *button, DataConsole *dconsole)
{
	data_console_execute (dconsole);
}

#ifdef HAVE_GDU
static void
help_clicked_cb (G_GNUC_UNUSED GtkButton *button, DataConsole *dconsole)
{
	ui_show_help ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) dconsole),
		      "data-manager-perspective");
}
#endif

static void
variables_clicked_cb (GtkToggleButton *button, DataConsole *dconsole)
{
	if (gtk_toggle_button_get_active (button))
		gtk_widget_show (dconsole->priv->params_top);
	else
		gtk_widget_hide (dconsole->priv->params_top);
}

static void
spec_editor_toggled_cb (GtkToggleButton *button, DataConsole *dconsole)
{
	gint display;
	display = gtk_toggle_button_get_active (button) ? EDITOR_PAGE_XML : EDITOR_PAGE_UI;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (dconsole->priv->editors_notebook),
				       display);
	switch (display) {
	case EDITOR_PAGE_XML:
		gtk_widget_set_sensitive (dconsole->priv->clear_xml_button, TRUE);
		gtk_widget_set_sensitive (dconsole->priv->add_source_button, FALSE);
		break;
	case EDITOR_PAGE_UI:
		gtk_widget_set_sensitive (dconsole->priv->clear_xml_button, FALSE);
		gtk_widget_set_sensitive (dconsole->priv->add_source_button, TRUE);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
param_activated_cb (G_GNUC_UNUSED GdauiBasicForm *form, DataConsole *dconsole)
{
	DataWidget *dwid = NULL;
	if (dconsole->priv->data)
		dwid = g_object_get_data ((GObject*) dconsole->priv->data, "data-widget");
	if (dwid)
		data_widget_rerun (DATA_WIDGET (dwid));
}

static void
data_source_source_changed_cb (DataSourceManager *mgr, G_GNUC_UNUSED DataSource *source,
			       DataConsole *dconsole)
{
	data_source_mgr_changed_cb (mgr, dconsole);
}

static void
data_source_mgr_changed_cb (DataSourceManager *mgr, DataConsole *dconsole)
{
	if (dconsole->priv->params_form) {
		gtk_widget_destroy (dconsole->priv->params_form);
		dconsole->priv->params_form = NULL;
	}

	GdaSet *params;
	params = data_source_manager_get_params (mgr);
	if (params) {
		dconsole->priv->params_form = gdaui_basic_form_new (params);
		g_signal_connect (dconsole->priv->params_form, "activated",
				  G_CALLBACK (param_activated_cb), dconsole);
	}
	else {
		dconsole->priv->params_form = gtk_label_new ("");
                gtk_label_set_markup (GTK_LABEL (dconsole->priv->params_form), VARIABLES_HELP);
	}
	gtk_container_add (GTK_CONTAINER (dconsole->priv->params_form_box),
			   dconsole->priv->params_form);
	gtk_widget_show (dconsole->priv->params_form);
}

static void
editor_clear_clicked_cb (G_GNUC_UNUSED GtkButton *button, DataConsole *dconsole)
{
	xml_spec_editor_set_xml_text (XML_SPEC_EDITOR (dconsole->priv->xml_sped), DEFAULT_XML);
	gtk_widget_grab_focus (dconsole->priv->xml_sped);
}

static void
add_source_mitem_activated_cb (GtkMenuItem *mitem, DataConsole *dconsole)
{
	const gchar *table;
	DataSource *source;
	gchar *str;
	GSList *list;

	table = (gchar*) g_object_get_data ((GObject*) mitem, "_table");

	source = data_source_new (dconsole->priv->tcnc, DATA_SOURCE_UNKNOWN);
	list = (GSList*) data_source_manager_get_sources (dconsole->priv->mgr);
	str = g_strdup_printf (_("source%d"), g_slist_length (list) + 1);
	data_source_set_id (source, str);
	g_free (str);
	if (table)
		data_source_set_table (source, table, NULL);
	else
		data_source_set_query (source, "SELECT", NULL);
	data_source_manager_add_source (dconsole->priv->mgr, source);
	ui_spec_editor_select_source (UI_SPEC_EDITOR (dconsole->priv->ui_sped), source);
	g_object_unref (source);
}

static gint
dbo_sort (GdaMetaDbObject *dbo1, GdaMetaDbObject *dbo2)
{
	gint res;
	res = g_strcmp0 (dbo1->obj_schema, dbo2->obj_schema);
	if (res)
		return - res;
	return g_strcmp0 (dbo1->obj_name, dbo2->obj_name);
}

static void
add_source_clicked_cb (G_GNUC_UNUSED GtkButton *button, DataConsole *dconsole)
{
	GdaMetaStruct *mstruct;
	DataSource *current_source;

	mstruct = t_connection_get_meta_struct (dconsole->priv->tcnc);
	current_source = ui_spec_editor_get_selected_source (UI_SPEC_EDITOR (dconsole->priv->ui_sped));

	/* remove customization, if any */
	if (dconsole->priv->customized_menu_items_list) {
		g_slist_foreach (dconsole->priv->customized_menu_items_list,
				 (GFunc) gtk_widget_destroy, NULL);
		g_slist_free (dconsole->priv->customized_menu_items_list);
		dconsole->priv->customized_menu_items_list = NULL;
	}

	if (dconsole->priv->add_source_menu &&
	    (dconsole->priv->add_source_menu_index != (gpointer) mstruct)) {
		gtk_widget_destroy (dconsole->priv->add_source_menu);
		dconsole->priv->add_source_menu = NULL;
		dconsole->priv->add_source_menu_index = NULL;
	}
	if (dconsole->priv->add_source_menu) {
		if (current_source)
			dconsole->priv->customized_menu_items_list =
				data_manager_add_proposals_to_menu (dconsole->priv->add_source_menu,
								    dconsole->priv->mgr,
								    current_source, GTK_WIDGET (dconsole));
		gtk_menu_popup_at_pointer (GTK_MENU (dconsole->priv->add_source_menu), NULL);
		return;
	}

	GtkWidget *menu, *mitem, *submenu = NULL;
	menu = gtk_menu_new ();
	mitem = gtk_menu_item_new_with_label (_("Data source from SQL"));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (add_source_mitem_activated_cb), dconsole);
	gtk_widget_show (mitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

	if (mstruct) {
		gboolean sep_added = FALSE;
		GSList *dbo_list, *list;
		gchar *current_schema = NULL;

		dbo_list = gda_meta_struct_get_all_db_objects (mstruct);
		list = g_slist_sort (dbo_list, (GCompareFunc) dbo_sort);
		dbo_list = list;
		for (list = dbo_list; list; list = list->next) {
			GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
			gchar *str;
			if (dbo->obj_type != GDA_META_DB_TABLE)
				continue;
			if (! sep_added) {
				mitem = gtk_separator_menu_item_new ();
				gtk_widget_show (mitem);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
				sep_added = TRUE;
			}

			if (!strcmp (dbo->obj_short_name, dbo->obj_full_name)) {
				gboolean schema_changed = TRUE;
				if (!current_schema)
					current_schema = g_strdup (dbo->obj_schema);
				else if (strcmp (current_schema, dbo->obj_schema)) {
					g_free (current_schema);
					current_schema = g_strdup (dbo->obj_schema);
				}
				else
					schema_changed = FALSE;

				if (schema_changed) {
					str = g_strdup_printf (_("In schema '%s'"), current_schema);
					mitem = gtk_menu_item_new_with_label (str);
					gtk_widget_show (mitem);
					g_free (str);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
					submenu = gtk_menu_new ();
					gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), submenu);
				}
			}

			str = g_strdup_printf (_("For table: %s"), dbo->obj_name);
			mitem = gtk_menu_item_new_with_label (str);
			g_object_set_data_full ((GObject*) mitem, "_table",
						g_strdup (dbo->obj_short_name), g_free);
			g_signal_connect (mitem, "activate",
					  G_CALLBACK (add_source_mitem_activated_cb), dconsole);
			gtk_widget_show (mitem);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu ? submenu : menu), mitem);
		}
		g_slist_free (dbo_list);
		g_free (current_schema);
	}
	dconsole->priv->add_source_menu_index = (gpointer) mstruct;
	dconsole->priv->add_source_menu = menu;

	if (current_source)
		dconsole->priv->customized_menu_items_list =
			data_manager_add_proposals_to_menu (dconsole->priv->add_source_menu,
							    dconsole->priv->mgr,
							    current_source, GTK_WIDGET (dconsole));

	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

/*
 * UI actions
 */
static void
compose_mode_toggled_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	DataConsole *dconsole;
	dconsole = DATA_CONSOLE (data);

	/* show variables if necessary */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dconsole->priv->params_toggle),
				      dconsole->priv->params_form &&
				      GDAUI_IS_BASIC_FORM (dconsole->priv->params_form) ? TRUE : FALSE);

	g_simple_action_set_state (action, state);
	if (dconsole->priv->toggling) {
		dconsole->priv->toggling = FALSE;
		return;
	}

	gint pagenb;
	pagenb = gtk_notebook_get_current_page (GTK_NOTEBOOK (dconsole->priv->main_notebook));
	if (pagenb == MAIN_PAGE_EDITORS) {
		/* Get Data sources */
		pagenb = MAIN_PAGE_DATA;
		if (dconsole->priv->data) {
			/* destroy existing data widgets */
			gtk_widget_destroy (dconsole->priv->data);
			dconsole->priv->data = NULL;
		}

		GtkWidget *sw, *vp, *dwid;
		
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
		
		vp = gtk_viewport_new (NULL, NULL);
		gtk_widget_set_name (vp, "gdaui-transparent-background");
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);
		gtk_container_add (GTK_CONTAINER (sw), vp);
		
		dwid = data_widget_new (dconsole->priv->mgr);
		gtk_container_add (GTK_CONTAINER (vp), dwid);
		g_object_set_data ((GObject*) sw, "data-widget", dwid);
		
		gtk_widget_show_all (vp);

		dconsole->priv->data = sw;
		gtk_box_pack_start (GTK_BOX (dconsole->priv->data_box), sw, TRUE, TRUE, 0);
		gtk_widget_show (sw);
	}
	else {
		/* simply change the current page */
		pagenb = MAIN_PAGE_EDITORS;
	}

	if (pagenb == MAIN_PAGE_DATA)
		browser_window_show_notice_printf (BROWSER_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) dconsole)),
						   GTK_MESSAGE_INFO,
						   "data-manager-exec-mode-switched",
						   "%s", _("Switching to execution mode. Hit the Escape key "
						     "to return to the compose mode"));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (dconsole->priv->main_notebook), pagenb);
}

static GActionEntry win_entries[] = {
	{ "ComposeMode", NULL, NULL, "false",  compose_mode_toggled_cb},
};

static void
data_console_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	customization_data_init (G_OBJECT (page), toolbar, header);

	/* add page's actions */
	customization_data_add_actions (G_OBJECT (page), win_entries, G_N_ELEMENTS (win_entries));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "applications-engineering-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Switch between compose and execute modes"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.ComposeMode");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));
}

static GtkWidget *
data_console_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	const gchar *tab_name;

	tab_name = _("Data manager");
	return ui_make_tab_label_with_icon (tab_name,
					    NULL,
					    out_close_button ? TRUE : FALSE, out_close_button);
}

/**
 * data_console_set_text
 */
void
data_console_set_text (DataConsole *console, const gchar *text)
{
	g_return_if_fail (IS_DATA_CONSOLE (console));

	xml_spec_editor_set_xml_text (XML_SPEC_EDITOR (console->priv->xml_sped), text);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (console->priv->main_notebook), MAIN_PAGE_EDITORS);
}

/**
 * data_console_get_text
 */
gchar *
data_console_get_text (DataConsole *console)
{
	g_return_val_if_fail (IS_DATA_CONSOLE (console), NULL);
	
	return xml_spec_editor_get_xml_text (XML_SPEC_EDITOR (console->priv->xml_sped));
}

/**
 * data_console_is_unused
 */
gboolean
data_console_is_unused (DataConsole *console)
{
	gchar *text;
	gboolean retval = TRUE;
	g_return_val_if_fail (IS_DATA_CONSOLE (console), FALSE);
	text = data_console_get_text (console);
	if (!text || !*text) {
		g_free (text);
		return TRUE;
	}
	if (strcmp (text, DEFAULT_XML))
		retval = FALSE;
	g_free (text);
	return retval;
}

/**
 * data_console_execute
 * @console: a #DataConsole widget
 *
 * Execute's @console's specs, if possible
 */
void
data_console_execute (DataConsole *console)
{
	g_return_if_fail (IS_DATA_CONSOLE (console));
	GAction *action;
	action = customization_data_get_action (G_OBJECT (console), "ComposeMode");
	if (action) {
		GVariant *value;
		value = g_variant_new_boolean (FALSE);
		g_simple_action_set_state (G_SIMPLE_ACTION (action), value);
	}
}
