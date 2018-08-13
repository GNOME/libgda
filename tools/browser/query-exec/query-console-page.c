/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "query-console-page.h"
#include "../dnd.h"
#include "../ui-support.h"
#include "../ui-customize.h"
#include "../gdaui-bar.h"
#include "query-exec-perspective.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "query-editor.h"
#include "query-result.h"
#include <libgda-ui/internal/popup-container.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda/gda-debug-macros.h>

struct _QueryConsolePagePrivate {
	TConnection *tcnc;
	GdaSqlParser *parser;

	GdauiBar *header;
	GtkWidget *vpaned; /* top=>query editor, bottom=>results */

	QueryEditor *editor;
	GtkWidget   *exec_button;
	GtkWidget   *indent_button;
	guint params_compute_id; /* timout ID to compute params */
	GdaSet *params; /* execution params */
	GtkWidget *params_popup; /* popup shown when invalid params are required */

	GtkToggleToolButton *params_toggle;
	GtkWidget *params_top;
	GtkWidget *params_form_box;
	GtkWidget *params_form;
	
	QueryEditor *history;
	GtkWidget *history_del_button;
	GtkWidget *history_copy_button;

	GtkWidget *query_result;

	gboolean currently_executing;

	gint fav_id;
	GtkWidget *favorites_menu;
};

static void query_console_page_class_init (QueryConsolePageClass *klass);
static void query_console_page_init       (QueryConsolePage *tconsole, QueryConsolePageClass *klass);
static void query_console_page_dispose   (GObject *object);
static void query_console_page_show_all (GtkWidget *widget);
static void query_console_page_grab_focus (GtkWidget *widget);

/* BrowserPage interface */
static void                 query_console_page_page_init (BrowserPageIface *iface);
static void                 query_console_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header);
static GtkWidget           *query_console_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static GObjectClass *parent_class = NULL;

/*
* QueryConsolePage class implementation
*/

static void
query_console_page_class_init (QueryConsolePageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = query_console_page_dispose;
	GTK_WIDGET_CLASS (klass)->show_all = query_console_page_show_all;
	GTK_WIDGET_CLASS (klass)->grab_focus = query_console_page_grab_focus;
}

static void
query_console_page_show_all (GtkWidget *widget)
{
	QueryConsolePage *tconsole = (QueryConsolePage *) widget;
	GTK_WIDGET_CLASS (parent_class)->show_all (widget);

	if (gtk_toggle_tool_button_get_active (tconsole->priv->params_toggle))
		gtk_widget_show (tconsole->priv->params_top);
	else
		gtk_widget_hide (tconsole->priv->params_top);
}

static void
query_console_page_page_init (BrowserPageIface *iface)
{
	iface->i_customize = query_console_customize;
	iface->i_uncustomize = NULL;
	iface->i_get_tab_label = query_console_page_get_tab_label;
}

static void
query_console_page_init (QueryConsolePage *tconsole, G_GNUC_UNUSED QueryConsolePageClass *klass)
{
	tconsole->priv = g_new0 (QueryConsolePagePrivate, 1);
	tconsole->priv->parser = NULL;
	tconsole->priv->params_compute_id = 0;
	tconsole->priv->params = NULL;
	tconsole->priv->params_popup = NULL;
	tconsole->priv->fav_id = -1;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (tconsole), GTK_ORIENTATION_VERTICAL);
}

static void connection_status_changed_cb (TConnection *tcnc, GdaConnectionStatus status, QueryConsolePage *tconsole);
static void
query_console_page_dispose (GObject *object)
{
	QueryConsolePage *tconsole = (QueryConsolePage *) object;

	/* free memory */
	if (tconsole->priv) {
		if (tconsole->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (tconsole->priv->tcnc,
							      G_CALLBACK (connection_status_changed_cb), tconsole);
			g_object_unref (tconsole->priv->tcnc);
		}
		if (tconsole->priv->parser)
			g_object_unref (tconsole->priv->parser);
		if (tconsole->priv->params)
			g_object_unref (tconsole->priv->params);
		if (tconsole->priv->params_compute_id)
			g_source_remove (tconsole->priv->params_compute_id);
		if (tconsole->priv->params_popup)
			gtk_widget_destroy (tconsole->priv->params_popup);
		if (tconsole->priv->favorites_menu)
			gtk_widget_destroy (tconsole->priv->favorites_menu);

		g_free (tconsole->priv);
		tconsole->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
query_console_page_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo console = {
			sizeof (QueryConsolePageClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_console_page_class_init,
			NULL,
			NULL,
			sizeof (QueryConsolePage),
			0,
			(GInstanceInitFunc) query_console_page_init,
			0
		};

		static GInterfaceInfo page_console = {
                        (GInterfaceInitFunc) query_console_page_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "QueryConsolePage", &console, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_console);
	}
	return type;
}

static void editor_changed_cb (QueryEditor *editor, QueryConsolePage *tconsole);
static void editor_execute_request_cb (QueryEditor *editor, QueryConsolePage *tconsole);
static void sql_clear_clicked_cb (GtkToolButton *button, QueryConsolePage *tconsole);
static void sql_variables_clicked_cb (GtkToggleToolButton *button, QueryConsolePage *tconsole);
static void sql_execute_clicked_cb (GtkToolButton *button, QueryConsolePage *tconsole);
static void sql_indent_clicked_cb (GtkToolButton *button, QueryConsolePage *tconsole);
static void sql_favorite_clicked_cb (GtkToolButton *button, QueryConsolePage *tconsole);

static void history_copy_clicked_cb (GtkToolButton *button, QueryConsolePage *tconsole);
static void history_clear_clicked_cb (GtkToolButton *button, QueryConsolePage *tconsole);
static void history_changed_cb (QueryEditor *history, QueryConsolePage *tconsole);

static void rerun_requested_cb (QueryResult *qres, QueryEditorHistoryBatch *batch,
				QueryEditorHistoryItem *item, QueryConsolePage *tconsole);
/**
 * query_console_page_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
query_console_page_new (TConnection *tcnc)
{
	QueryConsolePage *tconsole;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	tconsole = QUERY_CONSOLE_PAGE (g_object_new (QUERY_CONSOLE_PAGE_TYPE, NULL));

	tconsole->priv->tcnc = g_object_ref (tcnc);
	
	/* header */
        GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>", _("Query editor"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (tconsole), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
	tconsole->priv->header = GDAUI_BAR (label);

	/* main contents */
	GtkWidget *vpaned;
	vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_style_context_add_class (gtk_widget_get_style_context (vpaned), "paned-no-border");
	tconsole->priv->vpaned = NULL;
	gtk_box_pack_start (GTK_BOX (tconsole), vpaned, TRUE, TRUE, 0);	

	/* top paned for the editor */
	GtkWidget *wid, *vbox, *hpaned;

	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_style_context_add_class (gtk_widget_get_style_context (hpaned), "paned-no-border");

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("SQL code to execute:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_widget_set_tooltip_markup (wid, QUERY_EDITOR_TOOLTIP);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = query_editor_new ();
	tconsole->priv->editor = QUERY_EDITOR (wid);
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);
	g_signal_connect (wid, "changed",
			  G_CALLBACK (editor_changed_cb), tconsole);
	g_signal_connect (wid, "execute-request",
			  G_CALLBACK (editor_execute_request_cb), tconsole);
	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	tconsole->priv->params_top = vbox;
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, FALSE, FALSE);
	
	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Variables' values:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);
	
	GtkWidget *sw;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	tconsole->priv->params_form_box = gtk_viewport_new (NULL, NULL);
	gtk_widget_set_name (tconsole->priv->params_form_box, "gdaui-transparent-background");
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (tconsole->priv->params_form_box), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), tconsole->priv->params_form_box);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 250, -1);

	wid = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (wid), VARIABLES_HELP);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_container_add (GTK_CONTAINER (tconsole->priv->params_form_box), wid);
	tconsole->priv->params_form = wid;

	GtkWidget *packed;
	GtkWidget *toolbar;
	packed = ui_widget_add_toolbar (hpaned, &toolbar);
	gtk_paned_pack1 (GTK_PANED (vpaned), packed, TRUE, FALSE);

	GtkToolItem *titem;
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "edit-clear-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Clear the editor's\ncontents"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	g_signal_connect (titem, "clicked",
			  G_CALLBACK (sql_clear_clicked_cb), tconsole);

	titem = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "insert-object-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Show variables needed\nto execute SQL"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	tconsole->priv->params_toggle = GTK_TOGGLE_TOOL_BUTTON (titem);
	g_signal_connect (titem, "toggled",
			  G_CALLBACK (sql_variables_clicked_cb), tconsole);

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "system-run-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Execute SQL in editor"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	tconsole->priv->exec_button = GTK_WIDGET (titem);
	g_signal_connect (titem, "clicked",
			  G_CALLBACK (sql_execute_clicked_cb), tconsole);

	
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "format-indent-more-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Indent SQL in editor\n"
							   "and make the code more readable\n"
							   "(removes comments)"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	tconsole->priv->indent_button = GTK_WIDGET (titem);
	g_signal_connect (titem, "clicked",
			  G_CALLBACK (sql_indent_clicked_cb), tconsole);

	
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "starred-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Add SQL to favorite"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	g_signal_connect (titem, "clicked",
			  G_CALLBACK (sql_favorite_clicked_cb), tconsole);

	/* bottom paned for the results and history */
	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_style_context_add_class (gtk_widget_get_style_context (hpaned), "paned-no-border");
	gtk_paned_pack2 (GTK_PANED (vpaned), hpaned, TRUE, FALSE);

	/* bottom left */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Execution history:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = query_editor_new ();
	tconsole->priv->history = QUERY_EDITOR (wid);
	query_editor_set_mode (tconsole->priv->history, QUERY_EDITOR_HISTORY);
	gtk_widget_set_size_request (wid, 200, -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 6);
	g_signal_connect (wid, "changed",
			  G_CALLBACK (history_changed_cb), tconsole);

	packed = ui_widget_add_toolbar (vbox, &toolbar);
	gtk_paned_pack1 (GTK_PANED (hpaned), packed, FALSE, FALSE);

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "edit-copy-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Copy selected history\nto editor"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	tconsole->priv->history_copy_button = GTK_WIDGET (titem);
	gtk_widget_set_sensitive (GTK_WIDGET (titem), FALSE);
	g_signal_connect (titem, "clicked",
			  G_CALLBACK (history_copy_clicked_cb), tconsole);

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "edit-clear-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Clear history"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	tconsole->priv->history_del_button = GTK_WIDGET (titem);
	gtk_widget_set_sensitive (GTK_WIDGET (titem), FALSE);
	g_signal_connect (titem, "clicked",
			  G_CALLBACK (history_clear_clicked_cb), tconsole);

	/* bottom right */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Execution Results:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = query_result_new (tconsole->priv->history);
	tconsole->priv->query_result = wid;
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);
	g_signal_connect (wid, "rerun-requested",
			  G_CALLBACK (rerun_requested_cb), tconsole);

	/* show everything */
        gtk_widget_show_all (vpaned);
	gtk_widget_hide (tconsole->priv->params_top);

	/* busy connection handling */
	connection_status_changed_cb (tconsole->priv->tcnc,
				      gda_connection_get_status (t_connection_get_cnc (tconsole->priv->tcnc)),
				      tconsole);

	g_signal_connect (tconsole->priv->tcnc, "status-changed",
			  G_CALLBACK (connection_status_changed_cb), tconsole);

	return (GtkWidget*) tconsole;
}

static void
connection_status_changed_cb (G_GNUC_UNUSED TConnection *tcnc, GdaConnectionStatus status, QueryConsolePage *tconsole)
{
	gboolean is_busy;
	is_busy = (status == GDA_CONNECTION_STATUS_IDLE) ? FALSE : TRUE;

	gtk_widget_set_sensitive (tconsole->priv->exec_button, !is_busy);
	gtk_widget_set_sensitive (tconsole->priv->indent_button, !is_busy);

	GAction *action;
	action = customization_data_get_action (G_OBJECT (tconsole), "ExecuteQuery");
	if (action)
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !is_busy);
}

static void
history_changed_cb (G_GNUC_UNUSED QueryEditor *history, QueryConsolePage *tconsole)
{
	gboolean act = FALSE;
	QueryEditor *qe;
	
	qe = tconsole->priv->history;
        if (query_editor_get_current_history_item (qe, NULL)) {
		query_result_show_history_item (QUERY_RESULT (tconsole->priv->query_result),
						query_editor_get_current_history_item (qe, NULL));
		act = TRUE;
	}
	else if (query_editor_get_current_history_batch (qe)) {
		query_result_show_history_batch (QUERY_RESULT (tconsole->priv->query_result),
						 query_editor_get_current_history_batch (qe));
		act = TRUE;
	}
	else
		query_result_show_history_batch (QUERY_RESULT (tconsole->priv->query_result), NULL);

	gtk_widget_set_sensitive (tconsole->priv->history_copy_button, act);
	gtk_widget_set_sensitive (tconsole->priv->history_del_button, ! query_editor_history_is_empty (qe));
}

static void
history_clear_clicked_cb (G_GNUC_UNUSED GtkToolButton *button, QueryConsolePage *tconsole)
{
	query_editor_del_all_history_items (tconsole->priv->history);
}

static void
history_copy_clicked_cb (G_GNUC_UNUSED GtkToolButton *button, QueryConsolePage *tconsole)
{
	QueryEditorHistoryItem *qih;
	QueryEditor *qe;
	GString *string;

	string = g_string_new ("");
	qe = tconsole->priv->history;
        qih = query_editor_get_current_history_item (qe, NULL);
        if (qih)
		g_string_append (string, qih->sql);
        else {
                QueryEditorHistoryBatch *qib;
                qib = query_editor_get_current_history_batch (qe);
                if (qib) {
			GSList *list;
			for (list =  qib->hist_items; list; list = list->next) {
				if (list != qib->hist_items)
					g_string_append (string, "\n\n");
				g_string_append (string, ((QueryEditorHistoryItem*) list->data)->sql);
			}
		}
        }

	query_editor_set_text (tconsole->priv->editor, string->str);
	tconsole->priv->fav_id = -1;
	g_string_free (string, TRUE);
}

static void
params_form_activated_cb (GdauiBasicForm *form, QueryConsolePage *tconsole)
{
	sql_execute_clicked_cb (NULL, tconsole);
}

static gboolean
compute_params (QueryConsolePage *tconsole)
{
	gchar *sql;
	GdaBatch *batch;

	if (tconsole->priv->params) {
		t_connection_keep_variables (tconsole->priv->tcnc, tconsole->priv->params);
		g_object_unref (tconsole->priv->params);
	}
	tconsole->priv->params = NULL;

	if (tconsole->priv->params_form) {
		gtk_widget_destroy (tconsole->priv->params_form);
		tconsole->priv->params_form = NULL;		
	}

	if (!tconsole->priv->parser)
		tconsole->priv->parser = t_connection_create_parser (tconsole->priv->tcnc);

	sql = query_editor_get_all_text (tconsole->priv->editor);
	batch = gda_sql_parser_parse_string_as_batch (tconsole->priv->parser, sql, NULL, NULL);
	g_free (sql);
	if (batch) {
		GError *error = NULL;
		gboolean show_variables = FALSE;

		if (gda_batch_get_parameters (batch, &(tconsole->priv->params), &error)) {
			if (tconsole->priv->params) {
				t_connection_define_ui_plugins_for_batch (tconsole->priv->tcnc,
									  batch,
									  tconsole->priv->params);
				show_variables = TRUE;
				tconsole->priv->params_form = gdaui_basic_form_new (tconsole->priv->params);
				g_signal_connect (tconsole->priv->params_form, "activated",
						  G_CALLBACK (params_form_activated_cb), tconsole);
			}
			else {
				tconsole->priv->params_form = gtk_label_new ("");
				gtk_label_set_markup (GTK_LABEL (tconsole->priv->params_form), VARIABLES_HELP);
			}
		}
		else {
			show_variables = TRUE;
			tconsole->priv->params_form = gtk_label_new ("");
			gtk_label_set_markup (GTK_LABEL (tconsole->priv->params_form), VARIABLES_HELP);
		}
		gtk_container_add (GTK_CONTAINER (tconsole->priv->params_form_box), tconsole->priv->params_form);
		gtk_widget_show (tconsole->priv->params_form);
		g_object_unref (batch);

		t_connection_load_variables (tconsole->priv->tcnc, tconsole->priv->params);
		if (tconsole->priv->params && show_variables &&
		    gda_set_is_valid (tconsole->priv->params, NULL))
			show_variables = FALSE;
		if (show_variables && !gtk_toggle_tool_button_get_active (tconsole->priv->params_toggle))
			gtk_toggle_tool_button_set_active (tconsole->priv->params_toggle, TRUE);
	}
	else {
		tconsole->priv->params_form = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (tconsole->priv->params_form), VARIABLES_HELP);
		gtk_container_add (GTK_CONTAINER (tconsole->priv->params_form_box), tconsole->priv->params_form);
		gtk_widget_show (tconsole->priv->params_form);
	}
	
	/* remove timeout */
	tconsole->priv->params_compute_id = 0;
	return FALSE;
}

static void
editor_changed_cb (G_GNUC_UNUSED QueryEditor *editor, QueryConsolePage *tconsole)
{
	if (tconsole->priv->params_compute_id)
		g_source_remove (tconsole->priv->params_compute_id);
	tconsole->priv->params_compute_id = g_timeout_add_seconds (1, (GSourceFunc) compute_params, tconsole);
}

static void
editor_execute_request_cb (G_GNUC_UNUSED QueryEditor *editor, QueryConsolePage *tconsole)
{
	gboolean sensitive;
	g_object_get (tconsole->priv->exec_button, "sensitive", &sensitive, NULL);
	if (sensitive)
		sql_execute_clicked_cb (NULL, tconsole);
}
	
static void
sql_variables_clicked_cb (GtkToggleToolButton *button, QueryConsolePage *tconsole)
{
	if (gtk_toggle_tool_button_get_active (button))
		gtk_widget_show (tconsole->priv->params_top);
	else
		gtk_widget_hide (tconsole->priv->params_top);
}

static void
sql_clear_clicked_cb (G_GNUC_UNUSED GtkToolButton *button, QueryConsolePage *tconsole)
{
	query_editor_set_text (tconsole->priv->editor, NULL);
	tconsole->priv->fav_id = -1;
	gtk_widget_grab_focus (GTK_WIDGET (tconsole->priv->editor));
}

static void
sql_indent_clicked_cb (G_GNUC_UNUSED GtkToolButton *button, QueryConsolePage *tconsole)
{
	gchar *sql;
	GdaBatch *batch;

	if (!tconsole->priv->parser)
		tconsole->priv->parser = t_connection_create_parser (tconsole->priv->tcnc);

	sql = query_editor_get_all_text (tconsole->priv->editor);
	batch = gda_sql_parser_parse_string_as_batch (tconsole->priv->parser, sql, NULL, NULL);
	g_free (sql);
	if (batch) {
		GString *string;
		const GSList *stmt_list, *list;
		stmt_list = gda_batch_get_statements (batch);
		string = g_string_new ("");
		for (list = stmt_list; list; list = list->next) {
			sql = t_connection_render_pretty_sql (tconsole->priv->tcnc,
							      GDA_STATEMENT (list->data));
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

static void sql_favorite_new_mitem_cb (G_GNUC_UNUSED GtkMenuItem *mitem, QueryConsolePage *tconsole);
static void sql_favorite_modify_mitem_cb (G_GNUC_UNUSED GtkMenuItem *mitem, QueryConsolePage *tconsole);

static void
sql_favorite_clicked_cb (G_GNUC_UNUSED GtkToolButton *button, QueryConsolePage *tconsole)
{
	GtkWidget *menu, *mitem;
	TFavorites *tfav;

	if (tconsole->priv->favorites_menu)
		gtk_widget_destroy (tconsole->priv->favorites_menu);

	menu = gtk_menu_new ();
	tconsole->priv->favorites_menu = menu;
	mitem = gtk_menu_item_new_with_label (_("New favorite"));
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (sql_favorite_new_mitem_cb), tconsole);
	gtk_widget_show (mitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

	tfav = t_connection_get_favorites (tconsole->priv->tcnc);
	if (tconsole->priv->fav_id >= 0) {
		TFavoritesAttributes fav;
		if (t_favorites_get (tfav, tconsole->priv->fav_id, &fav, NULL)) {
			gchar *str;
			str = g_strdup_printf (_("Modify favorite '%s'"), fav.name);
			mitem = gtk_menu_item_new_with_label (str);
			g_free (str);
			g_signal_connect (mitem, "activate",
					  G_CALLBACK (sql_favorite_modify_mitem_cb), tconsole);
			g_object_set_data_full (G_OBJECT (mitem), "favname",
						g_strdup (fav.name), g_free);
			g_object_set_data (G_OBJECT (mitem), "favid",
					   GINT_TO_POINTER (tconsole->priv->fav_id));
			gtk_widget_show (mitem);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
			t_favorites_reset_attributes (&fav);
		}
	}

	GSList *allfav;
	allfav = t_favorites_list (tfav, 0, T_FAVORITES_QUERIES, ORDER_KEY_QUERIES, NULL);
	if (allfav && allfav->next) {
		GtkWidget *submenu;
		GSList *list;

		mitem = gtk_menu_item_new_with_label (_("Modify a favorite"));
		gtk_widget_show (mitem);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

		submenu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), submenu);
		for (list = allfav; list; list = list->next) {
			TFavoritesAttributes *fav;
			fav = (TFavoritesAttributes*) list->data;
			if (fav->id == tconsole->priv->fav_id)
				continue;
			gchar *str;
			str = g_strdup_printf (_("Modify favorite '%s'"), fav->name);
			mitem = gtk_menu_item_new_with_label (str);
			g_free (str);
			g_signal_connect (mitem, "activate",
					  G_CALLBACK (sql_favorite_modify_mitem_cb), tconsole);
			g_object_set_data_full (G_OBJECT (mitem), "favname",
						g_strdup (fav->name), g_free);
			g_object_set_data (G_OBJECT (mitem), "favid",
					   GINT_TO_POINTER (fav->id));
			gtk_widget_show (mitem);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mitem);
		}
		t_favorites_free_list (allfav);
	}

	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
fav_form_name_activated_cb (G_GNUC_UNUSED GtkWidget *form, GtkWidget *dlg)
{
	gtk_dialog_response (GTK_DIALOG (dlg), GTK_RESPONSE_ACCEPT);
}

static void
sql_favorite_new_mitem_cb (G_GNUC_UNUSED GtkMenuItem *mitem, QueryConsolePage *tconsole)
{
	TFavorites *tfav;
	TFavoritesAttributes fav;
	GError *error = NULL;

	GdaSet *set;
	GtkWidget *dlg, *form;
	gint response;
	const GValue *cvalue;
	set = gda_set_new_inline (1, _("Favorite's name"),
				  G_TYPE_STRING, _("Unnamed query"));
	dlg = gdaui_basic_form_new_in_dialog (set,
					      (GtkWindow*) gtk_widget_get_toplevel (GTK_WIDGET (tconsole)),
					      _("Name of the favorite to create"),
					      _("Enter the name of the favorite to create"));
	form = g_object_get_data (G_OBJECT (dlg), "form");
	g_signal_connect (form, "activated",
			  G_CALLBACK (fav_form_name_activated_cb), dlg);
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_REJECT) {
		g_object_unref (set);
		gtk_widget_destroy (dlg);
		return;
	}

	memset (&fav, 0, sizeof (fav));
	fav.id = -1;
	fav.type = T_FAVORITES_QUERIES;
	fav.contents = query_editor_get_all_text (tconsole->priv->editor);
	cvalue = gda_set_get_holder_value (set, _("Favorite's name"));
	fav.name = (gchar*) g_value_get_string (cvalue);

	tfav = t_connection_get_favorites (tconsole->priv->tcnc);

	if (! t_favorites_add (tfav, 0, &fav, ORDER_KEY_QUERIES, G_MAXINT, &error)) {
		ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tconsole),
			       _("Could not add favorite: %s"),
			       error && error->message ? error->message : _("No detail"));
                if (error)
                        g_error_free (error);
	}
	else
		tconsole->priv->fav_id = fav.id;
	g_free (fav.contents);

	g_object_unref (set);
	gtk_widget_destroy (dlg);
}

static void
sql_favorite_modify_mitem_cb (G_GNUC_UNUSED GtkMenuItem *mitem, QueryConsolePage *tconsole)
{
	TFavorites *tfav;
	TFavoritesAttributes fav;
	GError *error = NULL;

	memset (&fav, 0, sizeof (fav));
	fav.id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (mitem), "favid"));
	fav.type = T_FAVORITES_QUERIES;
	fav.contents = query_editor_get_all_text (tconsole->priv->editor);
	fav.name = g_object_get_data (G_OBJECT (mitem), "favname");

	tfav = t_connection_get_favorites (tconsole->priv->tcnc);

	if (! t_favorites_add (tfav, 0, &fav, ORDER_KEY_QUERIES, G_MAXINT, &error)) {
		ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tconsole),
			       _("Could not add favorite: %s"),
			       error && error->message ? error->message : _("No detail"));
                if (error)
                        g_error_free (error);
	}
	g_free (fav.contents);
}

static void
popup_container_position_func (PopupContainer *cont, gint *out_x, gint *out_y)
{
	GtkWidget *console, *top;
	gint x, y;
        GtkRequisition req;

	console = g_object_get_data (G_OBJECT (cont), "console");
	top = gtk_widget_get_toplevel (console);	
	gtk_widget_get_preferred_size ((GtkWidget*) cont, NULL, &req);

	GtkAllocation alloc;
        gdk_window_get_origin (gtk_widget_get_window (top), &x, &y);
	gtk_widget_get_allocation (top, &alloc);
	x += (alloc.width - req.width) / 2;
	y += (alloc.height - req.height) / 2;

        if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;

	*out_x = x;
	*out_y = y;
}

static void
params_form_changed_cb (GdauiBasicForm *form, G_GNUC_UNUSED GdaHolder *param,
			G_GNUC_UNUSED gboolean is_user_modif, QueryConsolePage *tconsole)
{
	/* if all params are valid => authorize the execute button */
	GtkWidget *button;

	button = g_object_get_data (G_OBJECT (tconsole->priv->params_popup), "exec");
	gtk_widget_set_sensitive (button,
				  gdaui_basic_form_is_valid (form));
}

static void actually_execute (QueryConsolePage *tconsole, const gchar *sql, GdaSet *params,
			      gboolean add_editor_history);
static void
sql_execute_clicked_cb (G_GNUC_UNUSED GtkToolButton *button, QueryConsolePage *tconsole)
{
	gchar *sql;

	if (tconsole->priv->params_popup)
		gtk_widget_hide (tconsole->priv->params_popup);

	/* compute parameters if necessary */
	if (tconsole->priv->params_compute_id > 0) {
		g_source_remove (tconsole->priv->params_compute_id);
		tconsole->priv->params_compute_id = 0;
		compute_params (tconsole);
	}

	if (tconsole->priv->params) {
		if (! gdaui_basic_form_is_valid (GDAUI_BASIC_FORM (tconsole->priv->params_form))) {
			GtkWidget *form, *cont;
			if (! tconsole->priv->params_popup) {
				tconsole->priv->params_popup = popup_container_new_with_func (popup_container_position_func);
				g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "console", tconsole);

				GtkWidget *vbox, *label, *bbox, *button;
				gchar *str;
				vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				gtk_container_add (GTK_CONTAINER (tconsole->priv->params_popup), vbox);
				gtk_container_set_border_width (GTK_CONTAINER (tconsole->priv->params_popup), 10);

				label = gtk_label_new ("");
				str = g_strdup_printf ("<b>%s</b>:\n<small>%s</small>",
						       _("Invalid variable's contents"),
						       _("assign values to the following variables"));
				gtk_label_set_markup (GTK_LABEL (label), str);
				g_free (str);
				gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

				cont = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				gtk_box_pack_start (GTK_BOX (vbox), cont, FALSE, FALSE, 10);
				g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "cont", cont);

				bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
				gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 10);
				gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
				
				button = gtk_button_new_from_icon_name ("system-run-symbolic", GTK_ICON_SIZE_BUTTON);
				gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);
				g_signal_connect_swapped (button, "clicked",
							  G_CALLBACK (gtk_widget_hide), tconsole->priv->params_popup);
				g_signal_connect (button, "clicked",
						  G_CALLBACK (sql_execute_clicked_cb), tconsole);
				gtk_widget_set_sensitive (button, FALSE);
				g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "exec", button);

				button = gtk_button_new_with_mnemonic (_("_Cancel"));
				gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);
				g_signal_connect_swapped (button, "clicked",
							  G_CALLBACK (gtk_widget_hide), tconsole->priv->params_popup);
			}
			else {
				form = g_object_get_data (G_OBJECT (tconsole->priv->params_popup), "form");
				if (form)
					gtk_widget_destroy (form);
			}

			cont = g_object_get_data (G_OBJECT (tconsole->priv->params_popup), "cont");
			form = gdaui_basic_form_new (tconsole->priv->params);
			g_signal_connect (form, "holder-changed",
					  G_CALLBACK (params_form_changed_cb), tconsole);
			g_signal_connect (form, "activated",
					  G_CALLBACK (sql_execute_clicked_cb), tconsole);

			gtk_box_pack_start (GTK_BOX (cont), form, TRUE, TRUE, 0);
			g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "form", form);
			gtk_widget_show_all (tconsole->priv->params_popup);

			return;
		}
	}

	sql = query_editor_get_all_text (tconsole->priv->editor);
	actually_execute (tconsole, sql, tconsole->priv->params, TRUE);
	g_free (sql);
}

static void
actually_execute (QueryConsolePage *tconsole, const gchar *sql, GdaSet *params,
		  gboolean add_editor_history)
{
	GdaBatch *batch;
	GError *error = NULL;
	const gchar *remain;

	/* if a query is being executed, then show an error */
	if (tconsole->priv->currently_executing) {
		ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
			       _("A query is already being executed, "
				 "to execute another query, open a new connection."));
		return;
	}
	tconsole->priv->currently_executing = TRUE;

	if (!tconsole->priv->parser)
		tconsole->priv->parser = t_connection_create_parser (tconsole->priv->tcnc);

	batch = gda_sql_parser_parse_string_as_batch (tconsole->priv->parser, sql, &remain, &error);
	if (!batch) {
		ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
			       _("Error while parsing code: %s"),
			       error && error->message ? error->message : _("No detail"));
		g_clear_error (&error);
		return;
	}

	if (add_editor_history) {
		/* mark the current SQL to be kept by the editor as an internal history */
		query_editor_keep_current_state (tconsole->priv->editor);
	}

	/* actual Execution */
	const GSList *stmt_list, *list;
	GTimeVal start_time;
	g_get_current_time (&start_time);

	QueryEditorHistoryBatch *hist_batch;
	hist_batch = query_editor_history_batch_new (start_time, params);
	query_editor_start_history_batch (tconsole->priv->history, hist_batch);
	query_editor_history_batch_unref (hist_batch);

	gboolean within_transaction;
	stmt_list = gda_batch_get_statements (batch);
	for (list = stmt_list; list; list = list->next) {
		GdaStatement *stmt;
		GObject *result;
		GError *lerror = NULL;
		within_transaction = t_connection_get_transaction_status (tconsole->priv->tcnc) ? TRUE : FALSE;
		stmt = GDA_STATEMENT (list->data);
		result = t_connection_execute_statement (tconsole->priv->tcnc, stmt, params,
							 GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							 NULL, &lerror);
		if (result) {
			QueryEditorHistoryItem *history;
			GdaSqlStatement *sqlst;
			g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
			if (!sqlst->sql) {
				gchar *sql;
				sql = gda_statement_to_sql (stmt, NULL, NULL);
				history = query_editor_history_item_new (sql, result, lerror);
				g_free (sql);
			}
			else
				history = query_editor_history_item_new (sqlst->sql, result, lerror);
			g_object_unref (result);
			gda_sql_statement_free (sqlst);
			
			history->within_transaction = within_transaction;

			/* display a message if a transaction has been started */
			if (! history->within_transaction &&
			    t_connection_get_transaction_status (tconsole->priv->tcnc) &&
			    gda_statement_get_statement_type (stmt) != GDA_SQL_STATEMENT_BEGIN) {
				browser_window_show_notice_printf (BROWSER_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
								   GTK_MESSAGE_INFO,
								   "QueryExecTransactionStarted",
								   "%s", _("A transaction has automatically been started\n"
									   "during this statement's execution, this usually\n"
									   "happens when blobs are selected (and the transaction\n"
									   "will have to remain opened while the blobs are still\n"
									   "accessible, clear the corresponding history item before\n"
									   "closing the transaction)."));
			}

			query_editor_add_history_item (tconsole->priv->history, history);
			query_editor_history_item_unref (history);

			browser_window_push_status (BROWSER_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
						    "QueryConsolePage", _("Statement executed"), TRUE);			
		}
		else {
			ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
				       _("Error executing query:\n%s"),
				       lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
			break;
		}
	}
	g_object_unref (batch);
	tconsole->priv->currently_executing = FALSE;
}

static void
rerun_requested_cb (QueryResult *qres, QueryEditorHistoryBatch *batch,
		    QueryEditorHistoryItem *item, QueryConsolePage *tconsole)
{
	if (!batch || !item || !item->sql) {
		ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
			       _("Internal error, please report error to "
				 "http://gitlab.gnome.org/GNOME/libgda/issues"));
		return;
	}

	actually_execute (tconsole, item->sql, batch->params, FALSE);
}


/**
 * query_console_page_set_text
 * @console: a #QueryConsolePage
 * @text: the new text
 * @fav_id: the favorite ID or -1 if not a favorite.
 *
 * Replaces the edited SQL with @text in @console
 */
void
query_console_page_set_text (QueryConsolePage *console, const gchar *text, gint fav_id)
{
	g_return_if_fail (IS_QUERY_CONSOLE_PAGE_PAGE (console));
	console->priv->fav_id = fav_id;
	query_editor_set_text (console->priv->editor, text);
}

/*
 * UI actions
 */
static void
query_execute_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	QueryConsolePage *tconsole;
	tconsole = QUERY_CONSOLE_PAGE (data);

	sql_execute_clicked_cb (NULL, tconsole);
}

static GActionEntry win_entries[] = {
        { "ExecuteQuery", query_execute_cb, NULL, NULL, NULL },
};

static void
query_console_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	customization_data_init (G_OBJECT (page), toolbar, header);

	/* add perspective's actions */
	customization_data_add_actions (G_OBJECT (page), win_entries, G_N_ELEMENTS (win_entries));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "system-run-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Execute query"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.ExecuteQuery");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));
}

static GtkWidget *
query_console_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	const gchar *tab_name;

	tab_name = _("Query editor");
	return ui_make_tab_label_with_icon (tab_name, NULL/*STOCK_CONSOLE*/,
					    out_close_button ? TRUE : FALSE, out_close_button);
}

static void
query_console_page_grab_focus (GtkWidget *widget)
{
	QueryConsolePage *tconsole;

	tconsole = QUERY_CONSOLE_PAGE (widget);
	gtk_widget_grab_focus (GTK_WIDGET (tconsole->priv->editor));
}
