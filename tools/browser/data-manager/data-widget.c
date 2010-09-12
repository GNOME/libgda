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
#include "../common/ui-formgrid.h"
#include "../browser-window.h"
#include "../support.h"
#include "data-source-editor.h"

/*
 * The DataPart structure represents the execution of a single DataSource 
 */
typedef struct {
	DataWidget *dwid;
	DataSource *source;

	GtkWidget *top;
	GtkNotebook *nb; /* page 0: spinner
			    page 1 or 2, depends on @data_widget_page, @error_widget_page and @edit_widget_page*/
	gint data_widget_page;
	gint error_widget_page;
	gint edit_widget_page;
	gint edit_widget_previous_page;

	BrowserSpinner *spinner;
	guint spinner_show_timer_id;
	GtkWidget *data_widget;
	GtkWidget *error_widget;
	GtkWidget *edit_widget;
	GdaSet *export_data;

	GSList *dep_parts; /* list of DataPart which need to be re-run when anything in @export_data
			    * changes */
	GtkWidget *menu;
} DataPart;
#define DATA_PART(x) ((DataPart*)(x))

static DataPart *data_part_find (DataWidget *dwid, DataSource *source);
static void data_part_free (DataPart *part, GSList *all_parts);

struct _DataWidgetPrivate {
	DataSourceManager *mgr;
	GtkWidget *top_nb; /* page 0 => error, page 1 => contents */
	GtkWidget *info_label;
	GtkWidget *contents_page_vbox;
	GtkWidget *contents_page;
	GSList *parts;
};

static void data_widget_class_init (DataWidgetClass *klass);
static void data_widget_init       (DataWidget *dwid, DataWidgetClass *klass);
static void data_widget_dispose    (GObject *object);
static void compute_sources_dependencies (DataPart *part);
static void mgr_list_changed_cb (DataSourceManager *mgr, DataWidget *dwid);

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

	/* init Widgets's structure */
	dwid->priv->top_nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (dwid->priv->top_nb), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (dwid->priv->top_nb), FALSE);
	gtk_box_pack_start (GTK_BOX (dwid), dwid->priv->top_nb, TRUE, TRUE, 0);

	/* error page */
#if GTK_CHECK_VERSION (2,18,0)
	GtkWidget *info;
	info = gtk_info_bar_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (dwid->priv->top_nb), info, NULL);
	dwid->priv->info_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (dwid->priv->info_label), 0., -1);
	gtk_label_set_ellipsize (GTK_LABEL (dwid->priv->info_label), PANGO_ELLIPSIZE_END);
	gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (info))),
			   dwid->priv->info_label);
#else
	dwid->priv->info_label = gtk_label_new ("");
	gtk_notebook_append_page (GTK_NOTEBOOK (dwid->priv->top_nb), dwid->priv->info_label, NULL);
#endif

	/* contents page */
	GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (dwid->priv->top_nb), vbox, NULL);
	dwid->priv->contents_page_vbox = vbox;

	gtk_widget_show_all (dwid->priv->top_nb);
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
data_part_free_func (DataPart *part)
{
	data_part_free (part, NULL);
}

static void
data_widget_dispose (GObject *object)
{
	DataWidget *dwid = (DataWidget*) object;
	if (dwid->priv) {
		if (dwid->priv->mgr) {
			g_signal_handlers_disconnect_by_func (dwid->priv->mgr,
							      G_CALLBACK (mgr_list_changed_cb), dwid);
			g_object_unref (dwid->priv->mgr);
		}
		if (dwid->priv->parts) {
			g_slist_foreach (dwid->priv->parts, (GFunc) data_part_free_func, NULL);
			g_slist_free (dwid->priv->parts);
		}
		g_free (dwid->priv);
		dwid->priv = NULL;
	}
	parent_class->dispose (object);
}

static void source_exec_started_cb (DataSource *source, DataPart *part);
static void source_exec_finished_cb (DataSource *source, GError *error, DataPart *part);
static void data_source_menu_clicked_cb (GtkButton *button, DataPart *part);

static DataPart *
create_or_reuse_part (DataWidget *dwid, DataSource *source, gboolean *out_reused)
{
	
	DataPart *part;
	*out_reused = FALSE;

	part = data_part_find (dwid, source);
	if (part) {
		GtkWidget *parent;
		parent = gtk_widget_get_parent (part->top);
		if (parent) {
			g_object_ref ((GObject*) part->top);
			gtk_container_remove (GTK_CONTAINER (parent), part->top);
		}
		*out_reused = TRUE;
		return part;
	}

	part = g_new0 (DataPart, 1);
	part->dwid = dwid;
	part->source = g_object_ref (source);
	part->edit_widget_previous_page = -1;
	g_signal_connect (source, "execution-started",
			  G_CALLBACK (source_exec_started_cb), part);
	g_signal_connect (source, "execution-finished",
			  G_CALLBACK (source_exec_finished_cb), part);

	dwid->priv->parts = g_slist_append (dwid->priv->parts, part);

	GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	part->top = vbox;
	g_object_ref_sink ((GObject*) part->top);

	GtkWidget *header, *label, *button, *image;
	const gchar *cstr;

	header = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	cstr = data_source_get_title (source);
	if (cstr) {
		gchar *tmp;
		tmp = g_markup_printf_escaped ("<b><small>%s</small></b>", cstr);
		gtk_label_set_markup (GTK_LABEL (label), tmp);
		g_free (tmp);
	}
	else
		gtk_label_set_markup (GTK_LABEL (label), "<b><small> </small></b>");
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_widget_set_size_request (label, 150, -1);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (header), label, TRUE, TRUE, 0);

	image = gtk_image_new_from_pixbuf (browser_get_pixbuf_icon (BROWSER_ICON_MENU_INDICATOR));
	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_widget_set_name (button, "browser-tab-close-button");
	gtk_widget_set_tooltip_text (button, _("Link to other data"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (data_source_menu_clicked_cb), part);
		
	gtk_container_add (GTK_CONTAINER (button), image);
	gtk_container_set_border_width (GTK_CONTAINER (button), 0);
	gtk_box_pack_start (GTK_BOX (header), button, FALSE, FALSE, 0);

	GtkWidget *nb, *page;
	nb = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (nb), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	part->nb = GTK_NOTEBOOK (nb);

	part->spinner = BROWSER_SPINNER (browser_spinner_new ());
	browser_spinner_set_size ((BrowserSpinner*) part->spinner, GTK_ICON_SIZE_LARGE_TOOLBAR);
	page = gtk_alignment_new (0.5, 0.5, 0., 0.);
	gtk_container_add (GTK_CONTAINER (page), (GtkWidget*) part->spinner);
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

static void
remove_data_source_mitem_activated_cb (GtkMenuItem *mitem, DataPart *part)
{
	data_source_manager_remove_source (part->dwid->priv->mgr, part->source);
}

static void
add_data_source_mitem_activated_cb (GtkMenuItem *mitem, DataPart *part)
{
	DataSource *source;
	GError *lerror = NULL;
	xmlNodePtr sourcespec;
	BrowserConnection *bcnc;

	bcnc = data_source_manager_get_browser_cnc (part->dwid->priv->mgr);
	sourcespec = (xmlNodePtr) g_object_get_data ((GObject*) mitem, "xml");
	source = data_source_new_from_xml_node (bcnc, sourcespec, &lerror);
	if (source) {
		data_source_manager_add_source (part->dwid->priv->mgr, source);
		g_object_unref (source);
	}
	else {
		BrowserWindow *bwin;
		bwin = BROWSER_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (part->dwid)));
		browser_window_show_notice_printf (bwin, GTK_MESSAGE_ERROR, "data-widget-add-new-source",
						   _("Error adding new data source: %s"),
						   lerror && lerror->message ? lerror->message :
						   _("No detail"));
		g_clear_error (&lerror);
	}

#ifdef GDA_DEBUG_NO
	xmlBufferPtr buffer;
	buffer = xmlBufferCreate ();
	xmlNodeDump (buffer, NULL, sourcespec, 0, 1);
	g_print ("Source to ADD: [%s]\n", (gchar *) xmlBufferContent (buffer));
	xmlBufferFree (buffer);
#endif
}

static void
data_source_props_activated_cb (GtkCheckMenuItem *mitem, DataPart *part)
{
	if (gtk_check_menu_item_get_active (mitem)) {
		part->edit_widget_previous_page = gtk_notebook_get_current_page (part->nb);
		if (! part->edit_widget) {
			part->edit_widget = data_source_editor_new ();
			data_source_editor_set_readonly (DATA_SOURCE_EDITOR (part->edit_widget));
			part->edit_widget_page = gtk_notebook_append_page (part->nb, part->edit_widget,
									   NULL);
			gtk_widget_show (part->edit_widget);
		}
		data_source_editor_display_source (DATA_SOURCE_EDITOR (part->edit_widget),
						   part->source);
		gtk_notebook_set_current_page (part->nb, part->edit_widget_page);
	}
	else
		gtk_notebook_set_current_page (part->nb, part->edit_widget_previous_page);
}

static gchar *compute_fk_dependency (GdaMetaTableForeignKey *fkey, GSList *selfields, gboolean reverse,
				     DataPart *part, xmlNodePtr *out_sourcespec);
static void
data_source_menu_clicked_cb (GtkButton *button, DataPart *part)
{
	if (! part->menu) {
		GtkWidget *menu, *mitem;
		menu = gtk_menu_new ();
		part->menu = menu;

		mitem = gtk_menu_item_new_with_label (_("Remove data source"));
		g_signal_connect (mitem, "activate",
				  G_CALLBACK (remove_data_source_mitem_activated_cb), part);
		gtk_widget_show (mitem);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

		mitem = gtk_check_menu_item_new_with_label (_("Show data source's properties"));
		g_signal_connect (mitem, "activate",
				  G_CALLBACK (data_source_props_activated_cb), part);
		gtk_widget_show (mitem);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);

		GdaStatement *stmt;
		stmt = data_source_get_statement (part->source);
		if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) {
			GSList *fields = NULL, *flist;
			GdaSqlStatement *sql_statement;
			BrowserConnection *bcnc;
			GHashTable *hash; /* key = a menu string, value= 0x1 */

			g_object_get (G_OBJECT (stmt), "structure", &sql_statement, NULL);

			bcnc = data_source_manager_get_browser_cnc (part->dwid->priv->mgr);
			if (browser_connection_check_sql_statement_validify (bcnc, sql_statement, NULL))
				fields = ((GdaSqlStatementSelect *) sql_statement->contents)->expr_list;

			hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
			for (flist = fields; flist; flist = flist->next) {
				GdaSqlSelectField *select_field = (GdaSqlSelectField*) flist->data;
				if (!select_field->validity_meta_table_column)
					continue;
				GdaMetaDbObject *dbo;
				GdaMetaTableColumn *field;
				dbo = select_field->validity_meta_object;
				field = select_field->validity_meta_table_column;
				if (dbo->obj_type == GDA_META_DB_TABLE) {
					GdaMetaTable *mtable;
					mtable = GDA_META_TABLE (dbo);

					GSList *fklist;
					for (fklist = mtable->reverse_fk_list; fklist; fklist = fklist->next) {
						GdaMetaTableForeignKey *fkey;
						fkey = (GdaMetaTableForeignKey *) fklist->data;
						gchar *tmp;
						xmlNodePtr sourcespec = NULL;
						if (fkey->meta_table->obj_type != GDA_META_DB_TABLE)
							continue;
						tmp = compute_fk_dependency (fkey, fields, FALSE,
									     part, &sourcespec);
						if (!tmp)
							continue;
						if (g_hash_table_lookup (hash, tmp)) {
							g_free (tmp);
							continue;
						}

						mitem = gtk_menu_item_new_with_label (tmp);
						g_object_set_data_full ((GObject*) mitem, "xml", sourcespec,
									(GDestroyNotify) xmlFreeNode);
						g_signal_connect (mitem, "activate",
								  G_CALLBACK (add_data_source_mitem_activated_cb),
								  part);
						gtk_widget_show (mitem);
						gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
						g_hash_table_insert (hash, tmp, (gpointer) 0x1);
					}
					for (fklist = mtable->fk_list; fklist; fklist = fklist->next) {
						GdaMetaTableForeignKey *fkey;
						fkey = (GdaMetaTableForeignKey *) fklist->data;
						gchar *tmp;
						xmlNodePtr sourcespec = NULL;
						if (fkey->depend_on->obj_type != GDA_META_DB_TABLE)
							continue;
						tmp = compute_fk_dependency (fkey, fields, TRUE,
									     part, &sourcespec);
						if (!tmp)
							continue;
						if (g_hash_table_lookup (hash, tmp)) {
							g_free (tmp);
							continue;
						}
						mitem = gtk_menu_item_new_with_label (tmp);
						g_object_set_data_full ((GObject*) mitem, "xml", sourcespec,
									(GDestroyNotify) xmlFreeNode);
						g_signal_connect (mitem, "activate",
								  G_CALLBACK (add_data_source_mitem_activated_cb),
								  part);
						gtk_widget_show (mitem);
						gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
						g_hash_table_insert (hash, tmp, (gpointer) 0x1);
					}
				}
			}
			g_hash_table_destroy (hash);
			gda_sql_statement_free (sql_statement);
		}
	}
	gtk_menu_popup (GTK_MENU (part->menu), NULL, NULL,
                        NULL, NULL, 0,
                        gtk_get_current_event_time ());
}

/*
 * Returns: a new list of #GdaSqlSelectField (which are already listed in @selfields), or %NULL
 */
static gchar *
compute_fk_dependency (GdaMetaTableForeignKey *fkey, GSList *selfields, gboolean reverse,
		       DataPart *part, xmlNodePtr *out_sourcespec)
{
	GString *string = NULL;
	gint i;
	gint *index_array;
	GdaMetaTable *table;

	*out_sourcespec = NULL;

	if (reverse) {
		table = GDA_META_TABLE (fkey->meta_table);
		index_array = fkey->fk_cols_array;
	}
	else {
		table = GDA_META_TABLE (fkey->depend_on);
		index_array = fkey->ref_pk_cols_array;
	}
	for (i = 0; i < fkey->cols_nb; i++) {
		gint pos;
		GdaMetaTableColumn *col;
		pos = index_array[i] - 1;
		col = g_slist_nth_data (table->columns, pos);

		/* make sure @col is among @selfields */
		GSList *flist;
		gboolean found = FALSE;
		for (flist = selfields; flist; flist = flist->next) {
			GdaSqlSelectField *select_field = (GdaSqlSelectField*) flist->data;
			if (!select_field->validity_meta_object ||
			    !select_field->validity_meta_table_column)
				continue;
			GdaMetaTableColumn *field;
			field = select_field->validity_meta_table_column;
			if (field == col) {
				found = TRUE;
				if (reverse) {
					if (!string) {
						string = g_string_new (_("Obtain referenced data in table "));
						g_string_append_printf (string, "%s from ",
									fkey->depend_on->obj_short_name);
					}
					else
						g_string_append (string, ", ");
					g_string_append_printf (string, "column n.%d (",
								g_slist_position (selfields, flist) + 1);
					if (select_field->as)
						g_string_append (string, select_field->as);
					else
						g_string_append (string,
								 select_field->validity_meta_table_column->column_name);
					g_string_append_c (string, ')');
				}
				else {
					if (!string) {
						string = g_string_new (_("List referencing data in "));
						g_string_append_printf (string, "%s.",
									fkey->meta_table->obj_short_name);
					}
					else
						g_string_append (string, ", ");
					g_string_append (string, fkey->fk_names_array [i]);
				}
				break;
			}
		}

		if (! found) {
			if (string) {
				g_string_free (string, TRUE);
				string = NULL;
			}
			break;
		}
	}

	if (string) {
		xmlNodePtr node, snode;
		node = xmlNewNode (NULL, BAD_CAST "table");
		xmlSetProp (node, BAD_CAST "name",
 			    BAD_CAST (reverse ? GDA_META_DB_OBJECT (fkey->depend_on)->obj_short_name : 
				      GDA_META_DB_OBJECT (fkey->meta_table)->obj_short_name));

		snode = xmlNewChild (node, NULL, BAD_CAST "depend", NULL);
		xmlSetProp (snode, BAD_CAST "foreign_key_table",
			    BAD_CAST GDA_META_DB_OBJECT (table)->obj_short_name);

		xmlSetProp (snode, BAD_CAST "id", BAD_CAST data_source_get_id (part->source));

		gint i;
		for (i = 0; i < fkey->cols_nb; i++)
			xmlNewChild (snode, NULL, BAD_CAST "column", BAD_CAST (fkey->fk_names_array[i]));

		*out_sourcespec = node;
		return g_string_free (string, FALSE);
	}
	else
		return NULL;
}

static void
update_layout (DataWidget *dwid)
{
	GSList *newparts_list = NULL;
	GArray *sources_array;
	GError *lerror = NULL;
	GtkWidget *new_contents;

	new_contents = gtk_vbox_new (FALSE, 0);

	sources_array = data_source_manager_get_sources_array (dwid->priv->mgr, &lerror);
	if (!sources_array) {
		gchar *str;
		if (lerror && lerror->message)
			str = g_strdup_printf (_("Error: %s"), lerror->message);
		else {
			const GSList *list;
			list = data_source_manager_get_sources (dwid->priv->mgr);
			if (list)
				str = g_strdup_printf (_("Error: %s"), _("No detail"));
			else
				str = g_strdup_printf (_("Error: %s"), _("No data source defined"));
		}
                g_clear_error (&lerror);
                gtk_label_set_text (GTK_LABEL (dwid->priv->info_label), str);
                g_free (str);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (dwid->priv->top_nb), 0);
		goto cleanups;
	}

	if (sources_array->len == 1) {
		GArray *subarray = g_array_index (sources_array, GArray*, 0);
		if (subarray->len == 1) {
			DataPart *part;
			gboolean reused;
			DataSource *source;
			source = g_array_index (subarray, DataSource*, 0);
			part = create_or_reuse_part (dwid, source, &reused);
			gtk_box_pack_start (GTK_BOX (new_contents), part->top, TRUE, TRUE, 0);
			g_object_unref ((GObject*) part->top);
			newparts_list = g_slist_prepend (newparts_list, part);
			if (!reused) {
				compute_sources_dependencies (part);
				data_source_execute (source, NULL);
			}
		}
		else {
			GSList *paned_list;
			gint i;
			paned_list = make_paned_list (subarray->len, FALSE);
			gtk_box_pack_start (GTK_BOX (new_contents),
					    GTK_WIDGET (paned_list->data), TRUE, TRUE, 0);
			for (i = 0; i < subarray->len; i++) {
				DataPart *part;
				gboolean reused;
				DataSource *source;
				source = g_array_index (subarray, DataSource*, i);
				part = create_or_reuse_part (dwid, source, &reused);
				pack_in_paned_list (paned_list, subarray->len, i, part->top);
				g_object_unref ((GObject*) part->top);
				newparts_list = g_slist_prepend (newparts_list, part);
				if (!reused) {
					compute_sources_dependencies (part);
					data_source_execute (source, NULL);
				}
			}
			g_slist_free (paned_list);
		}
	}
	else {
		GSList *top_paned_list;
		gint j;
		
		top_paned_list = make_paned_list (sources_array->len, TRUE);
		gtk_box_pack_start (GTK_BOX (new_contents),
				    GTK_WIDGET (top_paned_list->data), TRUE, TRUE, 0);
		for (j = 0; j < sources_array->len; j++) {
			GArray *subarray = g_array_index (sources_array, GArray*, j);
			DataSource *source;
			
			if (subarray->len == 1) {
				DataPart *part;
				gboolean reused;
				source = g_array_index (subarray, DataSource*, 0);
				part = create_or_reuse_part (dwid, source, &reused);
				pack_in_paned_list (top_paned_list, sources_array->len, j, part->top);
				g_object_unref ((GObject*) part->top);
				newparts_list = g_slist_prepend (newparts_list, part);
				if (!reused) {
					compute_sources_dependencies (part);
					data_source_execute (source, NULL);
				}
			}
			else {
				GSList *paned_list;
				gint i;
				paned_list = make_paned_list (subarray->len, FALSE);
				pack_in_paned_list (top_paned_list, sources_array->len, j,
						    GTK_WIDGET (paned_list->data));
				for (i = 0; i < subarray->len; i++) {
					DataPart *part;
					gboolean reused;
					source = g_array_index (subarray, DataSource*, i);
					part = create_or_reuse_part (dwid, source, &reused);
					pack_in_paned_list (paned_list, subarray->len, i, part->top);
					g_object_unref ((GObject*) part->top);
					newparts_list = g_slist_prepend (newparts_list, part);
					if (!reused) {
						compute_sources_dependencies (part);
						data_source_execute (source, NULL);
					}
				}
				g_slist_free (paned_list);
			}
		}
		g_slist_free (top_paned_list);
	}
	data_source_manager_destroy_sources_array (sources_array);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (dwid->priv->top_nb), 1);
	
 cleanups:
	{
		GSList *list, *nlist = NULL;
		for (list = dwid->priv->parts; list; list = list->next) {
			if (!g_slist_find (newparts_list, list->data)) {
				/* useless part */
				data_part_free ((DataPart *) list->data, dwid->priv->parts);
			}
			else
				nlist = g_slist_prepend (nlist, list->data);
		}
		g_slist_free (newparts_list);
		g_slist_free (dwid->priv->parts);
		dwid->priv->parts = g_slist_reverse (nlist);
	}

	gtk_box_pack_start (GTK_BOX (dwid->priv->contents_page_vbox), new_contents, TRUE, TRUE, 0);
	gtk_widget_show_all (new_contents);
	if (dwid->priv->contents_page)
		gtk_widget_destroy (dwid->priv->contents_page);
	dwid->priv->contents_page = new_contents;
}

static void
mgr_list_changed_cb (DataSourceManager *mgr, DataWidget *dwid)
{
	update_layout (dwid);
}

/**
 * data_widget_new
 *
 * Returns: the newly created widget.
 */
GtkWidget *
data_widget_new (DataSourceManager *mgr)
{
	DataWidget *dwid;

	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL); 

	dwid = g_object_new (DATA_WIDGET_TYPE, NULL);
	dwid->priv->mgr = DATA_SOURCE_MANAGER (mgr);
	g_object_ref ((GObject*) dwid->priv->mgr);
	g_signal_connect (mgr, "list_changed",
			  G_CALLBACK (mgr_list_changed_cb), dwid);

	update_layout (dwid);
	return GTK_WIDGET (dwid);
}

static void
data_part_free (DataPart *part, GSList *all_parts)
{
	if (part->spinner_show_timer_id) {
		g_source_remove (part->spinner_show_timer_id);
		part->spinner_show_timer_id = 0;
	}

	if (all_parts) {
		GSList *list;
		for (list = all_parts; list; list = list->next) {
			DataPart *apart = (DataPart *) list->data;
			if (apart == part)
				continue;
			apart->dep_parts = g_slist_remove_all (apart->dep_parts, part);
		}
	}

	if (part->top)
		gtk_widget_destroy (part->top);

	if (part->source) {
		g_signal_handlers_disconnect_by_func (part->source,
						      G_CALLBACK (source_exec_started_cb), part);
		g_signal_handlers_disconnect_by_func (part->source,
						      G_CALLBACK (source_exec_finished_cb), part);
		g_object_unref (part->source);
	}

	if (part->export_data)
		g_object_unref (part->export_data);
	if (part->dep_parts)
		g_slist_free (part->dep_parts);
	if (part->menu)
		gtk_widget_destroy (part->menu);
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

static gboolean
source_exec_started_cb_timeout (DataPart *part)
{
	gtk_notebook_set_current_page (part->nb, 0);
	browser_spinner_start (part->spinner);
	part->spinner_show_timer_id = 0;
	return FALSE; /* remove timer */
}

static void
source_exec_started_cb (DataSource *source, DataPart *part)
{
	if (! part->spinner_show_timer_id)
		part->spinner_show_timer_id = g_timeout_add (300,
							     (GSourceFunc) source_exec_started_cb_timeout,
							     part);
}

static void data_part_selection_changed_cb (GdauiDataSelector *gdauidataselector, DataPart *part);
static void
source_exec_finished_cb (DataSource *source, GError *error, DataPart *part)
{
	GtkWidget *wid;
	if (part->spinner_show_timer_id) {
		g_source_remove (part->spinner_show_timer_id);
		part->spinner_show_timer_id = 0;
	}
	else
		browser_spinner_stop (part->spinner);
	
#ifdef GDA_DEBUG_NO
	g_print ("==== Execution of source [%s] finished\n", data_source_get_title (part->source));
#endif
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
		GtkWidget *cwid;
		BrowserConnection *bcnc;
		bcnc = browser_window_get_connection ((BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) part->dwid));
		cwid = (GtkWidget*) data_source_create_grid (part->source);
		ui_formgrid_handle_user_prefs (UI_FORMGRID (cwid), bcnc,
					       data_source_get_statement (part->source));
		
		wid = (GtkWidget*) ui_formgrid_get_grid_widget (UI_FORMGRID (cwid));
		part->data_widget = wid;
		part->data_widget_page = gtk_notebook_append_page (part->nb, cwid, NULL);
		gtk_widget_show (cwid);
#ifdef GDA_DEBUG_NO
		g_print ("Creating data widget for source [%s]\n",
			 data_source_get_title (part->source));
#endif
		
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
#ifdef GDA_DEBUG_NO
				g_print ("[%s.][%s] bound to [%s].[%s]\n",
					 data_source_get_title (part->source),
					 hid,
					 data_source_get_title (opart->source),
					 gda_holder_get_id (holder2));
#endif
				
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

/**
 * data_widget_rerun
 */
void
data_widget_rerun (DataWidget *dwid)
{
	GSList *parts;
	g_return_if_fail (IS_DATA_WIDGET (dwid));

	for (parts = dwid->priv->parts; parts; parts = parts->next) {
		DataPart *part;
		part = (DataPart*) parts->data;
		data_source_execute (part->source, NULL);		
	}
}
