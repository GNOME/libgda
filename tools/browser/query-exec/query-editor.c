/* GNOME DB library
 * Copyright (C) 1999 - 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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
#include <gdk/gdkkeysyms.h>
#ifdef HAVE_GTKSOURCEVIEW
  #include <gtksourceview/gtksourceview.h>
  #include <gtksourceview/gtksourcelanguagemanager.h>
  #include <gtksourceview/gtksourcebuffer.h>
  #include <gtksourceview/gtksourcestyleschememanager.h>
  #include <gtksourceview/gtksourcestylescheme.h>
#endif
#include "query-editor.h"
#include <binreloc/gda-binreloc.h>

#define QUERY_EDITOR_LANGUAGE_SQL "gda-sql"
#define COLOR_ALTER_FACTOR 1.8
#define MAX_HISTORY_BATCH_ITEMS 20
#define STATES_ARRAY_SIZE 32

static void query_editor_history_batch_add_item (QueryEditorHistoryBatch *qib,
						 QueryEditorHistoryItem *qih);
static void query_editor_history_batch_del_item (QueryEditorHistoryBatch *qib,
						 QueryEditorHistoryItem *qih);

typedef void (* CreateTagsFunc) (QueryEditor *editor, const gchar *language);

typedef struct {
	QueryEditorHistoryBatch *batch; /* ref held here */
	QueryEditorHistoryItem *item; /* ref held here */
	GtkTextTag *tag; /* ref held here */
	GtkTextMark *start_mark; /* ref NOT held here */
	GtkTextMark *end_mark; /* ref NOT held here */

	gint ref_count;
} HistItemData;
static HistItemData *hist_item_data_new (void);
static HistItemData *hist_item_data_ref (HistItemData *hdata);
static void          hist_item_data_unref (HistItemData *hdata);

struct _QueryEditorPrivate {
	QueryEditorMode mode;
	GtkWidget *scrolled_window;
	GtkWidget *text;

	GtkTextTag *indent_tag;
	GArray *states; /* array of strings, pos 0 => oldest state */
	gint current_state;
	gchar *current_state_text;

	/* HISTORY mode */
	guint ts_timeout_id;
	GSList *batches_list; /* list of QueryEditorHistoryBatch, in reverse order, refs held here */
	GHashTable *hash; /* crossed references:
			   * QueryEditorHistoryBatch --> HistItemData on @batch
			   * QueryEditorHistoryItem --> HistItemData on @item
			   * GtkTextTag --> HistItemData on @tag
			   *
			   * hash table holds references to all HistItemData
			   */
	QueryEditorHistoryBatch *insert_into_batch; /* hold ref here */
	HistItemData *hist_focus; /* ref held here */
};

static void query_editor_class_init (QueryEditorClass *klass);
static void query_editor_init       (QueryEditor *editor, QueryEditorClass *klass);
static void query_editor_finalize   (GObject *object);

static void query_editor_map       (GtkWidget *widget);
static void query_editor_grab_focus (GtkWidget *widget);


static GObjectClass *parent_class = NULL;
static GHashTable *supported_languages = NULL;
static gint number_of_objects = 0;

static void focus_on_hist_data (QueryEditor *editor, HistItemData *hdata);
static HistItemData *get_next_hist_data (QueryEditor *editor, HistItemData *hdata);
static HistItemData *get_prev_hist_data (QueryEditor *editor, HistItemData *hdata);

static gboolean timestamps_update_cb (QueryEditor *editor);

/* signals */
enum
{
        CHANGED,
	HISTORY_ITEM_REMOVED,
	EXECUTE_REQUEST,
        LAST_SIGNAL
};

static gint query_editor_signals[LAST_SIGNAL] = { 0, 0, 0 };


/*
 * Private functions
 */
static void
create_tags_for_sql (QueryEditor *editor, const gchar *language)
{
	GtkTextBuffer *buffer;
#ifdef HAVE_GTKSOURCEVIEW
	GtkSourceLanguageManager *mgr;
	GtkSourceLanguage *lang;
	gchar ** current_search_path;
	gint len;
	gchar ** search_path;

	GtkSourceStyleSchemeManager* sch_mgr;
	GtkSourceStyleScheme *sch;
#endif

	g_return_if_fail (language != NULL);
	g_return_if_fail (!strcmp (language, QUERY_EDITOR_LANGUAGE_SQL));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
#ifdef HAVE_GTKSOURCEVIEW
	mgr = gtk_source_language_manager_new ();

	/* alter search path */
	current_search_path = (gchar **) gtk_source_language_manager_get_search_path (mgr);
	len = g_strv_length (current_search_path);
	search_path = g_new0 (gchar*, len + 2);
	memcpy (search_path, current_search_path, sizeof (gchar*) * len);
	search_path [len] = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "language-specs", NULL);
	gtk_source_language_manager_set_search_path (mgr, search_path);
	g_free (search_path [len]);
	g_free (search_path);

	lang = gtk_source_language_manager_get_language (mgr, "gda-sql");

	if (!lang) {
		gchar *tmp;
		tmp = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "language-spec", NULL);
		g_print ("Could not find the gda-sql.lang file in %s,\nusing the default SQL highlighting rules.\n",
			 tmp);
		g_free (tmp);
		lang = gtk_source_language_manager_get_language (mgr, "sql");
	}
	if (lang)
		gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), lang);

	g_object_unref (mgr);

	sch_mgr = gtk_source_style_scheme_manager_get_default ();
	sch = gtk_source_style_scheme_manager_get_scheme (sch_mgr, "tango");
	if (sch) 
		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buffer), sch);
	
#endif
}

/*
 * QueryEditor class implementation
 */
static void
query_editor_class_init (QueryEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	query_editor_signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (QueryEditorClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	query_editor_signals[EXECUTE_REQUEST] =
                g_signal_new ("execute-request",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (QueryEditorClass, execute_request),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	query_editor_signals[HISTORY_ITEM_REMOVED] =
                g_signal_new ("history-item-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (QueryEditorClass, history_item_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

	object_class->finalize = query_editor_finalize;
	GTK_WIDGET_CLASS (object_class)->map = query_editor_map;
	GTK_WIDGET_CLASS (object_class)->grab_focus = query_editor_grab_focus;
}

static void
text_buffer_changed_cb (GtkTextBuffer *buffer, QueryEditor *editor)
{
	if (editor->priv->mode != QUERY_EDITOR_HISTORY)
		g_signal_emit (editor, query_editor_signals[CHANGED], 0);
}

/*
 * Returns: -1 if none focussed
 */
static gboolean
event (GtkWidget *text_view, GdkEvent *ev, QueryEditor *editor)
{
	GtkTextIter start, end, iter;
	GtkTextBuffer *buffer;
	GdkEventButton *event;
	gint x, y;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	if ((editor->priv->mode == QUERY_EDITOR_HISTORY) &&
	    (ev->type == GDK_BUTTON_RELEASE)) {
		event = (GdkEventButton *)ev;
		
		if (event->button != 1)
			return FALSE;
		
		/* we shouldn't follow a link if the user has selected something */
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
			return FALSE;
		
		gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
						       GTK_TEXT_WINDOW_WIDGET,
						       event->x, event->y, &x, &y);
		
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);
		
		/* go through tags */
		GSList *tags = NULL, *tagp = NULL;
		HistItemData *hist_focus = NULL;
		tags = gtk_text_iter_get_tags (&iter);
		for (tagp = tags;  tagp;  tagp = tagp->next) {
			hist_focus = g_hash_table_lookup (editor->priv->hash, tagp->data);
			if (hist_focus)
				break;
		}
		focus_on_hist_data (editor, hist_focus);
		
		if (tags) 
			g_slist_free (tags);
	}
	else if (ev->type == GDK_KEY_PRESS) {
		GdkEventKey *evkey = ((GdkEventKey*) ev);
		if ((editor->priv->mode == QUERY_EDITOR_HISTORY) &&
		    ((evkey->keyval == GDK_Up) || (evkey->keyval == GDK_Down))) {
			HistItemData *nfocus = NULL;
			if (editor->priv->hist_focus) {
				if (((GdkEventKey*) ev)->keyval == GDK_Up)
					nfocus = get_prev_hist_data (editor, editor->priv->hist_focus);
				else
					nfocus = get_next_hist_data (editor, editor->priv->hist_focus);
				if (!nfocus)
					nfocus = editor->priv->hist_focus;
			}
			
			focus_on_hist_data (editor, nfocus);
			return TRUE;
		}
		else if ((editor->priv->mode == QUERY_EDITOR_HISTORY) &&
			 ((evkey->keyval == GDK_Delete) && editor->priv->hist_focus)) {
			if (editor->priv->hist_focus->item)
				query_editor_del_current_history_item (editor);
			else if (editor->priv->hist_focus->batch)
				query_editor_del_history_batch (editor, editor->priv->hist_focus->batch);
			return TRUE;
		}
		else if ((editor->priv->mode == QUERY_EDITOR_READWRITE) && 
			 (evkey->state & GDK_CONTROL_MASK) &&
			 ((evkey->keyval == GDK_L) || (evkey->keyval == GDK_l))) {
			GtkTextIter start, end;
			gtk_text_buffer_get_start_iter (buffer, &start);
			gtk_text_buffer_get_end_iter (buffer, &end);
			gtk_text_buffer_delete (buffer, &start, &end);
			return TRUE;
		}
		else if ((editor->priv->mode == QUERY_EDITOR_READWRITE) && 
			 (evkey->state & GDK_CONTROL_MASK) &&
			 (evkey->keyval == GDK_Return)) {
			g_signal_emit (editor, query_editor_signals[EXECUTE_REQUEST], 0);
			return TRUE;
		}
		else if ((editor->priv->mode == QUERY_EDITOR_READWRITE) && 
			 (evkey->state & GDK_CONTROL_MASK) &&
			 (evkey->keyval == GDK_Up) &&
			 editor->priv->states) {
			if (editor->priv->states->len > 0) {
				gint i = -1;
				if (editor->priv->current_state == G_MAXINT) {
					i = editor->priv->states->len - 1;
					g_free (editor->priv->current_state_text);
					editor->priv->current_state_text = query_editor_get_all_text (editor);
				}
				else if (editor->priv->current_state >= editor->priv->states->len)
					i = editor->priv->states->len - 1; /* last stored state */
				else if (editor->priv->current_state > 0)
					i = editor->priv->current_state - 1;
				gchar *ctext;
				ctext = query_editor_get_all_text (editor);				
				while (i >= 0) {
					gchar *tmp;
					editor->priv->current_state = i;
					tmp = g_array_index (editor->priv->states, gchar*, i);
					if (strcmp (tmp, ctext)) {
						query_editor_set_text (editor, tmp);
						break;
					}
					i--;
				}
				g_free (ctext);
			}
			return TRUE;
		}
		else if ((editor->priv->mode == QUERY_EDITOR_READWRITE) && 
			 (evkey->state & GDK_CONTROL_MASK) &&
			 (evkey->keyval == GDK_Down) &&
			 editor->priv->states) {
			if (editor->priv->states->len > 0) {
				if (editor->priv->current_state < editor->priv->states->len - 1) {
					gchar *tmp;
					editor->priv->current_state ++;
					tmp = g_array_index (editor->priv->states, gchar*, editor->priv->current_state);
					query_editor_set_text (editor, tmp);
				}
				else if (editor->priv->current_state_text) {
					editor->priv->current_state = G_MAXINT;
					query_editor_set_text (editor, editor->priv->current_state_text);
					g_free (editor->priv->current_state_text);
					editor->priv->current_state_text = NULL;
				}
			}
			return TRUE;
		}
	}
	else
		return FALSE;

	return FALSE;
}

static gboolean
text_view_expose_event (GtkTextView *tv, GdkEventExpose *event, QueryEditor *editor)
{
	GdkWindow *win;

	win = gtk_text_view_get_window (tv, GTK_TEXT_WINDOW_TEXT);
	if (event->window != win)
		return FALSE;
	if (!editor->priv->hist_focus)
		return FALSE;
	
	GdkRectangle visible_rect;
	GdkRectangle redraw_rect;
	GtkTextIter cur;
	gint y, ye;
	gint height, heighte;
	gint win_y;
	GdkGC *gc;
	gint margin;
	
	gtk_text_buffer_get_iter_at_mark (tv->buffer, &cur, editor->priv->hist_focus->start_mark);	
	gtk_text_view_get_line_yrange (tv, &cur, &y, &height);
	
	gtk_text_buffer_get_iter_at_mark (tv->buffer, &cur, editor->priv->hist_focus->end_mark);	
	gtk_text_view_get_line_yrange (tv, &cur, &ye, &heighte);
	height = ye - y;

	if (!editor->priv->hist_focus->item) {
		GSList *list;
		HistItemData *hdata;
		list = g_slist_last (editor->priv->hist_focus->batch->hist_items);
		if (list) {
			hdata = g_hash_table_lookup (editor->priv->hash, list->data);
			gtk_text_buffer_get_iter_at_mark (tv->buffer, &cur, hdata->end_mark);	
			gtk_text_view_get_line_yrange (tv, &cur, &ye, &heighte);
			height = ye - y;
		}
	}
		
	gtk_text_view_get_visible_rect (tv, &visible_rect);
	gtk_text_view_buffer_to_window_coords (tv,
					       GTK_TEXT_WINDOW_TEXT,
					       visible_rect.x,
					       visible_rect.y,
					       &redraw_rect.x,
					       &redraw_rect.y);	
	gtk_text_view_buffer_to_window_coords (tv,
					       GTK_TEXT_WINDOW_TEXT,
					       0,
					       y,
					       NULL,
					       &win_y);
	
	redraw_rect.width = visible_rect.width;
	redraw_rect.height = visible_rect.height;
	
	gc = GTK_WIDGET (tv)->style->bg_gc[GTK_WIDGET_STATE (GTK_WIDGET (tv))];
	margin = gtk_text_view_get_left_margin (tv);
	
	gdk_draw_rectangle (event->window,
			    gc,
			    TRUE,
			    redraw_rect.x + MAX (0, margin - 1),
			    win_y,
			    redraw_rect.width,
			    height);

	return FALSE;
}

static void
query_editor_init (QueryEditor *editor, QueryEditorClass *klass)
{
	int tab = 8;
	gboolean highlight = TRUE;
	gboolean showlinesno = FALSE;

	g_return_if_fail (QUERY_IS_EDITOR (editor));

	/* allocate private structure */
	editor->priv = g_new0 (QueryEditorPrivate, 1);
	editor->priv->mode = QUERY_EDITOR_READWRITE;
	editor->priv->batches_list = NULL;
	editor->priv->hash = NULL;
	editor->priv->hist_focus = NULL;

	editor->priv->states = NULL;
	editor->priv->current_state = G_MAXINT;
	editor->priv->current_state_text = NULL;

	/* set up widgets */
	editor->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window),
					     GTK_SHADOW_ETCHED_OUT);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (editor), editor->priv->scrolled_window, TRUE, TRUE, 2);

#ifdef HAVE_GTKSOURCEVIEW
	editor->priv->text = gtk_source_view_new ();
	gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text))),
					 highlight);
	gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (editor->priv->text), showlinesno);
	gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (editor->priv->text), tab);
#else
	editor->priv->text = gtk_text_view_new ();
#endif

	gtk_container_add (GTK_CONTAINER (editor->priv->scrolled_window), editor->priv->text);
	gtk_widget_set_tooltip_markup (editor->priv->text, QUERY_EDITOR_TOOLTIP);
	g_signal_connect (editor->priv->text, "event", 
			  G_CALLBACK (event), editor);
	g_signal_connect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)), "changed", 
			  G_CALLBACK (text_buffer_changed_cb), editor);
	g_signal_connect (editor->priv->text, "expose-event",
			  G_CALLBACK (text_view_expose_event), editor);

	/* create some tags */
	GtkTextBuffer *buffer;
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	gtk_text_buffer_create_tag (buffer, "h0",
				    "foreground", "#474A8F",
				    "weight", PANGO_WEIGHT_BOLD,
				    "variant", PANGO_VARIANT_SMALL_CAPS,
				    "scale", PANGO_SCALE_LARGE,
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);

	gtk_text_buffer_create_tag (buffer, "note",
				    "left-margin", 50,
				    "foreground", "black",
				    "weight", PANGO_WEIGHT_NORMAL,
				    "style", PANGO_STYLE_ITALIC,
				    NULL);

	/* initialize common data */
	number_of_objects++;
	if (!supported_languages) {
		supported_languages = g_hash_table_new (g_str_hash, g_str_equal);

		/* add the built-in languages */
		g_hash_table_insert (supported_languages, QUERY_EDITOR_LANGUAGE_SQL, create_tags_for_sql);
	}

	create_tags_for_sql (editor, QUERY_EDITOR_LANGUAGE_SQL);

	gtk_widget_show_all (editor->priv->scrolled_window);

	/* timeout function to update timestamps */
	editor->priv->ts_timeout_id = 0;
}

static void
query_editor_map (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (parent_class)->map (widget);
	if (QUERY_EDITOR (widget)->priv->mode == QUERY_EDITOR_HISTORY) {
		GtkStyle *style;
		GdkColor color;
		style = gtk_widget_get_style (widget);
		color = style->bg[GTK_STATE_NORMAL];
		color.red += (65535 - color.red) / COLOR_ALTER_FACTOR;
		color.green += (65535 - color.green) / COLOR_ALTER_FACTOR;
		color.blue += (65535 - color.blue) / COLOR_ALTER_FACTOR;
		gtk_widget_modify_base (QUERY_EDITOR (widget)->priv->text, GTK_STATE_NORMAL, &color);
	}
}


static void
query_editor_grab_focus (GtkWidget *widget)
{
	gtk_widget_grab_focus (QUERY_EDITOR (widget)->priv->text);
}


static void
hist_data_free_all (QueryEditor *editor)
{
	if (editor->priv->ts_timeout_id) {
		g_source_remove (editor->priv->ts_timeout_id);
		editor->priv->ts_timeout_id = 0;
	}
	if (editor->priv->batches_list) {
		g_slist_foreach (editor->priv->batches_list, (GFunc) query_editor_history_batch_unref, NULL);
		g_slist_free (editor->priv->batches_list);
		editor->priv->batches_list = NULL;
	}

	if (editor->priv->hash) {
		g_hash_table_destroy (editor->priv->hash);
		editor->priv->hash = NULL;
	}

	if (editor->priv->hist_focus) {
		hist_item_data_unref (editor->priv->hist_focus);
		editor->priv->hist_focus = NULL;
	}	
}

static void
query_editor_finalize (GObject *object)
{
	QueryEditor *editor = (QueryEditor *) object;

	g_return_if_fail (QUERY_IS_EDITOR (editor));

	/* free memory */
	hist_data_free_all (editor);
	if (editor->priv->states) {
		gint i;
		for (i = 0; i < editor->priv->states->len; i++) {
			gchar *str;
			str = g_array_index (editor->priv->states, gchar*, i);
			g_free (str);
		}
		g_array_free (editor->priv->states, TRUE);
	}
	g_free (editor->priv->current_state_text);

	g_free (editor->priv);
	editor->priv = NULL;

	parent_class->finalize (object);

	/* update common data */
	number_of_objects--;
	if (number_of_objects == 0) {
		g_hash_table_destroy (supported_languages);
		supported_languages = NULL;
	}
}

GType
query_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (QueryEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_editor_class_init,
			NULL,
			NULL,
			sizeof (QueryEditor),
			0,
			(GInstanceInitFunc) query_editor_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "QueryEditor", &info, 0);
	}
	return type;
}

/**
 * query_editor_new
 *
 * Create a new #QueryEditor widget, which is a multiline text widget
 * with support for several languages used to write database stored
 * procedures and functions. If libgnomedb was compiled with gtksourceview
 * in, this widget will support syntax highlighting for all supported
 * languages.
 *
 * Returns: the newly created widget.
 */
GtkWidget *
query_editor_new (void)
{
	QueryEditor *editor;

	editor = g_object_new (QUERY_TYPE_EDITOR, NULL);

	return GTK_WIDGET (editor);
}

/**
 * query_editor_get_mode
 * @editor: a #QueryEditor widget.
 *
 * Get @editor's mode
 *
 * Returns: @editor's mode
 */
QueryEditorMode
query_editor_get_mode (QueryEditor *editor)
{
	g_return_val_if_fail (QUERY_IS_EDITOR (editor), 0);
	return editor->priv->mode;
}

/**
 * query_editor_set_mode
 * @editor: a #QueryEditor widget.
 * @mode: new mode
 *
 * A mode change will empty the buffer.
 */
void
query_editor_set_mode (QueryEditor *editor, QueryEditorMode mode)
{
	GtkTextBuffer *buffer;
	gboolean clean = TRUE;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	if (editor->priv->mode == mode)
		return;

	if (((editor->priv->mode == QUERY_EDITOR_READWRITE) && (mode == QUERY_EDITOR_READONLY)) ||
	    ((editor->priv->mode == QUERY_EDITOR_READONLY) && (mode == QUERY_EDITOR_READWRITE)))
		clean = FALSE;
	else if (editor->priv->mode == QUERY_EDITOR_HISTORY)
		hist_data_free_all (editor);

	editor->priv->mode = mode;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	if (clean) {
		GtkTextIter start, end;
		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_delete (buffer, &start, &end);
	}

	switch (mode) {
	case QUERY_EDITOR_READWRITE:
		gtk_widget_set_tooltip_markup (editor->priv->text, QUERY_EDITOR_TOOLTIP);
		gtk_text_view_set_editable (GTK_TEXT_VIEW (editor->priv->text), TRUE);
		gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (editor->priv->text), TRUE);
		break;
	case QUERY_EDITOR_READONLY:
	case QUERY_EDITOR_HISTORY:
		gtk_widget_set_tooltip_markup (editor->priv->text, NULL);
		gtk_text_view_set_editable (GTK_TEXT_VIEW (editor->priv->text), FALSE);
		gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (editor->priv->text), FALSE);
		break;
	default:
		g_assert_not_reached ();
	}

	if (mode == QUERY_EDITOR_HISTORY) {
		GtkStyle *style;
		GdkColor color;
		style = gtk_widget_get_style ((GtkWidget*) editor);
		color = style->bg[GTK_STATE_NORMAL];
		color.red += (65535 - color.red) / COLOR_ALTER_FACTOR;
		color.green += (65535 - color.green) / COLOR_ALTER_FACTOR;
		color.blue += (65535 - color.blue) / COLOR_ALTER_FACTOR;
		gtk_widget_modify_base (editor->priv->text, GTK_STATE_NORMAL, &color);

		editor->priv->hash = g_hash_table_new_full (NULL, NULL, NULL,
							    (GDestroyNotify) hist_item_data_unref);
	}
	else {
		gtk_widget_modify_base (editor->priv->text,
					GTK_STATE_NORMAL, NULL);
	}
}

/**
 * query_editor_set_text
 * @editor: a #QueryEditor widget.
 * @text: text to display in the editor.
 *
 * Set @editor's text, removing any previous one
 */
void
query_editor_set_text (QueryEditor *editor, const gchar *text)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode != QUERY_EDITOR_HISTORY);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);
	gtk_text_buffer_delete (buffer, &start, &end);

	if (text) {
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_insert (buffer, &end, text, -1);
	}
}

/**
 * query_editor_append_text
 * @editor: a #QueryEditor widget.
 * @text: text to display in the editor.
 *
 * Appends some text to @editor.
 */
void
query_editor_append_text (QueryEditor *editor, const gchar *text)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode != QUERY_EDITOR_HISTORY);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));

	if (text) {
		GtkTextIter end;
		gint len;
		len = strlen (text);
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_insert (buffer, &end, text, -1);

		if ((len >= 1) && (text [len - 1] != '\n'))
			gtk_text_buffer_insert (buffer, &end, "\n", 1);
	}
}

/**
 * query_editor_keep_current_state
 * @editor:  #QueryEditor widget.
 *
 * Makes @editor keep the current text as a past state, which can be brought back
 * using CTRL-Up or Gtrl-Down
 */
void
query_editor_keep_current_state (QueryEditor *editor)
{
	gchar *text, *tmp;
	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode != QUERY_EDITOR_HISTORY);
	
	if (!editor->priv->states)
		editor->priv->states = g_array_sized_new (FALSE, FALSE, sizeof (gchar*), STATES_ARRAY_SIZE);

	editor->priv->current_state = G_MAXINT;

	text = query_editor_get_all_text (editor);
	if (editor->priv->states->len > 0) {
		tmp = g_array_index (editor->priv->states, gchar *, editor->priv->states->len-1);
		if (!strcmp (tmp, text)) {
			g_free (text);
			return;
		}
	}

	if (editor->priv->states->len == STATES_ARRAY_SIZE) {
		tmp = g_array_index (editor->priv->states, gchar *, 0);
		g_free (tmp);
		g_array_remove_index (editor->priv->states, 0);
	}
	g_array_append_val (editor->priv->states, text);
}

/**
 * query_editor_append_note
 * @editor: a #QueryEditor widget.
 * @text: text to display in the editor.
 * @level: 0 for header, 1 for text with marging and in italics
 *
 * Appends some text to @editor.
 */
void
query_editor_append_note (QueryEditor *editor, const gchar *text, gint level)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode != QUERY_EDITOR_HISTORY);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	if (text) {
		GtkTextIter end;
		gchar *str;
		
		switch (level) {
		case 0:
			str = g_strdup_printf ("%s\n", text);
			gtk_text_buffer_get_end_iter (buffer, &end);
			gtk_text_buffer_insert_with_tags_by_name (buffer, &end,
								  str, -1,
								  "h0", NULL);
			g_free (str);
			break;
		case 1:
			str = g_strdup_printf ("%s\n", text);
			gtk_text_buffer_get_end_iter (buffer, &end);
			gtk_text_buffer_insert_with_tags_by_name (buffer, &end,
								  str, -1,
								  "note", NULL);
			g_free (str);
			break;
		default:
			g_assert_not_reached ();
		}
	}	
}

/**
 * query_editor_show_tooltip
 * @editor: a #QueryEditor
 * @show_tooltip:
 *
 * Defines if tooltip can be shown or not
 */
void
query_editor_show_tooltip (QueryEditor *editor, gboolean show_tooltip)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode == QUERY_EDITOR_READWRITE);
	
	if (show_tooltip)
		gtk_widget_set_tooltip_markup (editor->priv->text, QUERY_EDITOR_TOOLTIP);
	else
		gtk_widget_set_tooltip_markup (editor->priv->text, NULL);
}

static void
focus_on_hist_data (QueryEditor *editor, HistItemData *hdata)
{
	if (editor->priv->hist_focus) {
		if (editor->priv->hist_focus == hdata)
			return;
		if (editor->priv->hist_focus->item)
			g_object_set (G_OBJECT (editor->priv->hist_focus->tag),
				      "foreground-set", TRUE, NULL);
		else {
			/* un-highlight all the batch */
			GSList *list;
			for (list = editor->priv->hist_focus->batch->hist_items; list; list = list->next) {
				HistItemData *hd;
				hd = g_hash_table_lookup (editor->priv->hash, list->data);
				g_object_set (G_OBJECT (hd->tag),
					      "foreground-set", TRUE, NULL);
			}
		}
		hist_item_data_unref (editor->priv->hist_focus);
		editor->priv->hist_focus = NULL;
	}

	if (hdata) {
		GtkTextBuffer *buffer;
		GtkTextIter iter;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
		editor->priv->hist_focus = hist_item_data_ref (hdata);
		if (hdata->item)
			g_object_set (G_OBJECT (hdata->tag),
				      "foreground-set", FALSE, NULL);
		else {
			/* highlight all the batch */
			GSList *list;
			for (list = hdata->batch->hist_items; list; list = list->next) {
				HistItemData *hd;
				hd = g_hash_table_lookup (editor->priv->hash, list->data);
				g_object_set (G_OBJECT (hd->tag),
					      "foreground-set", FALSE, NULL);
			}
		}
		
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, hdata->start_mark);
		gtk_text_buffer_place_cursor (buffer, &iter);
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (editor->priv->text), hdata->start_mark);
	}

	g_signal_emit (editor, query_editor_signals[CHANGED], 0);
}

const char *
get_date_format (time_t time)
{
        static char timebuf[200];
	GTimeVal now;
	unsigned long diff, tmp;

	g_get_current_time (&now);

	if (now.tv_sec < time)
		return _("In the future:\n");

	diff = now.tv_sec - time;
	if (diff < 60)
		return _("Less than a minute ago:\n");

	/* Turn it into minutes */
	tmp = diff / 60;
	if (tmp < 60) {
		snprintf (timebuf, sizeof(timebuf), ngettext ("%lu minute ago:\n",
							      "%lu minutes ago:\n", tmp), tmp);
		return timebuf;
	}
	/* Turn it into hours */
	tmp = diff / 3600;
	if (tmp < 24) {
		snprintf (timebuf, sizeof(timebuf), ngettext ("%lu hour ago\n",
							      "%lu hours ago\n", tmp), tmp);
		return timebuf;
	}
	/* Turn it into days */
	tmp = diff / 86400;
	snprintf (timebuf, sizeof(timebuf), ngettext ("%lu day ago\n",
						      "%lu days ago\n", tmp), tmp);

	return timebuf;
}

/**
 * query_editor_start_history_batch
 * @editor: a #QueryEditor widget.
 * @hist_batch: a #QueryEditorHistoryBatch to add, or %NULL
 *
 * @hist_hash ref is _NOT_ stolen here
 */
void
query_editor_start_history_batch (QueryEditor *editor, QueryEditorHistoryBatch *hist_batch)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	GtkTextTag *tag;
	HistItemData *hdata;
	gboolean empty = FALSE;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode == QUERY_EDITOR_HISTORY);
	
	if (!hist_batch) {
		GTimeVal run_time = {0, 0};
		empty = TRUE;
		
		hist_batch = query_editor_history_batch_new (run_time, NULL);
	}

	/* update editor->priv->insert_into_batch */
	if (editor->priv->insert_into_batch)
		query_editor_history_batch_unref (editor->priv->insert_into_batch);
	editor->priv->insert_into_batch = query_editor_history_batch_ref (hist_batch);
	editor->priv->batches_list = g_slist_prepend (editor->priv->batches_list,
						      query_editor_history_batch_ref (hist_batch));

	/* new HistItemData */
	hdata = hist_item_data_new ();
	hdata->batch = query_editor_history_batch_ref (hist_batch);
	hdata->item = NULL;
	g_hash_table_insert (editor->priv->hash, hist_batch, hdata);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	gtk_text_buffer_get_end_iter (buffer, &iter);
	
	/* mark start of insertion */
	hdata->start_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);	

	/* new tag */
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "pixels-below-lines", 3,
					  "foreground", "black",
					  "scale", PANGO_SCALE_SMALL,
					  "weight", PANGO_WEIGHT_BOLD, NULL);
	hdata->tag = g_object_ref (tag);
	g_hash_table_insert (editor->priv->hash, tag, hist_item_data_ref (hdata));
	
	if (!empty) { 
		/* insert text */
		gtk_text_buffer_insert_with_tags (buffer, &iter,
						  get_date_format (hist_batch->run_time.tv_sec),
						  -1, tag, NULL);
	}

	/* mark end of insertion */
	hdata->end_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);

	if (empty)
		query_editor_history_batch_unref (hist_batch);

	/* add timout to 1 sec. */
	editor->priv->ts_timeout_id  = g_timeout_add_seconds (60,
							      (GSourceFunc) timestamps_update_cb, editor);

	/* remove too old batches */
	if (g_slist_length (editor->priv->batches_list) > MAX_HISTORY_BATCH_ITEMS) {
		QueryEditorHistoryBatch *obatch;
		obatch = g_slist_last (editor->priv->batches_list)->data;
		query_editor_del_history_batch (editor, obatch);
	}
}

static gboolean
timestamps_update_cb (QueryEditor *editor)
{
	GSList *list;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	for (list = editor->priv->batches_list; list; list = list->next) {
		QueryEditorHistoryBatch *batch;
		batch = (QueryEditorHistoryBatch*) list->data;
		if (batch->run_time.tv_sec != 0) {
			HistItemData *hdata;
			hdata = g_hash_table_lookup (editor->priv->hash, batch);
			
			/* delete current text */
			gtk_text_buffer_get_iter_at_mark (buffer, &start, hdata->start_mark);
			gtk_text_buffer_get_iter_at_mark (buffer, &end, hdata->end_mark);
			gtk_text_buffer_delete (buffer, &start, &end);
			
			/* insert text */
			gtk_text_buffer_get_iter_at_mark (buffer, &start, hdata->start_mark);
			gtk_text_buffer_insert_with_tags (buffer, &start,
							  get_date_format (batch->run_time.tv_sec),
							  -1, hdata->tag, NULL);
			gtk_text_buffer_delete_mark (buffer, hdata->end_mark);
			hdata->end_mark = gtk_text_buffer_create_mark (buffer, NULL, &start, TRUE);
		}
	}
	return TRUE; /* don't remove timeout */
}

/**
 * query_editor_add_history_item
 * @editor: a #QueryEditor widget.
 * @hist_item: a #QueryEditorHistoryItem to add, or %NULL
 *
 * Adds some text. @text_data is usefull only if @editor's mode is HISTORY, it will be ignored
 * otherwise.
 *
 * Returns: the position of the added text chunk, or %0 if mode is not HISTORY
 */
void
query_editor_add_history_item (QueryEditor *editor, QueryEditorHistoryItem *hist_item)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	GtkTextTag *tag;
	HistItemData *hdata;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode == QUERY_EDITOR_HISTORY);
	g_return_if_fail (hist_item);
	g_return_if_fail (hist_item->sql);

	/* new HistItemData */
	hdata = hist_item_data_new ();
	hdata->batch = NULL;
	hdata->item = query_editor_history_item_ref (hist_item);
	g_hash_table_insert (editor->priv->hash, hist_item, hdata);

	/* remove leading and trailing spaces */
	g_strstrip (hist_item->sql);

	if (!editor->priv->insert_into_batch)
		query_editor_start_history_batch (editor, NULL);

	query_editor_history_batch_add_item (editor->priv->insert_into_batch, hist_item);
	hdata->batch = query_editor_history_batch_ref (editor->priv->insert_into_batch);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
	gtk_text_buffer_get_end_iter (buffer, &iter);

		
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "scale", 0.75,
					  "foreground", "gray",
					  "foreground-set", TRUE, NULL);
	if (hist_item->within_transaction)
		g_object_set (G_OBJECT (tag), "left-margin", 15, NULL);
	hdata->tag = g_object_ref (tag);
	g_hash_table_insert (editor->priv->hash, tag, hist_item_data_ref (hdata));

	/* mark start of insertion */
	hdata->start_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);

	/* insert text */
	gchar *sql, *tmp1, *tmp2;
	sql = g_strdup (hist_item->sql);
	for (tmp1 = sql; (*tmp1 == ' ') || (*tmp1 == '\n') || (*tmp1 == '\t'); tmp1 ++);
	for (tmp2 = sql + (strlen (sql) - 1);
	     (tmp2 > tmp1) && ((*tmp2 == ' ') || (*tmp2 == '\n') || (*tmp2 == '\t'));
	     tmp2--)
		*tmp2 = 0;
	gtk_text_buffer_insert_with_tags (buffer, &iter, tmp1, -1, tag, NULL);
	gtk_text_buffer_insert_with_tags (buffer, &iter, "\n", 1, tag, NULL);
	g_free (sql);

	/* mark end of insertion */
	gtk_text_buffer_get_end_iter (buffer, &iter);
	hdata->end_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);

	focus_on_hist_data (editor, hdata);
	gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (editor->priv->text),
				      &iter, 0., FALSE, 0., 0.);
}

/**
 * query_editor_get_current_history_item
 * @editor: a #QueryEditor widget.
 *
 * Get the current selected #QueryEditorHistoryItem
 * passed to query_editor_add_history_item().
 * 
 * Returns: a #QueryEditorHistoryItem pointer, or %NULL
 */
QueryEditorHistoryItem *
query_editor_get_current_history_item (QueryEditor *editor)
{
	g_return_val_if_fail (QUERY_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (editor->priv->mode == QUERY_EDITOR_HISTORY, NULL);

	if (editor->priv->hist_focus)
		return editor->priv->hist_focus->item;
	else
		return NULL;
}

/**
 * query_editor_get_current_history_batch
 * @editor: a #QueryEditor widget.
 *
 * Get the current selected #QueryEditorHistoryBatch if the selection is not on
 * a #QueryEditorHistoryItem, but on the #QueryEditorHistoryBatch which was last
 * set by a call to query_editor_start_history_batch().
 * 
 * Returns: a #QueryEditorHistoryItem pointer, or %NULL
 */
QueryEditorHistoryBatch *
query_editor_get_current_history_batch (QueryEditor *editor)
{
	g_return_val_if_fail (QUERY_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (editor->priv->mode == QUERY_EDITOR_HISTORY, NULL);

	if (editor->priv->hist_focus && !editor->priv->hist_focus->item)
		return editor->priv->hist_focus->batch;
	else
		return NULL;
}


/**
 * query_editor_del_current_history_item
 * @editor: a #QueryEditor widget.
 * 
 * Deletes the text associated to the current selection, usefull only if @editor's mode is HISTORY
 */
void
query_editor_del_current_history_item (QueryEditor *editor)
{
	HistItemData *hdata, *focus;
	GtkTextBuffer *buffer;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode == QUERY_EDITOR_HISTORY);

	if (!editor->priv->hist_focus)
		return;
	hdata = editor->priv->hist_focus;
	if (!hdata->item)
		return;

	focus = get_next_hist_data (editor, hdata);
	if (focus)
		focus_on_hist_data (editor, focus);
	else
		focus_on_hist_data (editor, get_prev_hist_data (editor, hdata));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));

	/* handle GtkTextBuffer's deletion */
	GtkTextIter start, end;
	gtk_text_buffer_get_iter_at_mark (buffer, &start, hdata->start_mark);
	gtk_text_buffer_get_iter_at_mark (buffer, &end, hdata->end_mark);
	gtk_text_buffer_delete (buffer, &start, &end);
	gtk_text_buffer_delete_mark (buffer, hdata->start_mark);
	gtk_text_buffer_delete_mark (buffer, hdata->end_mark);
	
	hist_item_data_ref (hdata);
	g_hash_table_remove (editor->priv->hash, hdata->item);
	g_hash_table_remove (editor->priv->hash, hdata->tag);
	
	g_assert (hdata->batch);
	query_editor_history_item_ref (hdata->item);
	query_editor_history_batch_del_item (hdata->batch, hdata->item);
	g_signal_emit (editor, query_editor_signals[HISTORY_ITEM_REMOVED], 0, hdata->item);
	query_editor_history_item_unref (hdata->item);

	if (! hdata->batch->hist_items) {
		/* remove hdata->batch */
		HistItemData *remhdata;
		
		editor->priv->batches_list = g_slist_remove (editor->priv->batches_list, hdata->batch);
		query_editor_history_batch_unref (hdata->batch);
		
		remhdata = g_hash_table_lookup (editor->priv->hash, hdata->batch);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, remhdata->start_mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &end, remhdata->end_mark);
		gtk_text_buffer_delete (buffer, &start, &end);
		gtk_text_buffer_delete_mark (buffer, remhdata->start_mark);
		gtk_text_buffer_delete_mark (buffer, remhdata->end_mark);
		
		g_hash_table_remove (editor->priv->hash, remhdata->batch);
		g_hash_table_remove (editor->priv->hash, remhdata->tag);
		
		if (editor->priv->insert_into_batch == hdata->batch) {
			query_editor_history_batch_unref (editor->priv->insert_into_batch);
			editor->priv->insert_into_batch = NULL;
		}
	}
	hist_item_data_unref (hdata);
}

/**
 * query_editor_del_history_batch
 * @editor: a #QueryEditor
 * @batch: a #QueryEditorHistoryBatch
 *
 * Deletes all regarding @batch.
 */
void
query_editor_del_history_batch (QueryEditor *editor, QueryEditorHistoryBatch *batch)
{
	GtkTextBuffer *buffer;
	GSList *list;
	HistItemData *hdata;
	GtkTextIter start, end;
	HistItemData *focus;
	gint i;

	g_return_if_fail (QUERY_IS_EDITOR (editor));
	g_return_if_fail (editor->priv->mode == QUERY_EDITOR_HISTORY);
	g_return_if_fail (batch);
	i = g_slist_index (editor->priv->batches_list, batch);	
	g_return_if_fail (i >= 0);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));

	/* compute new focus */
	focus = NULL;
	if (i > 0) {
		list = g_slist_nth (editor->priv->batches_list, i - 1);
		focus = g_hash_table_lookup (editor->priv->hash, list->data);
	}
	focus_on_hist_data (editor, focus);

	/* remove all history items */
	for (list =  batch->hist_items; list; list =  batch->hist_items) {
		hdata = g_hash_table_lookup (editor->priv->hash, list->data);
		g_assert (hdata);

		gtk_text_buffer_get_iter_at_mark (buffer, &start, hdata->start_mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &end, hdata->end_mark);
		gtk_text_buffer_delete (buffer, &start, &end);
		gtk_text_buffer_delete_mark (buffer, hdata->start_mark);
		gtk_text_buffer_delete_mark (buffer, hdata->end_mark);
		
		g_hash_table_remove (editor->priv->hash, hdata->item);
		g_hash_table_remove (editor->priv->hash, hdata->tag);
		query_editor_history_batch_del_item (batch, (QueryEditorHistoryItem*) list->data);
	}
	
	/* remove batch */
	editor->priv->batches_list = g_slist_remove (editor->priv->batches_list, batch);
	query_editor_history_batch_unref (batch);
	
	hdata = g_hash_table_lookup (editor->priv->hash, batch);
	gtk_text_buffer_get_iter_at_mark (buffer, &start, hdata->start_mark);
	gtk_text_buffer_get_iter_at_mark (buffer, &end, hdata->end_mark);
	gtk_text_buffer_delete (buffer, &start, &end);
	gtk_text_buffer_delete_mark (buffer, hdata->start_mark);
	gtk_text_buffer_delete_mark (buffer, hdata->end_mark);
	
	if (editor->priv->insert_into_batch == batch) {
		query_editor_history_batch_unref (editor->priv->insert_into_batch);
		editor->priv->insert_into_batch = NULL;
	}

	g_hash_table_remove (editor->priv->hash, hdata->batch);
	g_hash_table_remove (editor->priv->hash, hdata->tag);	
}

static HistItemData *
get_next_hist_data (QueryEditor *editor, HistItemData *hdata)
{
	GSList *node;
	g_return_val_if_fail (hdata, NULL);
	g_assert (hdata->batch);
	
	if (hdata->item) {
		node = g_slist_find (hdata->batch->hist_items, hdata->item);
		g_assert (node);
		node = node->next;
		if (node)
			return g_hash_table_lookup (editor->priv->hash, node->data);
	}
	else
		if (hdata->batch->hist_items)
			return g_hash_table_lookup (editor->priv->hash, hdata->batch->hist_items->data);

	/* move on to the next batch if any */
	gint i;
	i = g_slist_index (editor->priv->batches_list, hdata->batch);
	if (i > 0) {
		node = g_slist_nth (editor->priv->batches_list, i-1);
		hdata = g_hash_table_lookup (editor->priv->hash, node->data);
		return get_next_hist_data (editor, hdata);
	}
	return NULL;
}

static HistItemData *
get_prev_hist_data (QueryEditor *editor, HistItemData *hdata)
{
	GSList *node;
	gint i;
	g_return_val_if_fail (hdata, NULL);
	g_assert (hdata->batch);
	
	if (hdata->item) {
		node = g_slist_find (hdata->batch->hist_items, hdata->item);
		g_assert (node);
		i = g_slist_position (hdata->batch->hist_items, node);
		if (i > 0) {
			node = g_slist_nth (hdata->batch->hist_items, i - 1);
			return g_hash_table_lookup (editor->priv->hash, node->data);
		}
	}

	/* move to the previous batch, if any */
	node = g_slist_find (editor->priv->batches_list, hdata->batch);
	node = node->next;
	while (node) {
		QueryEditorHistoryBatch *b;
		b = (QueryEditorHistoryBatch*) node->data;
		GSList *l;
		l = g_slist_last (b->hist_items);
		if (l)
			return g_hash_table_lookup (editor->priv->hash, l->data);
		node = node->next;
	}

	return NULL;
}

/**
 * query_editor_get_all_text
 * @editor: a #QueryEditor widget.
 *
 * Retrieve the full contents of the given editor widget.
 *
 * Returns: the current contents of the editor buffer. You must free
 * the returned value when no longer needed.
 */
gchar *
query_editor_get_all_text (QueryEditor *editor)
{
	g_return_val_if_fail (QUERY_IS_EDITOR (editor), NULL);

	GtkTextBuffer *buffer;
        GtkTextIter start;
        GtkTextIter end;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);

        return gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                         &start, &end, FALSE);
}

/**
 * query_editor_load_from_file
 * @editor: a #QueryEditor widget.
 * @filename: the file to be loaded.
 *
 * Load the given filename into the editor widget.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
query_editor_load_from_file (QueryEditor *editor, const gchar *filename)
{
	gboolean retval = TRUE;
	gchar *contents;

	g_return_val_if_fail (QUERY_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		query_editor_set_text (editor, contents);
		g_free (contents);
	}
	else 
		retval = FALSE;

	return retval;
}

/**
 * query_editor_save_to_file
 * @editor: a #QueryEditor widget.
 * @filename: the file to save to.
 *
 * Save the current editor contents to the given file.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
query_editor_save_to_file (QueryEditor *editor, const gchar *filename)
{
	gchar *contents;
	gboolean retval = TRUE;

	g_return_val_if_fail (QUERY_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	contents = query_editor_get_all_text (editor);
	if (!g_file_set_contents (filename, contents, strlen (contents), NULL))
		retval = FALSE;

	g_free (contents);

	return retval;
}

/**
 * query_editor_copy_clipboard
 * @editor: a #QueryEditor widget.
 *
 * Copy currently selected text in the given editor widget to the clipboard.
 */
void
query_editor_copy_clipboard (QueryEditor *editor)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));
	gtk_text_buffer_copy_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                        gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
}

/**
 * query_editor_cut_clipboard
 * @editor: a #QueryEditor widget.
 *
 * Moves currently selected text in the given editor widget to the clipboard.
 */
void
query_editor_cut_clipboard (QueryEditor *editor)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));

	gtk_text_buffer_cut_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                       gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				       gtk_text_view_get_editable (GTK_TEXT_VIEW (editor->priv->text)));
}

/**
 * query_editor_paste_clipboard
 * @editor: a #QueryEditor widget.
 *
 * Paste clipboard contents into editor widget, at the current position.
 */
void
query_editor_paste_clipboard (QueryEditor *editor)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));

	gtk_text_buffer_paste_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                         gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
                                         NULL,
                                         gtk_text_view_get_editable (GTK_TEXT_VIEW (editor->priv->text)));
}

/*
 * QueryEditorHistoryBatch
 */
QueryEditorHistoryBatch *
query_editor_history_batch_new (GTimeVal run_time, GdaSet *params)
{
	QueryEditorHistoryBatch *qib;
	
	qib = g_new0 (QueryEditorHistoryBatch, 1);
	qib->ref_count = 1;
	qib->run_time = run_time;
	if (params)
		qib->params = gda_set_copy (params);

	return qib;
}

QueryEditorHistoryBatch *
query_editor_history_batch_ref (QueryEditorHistoryBatch *qib)
{
	g_return_val_if_fail (qib, NULL);
	qib->ref_count++;
	return qib;
}

void
query_editor_history_batch_unref (QueryEditorHistoryBatch *qib)
{
	g_return_if_fail (qib);
	qib->ref_count--;
	if (qib->ref_count <= 0) {
		if (qib->hist_items) {
			g_slist_foreach (qib->hist_items, (GFunc) query_editor_history_item_unref, NULL);
			g_slist_free (qib->hist_items);
		}
		if (qib->params)
			g_object_unref (qib->params);

		g_free (qib);
	}
}

static void
query_editor_history_batch_add_item (QueryEditorHistoryBatch *qib, QueryEditorHistoryItem *qih)
{
	g_return_if_fail (qib);
	g_return_if_fail (qih);
	qib->hist_items = g_slist_append (qib->hist_items, query_editor_history_item_ref (qih));
}

static void
query_editor_history_batch_del_item (QueryEditorHistoryBatch *qib, QueryEditorHistoryItem *qih)
{
	g_return_if_fail (qib);
	g_return_if_fail (qih);
	qib->hist_items = g_slist_remove (qib->hist_items, qih);
	query_editor_history_item_unref (qih);
}

/*
 * QueryEditorHistoryItem
 */
QueryEditorHistoryItem *
query_editor_history_item_new (const gchar *sql, GObject *result, GError *error)
{
	QueryEditorHistoryItem *qih;
	
	g_return_val_if_fail (sql, NULL);

	qih = g_new0 (QueryEditorHistoryItem, 1);
	qih->ref_count = 1;
	qih->sql = g_strdup (sql);
	if (result)
		qih->result = g_object_ref (result);
	if (error)
		qih->exec_error = g_error_copy (error);

	return qih;
}

QueryEditorHistoryItem *
query_editor_history_item_ref (QueryEditorHistoryItem *qih)
{
	g_return_val_if_fail (qih, NULL);
	qih->ref_count++;
	return qih;
}

void
query_editor_history_item_unref (QueryEditorHistoryItem *qih)
{
	g_return_if_fail (qih);
	qih->ref_count--;
	if (qih->ref_count <= 0) {
		g_free (qih->sql);
		if (qih->result)
			g_object_unref (qih->result);
		if (qih->exec_error)
			g_error_free (qih->exec_error);
		g_free (qih);
	}
}

/*
 * HistItemData
 */
static HistItemData *
hist_item_data_new (void)
{
	HistItemData *hdata;
	hdata = g_new0 (HistItemData, 1);
	hdata->ref_count = 1;
	return hdata;
}

static HistItemData *
hist_item_data_ref (HistItemData *hdata)
{
	g_return_val_if_fail (hdata, NULL);
	hdata->ref_count ++;
	return hdata;
}

static void
hist_item_data_unref (HistItemData *hdata)
{
	g_return_if_fail (hdata);
	hdata->ref_count --;
	if (hdata->ref_count <= 0) {
		if (hdata->batch)
			query_editor_history_batch_unref (hdata->batch);
		if (hdata->item)
			query_editor_history_item_unref (hdata->item);
		if (hdata->tag)
			g_object_unref (hdata->tag);
		g_free (hdata);
	}
}
