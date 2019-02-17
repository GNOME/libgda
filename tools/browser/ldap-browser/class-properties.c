/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include "class-properties.h"
#include "ldap-marshal.h"
#include "../text-search.h"
#include "../ui-support.h"

struct _ClassPropertiesPrivate {
	TConnection *tcnc;

	GtkTextView *view;
	GtkTextBuffer *text;
	gboolean hovering_over_link;

	GtkWidget *text_search;
};

static void class_properties_class_init (ClassPropertiesClass *klass);
static void class_properties_init       (ClassProperties *cprop, ClassPropertiesClass *klass);
static void class_properties_dispose   (GObject *object);

static GObjectClass *parent_class = NULL;

/* signals */
enum {
        OPEN_CLASS,
        LAST_SIGNAL
};

gint class_properties_signals [LAST_SIGNAL] = { 0 };

/*
 * ClassProperties class implementation
 */

static void
class_properties_class_init (ClassPropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	class_properties_signals [OPEN_CLASS] =
		g_signal_new ("open-class",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (ClassPropertiesClass, open_class),
                              NULL, NULL,
                              _ldap_marshal_VOID__STRING, G_TYPE_NONE,
                              1, G_TYPE_STRING);
	klass->open_class = NULL;

	object_class->dispose = class_properties_dispose;
}


static void
class_properties_init (ClassProperties *cprop, G_GNUC_UNUSED ClassPropertiesClass *klass)
{
	cprop->priv = g_new0 (ClassPropertiesPrivate, 1);
	cprop->priv->hovering_over_link = FALSE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (cprop), GTK_ORIENTATION_VERTICAL);
}

static void
class_properties_dispose (GObject *object)
{
	ClassProperties *cprop = (ClassProperties *) object;

	/* free memory */
	if (cprop->priv) {
		if (cprop->priv->tcnc) {
			g_object_unref (cprop->priv->tcnc);
		}
		g_free (cprop->priv);
		cprop->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
class_properties_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo columns = {
			sizeof (ClassPropertiesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) class_properties_class_init,
			NULL,
			NULL,
			sizeof (ClassProperties),
			0,
			(GInstanceInitFunc) class_properties_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "ClassProperties", &columns, 0);
	}
	return type;
}

static gboolean key_press_event (GtkWidget *text_view, GdkEventKey *event, ClassProperties *cprop);
static gboolean event_after (GtkWidget *text_view, GdkEvent *ev, ClassProperties *cprop);
static gboolean motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, ClassProperties *cprop);
static gboolean visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, ClassProperties *cprop);

static void show_search_bar (ClassProperties *cprop);

/**
 * class_properties_new:
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
class_properties_new (TConnection *tcnc)
{
	ClassProperties *cprop;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	cprop = CLASS_PROPERTIES (g_object_new (CLASS_PROPERTIES_TYPE, NULL));
	cprop->priv->tcnc = g_object_ref (tcnc);
	
	GtkWidget *sw;
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (cprop), sw, TRUE, TRUE, 0);

	GtkWidget *textview;
	textview = gtk_text_view_new ();
        gtk_container_add (GTK_CONTAINER (sw), textview);
        gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 5);
        gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 5);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
        cprop->priv->text = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	cprop->priv->view = GTK_TEXT_VIEW (textview);
        gtk_widget_show_all (sw);

        gtk_text_buffer_create_tag (cprop->priv->text, "section",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);

        gtk_text_buffer_create_tag (cprop->priv->text, "error",
                                    "foreground", "red", NULL);

        gtk_text_buffer_create_tag (cprop->priv->text, "data",
                                    "left-margin", 20, NULL);

        gtk_text_buffer_create_tag (cprop->priv->text, "starter",
                                    "indent", -10,
                                    "left-margin", 20, NULL);

	g_signal_connect (textview, "key-press-event", 
			  G_CALLBACK (key_press_event), cprop);
	g_signal_connect (textview, "event-after", 
			  G_CALLBACK (event_after), cprop);
	g_signal_connect (textview, "motion-notify-event", 
			  G_CALLBACK (motion_notify_event), cprop);
	g_signal_connect (textview, "visibility-notify-event", 
			  G_CALLBACK (visibility_notify_event), cprop);

	class_properties_set_class (cprop, NULL);

	return (GtkWidget*) cprop;
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;

/* Looks at all tags covering the position (x, y) in the text view, 
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView *text_view, gint x, gint y, ClassProperties *cprop)
{
	GSList *tags = NULL, *tagp = NULL;
	GtkTextIter iter;
	gboolean hovering = FALSE;
	
	gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
	
	tags = gtk_text_iter_get_tags (&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;

		if (g_object_get_data (G_OBJECT (tag), "class")) {
			hovering = TRUE;
			break;
		}
	}
	
	if (hovering != cprop->priv->hovering_over_link) {
		cprop->priv->hovering_over_link = hovering;
		
		if (cprop->priv->hovering_over_link) {
			if (! hand_cursor)
				hand_cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (text_view)), GDK_HAND2);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       hand_cursor);
		}
		else {
			if (!regular_cursor)
				regular_cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (text_view)), GDK_XTERM);
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
visibility_notify_event (GtkWidget *text_view, G_GNUC_UNUSED GdkEventVisibility *event,
			 ClassProperties *cprop)
{
	gint wx, wy, bx, by;
	GdkSeat *seat;
	GdkDevice *pointer;

	seat = gdk_display_get_default_seat (gtk_widget_get_display (text_view));
	pointer = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position (gtk_widget_get_window (text_view), pointer, &wx, &wy, NULL);
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       wx, wy, &bx, &by);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), bx, by, cprop);
	
	return FALSE;
}

/*
 * Update the cursor image if the pointer moved. 
 */
static gboolean
motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, ClassProperties *cprop)
{
	gint x, y;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), x, y, cprop);

	return FALSE;
}

/* Looks at all tags covering the position of iter in the text view, 
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (G_GNUC_UNUSED GtkWidget *text_view, GtkTextIter *iter, ClassProperties *cprop)
{
	GSList *tags = NULL, *tagp = NULL;
	
	tags = gtk_text_iter_get_tags (iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		const gchar *dn;
		
		dn = g_object_get_data (G_OBJECT (tag), "class");
		if (dn)
			g_signal_emit (cprop, class_properties_signals [OPEN_CLASS], 0, dn);
        }

	if (tags) 
		g_slist_free (tags);
}


/*
 * Links can also be activated by clicking.
 */
static gboolean
event_after (GtkWidget *text_view, GdkEvent *ev, ClassProperties *cprop)
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
	
	follow_if_link (text_view, &iter, cprop);
	
	return FALSE;
}

/* 
 * Links can be activated by pressing Enter.
 */
static gboolean
key_press_event (GtkWidget *text_view, GdkEventKey *event, ClassProperties *cprop)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	
	switch (event->keyval) {
	case GDK_KEY_Return: 
	case GDK_KEY_KP_Enter:
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, 
						  gtk_text_buffer_get_insert (buffer));
		follow_if_link (text_view, &iter, cprop);
		break;
	case GDK_KEY_F:
	case GDK_KEY_f:
		if (event->state & GDK_CONTROL_MASK) {
			show_search_bar (cprop);
			return TRUE;
		}
		break;
	case GDK_KEY_slash:
		show_search_bar (cprop);
		return TRUE;
		break;		
	default:
		break;
	}
	return FALSE;
}

/**
 * class_properties_set_class:
 * @cprop: a #ClassProperties widget
 * @classname: a DN to display information for
 *
 * Adjusts the display to show @classname's properties
 */
void
class_properties_set_class (ClassProperties *cprop, const gchar *classname)
{
	g_return_if_fail (IS_CLASS_PROPERTIES (cprop));

	GtkTextBuffer *tbuffer;
	GtkTextIter start, end;
	GdaLdapClass *lcl;
	GtkTextIter current;
	guint i;

	tbuffer = cprop->priv->text;
	gtk_text_buffer_get_start_iter (tbuffer, &start);
        gtk_text_buffer_get_end_iter (tbuffer, &end);
        gtk_text_buffer_delete (tbuffer, &start, &end);

	if (!classname || !*classname)
		return;

	lcl = t_connection_get_class_info (cprop->priv->tcnc, classname);
	if (!lcl) {
		ui_show_message (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) cprop)),
				      "%s", _("Could not get information about LDAP class"));
		return;
	}

	gtk_text_buffer_get_start_iter (tbuffer, &current);

	/* Description */
	if (lcl->description) {
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
							  _("Description:"), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, lcl->description, -1,
							  "data", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
	}

	/* OID */
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
						  _("Class OID:"), -1,
						  "section", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, lcl->oid, -1,
						  "data", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", -1);

	/* Kind */
	const gchar *kind;
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
						  _("Class kind:"), -1,
						  "section", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", 1);

	gtk_text_buffer_insert_pixbuf (tbuffer, &current, ui_connection_ldap_icon_for_class_kind (lcl->kind)); 

	kind = ui_connection_ldap_class_kind_to_string (lcl->kind);
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, kind, -1,
						  "data", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", -1);

	/* Class name */
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
						  ngettext ("Class name:", "Class names:", lcl->nb_names), -1,
						  "section", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
	for (i = 0; i < lcl->nb_names; i++) {
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, lcl->names[i], -1,
							  "data", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
	}

	/* obsolete */
	if (lcl->obsolete) {
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
							  _("This LDAP class is obsolete"), -1,
							  "error", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
	}
	
	/* req. attributes */
	if (lcl->nb_req_attributes > 0) {
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
							  ngettext ("Required attribute:",
								    "Required attributes:",
								    lcl->nb_req_attributes), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		for (i = 0; i < lcl->nb_req_attributes; i++) {
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, lcl->req_attributes[i], -1,
								  "data", NULL);
			gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		}
	}

	/* opt. attributes */
	if (lcl->nb_opt_attributes > 0) {
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
							  ngettext ("Optional attribute:",
								    "Optional attributes:",
								    lcl->nb_opt_attributes), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		for (i = 0; i < lcl->nb_opt_attributes; i++) {
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, lcl->opt_attributes[i], -1,
								  "data", NULL);
			gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		}
	}

	/* children */
	if (lcl->children) {
		gint nb;
		GSList *list;
		nb = g_slist_length (lcl->children);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
							  ngettext ("Children class:",
								    "Children classes:",
								    nb), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		for (list = lcl->children; list; list = list->next) {
			GdaLdapClass *olcl;
			gchar *tmp;
			GtkTextTag *tag;
			olcl = (GdaLdapClass*) list->data;
			gtk_text_buffer_insert_pixbuf (tbuffer, &current,
						       ui_connection_ldap_icon_for_class_kind (olcl->kind)); 
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
			tag = gtk_text_buffer_create_tag (tbuffer, NULL,
							  "foreground", "blue",
							  "weight", PANGO_WEIGHT_NORMAL,
							  "underline", PANGO_UNDERLINE_SINGLE,
							  NULL);
			tmp = olcl->names [0];
			g_object_set_data_full (G_OBJECT (tag), "class",
						g_strdup (tmp), g_free);
			gtk_text_buffer_insert_with_tags (tbuffer, &current,
							  tmp, -1,
							  tag, NULL);
			if (olcl->description) {
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  " (", -1,
									  "data", NULL);
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  olcl->description, -1,
									  "data", NULL);
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  ")", -1,
									  "data", NULL);
			}
			gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		}
	}

	/* parents */
	if (lcl->parents) {
		gint nb;
		GSList *list;
		nb = g_slist_length (lcl->parents);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
							  ngettext ("Inherited class:",
								    "Inherited classes:",
								    nb), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		for (list = lcl->parents; list; list = list->next) {
			GdaLdapClass *olcl;
			gchar *tmp;
			GtkTextTag *tag;
			olcl = (GdaLdapClass*) list->data;

			gtk_text_buffer_insert_pixbuf (tbuffer, &current,
						       ui_connection_ldap_icon_for_class_kind (olcl->kind)); 
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
			tag = gtk_text_buffer_create_tag (tbuffer, NULL,
							  "foreground", "blue",
							  "weight", PANGO_WEIGHT_NORMAL,
							  "underline", PANGO_UNDERLINE_SINGLE,
							  NULL);
			tmp = olcl->names [0];
			g_object_set_data_full (G_OBJECT (tag), "class",
						g_strdup (tmp), g_free);
			gtk_text_buffer_insert_with_tags (tbuffer, &current,
							  tmp, -1,
							  tag, NULL);

			if (olcl->description) {
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  " (", -1,
									  "data", NULL);
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  olcl->description, -1,
									  "data", NULL);
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  ")", -1,
									  "data", NULL);
			}

			gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
		}
	}

	if (cprop->priv->text_search && gtk_widget_get_visible (cprop->priv->text_search))
		text_search_rerun (TEXT_SEARCH (cprop->priv->text_search));
}

static void
show_search_bar (ClassProperties *cprop)
{
	if (! cprop->priv->text_search) {
		cprop->priv->text_search = text_search_new (GTK_TEXT_VIEW (cprop->priv->view));
		gtk_box_pack_start (GTK_BOX (cprop), cprop->priv->text_search, FALSE, FALSE, 0);
		gtk_widget_show (cprop->priv->text_search);
	}
	else {
		gtk_widget_show (cprop->priv->text_search);
		text_search_rerun (TEXT_SEARCH (cprop->priv->text_search));
	}

	gtk_widget_grab_focus (cprop->priv->text_search);
}
