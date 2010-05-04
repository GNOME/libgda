/* GNOME DB library
 * Copyright (C) 2010 The GNOME Foundation.
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
#include "data-widget.h"
#include "../browser-connection.h"
#include "../browser-spinner.h"

typedef struct {
	DataWidget *dwid;
	DataSource *source;

	GtkWidget *top;
	GtkNotebook *nb; /* page 0: spinner
			    page 1 or 2, depends on @data_widget_page */
	gint data_widget_page;
	gint error_widget_page;
	BrowserSpinner *spinner;
	GtkWidget *data_widget;
	GtkWidget *error_widget;
	GdaSet *export_data;

	GSList *dep_parts; /* list of DataPart which need to be re-run when anything in @export_data
			    * changes */
} DataPart;
#define DATA_PART(x) ((DataPart*)(x))

static DataPart *data_part_find (DataWidget *dwid, DataSource *source);
static void data_part_free (DataPart *part);

struct _DataWidgetPrivate {
	GtkWidget *hpaned;
	GSList *parts;
};

static void data_widget_class_init (DataWidgetClass *klass);
static void data_widget_init       (DataWidget *dwid, DataWidgetClass *klass);
static void data_widget_dispose    (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * DataWidget class implementation
 */
static void
data_widget_class_init (DataWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = data_widget_dispose;
}


static void
data_widget_init (DataWidget *dwid, DataWidgetClass *klass)
{
	g_return_if_fail (IS_DATA_WIDGET (dwid));

	/* allocate private structure */
	dwid->priv = g_new0 (DataWidgetPrivate, 1);
}

GType
data_widget_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (DataWidgetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_widget_class_init,
			NULL,
			NULL,
			sizeof (DataWidget),
			0,
			(GInstanceInitFunc) data_widget_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "DataWidget", &info, 0);
	}
	return type;
}

static void
data_widget_dispose (GObject *object)
{
	DataWidget *dwid = (DataWidget*) object;
	if (dwid->priv) {
		if (dwid->priv->parts) {
			g_slist_foreach (dwid->priv->parts, (GFunc) data_part_free, NULL);
			g_slist_free (dwid->priv->parts);
		}
		g_free (dwid->priv);
		dwid->priv = NULL;
	}
	parent_class->dispose (object);
}

static void source_exec_started_cb (DataSource *source, DataPart *part);
static void source_exec_finished_cb (DataSource *source, GError *error, DataPart *part);

static DataPart *
create_part (DataWidget *dwid, DataSource *source)
{
	DataPart *part;
	part = g_new0 (DataPart, 1);
	part->dwid = dwid;
	part->source = g_object_ref (source);
	g_signal_connect (source, "execution-started",
			  G_CALLBACK (source_exec_started_cb), part);
	g_signal_connect (source, "execution-finished",
			  G_CALLBACK (source_exec_finished_cb), part);

	dwid->priv->parts = g_slist_append (dwid->priv->parts, part);

	GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	part->top = vbox;

	GtkWidget *header;
	const gchar *cstr;
	header = gtk_label_new ("");
	cstr = data_source_get_title (source);
	if (cstr) {
		gchar *tmp;
		tmp = g_markup_printf_escaped ("<b><small>%s</small></b>", cstr);
		gtk_label_set_markup (GTK_LABEL (header), tmp);
		g_free (tmp);
	}
	else
		gtk_label_set_markup (GTK_LABEL (header), "<b><small> </small></b>");
	gtk_misc_set_alignment (GTK_MISC (header), 0., -1);
	gtk_widget_set_size_request (header, 150, -1);
	gtk_label_set_ellipsize (GTK_LABEL (header), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);

	GtkWidget *nb, *page;
	nb = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (nb), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	part->nb = GTK_NOTEBOOK (nb);

	part->spinner = BROWSER_SPINNER (browser_spinner_new ());
	browser_spinner_set_size ((BrowserSpinner*) part->spinner, GTK_ICON_SIZE_LARGE_TOOLBAR);
#if GTK_CHECK_VERSION(2,20,0)
	page = gtk_alignment_new (0.5, 0.5, 0., 0.);
	gtk_container_add (GTK_CONTAINER (page), (GtkWidget*) part->spinner);
#else
	page = GTK_WIDGET (part->spinner);
#endif
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), page, NULL);
	part->data_widget = NULL;

	gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);
	if (data_source_execution_going_on (source))
		source_exec_started_cb (source, part);

	return part;
}

/* make a super-container to contain @size items: the list
 * will have @size-2 paned widgets */
GSList *
make_paned_list (gint size, gboolean horiz)
{
	GSList *list = NULL;
	gint i;
	GtkWidget *paned;

	g_assert (size >= 2);
	paned = horiz ? gtk_hpaned_new () : gtk_vpaned_new ();
	list = g_slist_prepend (list, paned);

	for (i = 2; i < size; i++) {
		GtkWidget *paned2;
		paned2 = horiz ? gtk_hpaned_new () : gtk_vpaned_new ();
		gtk_paned_add2 (GTK_PANED (paned), paned2);
		list = g_slist_prepend (list, paned2);
		paned = paned2;
	}
	return g_slist_reverse (list);
}

static void
pack_in_paned_list (GSList *paned_list, gint length, gint pos, GtkWidget *wid)
{
	GtkPaned *paned;
	if (pos < length - 1) {
		paned = g_slist_nth_data (paned_list, pos);
		gtk_paned_add1 (paned, wid);
	}
	else {
		paned = g_slist_nth_data (paned_list, pos - 1);
		gtk_paned_add2 (paned, wid);
	}
}

/**
 * data_widget_new
 *
 * Returns: the newly created widget.
 */
GtkWidget *
data_widget_new (GArray *sources_array)
{
	DataWidget *dwid;

	g_return_val_if_fail (sources_array, NULL); 
	dwid = g_object_new (DATA_WIDGET_TYPE, NULL);

	if (sources_array->len == 1) {
		GArray *subarray = g_array_index (sources_array, GArray*, 0);
		if (subarray->len == 1) {
			DataPart *part;
			DataSource *source;
			source = g_array_index (subarray, DataSource*, 0);
			part = create_part (dwid, source);
			gtk_box_pack_start (GTK_BOX (dwid), part->top, TRUE, TRUE, 0);

			data_source_execute (source, NULL);
		}
		else {
			GSList *paned_list;
			gint i;
			paned_list = make_paned_list (subarray->len, FALSE);
			gtk_box_pack_start (GTK_BOX (dwid), GTK_WIDGET (paned_list->data), TRUE, TRUE, 0);
			for (i = 0; i < subarray->len; i++) {
				DataPart *part;
				DataSource *source;
				source = g_array_index (subarray, DataSource*, i);
				part = create_part (dwid, source);
				pack_in_paned_list (paned_list, subarray->len, i, part->top);

				data_source_execute (source, NULL);
			}
			g_slist_free (paned_list);
		}
	}
	else {
		GSList *top_paned_list;
		gint j;

		top_paned_list = make_paned_list (sources_array->len, TRUE);
		gtk_box_pack_start (GTK_BOX (dwid), GTK_WIDGET (top_paned_list->data), TRUE, TRUE, 0);
		for (j = 0; j < sources_array->len; j++) {
			GArray *subarray = g_array_index (sources_array, GArray*, j);
			DataSource *source;

			if (subarray->len == 1) {
				DataPart *part;
				source = g_array_index (subarray, DataSource*, 0);
				part = create_part (dwid, source);
				pack_in_paned_list (top_paned_list, sources_array->len, j, part->top);
				
				data_source_execute (source, NULL);
			}
			else {
				GSList *paned_list;
				gint i;
				paned_list = make_paned_list (subarray->len, FALSE);
				pack_in_paned_list (top_paned_list, sources_array->len, j,
						    GTK_WIDGET (paned_list->data));
				for (i = 0; i < subarray->len; i++) {
					DataPart *part;
					source = g_array_index (subarray, DataSource*, i);
					part = create_part (dwid, source);
					pack_in_paned_list (paned_list, subarray->len, i, part->top);
					
					data_source_execute (source, NULL);
				}
				g_slist_free (paned_list);
			}
		}
		g_slist_free (top_paned_list);
	}

	return GTK_WIDGET (dwid);
}

static void
data_part_free (DataPart *part)
{
	if (part->source)
		g_object_unref (part->source);
	if (part->export_data)
		g_object_unref (part->export_data);
	if (part->dep_parts)
		g_slist_free (part->dep_parts);
	g_free (part);
}

static DataPart *
data_part_find (DataWidget *dwid, DataSource *source)
{
	GSList *list;
	for (list = dwid->priv->parts; list; list = list->next) {
		if (DATA_PART (list->data)->source == source)
			return DATA_PART (list->data);
	}
	return NULL;
}

static void
source_exec_started_cb (DataSource *source, DataPart *part)
{
	gtk_notebook_set_current_page (part->nb, 0);
	browser_spinner_start (part->spinner);
}

static void data_part_selection_changed_cb (GdauiDataSelector *gdauidataselector, DataPart *part);
static void compute_sources_dependencies (DataPart *part);
static void
source_exec_finished_cb (DataSource *source, GError *error, DataPart *part)
{
	GtkWidget *wid;
	browser_spinner_stop (part->spinner);
	
	/*g_print ("Execution of source [%s] finished\n", data_source_get_title (part->source));*/
	if (error) {
		gchar *tmp;
		tmp = g_markup_printf_escaped ("\n<b>Error:\n</b>%s",
					       error->message ? error->message : _("Error: no detail"));
		if (! part->error_widget) {
			part->error_widget = gtk_label_new ("");
			gtk_misc_set_alignment (GTK_MISC (part->error_widget), 0., 0.);
			part->error_widget_page = gtk_notebook_append_page (part->nb, part->error_widget,
									    NULL);
			gtk_widget_show (part->error_widget);
		}
		gtk_label_set_markup (GTK_LABEL (part->error_widget), tmp);
		g_free (tmp);
		gtk_notebook_set_current_page (part->nb, part->error_widget_page);
		return;
	}
	
	if (! part->data_widget) {
		wid = (GtkWidget*) data_source_create_grid (part->source);
		part->data_widget = wid;
		part->data_widget_page = gtk_notebook_append_page (part->nb, wid, NULL);
		gtk_widget_show (part->data_widget);
		g_print ("Creating data widget for source [%s]\n", data_source_get_title (part->source));

		/* compute part->export_data */
		GArray *export_names;
		export_names = data_source_get_export_names (part->source);
		if (export_names && (export_names->len > 0)) {
			GSList *holders = NULL;
			GdaDataModel *model;
			GHashTable *export_columns;
			gint i;
			GdaDataModelIter *iter;

			iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (wid));
			g_object_get (wid, "model", &model, NULL);

			export_columns = data_source_get_export_columns (part->source);
			for (i = 0; i < export_names->len; i++) {
				gint col;
				GdaHolder *bindto;
				col = GPOINTER_TO_INT (g_hash_table_lookup (export_columns,
									    g_array_index (export_names,
											   gchar*, i))) - 1;
				bindto = gda_data_model_iter_get_holder_for_field (iter, col);
				if (bindto) {
					GdaHolder *holder;
					holder = gda_holder_copy (bindto);
					g_object_set ((GObject*) holder, "id",
						      g_array_index (export_names, gchar*, i), NULL);
					holders = g_slist_prepend (holders, holder);
#ifdef DEBUG_NO
					g_print ("HOLDER [%s::%s]\n",
						 gda_holder_get_id (holder),
						 g_type_name (gda_holder_get_g_type (holder)));
#endif
					g_assert (gda_holder_set_bind (holder, bindto, NULL));
				}
			}
			
			g_object_unref (model);
			if (holders) {
				part->export_data = gda_set_new (holders);
				g_slist_foreach (holders, (GFunc) g_object_unref, NULL);
				g_slist_free (holders);

				g_signal_connect (wid, "selection-changed",
						  G_CALLBACK (data_part_selection_changed_cb), part);
			}
		}
	}
	gtk_notebook_set_current_page (part->nb, part->data_widget_page);
	compute_sources_dependencies (part);
}

static void
data_part_selection_changed_cb (GdauiDataSelector *gdauidataselector, DataPart *part)
{
	if (part->export_data) {
		GSList *list;
#ifdef DEBUG_NO
		for (list = part->export_data->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			gchar *tmp;
			tmp = gda_value_stringify (gda_holder_get_value (holder));
			g_print ("%s=>[%s]\n", gda_holder_get_id (holder), tmp);
			g_free (tmp);
		}
#endif

		for (list = part->dep_parts; list; list = list->next) {
			DataPart *spart = DATA_PART (list->data);
			data_source_execute (spart->source, NULL);
		}
	}
}

static void
compute_sources_dependencies (DataPart *part)
{
	GdaSet *import;
	import = data_source_get_import (part->source);
	if (!import)
		return;

	GSList *holders;
	for (holders = import->holders; holders; holders = holders->next) {
		GdaHolder *holder = (GdaHolder*) holders->data;
		const gchar *hid = gda_holder_get_id (holder);
		GSList *list;
	
		for (list = part->dwid->priv->parts; list; list = list->next) {
			DataPart *opart = DATA_PART (list->data);
			if (opart == part)
				continue;

			GdaSet *export;
			GdaHolder *holder2;
			export = data_widget_get_export (part->dwid, opart->source);
			if (!export)
				continue;

			holder2 = gda_set_get_holder (export, hid);
			if (holder2) {
				GError *lerror = NULL;
				if (! gda_holder_set_bind (holder, holder2, &lerror)) {
					TO_IMPLEMENT;
					g_print ("Error: %s\n", lerror && lerror->message ? 
						 lerror->message : "???");
					g_clear_error (&lerror);
				}
				g_print ("[%s.][%s] bound to [%s].[%s]\n",
					 data_source_get_title (part->source),
					 hid,
					 data_source_get_title (opart->source),
					 gda_holder_get_id (holder2));
				
				if (! g_slist_find (opart->dep_parts, part))
					opart->dep_parts = g_slist_append (opart->dep_parts, part);
				continue;
			}
		}
	}
}

/**
 * data_widget_get_export
 *
 * Returns: the #GdaSet or %NULL, no ref held to it by the caller
 */
GdaSet *
data_widget_get_export (DataWidget *dwid, DataSource *source)
{
	DataPart *part;
	g_return_val_if_fail (IS_DATA_WIDGET (dwid), NULL);
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	part = data_part_find (dwid, source);
	if (!part) {
		g_warning ("Can't find DataPart for DataSource");
		return NULL;
	}
	return part->export_data;
}
