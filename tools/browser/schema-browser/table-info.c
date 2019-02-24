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
#include "table-info.h"
#include "../dnd.h"
#include "../ui-support.h"
#include "../ui-customize.h"
#include "../gdaui-bar.h"
#include "table-columns.h"
#include "table-preferences.h"
#ifdef HAVE_GOOCANVAS
#include "table-relations.h"
#endif
#include "schema-browser-perspective.h"
#include "../browser-page.h"
#include "../browser-window.h"
#include "../data-manager/data-manager-perspective.h"
#include <libgda-ui/gdaui-enums.h>
#include <libgda-ui/gdaui-basic-form.h>
#include <libgda/gda-data-model-extra.h>
#include "../fk-declare.h"
#include <libgda/gda-debug-macros.h>
#include "../perspectives-names.h"

#define PAGE_COLUMNS "C"
#define PAGE_RELATIONS "R"
#define PAGE_PREFS "P"

struct _TableInfoPrivate {
	TConnection *tcnc;

	gchar *schema;
	gchar *table_name;
	gchar *table_short_name;

	GdauiBar *header;
	GtkWidget *contents; /* notebook with pageO <=> @unknown_table_notice, page1 <=> pages stack */
	GtkWidget *unknown_table_notice;

	GtkWidget *insert_popup;
	GHashTable *insert_columns_hash; /* key = column index as a pointer, value = GdaHolder in the
					 * params used in the INSERT statement */
};

static void table_info_class_init (TableInfoClass *klass);
static void table_info_init       (TableInfo *tinfo, TableInfoClass *klass);
static void table_info_dispose   (GObject *object);
static void table_info_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void table_info_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);
/* BrowserPage interface */
static void                 table_info_page_init (BrowserPageIface *iface);
static void                 table_info_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header);
static GtkWidget           *table_info_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static void meta_changed_cb (TConnection *tcnc, GdaMetaStruct *mstruct, TableInfo *tinfo);

/* properties */
enum {
        PROP_0,
};

static GObjectClass *parent_class = NULL;


/*
 * TableInfo class implementation
 */

static void
table_info_class_init (TableInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Properties */
        object_class->set_property = table_info_set_property;
        object_class->get_property = table_info_get_property;

	object_class->dispose = table_info_dispose;
}

static void
table_info_page_init (BrowserPageIface *iface)
{
	iface->i_customize = table_info_customize;
	iface->i_uncustomize = NULL;
	iface->i_get_tab_label = table_info_get_tab_label;
}

static void
table_info_init (TableInfo *tinfo, G_GNUC_UNUSED TableInfoClass *klass)
{
	tinfo->priv = g_new0 (TableInfoPrivate, 1);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (tinfo), GTK_ORIENTATION_VERTICAL);
}

static void
table_info_dispose (GObject *object)
{
	TableInfo *tinfo = (TableInfo *) object;

	/* free memory */
	if (tinfo->priv) {
		if (tinfo->priv->insert_columns_hash)
			g_hash_table_destroy (tinfo->priv->insert_columns_hash);
		if (tinfo->priv->insert_popup)
			gtk_widget_destroy (tinfo->priv->insert_popup);
		g_free (tinfo->priv->schema);
		g_free (tinfo->priv->table_name);
		g_free (tinfo->priv->table_short_name);
		if (tinfo->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (tinfo->priv->tcnc,
							      G_CALLBACK (meta_changed_cb), tinfo);
			g_object_unref (tinfo->priv->tcnc);
		}

		g_free (tinfo->priv);
		tinfo->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
table_info_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (TableInfoClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) table_info_class_init,
			NULL,
			NULL,
			sizeof (TableInfo),
			0,
			(GInstanceInitFunc) table_info_init,
			0
		};

		static GInterfaceInfo page_info = {
                        (GInterfaceInitFunc) table_info_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "TableInfo", &info, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_info);
	}
	return type;
}

static void
table_info_set_property (GObject *object,
			 guint param_id,
			 G_GNUC_UNUSED const GValue *value,
			 GParamSpec *pspec)
{
	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
table_info_get_property (GObject *object,
			 guint param_id,
			 G_GNUC_UNUSED GValue *value,
			 GParamSpec *pspec)
{
	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
display_table_not_found_error (TableInfo *tinfo, gboolean is_error)
{
	if (is_error)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (tinfo->priv->contents), 0);
	else
		gtk_notebook_set_current_page (GTK_NOTEBOOK (tinfo->priv->contents), 1);
}

static gchar *
table_info_to_selection (TableInfo *tinfo)
{
	GString *string;
	gchar *tmp;
	string = g_string_new ("OBJ_TYPE=table");
	tmp = gda_rfc1738_encode (tinfo->priv->schema);
	g_string_append_printf (string, ";OBJ_SCHEMA=%s", tmp);
	g_free (tmp);
	tmp = gda_rfc1738_encode (tinfo->priv->table_name);
	g_string_append_printf (string, ";OBJ_NAME=%s", tmp);
	g_free (tmp);
	tmp = gda_rfc1738_encode (tinfo->priv->table_short_name);
	g_string_append_printf (string, ";OBJ_SHORT_NAME=%s", tmp);
	g_free (tmp);
	return g_string_free (string, FALSE);
}

static void
source_drag_data_get_cb (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 guint info, G_GNUC_UNUSED guint time, TableInfo *tinfo)
{
	switch (info) {
	case TARGET_KEY_VALUE: {
		gchar *str;
		str = table_info_to_selection (tinfo);
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8, (guchar*) str,
					strlen (str));
		g_free (str);
		break;
	}
	default:
	case TARGET_PLAIN: {
		gchar *str;
		str = g_strdup_printf ("%s.%s", tinfo->priv->schema, tinfo->priv->table_name);
		gtk_selection_data_set_text (selection_data, str, -1);
		g_free (str);
		break;
	}
	case TARGET_ROOTWIN:
		TO_IMPLEMENT; /* dropping on the Root Window => create a file */
		break;
	}
}

static GdaMetaDbObject *
get_table_meta_data (TableInfo *tinfo)
{
	gtk_widget_set_sensitive (GTK_WIDGET (tinfo), FALSE);
	GdaMetaStruct *mstruct;
	mstruct = t_connection_get_meta_struct (tinfo->priv->tcnc);
	if (!mstruct) {
		BrowserWindow *bwin;
		bwin = (BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo);
		gtk_widget_set_sensitive (GTK_WIDGET (tinfo), TRUE);
		ui_show_error (GTK_WINDOW (bwin), _("Meta data not yet available"));
		return NULL;
	}

	/* get table's information */
	GdaMetaDbObject *dbo;
	GValue *v1, *v2;
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)),
			    tinfo->priv->schema);
	g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)),
			    tinfo->priv->table_name);
	dbo = gda_meta_struct_complement (mstruct,
					  GDA_META_DB_TABLE, NULL, v1, v2, NULL);
	gda_value_free (v1);
	gda_value_free (v2);

	if (! dbo) {
		BrowserWindow *bwin;
		bwin = (BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo);
		gtk_widget_set_sensitive (GTK_WIDGET (tinfo), TRUE);
		ui_show_error (GTK_WINDOW (bwin), _("Can't find information about table"));
	}

	gtk_widget_set_sensitive (GTK_WIDGET (tinfo), TRUE);
	return dbo;
}

static void
meta_changed_cb (G_GNUC_UNUSED TConnection *tcnc, GdaMetaStruct *mstruct, TableInfo *tinfo)
{
	if (tinfo->priv->insert_columns_hash) {
		g_hash_table_destroy (tinfo->priv->insert_columns_hash);
		tinfo->priv->insert_columns_hash = NULL;
	}
	if (tinfo->priv->insert_popup) {
		gtk_widget_destroy (tinfo->priv->insert_popup);
		tinfo->priv->insert_popup = NULL;
	}

	if (tinfo->priv->table_short_name) {
		g_free (tinfo->priv->table_short_name);
		tinfo->priv->table_short_name = NULL;

		gtk_drag_source_unset ((GtkWidget *) tinfo->priv->header);
		g_signal_handlers_disconnect_by_func (tinfo->priv->header,
						      G_CALLBACK (source_drag_data_get_cb), tinfo);
	}

	GdaMetaDbObject *dbo;
	dbo = get_table_meta_data (tinfo);
	if (dbo) {
		tinfo->priv->table_short_name = g_strdup (dbo->obj_short_name);
		gtk_drag_source_set ((GtkWidget *) tinfo->priv->header, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
				     dbo_table, G_N_ELEMENTS (dbo_table), GDK_ACTION_COPY);
		gtk_drag_source_set_icon_pixbuf ((GtkWidget *) tinfo->priv->header,
						 ui_get_pixbuf_icon (UI_ICON_TABLE));
		g_signal_connect (tinfo->priv->header, "drag-data-get",
				  G_CALLBACK (source_drag_data_get_cb), tinfo);
	}

	display_table_not_found_error (tinfo, dbo ? FALSE : TRUE);
}

/**
 * table_info_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
table_info_new (TConnection *tcnc,
		const gchar *schema, const gchar *table)
{
	TableInfo *tinfo;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (schema, NULL);
	g_return_val_if_fail (table, NULL);

	tinfo = TABLE_INFO (g_object_new (TABLE_INFO_TYPE, NULL));

	tinfo->priv->tcnc = g_object_ref (tcnc);
	g_signal_connect (tinfo->priv->tcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), tinfo);
	tinfo->priv->schema = g_strdup (schema);
	tinfo->priv->table_name = g_strdup (table);

	GdaMetaDbObject *dbo;
	dbo = get_table_meta_data (tinfo);

	/* header */
        GtkWidget *label;
	gchar *str, *tmp;

	/* To translators: "Object" is used when we can't determine if @table is a table or a view */
	const gchar *otype = _("Object");
	if (dbo) {
		switch (dbo->obj_type) {
		case GDA_META_DB_TABLE:
			otype = _("Table");
			break;
		case GDA_META_DB_VIEW:
			otype = _("View");
			break;
		default:
			break;
		}
	}

	/* To translators: "In schema" refers to the database schema an object is in */
	tmp = g_strdup_printf (_("In schema '%s'"), schema);
	str = g_markup_printf_escaped ("%s <b>%s</b>\n%s", otype, table, tmp);
	g_free (tmp);
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (tinfo), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
	tinfo->priv->header = GDAUI_BAR (label);

	/* main contents */
	GtkWidget *top_nb;
	top_nb = gtk_notebook_new ();
	tinfo->priv->contents = top_nb;
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (top_nb), GTK_POS_BOTTOM);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (top_nb), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (top_nb), FALSE);
	gtk_box_pack_start (GTK_BOX (tinfo), top_nb, TRUE, TRUE, 0);	

	/* "table not found" error page */
	GtkWidget *image, *hbox;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), TRUE, TRUE, 0);
	image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 10);
	label = gtk_label_new (_("Table not found. If you think this is an error,\n"
				 "please refresh the meta data from the database\n"
				 "(menu Connection/Fetch meta data)."));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), TRUE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), hbox, NULL);
	tinfo->priv->unknown_table_notice = label;

	/* notebook for the pages */
	GtkWidget *sub_stack;
	sub_stack = gtk_stack_new ();
	gtk_stack_set_transition_type (GTK_STACK (sub_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
	GtkWidget *vbox;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sub_stack, TRUE, TRUE, 0);
	GtkWidget *ss;
	ss = gtk_stack_switcher_new ();
	gdaui_bar_add_widget (tinfo->priv->header, ss);
	gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (ss), GTK_STACK (sub_stack));
	gtk_widget_show (ss);
	gtk_notebook_append_page (GTK_NOTEBOOK (top_nb), vbox, NULL);

	/* append pages */
	GtkWidget *page;
	page = table_columns_new (tinfo);
	if (page) {
		gtk_widget_show (page);
		gtk_stack_add_titled (GTK_STACK (sub_stack), page, PAGE_COLUMNS, _("Columns"));
	}
#ifdef HAVE_GOOCANVAS
	page = table_relations_new (tinfo);
	if (page) {
		gtk_widget_show (page);
		gtk_stack_add_titled (GTK_STACK (sub_stack), page, PAGE_RELATIONS, _("Relations"));
	}
#endif
	page = table_preferences_new (tinfo);
	if (page) {
		gtk_widget_show (page);
		gtk_stack_add_titled (GTK_STACK (sub_stack), page, PAGE_PREFS, _("Preferences"));
	}
	gtk_stack_set_visible_child_name (GTK_STACK (sub_stack), PAGE_COLUMNS);

	/* show everything */
        gtk_widget_show_all (top_nb);
	display_table_not_found_error (tinfo, TRUE);
	
	GdaMetaStruct *mstruct;
	mstruct = t_connection_get_meta_struct (tinfo->priv->tcnc);
	if (mstruct)
		meta_changed_cb (tinfo->priv->tcnc, mstruct, tinfo);
	else
		display_table_not_found_error (tinfo, dbo ? FALSE : TRUE);

	return (GtkWidget*) tinfo;
}

/**
 * table_info_get_table_schema
 */
const gchar *
table_info_get_table_schema (TableInfo *tinfo)
{
	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);
	return tinfo->priv->schema;
}

/**
 * table_info_get_table_name
 */
const gchar *
table_info_get_table_name (TableInfo *tinfo)
{
	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);
	return tinfo->priv->table_name;
}

/**
 * table_info_get_connection
 */
TConnection *
table_info_get_connection (TableInfo *tinfo)
{
	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);
	return tinfo->priv->tcnc;
}

/*
 * UI actions
 */
static void
action_add_to_fav_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	TableInfo *tinfo;
	tinfo = TABLE_INFO (data);

	TFavorites *bfav;
        TFavoritesAttributes fav;
        GError *error = NULL;

        memset (&fav, 0, sizeof (TFavoritesAttributes));
        fav.id = -1;
        fav.type = T_FAVORITES_TABLES;
        fav.name = NULL;
        fav.descr = NULL;
        fav.contents = table_info_to_selection (tinfo);

        bfav = t_connection_get_favorites (tinfo->priv->tcnc);
        if (! t_favorites_add (bfav, 0, &fav, ORDER_KEY_SCHEMA, G_MAXINT, &error)) {
                ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo),
                                    _("Could not add favorite: %s"),
                                    error && error->message ? error->message : _("No detail"));
                if (error)
                        g_error_free (error);
        }
	g_free (fav.contents);
}

static void
action_view_contents_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	TableInfo *tinfo;
	tinfo = TABLE_INFO (data);
	if (! tinfo->priv->table_short_name)
		return;

	BrowserWindow *bwin;
	BrowserPerspective *pers;
	bwin = (BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo);
	pers = browser_window_change_perspective (bwin, DATA_MANAGER_PERSPECTIVE_NAME);

	xmlDocPtr doc;
	xmlNodePtr node, topnode;
	xmlChar *contents;
	int size;
	doc = xmlNewDoc (BAD_CAST "1.0");
	topnode = xmlNewDocNode (doc, NULL, BAD_CAST "data", NULL);
	xmlDocSetRootElement (doc, topnode);
	node = xmlNewChild (topnode, NULL, BAD_CAST "table", NULL);
	xmlSetProp (node, BAD_CAST "name", BAD_CAST tinfo->priv->table_short_name);
	xmlDocDumpFormatMemory (doc, &contents, &size, 1);
	xmlFreeDoc (doc);

	data_manager_perspective_new_tab (DATA_MANAGER_PERSPECTIVE (pers), (gchar*) contents);
	xmlFree (contents);
}

static void
insert_form_params_changed_cb (GdauiBasicForm *form, G_GNUC_UNUSED GdaHolder *param,
			       G_GNUC_UNUSED gboolean is_user_modif, GtkWidget *popup)
{
	/* if all params are valid => authorize the execute button */
	gtk_dialog_set_response_sensitive (GTK_DIALOG (popup), GTK_RESPONSE_ACCEPT,
					   gdaui_basic_form_is_valid (form));
}

static void
insert_response_cb (GtkWidget *dialog, gint response_id, TableInfo *tinfo)
{
	if (response_id == GTK_RESPONSE_ACCEPT) {
		GdaStatement *stmt;
		GdaSet *params;
		GError *lerror = NULL;
		
		stmt = g_object_get_data (G_OBJECT (dialog), "stmt");
		params = g_object_get_data (G_OBJECT (dialog), "params");

		GObject *result;
		result = t_connection_execute_statement (tinfo->priv->tcnc,
							       stmt, params,
							       GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, &lerror);
		if (result) {
			browser_window_show_notice_printf (BROWSER_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tinfo)),
							   GTK_MESSAGE_INFO,
							   "DataInsertQuery",
							   "%s", _("Data successfully inserted"));
			g_object_unref (result);
		}
		else {
			ui_show_error (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) tinfo)),
					    _("Error executing query: %s"),
					    lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
		}
		gtk_widget_hide (dialog);
	}
#ifdef HAVE_GDU
	else if (response_id == GTK_RESPONSE_HELP) {
		ui_show_help ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo),
				   "table-insert-data");
	}
#endif
	else
		gtk_widget_hide (dialog);
}

typedef struct {
	gint          cols_nb;
	gint         *fk_cols_array;
	GdaSet       *insert_params;
	GHashTable   *chash;
	GdaStatement *stmt;
	GdaDataModel *model;
	gboolean      model_rerunning;
} FKBindData;

/*
 * @result: (transfer full)
 */
static void
fk_bind_select_executed_cb (GObject *result, FKBindData *fkdata)
{
	gint i;
	GdaDataModel *model;
	if (! result)
		return;

	if (fkdata->model)
		g_object_unref (fkdata->model);

	model = GDA_DATA_MODEL (result);
	for (i = 0; i < fkdata->cols_nb; i++) {
		GdaHolder *h;
		GdaSetSource *source;
		h = g_hash_table_lookup (fkdata->chash,
					 GINT_TO_POINTER (fkdata->fk_cols_array [i] - 1));
		source = gda_set_get_source (fkdata->insert_params, h);
		if (source && gda_holder_get_source_model (h, NULL)) {
			gda_set_replace_source_model (fkdata->insert_params, source,
						      model);
			/* break now as gda_set_replace_source_model() does the job of replacing
			 * the data model for all the holders which share the same data model */
			break;
		}
		else {
			gboolean bound;
			bound = gda_holder_set_source_model (h, model, i, NULL);
#ifdef GDA_DEBUG_NO
			if (bound)
				g_print ("Bound holder [%s] to column %d for model %p\n", gda_holder_get_id (h), i, model);
			else
				g_print ("Could not bind holder [%s] to column %d\n", gda_holder_get_id (h), i);
#endif
			if (!bound) {
				/* There was an error => unbind all the parameters */
				for (i = 0; i < fkdata->cols_nb; i++) {
					h = g_hash_table_lookup (fkdata->chash,
								 GINT_TO_POINTER (fkdata->fk_cols_array [i] - 1));
					gda_holder_set_source_model (h, NULL, 0, NULL);
				}
				break;
			}
		}
	}
	fkdata->model = GDA_DATA_MODEL (result);
	fkdata->model_rerunning = FALSE;
}

static void
fkdata_list_free (GSList *fkdata_list)
{
	GSList *list;
	for (list = fkdata_list; list; list = list->next) {
		FKBindData *fkdata = (FKBindData*) list->data;
		g_free (fkdata->fk_cols_array);
		g_object_unref (fkdata->insert_params);
		g_object_unref (fkdata->stmt);
		if (fkdata->model)
			g_object_unref (fkdata->model);
		g_free (fkdata);
	}
	g_slist_free (fkdata_list);
}

static void
action_insert_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	TableInfo *tinfo;
	tinfo = TABLE_INFO (data);

	/* init */
	if (! tinfo->priv->table_short_name)
		return;

	if (tinfo->priv->insert_popup) {
		gtk_widget_show (tinfo->priv->insert_popup);
		GSList *fkdata_list;
		
		for (fkdata_list = g_object_get_data (G_OBJECT (tinfo->priv->insert_popup), "fkdata_list");
		     fkdata_list; fkdata_list = fkdata_list->next) {
			FKBindData *fkdata = (FKBindData *) fkdata_list->data;
			if (fkdata->model && !fkdata->model_rerunning) {
				GObject *result;
				fkdata->model_rerunning = TRUE;
				result = t_connection_execute_statement (tinfo->priv->tcnc,
									       fkdata->stmt, NULL,
									       GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL,
									       NULL);
				fk_bind_select_executed_cb (result, fkdata);
			}
		}
		return;
	}

	/* get table's information */
	GdaMetaDbObject *dbo;
	dbo = get_table_meta_data (tinfo);
	if (! dbo)
		return;

	/* build statement */
	GdaSqlBuilder *b;
	GSList *list;
	GdaMetaTable *mtable;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
	gda_sql_builder_set_table (b, tinfo->priv->table_short_name);
	mtable = GDA_META_TABLE (dbo);
	for (list = mtable->columns; list; list = list->next) {
		GdaMetaTableColumn *col = (GdaMetaTableColumn*) list->data;
		gda_sql_builder_add_field_value_id (b,
						    gda_sql_builder_add_id (b, col->column_name),
						    gda_sql_builder_add_param (b, col->column_name,
									       col->gtype, col->nullok));
	}
	GdaStatement *stmt;
	stmt = gda_sql_builder_get_statement (b, NULL);
	g_object_unref (b);
#ifdef GDA_DEBUG_NO
	gchar *sql;
	sql = gda_statement_to_sql (stmt, NULL, NULL);
	g_print ("[%s]\n", sql);
	g_free (sql);
#endif

	/* common data required further down */
	BrowserWindow *bwin;
	GdaMetaStruct *mstruct;
	bwin = (BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) tinfo);
	mstruct = t_connection_get_meta_struct (tinfo->priv->tcnc);
	if (!mstruct) {
		ui_show_error (GTK_WINDOW (bwin), _("Meta data not yet available"));
		return;
	}

	/* handle user preferences */
	GdaSet *params;
	if (! gda_statement_get_parameters (stmt, &params, NULL)) {
		gchar *sql;
		sql = gda_statement_to_sql (stmt, NULL, NULL);

		ui_show_error (GTK_WINDOW (bwin),
			       _("Internal error while building INSERT statement:\n%s"), sql);
		g_free (sql);
		g_object_unref (stmt);
		return;
	}
	GSList *fkdata_list = NULL;
	gint nthcol;
	if (mtable->fk_list)
		tinfo->priv->insert_columns_hash = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

	for (nthcol = 0, list = mtable->columns; list; nthcol++, list = list->next) {
		GdaMetaTableColumn *col = (GdaMetaTableColumn*) list->data;
		gchar *plugin;
		GdaHolder *holder;

		plugin = t_connection_get_table_column_attribute (tinfo->priv->tcnc,
									mtable,	col,
									T_CONNECTION_COLUMN_PLUGIN,
									NULL);
		holder = gda_set_get_holder (params, col->column_name);;
		if (!holder)
			continue;

		if (tinfo->priv->insert_columns_hash)
			g_hash_table_insert (tinfo->priv->insert_columns_hash, GINT_TO_POINTER (nthcol), g_object_ref (holder));
		
		if (!plugin && !col->default_value && !col->auto_incement)
			continue;
		if (plugin) {
			g_object_set ((GObject*) holder, "plugin", plugin, NULL);
		}
		
		if (col->default_value) {
			GValue *dv;
			//g_value_set_string ((dv = gda_value_new (G_TYPE_STRING)), col->default_value);
			dv = gda_value_new_null ();
			gda_holder_set_default_value (holder, dv);
			gda_value_free (dv);
			gda_holder_set_value_to_default (holder);

			gchar *tmp;
			tmp = g_strdup_printf (_("Default value: '%s'"), col->default_value);
			g_object_set (holder, "description", tmp, NULL);
			g_free (tmp);
		}
		else if (col->auto_incement) {
			GValue *dv;
			g_value_set_string ((dv = gda_value_new (G_TYPE_STRING)), "");
			gda_holder_set_default_value (holder, dv);
			gda_value_free (dv);
			gda_holder_set_value_to_default (holder);
			g_object_set (holder, "description", _("Default value: auto incremented value"), NULL);
		}

		g_free (plugin);
	}

	/* analyse FK list to propose values to choose from */
	for (list = mtable->fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
		if (fk->depend_on == dbo) /* don't link to itself */
			continue;
		else if (fk->depend_on->obj_type != GDA_META_DB_TABLE)
			continue;
		GdaMetaDbObject *rdbo = (GdaMetaDbObject*) fk->depend_on;
		GdaDataModel *cmodel;
		GValue *schema_v, *name_v, *catalog_v;
		GError *lerror = NULL;
		
		g_value_set_string ((catalog_v = gda_value_new (G_TYPE_STRING)), rdbo->obj_catalog);
		g_value_set_string ((schema_v = gda_value_new (G_TYPE_STRING)), rdbo->obj_schema);
		g_value_set_string ((name_v = gda_value_new (G_TYPE_STRING)), rdbo->obj_name);
		dbo = gda_meta_struct_get_db_object (mstruct, NULL, schema_v, name_v);

		cmodel = gda_meta_store_extract (t_connection_get_meta_store (tinfo->priv->tcnc),
						"SELECT tc.constraint_name, k.column_name FROM _key_column_usage k INNER JOIN _table_constraints tc ON (k.table_catalog=tc.table_catalog AND k.table_schema=tc.table_schema AND k.table_name=tc.table_name AND k.constraint_name=tc.constraint_name) WHERE tc.constraint_type='UNIQUE' AND k.table_catalog = ##catalog::string AND k.table_schema = ##schema::string AND k.table_name = ##tname::string ORDER by k.ordinal_position", &lerror,
						"catalog", catalog_v,
						"schema", schema_v,
						"tname", name_v, NULL);

		gda_value_free (catalog_v);
		gda_value_free (schema_v);
		gda_value_free (name_v);

		GdaSqlBuilder *b = NULL;
		if (cmodel) {
			gint nrows, i;
			nrows = gda_data_model_get_n_rows (cmodel);
			b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
			gda_sql_builder_select_add_target_id (b,
							      gda_sql_builder_add_id (b, rdbo->obj_short_name),
							      NULL);
			/* add REF PK fields */
			for (i = 0; i < fk->cols_nb; i++) {
				gda_sql_builder_select_add_field (b, fk->ref_pk_names_array [i], NULL, NULL);
			}

			/* add UNIQUE fields */
			for (i = 0; i < nrows; i++) {
				const GValue *cvalue;
				cvalue = gda_data_model_get_value_at (cmodel, 1, i, NULL);
				if (!cvalue || (G_VALUE_TYPE (cvalue) != G_TYPE_STRING))
					break;
				gda_sql_builder_select_add_field (b, g_value_get_string (cvalue), NULL, NULL);
				gda_sql_builder_select_order_by (b,
								 gda_sql_builder_add_id (b, g_value_get_string (cvalue)),
								 TRUE, NULL);
			}
			if (i < nrows) {
				/* error  */
				g_object_unref (b);
				b = NULL;
			}
			/*gda_data_model_dump (cmodel, NULL);*/
			g_object_unref (cmodel);
		}
		else {
			g_warning ("Can't get list of unique constraints for table '%s': %s",
				   rdbo->obj_short_name,
				   lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
		}

		if (b) {
			GdaStatement *stmt;
			stmt = gda_sql_builder_get_statement (b, NULL);
			if (stmt) {
#ifdef GDA_DEBUG_NO
				gchar *sql;
				sql = gda_statement_to_sql (stmt, NULL, NULL);
				g_print ("UNIQUE SELECT [%s]\n", sql);
				g_free (sql);
#endif

				FKBindData *fkdata;
				fkdata = g_new0 (FKBindData, 1);
				fkdata->cols_nb = fk->cols_nb;
				fkdata->fk_cols_array = g_new (gint, fk->cols_nb);
				memcpy (fkdata->fk_cols_array, fk->fk_cols_array, sizeof (gint) * fk->cols_nb);
				fkdata->chash = tinfo->priv->insert_columns_hash;
				fkdata->stmt = stmt;

				GObject *result;
				result = t_connection_execute_statement (tinfo->priv->tcnc, stmt, NULL,
									       GDA_STATEMENT_MODEL_RANDOM_ACCESS,
									       NULL, NULL);
				fkdata->insert_params = g_object_ref (params);

				/* attach the kfdata to @popup to be able to re-run the SELECT
				 * everytime the window is shown */
				fkdata_list = g_slist_prepend (fkdata_list, fkdata);

				fk_bind_select_executed_cb (result, fkdata);
			}

			g_object_unref (b);
		}
	}

	/* create popup */
	GtkWidget *popup;
	popup = gtk_dialog_new_with_buttons (_("Values to insert into table"), GTK_WINDOW (bwin),
					     0,
#ifdef HAVE_GDU
					     _("Help"), GTK_RESPONSE_HELP,
#endif
					     _("Execute"), GTK_RESPONSE_ACCEPT,
					     _("Cancel"), GTK_RESPONSE_REJECT,
					     NULL);
	tinfo->priv->insert_popup = popup;
	g_object_set_data_full (G_OBJECT (popup), "stmt", stmt, g_object_unref);
	g_object_set_data_full (G_OBJECT (popup), "params", params, g_object_unref);
	if (fkdata_list)
		g_object_set_data_full (G_OBJECT (popup), "fkdata_list",
					fkdata_list, (GDestroyNotify) fkdata_list_free);

	g_signal_connect (popup, "close",
			  G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (popup, "response",
			  G_CALLBACK (insert_response_cb), tinfo);

	GtkWidget *label, *form;
	gchar *str;	
	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>:\n<small>%s</small>",
			       _("Values to insert into table"),
			       _("assign values to the following variables"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (popup))),
			    label, FALSE, FALSE, 0);
	
	form = gdaui_basic_form_new (params);
	g_signal_connect (form, "holder-changed",
			  G_CALLBACK (insert_form_params_changed_cb), popup);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (popup))),
			    form, TRUE, TRUE, 5);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (popup), GTK_RESPONSE_ACCEPT,
					   gdaui_basic_form_is_valid (GDAUI_BASIC_FORM (form)));
		
	gtk_widget_show_all (popup);
}

static void
action_declarefk_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	TableInfo *tinfo;
	tinfo = TABLE_INFO (data);

	GtkWidget *dlg, *parent;
	GdaMetaStruct *mstruct;
	GdaMetaDbObject *dbo;
	gint response;

	parent = (GtkWidget*) gtk_widget_get_toplevel ((GtkWidget*) tinfo);
	mstruct = t_connection_get_meta_struct (tinfo->priv->tcnc);
	dbo = get_table_meta_data (tinfo);
	if (!dbo || (dbo->obj_type != GDA_META_DB_TABLE)) {
		ui_show_error ((GtkWindow *) parent, _("Can't find information about table '%s'"),
				    tinfo->priv->table_short_name);
		return;
	}

	dlg = fk_declare_new ((GtkWindow *) parent, mstruct, GDA_META_TABLE (dbo));
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_ACCEPT) {
		GError *error = NULL;
		if (! fk_declare_write (FK_DECLARE (dlg),
					BROWSER_IS_WINDOW (parent) ? BROWSER_WINDOW (parent) : NULL,
					&error)) {
			ui_show_error ((GtkWindow *) parent, _("Failed to declare foreign key: %s"),
					    error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
		else if (BROWSER_IS_WINDOW (parent))
			browser_window_show_notice (BROWSER_WINDOW (parent),
						    GTK_MESSAGE_INFO, "fkdeclare",
						    _("Successfully declared foreign key"));
		else
			ui_show_message ((GtkWindow *) parent, "%s",
					      _("Successfully declared foreign key"));
	}
	
	gtk_widget_destroy (dlg);
}

static GActionEntry win_entries[] = {
        { "AddToFav", action_add_to_fav_cb, NULL, NULL, NULL },
	{ "ViewContents", action_view_contents_cb, NULL, NULL, NULL },
	{ "InsertData", action_insert_cb, NULL, NULL, NULL },
	{ "KfDeclare", action_declarefk_cb, NULL, NULL, NULL }
};

static void
table_info_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	customization_data_init (G_OBJECT (page), toolbar, header);

	/* add perspective's actions */
	customization_data_add_actions (G_OBJECT (page), win_entries, G_N_ELEMENTS (win_entries));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "bookmark-new-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Add to Favorites"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.AddToFav");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "go-jump-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("View table's contents"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.ViewContents");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "document-new-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Insert data into table"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.InsertData");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "insert-link-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Declare Foreign Key"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.KfDeclare");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));
}

static GtkWidget *
table_info_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	TableInfo *tinfo;
	const gchar *tab_name;
	GdkPixbuf *table_pixbuf;

	tinfo = TABLE_INFO (page);
	table_pixbuf = ui_get_pixbuf_icon (UI_ICON_TABLE);
	tab_name = tinfo->priv->table_short_name ? tinfo->priv->table_short_name : tinfo->priv->table_name;
	return ui_make_tab_label_with_pixbuf (tab_name,
					      table_pixbuf,
					      out_close_button ? TRUE : FALSE, out_close_button);
}
