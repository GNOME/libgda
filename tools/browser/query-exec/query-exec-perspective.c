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
#include "query-exec-perspective.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "query-console.h"
#include "../browser-stock-icons.h"
#include "../support.h"

/* 
 * Main static functions 
 */
static void query_exec_perspective_class_init (QueryExecPerspectiveClass *klass);
static void query_exec_perspective_init (QueryExecPerspective *stmt);
static void query_exec_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 query_exec_perspective_perspective_init (BrowserPerspectiveIface *iface);
static GtkActionGroup      *query_exec_perspective_get_actions_group (BrowserPerspective *perspective);
static const gchar         *query_exec_perspective_get_actions_ui (BrowserPerspective *perspective);
static void                 query_exec_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _QueryExecPerspectivePrivate {
	GtkWidget *notebook;
	BrowserWindow *bwin;
	BrowserConnection *bcnc;
	
	GtkActionGroup *action_group;
	gboolean updating_transaction_status;
};

GType
query_exec_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (QueryExecPerspectiveClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_exec_perspective_class_init,
			NULL,
			NULL,
			sizeof (QueryExecPerspective),
			0,
			(GInstanceInitFunc) query_exec_perspective_init
		};

		static GInterfaceInfo perspective_info = {
                        (GInterfaceInitFunc) query_exec_perspective_perspective_init,
			NULL,
                        NULL
                };
		
		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_VBOX, "QueryExecPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
query_exec_perspective_class_init (QueryExecPerspectiveClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = query_exec_perspective_dispose;
}

static void
query_exec_perspective_perspective_init (BrowserPerspectiveIface *iface)
{
	iface->i_get_actions_group = query_exec_perspective_get_actions_group;
	iface->i_get_actions_ui = query_exec_perspective_get_actions_ui;
	iface->i_page_tab_label_change = query_exec_perspective_page_tab_label_change;
}


static void
query_exec_perspective_init (QueryExecPerspective *perspective)
{
	perspective->priv = g_new0 (QueryExecPerspectivePrivate, 1);
	perspective->priv->action_group = NULL;
}

static void nb_switch_page_cb (GtkNotebook *nb, GtkNotebookPage *page, gint page_num,
			       QueryExecPerspective *perspective);
static void nb_page_removed_cb (GtkNotebook *nb, GtkNotebookPage *page, gint page_num,
			       QueryExecPerspective *perspective);
static void close_button_clicked_cb (GtkWidget *wid, GtkWidget *page_widget);

static void transaction_status_changed_cb (BrowserConnection *bcnc, QueryExecPerspective *perspective);

/**
 * query_exec_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
query_exec_perspective_new (BrowserWindow *bwin)
{
	BrowserConnection *bcnc;
	BrowserPerspective *bpers;
	QueryExecPerspective *perspective;

	bpers = (BrowserPerspective*) g_object_new (TYPE_QUERY_EXEC_PERSPECTIVE, NULL);
	perspective = (QueryExecPerspective*) bpers;

	perspective->priv->bwin = bwin;
	bcnc = browser_window_get_connection (bwin);
	perspective->priv->bcnc = g_object_ref (bcnc);

	/* contents */
	GtkWidget *paned, *nb;
	paned = gtk_hpaned_new ();
	/*
	wid = favorite_selector_new (bcnc);
	g_signal_connect (wid, "selection-changed",
			  G_CALLBACK (fav_selection_changed_cb), bpers);
	gtk_paned_add1 (GTK_PANED (paned), wid);
	*/

	nb = gtk_notebook_new ();
	perspective->priv->notebook = nb;
	gtk_paned_add2 (GTK_PANED (paned), nb);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));
	g_signal_connect (G_OBJECT (nb), "switch-page",
			  G_CALLBACK (nb_switch_page_cb), perspective);
	g_signal_connect (G_OBJECT (nb), "page-removed",
			  G_CALLBACK (nb_page_removed_cb), perspective);

	GtkWidget *page, *tlabel, *button;

	page = query_console_new (bcnc);
	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (close_button_clicked_cb), page);

	gtk_notebook_append_page (GTK_NOTEBOOK (nb), page, tlabel);

	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb), page, TRUE);
	gtk_notebook_set_group (GTK_NOTEBOOK (nb), bcnc + 0x01); /* add 0x01 to differentiate it from SchemaBrowser */
	gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
					 TRUE);

	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
	gtk_notebook_set_menu_label (GTK_NOTEBOOK (nb), page, tlabel);

	gtk_box_pack_start (GTK_BOX (bpers), paned, TRUE, TRUE, 0);
	gtk_widget_show_all (paned);

	/* transaction status detection */
	g_signal_connect (bcnc, "transaction-status-changed",
			  G_CALLBACK (transaction_status_changed_cb), bpers);

	return bpers;
}

static void
nb_switch_page_cb (GtkNotebook *nb, GtkNotebookPage *page, gint page_num,
		   QueryExecPerspective *perspective)
{
	GtkWidget *page_contents;
	GtkActionGroup *actions = NULL;
	const gchar *ui = NULL;

	page_contents = gtk_notebook_get_nth_page (nb, page_num);
	if (IS_BROWSER_PAGE (page_contents)) {
		actions = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}
	browser_window_customize_perspective_ui (perspective->priv->bwin,
						 BROWSER_PERSPECTIVE (perspective), actions, 
						 ui);
	if (actions)
		g_object_unref (actions);
}

static void
nb_page_removed_cb (GtkNotebook *nb, GtkNotebookPage *page, gint page_num,
		    QueryExecPerspective *perspective)
{
	if (gtk_notebook_get_n_pages (nb) == 0) {
		browser_window_customize_perspective_ui (perspective->priv->bwin,
							 BROWSER_PERSPECTIVE (perspective),
							 NULL, NULL);
	}
}

static void
close_button_clicked_cb (GtkWidget *wid, GtkWidget *page_widget)
{
	gtk_widget_destroy (page_widget);
}

static void
query_exec_perspective_dispose (GObject *object)
{
	QueryExecPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY_EXEC_PERSPECTIVE (object));

	perspective = QUERY_EXEC_PERSPECTIVE (object);
	if (perspective->priv) {
		g_signal_handlers_disconnect_by_func (perspective->priv->bcnc,
						      G_CALLBACK (transaction_status_changed_cb), perspective);
		if (perspective->priv->bcnc)
			g_object_unref (perspective->priv->bcnc);

		if (perspective->priv->action_group)
			g_object_unref (perspective->priv->action_group);

		g_signal_handlers_disconnect_by_func (perspective->priv->notebook,
						      G_CALLBACK (nb_page_removed_cb), perspective);
		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
query_exec_add_cb (GtkAction *action, BrowserPerspective *bpers)
{
	GtkWidget *page, *tlabel, *button;
	QueryExecPerspective *perspective;
	BrowserConnection *bcnc;
	gint i;

	perspective = QUERY_EXEC_PERSPECTIVE (bpers);
	bcnc = perspective->priv->bcnc;

	page = query_console_new (bcnc);
	gtk_widget_show (page);
	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (close_button_clicked_cb), page);


	i = gtk_notebook_append_page (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (perspective->priv->notebook), i);

	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (perspective->priv->notebook), page, TRUE);
	gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
					 TRUE);

	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
	gtk_notebook_set_menu_label (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);
}

static void
transaction_status_changed_cb (BrowserConnection *bcnc, QueryExecPerspective *perspective)
{
	if (!perspective->priv->action_group)
		return;

	GtkAction *action;
	gboolean trans_started;

	trans_started = browser_connection_get_transaction_status (bcnc) ? TRUE : FALSE;
	perspective->priv->updating_transaction_status = TRUE;

	action = gtk_action_group_get_action (perspective->priv->action_group, "QueryExecBegin");
	gtk_action_set_sensitive (action, !trans_started);

	action = gtk_action_group_get_action (perspective->priv->action_group, "QueryExecCommit");
	gtk_action_set_sensitive (action, trans_started);

	action = gtk_action_group_get_action (perspective->priv->action_group, "QueryExecRollback");
	gtk_action_set_sensitive (action, trans_started);
				      
	perspective->priv->updating_transaction_status = FALSE;
}

static void
transaction_begin_cb (GtkAction *action, BrowserPerspective *bpers)
{
	QueryExecPerspective *perspective;
	BrowserConnection *bcnc;

	perspective = QUERY_EXEC_PERSPECTIVE (bpers);
	bcnc = perspective->priv->bcnc;
	if (!perspective->priv->updating_transaction_status) {
		GError *error = NULL;
		if (! browser_connection_begin (bcnc, &error)) {
			browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) bpers)),
					    _("Error starting transaction: %s"),
					    error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static void
transaction_commit_cb (GtkAction *action, BrowserPerspective *bpers)
{
	QueryExecPerspective *perspective;
	BrowserConnection *bcnc;

	perspective = QUERY_EXEC_PERSPECTIVE (bpers);
	bcnc = perspective->priv->bcnc;
	if (!perspective->priv->updating_transaction_status) {
		GError *error = NULL;
		if (! browser_connection_commit (bcnc, &error)) {
			browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) bpers)),
					    _("Error committing transaction: %s"),
					    error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static void
transaction_rollback_cb (GtkAction *action, BrowserPerspective *bpers)
{
	QueryExecPerspective *perspective;
	BrowserConnection *bcnc;

	perspective = QUERY_EXEC_PERSPECTIVE (bpers);
	bcnc = perspective->priv->bcnc;
	if (!perspective->priv->updating_transaction_status) {
		GError *error = NULL;
		if (! browser_connection_rollback (bcnc, &error)) {
			browser_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) bpers)),
					    _("Error rolling back transaction: %s"),
					    error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static GtkActionEntry ui_actions[] = {
        { "QueryExecMenu", NULL, N_("_Query"), NULL, "QueryExecMenu", NULL },
        { "QueryExecItem1", STOCK_CONSOLE, N_("_New editor"), NULL, N_("Open a new query editor"),
          G_CALLBACK (query_exec_add_cb)},
        { "QueryExecBegin", BROWSER_STOCK_BEGIN, N_("_Begin"), NULL, N_("Begin a new transaction"),
          G_CALLBACK (transaction_begin_cb)},
        { "QueryExecCommit", BROWSER_STOCK_COMMIT, N_("_Commit"), NULL, N_("Commit current transaction"),
          G_CALLBACK (transaction_commit_cb)},
        { "QueryExecRollback", BROWSER_STOCK_ROLLBACK, N_("_Rollback"), NULL, N_("Rollback current transaction"),
          G_CALLBACK (transaction_rollback_cb)},

};

static const gchar *ui_actions_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='QueryExec' action='QueryExecMenu'>"
        "        <menuitem name='QueryExecItem1' action= 'QueryExecItem1'/>"
        "        <menuitem name='QueryExecBegin' action= 'QueryExecBegin'/>"
        "        <menuitem name='QueryExecCommit' action= 'QueryExecCommit'/>"
        "        <menuitem name='QueryExecRollback' action= 'QueryExecRollback'/>"
        "      </menu>"
	"    </placeholder>"
        "  </menubar>"
        "  <toolbar name='ToolBar'>"
        "    <separator/>"
        "    <toolitem action='QueryExecItem1'/>"
        "    <toolitem action='QueryExecBegin'/>"
        "    <toolitem action='QueryExecCommit'/>"
        "    <toolitem action='QueryExecRollback'/>"
        "  </toolbar>"
        "</ui>";

static GtkActionGroup *
query_exec_perspective_get_actions_group (BrowserPerspective *perspective)
{
	QueryExecPerspective *bpers;
	bpers = QUERY_EXEC_PERSPECTIVE (perspective);

	if (!bpers->priv->action_group) {
		GtkActionGroup *agroup;
		agroup = gtk_action_group_new ("QueryExecActions");
		gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), bpers);
		bpers->priv->action_group = g_object_ref (agroup);
	}
	
	transaction_status_changed_cb (bpers->priv->bcnc, bpers);

	return bpers->priv->action_group;
}

static const gchar *
query_exec_perspective_get_actions_ui (BrowserPerspective *perspective)
{
	return ui_actions_info;
}

static void
query_exec_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page)
{
	QueryExecPerspective *bpers;
	GtkWidget *tab_label;
	GtkWidget *close_btn;
	
	bpers = QUERY_EXEC_PERSPECTIVE (perspective);
	tab_label = browser_page_get_tab_label (page, &close_btn);
	if (tab_label) {
		gtk_notebook_set_tab_label (GTK_NOTEBOOK (bpers->priv->notebook),
					    GTK_WIDGET (page), tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), page);
		
		tab_label = browser_page_get_tab_label (page, NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook),
					     GTK_WIDGET (page), tab_label);
	}
}
