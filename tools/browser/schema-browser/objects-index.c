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
#include <libgda/gda-tree.h>
#include "objects-index.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../support.h"
#include "../cc-gray-bar.h"
#include "marshal.h"
#include <gdk/gdkkeysyms.h>
#include "../common/popup-container.h"
#include "../common/objects-cloud.h"

struct _ObjectsIndexPrivate {
	BrowserConnection *bcnc;
	ObjectsCloud      *cloud;
	GtkWidget         *popup_container;
};

static void objects_index_class_init (ObjectsIndexClass *klass);
static void objects_index_init       (ObjectsIndex *index,
				       ObjectsIndexClass *klass);
static void objects_index_dispose   (GObject *object);

static void meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, ObjectsIndex *index);

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint objects_index_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;


/*
 * ObjectsIndex class implementation
 */

static void
objects_index_class_init (ObjectsIndexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	objects_index_signals [SELECTION_CHANGED] =
                g_signal_new ("selection-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (ObjectsIndexClass, selection_changed),
                              NULL, NULL,
                              _sb_marshal_VOID__ENUM_STRING, G_TYPE_NONE,
                              2, G_TYPE_UINT, G_TYPE_STRING);
	klass->selection_changed = NULL;

	object_class->dispose = objects_index_dispose;
}


static void
objects_index_init (ObjectsIndex *index, ObjectsIndexClass *klass)
{
	index->priv = g_new0 (ObjectsIndexPrivate, 1);
}

static void
objects_index_dispose (GObject *object)
{
	ObjectsIndex *index = (ObjectsIndex *) object;

	/* free memory */
	if (index->priv) {
		gtk_widget_destroy (index->priv->popup_container);
		if (index->priv->bcnc) {
			g_signal_handlers_disconnect_by_func (index->priv->bcnc,
							      G_CALLBACK (meta_changed_cb), index);
			g_object_unref (index->priv->bcnc);
		}
		g_free (index->priv);
		index->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
objects_index_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (ObjectsIndexClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) objects_index_class_init,
			NULL,
			NULL,
			sizeof (ObjectsIndex),
			0,
			(GInstanceInitFunc) objects_index_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "ObjectsIndex",
					       &info, 0);
	}
	return type;
}

static void
cloud_object_selected_cb (ObjectsCloud *sel, ObjectsCloudObjType sel_type, 
			  const gchar *sel_contents, ObjectsIndex *index)
{
	/* FIXME: adjust with sel->priv->type */
	g_signal_emit (index, objects_index_signals [SELECTION_CHANGED], 0,
		       BROWSER_FAVORITES_TABLES, sel_contents);
}

/**
 * objects_index_new
 *
 *
 *
 * Returns:
 */
GtkWidget *
objects_index_new (BrowserConnection *bcnc)
{
	ObjectsIndex *index;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	index = OBJECTS_INDEX (g_object_new (OBJECTS_INDEX_TYPE, NULL));

	index->priv->bcnc = g_object_ref (bcnc);
	g_signal_connect (index->priv->bcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), index);

	/* header */
	GtkWidget *hbox, *wid;
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (index), hbox, FALSE, FALSE, 0);

	GtkWidget *label;
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>", _("Tables' index"));
	label = cc_gray_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        gtk_widget_show (label);

	/* cloud */
	GdaMetaStruct *mstruct;
	GtkWidget *cloud;
	mstruct = browser_connection_get_meta_struct (index->priv->bcnc);
	cloud = objects_cloud_new (mstruct, OBJECTS_CLOUD_TYPE_TABLE);
	objects_cloud_show_schemas (OBJECTS_CLOUD (cloud), TRUE);
	gtk_box_pack_start (GTK_BOX (index), cloud, TRUE, TRUE, 0);
	index->priv->cloud = OBJECTS_CLOUD (cloud);
	g_signal_connect (cloud, "selected",
			  G_CALLBACK (cloud_object_selected_cb), index);

	/* find button */
	wid = gtk_button_new ();
	label = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (wid), label);
	gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
	g_object_set (G_OBJECT (wid), "label", NULL, NULL);
	
	GtkWidget *popup;
	popup = popup_container_new (wid);
	index->priv->popup_container = popup;
	g_signal_connect_swapped (wid, "clicked",
				  G_CALLBACK (gtk_widget_show), popup);
	g_object_set_data (G_OBJECT (popup), "button", wid);

	wid = objects_cloud_create_filter (OBJECTS_CLOUD (cloud));
	gtk_container_add (GTK_CONTAINER (popup), wid);
	gtk_widget_show (wid);

	return (GtkWidget*) index;
}

static void
meta_changed_cb (BrowserConnection *bcnc, GdaMetaStruct *mstruct, ObjectsIndex *index)
{
	objects_cloud_set_meta_struct (index->priv->cloud, mstruct);
}
