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
};

static void query_console_class_init (QueryConsoleClass *klass);
static void query_console_init       (QueryConsole *tconsole, QueryConsoleClass *klass);
static void query_console_dispose   (GObject *object);

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

	GtkWidget *wid;
	wid = query_editor_new ();
	tconsole->priv->editor = QUERY_EDITOR (wid);
	gtk_paned_add1 (GTK_PANED (vpaned), wid);

	/* show everything */
        gtk_widget_show_all (vpaned);

	return (GtkWidget*) tconsole;
}

/*
 * UI actions
 */
static void
query_execute_cb (GtkAction *action, QueryConsole *tconsole)
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
	TO_IMPLEMENT;
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
