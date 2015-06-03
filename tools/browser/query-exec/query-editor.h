/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __QUERY_EDITOR_H__
#define __QUERY_EDITOR_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define QUERY_TYPE_EDITOR            (query_editor_get_type())
#define QUERY_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, QUERY_TYPE_EDITOR, QueryEditor))
#define QUERY_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, QUERY_TYPE_EDITOR, QueryEditorClass))
#define QUERY_IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, QUERY_TYPE_EDITOR))
#define QUERY_IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_TYPE_EDITOR))

#define QUERY_EDITOR_TOOLTIP _("Enter SQL code to execute\n(must be understood by the database to\n" \
			       "which the connection is opened, except for the variables definition)\n" \
			       "The following shortcuts are allowed:\n" \
			       "   <small><b>CTRL - l</b></small> to clear the editor\n" \
			       "   <small><b>CTRL - ENTER</b></small> to execute SQL\n" \
			       "   <small><b>CTRL - Up</b></small> to move to previous executed SQL in history\n" \
			       "   <small><b>CTRL - Down</b></small> to move to next executed SQL in history\n" \
			       "   <small><b>CTRL - SPACE</b></small> to obtain a completion list")

typedef struct _QueryEditor        QueryEditor;
typedef struct _QueryEditorClass   QueryEditorClass;
typedef struct _QueryEditorPrivate QueryEditorPrivate;

/*
 * Query history item
 */
typedef struct {
	gchar *sql;
	GObject *result;
	GError *exec_error;
	gboolean within_transaction;

	gint ref_count;
} QueryEditorHistoryItem;

QueryEditorHistoryItem *query_editor_history_item_new (const gchar *sql, GObject *result, GError *error);
QueryEditorHistoryItem *query_editor_history_item_ref (QueryEditorHistoryItem *qih);
void                    query_editor_history_item_unref (QueryEditorHistoryItem *qih);

/*
 * Query history batch
 */
typedef struct {
	GTimeVal run_time;
	GdaSet *params;
	GSList *hist_items; /* list of QueryEditorHistoryItem, ref held here */

	gint ref_count;
} QueryEditorHistoryBatch;

QueryEditorHistoryBatch *query_editor_history_batch_new (GTimeVal run_time, GdaSet *params);
QueryEditorHistoryBatch *query_editor_history_batch_ref (QueryEditorHistoryBatch *qib);
void                     query_editor_history_batch_unref (QueryEditorHistoryBatch *qib);


struct _QueryEditor {
	GtkBox parent;
	QueryEditorPrivate *priv;
};

struct _QueryEditorClass {
	GtkBoxClass parent_class;

	/* signals */
	void (* changed) (QueryEditor *editor);
	void (* history_item_removed) (QueryEditor *editor, QueryEditorHistoryItem *item);
	void (* history_cleared) (QueryEditor *editor);
	void (* execute_request) (QueryEditor *editor);
};

/*
 * Editor modes
 */
typedef enum {
	QUERY_EDITOR_READWRITE,
	QUERY_EDITOR_READONLY,
	QUERY_EDITOR_HISTORY
} QueryEditorMode;


GType      query_editor_get_type (void) G_GNUC_CONST;
GtkWidget *query_editor_new (void);

/* common API */
void       query_editor_set_mode (QueryEditor *editor, QueryEditorMode mode);
QueryEditorMode query_editor_get_mode (QueryEditor *editor);

gchar     *query_editor_get_all_text (QueryEditor *editor);
gboolean   query_editor_load_from_file (QueryEditor *editor, const gchar *filename);
gboolean   query_editor_save_to_file (QueryEditor *editor, const gchar *filename);

void       query_editor_copy_clipboard (QueryEditor *editor);
void       query_editor_cut_clipboard (QueryEditor *editor);
void       query_editor_paste_clipboard (QueryEditor *editor);

/* normal editor's API */
void       query_editor_set_text (QueryEditor *editor, const gchar *text);
void       query_editor_append_text (QueryEditor *editor, const gchar *text);
void       query_editor_keep_current_state (QueryEditor *editor);
void       query_editor_append_note (QueryEditor *editor, const gchar *text, gint level);
void       query_editor_show_tooltip (QueryEditor *editor, gboolean show_tooltip);

/* history API */
void       query_editor_start_history_batch (QueryEditor *editor, QueryEditorHistoryBatch *hist_batch);
void       query_editor_add_history_item (QueryEditor *editor, QueryEditorHistoryItem *hist_item);
QueryEditorHistoryItem *query_editor_get_current_history_item (QueryEditor *editor,
							       QueryEditorHistoryBatch **out_in_batch);
QueryEditorHistoryBatch *query_editor_get_current_history_batch (QueryEditor *editor);
gboolean   query_editor_history_is_empty (QueryEditor *editor);

void       query_editor_del_all_history_items (QueryEditor *editor);
void       query_editor_del_current_history_item (QueryEditor *editor);
void       query_editor_del_history_batch (QueryEditor *editor, QueryEditorHistoryBatch *batch);

G_END_DECLS

#endif
