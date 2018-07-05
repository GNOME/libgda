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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "query-result.h"
#include "../browser-window.h"
#include <libgda-ui/libgda-ui.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include "../ui-formgrid.h"
#include "query-marshal.h"

struct _QueryResultPrivate {
	QueryEditor *history;

	GHashTable *hash; /* key = a QueryEditorHistoryItem, value = a #GtkWidget, refed here all the times */
	GtkWidget *child;
};

static void query_result_class_init (QueryResultClass *klass);
static void query_result_init       (QueryResult *result, QueryResultClass *klass);
static void query_result_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/* signals */
enum {
        RERUN_REQUESTED,
        LAST_SIGNAL
};

static gint query_result_signals [LAST_SIGNAL] = { 0 };

static GtkWidget *make_widget_for_notice (void);
static GtkWidget *make_widget_for_data_model (GdaDataModel *model, QueryResult *qres, const gchar *sql);
static GtkWidget *make_widget_for_set (GdaSet *set);
static GtkWidget *make_widget_for_error (GError *error);


/*
 * QueryResult class implementation
 */
static void
query_result_class_init (QueryResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	query_result_signals [RERUN_REQUESTED] =
		g_signal_new ("rerun-requested",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (QueryResultClass, rerun_requested),
                              NULL, NULL,
                              _qe_marshal_VOID__POINTER_POINTER, G_TYPE_NONE,
                              2, G_TYPE_POINTER, G_TYPE_POINTER);

	object_class->finalize = query_result_finalize;
}

static void
query_result_init (QueryResult *result, G_GNUC_UNUSED QueryResultClass *klass)
{
	GtkWidget *wid;

	/* allocate private structure */
	result->priv = g_new0 (QueryResultPrivate, 1);
	result->priv->history = NULL;
	result->priv->hash = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (result), GTK_ORIENTATION_VERTICAL);

	wid = make_widget_for_notice ();
	gtk_box_pack_start (GTK_BOX (result), wid, TRUE, TRUE, 0);
	gtk_widget_show (wid);
	result->priv->child = wid;
}

static void
history_item_removed_cb (G_GNUC_UNUSED QueryEditor *history, QueryEditorHistoryItem *item,
			 QueryResult *result)
{
	g_hash_table_remove (result->priv->hash, item);
}

static void
history_cleared_cb (G_GNUC_UNUSED QueryEditor *history, QueryResult *result)
{
	g_hash_table_remove_all (result->priv->hash);
}

static void
query_result_finalize (GObject *object)
{
	QueryResult *result = (QueryResult *) object;

	g_return_if_fail (IS_QUERY_RESULT (result));

	/* free memory */
	if (result->priv->hash)
		g_hash_table_destroy (result->priv->hash);
	if (result->priv->history) {
		g_signal_handlers_disconnect_by_func (result->priv->history,
						      G_CALLBACK (history_item_removed_cb), result);
		g_signal_handlers_disconnect_by_func (result->priv->history,
						      G_CALLBACK (history_cleared_cb), result);
		g_object_unref (result->priv->history);
	}

	g_free (result->priv);
	result->priv = NULL;

	parent_class->finalize (object);
}

GType
query_result_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (QueryResultClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_result_class_init,
			NULL,
			NULL,
			sizeof (QueryResult),
			0,
			(GInstanceInitFunc) query_result_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "QueryResult", &info, 0);
	}
	return type;
}

/**
 * query_result_new
 * @history: a #QueryEditor to take history from
 *
 * Create a new #QueryResult widget
 *
 * Returns: the newly created widget.
 */
GtkWidget *
query_result_new (QueryEditor *history)
{
	QueryResult *result;
	g_return_val_if_fail (QUERY_IS_EDITOR (history), NULL);

	result = g_object_new (QUERY_TYPE_RESULT, NULL);
	g_signal_connect (history, "history-item-removed",
			  G_CALLBACK (history_item_removed_cb), result);
	g_signal_connect (history, "history-cleared",
			  G_CALLBACK (history_cleared_cb), result);
	result->priv->history = g_object_ref (history);

	return GTK_WIDGET (result);
}

/**
 * query_result_show_history_batch
 * @qres: a #QueryResult widget
 * @hbatch: the #QueryEditorHistoryBatch to display, or %NULL
 *
 * 
 */
void
query_result_show_history_batch (QueryResult *qres, QueryEditorHistoryBatch *hbatch)
{
	GSList *list;
	GtkWidget *child;	
	GtkWidget *vbox;
	gchar *str;

	g_return_if_fail (IS_QUERY_RESULT (qres));
	if (qres->priv->child)
		gtk_container_remove (GTK_CONTAINER (qres), qres->priv->child);

	if (!hbatch) {
		child = make_widget_for_notice ();
		gtk_box_pack_start (GTK_BOX (qres), child, TRUE, TRUE, 10);
		gtk_widget_show (child);
		qres->priv->child = child;
		return;
	}

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	child = query_editor_new ();
	query_editor_set_mode (QUERY_EDITOR (child), QUERY_EDITOR_READONLY);
	gtk_box_pack_start (GTK_BOX (vbox), child, TRUE, TRUE, 0);
	for (list = hbatch->hist_items; list; list = list->next) {
		QueryEditorHistoryItem *hitem;
		GString *string;
		hitem = (QueryEditorHistoryItem *) list->data;
		if (list != hbatch->hist_items)
			query_editor_append_text (QUERY_EDITOR (child), "\n");
		query_editor_append_note (QUERY_EDITOR (child), _("Statement:"), 0);
		query_editor_append_text (QUERY_EDITOR (child), hitem->sql);

		string = g_string_new ("");
		if (hitem->result) {
			if (GDA_IS_DATA_MODEL (hitem->result)) {
				gint n, c;
				gchar *tmp1, *tmp2;
				n = gda_data_model_get_n_rows (GDA_DATA_MODEL (hitem->result));
				c = gda_data_model_get_n_columns (GDA_DATA_MODEL (hitem->result));
				tmp1 = g_strdup_printf (ngettext ("%d row", "%d rows", n), n);
				tmp2 = g_strdup_printf (ngettext ("%d column", "%d columns", c), c);
				g_string_append_printf (string, 
							_("Data set with %s and %s"),
							tmp1, tmp2);
				g_free (tmp1);
				g_free (tmp2);

				gdouble etime;
				g_object_get (hitem->result, "execution-delay", &etime, NULL);
				g_string_append (string, "\n");
				g_string_append_printf (string, _("Execution delay"));
				g_string_append_printf (string, ": %.03f s", etime);
			}
			else if (GDA_IS_SET (hitem->result)) {
				GdaSet *set;
				GSList *list;
				set = GDA_SET (hitem->result);
				for (list = gda_set_get_holders (set); list; list = list->next) {
					GdaHolder *h;
					const GValue *value;
					const gchar *cstr;
					gchar *tmp;
					h = GDA_HOLDER (list->data);
					
					if (list != gda_set_get_holders (set))
						g_string_append_c (string, '\n');
					
					cstr = gda_holder_get_id (h);
					value = gda_holder_get_value (h);
					if (!strcmp (cstr, "IMPACTED_ROWS")) {
						g_string_append (string, _("Number of rows impacted"));
						g_string_append (string, ": ");
						tmp = gda_value_stringify (value);
						g_free (tmp);
					}
					else if (!strcmp (cstr, "EXEC_DELAY")) {
						gdouble etime;
						etime = g_value_get_double (value);
						g_string_append_printf (string, _("Execution delay"));
						g_string_append_printf (string, ": %.03f s", etime);
					}
					else {
						g_string_append (string, cstr);
						g_string_append (string, ": ");
						tmp = gda_value_stringify (value);
						g_string_append_printf (string, "%s", tmp);
						g_free (tmp);
					}
				}
			}
			else
				g_assert_not_reached ();
		}
		else 
			g_string_append_printf (string, _("Error: %s"),
						hitem->exec_error && hitem->exec_error->message ?
						hitem->exec_error->message : _("No detail"));
		query_editor_append_note (QUERY_EDITOR (child), string->str, 1);
		g_string_free (string, TRUE);
	}

	if (hbatch->params) {
		GtkWidget *exp, *form;
		str = g_strdup_printf ("<b>%s:</b>", _("Execution Parameters"));
		exp = gtk_expander_new (str);
		g_free (str);
		gtk_expander_set_use_markup (GTK_EXPANDER (exp), TRUE);
		gtk_box_pack_start (GTK_BOX (vbox), exp, FALSE, FALSE, 0);

		form = gdaui_basic_form_new (hbatch->params);
		gdaui_basic_form_entry_set_editable (GDAUI_BASIC_FORM (form), NULL, FALSE);
		gtk_container_add (GTK_CONTAINER (exp), form);
	}

	gtk_box_pack_start (GTK_BOX (qres), vbox, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);
	qres->priv->child = vbox;
}

/**
 * query_result_show_history_item
 * @qres: a #QueryResult widget
 * @hitem: the #QueryEditorHistoryItem to display, or %NULL
 *
 * 
 */
void
query_result_show_history_item (QueryResult *qres, QueryEditorHistoryItem *hitem)
{
	GtkWidget *child;

	g_return_if_fail (IS_QUERY_RESULT (qres));
	if (qres->priv->child)
		gtk_container_remove (GTK_CONTAINER (qres), qres->priv->child);
	
	if (!hitem)
		child = make_widget_for_notice ();
	else {
		child = g_hash_table_lookup (qres->priv->hash, hitem);
		if (!child) {
			if (hitem->result) {
				if (GDA_IS_DATA_MODEL (hitem->result))
					child = make_widget_for_data_model (GDA_DATA_MODEL (hitem->result),
									    qres, hitem->sql);
				else if (GDA_IS_SET (hitem->result))
					child = make_widget_for_set (GDA_SET (hitem->result));
				else
					g_assert_not_reached ();
			}
			else 
				child = make_widget_for_error (hitem->exec_error);

			g_hash_table_insert (qres->priv->hash, hitem, g_object_ref_sink (G_OBJECT (child)));
		}
	}

	gtk_box_pack_start (GTK_BOX (qres), child, TRUE, TRUE, 0);
	gtk_widget_show (child);
	qres->priv->child = child;
}

static GtkWidget *
make_widget_for_notice (void)
{
	GtkWidget *label;
	label = gtk_label_new (_("No result selected"));
	return label;
}

static void
action_refresh_cb (G_GNUC_UNUSED GtkWidget *button, QueryResult *qres)
{
	QueryEditorHistoryBatch *batch;
	QueryEditorHistoryItem *item;
	item = query_editor_get_current_history_item (qres->priv->history, &batch);
	g_signal_emit (qres, query_result_signals [RERUN_REQUESTED], 0, batch, item);
}

static GtkWidget *
make_widget_for_data_model (GdaDataModel *model, QueryResult *qres, const gchar *sql)
{
	GtkWidget *grid;
	grid = ui_formgrid_new (model, TRUE, GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS);
	ui_formgrid_set_sample_size (UI_FORMGRID (grid), 300);
	if (sql) {
		TConnection *tcnc;
		tcnc = browser_window_get_connection ((BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) qres));
		if (!tcnc)
			goto out;

		GdaSqlParser *parser;
		GdaStatement *stmt;
		parser = t_connection_create_parser (tcnc);
		stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
		g_object_unref (parser);
		if (!stmt)
			goto out;
		ui_formgrid_handle_user_prefs (UI_FORMGRID (grid), tcnc, stmt);
		g_object_unref (stmt);

		ui_formgrid_set_refresh_func (UI_FORMGRID (grid), G_CALLBACK (action_refresh_cb), qres);
	}
 out:
	return grid;
}

static GtkWidget *
make_widget_for_set (GdaSet *set)
{
	GtkWidget *hbox, *img;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	
	img = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_halign (img, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);

	GtkWidget *label;
	GString *string;
	GSList *list;

	label = gtk_label_new ("");
	string = g_string_new ("");
	for (list = gda_set_get_holders (set); list; list = list->next) {
		GdaHolder *h;
		const GValue *value;
		gchar *tmp;
		const gchar *cstr;
		h = GDA_HOLDER (list->data);

		if (list != gda_set_get_holders (set))
			g_string_append_c (string, '\n');
		
		cstr = gda_holder_get_id (h);
		value = gda_holder_get_value (h);
		if (!strcmp (cstr, "IMPACTED_ROWS")) {
			g_string_append_printf (string, "<b>%s:</b> ",
						_("Number of rows impacted"));
			tmp = gda_value_stringify (value);
			g_string_append_printf (string, "%s", tmp);
			g_free (tmp);
		}
		else if (!strcmp (cstr, "EXEC_DELAY")) {
			g_string_append_printf (string, "<b>%s:</b> ",
						_("Execution delay"));
			gdouble etime;
			etime = g_value_get_double (value);
			g_string_append_printf (string, "%.03f s", etime);
		}
		else {
			tmp = g_markup_escape_text (cstr, -1);
			g_string_append_printf (string, "<b>%s:</b> ", tmp);
			g_free (tmp);

			tmp = gda_value_stringify (value);
			g_string_append_printf (string, "%s", tmp);
			g_free (tmp);
		}
	}
	gtk_label_set_markup (GTK_LABEL (label), string->str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	g_string_free (string, TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);
	gtk_widget_hide (hbox);

	return hbox;
}

static GtkWidget *
make_widget_for_error (GError *error)
{
	GtkWidget *hbox, *img;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	
	img = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_halign (img, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);

	GtkWidget *label;
	GString *string;
	gchar *tmp;

	label = gtk_label_new ("");
	string = g_string_new ("");
	g_string_append_printf (string, "<b>%s</b>  ", _("Execution error:\n"));
	if (error && error->message) {
		tmp = g_markup_escape_text (error->message, -1);
		g_string_append (string, tmp);
		g_free (tmp);
	}
	else
		g_string_append (string, _("No detail"));

	gtk_label_set_markup (GTK_LABEL (label), string->str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_string_free (string, TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);	

	gtk_widget_show_all (hbox);
	gtk_widget_hide (hbox);
	return hbox;
}
