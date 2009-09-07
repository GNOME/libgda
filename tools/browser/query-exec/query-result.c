/* GNOME DB library
 * Copyright (C) 2009 The GNOME Foundation.
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
#include "query-result.h"
#include <libgda-ui/libgda-ui.h>

struct _QueryResultPrivate {
	QueryEditorHistoryBatch *hbatch;
	QueryEditorHistoryItem *hitem;

	GtkWidget *child;
};

static void query_result_class_init (QueryResultClass *klass);
static void query_result_init       (QueryResult *result, QueryResultClass *klass);
static void query_result_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

static GtkWidget *make_widget_for_notice (void);
static GtkWidget *make_widget_for_data_model (GdaDataModel *model);
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

	object_class->finalize = query_result_finalize;
}

static void
query_result_init (QueryResult *result, QueryResultClass *klass)
{
	GtkWidget *wid;

	/* allocate private structure */
	result->priv = g_new0 (QueryResultPrivate, 1);
	result->priv->hbatch = NULL;
	result->priv->hitem = NULL;

	wid = make_widget_for_notice ();
	gtk_box_pack_start (GTK_BOX (result), wid, TRUE, TRUE, 0);
	gtk_widget_show (wid);
	result->priv->child = wid;
}

static void
query_result_finalize (GObject *object)
{
	QueryResult *result = (QueryResult *) object;

	g_return_if_fail (IS_QUERY_RESULT (result));

	/* free memory */
	if (result->priv->hbatch)
		query_editor_history_batch_unref (result->priv->hbatch);
	if (result->priv->hitem)
		query_editor_history_item_unref (result->priv->hitem);

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
			(GInstanceInitFunc) query_result_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "QueryResult", &info, 0);
	}
	return type;
}

/**
 * query_result_new
 *
 * Create a new #QueryResult widget
 *
 * Returns: the newly created widget.
 */
GtkWidget *
query_result_new (void)
{
	QueryResult *result;

	result = g_object_new (QUERY_TYPE_RESULT, NULL);

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
	GtkWidget *sw, *vbox;
	GSList *list;
	GtkWidget *child;
	

	g_return_if_fail (IS_QUERY_RESULT (qres));
	if (qres->priv->child)
		gtk_widget_destroy (qres->priv->child);

	if (!hbatch) {
		child = make_widget_for_notice ();
		gtk_box_pack_start (GTK_BOX (qres), child, TRUE, TRUE, 10);
		gtk_widget_show (child);
		qres->priv->child = child;
		return;
	}

	vbox = gtk_vbox_new (FALSE, 0);
	for (list = hbatch->hist_items; list; list = list->next) {
		QueryEditorHistoryItem *hitem;

		hitem = (QueryEditorHistoryItem*) list->data;
		if (hitem->result) {
			if (GDA_IS_DATA_MODEL (hitem->result))
				child = make_widget_for_data_model (GDA_DATA_MODEL (hitem->result));
			else if (GDA_IS_SET (hitem->result))
				child = make_widget_for_set (GDA_SET (hitem->result));
			else
				g_assert_not_reached ();
		}
		else 
			child = make_widget_for_error (hitem->exec_error);
		gtk_box_pack_start (GTK_BOX (vbox), child, TRUE, TRUE, 10);
	}

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);

	gtk_box_pack_start (GTK_BOX (qres), sw, TRUE, TRUE, 0);
	gtk_widget_show_all (sw);
	qres->priv->child = sw;
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
		gtk_widget_destroy (qres->priv->child);
	
	if (!hitem)
		child = make_widget_for_notice ();
	else if (hitem->result) {
		if (GDA_IS_DATA_MODEL (hitem->result))
			child = make_widget_for_data_model (GDA_DATA_MODEL (hitem->result));
		else if (GDA_IS_SET (hitem->result))
			child = make_widget_for_set (GDA_SET (hitem->result));
		else
			g_assert_not_reached ();
	}
	else 
		child = make_widget_for_error (hitem->exec_error);

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

static GtkWidget *
make_widget_for_data_model (GdaDataModel *model)
{
	GtkWidget *grid;
	grid = gdaui_grid_new (model);
	gdaui_grid_set_sample_size (GDAUI_GRID (grid), 300);
	return grid;
}

static GtkWidget *
make_widget_for_set (GdaSet *set)
{
	GtkWidget *hbox, *img;
	hbox = gtk_hbox_new (FALSE, 5);
	
	img = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (img), 0., 0.);
	gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);

	GtkWidget *label;
	GString *string;
	GSList *list;

	label = gtk_label_new ("");
	string = g_string_new ("");
	for (list = set->holders; list; list = list->next) {
		GdaHolder *h;
		const GValue *value;
		gchar *tmp;
		const gchar *cstr;
		h = GDA_HOLDER (list->data);

		if (list != set->holders)
			g_string_append_c (string, '\n');
		
		cstr = gda_holder_get_id (h);
		if (!strcmp (cstr, "IMPACTED_ROWS"))
			g_string_append_printf (string, "<b>%s</b>  ",
						_("Number of rows impacted:"));
		else {
			tmp = g_markup_escape_text (cstr, -1);
			g_string_append_printf (string, "<b>%s</b>", tmp);
			g_free (tmp);
		}

		value = gda_holder_get_value (h);
		tmp = gda_value_stringify (value);
		g_string_append_printf (string, "%s", tmp);
		g_free (tmp);
	}
	gtk_label_set_markup (GTK_LABEL (label), string->str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., 0.);
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
	hbox = gtk_hbox_new (FALSE, 5);
	
	img = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (img), 0., 0.);
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
	gtk_misc_set_alignment (GTK_MISC (label), 0., 0.);
	g_string_free (string, TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);	

	gtk_widget_show_all (hbox);
	gtk_widget_hide (hbox);
	return hbox;
}
