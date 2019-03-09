/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <gtk/gtk.h>
#include <libgda/gda-tree.h>
#include "objects-index.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../ui-support.h"
#include "../gdaui-bar.h"
#include "marshal.h"
#include <gdk/gdkkeysyms.h>
#include "../objects-cloud.h"

struct _ObjectsIndexPrivate {
	TConnection *tcnc;
	ObjectsCloud      *cloud;
};

static void objects_index_class_init (ObjectsIndexClass *klass);
static void objects_index_init       (ObjectsIndex *index,
				       ObjectsIndexClass *klass);
static void objects_index_dispose   (GObject *object);

static void meta_changed_cb (TConnection *tcnc, GdaMetaStruct *mstruct, ObjectsIndex *index);

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
objects_index_init (ObjectsIndex *index, G_GNUC_UNUSED ObjectsIndexClass *klass)
{
	index->priv = g_new0 (ObjectsIndexPrivate, 1);
	index->priv->tcnc = NULL;
	index->priv->cloud = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (index), GTK_ORIENTATION_VERTICAL);
}

static void
objects_index_dispose (GObject *object)
{
	ObjectsIndex *index = (ObjectsIndex *) object;

	/* free memory */
	if (index->priv) {
		if (index->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (index->priv->tcnc,
							      G_CALLBACK (meta_changed_cb), index);
			g_object_unref (index->priv->tcnc);
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
			(GInstanceInitFunc) objects_index_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "ObjectsIndex",
					       &info, 0);
	}
	return type;
}

static void
cloud_object_selected_cb (G_GNUC_UNUSED ObjectsCloud *sel, G_GNUC_UNUSED ObjectsCloudObjType sel_type,
			  const gchar *sel_contents, ObjectsIndex *index)
{
	/* FIXME: adjust with sel->priv->type */
	g_signal_emit (index, objects_index_signals [SELECTION_CHANGED], 0,
		       T_FAVORITES_TABLES, sel_contents);
}

static void
find_changed_cb (GtkEntry *entry, ObjectsIndex *index)
{
	objects_cloud_filter (index->priv->cloud, gtk_entry_get_text (entry));
}

/**
 * objects_index_new:
 *
 *
 *
 * Returns:
 */
GtkWidget *
objects_index_new (TConnection *tcnc)
{
	ObjectsIndex *index;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	index = OBJECTS_INDEX (g_object_new (OBJECTS_INDEX_TYPE, NULL));

	index->priv->tcnc = g_object_ref (tcnc);
	g_signal_connect (index->priv->tcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), index);

	/* header */
	GtkWidget *hbox, *wid;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (index), hbox, FALSE, FALSE, 0);

	GtkWidget *label;
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>\n", _("Index of tables and views"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        gtk_widget_show (label);

	objects_index_update (index);
	/* search entry */
	wid = gdaui_bar_add_search_entry (GDAUI_BAR (label));

	g_signal_connect (wid, "changed",
			  G_CALLBACK (find_changed_cb), index);

	return (GtkWidget*) index;
}

void
objects_index_update (ObjectsIndex *index)
{
	GdaMetaStruct *mstruct;
	GtkWidget *cloud;
	if (index->priv->cloud != NULL) {
		g_signal_handlers_disconnect_by_func (GTK_WIDGET (index->priv->cloud),
		                                      G_CALLBACK (cloud_object_selected_cb), index);
		gtk_widget_destroy (GTK_WIDGET (index->priv->cloud));
		index->priv->cloud = NULL;
		g_message ("Destroied cloud object");
	}

	mstruct = t_connection_get_meta_struct (index->priv->tcnc);
	cloud = objects_cloud_new (mstruct, OBJECTS_CLOUD_TYPE_TABLE);
	objects_cloud_show_schemas (OBJECTS_CLOUD (cloud), TRUE);
	gtk_box_pack_start (GTK_BOX (index), cloud, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (index));
	index->priv->cloud = OBJECTS_CLOUD (cloud);
	g_signal_connect (cloud, "selected",
			  G_CALLBACK (cloud_object_selected_cb), index);
}

static void
meta_changed_cb (G_GNUC_UNUSED TConnection *tcnc, GdaMetaStruct *mstruct, ObjectsIndex *index)
{
	objects_cloud_set_meta_struct (index->priv->cloud, mstruct);
}
