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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "browser-window.h"
#include <common/t-app.h>
#include <common/t-connection.h>
#include "ui-support.h"
#include "connection-binding-properties.h"
#include <gdk/gdkkeysyms.h>
#include <thread-wrapper/gda-connect.h>
#include "browser-connections-list.h"

/*
 * structure representing a 'tab' in a window
 *
 * a 'page' is a set of widgets created by a single BrowserPerspectiveFactory object, for a connection
 * Each is owned by a BrowserWindow
 */
typedef struct {
        BrowserWindow             *bwin; /* pointer to window the tab is in, no ref held here */
        BrowserPerspectiveFactory *factory;
        BrowserPerspective        *perspective_widget;
} PerspectiveData;
#define PERSPECTIVE_DATA(x) ((PerspectiveData*)(x))
static PerspectiveData *perspective_data_new (BrowserWindow *bwin, BrowserPerspectiveFactory *factory);
static void             perspective_data_free (PerspectiveData *pers);

/* 
 * Main static functions 
 */
static void browser_window_class_init (BrowserWindowClass *klass);
static void browser_window_init (BrowserWindow *bwin);
static void browser_window_dispose (GObject *object);

static gboolean key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer userdata);

static void transaction_status_changed_cb (TConnection *tcnc, BrowserWindow *bwin);
static void cnc_status_changed_cb (TConnection *tcnc, GdaConnectionStatus status, BrowserWindow *bwin);


enum {
        FULLSCREEN_CHANGED,
        META_UPDATED,
        LAST_SIGNAL
};

static gint browser_window_signals[LAST_SIGNAL] = { 0 };


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _BrowserWindowPrivate {
	TConnection       *tcnc;
	GtkStack          *pers_stack; /* notebook used to switch between tabs */
        GSList            *pers_list; /* list of PerspectiveData pointers, owned here */
	PerspectiveData   *current_perspective;
	guint              ui_manager_merge_id; /* for current perspective */

	GtkWidget         *spinner;
	gboolean           updating_transaction_status;

	GtkToolbarStyle    toolbar_style;

	GtkWidget         *notif_box;
	GSList            *notif_widgets;

	GtkWidget         *statusbar;
	guint              cnc_statusbar_context;

	gboolean           fullscreen;

	gulong             cnc_status_sigid;
	gulong             trans_status_sigid;

	GtkToolbar        *toolbar;
	GtkHeaderBar      *header;
};

GType
browser_window_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (BrowserWindowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_window_class_init,
			NULL,
			NULL,
			sizeof (BrowserWindow),
			0,
			(GInstanceInitFunc) browser_window_init,
			0
		};

		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_APPLICATION_WINDOW, "BrowserWindow", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
browser_window_class_init (BrowserWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	browser_window_signals[FULLSCREEN_CHANGED] =
                g_signal_new ("fullscreen-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (BrowserWindowClass, fullscreen_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	browser_window_signals[META_UPDATED] =
                g_signal_new ("meta-updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (BrowserWindowClass, meta_updated),
                              NULL, NULL,
                              NULL,
                              G_TYPE_NONE, 1, G_TYPE_OBJECT);

	object_class->dispose = browser_window_dispose;
}

static void
browser_window_init (BrowserWindow *bwin)
{
	bwin->priv = g_new0 (BrowserWindowPrivate, 1);
	bwin->priv->tcnc = NULL;
	bwin->priv->pers_stack = NULL;
	bwin->priv->pers_list = NULL;
	bwin->priv->updating_transaction_status = FALSE;
	bwin->priv->fullscreen = FALSE;
	g_signal_connect (G_OBJECT (bwin), "key-press-event", G_CALLBACK (key_press_event), NULL);
}

static void
browser_window_dispose (GObject *object)
{
	BrowserWindow *bwin;

	g_return_if_fail (BROWSER_IS_WINDOW (object));

	bwin = BROWSER_WINDOW (object);
	if (bwin->priv) {
		if (bwin->priv->tcnc) {
			gda_signal_handler_disconnect (bwin->priv->tcnc, bwin->priv->cnc_status_sigid);
			gda_signal_handler_disconnect (bwin->priv->tcnc, bwin->priv->trans_status_sigid);
			g_object_unref (bwin->priv->tcnc);
		}
		if (bwin->priv->pers_list) {
			g_slist_foreach (bwin->priv->pers_list, (GFunc) perspective_data_free, NULL);
			g_slist_free (bwin->priv->pers_list);
		}
		if (bwin->priv->pers_stack)
			g_object_unref (bwin->priv->pers_stack);

		if (bwin->priv->notif_widgets)
			g_slist_free (bwin->priv->notif_widgets);
		g_free (bwin->priv);
		bwin->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void connection_meta_update_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data);
static void transaction_begin_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data);
static void transaction_commit_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data);
static void transaction_rollback_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data);
static void connection_properties_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data);
static void connection_close_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data);
static void fullscreen_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *state, gpointer data);
static void change_perspective_cb (GSimpleAction *action, GVariant *state, gpointer data);
static GActionEntry win_entries[] = {
	{ "change-perspective", NULL, "s", NULL, change_perspective_cb },

	{ "meta-sync", connection_meta_update_cb, NULL, NULL, NULL },

	{ "begin", transaction_begin_cb, NULL, NULL, NULL },
	{ "commit", transaction_commit_cb, NULL, NULL, NULL },
	{ "rollback", transaction_rollback_cb, NULL, NULL, NULL },

	{ "properties", connection_properties_cb, NULL, NULL, NULL },
	{ "cncclose", connection_close_cb, NULL, NULL, NULL },

	{ "fullscreen", NULL, NULL, "false", fullscreen_cb },
};

/**
 * browser_window_new
 * @tcnc: a #TConnection, not %NULL
 * @factory: a #BrowserPerspectiveFactory, may be %NULL
 *
 * Creates a new #BrowserWindow window for the @tcnc connection, and displays it.
 * If @factory is not %NULL, then the new window will show the perspective corresponding
 * to @factory. If it's %NULL, then the default #BrowserPerspectiveFactory will be used,
 * see t_app_get_default_factory().
 *
 * Returns: the new object
 */
BrowserWindow*
browser_window_new (TConnection *tcnc, BrowserPerspectiveFactory *factory)
{
	BrowserWindow *bwin;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	bwin = BROWSER_WINDOW (g_object_new (BROWSER_TYPE_WINDOW, "application", t_app_get(),
					     "show-menubar", TRUE, NULL));
	gtk_application_add_window (GTK_APPLICATION (t_app_get ()), GTK_WINDOW (bwin));
	gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (bwin), FALSE);

	bwin->priv->tcnc = g_object_ref (tcnc);
	bwin->priv->trans_status_sigid = gda_signal_connect (tcnc, "transaction-status-changed",
							     G_CALLBACK (transaction_status_changed_cb), bwin,
							     NULL, 0, NULL);
	bwin->priv->cnc_status_sigid = gda_signal_connect (tcnc, "status-changed",
							   G_CALLBACK (cnc_status_changed_cb), bwin,
							   NULL, 0, NULL);

	gchar *tmp;

	gtk_window_set_default_size ((GtkWindow*) bwin, 900, 650);

	/* icon */
	GdkPixbuf *icon;
	icon = gdk_pixbuf_new_from_resource ("/images/gda-browser.png", NULL);
        if (icon) {
                gtk_window_set_icon (GTK_WINDOW (bwin), icon);
                g_object_unref (icon);
        }

	/* Obtain a valid Perspective factory */
	if (! factory && t_connection_is_ldap (tcnc))
		factory = browser_get_factory (_("LDAP browser"));
	if (!factory)
		factory = browser_get_default_factory ();
	tmp = g_strdup_printf ("'%s'", factory->id);
	win_entries [0].state = tmp;

	/* menu */
	g_action_map_add_action_entries (G_ACTION_MAP (bwin),
					 win_entries, G_N_ELEMENTS (win_entries),
					 bwin);
	win_entries [0].state = NULL;
	g_free (tmp);

	/* main VBox */
	GtkWidget *vbox;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add (GTK_CONTAINER (bwin), vbox);
        gtk_widget_show (vbox);

	/* header */
	GtkWidget *header;
	header = gtk_header_bar_new ();
	bwin->priv->header = GTK_HEADER_BAR (header);
	gtk_window_set_titlebar (GTK_WINDOW (bwin), header);
	tmp = t_connection_get_long_name (tcnc);
	gtk_header_bar_set_title (GTK_HEADER_BAR (header), t_connection_get_name (tcnc));
	gtk_header_bar_set_subtitle (GTK_HEADER_BAR (header), t_connection_get_information (tcnc));
	g_free (tmp);

	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
	gtk_header_bar_set_has_subtitle (GTK_HEADER_BAR (header), FALSE);
	gtk_widget_show (header);

	/* Main menu button */
	GtkWidget *img, *button;
	button = gtk_menu_button_new ();
	img = gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (button), img);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);
	gtk_widget_show_all (button);
	GMenu *menu;
	menu = g_menu_new ();
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));

	GMenu *smenu;
	smenu = g_menu_new ();
	GMenuItem *msection;
	msection = g_menu_item_new_section (NULL, G_MENU_MODEL (smenu));
	g_menu_insert_item (menu, -1, msection);

	GMenuItem *menu_item;
	menu_item = g_menu_item_new (_("Fetch Meta Data"), "win.meta-sync");
	g_menu_insert_item (smenu, -1, menu_item);
	g_object_unref (menu_item);
	menu_item = g_menu_item_new (_("Connection properties"), "win.properties");
	g_menu_insert_item (smenu, -1, menu_item);
	g_object_unref (menu_item);

	smenu = g_menu_new ();
	msection = g_menu_item_new_section (NULL, G_MENU_MODEL (smenu));
	g_menu_insert_item (menu, -1, msection);
	menu_item = g_menu_item_new (_("Close connection"), "win.cncclose");
	g_menu_insert_item (smenu, -1, menu_item);
	g_object_unref (menu_item);

	/* Display options button */
	GtkWidget *menu_button;
	button = gtk_menu_button_new ();
	gtk_widget_set_tooltip_text (button, _("Display options"));
	img = gtk_image_new_from_icon_name ("document-properties-symbolic", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (button), img);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);
	gtk_widget_show_all (button);
	menu_button = button;

	/* toolbar */
	GtkWidget *toolbar;
	toolbar = gtk_toolbar_new ();
	bwin->priv->toolbar = GTK_TOOLBAR (toolbar);
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
	GtkToolItem *titem;
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Rollback transaction"));
	img = gtk_image_new_from_resource ("/images/transaction-rollback-symbolic.png");
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (titem), img);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, 0);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.rollback");

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Commit transaction"));
	img = gtk_image_new_from_resource ("/images/transaction-commit-symbolic.png");
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (titem), img);	
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, 0);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.commit");

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Begin transaction"));
	img = gtk_image_new_from_resource ("/images/transaction-start-symbolic.png");
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (titem), img);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, 0);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.begin");

	titem = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	gtk_widget_show_all (toolbar);

	/* notification area */
	bwin->priv->notif_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start (GTK_BOX (vbox), bwin->priv->notif_box, FALSE, FALSE, 0);
        gtk_widget_show (bwin->priv->notif_box);
        bwin->priv->notif_widgets = NULL;

	/* create a PerspectiveData */
	PerspectiveData *pers;
	pers = perspective_data_new (bwin, factory);
	bwin->priv->pers_list = g_slist_prepend (bwin->priv->pers_list, pers);
	bwin->priv->current_perspective = pers;
	
	/* insert perspective into window */
        bwin->priv->pers_stack = (GtkStack*) gtk_stack_new ();
        g_object_ref (bwin->priv->pers_stack);
	gtk_box_pack_start (GTK_BOX (vbox), (GtkWidget*) bwin->priv->pers_stack,
			    TRUE, TRUE, 0);

	gtk_stack_add_named (bwin->priv->pers_stack, GTK_WIDGET (pers->perspective_widget),
			     pers->factory->id);
        gtk_widget_show_all ((GtkWidget*) bwin->priv->pers_stack);
	gtk_widget_grab_focus (GTK_WIDGET (pers->perspective_widget));

	/* build the perspectives menu */
	GMenuModel *menumodel;
	menumodel = (GMenuModel*) g_object_get_data (G_OBJECT (t_app_get ()), "perspectives");
	g_assert (menumodel);
	menu = G_MENU (menumodel);
	g_menu_remove_all (menu);

	GMenu *bmenu;
	bmenu = g_menu_new ();
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), G_MENU_MODEL (bmenu));

	GMenuItem *mitem;
	mitem = g_menu_item_new (_("Fullscreen"), "win.fullscreen");
	g_menu_insert_item (bmenu, -1, mitem);
	GMenuItem *open_cnc;
	open_cnc = g_menu_item_new (_("Open Connection"), "app.opencnc");
	g_menu_insert_item (bmenu, -1, open_cnc);
	GMenuItem *bind_cnc;
	bind_cnc = g_menu_item_new (_("Bind Connections"), "app.bind");
	g_menu_insert_item (bmenu, -1, bind_cnc);
	GMenuItem *listcnc;
	listcnc = g_menu_item_new (_("Connections List"), "app.listcnc");
	g_menu_insert_item (bmenu, -1, listcnc);
	GMenuItem *newwin;
	newwin = g_menu_item_new (_("New Window"), "app.newwin");
	g_menu_insert_item (bmenu, -1, newwin);
	GMenuItem *about;
	about = g_menu_item_new (_("About"), "app.about");
	g_menu_insert_item (bmenu, -1, about);
#ifdef HAVE_GDU
	GMenuItem *help;
	help = g_menu_item_new (_("Help"), "app.newwin");
	g_menu_insert_item (bmenu, -1, help);
#endif
	GMenuItem *quit;
	quit = g_menu_item_new (_("Quit"), "app.quit");
	g_menu_insert_item (bmenu, -1, quit);

	smenu = g_menu_new ();
	msection = g_menu_item_new_section (_("Perspectives"), G_MENU_MODEL (smenu));
	g_menu_insert_item (bmenu, -1, msection);

	const GSList *plist;
	for (plist = browser_get_factories (); plist; plist = plist->next) {
		const gchar *name, *id;

		name = BROWSER_PERSPECTIVE_FACTORY (plist->data)->perspective_name;
		id = BROWSER_PERSPECTIVE_FACTORY (plist->data)->id;
		if (!strcmp (name, _("LDAP browser")) && !t_connection_is_ldap (tcnc))
			continue;

		gchar *tmp;
		tmp = g_strdup_printf ("win.change-perspective::%s", id);

		menu_item = g_menu_item_new (name, tmp);
		g_menu_insert_item (menu, -1, menu_item);
		g_object_unref (menu_item);

		menu_item = g_menu_item_new (name, tmp);
		g_menu_insert_item (smenu, -1, menu_item);
		g_object_unref (menu_item);
		
		/* accelerators */
		const gchar *accels[2];
		accels [0] = BROWSER_PERSPECTIVE_FACTORY (plist->data)->menu_shortcut;
		accels [1] = NULL;
		gtk_application_set_accels_for_action (GTK_APPLICATION (t_app_get ()), tmp, accels);

		g_print ("ADDED perspective %s (%s) to menu with action [%s], accel [%s]\n", name, id, tmp,
			 BROWSER_PERSPECTIVE_FACTORY (plist->data)->menu_shortcut);
		g_free (tmp);
	}

	/* statusbar and spinner */
	bwin->priv->statusbar = gtk_statusbar_new ();
	bwin->priv->cnc_statusbar_context = gtk_statusbar_get_context_id (GTK_STATUSBAR (bwin->priv->statusbar),
									  "cncbusy");
	GtkWidget *lbox;
	lbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), lbox, FALSE, FALSE, 0);
	bwin->priv->spinner = gtk_spinner_new (); /* don't show spinner now */
	gtk_box_pack_start (GTK_BOX (lbox), bwin->priv->statusbar, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (lbox), bwin->priv->spinner, FALSE, FALSE, 0);
        gtk_widget_show_all (lbox);

	/* accels */
	const gchar *accels[2];
	accels [0] = "F11";
	accels [1] = NULL;
	gtk_application_set_accels_for_action (GTK_APPLICATION (t_app_get ()), "win.fullscreen", accels);

	cnc_status_changed_cb (tcnc, gda_connection_get_status (t_connection_get_cnc (tcnc)), bwin);
        gtk_widget_show (GTK_WIDGET (bwin));

	gtk_widget_set_can_focus ((GtkWidget* )pers->perspective_widget, TRUE);
	gtk_widget_grab_focus ((GtkWidget* )pers->perspective_widget);

	/* customize currect perspective */
	browser_perspective_customize (bwin->priv->current_perspective->perspective_widget,
				       bwin->priv->toolbar, bwin->priv->header);

	return bwin;
}

static void
fullscreen_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *state, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	if (g_variant_get_boolean (state)) {
		gtk_window_fullscreen (GTK_WINDOW (bwin));
		bwin->priv->fullscreen = TRUE;
		gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (bwin), TRUE);
		browser_window_show_notice_printf (bwin, GTK_MESSAGE_INFO,
						   "fullscreen-esc",
						   "%s", _("Hit the F11 key to leave the fullscreen mode"));
	}
	else {
		gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (bwin), FALSE);
		gtk_window_unfullscreen (GTK_WINDOW (bwin));
		bwin->priv->fullscreen = FALSE;
	}
	g_simple_action_set_state (action, state);
}

static void
cnc_status_changed_cb (TConnection *tcnc, GdaConnectionStatus status, BrowserWindow *bwin)
{
	gchar *reason = "AAAA";
	gboolean is_busy;
	is_busy = (status == GDA_CONNECTION_STATUS_IDLE) ? FALSE : TRUE;

	if (tcnc == bwin->priv->tcnc) {
		if (is_busy) {
			gtk_widget_show (GTK_WIDGET (bwin->priv->spinner));
			gtk_spinner_start (GTK_SPINNER (bwin->priv->spinner));
			gtk_widget_set_tooltip_text (bwin->priv->spinner, reason);
			gtk_statusbar_push (GTK_STATUSBAR (bwin->priv->statusbar),
					    bwin->priv->cnc_statusbar_context,
					    reason);

			GAction *action;
			action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "begin");
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
			action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "commit");
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
			action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "rollback");
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
			action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "meta-sync");
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
		}
		else {
			gtk_spinner_stop (GTK_SPINNER (bwin->priv->spinner));
			gtk_widget_hide (GTK_WIDGET (bwin->priv->spinner));
			gtk_widget_set_tooltip_text (bwin->priv->spinner, NULL);
			gtk_statusbar_pop (GTK_STATUSBAR (bwin->priv->statusbar),
					   bwin->priv->cnc_statusbar_context);
			transaction_status_changed_cb (tcnc, bwin);
		}
	}
}

static void
connection_close_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	/* confirmation dialog */
	GtkWidget *dialog;
	char *str;
	TConnection *tcnc;
	tcnc = browser_window_get_connection (bwin);
		
	str = g_strdup_printf (_("Do you want to close the '%s' connection?"),
			       t_connection_get_name (tcnc));
	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (bwin), GTK_DIALOG_MODAL,
						     GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_YES_NO,
						     "<b>%s</b>", str);
	g_free (str);

	gboolean doclose;
	doclose = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) ? TRUE : FALSE;
	gtk_widget_destroy (dialog);
	if (doclose) {
		/* actual connection closing */
		t_connection_close (tcnc);
	}
}

static void
transaction_status_changed_cb (TConnection *tcnc, BrowserWindow *bwin)
{
	gboolean trans_started;

	trans_started = t_connection_get_transaction_status (tcnc) ? TRUE : FALSE;
	bwin->priv->updating_transaction_status = TRUE;

	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "begin");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !trans_started);
	action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "commit");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), trans_started);
	action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "rollback");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), trans_started);

	action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "meta-sync");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !trans_started);
				      
	bwin->priv->updating_transaction_status = FALSE;
}

static void
transaction_begin_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	if (!bwin->priv->updating_transaction_status) {
		GError *error = NULL;
		if (! t_connection_begin (bwin->priv->tcnc, &error)) {
			ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) bwin)),
				       _("Error starting transaction: %s"),
				       error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static void
transaction_commit_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	if (!bwin->priv->updating_transaction_status) {
		GError *error = NULL;
		if (! t_connection_commit (bwin->priv->tcnc, &error)) {
			ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) bwin)),
				       _("Error committing transaction: %s"),
				       error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static void
transaction_rollback_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	if (!bwin->priv->updating_transaction_status) {
		GError *error = NULL;
		if (! t_connection_rollback (bwin->priv->tcnc, &error)) {
			ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) bwin)),
				       _("Error rolling back transaction: %s"),
				       error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static void
change_perspective_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	const gchar *pname;
	pname = g_variant_get_string (state, NULL);
	g_print ("Switching to perspective [%s]\n", pname);
	browser_window_change_perspective (bwin, pname);
}

static gboolean
key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer userdata)
{
	if ((event->keyval == GDK_KEY_Escape) &&
	    browser_window_is_fullscreen (BROWSER_WINDOW (widget))) {
		browser_window_set_fullscreen (BROWSER_WINDOW (widget), FALSE);
		return TRUE;
	}

	return FALSE;
}

static void
connection_properties_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data)
{
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	if (T_IS_VIRTUAL_CONNECTION (bwin->priv->tcnc)) {
		GtkWidget *win;
		TVirtualConnectionSpecs *specs;
		gint res;

		g_object_get (G_OBJECT (bwin->priv->tcnc), "specs", &specs, NULL);
		win = connection_binding_properties_new_edit (specs); /* @specs is not modified */
		gtk_window_set_transient_for (GTK_WINDOW (win), GTK_WINDOW (bwin));
		gtk_widget_show (win);
		
		res = gtk_dialog_run (GTK_DIALOG (win));
		gtk_widget_hide (win);
		if (res == GTK_RESPONSE_OK) {
			GError *error = NULL;
			if (!t_virtual_connection_modify_specs (T_VIRTUAL_CONNECTION (bwin->priv->tcnc),
								connection_binding_properties_get_specs (CONNECTION_BINDING_PROPERTIES (win)),
								&error)) {
				ui_show_error ((GtkWindow*) bwin,
					       _("Error updating bound connection: %s"),
					       error && error->message ? error->message : _("No detail"));
				g_clear_error (&error);
			}

			/* update meta store */
			t_connection_update_meta_data (bwin->priv->tcnc, &error);
      if (error != NULL) {
        ui_show_error ((GtkWindow*) bwin,
					       _("Error updating meta data for connection: %s"),
					       error && error->message ? error->message : _("No detail"));
				g_clear_error (&error);
      }
		}
		gtk_widget_destroy (win);
	}
	else
		browser_connections_list_show (bwin->priv->tcnc);
}

static void
connection_meta_update_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *parameter, gpointer data)
{
  GError *error = NULL;
	BrowserWindow *bwin = BROWSER_WINDOW (data);
	t_connection_update_meta_data (bwin->priv->tcnc, &error);
  if (error != NULL) {
    GtkWidget *msg;
    msg = gtk_message_dialog_new_with_markup (NULL,
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_WARNING,
                                              GTK_BUTTONS_CLOSE,
                                              _("Error Updating Meta Data: %s"),
                                              error->message ? error->message : _("No detail")
                                              );
    gtk_dialog_run (GTK_DIALOG (msg));
    gtk_widget_destroy (msg);
    g_clear_error (&error);
  }
  g_signal_emit (bwin, browser_window_signals[META_UPDATED], 0, bwin->priv->tcnc, NULL);
	//gtk_widget_insert_action_group (GTK_WIDGET (bwin), "win", NULL);
}


/**
 * browser_window_get_connection
 * @bwin: a #BrowserWindow
 * 
 * Returns: the #TConnection used in @bwin
 */
TConnection *
browser_window_get_connection (BrowserWindow *bwin)
{
	g_return_val_if_fail (BROWSER_IS_WINDOW (bwin), NULL);
	return bwin->priv->tcnc;
}


/*
 * perspective_data_new
 * @bwin: a #BrowserWindow in which the perspective will be
 * @factory: (nullable): a #BrowserPerspectiveFactory, or %NULL
 *
 * Creates a new #PerspectiveData structure, it increases @tcnc's reference count.
 *
 * Returns: a new #PerspectiveData
 */
static PerspectiveData *
perspective_data_new (BrowserWindow *bwin, BrowserPerspectiveFactory *factory)
{
        PerspectiveData *pers;

        pers = g_new0 (PerspectiveData, 1);
        pers->bwin = NULL;
        pers->factory = factory;
        if (!pers->factory)
                pers->factory = browser_get_default_factory ();
        g_assert (pers->factory);
	pers->perspective_widget = g_object_ref (pers->factory->perspective_create (bwin));

        return pers;
}

/*
 * perspective_data_free
 */
static void
perspective_data_free (PerspectiveData *pers)
{
        if (pers->perspective_widget)
                g_object_unref (pers->perspective_widget);
        g_free (pers);
}

typedef struct {
	BrowserWindow *bwin;
	guint cid; /* statusbar context id */
	guint msgid; /* statusbar message id */
} StatusData;

static gboolean
status_auto_pop_timeout (StatusData *sd)
{
	if (sd->bwin) {
		g_object_remove_weak_pointer (G_OBJECT (sd->bwin), (gpointer*) &(sd->bwin));
		gtk_statusbar_remove (GTK_STATUSBAR (sd->bwin->priv->statusbar), sd->cid, sd->msgid);
	}
	g_free (sd);

	return FALSE; /* remove source */
}

/**
 * browser_window_push_status
 * @bwin: a #BrowserWindow
 * @context: textual description of what context the new message is being used in
 * @text: textual message
 * @auto_clear: %TRUE if the message has to disappear after a while
 *
 * Pushes a new message onto @bwin's statusbar's stack.
 *
 * Returns: the message ID, see gtk_statusbar_push(), or %0 if @auto_clear is %TRUE
 */
guint
browser_window_push_status (BrowserWindow *bwin, const gchar *context, const gchar *text, gboolean auto_clear)
{
	guint cid;
	guint retval;

	g_return_val_if_fail (BROWSER_IS_WINDOW (bwin), 0);
	g_return_val_if_fail (context, 0);
	g_return_val_if_fail (text, 0);
	
	cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (bwin->priv->statusbar), context);
	retval = gtk_statusbar_push (GTK_STATUSBAR (bwin->priv->statusbar), cid, text);
	if (auto_clear) {
		StatusData *sd;
		sd = g_new0 (StatusData, 1);
		sd->bwin = bwin;
		g_object_add_weak_pointer (G_OBJECT (sd->bwin), (gpointer*) &(sd->bwin));
		sd->cid = cid;
		sd->msgid = retval;
		g_timeout_add_seconds (5, (GSourceFunc) status_auto_pop_timeout, sd);
		return 0;
	}
	else
		return retval;
}

/**
 * browser_window_pop_status
 * @bwin: a #BrowserWindow
 * @context: textual description of what context the message is being used in
 *
 * Removes the first message in the @bwin's statusbar's stack with the given context.
 */
void
browser_window_pop_status (BrowserWindow *bwin, const gchar *context)
{
	guint cid;

	g_return_if_fail (BROWSER_IS_WINDOW (bwin));
	g_return_if_fail (context);

	cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (bwin->priv->statusbar), context);
	gtk_statusbar_pop (GTK_STATUSBAR (bwin->priv->statusbar), cid);
}

/**
 * browser_window_show_notice_printf
 * @bwin: a #BrowserWindow
 * @context: textual description of what context the message is being used in
 * @format: the text to display
 *
 * Make @bwin display a notice
 */
void
browser_window_show_notice_printf (BrowserWindow *bwin, GtkMessageType type, const gchar *context,
				   const gchar *format, ...)
{
	va_list args;
        gchar sz[2048];

	g_return_if_fail (BROWSER_IS_WINDOW (bwin));

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof (sz), format, args);
        va_end (args);
	browser_window_show_notice (bwin, type, context, sz);
}


static void
info_bar_response_cb (GtkInfoBar *ibar, G_GNUC_UNUSED gint response, BrowserWindow *bwin)
{
	bwin->priv->notif_widgets = g_slist_remove (bwin->priv->notif_widgets, ibar);	
	gtk_widget_destroy ((GtkWidget*) ibar);
}

/* hash table to remain which context notices have to be hidden: key=context, value=GINT_TO_POINTER (1) */
static GHashTable *hidden_contexts = NULL;
static void
hidden_contexts_foreach_save (const gchar *context, G_GNUC_UNUSED gint value, xmlNodePtr root)
{
	xmlNewChild (root, NULL, BAD_CAST "hide-notice", BAD_CAST context);
}

static void
hide_notice_toggled_cb (GtkToggleButton *toggle, gchar *context)
{
	g_assert (hidden_contexts);
	if (gtk_toggle_button_get_active (toggle))
		g_hash_table_insert (hidden_contexts, g_strdup (context), GINT_TO_POINTER (TRUE));
	else
		g_hash_table_remove (hidden_contexts, context);

	/* save config */
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *xml_contents;
	gint size;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "gda-browser-preferences");
	xmlDocSetRootElement (doc, root);
	g_hash_table_foreach (hidden_contexts, (GHFunc) hidden_contexts_foreach_save, root);
	xmlDocDumpFormatMemory (doc, &xml_contents, &size, 1);
	xmlFreeDoc (doc);

	/* save to disk */
	gchar *confdir, *conffile = NULL;
	GError *lerror = NULL;
	confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir(), "gda-browser", NULL);
	if (!g_file_test (confdir, G_FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (confdir, 0700)) {
			g_warning ("Can't create configuration directory '%s' to save preferences.",
				   confdir);
			goto out;
		}
	}
	conffile = g_build_filename (confdir, "preferences.xml", NULL);
	if (! g_file_set_contents (conffile, (gchar *) xml_contents, size, &lerror)) {
		g_warning ("Can't save preferences file '%s': %s", conffile,
			   lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
	}

 out:
	xmlFree (xml_contents);
	g_free (confdir);
	g_free (conffile);
}

static void
load_preferences (void)
{
	/* load preferences */
	xmlDocPtr doc;
	xmlNodePtr root, node;
	gchar *conffile;
	conffile = g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir(),
				 "gda-browser", "preferences.xml", NULL);
	if (!g_file_test (conffile, G_FILE_TEST_EXISTS)) {
		g_free (conffile);
		return;
	}

	doc = xmlParseFile (conffile);
	if (!doc) {
		g_warning ("Error loading preferences from file '%s'", conffile);
		g_free (conffile);
		return;
	}
	g_free (conffile);

	root = xmlDocGetRootElement (doc);
	for (node = root->children; node; node = node->next) {
		xmlChar *contents;
		if (strcmp ((gchar *) node->name, "hide-notice"))
			continue;
		contents = xmlNodeGetContent (node);
		if (contents) {
			g_hash_table_insert (hidden_contexts, g_strdup ((gchar*) contents),
					     GINT_TO_POINTER (TRUE));
			xmlFree (contents);
		}
	}
	xmlFreeDoc (doc);
}


/**
 * browser_window_show_notice
 * @bwin: a #BrowserWindow
 * @context: textual description of what context the message is being used in
 * @text: the information's text
 *
 * Makes @bwin display a notice
 */
void
browser_window_show_notice (BrowserWindow *bwin, GtkMessageType type, const gchar *context, const gchar *text)
{
	g_return_if_fail (BROWSER_IS_WINDOW (bwin));
	gboolean hide = FALSE;

	if ((type != GTK_MESSAGE_ERROR) && context) {
		if (!hidden_contexts) {
			hidden_contexts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
			load_preferences ();
		}
		hide = GPOINTER_TO_INT (g_hash_table_lookup (hidden_contexts, context));
	}

	if (hide) {
		gchar *ptr, *tmp;
		tmp = g_strdup (text);
		for (ptr = tmp; *ptr && (*ptr != '\n'); ptr++);
		if (*ptr) {
			*ptr = '.'; ptr++;
			*ptr = '.'; ptr++;
			*ptr = '.'; ptr++;
			*ptr = 0;
		}
		browser_window_push_status (bwin, "SupportNotice", tmp, TRUE);
		g_free (tmp);
	}
	else {
		if (bwin->priv->notif_widgets) {
			GSList *list;
			for (list = bwin->priv->notif_widgets; list; list = list->next) {
				const gchar *c1, *t1;
				c1 = g_object_get_data (G_OBJECT (list->data), "context");
				t1 = g_object_get_data (G_OBJECT (list->data), "text");
				if (((c1 && context && !strcmp (c1, context)) || (!c1 && !context)) &&
				    ((t1 && text && !strcmp (t1, text)) || (!t1 && !text)))
					break;
			}
			if (list) {
				GtkWidget *wid;
				wid = GTK_WIDGET (list->data);
				g_object_ref ((GObject*) wid);
				gtk_container_remove (GTK_CONTAINER (bwin->priv->notif_box), wid);
				gtk_box_pack_end (GTK_BOX (bwin->priv->notif_box), wid, TRUE, TRUE, 0);
				g_object_unref ((GObject*) wid);
				return;
			}
		}

		GtkWidget *cb = NULL;
		if (context && (type == GTK_MESSAGE_INFO)) {
			cb = gtk_check_button_new_with_label (_("Don't show this message again"));
			g_signal_connect_data (cb, "toggled",
					       G_CALLBACK (hide_notice_toggled_cb), g_strdup (context),
					       (GClosureNotify) g_free, 0);
		}

		/* use a GtkInfoBar */
		GtkWidget *ibar, *content_area, *label;
		
		ibar = gtk_info_bar_new_with_buttons (_("_Close"), 1, NULL);
		if (context)
			g_object_set_data_full (G_OBJECT (ibar), "context", g_strdup (context), g_free);
		if (text)
			g_object_set_data_full (G_OBJECT (ibar), "text", g_strdup (text), g_free);
		gtk_info_bar_set_message_type (GTK_INFO_BAR (ibar), type);
		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), text);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (ibar));
		if (cb) {
			GtkWidget *box;
			box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
			gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (box), cb, FALSE, FALSE, 0);
			gtk_container_add (GTK_CONTAINER (content_area), box);
			gtk_widget_show_all (box);
		}
		else {
			gtk_container_add (GTK_CONTAINER (content_area), label);
			gtk_widget_show (label);
		}
		g_signal_connect (ibar, "response",
				  G_CALLBACK (info_bar_response_cb), bwin);
		gtk_box_pack_end (GTK_BOX (bwin->priv->notif_box), ibar, TRUE, TRUE, 0);
		bwin->priv->notif_widgets = g_slist_append (bwin->priv->notif_widgets, ibar);
		if (g_slist_length (bwin->priv->notif_widgets) > 2) {
			gtk_widget_destroy (GTK_WIDGET (bwin->priv->notif_widgets->data));
			bwin->priv->notif_widgets = g_slist_delete_link (bwin->priv->notif_widgets,
									 bwin->priv->notif_widgets);
		}
		gtk_widget_show (ibar);
	}
}

/**
 * browser_window_change_perspective
 * @bwin: a #BrowserWindow
 * @perspective_id: the ID of the perspective to change to
 *
 * Make @bwin switch to the perspective named @perspective
 *
 * Returns: a pointer to the #BrowserPerspective, or %NULL if not found
 */
BrowserPerspective *
browser_window_change_perspective (BrowserWindow *bwin, const gchar *perspective_id)
{
	BrowserPerspectiveFactory *bpf;
	BrowserPerspective *bpers = NULL;
	PerspectiveData *current_pdata;

	g_return_val_if_fail (BROWSER_IS_WINDOW (bwin), NULL);
	g_return_val_if_fail (perspective_id, NULL);

	current_pdata = bwin->priv->current_perspective;
	if (current_pdata) {
		if (!strcmp (current_pdata->factory->id, perspective_id))
			/* nothing to do, keep same perspective shown */
			return current_pdata->perspective_widget;

		/* clean ups for current perspective */
		browser_perspective_uncustomize (bwin->priv->current_perspective->perspective_widget);
	}

	/* find factory */
	bpf = browser_get_factory (perspective_id);
	if (!bpf) {
		g_warning ("Could not identify perspective %s", perspective_id);
		return NULL;
	}

	/* check if perspective already exists */
	PerspectiveData *pdata = NULL;
	bpers = (BrowserPerspective*) gtk_stack_get_child_by_name (bwin->priv->pers_stack, perspective_id);
	if (! bpers) {
		pdata = perspective_data_new (bwin, bpf);
                bwin->priv->pers_list = g_slist_prepend (bwin->priv->pers_list, pdata);
                gtk_stack_add_named (bwin->priv->pers_stack, GTK_WIDGET (pdata->perspective_widget),
				     bpf->id);
		bpers = pdata->perspective_widget;
		gtk_widget_show_all (GTK_WIDGET (pdata->perspective_widget));
	}
	else {
		GSList *list;
		for (list = bwin->priv->pers_list; list; list = list->next) {
			if (PERSPECTIVE_DATA (list->data)->factory == bpf) {
				pdata = PERSPECTIVE_DATA (list->data);
				break;
			}
		}
	}

	/* update GAction's state */
	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "change-perspective");
	GVariant *value;
	value = g_variant_new_string (perspective_id);
	g_simple_action_set_state (G_SIMPLE_ACTION (action), value);

	/* actually switch displayed page */
	gtk_stack_set_visible_child (bwin->priv->pers_stack, GTK_WIDGET (bpers));
	browser_perspective_customize (bpers,
				       bwin->priv->toolbar, bwin->priv->header);
	bwin->priv->current_perspective = pdata;

	/* notice of perspective change */
	gchar *tmp;
	tmp = g_markup_printf_escaped (_("The current perspective has changed to the '%s' perspective, you "
					 "can switch back to previous perspective through the "
					 "'Perspective/%s' menu, or using the '%s' shortcut"),
				       bwin->priv->current_perspective->factory->perspective_name,
				       current_pdata->factory->perspective_name,
				       current_pdata->factory->menu_shortcut);
	browser_window_show_notice (bwin, GTK_MESSAGE_INFO, "Perspective change", tmp);
	g_free (tmp);

	return bpers;
}

/**
 * browser_window_is_fullscreen
 * @bwin: a #BrowserWindow
 *
 * Returns: %TRUE if @bwin is fullscreen
 */
gboolean
browser_window_is_fullscreen (BrowserWindow *bwin)
{
	g_return_val_if_fail (BROWSER_IS_WINDOW (bwin), FALSE);
	return bwin->priv->fullscreen;
}

/**
 * browser_window_set_fullscreen
 * @bwin: a #BrowserWindow
 * @fullscreen:
 *
 * Requires @bwin to be fullscreen if @fullscreen is %TRUE
 */
void
browser_window_set_fullscreen (BrowserWindow *bwin, gboolean fullscreen)
{
	g_return_if_fail (BROWSER_IS_WINDOW (bwin));

	GAction *action;
	action = g_action_map_lookup_action (G_ACTION_MAP (bwin), "fullscreen");

	GVariant *value;
	value = g_variant_new_boolean (fullscreen);
	g_action_change_state (action, value);
}
