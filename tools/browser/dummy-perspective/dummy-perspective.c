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
#include "../ui-customize.h"
#include "dummy-perspective.h"

struct _DummyPerspectivePriv {
	GtkWidget *notebook;
};
/* 
 * Main static functions 
 */
static void dummy_perspective_class_init (DummyPerspectiveClass *klass);
static void dummy_perspective_init (DummyPerspective *pers);
static void dummy_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 dummy_perspective_perspective_init (BrowserPerspectiveIface *iface);
static GtkWidget           *dummy_perspective_get_notebook (BrowserPerspective *perspective);
static void                 dummy_perspective_customize (BrowserPerspective *perspective,
							 GtkToolbar *toolbar, GtkHeaderBar *header);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


GType
dummy_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (DummyPerspectiveClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) dummy_perspective_class_init,
			NULL,
			NULL,
			sizeof (DummyPerspective),
			0,
			(GInstanceInitFunc) dummy_perspective_init,
			0
		};

		static GInterfaceInfo perspective_info = {
                        (GInterfaceInitFunc) dummy_perspective_perspective_init,
			NULL,
                        NULL
                };
		
		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "DummyPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
dummy_perspective_class_init (DummyPerspectiveClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = dummy_perspective_dispose;
}

static void
dummy_perspective_perspective_init (BrowserPerspectiveIface *iface)
{
	iface->i_get_notebook = dummy_perspective_get_notebook;
	iface->i_customize = dummy_perspective_customize;
	iface->i_uncustomize = NULL;
}


static void
dummy_perspective_init (DummyPerspective *perspective)
{
	GtkWidget *wid, *nb;

	perspective->priv = g_new0 (DummyPerspectivePriv, 1);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);

	nb = browser_perspective_create_notebook (BROWSER_PERSPECTIVE (perspective));
	perspective->priv->notebook = nb;
	gtk_box_pack_start (GTK_BOX (perspective), nb, TRUE, TRUE, 0);
	
	wid = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (wid),
			      "<big><b>Dummy perspective</b></big>\n"
			      "Use this as a starting point for your own perspective...");
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, NULL);

	gtk_widget_show_all (nb);
}

/**
 * dummy_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
dummy_perspective_new (BrowserWindow *bwin)
{
	BrowserPerspective *bpers;
	bpers = (BrowserPerspective*) g_object_new (TYPE_DUMMY_PERSPECTIVE, NULL);

	return bpers;
}


static void
dummy_perspective_dispose (GObject *object)
{
	DummyPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DUMMY_PERSPECTIVE (object));

	perspective = DUMMY_PERSPECTIVE (object);
	if (perspective->priv) {
		if (customization_data_exists (object))
			customization_data_release (object);

		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static GtkWidget *
dummy_perspective_get_notebook (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_DUMMY_PERSPECTIVE (perspective), NULL);
	return DUMMY_PERSPECTIVE (perspective)->priv->notebook;
}

static void
dummy1_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	/* @data is a DummyPerspective */
	if (g_variant_get_boolean (state))
		g_print ("%s () called, checked\n", __FUNCTION__);
	else
		g_print ("%s () called, not checked\n", __FUNCTION__);
	g_simple_action_set_state (action, state);
}

static void
dummy2_cb (G_GNUC_UNUSED GSimpleAction *action, GVariant *state, gpointer data)
{
	/* @data is a DummyPerspective */
	g_print ("%s () called\n", __FUNCTION__);
}

static GActionEntry win_entries[] = {
	{ "dummy1", NULL, NULL, "false", dummy1_cb },
	{ "dummy2", dummy2_cb, NULL, NULL, NULL },
};

static void
dummy_perspective_customize (BrowserPerspective *perspective, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	customization_data_init (G_OBJECT (perspective), toolbar, header);

	/* add perspective's actions */
	customization_data_add_actions (G_OBJECT (perspective), win_entries, G_N_ELEMENTS (win_entries));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "send-to-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), "Dummy 1 option");
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.dummy1");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "process-stop-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), "Dummy 2 action");
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.dummy2");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));

	/* add to header bar */
	GtkWidget *img, *button;
	button = gtk_menu_button_new ();
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (button));
	img = gtk_image_new_from_icon_name ("start-here-symbolic", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (button), img);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);
	gtk_widget_show_all (button);

	GMenu *smenu;
	smenu = g_menu_new ();
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (smenu));

	GMenuItem *mitem;
	mitem = g_menu_item_new ("Dummy 1", "win.dummy1");
	g_menu_insert_item (smenu, -1, mitem);
	mitem = g_menu_item_new ("Dummy 2", "win.dummy2");
	g_menu_insert_item (smenu, -1, mitem);
}
