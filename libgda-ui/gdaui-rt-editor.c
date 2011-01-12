/* gdaui-rt-editor.c
 *
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include "gdaui-rt-editor.h"
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include "bullet.h"
#include "bulleth.h"
#define MAX_BULLETS 2
//gchar * bullet_strings[] = {"•", "◦"};

GdkPixbuf *bullet_pix[MAX_BULLETS] = {NULL};
gchar     *lists_tokens[MAX_BULLETS] = {"- ", " - "};

static void gdaui_rt_editor_class_init (GdauiRtEditorClass *klass);
static void gdaui_rt_editor_init (GdauiRtEditor *wid);
static void gdaui_rt_editor_dispose (GObject *object);

static void gdaui_rt_editor_set_property (GObject *object,
					  guint param_id,
					  const GValue *value,
					  GParamSpec *pspec);
static void gdaui_rt_editor_get_property (GObject *object,
					  guint param_id,
					  GValue *value,
					  GParamSpec *pspec);
static void gdaui_rt_editor_show_all (GtkWidget *widget);

static void _gdaui_rt_editor_set_show_markup (GdauiRtEditor *editor, gboolean show_markup);

/* tag types */
enum {
	TEXT_TAG_ITALIC,
	TEXT_TAG_BOLD,
	TEXT_TAG_TT,
	TEXT_TAG_VERBATIM,
	TEXT_TAG_UNDERLINE,
	TEXT_TAG_STRIKE,
	TEXT_TAG_TITLE1,
	TEXT_TAG_TITLE2,
	TEXT_TAG_LIST1,
	TEXT_TAG_LIST2,

	TEXT_TAG_LAST
};

typedef struct {
	GtkTextTag *tag;
	gchar      *action_name;
} TagData;

struct _GdauiRtEditorPriv
{
	GtkTextView    *textview;
	GtkTextBuffer  *textbuffer;
	GtkWidget      *toolbar;
	GtkActionGroup *actions_group;
	GtkUIManager   *uimanager;
	TagData         tags[TEXT_TAG_LAST];
	gboolean        selection_changing;
	gboolean        show_markup;
	gchar          *saved_for_help;
	gboolean        enable_changed_signal;
	gboolean        no_background;
	gint            insert_offset;

	gboolean        contents_setting; /* TRUE if whole contents is being changed */
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;
static gchar *help_str=N_("\"\"\"= Title level 1 =\n"
			  "== Title level 2 ==\n"
			  "\"\"\"= Title level 1 =\n"
			  "== Title level 2 ==\n\n"
			  "\"\"\""
			  "For beautifiers we have **bold**\n"
			  "and //italic//.\n"
			  "There is also __underline__, --strike--\n"
			  "and ``monospaced``.\n"
			  "\"\"\"\n"
			  "For beautifiers we have **bold**\n"
			  "and //italic//.\n"
			  "There is also __underline__, --strike--\n"
			  "and ``monospaced``.\n\n"
			  "\"\"\""
			  "- This is a list of items\n"
			  "- Just use hyphens\n"
			  " - And starting space for indenting\n"
			  "\"\"\"- This is a list of items\n"
			  "- Just use hyphens\n"
			  " - And starting space for indenting\n"
			  "\nRaw areas are enclosed inside three doublequotes and no markup is interpreted");

/* signals */
enum {
	CHANGED,
	LAST_SIGNAL
};

static gint gdaui_rt_editor_signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
	PROP_0,
	PROP_NO_BACKGROUND,
	PROP_SHOW_MARKUP,
	PROP_TEXTBUFFER
};

/* global pixbufs */
static gint spaces_since_start_of_line (GtkTextIter *iter);
static gchar *real_gdaui_rt_editor_get_contents (GdauiRtEditor *editor);

static GtkTextTag *iter_begins_list (GdauiRtEditor *rte, GtkTextIter *iter, gint *out_list_level);
static void mark_set_cb (GtkTextBuffer *textbuffer, GtkTextIter *location,
			 GtkTextMark *mark, GdauiRtEditor *rte);
static void insert_text_cb (GtkTextBuffer *textbuffer, GtkTextIter *location, gchar *text, gint len, GdauiRtEditor *rte);
static void insert_text_after_cb (GtkTextBuffer *textbuffer, GtkTextIter *location, gchar *text, gint len, GdauiRtEditor *rte);
static void text_buffer_changed_cb (GtkTextBuffer *textbuffer, GdauiRtEditor *rte);
static void populate_popup_cb (GtkTextView *entry, GtkMenu *menu, GdauiRtEditor *rte);

static void show_hide_toolbar (GdauiRtEditor *editor);

static gchar *add_newlines_to_base64 (gchar *base64);
static gchar *remove_newlines_from_base64 (gchar *base64);

static void italic_cb (GtkToggleAction *action, GdauiRtEditor *rte);
static void strike_cb (GtkToggleAction *action, GdauiRtEditor *rte);
static void underline_cb (GtkToggleAction *action, GdauiRtEditor *rte);
static void bold_cb (GtkToggleAction *action, GdauiRtEditor *rte);
static void reset_all_cb (GtkAction *action, GdauiRtEditor *rte);
static void add_image_cb (GtkAction *action, GdauiRtEditor *rte);
static void help_cb (GtkToggleAction *action, GdauiRtEditor *rte);

static const GtkToggleActionEntry ui_toggle_actions [] =
{
        { "ActionBold", GTK_STOCK_BOLD, N_("_Bold"), NULL, N_("Bold text"), G_CALLBACK (bold_cb), FALSE},
        { "ActionItalic", GTK_STOCK_ITALIC, N_("_Italic"), NULL, N_("Italic text"), G_CALLBACK (italic_cb), FALSE},
        { "ActionUnderline", GTK_STOCK_UNDERLINE, N_("_Underline"), NULL, N_("Underline text"), G_CALLBACK (underline_cb), FALSE},
        { "ActionStrike", GTK_STOCK_STRIKETHROUGH, N_("_Strike through"), NULL, N_("Strike through text"), G_CALLBACK (strike_cb), FALSE},
        { "ActionHelp", GTK_STOCK_HELP, N_("_Syntax help"), NULL, N_("Show syntax help"), G_CALLBACK (help_cb), FALSE}
};

/* STOCK_SELECT_COLOR */
static const GtkActionEntry ui_actions[] = {
        { "ActionAddImage", "insert-image", N_("_Add image"), NULL, N_("Insert image"), G_CALLBACK (add_image_cb)},
        { "ActionReset", GTK_STOCK_CLEAR, N_("_Reset"), NULL, N_("Reset to normal text"), G_CALLBACK (reset_all_cb)},
};

static const gchar *ui_actions_info =
        "<ui>"
        "  <toolbar name='ToolBar'>"
        "    <toolitem action='ActionBold'/>"
        "    <toolitem action='ActionItalic'/>"
        "    <toolitem action='ActionUnderline'/>"
        "    <toolitem action='ActionStrike'/>"
        "    <toolitem action='ActionAddImage'/>"
        "    <toolitem action='ActionReset'/>"
        "    <toolitem action='ActionHelp'/>"
        "  </toolbar>"
        "</ui>";

GType
gdaui_rt_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiRtEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_rt_editor_class_init,
			NULL,
			NULL,
			sizeof (GdauiRtEditor),
			0,
			(GInstanceInitFunc) gdaui_rt_editor_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiRtEditor", &info, 0);
	}

	return type;
}

static void
gdaui_rt_editor_class_init (GdauiRtEditorClass *klass)
{
	GObjectClass  *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gdaui_rt_editor_dispose;

	GTK_WIDGET_CLASS (klass)->show_all = gdaui_rt_editor_show_all;

	/* signals */
	gdaui_rt_editor_signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdauiRtEditorClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

	/* Properties */
        object_class->set_property = gdaui_rt_editor_set_property;
        object_class->get_property = gdaui_rt_editor_get_property;
	/**
	 * GdauiRtEditor:no-background:
	 *
	 * If set to %TRUE, then the default text background is removed
	 * and thus the textbackground is the default widget's background.
	 *
	 * This property has to be set before the widget is realized, and is taken into account only
	 * if the widget is not editable (when it's realized).
	 **/
	g_object_class_install_property (object_class, PROP_NO_BACKGROUND,
                                         g_param_spec_boolean ("no-background",
                                                               _("Don't display a specific background for the text"),
                                                               NULL, FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * GdauiRtEditor:show-markup:
	 *
	 * Instead of showing the formatted text, display the raw text (in the txt2tags syntax)
	 **/
	g_object_class_install_property (object_class, PROP_SHOW_MARKUP,
					 g_param_spec_boolean ("show-markup",
                                                               _("Display raw markup text instead of formatted text"),
                                                               NULL, FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));

	/**
	 * GdauiRtEditor:buffer:
	 *
	 * Get access to the actual #GtkTextBuffer used. Do not modify it!
	 **/
	g_object_class_install_property (object_class, PROP_TEXTBUFFER,
					 g_param_spec_object ("buffer",
							      _("The buffer which is displayed"),
							      NULL, GTK_TYPE_TEXT_BUFFER,
							      G_PARAM_READABLE));
}

static void
text_view_realized_cb (GtkWidget *tv, GdauiRtEditor *rte)
{
	GdkWindow *win;
	GtkStyle *style;
	if (rte->priv->no_background && ! gtk_text_view_get_editable (GTK_TEXT_VIEW (tv))) {
		win = gtk_text_view_get_window (GTK_TEXT_VIEW (tv), GTK_TEXT_WINDOW_TEXT);
		style = gtk_widget_get_style (tv);
		gdk_window_set_background (win, &(style->bg [GTK_STATE_NORMAL]));
	}
}

static void
gdaui_rt_editor_init (GdauiRtEditor *rte)
{
	GtkWidget *sw, *textview, *toolbar;

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_end (GTK_BOX (rte), sw, TRUE, TRUE, 0);

	textview = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
	gtk_container_add (GTK_CONTAINER (sw), textview);
	g_signal_connect (textview, "realize",
			  G_CALLBACK (text_view_realized_cb), rte);

	gtk_widget_show_all (sw);

	rte->priv = g_new0 (GdauiRtEditorPriv, 1);
	rte->priv->saved_for_help = NULL;
	rte->priv->enable_changed_signal = TRUE;
	rte->priv->no_background = FALSE;
	rte->priv->insert_offset = -1;
	rte->priv->textview = GTK_TEXT_VIEW (textview);
	rte->priv->textbuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	rte->priv->contents_setting = FALSE;
	g_signal_connect (rte->priv->textbuffer, "changed",
			  G_CALLBACK (text_buffer_changed_cb), rte);
	g_signal_connect (rte->priv->textbuffer, "mark-set",
			  G_CALLBACK (mark_set_cb), rte);
	g_signal_connect (rte->priv->textbuffer, "insert-text",
			  G_CALLBACK (insert_text_cb), rte);
	g_signal_connect_after (rte->priv->textbuffer, "insert-text",
				G_CALLBACK (insert_text_after_cb), rte);
	g_signal_connect (rte->priv->textview, "populate-popup",
			  G_CALLBACK (populate_popup_cb), rte);

	/* tags. REM: leave the LIST* and BULLET tags defined 1st as they will be with less priority
	 * and it affects the result returned by gtk_text_iter_get_tags() */
	rte->priv->show_markup = FALSE;
	memset (rte->priv->tags, 0, sizeof (rte->priv->tags));

	rte->priv->tags[TEXT_TAG_LIST1].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									  "indent", -5,
									  "left_margin", 15,
									  /*"background", "#cbbcbc",*/
									  NULL);
	rte->priv->tags[TEXT_TAG_LIST2].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									  "indent", -10,
									  "left_margin", 25,
									  /*"background", "#dcbcbc",*/
									  NULL);
	rte->priv->tags[TEXT_TAG_ITALIC].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									   "style", PANGO_STYLE_ITALIC, NULL);
	rte->priv->tags[TEXT_TAG_ITALIC].action_name = "/ToolBar/ActionItalic";

	rte->priv->tags[TEXT_TAG_BOLD].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									 "weight", PANGO_WEIGHT_BOLD, NULL);
	rte->priv->tags[TEXT_TAG_BOLD].action_name = "/ToolBar/ActionBold";

	rte->priv->tags[TEXT_TAG_TT].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
								       "family", "Monospace", NULL);
	rte->priv->tags[TEXT_TAG_VERBATIM].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									     "background", "#e5e2e2",
									     NULL);
	
	rte->priv->tags[TEXT_TAG_UNDERLINE].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									      "underline", PANGO_UNDERLINE_SINGLE, NULL);
	rte->priv->tags[TEXT_TAG_UNDERLINE].action_name = "/ToolBar/ActionUnderline";
	
	rte->priv->tags[TEXT_TAG_STRIKE].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									   "strikethrough", TRUE, NULL);
	rte->priv->tags[TEXT_TAG_STRIKE].action_name = "/ToolBar/ActionStrike";
	
	rte->priv->tags[TEXT_TAG_TITLE1].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									   "size", 15 * PANGO_SCALE,
									   "weight", PANGO_WEIGHT_SEMIBOLD,
									   NULL);
	rte->priv->tags[TEXT_TAG_TITLE2].tag = gtk_text_buffer_create_tag (rte->priv->textbuffer, NULL,
									   "size", 13 * PANGO_SCALE,
									   "weight", PANGO_WEIGHT_SEMIBOLD,
									   NULL);

	/* action group */
	rte->priv->actions_group = gtk_action_group_new ("Actions");
	gtk_action_group_add_toggle_actions (rte->priv->actions_group, ui_toggle_actions,
					     G_N_ELEMENTS (ui_toggle_actions), rte);
        gtk_action_group_add_actions (rte->priv->actions_group, ui_actions, G_N_ELEMENTS (ui_actions),
				      rte);

	/* ui manager */
	rte->priv->uimanager = gtk_ui_manager_new ();
        gtk_ui_manager_insert_action_group (rte->priv->uimanager, rte->priv->actions_group, 0);
        gtk_ui_manager_add_ui_from_string (rte->priv->uimanager, ui_actions_info, -1, NULL);

	/* toolbar */
	toolbar = gtk_ui_manager_get_widget (rte->priv->uimanager, "/ToolBar");
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);
	rte->priv->toolbar = toolbar;
	gtk_box_pack_end (GTK_BOX (rte), toolbar, FALSE, FALSE, 0);
	gtk_widget_show (toolbar);
}

/**
 * gdaui_rt_editor_new:
 *
 * Creates a new #GdauiRtEditor widget
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2.2
 */
GtkWidget *
gdaui_rt_editor_new ()
{
	GtkWidget *rte;

	rte = (GtkWidget *) g_object_new (GDAUI_TYPE_RT_EDITOR, NULL);

	return rte;
}

static void
gdaui_rt_editor_dispose (GObject *object)
{
	GdauiRtEditor *rte;

	g_return_if_fail (GDAUI_IS_RT_EDITOR (object));
	rte = GDAUI_RT_EDITOR (object);

	if (rte->priv) {
		if (rte->priv->actions_group) {
			g_object_unref (G_OBJECT (rte->priv->actions_group));
			rte->priv->actions_group = NULL;
		}

		if (rte->priv->uimanager)
			g_object_unref (rte->priv->uimanager);

		g_free (rte->priv->saved_for_help);
		/* NB: GtkTextTags are owned by the GtkTextBuffer's text tags table */

		/* the private area itself */
		g_free (rte->priv);
		rte->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
gdaui_rt_editor_show_all (GtkWidget *widget)
{
	GdauiRtEditor *editor;
	editor = GDAUI_RT_EDITOR (widget);
	GTK_WIDGET_CLASS (parent_class)->show_all (widget);
	show_hide_toolbar (editor);
}

static void
gdaui_rt_editor_set_property (GObject *object,
			      guint param_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	GdauiRtEditor *editor;

        editor = GDAUI_RT_EDITOR (object);
        if (editor->priv) {
                switch (param_id) {
		case PROP_NO_BACKGROUND:
			editor->priv->no_background = g_value_get_boolean (value);
			break;
		case PROP_SHOW_MARKUP:
			_gdaui_rt_editor_set_show_markup (editor, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
gdaui_rt_editor_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdauiRtEditor *editor;

        editor = GDAUI_RT_EDITOR (object);
        if (editor->priv) {
                switch (param_id) {
		case PROP_NO_BACKGROUND:
			g_value_set_boolean (value, editor->priv->no_background);
			break;
		case PROP_SHOW_MARKUP:
			g_value_set_boolean (value, editor->priv->show_markup);
			break;
		case PROP_TEXTBUFFER:
			g_value_set_object (value, editor->priv->textbuffer);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static GtkTextTag *
iter_begins_list (GdauiRtEditor *rte, GtkTextIter *iter, gint *out_list_level)
{
	gint idx = -1;
	GtkTextTag *tag = NULL;
	if (gtk_text_iter_has_tag (iter, rte->priv->tags[TEXT_TAG_LIST1].tag)) {
		tag = rte->priv->tags[TEXT_TAG_LIST1].tag;
		idx = 0;
	}
	else if (gtk_text_iter_has_tag (iter, rte->priv->tags[TEXT_TAG_LIST2].tag)) {
		tag = rte->priv->tags[TEXT_TAG_LIST2].tag;
		idx = 1;
	}
	if (out_list_level)
		*out_list_level = idx;
	return tag;
}

/* tags management */
static void
apply_tag (GdauiRtEditor *rte, gboolean reverse, GtkTextTag *tag)
{
	GtkTextIter start;
	GtkTextIter end;

	g_return_if_fail (rte->priv->textbuffer);

	if (rte->priv->selection_changing)
		return;

	if (gtk_text_buffer_get_selection_bounds (rte->priv->textbuffer, &start, &end)) {
		if (tag) {
			if (reverse)
				gtk_text_buffer_remove_tag (rte->priv->textbuffer, tag, &start, &end);
			else {
				gtk_text_buffer_apply_tag (rte->priv->textbuffer, tag, &start, &end);
				/* if there are LIST tags, then remove the applied tag */
				GtkTextIter iter;
				for (iter = start; gtk_text_iter_compare (&iter, &end) < 0; ) {
					GtkTextTag *ltag;
					gint idx;
					ltag = iter_begins_list (rte, &iter, &idx);
					if (ltag) {
						GtkTextIter liter;
						liter = iter;
						gtk_text_iter_forward_to_tag_toggle (&liter, ltag);
						gtk_text_iter_backward_char (&iter);
						gtk_text_buffer_remove_tag (rte->priv->textbuffer,
									    tag, &iter, &liter);
						gtk_text_iter_forward_char (&iter);
					}
					if (! gtk_text_iter_forward_char (&iter))
						break;
				}
			}
		}
		else {
			GtkTextIter iter;
			for (iter = start; gtk_text_iter_compare (&iter, &end) < 0; ) {
			
				GSList *tags, *list;
				tags = gtk_text_iter_get_tags (&iter);
				if (tags) {
					for (list = tags; list; list = list->next) {
						GtkTextTag *tag = (GtkTextTag *) list->data;
						if ((tag != rte->priv->tags[TEXT_TAG_LIST1].tag) &&
						    (tag != rte->priv->tags[TEXT_TAG_LIST2].tag))
							gtk_text_buffer_remove_tag (rte->priv->textbuffer,
										    tag, &start, &end);
					}
					g_slist_free (tags);
				}
				if (! gtk_text_iter_forward_char (&iter))
					break;
			}
		}
	}

	if (rte->priv->enable_changed_signal)
		g_signal_emit (rte, gdaui_rt_editor_signals[CHANGED], 0, NULL);

}

static void
help_cb (GtkToggleAction *action, GdauiRtEditor *rte)
{
	if (gtk_toggle_action_get_active (action)) {
		rte->priv->enable_changed_signal = FALSE;
		g_free (rte->priv->saved_for_help);
		rte->priv->saved_for_help = gdaui_rt_editor_get_contents (rte);
		gdaui_rt_editor_set_contents (rte, help_str, -1);
	}
	else {
		gdaui_rt_editor_set_contents (rte, rte->priv->saved_for_help, -1);
		rte->priv->enable_changed_signal = TRUE;
		g_free (rte->priv->saved_for_help);
		rte->priv->saved_for_help = NULL;
	}
}

static void
italic_cb (GtkToggleAction *action, GdauiRtEditor *rte)
{
	apply_tag (rte, gtk_toggle_action_get_active (action) ? FALSE : TRUE,
		   rte->priv->tags [TEXT_TAG_ITALIC].tag);
}

static void
bold_cb (GtkToggleAction *action, GdauiRtEditor *rte)
{
	apply_tag (rte, gtk_toggle_action_get_active (action) ? FALSE : TRUE,
		   rte->priv->tags [TEXT_TAG_BOLD].tag);
}

static void
strike_cb (GtkToggleAction *action, GdauiRtEditor *rte)
{
	apply_tag (rte, gtk_toggle_action_get_active (action) ? FALSE : TRUE,
		   rte->priv->tags [TEXT_TAG_STRIKE].tag);
}

static void
underline_cb (GtkToggleAction *action, GdauiRtEditor *rte)
{
	apply_tag (rte, gtk_toggle_action_get_active (action) ? FALSE : TRUE,
		   rte->priv->tags [TEXT_TAG_UNDERLINE].tag);
}

static void
reset_all_cb (G_GNUC_UNUSED GtkAction *action, GdauiRtEditor *rte)
{
	apply_tag (rte, FALSE, NULL);
}

static void
add_image_cb (G_GNUC_UNUSED GtkAction *action, GdauiRtEditor *rte)
{
	GtkWidget *dlg;
        GtkFileFilter *filter;

        dlg = gtk_file_chooser_dialog_new (_("Select image to load"),
                                           GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) rte)),
                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                           NULL);
        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dlg), filter);

        if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT) {
                char *filename;
                GError *error = NULL;

                filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));

		GdkPixbuf *pixbuf;
		pixbuf = gdk_pixbuf_new_from_file (filename, &error);
		if (pixbuf) {
			GtkTextIter start;
			GtkTextIter end;
			gboolean ret = FALSE;
			
			ret = gtk_text_buffer_get_selection_bounds (rte->priv->textbuffer, &start, &end);
			if (ret)
				gtk_text_buffer_delete (rte->priv->textbuffer, &start, &end);
			
			gtk_text_buffer_get_iter_at_mark (rte->priv->textbuffer, &start,
							  gtk_text_buffer_get_insert (rte->priv->textbuffer));
			gtk_text_buffer_insert_pixbuf (rte->priv->textbuffer, &start, pixbuf);
			g_object_unref (pixbuf);
		}
		else {
			GtkWidget *msg;

                        msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) rte)),
                                                                  GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                                  GTK_BUTTONS_CLOSE,
                                                                  _("Could not load the contents of '%s':\n %s"),
                                                                  filename,
                                                                  error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
                        gtk_widget_destroy (dlg);
                        dlg = NULL;
			
                        gtk_dialog_run (GTK_DIALOG (msg));
                        gtk_widget_destroy (msg);
		}
	}

	gtk_widget_destroy (dlg);
}

static void
mark_set_cb (GtkTextBuffer *textbuffer, GtkTextIter *location, GtkTextMark *mark, GdauiRtEditor *rte)
{
	if (mark == gtk_text_buffer_get_insert (textbuffer)) {
		GtkAction *action;
		gboolean act;
		gint i;

		rte->priv->selection_changing = TRUE;

		for (i = 0; i < TEXT_TAG_LAST; i++) {
			if (! rte->priv->tags[i].action_name)
				continue;

			action = gtk_ui_manager_get_action (rte->priv->uimanager,
							    rte->priv->tags[i].action_name);
			if (gtk_text_buffer_get_has_selection (textbuffer))
				act = FALSE;
			else
				act = gtk_text_iter_has_tag (location, rte->priv->tags [i].tag);
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), act);
		}

		rte->priv->selection_changing = FALSE;
	}
}

static void
insert_text_cb (GtkTextBuffer *textbuffer, GtkTextIter *location, G_GNUC_UNUSED gchar *text, G_GNUC_UNUSED gint len, GdauiRtEditor *rte)
{
	/* if inserting is before a bullet, then insert right after */
	GtkTextTag *tag;
	tag = iter_begins_list (rte, location, NULL);
	if (tag) {
		gtk_text_iter_forward_char (location);
		gtk_text_buffer_place_cursor (textbuffer, location);
	}
	rte->priv->insert_offset = gtk_text_iter_get_offset (location);
}

static void
insert_text_after_cb (GtkTextBuffer *textbuffer, GtkTextIter *location, gchar *text, G_GNUC_UNUSED gint len, GdauiRtEditor *rte)
{
	GtkTextIter start, end;

	if ((rte->priv->insert_offset < 0) || rte->priv->show_markup)
		return;

	/* disable any extra modification while text is being set using gdaui_rt_editor_set_contents() */
	if (rte->priv->contents_setting)
		return;

	/* apply selected tag in toolbar if any */
	gtk_text_buffer_get_iter_at_offset (textbuffer, &start, rte->priv->insert_offset);
	end = *location;
	if (gtk_text_iter_backward_chars (&end, g_utf8_strlen (text, -1))) {
		gint i;
		for (i = 0; i < TEXT_TAG_LAST; i++) {
			GtkAction *action;
			if (! rte->priv->tags[i].action_name)
				continue;
			action = gtk_ui_manager_get_action (rte->priv->uimanager,
							    rte->priv->tags[i].action_name);
			if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
				gtk_text_buffer_apply_tag (rte->priv->textbuffer,
							   rte->priv->tags[i].tag, location, &end);
		}
	}
	rte->priv->insert_offset = -1;

	gtk_text_iter_set_line_offset (&start, 0);
	/* add new bullet if already in list */
	if (*text == '\n') {
		gchar *text_to_insert = NULL;
		GtkTextTag *tag;
		gint index;
		tag = iter_begins_list (rte, &start, &index);
		if (tag)
			text_to_insert = lists_tokens [index];

		if (text_to_insert) {
			if (! gtk_text_iter_forward_char (&end))
				gtk_text_buffer_get_end_iter (textbuffer, &end);
			gtk_text_buffer_insert (textbuffer, &end, text_to_insert, -1);
		}
	}
	else {
		GtkTextTag *tag = NULL;
		end = *location;
		
		if (gtk_text_iter_begins_tag (&start, rte->priv->tags[TEXT_TAG_TITLE1].tag))
			tag = rte->priv->tags[TEXT_TAG_TITLE1].tag;
		else if (gtk_text_iter_begins_tag (&start, rte->priv->tags[TEXT_TAG_TITLE2].tag))
			tag = rte->priv->tags[TEXT_TAG_TITLE2].tag;
		if (tag)
			gtk_text_buffer_apply_tag (rte->priv->textbuffer, tag, &start, &end);
	}
}

static void
show_markup_item_activate_cb (GtkCheckMenuItem *checkmenuitem, GdauiRtEditor *rte)
{
	gboolean show;
	show = gtk_check_menu_item_get_active (checkmenuitem);
	_gdaui_rt_editor_set_show_markup (rte, show);
}

static void
bigger_font_item_activate_cb (G_GNUC_UNUSED GtkCheckMenuItem *checkmenuitem,
			      GdauiRtEditor *rte)
{
	PangoContext *pcontext;
	PangoFontDescription *fd, *nfd;
	pcontext = gtk_widget_get_pango_context (GTK_WIDGET (rte->priv->textview));
	fd = pango_context_get_font_description (pcontext);
	nfd = pango_font_description_copy_static (fd);
	pango_font_description_set_size (nfd, pango_font_description_get_size (fd) * 1.2);
	gtk_widget_modify_font (GTK_WIDGET (rte->priv->textview), nfd);
	pango_font_description_free (nfd);
}

static void
smaller_font_item_activate_cb (G_GNUC_UNUSED GtkCheckMenuItem *checkmenuitem,
			       GdauiRtEditor *rte)
{
	PangoContext *pcontext;
	PangoFontDescription *fd, *nfd;
	pcontext = gtk_widget_get_pango_context (GTK_WIDGET (rte->priv->textview));
	fd = pango_context_get_font_description (pcontext);
	nfd = pango_font_description_copy_static (fd);
	pango_font_description_set_size (nfd, pango_font_description_get_size (fd) / 1.2);
	gtk_widget_modify_font (GTK_WIDGET (rte->priv->textview), nfd);
	pango_font_description_free (nfd);
}

static void
reset_font_item_activate_cb (G_GNUC_UNUSED GtkCheckMenuItem *checkmenuitem,
			     GdauiRtEditor *rte)
{
	gtk_widget_modify_font (GTK_WIDGET (rte->priv->textview), NULL);
}

static void
populate_popup_cb (G_GNUC_UNUSED GtkTextView *entry, GtkMenu *menu, GdauiRtEditor *rte)
{
	GtkWidget *item;

	item = gtk_separator_menu_item_new ();
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Reset font size"));
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
        g_signal_connect (G_OBJECT (item), "activate",
                          G_CALLBACK (reset_font_item_activate_cb), rte);
        gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Decrease font size (zoom out)"));
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
        g_signal_connect (G_OBJECT (item), "activate",
                          G_CALLBACK (smaller_font_item_activate_cb), rte);
        gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Increase font size (zoom in)"));
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
        g_signal_connect (G_OBJECT (item), "activate",
                          G_CALLBACK (bigger_font_item_activate_cb), rte);
        gtk_widget_show (item);

	item = gtk_check_menu_item_new_with_label (_("Show source markup"));
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), rte->priv->show_markup);
        g_signal_connect (G_OBJECT (item), "toggled",
                          G_CALLBACK (show_markup_item_activate_cb), rte);
        gtk_widget_show (item);
}

/* RTE markup analysis */
typedef enum {
	MARKUP_NONE,      /* 0 */
	MARKUP_BOLD,
	MARKUP_TT,
	MARKUP_VERBATIM,
	MARKUP_ITALIC,
	MARKUP_STRIKE,    /* 5 */
	MARKUP_UNDERLINE,

	MARKUP_TITLE1_S,
	MARKUP_TITLE1_E,
	MARKUP_TITLE2_S,
	MARKUP_TITLE2_E,  /* 10 */

	MARKUP_PICTURE_S,
	MARKUP_PICTURE_E,

	MARKUP_LIST_S,
	MARKUP_LIST_E,

	MARKUP_EOF
} MarkupTag;

typedef struct {
	GtkTextMark *m_start;
	GtkTextMark *m_end;
	gboolean init;
	MarkupTag markup;
} TextTag;


static MarkupTag get_markup_token (GtkTextIter *iter, gint *out_nb_spaces_before, GtkTextIter *out_end,
				   TextTag *start_tag, GdauiRtEditor *rte);
static MarkupTag get_token (GtkTextIter *iter, gint *out_nb_spaces_before, GtkTextIter *out_end,
			    TextTag *start_tag, GdauiRtEditor *rte);
static gchar get_char_at_iter (GtkTextIter *iter, gboolean move_forward_first);
static gboolean markup_tag_match (MarkupTag tag1, gint tagline1, MarkupTag tag2, gint tagline2);

static void
apply_markup (GdauiRtEditor *rte, GtkTextBuffer *textbuffer, TextTag *current, GtkTextMark *mark_start, GtkTextMark *mark_end)
{
	gint ssol;
	GtkTextIter start, end;

	rte->priv->insert_offset = -1;

	gtk_text_buffer_get_iter_at_mark (textbuffer, &start, mark_start);
	gtk_text_buffer_get_iter_at_mark (textbuffer, &end, mark_end);

	/* apply markup */
	GtkTextIter astart;
	gtk_text_buffer_get_iter_at_mark (textbuffer, &astart, current->m_start);
	switch (current->markup) {
	case MARKUP_BOLD:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_BOLD].tag,
					   &astart, &end);
		break;
	case MARKUP_VERBATIM:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_VERBATIM].tag,
					   &astart, &end);
		break;
	case MARKUP_TT:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_TT].tag,
					   &astart, &end);
		break;
	case MARKUP_ITALIC:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_ITALIC].tag,
					   &astart, &end);
		break;
	case MARKUP_STRIKE:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_STRIKE].tag,
					   &astart, &end);
		break;
	case MARKUP_UNDERLINE:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_UNDERLINE].tag,
					   &astart, &end);
		break;
	case MARKUP_TITLE1_S:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_TITLE1].tag,
					   &astart, &end);
		break;
	case MARKUP_TITLE2_S:
		gtk_text_buffer_apply_tag (textbuffer,
					   rte->priv->tags[TEXT_TAG_TITLE2].tag,
					   &astart, &end);
		break;
	case MARKUP_LIST_S: {
		GtkTextIter ps, pe;
		gtk_text_buffer_get_iter_at_mark (textbuffer, &ps, current->m_start);
		ssol = spaces_since_start_of_line (&ps);
		if (ssol > 0) {
			GtkTextIter diter;
			diter = ps;
			if (gtk_text_iter_backward_chars (&diter, ssol))
				gtk_text_buffer_delete (textbuffer, &diter, &ps);
		}

		GtkTextTag *tag;
		gint bindex = 0;
		gtk_text_buffer_get_iter_at_mark (textbuffer, &ps, current->m_end);
		if (ssol > 0) {
			bindex = 1;
			tag = rte->priv->tags[TEXT_TAG_LIST2].tag;
		}
		else
			tag = rte->priv->tags[TEXT_TAG_LIST1].tag;

		if (! bullet_pix[0]) {
			bullet_pix[0] = gdk_pixbuf_new_from_inline (-1, bullet_pixdata,
								    FALSE, NULL);
			bullet_pix[1] = gdk_pixbuf_new_from_inline (-1, bulleth_pixdata,
								    FALSE, NULL);
		}
		gtk_text_buffer_insert_pixbuf (textbuffer, &ps, bullet_pix[bindex]);
		gtk_text_buffer_get_iter_at_mark (textbuffer, &ps, current->m_end);
		pe = ps;
		gtk_text_iter_forward_char (&pe);
		gtk_text_buffer_apply_tag (rte->priv->textbuffer, tag, &ps, &pe);

		/* remove all other tags */
		gint i;
		gtk_text_iter_set_line_index (&ps, 0);
		gtk_text_iter_backward_char (&ps); /* to catch the previous line's '\n' */
		for (i = 0; i < TEXT_TAG_LAST; i++) {
			if (rte->priv->tags[i].tag == tag)
				continue;
			else
				gtk_text_buffer_remove_tag (rte->priv->textbuffer,
							    rte->priv->tags[i].tag, &ps, &pe);
		}

		break;
	}
	case MARKUP_PICTURE_S: {
		gchar *data;
		GtkTextIter ps, pe;
		gsize length;
		GdkPixdata pixdata;
		gtk_text_buffer_get_iter_at_mark (textbuffer, &ps, current->m_end);
		pe = start;
		data = remove_newlines_from_base64 (gtk_text_buffer_get_text (textbuffer, &ps, &pe, FALSE));
		/*g_print ("{{{%s}}}\n", data);*/
		g_base64_decode_inplace (data, &length);
		if (gdk_pixdata_deserialize (&pixdata, length, (guint8*) data, NULL)) {
			GdkPixbuf *pixbuf;
			pixbuf = gdk_pixbuf_from_pixdata (&pixdata, TRUE, NULL);
			if (pixbuf) {
				gtk_text_buffer_delete (textbuffer, &ps, &pe);
				gtk_text_buffer_get_iter_at_mark (textbuffer, &ps, mark_end);
				gtk_text_buffer_insert_pixbuf (textbuffer, &ps, pixbuf);
				g_object_unref (pixbuf);
			}
		}
		g_free (data);
		break;
	}
	default:
		g_warning ("Unhandled marker (type %d)", current->markup);
		break;
	}
	/* remove markup text */
	gtk_text_buffer_get_iter_at_mark (textbuffer, &start, mark_start);
	gtk_text_buffer_get_iter_at_mark (textbuffer, &end, mark_end);
	if (! gtk_text_iter_equal (&start, &end))
		gtk_text_buffer_delete (textbuffer, &start, &end);
	gtk_text_buffer_get_iter_at_mark (textbuffer, &start, current->m_start);
	gtk_text_buffer_get_iter_at_mark (textbuffer, &end, current->m_end);
	if (! gtk_text_iter_equal (&start, &end))
		gtk_text_buffer_delete (textbuffer, &start, &end);
}

static void
text_buffer_changed_cb (GtkTextBuffer *textbuffer, GdauiRtEditor *rte)
{
	GSList *queue = NULL;
	MarkupTag mt;
	TextTag *current = NULL;
	GtkTextIter start, end;
	gint ssol;

	if (rte->priv->show_markup) {
		if (rte->priv->enable_changed_signal)
			g_signal_emit (rte, gdaui_rt_editor_signals[CHANGED], 0, NULL);
		return;
	}

	gtk_text_buffer_get_start_iter (textbuffer, &start);

	g_signal_handlers_block_by_func (textbuffer,
					 G_CALLBACK (text_buffer_changed_cb), rte);
	
	for (mt = get_token (&start, &ssol, &end, current, rte);
	     mt != MARKUP_EOF;
	     mt = get_token (&start, &ssol, &end, current, rte)) {
		/*
		gchar *text= gtk_text_iter_get_text (&start, &end);
		g_print ("Token %d [%s] with SSOL %d\n", mt, text, ssol);
		g_free (text);
		*/
		if (mt == MARKUP_NONE) {
			start = end;
			continue;
		}
		if (! current) {
			current = g_new (TextTag, 1);
			current->markup = mt;
			current->m_start = gtk_text_buffer_create_mark (textbuffer, NULL, &start, TRUE);
			current->m_end = gtk_text_buffer_create_mark (textbuffer, NULL, &end, TRUE);

			queue = g_slist_prepend (queue, current);
		}
		else {
			GtkTextIter liter;
			gtk_text_buffer_get_iter_at_mark (textbuffer, &liter, current->m_start);
			if (markup_tag_match (current->markup, gtk_text_iter_get_line (&liter),
					      mt, gtk_text_iter_get_line (&start))) {
				/* save iters as marks */
				GtkTextMark *mark_start, *mark_end;
				mark_start = gtk_text_buffer_create_mark (textbuffer, NULL, &start, TRUE);
				mark_end = gtk_text_buffer_create_mark (textbuffer, NULL, &end, TRUE);

				/* apply markup */
				apply_markup (rte, textbuffer, current, mark_start, mark_end);

				/* get rid of @current */
				gtk_text_buffer_delete_mark (textbuffer, current->m_start);
				gtk_text_buffer_delete_mark (textbuffer, current->m_end);
				g_free (current);
				queue = g_slist_remove (queue, current);
				current = NULL;

				if (queue) {
					current = (TextTag*) queue->data;
					gtk_text_buffer_get_iter_at_mark (textbuffer, &end, current->m_end);
				}
				else {
					/* restore iter from marks */
					gtk_text_buffer_get_iter_at_mark (textbuffer, &end, mark_end);
				}

				/* delete marks */
				gtk_text_buffer_delete_mark (textbuffer, mark_end);
				gtk_text_buffer_delete_mark (textbuffer, mark_start);
			}
			else {
				current = g_new (TextTag, 1);
				current->markup = mt;
				current->m_start = gtk_text_buffer_create_mark (textbuffer, NULL, &start,
										TRUE);
				current->m_end = gtk_text_buffer_create_mark (textbuffer, NULL, &end, TRUE);

				queue = g_slist_prepend (queue, current);
			}
		}

		start = end;
	}

	while (queue) {
		current = (TextTag*) queue->data;
		gtk_text_buffer_delete_mark (textbuffer, current->m_start);
		gtk_text_buffer_delete_mark (textbuffer, current->m_end);
		g_free (current);
		queue = g_slist_delete_link (queue, queue);
	}

	g_signal_handlers_unblock_by_func (textbuffer,
					   G_CALLBACK (text_buffer_changed_cb), rte);

	if (rte->priv->enable_changed_signal)
		g_signal_emit (rte, gdaui_rt_editor_signals[CHANGED], 0, NULL);
}

/**
 * get_token
 *
 * returns the token type starting from @iter, and positions @out_end to the last used position
 * position.
 */
static MarkupTag
get_token (GtkTextIter *iter, gint *out_nb_spaces_before, GtkTextIter *out_end,
	   TextTag *start_tag, GdauiRtEditor *rte)
{
	MarkupTag retval;
	GtkTextIter inti;
	inti = *iter;

	retval = get_markup_token (&inti, out_nb_spaces_before, out_end, start_tag, rte);
	if ((retval != MARKUP_NONE) || (retval == MARKUP_EOF))
		return retval;

	for (; gtk_text_iter_forward_char (&inti);) {
		retval = get_markup_token (&inti, NULL, NULL, start_tag, rte);
		if ((retval != MARKUP_NONE) || (retval == MARKUP_EOF))
			break;
	}
	*out_end = inti;
	return MARKUP_NONE;
}

/*
 * spaces_since_start_of_line
 * @iter: an iterator, __not modified__
 *
 * Computes the number of spaces since start of line in case there is no other
 * character on the line except for spaces
 *
 * Returns: number of spaces, or -1 if there is not only some spaces on the line before @iter
 */
static gint
spaces_since_start_of_line (GtkTextIter *iter)
{
	gint i = 0;
	GtkTextIter inti = *iter;
	gunichar u;
	for (; !gtk_text_iter_starts_line (&inti) && gtk_text_iter_backward_char (&inti); i++) {
		u = gtk_text_iter_get_char (&inti);
		if (! g_unichar_isspace (u))
			return -1;
	}
	
#ifdef GDA_DEBUG_NO
	gchar *data;
	data = gtk_text_buffer_get_slice (gtk_text_iter_get_buffer (iter), &inti, iter, TRUE);
	g_print ("FOUND %d spaces for [%s]\n", i, data);
	g_free (data);
#endif
	return i;
}

/**
 * get_markup_token
 * @iter: starting position
 * @out_nb_spaces_before: a place to set the value returned by spaces_since_start_of_line() if called
 * @out_end: place to put the last used position, or %NULL
 * @rte: the #GdauiRtEditor
 *
 * Parses marking tokens, nothing else
 *
 * Returns: a markup token, or MARKUP_NONE or MARKUP_EOF otherwise
 */
static MarkupTag
get_markup_token (GtkTextIter *iter, gint *out_nb_spaces_before, GtkTextIter *out_end,
		  TextTag *start_tag, GdauiRtEditor *rte)
{
	GtkTextIter inti;
	gchar c;
	gint ssol = -1; /* spaces since start of line */
	MarkupTag start_markup = MARKUP_EOF;
	gint linestart = 0;

#define SET_OUT \
	if (out_end) {							\
		gtk_text_iter_forward_char (&inti);			\
		*out_end = inti;					\
	}								\
	if (out_nb_spaces_before)					\
		*out_nb_spaces_before = ssol

	if (start_tag) {
		start_markup = start_tag->markup;
		gtk_text_buffer_get_iter_at_mark (gtk_text_iter_get_buffer (iter), &inti, start_tag->m_start);
		linestart = gtk_text_iter_get_line (&inti);
	}

	inti = *iter;
	if (out_end)
		*out_end = inti;

	c = get_char_at_iter (&inti, FALSE);

	/* tests involving starting markup before anything else */
	if (start_markup == MARKUP_PICTURE_S) {
		if (c == ']') {
			c = get_char_at_iter (&inti, TRUE);
			if (c == ']') {
				c = get_char_at_iter (&inti, TRUE);
				if (c == ']') {
					SET_OUT;
					return MARKUP_PICTURE_E;
				}
			}
		}
		if (!c)
			return MARKUP_EOF;
		else
			return MARKUP_NONE;
	}
	else if (start_markup == MARKUP_VERBATIM) {
		if (c == '"') {
			c = get_char_at_iter (&inti, TRUE);
			if (c == '"') {
				c = get_char_at_iter (&inti, TRUE);
				if (c == '"') {
					SET_OUT;
					return MARKUP_VERBATIM;
				}
			}
		}
		if (!c)
			return MARKUP_EOF;
		else
			return MARKUP_NONE;
	}
	else if (gtk_text_iter_has_tag (&inti, rte->priv->tags[TEXT_TAG_VERBATIM].tag)) {
		if (!c)
			return MARKUP_EOF;
		else
			return MARKUP_NONE;		
	}

	if (gtk_text_iter_ends_line (&inti) && (start_markup == MARKUP_LIST_S) &&
	    (linestart == gtk_text_iter_get_line (&inti)))
		return MARKUP_LIST_E;

	if (!c)
		return MARKUP_EOF;

	/* other tests */
	ssol = spaces_since_start_of_line (&inti);
	if (ssol >= 0) {
		/* we are on a line with only spaces since its start */
		if (c == '=') {
			c = get_char_at_iter (&inti, TRUE);
			if (c == ' ') {
				SET_OUT;
				return MARKUP_TITLE1_S;
			}
			else if (c == '=') {
				c = get_char_at_iter (&inti, TRUE);
				if (c == ' ') {
					SET_OUT;
					return MARKUP_TITLE2_S;
				}
			}
		}
		else if (c == '-') {
			c = get_char_at_iter (&inti, TRUE);
			if (c == ' ') {
				SET_OUT;
				return MARKUP_LIST_S;
			}
		}
	}

	if (c == '*') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '*') {
			SET_OUT;
			return MARKUP_BOLD;
		}
	}
	else if (c == '/') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '/') {
			SET_OUT;
			return MARKUP_ITALIC;
		}
	}
	else if (c == '_') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '_') {
			SET_OUT;
			return MARKUP_UNDERLINE;
		}
	}
	else if (c == '-') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '-') {
			SET_OUT;
			return MARKUP_STRIKE;
		}
	}
	else if (c == '`') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '`') {
			SET_OUT;
			return MARKUP_TT;
		}
	}
	else if (c == '"') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '"') {
			c = get_char_at_iter (&inti, TRUE);
			if (c == '"') {
				SET_OUT;
				return MARKUP_VERBATIM;
			}
		}
	}
	else if (c == ' ') {
		gboolean line;
		line = gtk_text_iter_starts_line (&inti);

		c = get_char_at_iter (&inti, TRUE);
		if (c == '=') {
			if (start_markup == MARKUP_TITLE1_S) {
				GtkTextIter it = inti;
				gtk_text_iter_forward_char (&it);
				if (gtk_text_iter_ends_line (&it) &&
				    (linestart == gtk_text_iter_get_line (&inti))) {
					SET_OUT;
					return MARKUP_TITLE1_E;
				}
			}
			else {
				c = get_char_at_iter (&inti, TRUE);
				if (c == '=') {
					GtkTextIter it = inti;
					gtk_text_iter_forward_char (&it);
					if ((start_markup == MARKUP_TITLE2_S) && gtk_text_iter_ends_line (&it) &&
					    (linestart == gtk_text_iter_get_line (&inti))) {
						SET_OUT;
						return MARKUP_TITLE2_E;
					}
				}
			}
		}
	}
	else if (c == '[') {
		c = get_char_at_iter (&inti, TRUE);
		if (c == '[') {
			c = get_char_at_iter (&inti, TRUE);
			if (c == '[') {
				SET_OUT;
				return MARKUP_PICTURE_S;
			}
		}
	}
	return MARKUP_NONE;
}

/**
 * get_char_at_iter
 * @iter: an iter
 * @move_forward_first: %TRUE if @iter should be moved forward first
 *
 * Returns: 0 for EOF, 1 for the "unknown" unicode char (usually pixbufs) or for chars represented
 * by more than one byte
 */
static gchar
get_char_at_iter (GtkTextIter *iter, gboolean move_forward_first)
{
	if (!move_forward_first || 
	    (move_forward_first	&& gtk_text_iter_forward_char (iter))) {
		gunichar uc;
		gchar buf1[6];
		uc = gtk_text_iter_get_char (iter);
		if (!uc)
			return 0;
		if (g_unichar_to_utf8 (uc, buf1) == 1)
			return *buf1;
		return 1;
	}
	return 0;
}

static gboolean
markup_tag_match (MarkupTag tag1, gint tagline1, MarkupTag tag2, gint tagline2)
{
	gboolean retval;
	switch (tag1) {
	case MARKUP_BOLD:
	case MARKUP_TT:
	case MARKUP_VERBATIM:
	case MARKUP_ITALIC:
	case MARKUP_STRIKE:
	case MARKUP_UNDERLINE:
		retval = (tag1 == tag2) ? TRUE : FALSE;
		break;
	case MARKUP_TITLE1_S:
		retval = (tag2 == MARKUP_TITLE1_E) ? TRUE : FALSE;
		break;
	case MARKUP_TITLE2_S:
		retval = (tag2 == MARKUP_TITLE2_E) ? TRUE : FALSE;
		break;
	case MARKUP_PICTURE_S:
		retval = (tag2 == MARKUP_PICTURE_E) ? TRUE : FALSE;
		break;
	case MARKUP_LIST_S:
		retval = (tag2 == MARKUP_LIST_E) ? TRUE : FALSE;
		break;
	default:
		retval = FALSE;
		break;
	}

	if (retval) {
		if ((tag1 != MARKUP_PICTURE_S) && (tag1 != MARKUP_VERBATIM))
			retval = (tagline1 == tagline2) ? TRUE : FALSE;
	}
	return retval;
}

static guint8 *serialize_as_txt2tag (GtkTextBuffer     *register_buffer,
				     GtkTextBuffer     *content_buffer,
				     const GtkTextIter *start,
				     const GtkTextIter *end,
				     gsize             *length,
				     GdauiRtEditor     *editor);

/**
 * gdaui_rt_editor_get_contents
 * @editor: a #GdauiRtEditor
 *
 * Get the contents of @editor, using the markup syntax
 *
 * Returns: (transfer full): a new string, or %NULL if there was an error
 *
 * Since: 4.2.2
 */
gchar *
gdaui_rt_editor_get_contents (GdauiRtEditor *editor)
{
	g_return_val_if_fail (GDAUI_IS_RT_EDITOR (editor), NULL);

	if (editor->priv->saved_for_help)
		return g_strdup (editor->priv->saved_for_help);
	else
		return real_gdaui_rt_editor_get_contents (editor);
}

static gchar *
real_gdaui_rt_editor_get_contents (GdauiRtEditor *editor)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds (editor->priv->textbuffer, &start, &end);

	if (editor->priv->show_markup)
		return gtk_text_buffer_get_text (editor->priv->textbuffer, &start, &end, FALSE);
	else {
		GdkAtom format;
		guint8 *data;
		gsize length;
		format = gtk_text_buffer_register_serialize_format (editor->priv->textbuffer, "txt/rte",
								    (GtkTextBufferSerializeFunc) serialize_as_txt2tag,
								    editor, NULL);
		data = gtk_text_buffer_serialize (editor->priv->textbuffer, editor->priv->textbuffer, format,
						  &start, &end, &length);
		return (gchar*) data;
	}
}

/*
 * Serialization as txt2tag
 */
typedef struct
{
	GString *text_str;
	GHashTable *tags;
	GtkTextIter start, end;

	gint tag_id;
	GHashTable *tag_id_tags;
} SerializationContext;

/*
 * serialize_tag:
 * @tag: a #GtkTextTag
 * @starting: %TRUE if serialization has to be done for the opening part
 *
 * Returns: a static string, never %NULL
 */
static const gchar *
serialize_tag (GtkTextTag *tag, gboolean starting, GdauiRtEditor *editor)
{
	if (tag == editor->priv->tags[TEXT_TAG_ITALIC].tag)
		return "//";
	else if (tag == editor->priv->tags[TEXT_TAG_BOLD].tag)
		return "**";
	else if (tag == editor->priv->tags[TEXT_TAG_UNDERLINE].tag)
		return "__";
	else if (tag == editor->priv->tags[TEXT_TAG_STRIKE].tag)
		return "--";
	else if (tag == editor->priv->tags[TEXT_TAG_TT].tag)
		return "``";
	else if (tag == editor->priv->tags[TEXT_TAG_VERBATIM].tag)
		return "\"\"\"";
	else if (tag == editor->priv->tags[TEXT_TAG_TITLE1].tag) {
		if (starting)
			return "= ";
		else
			return " =";
	}
	else if (tag == editor->priv->tags[TEXT_TAG_TITLE2].tag) {
		if (starting)
			return "== ";
		else
			return " ==";
	}
	else if (tag == editor->priv->tags[TEXT_TAG_LIST1].tag) {
		if (starting)
			return lists_tokens [0];
		else
			return "";
	}
	else if (tag == editor->priv->tags[TEXT_TAG_LIST2].tag) {
		if (starting)
			return lists_tokens [1];
		else
			return "";
	}
	else {
		gchar *tagname;    
		g_object_get ((GObject*) tag, "name", &tagname, NULL);
		g_warning ("Unknown tag '%s'\n", tagname);
		g_free (tagname);
		return "";
	}
}

/**
 * steals @base64
 */
static gchar *
add_newlines_to_base64 (gchar *base64)
{
	GString *string;
	gint i;
	gchar *ptr;
	string = g_string_new ("");
	for (i = 0, ptr = base64; *ptr; i++, ptr++) {
		if (i && ! (i % 100))
			g_string_append_c (string, '\n');
		g_string_append_c (string, *ptr);
	}
	g_free (base64);
	return g_string_free (string, FALSE);
}

/**
 * steals @base64
 */
static gchar *
remove_newlines_from_base64 (gchar *base64)
{
	GString *string;
	gchar *ptr;
	string = g_string_new ("");
	for (ptr = base64; *ptr; ptr++) {
		if (*ptr != '\n')
			g_string_append_c (string, *ptr);
	}
	g_free (base64);
	return g_string_free (string, FALSE);
}

static void
serialize_text (G_GNUC_UNUSED GtkTextBuffer *buffer, SerializationContext *context,
		GdauiRtEditor *editor)
{
	GtkTextIter iter, old_iter;
	GList *opened_tags = NULL; /* 1st element of the list is the last opened tag (ie. the one
				    * which should be closed 1st */

	/*g_string_append (context->text_str, "###");*/
	iter = context->start;
	do {
		GSList *new_tag_list, *list;
		GList *dlist;
		gboolean tags_needs_reopened = TRUE;
		new_tag_list = gtk_text_iter_get_tags (&iter);

		/*
		 * Close tags which need closing
		 */
		/* Find the last element in @opened_tags which is not opened anymore */
		for (dlist = g_list_last (opened_tags); dlist; dlist = dlist->prev) {
			if (! g_slist_find (new_tag_list, dlist->data))
				break;
		}

		if (dlist) {
			/* close all the tags up to dlist->data and at the same time remove them
			 from @opened_tags */
			for (; opened_tags != dlist;
			     opened_tags = g_list_delete_link (opened_tags, opened_tags)) {
				g_string_append (context->text_str,
						 serialize_tag ((GtkTextTag*) opened_tags->data,
								FALSE, editor));
			}
			g_string_append (context->text_str,
					 serialize_tag ((GtkTextTag*) opened_tags->data,
							FALSE, editor));
			opened_tags = g_list_delete_link (opened_tags, opened_tags);
		}

		/* Now try to go to either the next tag toggle, or if a pixbuf appears */
		old_iter = iter;
		while (1) {
			gunichar ch = gtk_text_iter_get_char (&iter);

			if (ch == 0xFFFC) {
				GdkPixbuf *pixbuf = gtk_text_iter_get_pixbuf (&iter);

				if (pixbuf) {
					/* Append the text before the pixbuf */
					gchar *tmp_text;
					tmp_text = gtk_text_iter_get_slice (&old_iter, &iter);
					g_string_append (context->text_str, tmp_text);
					g_free (tmp_text);

					if ((pixbuf != bullet_pix[0]) &&
					    (pixbuf != bullet_pix[1])) {
						GdkPixdata pixdata;
						guint8 *tmp;
						guint len;
						gchar *data;
						
						gdk_pixdata_from_pixbuf (&pixdata, pixbuf, FALSE);
						tmp = gdk_pixdata_serialize (&pixdata, &len);
						data = add_newlines_to_base64 (g_base64_encode (tmp, len));
						g_free (tmp);
						g_string_append (context->text_str, "[[[");
						g_string_append (context->text_str, data);
						g_free (data);
						g_string_append (context->text_str, "]]]");
					}

					/* Forward so we don't get the 0xfffc char */
					gtk_text_iter_forward_char (&iter);
					old_iter = iter;
				}
			}
			else if (ch == 0) {
				break;
			}
			else
				gtk_text_iter_forward_char (&iter);

			if (gtk_text_iter_toggles_tag (&iter, NULL)) {
				/*g_print ("Toggle @pos %d:%d\n", gtk_text_iter_get_line (&iter),
				  gtk_text_iter_get_line_offset (&iter));*/
				break;
			}
		}

		/* We might have moved too far */
		if (gtk_text_iter_compare (&iter, &context->end) > 0)
			iter = context->end;

		/* Append the text, except if there is the TEXT_TAG_BULLET tag */
		GtkTextTag *ltag;
		ltag = iter_begins_list (editor, &old_iter, NULL);
		if (! ltag) {
			gint i;
			gchar *tmp_text;
			tmp_text = gtk_text_iter_get_slice (&old_iter, &iter);
#ifdef NONO
			g_string_append (context->text_str, tmp_text);
#endif
			for (i = 0; tmp_text[i]; i++) {
				if (tmp_text[i] != '\n') {
					if (tags_needs_reopened) {
						/*
						 * (re)open tags which are still there
						 */
						for (list = new_tag_list; list; list = list->next) {
							if (! g_list_find (opened_tags, list->data)) {
								opened_tags = g_list_prepend (opened_tags, list->data);
								g_string_append (context->text_str,
										 serialize_tag ((GtkTextTag*) opened_tags->data,
												TRUE, editor));
							}
						}
						tags_needs_reopened = FALSE;
					}

					g_string_append_c (context->text_str, tmp_text[i]);
					continue;
				}
				/* close all tags and re-open them after the newline */
				for (dlist = opened_tags; dlist; dlist = dlist->next) {
					g_string_append (context->text_str,
							 serialize_tag (GTK_TEXT_TAG (dlist->data),
									FALSE, editor));
				}
				g_string_append_c (context->text_str, '\n');
				/* re-open tags */
				for (dlist = g_list_last (opened_tags); dlist; dlist = dlist->prev) {
					g_string_append (context->text_str,
							 serialize_tag (GTK_TEXT_TAG (dlist->data),
									TRUE, editor));
				}
			}
			g_free (tmp_text);
		}

		if (tags_needs_reopened) {
			/*
			 * (re)open tags which are still there
			 */
			for (list = new_tag_list; list; list = list->next) {
				if (! g_list_find (opened_tags, list->data)) {
					opened_tags = g_list_prepend (opened_tags, list->data);
					g_string_append (context->text_str,
							 serialize_tag ((GtkTextTag*) opened_tags->data,
									TRUE, editor));
				}
			}
			tags_needs_reopened = FALSE;
		}


		g_slist_free (new_tag_list);
	}
	while (!gtk_text_iter_equal (&iter, &context->end));

	/* Close any open tags */
	for (; opened_tags;
	     opened_tags = g_list_delete_link (opened_tags, opened_tags)) {
		g_string_append (context->text_str,
				 serialize_tag ((GtkTextTag*) opened_tags->data,
						FALSE, editor));
	}

	/*g_string_append (context->text_str, "###");*/
}

/*
 * serialize_as_txt2tag:
 */
static guint8 *
serialize_as_txt2tag (G_GNUC_UNUSED  GtkTextBuffer *register_buffer,
		      GtkTextBuffer     *content_buffer,
		      const GtkTextIter *start,
		      const GtkTextIter *end,
		      gsize             *length,
		      GdauiRtEditor     *editor)
{
	SerializationContext context;
	GString *text;

	context.tags = g_hash_table_new (NULL, NULL);
	context.text_str = g_string_new (NULL);
	context.start = *start;
	context.end = *end;
	context.tag_id = 0;
	context.tag_id_tags = g_hash_table_new (NULL, NULL);

	serialize_text (content_buffer, &context, editor);

	text = g_string_new (NULL);

	g_string_append_len (text, context.text_str->str, context.text_str->len);

	g_hash_table_destroy (context.tags);
	g_string_free (context.text_str, TRUE);
	g_hash_table_destroy (context.tag_id_tags);

	*length = text->len;

	return (guint8 *) g_string_free (text, FALSE);
}


/**
 * gdaui_rt_editor_set_contents
 * @editor: a #GdauiRtEditor
 * @markup: the text to set in @editor, using the markup syntax (must be valid UTF-8)
 * @length: length of text in bytes.
 *
 * Set @editor's contents. If @length is -1, @markup must be nul-terminated
 *
 * Since: 4.2.2
 */
void
gdaui_rt_editor_set_contents (GdauiRtEditor *editor, const gchar *markup, gint length)
{
	g_return_if_fail (GDAUI_IS_RT_EDITOR (editor));

	editor->priv->contents_setting = TRUE;
	gtk_text_buffer_set_text (editor->priv->textbuffer, markup, length);
	editor->priv->contents_setting = FALSE;
}

/**
 * gdaui_rt_editor_set_editable
 * @editor: a #GdauiRtEditor
 * @editable: whether it's editable
 *
 * Set @editor's editability
 *
 * Since: 4.2.2
 */
void
gdaui_rt_editor_set_editable (GdauiRtEditor *editor, gboolean editable)
{
	g_return_if_fail (GDAUI_IS_RT_EDITOR (editor));
	gtk_text_view_set_editable (editor->priv->textview, editable);
	show_hide_toolbar (editor);
}

/*
 * _gdaui_rt_editor_set_show_markup
 * @editor: a #GdauiRtEditor
 * @show_markup: whether @editor shows markup of applies it
 *
 * If @show_markup is %FALSE, then @editor displays text with tags applied (bold text, ...); and if it's
 * %TRUE then it shows markup text instead
 *
 * Since: 4.2.2
 */
static void
_gdaui_rt_editor_set_show_markup (GdauiRtEditor *editor, gboolean show_markup)
{
	gchar *data;
	gint cursor_pos;
	GtkTextIter iter;

	g_return_if_fail (GDAUI_IS_RT_EDITOR (editor));
	if (editor->priv->show_markup == show_markup)
		return;

	g_object_get (editor->priv->textbuffer, "cursor-position", &cursor_pos, NULL);
	data = real_gdaui_rt_editor_get_contents (editor);
	editor->priv->show_markup = show_markup;
	gdaui_rt_editor_set_contents (editor, data, -1);
	g_free (data);

	gtk_text_buffer_get_iter_at_offset (editor->priv->textbuffer, &iter, cursor_pos);
	gtk_text_buffer_place_cursor (editor->priv->textbuffer, &iter);

	show_hide_toolbar (editor);
}

static void
show_hide_toolbar (GdauiRtEditor *editor)
{
	gboolean enable_markup = TRUE;
	GtkAction *action;
	
	if (gtk_text_view_get_editable (editor->priv->textview))
		gtk_widget_show (editor->priv->toolbar);
	else
		gtk_widget_hide (editor->priv->toolbar);

	if (editor->priv->show_markup ||
	    ! gtk_text_view_get_editable (editor->priv->textview))
		enable_markup = FALSE;

	action = gtk_ui_manager_get_action (editor->priv->uimanager, "/ToolBar/ActionBold");
	gtk_action_set_sensitive (action, enable_markup);
	action = gtk_ui_manager_get_action (editor->priv->uimanager, "/ToolBar/ActionItalic");
	gtk_action_set_sensitive (action, enable_markup);
	action = gtk_ui_manager_get_action (editor->priv->uimanager, "/ToolBar/ActionUnderline");
	gtk_action_set_sensitive (action, enable_markup);
	action = gtk_ui_manager_get_action (editor->priv->uimanager, "/ToolBar/ActionStrike");
	gtk_action_set_sensitive (action, enable_markup);
	action = gtk_ui_manager_get_action (editor->priv->uimanager, "/ToolBar/ActionAddImage");
	gtk_action_set_sensitive (action, enable_markup);
	action = gtk_ui_manager_get_action (editor->priv->uimanager, "/ToolBar/ActionReset");
	gtk_action_set_sensitive (action, enable_markup);
}
