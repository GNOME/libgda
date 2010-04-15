/*
 * Copyright (C) 2009 - 2010 The GNOME Foundation
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "data-console.h"
#include "data-widget.h"
#include "spec-editor.h"
#include "../dnd.h"
#include "../support.h"
#include "../cc-gray-bar.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "../browser-stock-icons.h"
#include "../common/popup-container.h"
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda-ui/libgda-ui.h>

#define PAGE_XML 0
#define PAGE_DATA 1

typedef enum {
	LAYOUT_HORIZ,
	LAYOUT_VERT
} LayoutType;

struct _DataConsolePrivate {
	LayoutType layout_type;
	BrowserConnection *bcnc;
	GtkWidget *notebook;
	SpecEditor *sped;
	GtkWidget *data_box; /* in notebook */
	GtkWidget *data;
	GtkActionGroup *agroup;

	gboolean toggling;
	GtkToggleButton *params_toggle;
	GtkWidget *params_top;
	GtkWidget *params_form_box;
	GtkWidget *params_form;
};

static void data_console_class_init (DataConsoleClass *klass);
static void data_console_init       (DataConsole *dconsole, DataConsoleClass *klass);
static void data_console_dispose    (GObject *object);
static void data_console_show_all   (GtkWidget *widget);

/* BrowserPage interface */
static void                 data_console_page_init (BrowserPageIface *iface);
static GtkActionGroup      *data_console_page_get_actions_group (BrowserPage *page);
static const gchar         *data_console_page_get_actions_ui (BrowserPage *page);
static GtkWidget           *data_console_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

enum {
	DUMMY,
	LAST_SIGNAL
};

static guint data_console_signals[LAST_SIGNAL] = { };
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
data_console_page_init (BrowserPageIface *iface)
{
	iface->i_get_actions_group = data_console_page_get_actions_group;
	iface->i_get_actions_ui = data_console_page_get_actions_ui;
	iface->i_get_tab_label = data_console_page_get_tab_label;
}

static void
data_console_init (DataConsole *dconsole, DataConsoleClass *klass)
{
	dconsole->priv = g_new0 (DataConsolePrivate, 1);
	dconsole->priv->layout_type = LAYOUT_HORIZ;
}

static void
data_console_dispose (GObject *object)
{
	DataConsole *dconsole = (DataConsole *) object;

	/* free memory */
	if (dconsole->priv) {
		if (dconsole->priv->bcnc)
			g_object_unref (dconsole->priv->bcnc);
		if (dconsole->priv->agroup)
			g_object_unref (dconsole->priv->agroup);
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
			(GInstanceInitFunc) data_console_init
		};

		static GInterfaceInfo page_console = {
                        (GInterfaceInitFunc) data_console_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_VBOX, "DataConsole", &console, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_console);
	}
	return type;
}

static void editor_clear_clicked_cb (GtkButton *button, DataConsole *dconsole);
static void variables_clicked_cb (GtkToggleButton *button, DataConsole *dconsole);
static void execute_clicked_cb (GtkButton *button, DataConsole *dconsole);
#ifdef HAVE_GDU
static void help_clicked_cb (GtkButton *button, DataConsole *dconsole);
#endif
static void spec_editor_toggled_cb (GtkToggleButton *button, DataConsole *dconsole);
static void spec_editor_changed_cb (SpecEditor *sped, DataConsole *dconsole);

/**
 * data_console_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
data_console_new (BrowserConnection *bcnc)
{
	DataConsole *dconsole;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	dconsole = DATA_CONSOLE (g_object_new (DATA_CONSOLE_TYPE, NULL));

	dconsole->priv->bcnc = g_object_ref (bcnc);
	
	/* header */
        GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>", _("Data Manager"));
	label = cc_gray_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (dconsole), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	/* main container */
	GtkWidget *hpaned, *nb;
	hpaned = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (dconsole), hpaned, TRUE, TRUE, 0);

	/* variables */
        GtkWidget *vbox, *sw;
	vbox = gtk_vbox_new (FALSE, 0);
	dconsole->priv->params_top = vbox;
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, FALSE, TRUE);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Variables' values:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	dconsole->priv->params_form_box = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (dconsole->priv->params_form_box), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), dconsole->priv->params_form_box);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (dconsole->priv->params_form_box, 250, -1);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), VARIABLES_HELP);
	gtk_misc_set_alignment (GTK_MISC (label), -1, 0.);
	gtk_container_add (GTK_CONTAINER (dconsole->priv->params_form_box), label);
	dconsole->priv->params_form = label;

	/* main contents */
	nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_paned_pack2 (GTK_PANED (hpaned), nb, TRUE, FALSE);
	dconsole->priv->notebook = nb;

	/* editor page */
	GtkWidget *hbox;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), vbox, NULL);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("SQL code to execute:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	dconsole->priv->sped = spec_editor_new (dconsole->priv->bcnc);
	g_signal_connect (dconsole->priv->sped, "changed",
			  G_CALLBACK (spec_editor_changed_cb), dconsole);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (dconsole->priv->sped), TRUE, TRUE, 0);

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

	//spec_editor_set_xml_text (dconsole->priv->sped, DEFAULT_XML);

	/* buttons */
	GtkWidget *bbox, *button;
	bbox = gtk_vbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, FALSE, 5);

	button = browser_make_small_button (FALSE, _("Clear"), GTK_STOCK_CLEAR, _("Clear the editor's\ncontents"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (editor_clear_clicked_cb), dconsole);

	button = browser_make_small_button (TRUE, _("Variables"), NULL, _("Show variables needed"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	dconsole->priv->params_toggle = GTK_TOGGLE_BUTTON (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (variables_clicked_cb), dconsole);

	button = browser_make_small_button (FALSE, _("Execute"), GTK_STOCK_EXECUTE, _("Execute specified\n"
										      "data manager"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (execute_clicked_cb), dconsole);

	button = browser_make_small_button (TRUE, _("View XML"), NULL, _("View specifications\n"
									 "as XML (advanced)"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      spec_editor_get_mode (dconsole->priv->sped) == SPEC_EDITOR_XML ? 
				      TRUE : FALSE);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (spec_editor_toggled_cb), dconsole);

#ifdef HAVE_GDU
	button = browser_make_small_button (FALSE, _("Help"), GTK_STOCK_HELP, _("Help"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (help_clicked_cb), dconsole);
#endif


	/* data contents page */
	GtkWidget *wid;

	vbox = gtk_vbox_new (FALSE, 0);
	dconsole->priv->data_box = vbox;
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), vbox, NULL);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("BBB:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);

	gtk_box_pack_start (GTK_BOX (dconsole->priv->data_box), wid, TRUE, TRUE, 0);
	dconsole->priv->data = wid;

	/* show everything */
        gtk_widget_show_all (hpaned);
	gtk_widget_hide (dconsole->priv->params_top);

	return (GtkWidget*) dconsole;
}

static void
execute_clicked_cb (GtkButton *button, DataConsole *dconsole)
{
	data_console_execute (dconsole);
}

#ifdef HAVE_GDU
static void
help_clicked_cb (GtkButton *button, DataConsole *dconsole)
{
	browser_show_help ((GtkWindow*) gtk_widget_get_toplevel (dconsole), "data-manager-perspective");
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
	if (gtk_toggle_button_get_active (button))
		spec_editor_set_mode (dconsole->priv->sped, SPEC_EDITOR_XML);
	else
		spec_editor_set_mode (dconsole->priv->sped, SPEC_EDITOR_UI);
}

static void
spec_editor_changed_cb (SpecEditor *sped, DataConsole *dconsole)
{
	if (dconsole->priv->params_form) {
		gtk_widget_destroy (dconsole->priv->params_form);
		dconsole->priv->params_form = NULL;
	}

	GdaSet *params;
	gboolean show_variables = FALSE;
	params = spec_editor_get_params (sped);
	if (params) {
		dconsole->priv->params_form = gdaui_basic_form_new (params);
		g_object_set ((GObject*) dconsole->priv->params_form,
			      "show-actions", TRUE, NULL);
		show_variables = TRUE;
	}
	else {
		dconsole->priv->params_form = gtk_label_new ("");
                gtk_label_set_markup (GTK_LABEL (dconsole->priv->params_form), VARIABLES_HELP);
	}
	gtk_container_add (GTK_CONTAINER (dconsole->priv->params_form_box),
			   dconsole->priv->params_form);
	gtk_widget_show (dconsole->priv->params_form);

	/* force showing variables */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dconsole->priv->params_toggle), show_variables);
}

static void
editor_clear_clicked_cb (GtkButton *button, DataConsole *dconsole)
{
	spec_editor_set_xml_text (dconsole->priv->sped, "");
	gtk_widget_grab_focus (GTK_WIDGET (dconsole->priv->sped));
}

static GtkWidget *
create_widget (DataConsole *dconsole, GArray *sources_array, GError **error)
{
	GtkWidget *sw, *vp, *dwid;

	if (! sources_array) {
		g_set_error (error, 0, 0,
			     _("No data source defined"));
		return NULL;
	}

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);

	vp = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), vp);

	dwid = data_widget_new (sources_array);
	gtk_container_add (GTK_CONTAINER (vp), dwid);

	gtk_widget_show_all (vp);
	return sw;
}

/*
 * UI actions
 */
static void
compose_mode_toggled_cb (GtkToggleAction *action, DataConsole *dconsole)
{
	gint pagenb;

	if (dconsole->priv->toggling) {
		dconsole->priv->toggling = FALSE;
		return;
	}

	pagenb = gtk_notebook_get_current_page (GTK_NOTEBOOK (dconsole->priv->notebook));
	if (pagenb == PAGE_XML) {
		/* Get Data sources */
		GArray *sources_array;
		GError *lerror = NULL;
		sources_array = spec_editor_get_sources_array (dconsole->priv->sped, &lerror);
		if (sources_array) {
			if (dconsole->priv->data) {
				/* destroy existing data widgets */
				gtk_widget_destroy (dconsole->priv->data);
				dconsole->priv->data = NULL;
			}

			GtkWidget *wid;
			wid = create_widget (dconsole, sources_array, &lerror);
			spec_editor_destroy_sources_array (sources_array);
			if (wid) {
				dconsole->priv->data = wid;
				gtk_box_pack_start (GTK_BOX (dconsole->priv->data_box), wid, TRUE, TRUE, 0);
				gtk_widget_show (wid);
				pagenb = PAGE_DATA;
			}
		}
		if (lerror) {
			browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) dconsole),
					    lerror && lerror->message ? lerror->message :
					    _("Error parsing XML specifications"));
			g_clear_error (&lerror);
		}
		if (pagenb == PAGE_XML) {
			dconsole->priv->toggling = TRUE;
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		}
	}
	else {
		/* simply change the current page */
		pagenb = PAGE_XML;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (dconsole->priv->notebook), pagenb);
}

static GtkToggleActionEntry ui_actions[] = {
	{ "ComposeMode", BROWSER_STOCK_BUILDER, N_("_Toggle mode"), NULL, N_("Switch between compose and execute modes"),
	  G_CALLBACK (compose_mode_toggled_cb), TRUE},
};
static const gchar *ui_actions_console =
	"<ui>"
        "  <menubar name='MenuBar'>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='Data manager' action='DataManagerMenu'>"
        "        <menuitem name='ComposeMode' action= 'ComposeMode'/>"
        "      </menu>"
	"    </placeholder>"
        "  </menubar>"
	"  <toolbar name='ToolBar'>"
	"    <separator/>"
	"    <toolitem action='ComposeMode'/>"
	"  </toolbar>"
	"</ui>";

static GtkActionGroup *
data_console_page_get_actions_group (BrowserPage *page)
{
	DataConsole *dconsole;
	dconsole = DATA_CONSOLE (page);
	if (! dconsole->priv->agroup) {
		dconsole->priv->agroup = gtk_action_group_new ("QueryExecConsoleActions");
		gtk_action_group_add_toggle_actions (dconsole->priv->agroup,
						     ui_actions, G_N_ELEMENTS (ui_actions), page);
	}
	return g_object_ref (dconsole->priv->agroup);
}

static const gchar *
data_console_page_get_actions_ui (BrowserPage *page)
{
	return ui_actions_console;
}

static GtkWidget *
data_console_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	DataConsole *dconsole;
	const gchar *tab_name;

	dconsole = DATA_CONSOLE (page);
	tab_name = _("Data manager");
	return browser_make_tab_label_with_stock (tab_name,
						  STOCK_CONSOLE,
						  out_close_button ? TRUE : FALSE, out_close_button);
}

/**
 * data_console_set_text
 */
void
data_console_set_text (DataConsole *console, const gchar *text)
{
	g_return_if_fail (IS_DATA_CONSOLE (console));

	spec_editor_set_xml_text (console->priv->sped, text);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (console->priv->notebook), PAGE_XML);
}

/**
 * data_console_get_text
 */
gchar *
data_console_get_text (DataConsole *console)
{
	g_return_val_if_fail (IS_DATA_CONSOLE (console), NULL);
	
	return spec_editor_get_xml_text (console->priv->sped);
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
	if (console->priv->agroup) {
		GtkAction *action;
		action = gtk_action_group_get_action (console->priv->agroup, "ComposeMode");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
	}
}
