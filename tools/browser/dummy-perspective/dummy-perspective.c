/*
 * Copyright (C) 2009 - 2011 Vivien Malerba
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
#include "dummy-perspective.h"

/* 
 * Main static functions 
 */
static void dummy_perspective_class_init (DummyPerspectiveClass *klass);
static void dummy_perspective_init (DummyPerspective *pers);
static void dummy_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 dummy_perspective_perspective_init (BrowserPerspectiveIface *iface);
static BrowserWindow       *dummy_perspective_get_window (BrowserPerspective *perspective);
static GtkActionGroup      *dummy_perspective_get_actions_group (BrowserPerspective *perspective);
static const gchar         *dummy_perspective_get_actions_ui (BrowserPerspective *perspective);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


GType
dummy_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		
		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_VBOX, "DummyPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_static_mutex_unlock (&registering);
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
	iface->i_get_window = dummy_perspective_get_window;
	iface->i_get_actions_group = dummy_perspective_get_actions_group;
	iface->i_get_actions_ui = dummy_perspective_get_actions_ui;
}


static void
dummy_perspective_init (DummyPerspective *perspective)
{
	GtkWidget *wid;
	
	wid = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (wid),
			      "<big><b>Dummy perspective</b></big>\n"
			      "Use this as a starting point for your own perspective...");
	gtk_box_pack_start (GTK_BOX (perspective), wid, TRUE, TRUE, 0);
	gtk_widget_show (wid);
}

/**
 * dummy_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
dummy_perspective_new (G_GNUC_UNUSED BrowserWindow *bwin)
{
	BrowserPerspective *bpers;
	bpers = (BrowserPerspective*) g_object_new (TYPE_DUMMY_PERSPECTIVE, NULL);

	/* if there is a notebook to store pages, use: 
	browser_perspective_declare_notebook (bpers, GTK_NOTEBOOK (perspective->priv->notebook));
	*/

	return bpers;
}


static void
dummy_perspective_dispose (GObject *object)
{
	DummyPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DUMMY_PERSPECTIVE (object));

	perspective = DUMMY_PERSPECTIVE (object);
	browser_perspective_declare_notebook ((BrowserPerspective*) perspective, NULL);

	/* parent class */
	parent_class->dispose (object);
}

static void
dummy_add_cb (G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED BrowserPerspective *bpers)
{
	g_print ("Add something...\n");
}

static void
dummy_list_cb (G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED BrowserPerspective *bpers)
{
	g_print ("List something...\n");
}

static GtkActionEntry ui_actions[] = {
        { "DummyMenu", NULL, "_Dummy", NULL, "DummyMenu", NULL },
        { "DummyItem1", GTK_STOCK_ADD, "_Dummy add", NULL, "Add something",
          G_CALLBACK (dummy_add_cb)},
        { "DummyItem2", GTK_STOCK_REMOVE, "_Dummy list", NULL, "List something",
          G_CALLBACK (dummy_list_cb)},
};

static const gchar *ui_actions_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='Dummy' action='DummyMenu'>"
        "        <menuitem name='DummyItem1' action= 'DummyItem1'/>"
        "        <menuitem name='DummyItem2' action= 'DummyItem2'/>"
        "      </menu>"
	"    </placeholder>"
        "  </menubar>"
        "  <toolbar name='ToolBar'>"
        "    <separator/>"
        "    <toolitem action='DummyItem1'/>"
        "    <toolitem action='DummyItem2'/>"
        "  </toolbar>"
        "</ui>";

static GtkActionGroup *
dummy_perspective_get_actions_group (BrowserPerspective *bpers)
{
	GtkActionGroup *agroup;
	agroup = gtk_action_group_new ("DummyActions");
	gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), bpers);
	
	return agroup;
}

static const gchar *
dummy_perspective_get_actions_ui (G_GNUC_UNUSED BrowserPerspective *bpers)
{
	return ui_actions_info;
}

static BrowserWindow *
dummy_perspective_get_window (BrowserPerspective *perspective)
{
	DummyPerspective *bpers;
	bpers = DUMMY_PERSPECTIVE (perspective);
	return NULL;/*bpers->priv->bwin;*/
}
