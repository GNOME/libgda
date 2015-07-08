/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include "text-search.h"
#include "ui-support.h"

struct _TextSearchPrivate {
	GtkTextView *view;
	GtkTextBuffer *text;
        GtkWidget *search_entry;
        GtkToggleButton *search_sensitive;
        GList     *search_marks;
        GList     *current_mark; /* in @search_marks */
};

static void text_search_class_init (TextSearchClass *klass);
static void text_search_init       (TextSearch *tsearch, TextSearchClass *klass);
static void text_search_dispose    (GObject *object);
static void text_search_grab_focus (GtkWidget *widget);

static GObjectClass *parent_class = NULL;

/*
 * TextSearch class implementation
 */

static void
text_search_class_init (TextSearchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	GTK_WIDGET_CLASS (klass)->grab_focus = text_search_grab_focus;
	object_class->dispose = text_search_dispose;
}

static void
text_search_init (TextSearch *tsearch, G_GNUC_UNUSED TextSearchClass *klass)
{
	tsearch->priv = g_new0 (TextSearchPrivate, 1);
}

static void
text_search_dispose (GObject *object)
{
	TextSearch *tsearch = (TextSearch *) object;

	/* free memory */
	if (tsearch->priv) {
		g_object_unref ((GObject*) tsearch->priv->view);
		if (tsearch->priv->search_marks)
                        g_list_free (tsearch->priv->search_marks);

		g_free (tsearch->priv);
		tsearch->priv = NULL;
	}

	parent_class->dispose (object);
}

static void
text_search_grab_focus (GtkWidget *widget)
{
	gtk_widget_grab_focus (TEXT_SEARCH (widget)->priv->search_entry);
}

GType
text_search_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (TextSearchClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) text_search_class_init,
			NULL,
			NULL,
			sizeof (TextSearch),
			0,
			(GInstanceInitFunc) text_search_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_BOX, "TextSearch", &info, 0);
	}
	return type;
}

static void
search_text_changed_cb (GtkEntry *entry, TextSearch *tsearch)
{
	GtkTextIter iter, siter, end;
	GtkTextBuffer *buffer;
	const gchar *search_text, *sptr;
	gboolean sensitive;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tsearch->priv->view));
	
	/* clean all previous search result */
	gtk_text_buffer_get_bounds (buffer, &iter, &end);
	gtk_text_buffer_remove_tag_by_name (buffer, "search", &iter, &end);
	tsearch->priv->current_mark = NULL;
	if (tsearch->priv->search_marks) {
		GList *list;
		for (list = tsearch->priv->search_marks; list; list = list->next)
			gtk_text_buffer_delete_mark (buffer, GTK_TEXT_MARK (list->data));

		g_list_free (tsearch->priv->search_marks);
		tsearch->priv->search_marks = NULL;
	}

	gtk_text_buffer_get_start_iter (buffer, &iter);
	search_text = gtk_entry_get_text (entry);

	if (!search_text || !*search_text)
		return;

	sensitive = gtk_toggle_button_get_active (tsearch->priv->search_sensitive);

	while (1) {
		gboolean high = TRUE;
		siter = iter;
		sptr = search_text;

		/* search for @search_text starting from the @siter position */
		while (1) {
			gunichar c1, c2;
			c1 = gtk_text_iter_get_char (&siter);
			c2 = g_utf8_get_char (sptr);
			if (!sensitive) {
				c1 = g_unichar_tolower (c1);
				c2 = g_unichar_tolower (c2);
			}
			if (c1 != c2) {
				high = FALSE;
				break;
			}
			
			sptr = g_utf8_find_next_char (sptr, NULL);
			if (!sptr || !*sptr)
				break;

			if (! gtk_text_iter_forward_char (&siter)) {
				high = FALSE;
				break;
			}
		}
		if (high) {
			if (gtk_text_iter_forward_char (&siter)) {
				GtkTextMark *mark;
				gtk_text_buffer_apply_tag_by_name (buffer, "search", &iter, &siter);
				mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, FALSE);
				tsearch->priv->search_marks = g_list_prepend (tsearch->priv->search_marks,
									    mark);
			}
			iter = siter;
		}
		else {
			if (! gtk_text_iter_forward_char (&iter))
				break;
		}
	}

	if (tsearch->priv->search_marks) {
		tsearch->priv->search_marks = g_list_reverse (tsearch->priv->search_marks);
		tsearch->priv->current_mark = tsearch->priv->search_marks;
		gtk_text_view_scroll_mark_onscreen (tsearch->priv->view,
						    GTK_TEXT_MARK (tsearch->priv->current_mark->data));
	}
}

static void
sensitive_toggled_cb (G_GNUC_UNUSED GtkToggleButton *button, TextSearch *tsearch)
{
	search_text_changed_cb (GTK_ENTRY (tsearch->priv->search_entry), tsearch);
}

static void
hide_search_bar (TextSearch *tsearch)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer;

	/* clean all previous search result */
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tsearch->priv->view));
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	gtk_text_buffer_remove_tag_by_name (buffer, "search", &start, &end);

	if (tsearch->priv->search_marks) {
		GList *list;
		for (list = tsearch->priv->search_marks; list; list = list->next)
			gtk_text_buffer_delete_mark (buffer, GTK_TEXT_MARK (list->data));

		g_list_free (tsearch->priv->search_marks);
		tsearch->priv->search_marks = NULL;
	}
	tsearch->priv->current_mark = NULL;

	gtk_widget_hide (GTK_WIDGET (tsearch));
}

static void
go_back_search_cb (G_GNUC_UNUSED GtkButton *button, TextSearch *tsearch)
{
	if (tsearch->priv->current_mark && tsearch->priv->current_mark->prev) {
		tsearch->priv->current_mark = tsearch->priv->current_mark->prev;
		gtk_text_view_scroll_mark_onscreen (tsearch->priv->view,
						    GTK_TEXT_MARK (tsearch->priv->current_mark->data));
	}
}

static void
go_forward_search_cb (G_GNUC_UNUSED GtkButton *button, TextSearch *tsearch)
{
	if (tsearch->priv->current_mark && tsearch->priv->current_mark->next) {
		tsearch->priv->current_mark = tsearch->priv->current_mark->next;
		gtk_text_view_scroll_mark_onscreen (tsearch->priv->view,
						    GTK_TEXT_MARK (tsearch->priv->current_mark->data));
	}
}

/**
 * text_search_new:
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
text_search_new (GtkTextView *view)
{
	g_return_val_if_fail (GTK_IS_TEXT_VIEW (view), NULL);

	TextSearch *tsearch;
	GtkWidget *wid;

	tsearch = TEXT_SEARCH (g_object_new (TEXT_SEARCH_TYPE, "spacing", 5,
					     "homogeneous", FALSE, NULL));
	tsearch->priv->view = view;
	g_object_ref ((GObject*) tsearch->priv->view);
	tsearch->priv->text = gtk_text_view_get_buffer (view);

	gtk_text_buffer_create_tag (tsearch->priv->text, "search",
                                    "background", "yellow", NULL);

	wid = ui_make_small_button (FALSE, FALSE, NULL, "window-close-symbolic",
				    _("Hide search toolbar"));
	gtk_box_pack_start (GTK_BOX (tsearch), wid, FALSE, FALSE, 0);
	g_signal_connect_swapped (wid, "clicked",
				  G_CALLBACK (hide_search_bar), tsearch);
	
	wid = gtk_label_new (_("Search:"));
	gtk_box_pack_start (GTK_BOX (tsearch), wid, FALSE, FALSE, 0);
	
	wid = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (tsearch), wid, TRUE, TRUE, 0);
	tsearch->priv->search_entry = wid;
	gtk_container_set_focus_child (GTK_CONTAINER (tsearch), tsearch->priv->search_entry);
	g_signal_connect (wid, "changed",
			  G_CALLBACK (search_text_changed_cb), tsearch);
	
	wid = ui_make_small_button (FALSE, FALSE, NULL, "go-previous-symbolic", NULL);
	gtk_box_pack_start (GTK_BOX (tsearch), wid, FALSE, FALSE, 0);
	g_signal_connect (wid, "clicked",
			  G_CALLBACK (go_back_search_cb), tsearch);
	
	wid = ui_make_small_button (FALSE, FALSE, NULL, "go-next-symbolic", NULL);
	gtk_box_pack_start (GTK_BOX (tsearch), wid, FALSE, FALSE, 0);
	g_signal_connect (wid, "clicked",
			  G_CALLBACK (go_forward_search_cb), tsearch);
	
	wid = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_box_pack_start (GTK_BOX (tsearch), wid, FALSE, FALSE, 0);
	tsearch->priv->search_sensitive = GTK_TOGGLE_BUTTON (wid);
	g_signal_connect (wid, "toggled",
			  G_CALLBACK (sensitive_toggled_cb), tsearch);

	gtk_widget_show_all ((GtkWidget*) tsearch);
	gtk_widget_hide ((GtkWidget*) tsearch);

	return (GtkWidget*) tsearch;
}

/**
 * text_search_rerun:
 *
 * To be executed when the #GtkTextView's contents has changed
 */
void
text_search_rerun (TextSearch *tsearch)
{
	g_return_if_fail (IS_TEXT_SEARCH (tsearch));
	search_text_changed_cb (GTK_ENTRY (tsearch->priv->search_entry), tsearch);
}
