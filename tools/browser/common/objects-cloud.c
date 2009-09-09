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
#include "objects-cloud.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../support.h"
#include "../cc-gray-bar.h"
#include "marshal.h"
#include <gdk/gdkkeysyms.h>
#include "popup-container.h"

struct _ObjectsCloudPrivate {
	gboolean            show_schemas;
	ObjectsCloudObjType type;
	GdaMetaStruct       *mstruct;
	GtkTextBuffer       *tbuffer;
	GtkWidget           *tview;

	gboolean             hovering_over_link;
};

static void objects_cloud_class_init (ObjectsCloudClass *klass);
static void objects_cloud_init       (ObjectsCloud *cloud,
				      ObjectsCloudClass *klass);
static void objects_cloud_dispose   (GObject *object);

enum {
	SELECTED,
	LAST_SIGNAL
};

static guint objects_cloud_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;


/*
 * ObjectsCloud class implementation
 */

static void
objects_cloud_class_init (ObjectsCloudClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	objects_cloud_signals [SELECTED] =
                g_signal_new ("selected",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (ObjectsCloudClass, selected),
                              NULL, NULL,
                              _common_marshal_VOID__ENUM_STRING, G_TYPE_NONE,
                              2, G_TYPE_UINT, G_TYPE_STRING);
	klass->selected = NULL;

	object_class->dispose = objects_cloud_dispose;
}


static void
objects_cloud_init (ObjectsCloud *cloud, ObjectsCloudClass *klass)
{
	cloud->priv = g_new0 (ObjectsCloudPrivate, 1);
	cloud->priv->show_schemas = FALSE;
	cloud->priv->tbuffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_create_tag (cloud->priv->tbuffer, "section",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);
}

static void
objects_cloud_dispose (GObject *object)
{
	ObjectsCloud *cloud = (ObjectsCloud *) object;

	/* free memory */
	if (cloud->priv) {
		if (cloud->priv->mstruct)
			g_object_unref (cloud->priv->mstruct);
		g_object_unref (cloud->priv->tbuffer);
		g_free (cloud->priv);
		cloud->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
objects_cloud_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (ObjectsCloudClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) objects_cloud_class_init,
			NULL,
			NULL,
			sizeof (ObjectsCloud),
			0,
			(GInstanceInitFunc) objects_cloud_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "ObjectsCloud",
					       &info, 0);
	}
	return type;
}

typedef struct {
	gchar *schema;
	GtkTextMark *mark;
} SchemaData;
static void add_to_schema_data (ObjectsCloud *cloud, SchemaData *sd, GdaMetaDbObject *dbo);
static void
update_display (ObjectsCloud *cloud)
{
	gchar *str;
	GSList *schemas = NULL; /* holds pointers to @SchemaData structures */
	SchemaData *default_sd = NULL, *sd;
	
	GtkTextBuffer *tbuffer;
        GtkTextIter start, end;

        /* clean all */
        tbuffer = cloud->priv->tbuffer;
        gtk_text_buffer_get_start_iter (tbuffer, &start);
        gtk_text_buffer_get_end_iter (tbuffer, &end);
        gtk_text_buffer_delete (tbuffer, &start, &end);

	default_sd = g_new0 (SchemaData, 1);
	default_sd->schema = NULL;
	default_sd->mark = gtk_text_mark_new (NULL, TRUE);
	gtk_text_buffer_get_end_iter (tbuffer, &end);
	
	gtk_text_buffer_get_end_iter (tbuffer, &end);
	if (cloud->priv->show_schemas) {
		gtk_text_buffer_insert_pixbuf (tbuffer, &end,
					       browser_get_pixbuf_icon (BROWSER_ICON_TABLE));
		gtk_text_buffer_insert (tbuffer, &end, " ", 1);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &end,
							  _("Tables:"), -1, "section", NULL);
		gtk_text_buffer_insert (tbuffer, &end, "\n\n", 2);
	}
	gtk_text_buffer_add_mark (tbuffer, default_sd->mark, &end);


	GdaMetaStruct *mstruct;
	GSList *dbo_list, *list;
	mstruct = cloud->priv->mstruct;
	if (!mstruct) {
		/* nothing to display */
		return;
	}
	dbo_list = g_slist_reverse (gda_meta_struct_get_all_db_objects (mstruct));
	for (list = dbo_list; list; list = list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		GSList *list;
		gboolean is_default;

		if (dbo->obj_type != GDA_META_DB_TABLE)
			continue;
		g_assert (dbo->obj_schema);

		is_default = strcmp (dbo->obj_short_name, dbo->obj_full_name) ? TRUE : FALSE;
		sd = NULL;
		if (cloud->priv->show_schemas) {
			for (list = schemas; list; list = list->next) {
				if (!strcmp (((SchemaData *) list->data)->schema, dbo->obj_schema)) {
					sd = (SchemaData *) list->data;
					break;
				}
			}
			if (!sd) {
				sd = g_new0 (SchemaData, 1);
				sd->schema = g_strdup (dbo->obj_schema);
				sd->mark = gtk_text_mark_new (NULL, TRUE);
				
				gtk_text_buffer_get_end_iter (tbuffer, &end);
				gtk_text_buffer_insert (tbuffer, &end, "\n\n", 2);
				gtk_text_buffer_insert_pixbuf (tbuffer, &end,
							       browser_get_pixbuf_icon (BROWSER_ICON_TABLE));
				gtk_text_buffer_insert (tbuffer, &end, " ", 1);
				str = g_strdup_printf (_("Tables in schema '%s':"), sd->schema);
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &end,
									  str, -1, "section", NULL);
				g_free (str);
				gtk_text_buffer_insert (tbuffer, &end, "\n\n", 2);
				
				gtk_text_buffer_add_mark (tbuffer, sd->mark, &end);
				
				schemas = g_slist_append (schemas, sd);
			}

			add_to_schema_data (cloud, sd, dbo);
		}
		if (is_default)
			add_to_schema_data (cloud, default_sd, dbo);
	}
	g_slist_free (dbo_list);

	if (default_sd)
		schemas = g_slist_prepend (schemas, default_sd);

	/* get rid of the SchemaData structures */
	for (list = schemas; list; list = list->next) {
		sd = (SchemaData*) list->data;

		g_free (sd->schema);
		g_object_unref (sd->mark);
		g_free (sd);
	}
}

static void
add_to_schema_data (ObjectsCloud *cloud, SchemaData *sd, GdaMetaDbObject *dbo)
{
	GtkTextTag *tag;
	GtkTextIter iter;
	gdouble scale = 1.0;

	if (dbo->obj_type == GDA_META_DB_TABLE)
		scale = 1.5 + g_slist_length (dbo->depend_list) / 5.;

	gtk_text_buffer_get_iter_at_mark (cloud->priv->tbuffer, &iter, sd->mark);
	tag = gtk_text_buffer_create_tag (cloud->priv->tbuffer, NULL, 
					  "foreground", "#6161F2", 
					  //"underline", PANGO_UNDERLINE_SINGLE,
					  "scale", scale,
					  NULL);
	g_object_set_data_full (G_OBJECT (tag), "dbo_obj_schema", g_strdup (dbo->obj_schema), g_free);
	g_object_set_data_full (G_OBJECT (tag), "dbo_obj_name", g_strdup (dbo->obj_name), g_free);
	g_object_set_data_full (G_OBJECT (tag), "dbo_obj_short_name", g_strdup (dbo->obj_short_name), g_free);

	gtk_text_buffer_insert_with_tags (cloud->priv->tbuffer, &iter, dbo->obj_name, -1,
					  tag, NULL);
	gtk_text_buffer_insert (cloud->priv->tbuffer, &iter, " ", -1);
}

static gboolean key_press_event (GtkWidget *text_view, GdkEventKey *event, ObjectsCloud *cloud);
static gboolean event_after (GtkWidget *text_view, GdkEvent *ev, ObjectsCloud *cloud);
static gboolean motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, ObjectsCloud *cloud);
static gboolean visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, ObjectsCloud *cloud);

static void
text_tag_table_foreach_cb (GtkTextTag *tag, const gchar *find)
{
	const gchar *name;

	name = g_object_get_data (G_OBJECT (tag), "dbo_obj_name");

	if (!name)
		return;
	
	if (!*find) {
		g_object_set (tag, "foreground", "#6161F2", NULL);
	}
	else {
		gchar *ptr;
		gchar *lcname, *lcfind;
		lcname = g_utf8_strdown (name, -1);
		lcfind = g_utf8_strdown (find, -1);

		ptr = strstr (lcname, lcfind);
		if (!ptr) {
			/* string not present in name */
			g_object_set (tag, "foreground", "#DBDBDB", NULL);
		}
		else if ((ptr == lcname) ||
			 ((*name == '"') && (ptr == lcname+1))) {
			/* string present as start of name */
			g_object_set (tag, "foreground", "#6161F2", NULL);
		}
		else {
			/* string present in name but not at the start */
			g_object_set (tag, "foreground", "#A0A0A0", NULL);
		}

		g_free (lcname);
		g_free (lcfind);
	}
}

void
objects_cloud_filter (ObjectsCloud *cloud, const gchar *filter)
{
	g_return_if_fail (IS_OBJECTS_CLOUD (cloud));

	GtkTextTagTable *tags_table = gtk_text_buffer_get_tag_table (cloud->priv->tbuffer);

	gtk_text_tag_table_foreach (tags_table, (GtkTextTagTableForeach) text_tag_table_foreach_cb,
				    (gpointer) filter);
}

/**
 * objects_cloud_new
 *
 *
 *
 * Returns:
 */
GtkWidget *
objects_cloud_new (GdaMetaStruct *mstruct, ObjectsCloudObjType type)
{
	ObjectsCloud *cloud;

	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), NULL);
	cloud = OBJECTS_CLOUD (g_object_new (OBJECTS_CLOUD_TYPE, NULL));

	if (mstruct)
		cloud->priv->mstruct = g_object_ref (mstruct);
	cloud->priv->type = type;

	/* text contents */
	GtkWidget *sw, *vbox;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (cloud), sw, TRUE, TRUE, 0);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);

	cloud->priv->tview = gtk_text_view_new_with_buffer (cloud->priv->tbuffer);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (cloud->priv->tview), GTK_WRAP_WORD);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (cloud->priv->tview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (cloud->priv->tview), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), cloud->priv->tview, TRUE, TRUE, 0);
	gtk_widget_show_all (sw);

	g_signal_connect (cloud->priv->tview, "key-press-event", 
			  G_CALLBACK (key_press_event), cloud);
	g_signal_connect (cloud->priv->tview, "event-after", 
			  G_CALLBACK (event_after), cloud);
	g_signal_connect (cloud->priv->tview, "motion-notify-event", 
			  G_CALLBACK (motion_notify_event), cloud);
	g_signal_connect (cloud->priv->tview, "visibility-notify-event", 
			  G_CALLBACK (visibility_notify_event), cloud);

	/* initial update */
	update_display (cloud);

	return (GtkWidget*) cloud;
}

/**
 * objects_cloud_set_meta_struct
 *
 */
void
objects_cloud_set_meta_struct (ObjectsCloud *cloud, GdaMetaStruct *mstruct)
{
	g_return_if_fail (IS_OBJECTS_CLOUD (cloud));
	g_return_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct));

	if (cloud->priv->mstruct) {
		g_object_unref (cloud->priv->mstruct);
		cloud->priv->mstruct = NULL;
	}
	if (mstruct)
		cloud->priv->mstruct = g_object_ref (mstruct);
	update_display (cloud);
}

/**
 * objects_cloud_show_schemas
 */
void
objects_cloud_show_schemas (ObjectsCloud *cloud, gboolean show_schemas)
{
	g_return_if_fail (IS_OBJECTS_CLOUD (cloud));

	cloud->priv->show_schemas = show_schemas;
	update_display (cloud);
}

static void
find_entry_changed_cb (GtkWidget *entry, ObjectsCloud *cloud)
{
	gchar *find = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	objects_cloud_filter (cloud, find);
	g_free (find);
}

/**
 * objects_cloud_create_filter
 */
GtkWidget *
objects_cloud_create_filter (ObjectsCloud *cloud)
{
	GtkWidget *hbox, *label, *wid;
	g_return_val_if_fail (IS_OBJECTS_CLOUD (cloud), NULL);
	
	hbox = gtk_hbox_new (FALSE, 0);

	label = gtk_label_new (_("Find:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	wid = gtk_entry_new ();
	g_signal_connect (wid, "changed",
			  G_CALLBACK (find_entry_changed_cb), cloud);
	gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);
	gtk_widget_hide (hbox);

	return hbox;
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;

/* Looks at all tags covering the position (x, y) in the text view, 
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView *text_view, gint x, gint y, ObjectsCloud *cloud)
{
	GSList *tags = NULL, *tagp = NULL;
	GtkTextIter iter;
	gboolean hovering = FALSE;
	
	gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
	
	tags = gtk_text_iter_get_tags (&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		gchar *table_name;
		
		table_name = g_object_get_data (G_OBJECT (tag), "dbo_obj_name");
		if (table_name) {
			hovering = TRUE;
			break;
		}
	}
	
	if (hovering != cloud->priv->hovering_over_link) {
		cloud->priv->hovering_over_link = hovering;
		
		if (cloud->priv->hovering_over_link) {
			if (! hand_cursor)
				hand_cursor = gdk_cursor_new (GDK_HAND2);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       hand_cursor);
		}
		else {
			if (!regular_cursor)
				regular_cursor = gdk_cursor_new (GDK_XTERM);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       regular_cursor);
		}
	}
	
	if (tags) 
		g_slist_free (tags);
}

/* 
 * Also update the cursor image if the window becomes visible
 * (e.g. when a window covering it got iconified).
 */
static gboolean
visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, ObjectsCloud *cloud)
{
	gint wx, wy, bx, by;
	
	gdk_window_get_pointer (text_view->window, &wx, &wy, NULL);
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       wx, wy, &bx, &by);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), bx, by, cloud);
	
	return FALSE;
}

/*
 * Update the cursor image if the pointer moved. 
 */
static gboolean
motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, ObjectsCloud *cloud)
{
	gint x, y;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), x, y, cloud);
	
	gdk_window_get_pointer (text_view->window, NULL, NULL, NULL);
	return FALSE;
}


/* Looks at all tags covering the position of iter in the text view, 
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (GtkWidget *text_view, GtkTextIter *iter, ObjectsCloud *cloud)
{
	GSList *tags = NULL, *tagp = NULL;
	
	tags = gtk_text_iter_get_tags (iter);
	for (tagp = tags;  tagp;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		const gchar *schema;
		
		schema = g_object_get_data (G_OBJECT (tag), "dbo_obj_schema");
		if (schema) {
			const gchar *objects_name, *short_name;
		
			objects_name = g_object_get_data (G_OBJECT (tag), "dbo_obj_name");
			short_name = g_object_get_data (G_OBJECT (tag), "dbo_obj_short_name");
			
			if (objects_name && short_name) {
				gchar *str, *tmp1, *tmp2, *tmp3;
				tmp1 = gda_rfc1738_encode (schema);
				tmp2 = gda_rfc1738_encode (objects_name);
				tmp3 = gda_rfc1738_encode (short_name);
				str = g_strdup_printf ("OBJ_TYPE=table;OBJ_SCHEMA=%s;OBJ_NAME=%s;OBJ_SHORT_NAME=%s",
						       tmp1, tmp2, tmp3);
				g_free (tmp1);
				g_free (tmp2);
				g_free (tmp3);
				g_signal_emit (cloud, objects_cloud_signals [SELECTED], 0,
					       cloud->priv->type, str);
				g_free (str);
			}
		}
        }

	if (tags) 
		g_slist_free (tags);
}

/*
 * Links can also be activated by clicking.
 */
static gboolean
event_after (GtkWidget *text_view, GdkEvent *ev, ObjectsCloud *cloud)
{
	GtkTextIter start, end, iter;
	GtkTextBuffer *buffer;
	GdkEventButton *event;
	gint x, y;
	
	if (ev->type != GDK_BUTTON_RELEASE)
		return FALSE;
	
	event = (GdkEventButton *)ev;
	
	if (event->button != 1)
		return FALSE;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	
	/* we shouldn't follow a link if the user has selected something */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
		return FALSE;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);
	
	follow_if_link (text_view, &iter, cloud);
	
	return FALSE;
}

/* 
 * Links can be activated by pressing Enter.
 */
static gboolean
key_press_event (GtkWidget *text_view, GdkEventKey *event, ObjectsCloud *cloud)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	
	switch (event->keyval) {
	case GDK_Return: 
	case GDK_KP_Enter:
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, 
						  gtk_text_buffer_get_insert (buffer));
		follow_if_link (text_view, &iter, cloud);
		break;
		
	default:
		break;
	}
	return FALSE;
}
