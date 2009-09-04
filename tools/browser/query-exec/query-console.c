/*
 * Copyright (C) 2009 The GNOME Foundation
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
#include <gtk/gtk.h>
#include "query-console.h"
#include "../dnd.h"
#include "../support.h"
#include "../cc-gray-bar.h"
#include "query-exec-perspective.h"
#include "../browser-page.h"
#include "../browser-stock-icons.h"
#include "query-editor.h"
#include <libgda/sql-parser/gda-sql-parser.h>

struct _QueryConsolePrivate {
	BrowserConnection *bcnc;
	GdaSqlParser *parser;

	CcGrayBar *header;
	GtkWidget *vpaned; /* top=>query editor, bottom=>results */
	QueryEditor *editor;

	GtkToggleButton *params_toggle;
	GtkWidget *params_form;
	
	QueryEditor *history;
};

static void query_console_class_init (QueryConsoleClass *klass);
static void query_console_init       (QueryConsole *tconsole, QueryConsoleClass *klass);
static void query_console_dispose   (GObject *object);
static void query_console_show_all (GtkWidget *widget);

/* BrowserPage interface */
static void                 query_console_page_init (BrowserPageIface *iface);
static GtkActionGroup      *query_console_page_get_actions_group (BrowserPage *page);
static const gchar         *query_console_page_get_actions_ui (BrowserPage *page);
static GtkWidget           *query_console_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

enum {
	LAST_SIGNAL
};

static guint query_console_signals[LAST_SIGNAL] = { };
static GObjectClass *parent_class = NULL;

/*
 * QueryConsole class implementation
 */

static void
query_console_class_init (QueryConsoleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = query_console_dispose;
	GTK_WIDGET_CLASS (klass)->show_all = query_console_show_all;
}

static void
query_console_show_all (GtkWidget *widget)
{
	QueryConsole *tconsole = (QueryConsole *) widget;
	GTK_WIDGET_CLASS (parent_class)->show_all (widget);

	if (gtk_toggle_button_get_active (tconsole->priv->params_toggle))
		gtk_widget_show (tconsole->priv->params_form);
	else
		gtk_widget_hide (tconsole->priv->params_form);
}

static void
query_console_page_init (BrowserPageIface *iface)
{
	iface->i_get_actions_group = query_console_page_get_actions_group;
	iface->i_get_actions_ui = query_console_page_get_actions_ui;
	iface->i_get_tab_label = query_console_page_get_tab_label;
}

static void
query_console_init (QueryConsole *tconsole, QueryConsoleClass *klass)
{
	tconsole->priv = g_new0 (QueryConsolePrivate, 1);
	tconsole->priv->parser = NULL;
}

static void
query_console_dispose (GObject *object)
{
	QueryConsole *tconsole = (QueryConsole *) object;

	/* free memory */
	if (tconsole->priv) {
		if (tconsole->priv->bcnc)
			g_object_unref (tconsole->priv->bcnc);
		if (tconsole->priv->parser)
			g_object_unref (tconsole->priv->parser);

		g_free (tconsole->priv);
		tconsole->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
query_console_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo console = {
			sizeof (QueryConsoleClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_console_class_init,
			NULL,
			NULL,
			sizeof (QueryConsole),
			0,
			(GInstanceInitFunc) query_console_init
		};

		static GInterfaceInfo page_console = {
                        (GInterfaceInitFunc) query_console_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_VBOX, "QueryConsole", &console, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_console);
	}
	return type;
}

static GtkWidget *make_small_button (gboolean is_toggle,
				     const gchar *label, const gchar *stock_id, const gchar *tooltip);

static void sql_clear_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void sql_variables_clicked_cb (GtkToggleButton *button, QueryConsole *tconsole);
static void sql_execute_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void sql_indent_clicked_cb (GtkButton *button, QueryConsole *tconsole);

/**
 * query_console_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
query_console_new (BrowserConnection *bcnc)
{
	QueryConsole *tconsole;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	tconsole = QUERY_CONSOLE (g_object_new (QUERY_CONSOLE_TYPE, NULL));

	tconsole->priv->bcnc = g_object_ref (bcnc);
	
	/* header */
        GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>", _("Query editor"));
	label = cc_gray_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (tconsole), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
	tconsole->priv->header = CC_GRAY_BAR (label);

	/* main contents */
	GtkWidget *vpaned;
	vpaned = gtk_vpaned_new ();
	tconsole->priv->vpaned = NULL;
	gtk_box_pack_start (GTK_BOX (tconsole), vpaned, TRUE, TRUE, 0);	

	/* top paned for the editor */
	GtkWidget *wid, *vbox, *hbox, *bbox, *hpaned, *button;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_add1 (GTK_PANED (vpaned), vbox);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("SQL code to execute:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	hpaned = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (hbox), hpaned, TRUE, TRUE, 0);

	wid = query_editor_new ();
	tconsole->priv->editor = QUERY_EDITOR (wid);
	gtk_paned_pack1 (GTK_PANED (hpaned), wid, TRUE, FALSE);
	
	wid = gtk_label_new ("Here goes the\nparams' form");
	tconsole->priv->params_form = wid;
	gtk_paned_pack2 (GTK_PANED (hpaned), wid, FALSE, TRUE);
	
	bbox = gtk_vbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, FALSE, 5);

	button = make_small_button (FALSE, _("Clear"), GTK_STOCK_CLEAR, _("Clear the editor"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (sql_clear_clicked_cb), tconsole);

	button = make_small_button (TRUE, _("Variables"), NULL, _("Show variables needed\nto execute SQL"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	tconsole->priv->params_toggle = GTK_TOGGLE_BUTTON (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (sql_variables_clicked_cb), tconsole);

	button = make_small_button (FALSE, _("Execute"), GTK_STOCK_EXECUTE, _("Execute SQL in editor"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (sql_execute_clicked_cb), tconsole);
	
	button = make_small_button (FALSE, _("Indent"), GTK_STOCK_INDENT, _("Indent SQL in editor"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (sql_indent_clicked_cb), tconsole);

	/* bottom paned for the results and history */
	hpaned = gtk_hpaned_new ();
	gtk_paned_add2 (GTK_PANED (vpaned), hpaned);

	/* bottom left */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, FALSE, TRUE);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Execution history:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = query_editor_new ();
	tconsole->priv->history = QUERY_EDITOR (wid);
	query_editor_set_mode (tconsole->priv->history, QUERY_EDITOR_HISTORY);
	gtk_widget_set_size_request (wid, 200, -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);

	bbox = gtk_vbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

	button = make_small_button (FALSE, _("Delete"), GTK_STOCK_DELETE, _("Delete history item"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

	/* bottom right */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);
	
	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Execution Results:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = gtk_label_new ("Here go the\nresults' form");
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);

	/* show everything */
        gtk_widget_show_all (vpaned);
	gtk_widget_hide (tconsole->priv->params_form);

	return (GtkWidget*) tconsole;
}

static GtkWidget *
make_small_button (gboolean is_toggle, const gchar *label, const gchar *stock_id, const gchar *tooltip)
{
	GtkWidget *button, *hbox = NULL;

	if (is_toggle)
		button = gtk_toggle_button_new ();
	else
		button = gtk_button_new ();
	if (label && stock_id) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (button), hbox);
		gtk_widget_show (hbox);
	}

	if (stock_id) {
		GtkWidget *image;
		image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
		if (hbox)
			gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		else
			gtk_container_add (GTK_CONTAINER (button), image);
		gtk_widget_show (image);
	}
	if (label) {
		GtkWidget *wid;
		wid = gtk_label_new (label);
		if (hbox)
			gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 5);
		else
			gtk_container_add (GTK_CONTAINER (button), wid);
		gtk_widget_show (wid);
	}

	if (tooltip)
		gtk_widget_set_tooltip_text (button, tooltip);
	return button;
}

static void
sql_variables_clicked_cb (GtkToggleButton *button, QueryConsole *tconsole)
{
	if (gtk_toggle_button_get_active (button))
		gtk_widget_show (tconsole->priv->params_form);
	else
		gtk_widget_hide (tconsole->priv->params_form);
}

static void
sql_clear_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	query_editor_set_text (tconsole->priv->editor, NULL);
}

static void
sql_indent_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	gchar *sql;
	GdaBatch *batch;

	if (!tconsole->priv->parser)
		tconsole->priv->parser = browser_connection_create_parser (tconsole->priv->bcnc);

	sql = query_editor_get_all_text (tconsole->priv->editor);
	batch = gda_sql_parser_parse_string_as_batch (tconsole->priv->parser, sql, NULL, NULL);
	g_free (sql);
	if (batch) {
		GString *string;
		const GSList *stmt_list, *list;
		stmt_list = gda_batch_get_statements (batch);
		string = g_string_new ("");
		for (list = stmt_list; list; list = list->next) {
			GError *error = NULL;
			sql = gda_statement_to_sql_extended (GDA_STATEMENT (list->data), NULL, NULL,
							     GDA_STATEMENT_SQL_PRETTY |
							     GDA_STATEMENT_SQL_PARAMS_SHORT,
							     NULL, &error);
			if (!sql)
				sql = gda_statement_to_sql (GDA_STATEMENT (list->data), NULL, NULL);
			if (list != stmt_list)
				g_string_append (string, "\n\n");
			g_string_append_printf (string, "%s;\n", sql);
			g_free (sql);

		}
		g_object_unref (batch);

		query_editor_set_text (tconsole->priv->editor, string->str);
		g_string_free (string, TRUE);
	}
}

static void
sql_execute_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	gchar *sql;
	const gchar *remain;
	GdaBatch *batch;
	GError *error = NULL;

	if (!tconsole->priv->parser)
		tconsole->priv->parser = browser_connection_create_parser (tconsole->priv->bcnc);

	sql = query_editor_get_all_text (tconsole->priv->editor);
	batch = gda_sql_parser_parse_string_as_batch (tconsole->priv->parser, sql, &remain, &error);
	if (!batch) {
		browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
				    _("Error while parsing code: %s"),
				    error && error->message ? error->message : _("No detail"));
		g_clear_error (&error);
		g_free (sql);
		return;
	}
	g_free (sql);

	QueryEditorHistoryBatch *hbatch;
	GTimeVal tv;
	g_get_current_time (&tv);
	hbatch = query_editor_history_batch_new (tv);
	query_editor_start_history_batch (tconsole->priv->history, hbatch);
	query_editor_history_batch_unref (hbatch);

	const GSList *stmt_list, *list;
	stmt_list = gda_batch_get_statements (batch);
	for (list = stmt_list; list; list = list->next) {
		QueryEditorHistoryItem *history;
		GdaSqlStatement *sqlst;
		g_object_get (G_OBJECT (list->data), "structure", &sqlst, NULL);
		if (!sqlst->sql) {
			sql = gda_statement_to_sql (GDA_STATEMENT (list->data), NULL, NULL);
			history = query_editor_history_item_new (sql, NULL, NULL);
			g_free (sql);
		}
		else
			history = query_editor_history_item_new (sqlst->sql, NULL, NULL);
		gda_sql_statement_free (sqlst);

		query_editor_add_history_item (tconsole->priv->history, history);
		query_editor_history_item_unref (history);
	}
	g_object_unref (batch);
}


/*
 * UI actions
 */
static void
query_execute_cb (GtkAction *action, QueryConsole *tconsole)
{
	sql_execute_clicked_cb (NULL, tconsole);
}

#ifdef HAVE_GTKSOURCEVIEW
static void
editor_undo_cb (GtkAction *action, QueryConsole *tconsole)
{
	TO_IMPLEMENT;
}
#endif

static GtkActionEntry ui_actions[] = {
	{ "ExecuteQuery", GTK_STOCK_EXECUTE, N_("_Execute"), NULL, N_("Execute query"),
	  G_CALLBACK (query_execute_cb)},
#ifdef HAVE_GTKSOURCEVIEW
	{ "EditorUndo", GTK_STOCK_UNDO, N_("_Undo"), NULL, N_("Undo last change"),
	  G_CALLBACK (editor_undo_cb)},
#endif
};
static const gchar *ui_actions_console =
	"<ui>"
	"  <menubar name='MenuBar'>"
	"      <menu name='Edit' action='Edit'>"
        "        <menuitem name='EditorUndo' action= 'EditorUndo'/>"
        "      </menu>"
	"  </menubar>"
	"  <toolbar name='ToolBar'>"
	"    <separator/>"
	"    <toolitem action='ExecuteQuery'/>"
	"  </toolbar>"
	"</ui>";

static GtkActionGroup *
query_console_page_get_actions_group (BrowserPage *page)
{
	GtkActionGroup *agroup;
	agroup = gtk_action_group_new ("QueryExecConsoleActions");
	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), page);
	
	return agroup;
}

static const gchar *
query_console_page_get_actions_ui (BrowserPage *page)
{
	return ui_actions_console;
}

static GtkWidget *
query_console_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	QueryConsole *tconsole;
	const gchar *tab_name;

	tconsole = QUERY_CONSOLE (page);
	tab_name = _("Query editor");
	return browser_make_tab_label_with_stock (tab_name,
						  STOCK_CONSOLE,
						  out_close_button ? TRUE : FALSE, out_close_button);
}
