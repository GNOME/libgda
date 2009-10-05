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
#include "../browser-window.h"
#include "../browser-page.h"
#include "../browser-stock-icons.h"
#include "query-editor.h"
#include "query-result.h"
#include "../common/popup-container.h"
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda-ui/libgda-ui.h>

#define VARIABLES_HELP _("<small>This area allows to give values to\n" \
			 "variables defined in the SQL code\n"		\
			 "using the following syntax:\n"		\
			 "<b><tt>##&lt;variable name&gt;::&lt;type&gt;[::null]</tt></b>\n" \
			 "For example:\n"				\
			 "<span foreground=\"#4e9a06\"><b><tt>##id::int</tt></b></span>\n      defines <b>id</b> as a non NULL integer\n" \
			 "<span foreground=\"#4e9a06\"><b><tt>##age::string::null</tt></b></span>\n      defines <b>age</b> as a a string\n\n" \
			 "Valid types are: <tt>string</tt>, <tt>boolean</tt>, <tt>int</tt>,\n" \
			 "<tt>date</tt>, <tt>time</tt>, <tt>timestamp</tt>, <tt>guint</tt>, <tt>blob</tt> and\n" \
			 "<tt>binary</tt></small>")

/*
 * Statement execution structures
 */
typedef struct {
	GTimeVal start_time;
	GdaBatch *batch; /* ref held here */
	QueryEditorHistoryBatch *hist_batch; /* ref held here */
	GSList *statements; /* list of ExecutionStatament */
} ExecutionBatch;
static void execution_batch_free (ExecutionBatch *ebatch);

typedef struct {
	GdaStatement *stmt /* no ref held here */;
	gboolean within_transaction;
	GError *exec_error;
	GObject *result;
	guint exec_id; /* 0 when execution not requested */
} ExecutionStatement;
static void execution_statement_free (ExecutionStatement *estmt);

/*
 * Frees an @ExecutionBatch
 *
 * If necessary, may set up and idle handler to perform some
 * additionnal jobs
 */
static void
execution_batch_free (ExecutionBatch *ebatch)
{
	GSList *list;
	for (list = ebatch->statements; list; list = list->next)
		execution_statement_free ((ExecutionStatement*) list->data);
	g_slist_free (ebatch->statements);

	g_object_unref (ebatch->batch);
	if (ebatch->hist_batch)
		query_editor_history_batch_unref (ebatch->hist_batch);

	g_free (ebatch);
}

static void
execution_statement_free (ExecutionStatement *estmt)
{
	if (estmt->exec_id > 0) {
		/* job started => set up idle task */
		TO_IMPLEMENT;
	}
	else {
		if (estmt->exec_error)
			g_error_free (estmt->exec_error);
		if (estmt->result)
			g_object_unref (estmt->result);
		g_free (estmt);
	}
}

struct _QueryConsolePrivate {
	BrowserConnection *bcnc;
	GdaSqlParser *parser;

	CcGrayBar *header;
	GtkWidget *vpaned; /* top=>query editor, bottom=>results */

	QueryEditor *editor;
	guint params_compute_id; /* timout ID to compute params */
	GdaSet *past_params; /* keeps values given to old params */
	GdaSet *params; /* execution params */
	GtkWidget *params_popup; /* popup shown when invalid params are required */

	GtkToggleButton *params_toggle;
	GtkWidget *params_top;
	GtkWidget *params_form_box;
	GtkWidget *params_form;
	
	QueryEditor *history;
	GtkWidget *history_del_button;
	GtkWidget *history_copy_button;

	GtkWidget *query_result;

	ExecutionBatch *current_exec;
	guint current_exec_id; /* timout ID to fetch execution results */
};

static void query_console_class_init (QueryConsoleClass *klass);
static void query_console_init       (QueryConsole *tconsole, QueryConsoleClass *klass);
static void query_console_dispose   (GObject *object);
static void query_console_show_all (GtkWidget *widget);
static void query_console_grab_focus (GtkWidget *widget);

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
	GTK_WIDGET_CLASS (klass)->grab_focus = query_console_grab_focus;
}

static void
query_console_show_all (GtkWidget *widget)
{
	QueryConsole *tconsole = (QueryConsole *) widget;
	GTK_WIDGET_CLASS (parent_class)->show_all (widget);

	if (gtk_toggle_button_get_active (tconsole->priv->params_toggle))
		gtk_widget_show (tconsole->priv->params_top);
	else
		gtk_widget_hide (tconsole->priv->params_top);
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
	tconsole->priv->params_compute_id = 0;
	tconsole->priv->past_params = NULL;
	tconsole->priv->params = NULL;
	tconsole->priv->params_popup = NULL;
}

static void
query_console_dispose (GObject *object)
{
	QueryConsole *tconsole = (QueryConsole *) object;

	/* free memory */
	if (tconsole->priv) {
		if (tconsole->priv->current_exec_id)
			g_source_remove (tconsole->priv->current_exec_id);
		if (tconsole->priv->current_exec)
			execution_batch_free (tconsole->priv->current_exec);
		if (tconsole->priv->bcnc)
			g_object_unref (tconsole->priv->bcnc);
		if (tconsole->priv->parser)
			g_object_unref (tconsole->priv->parser);
		if (tconsole->priv->past_params)
			g_object_unref (tconsole->priv->past_params);
		if (tconsole->priv->params)
			g_object_unref (tconsole->priv->params);
		if (tconsole->priv->params_compute_id)
			g_source_remove (tconsole->priv->params_compute_id);
		if (tconsole->priv->params_popup)
			gtk_widget_destroy (tconsole->priv->params_popup);

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

static void editor_changed_cb (QueryEditor *editor, QueryConsole *tconsole);
static void editor_execute_request_cb (QueryEditor *editor, QueryConsole *tconsole);
static void sql_clear_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void sql_variables_clicked_cb (GtkToggleButton *button, QueryConsole *tconsole);
static void sql_execute_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void sql_indent_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void sql_favorite_clicked_cb (GtkButton *button, QueryConsole *tconsole);

static void history_copy_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void history_delete_clicked_cb (GtkButton *button, QueryConsole *tconsole);
static void history_changed_cb (QueryEditor *history, QueryConsole *tconsole);
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
	gtk_box_pack_start (GTK_BOX (tconsole), vpaned, TRUE, TRUE, 6);	

	/* top paned for the editor */
	GtkWidget *wid, *vbox, *hbox, *bbox, *hpaned, *button;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_paned_add1 (GTK_PANED (vpaned), hbox);

	hpaned = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (hbox), hpaned, TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("SQL code to execute:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
	gtk_widget_set_tooltip_markup (wid, QUERY_EDITOR_TOOLTIP);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = query_editor_new ();
	tconsole->priv->editor = QUERY_EDITOR (wid);
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);
	g_signal_connect (wid, "changed",
			  G_CALLBACK (editor_changed_cb), tconsole);
	g_signal_connect (wid, "execute-request",
			  G_CALLBACK (editor_execute_request_cb), tconsole);
	gtk_widget_set_size_request (wid, -1, 200);
	
	vbox = gtk_vbox_new (FALSE, 0);
	tconsole->priv->params_top = vbox;
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, FALSE, TRUE);
	
	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Variables' values:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);
	
	GtkWidget *sw;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	tconsole->priv->params_form_box = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (tconsole->priv->params_form_box), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (sw), tconsole->priv->params_form_box);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (tconsole->priv->params_form_box, 250, -1);

	wid = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (wid), VARIABLES_HELP);
	gtk_container_add (GTK_CONTAINER (tconsole->priv->params_form_box), wid);
	tconsole->priv->params_form = wid;
	
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
	
	button = make_small_button (FALSE, _("Indent"), GTK_STOCK_INDENT, _("Indent SQL in editor\n"
									    "and make the code more readable\n"
									    "(removes comments)"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (sql_indent_clicked_cb), tconsole);

	button = make_small_button (FALSE, _("Favorite"), STOCK_ADD_BOOKMARK, _("Add SQL to favorite"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (sql_favorite_clicked_cb), tconsole);

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
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 6);
	g_signal_connect (wid, "changed",
			  G_CALLBACK (history_changed_cb), tconsole);

	bbox = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);

	button = make_small_button (FALSE, _("Copy"), GTK_STOCK_COPY, _("Copy selected history\nto editor"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (history_copy_clicked_cb), tconsole);
	tconsole->priv->history_copy_button = button;
	gtk_widget_set_sensitive (button, FALSE);

	button = make_small_button (FALSE, _("Delete"), GTK_STOCK_DELETE, _("Delete history item"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (history_delete_clicked_cb), tconsole);
	tconsole->priv->history_del_button = button;
	gtk_widget_set_sensitive (button, FALSE);

	/* bottom right */
	vbox = gtk_vbox_new (FALSE, 8);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);
	
	wid = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Execution Results:"));
	gtk_label_set_markup (GTK_LABEL (wid), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), wid, FALSE, FALSE, 0);

	wid = query_result_new (tconsole->priv->history);
	tconsole->priv->query_result = wid;
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);

	/* show everything */
        gtk_widget_show_all (vpaned);
	gtk_widget_hide (tconsole->priv->params_top);

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
history_changed_cb (QueryEditor *history, QueryConsole *tconsole)
{
	gboolean act = FALSE;
	QueryEditor *qe;
	
	qe = tconsole->priv->history;
        if (query_editor_get_current_history_item (qe)) {
		query_result_show_history_item (QUERY_RESULT (tconsole->priv->query_result),
						query_editor_get_current_history_item (qe));
		act = TRUE;
	}
	else if (query_editor_get_current_history_batch (qe)) {
		query_result_show_history_batch (QUERY_RESULT (tconsole->priv->query_result),
						 query_editor_get_current_history_batch (qe));
		act = TRUE;
	}
	else
		query_result_show_history_batch (QUERY_RESULT (tconsole->priv->query_result), NULL);

	gtk_widget_set_sensitive (tconsole->priv->history_del_button, act);
	gtk_widget_set_sensitive (tconsole->priv->history_copy_button, act);
}

static void
history_delete_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	QueryEditorHistoryItem *qih;
	QueryEditor *qe;
	
	qe = tconsole->priv->history;
        qih = query_editor_get_current_history_item (qe);
        if (qih)
                query_editor_del_current_history_item (qe);
        else {
                QueryEditorHistoryBatch *qib;
                qib = query_editor_get_current_history_batch (qe);
                if (qib)
                        query_editor_del_history_batch (qe, qib);
        }
}

static void
history_copy_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	QueryEditorHistoryItem *qih;
	QueryEditor *qe;
	GString *string;

	string = g_string_new ("");
	qe = tconsole->priv->history;
        qih = query_editor_get_current_history_item (qe);
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
	g_string_free (string, TRUE);
}

static gboolean
compute_params (QueryConsole *tconsole)
{
	gchar *sql;
	GdaBatch *batch;

	if (tconsole->priv->params) {
		if (tconsole->priv->past_params) {
			gda_set_merge_with_set (tconsole->priv->params, tconsole->priv->past_params);
			g_object_unref (tconsole->priv->past_params);
			tconsole->priv->past_params = tconsole->priv->params;
		}
		else
			tconsole->priv->past_params = tconsole->priv->params;
	}

	tconsole->priv->params = NULL;
	if (tconsole->priv->params_form) {
		gtk_widget_destroy (tconsole->priv->params_form);
		tconsole->priv->params_form = NULL;		
	}

	if (!tconsole->priv->parser)
		tconsole->priv->parser = browser_connection_create_parser (tconsole->priv->bcnc);

	sql = query_editor_get_all_text (tconsole->priv->editor);
	batch = gda_sql_parser_parse_string_as_batch (tconsole->priv->parser, sql, NULL, NULL);
	g_free (sql);
	if (batch) {
		GError *error = NULL;
		gboolean show_variables = FALSE;

		if (gda_batch_get_parameters (batch, &(tconsole->priv->params), &error)) {
			if (tconsole->priv->params) {
				show_variables = TRUE;
				tconsole->priv->params_form = gdaui_basic_form_new (tconsole->priv->params);
				gdaui_basic_form_show_entry_actions (GDAUI_BASIC_FORM (tconsole->priv->params_form),
								     TRUE);
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

		if (tconsole->priv->past_params && tconsole->priv->params) {
			/* copy the values from tconsole->priv->past_params to tconsole->priv->params */
			GSList *list;
			for (list = tconsole->priv->params->holders; list; list = list->next) {
				GdaHolder *oldh, *newh;
				newh = GDA_HOLDER (list->data);
				oldh = gda_set_get_holder (tconsole->priv->past_params, gda_holder_get_id  (newh));
				if (oldh) {
					GType otype, ntype;
					otype = gda_holder_get_g_type (oldh);
					ntype = gda_holder_get_g_type (newh);
					if (otype == ntype) {
						const GValue *ovalue;
						ovalue = gda_holder_get_value (oldh);
						gda_holder_set_value (newh, ovalue, NULL);
					}
					else if (g_value_type_transformable (otype, ntype)) {
						const GValue *ovalue;
						GValue *nvalue;
						ovalue = gda_holder_get_value (oldh);
						nvalue = gda_value_new (ntype);
						if (g_value_transform (ovalue, nvalue))
							gda_holder_take_value (newh, nvalue, NULL);
						else
							gda_value_free  (nvalue);
					}
				}
			}
		}
		if (tconsole->priv->params && show_variables &&
		    gda_set_is_valid (tconsole->priv->params, NULL))
			show_variables = FALSE;
		if (show_variables && !gtk_toggle_button_get_active (tconsole->priv->params_toggle))
			gtk_toggle_button_set_active (tconsole->priv->params_toggle, TRUE);
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
editor_changed_cb (QueryEditor *editor, QueryConsole *tconsole)
{
	if (tconsole->priv->params_compute_id)
		g_source_remove (tconsole->priv->params_compute_id);
	tconsole->priv->params_compute_id = g_timeout_add_seconds (1, (GSourceFunc) compute_params, tconsole);
}

static void
editor_execute_request_cb (QueryEditor *editor, QueryConsole *tconsole)
{
	sql_execute_clicked_cb (NULL, tconsole);
}
	
static void
sql_variables_clicked_cb (GtkToggleButton *button, QueryConsole *tconsole)
{
	if (gtk_toggle_button_get_active (button))
		gtk_widget_show (tconsole->priv->params_top);
	else
		gtk_widget_hide (tconsole->priv->params_top);
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
			sql = browser_connection_render_pretty_sql (tconsole->priv->bcnc,
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

static void
sql_favorite_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	BrowserFavorites *bfav;
	BrowserFavoritesAttributes fav;
	GError *error = NULL;

	memset (&fav, 0, sizeof (fav));
	fav.id = -1;
	fav.type = BROWSER_FAVORITES_QUERIES;
	fav.contents = query_editor_get_all_text (tconsole->priv->editor);
	fav.name = _("Unnamed query");

	bfav = browser_connection_get_favorites (tconsole->priv->bcnc);

	if (! browser_favorites_add (bfav, 0, &fav, ORDER_KEY_QUERIES, G_MAXINT, &error)) {
		browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tconsole),
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
        gtk_widget_size_request ((GtkWidget*) cont, &req);
        gdk_window_get_origin (top->window, &x, &y);
	
	x += (top->allocation.width - req.width) / 2;
	y += (top->allocation.height - req.height) / 2;

        if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;

	*out_x = x;
	*out_y = y;
}

static void
params_form_changed_cb (GdauiBasicForm *form, GdaHolder *param, gboolean is_user_modif, QueryConsole *tconsole)
{
	/* if all params are valid => authorize the execute button */
	GtkWidget *button;

	button = g_object_get_data (G_OBJECT (tconsole->priv->params_popup), "exec");
	gtk_widget_set_sensitive (button,
				  gdaui_basic_form_is_valid (form));
}

static gboolean query_exec_fetch_cb (QueryConsole *tconsole);

static void
sql_execute_clicked_cb (GtkButton *button, QueryConsole *tconsole)
{
	gchar *sql;
	const gchar *remain;
	GdaBatch *batch;
	GError *error = NULL;

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
				vbox = gtk_vbox_new (FALSE, 0);
				gtk_container_add (GTK_CONTAINER (tconsole->priv->params_popup), vbox);
				gtk_container_set_border_width (GTK_CONTAINER (tconsole->priv->params_popup), 10);

				label = gtk_label_new ("");
				str = g_strdup_printf ("<b>%s</b>:\n<small>%s</small>",
						       _("Invalid variable's contents"),
						       _("assign values to the following variables"));
				gtk_label_set_markup (GTK_LABEL (label), str);
				g_free (str);
				gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

				cont = gtk_vbox_new (FALSE, 0);
				gtk_box_pack_start (GTK_BOX (vbox), cont, FALSE, FALSE, 10);
				g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "cont", cont);

				bbox = gtk_hbutton_box_new ();
				gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 10);
				gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
				
				button = gtk_button_new_from_stock (GTK_STOCK_EXECUTE);
				gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);
				g_signal_connect_swapped (button, "clicked",
							  G_CALLBACK (gtk_widget_hide), tconsole->priv->params_popup);
				g_signal_connect (button, "clicked",
						  G_CALLBACK (sql_execute_clicked_cb), tconsole);
				gtk_widget_set_sensitive (button, FALSE);
				g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "exec", button);

				button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
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
			gdaui_basic_form_show_entry_actions (GDAUI_BASIC_FORM (form), TRUE);
			g_signal_connect (form, "param-changed",
					  G_CALLBACK (params_form_changed_cb), tconsole);

			gtk_box_pack_start (GTK_BOX (cont), form, TRUE, TRUE, 0);
			g_object_set_data (G_OBJECT (tconsole->priv->params_popup), "form", form);
			gtk_widget_show_all (tconsole->priv->params_popup);

			return;
		}
	}

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

	/* if a query is being executed, then show an error */
	if (tconsole->priv->current_exec) {
		g_object_unref (batch);
		browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
				    _("A query is already being executed, "
				      "to execute another query, open a new connection."));
		return;
	}

	/* mark the current SQL to be kept by the editor as an internal history */
	query_editor_keep_current_state (tconsole->priv->editor);

	/* actual Execution */
	const GSList *stmt_list, *list;
	ExecutionBatch *ebatch;
	ebatch = g_new0 (ExecutionBatch, 1);
	ebatch->batch = batch;
	g_get_current_time (&(ebatch->start_time));
	ebatch->hist_batch = query_editor_history_batch_new (ebatch->start_time, tconsole->priv->params);
	query_editor_start_history_batch (tconsole->priv->history, ebatch->hist_batch);

	stmt_list = gda_batch_get_statements (batch);
	for (list = stmt_list; list; list = list->next) {
		ExecutionStatement *estmt;
		estmt = g_new0 (ExecutionStatement, 1);
		estmt->stmt = GDA_STATEMENT (list->data);
		ebatch->statements = g_slist_prepend (ebatch->statements, estmt);

		if (list == stmt_list) {
			estmt->within_transaction =
				browser_connection_get_transaction_status (tconsole->priv->bcnc) ? TRUE : FALSE;
			estmt->exec_id = browser_connection_execute_statement (tconsole->priv->bcnc,
									       estmt->stmt,
									       tconsole->priv->params,
									       GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									       FALSE, &(estmt->exec_error));
			if (estmt->exec_id == 0) {
				browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
						    _("Error executing query: %s"),
						    estmt->exec_error && estmt->exec_error->message ? estmt->exec_error->message : _("No detail"));
				execution_batch_free (ebatch);
				return;
			}
		}
	}
	ebatch->statements = g_slist_reverse (ebatch->statements);

	tconsole->priv->current_exec = ebatch;
	tconsole->priv->current_exec_id = g_timeout_add (200,
							 (GSourceFunc) query_exec_fetch_cb,
							 tconsole);
}

static gboolean
query_exec_fetch_cb (QueryConsole *tconsole)
{
	gboolean alldone = TRUE;
	
	if (tconsole->priv->current_exec) {
		if (tconsole->priv->current_exec->statements) {
			ExecutionStatement *estmt;
			estmt = (ExecutionStatement*) tconsole->priv->current_exec->statements->data;
			g_assert (estmt->exec_id);
			g_assert (!estmt->result);
			g_assert (!estmt->exec_error);
			estmt->result = browser_connection_execution_get_result (tconsole->priv->bcnc,
										 estmt->exec_id, NULL,
										 &(estmt->exec_error));
			if (estmt->result || estmt->exec_error) {
				QueryEditorHistoryItem *history;
				GdaSqlStatement *sqlst;
				g_object_get (G_OBJECT (estmt->stmt), "structure", &sqlst, NULL);
				if (!sqlst->sql) {
					gchar *sql;
					sql = gda_statement_to_sql (GDA_STATEMENT (estmt->stmt), NULL, NULL);
					history = query_editor_history_item_new (sql, estmt->result, estmt->exec_error);
					g_free (sql);
				}
				else
					history = query_editor_history_item_new (sqlst->sql,
										 estmt->result, estmt->exec_error);
				gda_sql_statement_free (sqlst);

				history->within_transaction = estmt->within_transaction;

				if (estmt->exec_error) {
					browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
							    _("Error executing query:\n%s"),
							    estmt->exec_error && estmt->exec_error->message ?
							    estmt->exec_error->message : _("No detail"));
					TO_IMPLEMENT;
					/* FIXME: offer the opportunity to stop execution of batch if there
					 *        are some remaining statements to execute
					 * FIXME: store error in history => modify QueryEditorHistoryItem
					 */
				}
				else
					browser_window_push_status (BROWSER_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
								    "QueryConsole", _("Statement executed"), TRUE);

				/* display a message if a transaction has been started */
				if (! history->within_transaction &&
				    browser_connection_get_transaction_status (tconsole->priv->bcnc) &&
				    gda_statement_get_statement_type (estmt->stmt) != GDA_SQL_STATEMENT_BEGIN) {
					browser_show_notice (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
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

				/* get rid of estmt */
				estmt->exec_id = 0;
				execution_statement_free (estmt);
				tconsole->priv->current_exec->statements =
					g_slist_delete_link (tconsole->priv->current_exec->statements,
							     tconsole->priv->current_exec->statements);

				/* more to do ? */
				if (tconsole->priv->current_exec->statements) {
					estmt = (ExecutionStatement*) tconsole->priv->current_exec->statements->data;
					estmt->within_transaction =
						browser_connection_get_transaction_status (tconsole->priv->bcnc) ? TRUE : FALSE;
					estmt->exec_id = browser_connection_execute_statement (tconsole->priv->bcnc,
											       estmt->stmt,
											       tconsole->priv->params,
											       GDA_STATEMENT_MODEL_RANDOM_ACCESS,
											       FALSE,
											       &(estmt->exec_error));
					if (estmt->exec_id == 0) {
						browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tconsole)),
								    _("Error executing query:\n%s"),
								    estmt->exec_error && estmt->exec_error->message ?
								    estmt->exec_error->message : _("No detail"));
					}
					else
						alldone = FALSE;
				}
			}
			else
				alldone = FALSE;
		}
	}
	
	if (alldone) {
		if (tconsole->priv->current_exec) {
			execution_batch_free (tconsole->priv->current_exec);
			tconsole->priv->current_exec = NULL;
		}

		tconsole->priv->current_exec_id = 0;
	}

	return !alldone;
}

/**
 * query_console_set_text
 * @console:
 * @text:
 *
 * Replaces the edited SQL with @text in @console
 */
void
query_console_set_text (QueryConsole *console, const gchar *text)
{
	g_return_if_fail (IS_QUERY_CONSOLE (console));
	query_editor_set_text (console->priv->editor, text);
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

static void
query_console_grab_focus (GtkWidget *widget)
{
	QueryConsole *tconsole;

	tconsole = QUERY_CONSOLE (widget);
	gtk_widget_grab_focus (GTK_WIDGET (tconsole->priv->editor));
}
