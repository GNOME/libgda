/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Daniel Espinosa <esodan@gmail.com>
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgda/gda-tree.h>
#include "table-info.h"
#include "table-columns.h"
#include <libgda-ui/gdaui-tree-store.h>
#include <t-utils.h>
#include "../ui-support.h"
#include "../gdaui-bar.h"
#include "mgr-columns.h"
#include "schema-browser-perspective.h"
#include "../browser-window.h"
#include "../fk-declare.h"
#ifdef HAVE_LDAP
#include "../ldap-browser/ldap-browser-perspective.h"
#endif
#include <libgda/gda-debug-macros.h>

struct _TableColumnsPrivate {
	TConnection *tcnc;
	TableInfo *tinfo;
	GdaTree *columns_tree;
	guint idle_update_columns;

	GtkTextBuffer *constraints;
	gboolean hovering_over_link;
#ifdef HAVE_LDAP
	GtkTextBuffer *ldap_def;
	GtkWidget *ldap_header;
	GtkWidget *ldap_text;
	gboolean ldap_props_shown;
#endif
};

static void table_columns_class_init (TableColumnsClass *klass);
static void table_columns_init       (TableColumns *tcolumns, TableColumnsClass *klass);
static void table_columns_dispose    (GObject *object);
static void table_columns_show_all   (GtkWidget *widget);

static void meta_changed_cb (TConnection *tcnc, GdaMetaStruct *mstruct, TableColumns *tcolumns);

static GObjectClass *parent_class = NULL;


/*
 * TableColumns class implementation
 */

static void
table_columns_class_init (TableColumnsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = table_columns_dispose;
	GTK_WIDGET_CLASS (klass)->show_all = table_columns_show_all;
}


static void
table_columns_init (TableColumns *tcolumns, G_GNUC_UNUSED TableColumnsClass *klass)
{
	tcolumns->priv = g_new0 (TableColumnsPrivate, 1);
	tcolumns->priv->idle_update_columns = 0;
	tcolumns->priv->hovering_over_link = FALSE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (tcolumns), GTK_ORIENTATION_VERTICAL);
}

static void
table_columns_dispose (GObject *object)
{
	TableColumns *tcolumns = (TableColumns *) object;

	/* free memory */
	if (tcolumns->priv) {
		if (tcolumns->priv->idle_update_columns)
			g_source_remove (tcolumns->priv->idle_update_columns);
		if (tcolumns->priv->columns_tree)
			g_object_unref (tcolumns->priv->columns_tree);
		if (tcolumns->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (tcolumns->priv->tcnc,
							      G_CALLBACK (meta_changed_cb), tcolumns);
			g_object_unref (tcolumns->priv->tcnc);
		}
		g_free (tcolumns->priv);
		tcolumns->priv = NULL;
	}

	parent_class->dispose (object);
}

static void
table_columns_show_all (GtkWidget *widget)
{
#ifdef HAVE_LDAP
	TableColumns *tcolumns = (TableColumns *) widget;
#endif
        GTK_WIDGET_CLASS (parent_class)->show_all (widget);
#ifdef HAVE_LDAP
	if (t_connection_is_ldap (tcolumns->priv->tcnc)) {
		if (! tcolumns->priv->ldap_props_shown) {
			gtk_widget_hide (tcolumns->priv->ldap_header);
			gtk_widget_hide (tcolumns->priv->ldap_text);
		}
	}
#endif
}

GType
table_columns_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo columns = {
			sizeof (TableColumnsClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) table_columns_class_init,
			NULL,
			NULL,
			sizeof (TableColumns),
			0,
			(GInstanceInitFunc) table_columns_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "TableColumns", &columns, 0);
	}
	return type;
}

static gboolean
idle_update_columns (TableColumns *tcolumns)
{
        gboolean done;
	GError *lerror = NULL;
        done = gda_tree_update_all (tcolumns->priv->columns_tree, &lerror);
	if (!done) {
		if (lerror &&
		    (lerror->domain == MGR_COLUMNS_ERROR) && 
		    ((lerror->code == MGR_COLUMNS_TABLE_NOT_FOUND) ||
		     (lerror->code == MGR_COLUMNS_WRONG_OBJ_TYPE)))
			done = TRUE;
	}
        if (done)
                tcolumns->priv->idle_update_columns = 0;
	else
		tcolumns->priv->idle_update_columns = g_timeout_add_seconds (1, (GSourceFunc) idle_update_columns,
									     tcolumns);
        return FALSE;
}

static gboolean key_press_event (GtkWidget *text_view, GdkEventKey *event, TableColumns *tcolumns);
static gboolean event_after (GtkWidget *text_view, GdkEvent *ev, TableColumns *tcolumns);
static gboolean motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, TableColumns *tcolumns);
static gboolean visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, TableColumns *tcolumns);
static GSList *build_reverse_depend_list (GdaMetaStruct *mstruct, GdaMetaTable *mtable);

static void
meta_changed_cb (G_GNUC_UNUSED TConnection *tcnc, GdaMetaStruct *mstruct, TableColumns *tcolumns)
{
	GtkTextBuffer *tbuffer;
	GtkTextIter start, end;

	/* cleanups */
#ifdef HAVE_LDAP
	if (t_connection_is_ldap (tcolumns->priv->tcnc)) {
		tbuffer = tcolumns->priv->ldap_def;
		gtk_text_buffer_get_start_iter (tbuffer, &start);
		gtk_text_buffer_get_end_iter (tbuffer, &end);
		gtk_text_buffer_delete (tbuffer, &start, &end);
	}
#endif
	tbuffer = tcolumns->priv->constraints;
	gtk_text_buffer_get_start_iter (tbuffer, &start);
        gtk_text_buffer_get_end_iter (tbuffer, &end);
        gtk_text_buffer_delete (tbuffer, &start, &end);

	/* update column's tree */
	if (! gda_tree_update_all (tcolumns->priv->columns_tree, NULL)) {
                if (tcolumns->priv->idle_update_columns == 0)
                        tcolumns->priv->idle_update_columns = g_idle_add ((GSourceFunc) idle_update_columns, tcolumns);
        }
	else {
		/* constraints descr. update */
		GdaMetaDbObject *dbo;
		GValue *schema_v = NULL, *name_v, *catalog_v = NULL;
		const gchar *str;
		
		str = table_info_get_table_schema (tcolumns->priv->tinfo);
		if (str)
			g_value_set_string ((schema_v = gda_value_new (G_TYPE_STRING)), str);
		str = table_info_get_table_name (tcolumns->priv->tinfo);
		g_value_set_string ((name_v = gda_value_new (G_TYPE_STRING)), str);
		dbo = gda_meta_struct_get_db_object (mstruct, NULL, schema_v, name_v);
		
		if (dbo) {
			GdaMetaTable *mtable = GDA_META_TABLE (dbo);
			GtkTextIter current;
			gtk_text_buffer_get_start_iter (tbuffer, &current);
			
			/* Primary key */
			if (mtable->pk_cols_nb > 0) {
				gint i;
				
				gtk_text_buffer_insert_with_tags_by_name (tbuffer,
									  &current, 
									  _("Primary key"), -1,
									  "section", NULL);
				gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
				for (i = 0; i < mtable->pk_cols_nb; i++) {
					GdaMetaTableColumn *col;
					if (i > 0)
						gtk_text_buffer_insert (tbuffer, &current, ", ", -1);
					col = g_slist_nth_data (mtable->columns, mtable->pk_cols_array [i]);
					gtk_text_buffer_insert (tbuffer, &current, 
								col ? col->column_name : "???", -1);
				}
				gtk_text_buffer_insert (tbuffer, &current, "\n\n", -1);
			}
			
			/* Foreign keys */
			GSList *list;
			for (list = mtable->fk_list; list; list = list->next) {
				GdaMetaTableForeignKey *fk = GDA_META_TABLE_FOREIGN_KEY (list->data);
				GdaMetaDbObject *fdbo = fk->depend_on;
				gint i;
				gchar *str;
				
				if (fdbo->obj_type == GDA_META_DB_UNKNOWN) {
					GValue *v1, *v2, *v3;
					g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)),
							    fdbo->obj_catalog);
					g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)),
							    fdbo->obj_schema);
					g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)),
							    fdbo->obj_name);
					gda_meta_struct_complement (mstruct, 
								    GDA_META_DB_TABLE, v1, v2, v3, NULL);
					gda_value_free (v1);
					gda_value_free (v2);
					gda_value_free (v3);
					continue;
				}
				if (GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (fk))
					str = g_strdup_printf (_("Declared foreign key '%s' on "),
							       fk->fk_name);
				else
					str = g_strdup_printf (_("Foreign key '%s' on "),
							       fk->fk_name);
				gtk_text_buffer_insert_with_tags_by_name (tbuffer,
									  &current, str, -1,
									  "section", NULL);
				g_free (str);
				if (fdbo->obj_type == GDA_META_DB_TABLE) {
					/* insert link to table name */
					GtkTextTag *tag;
					tag = gtk_text_buffer_create_tag (tbuffer, NULL, 
									  "foreground", "blue", 
									  "weight", PANGO_WEIGHT_BOLD,
									  "underline", PANGO_UNDERLINE_SINGLE, 
									  NULL);
					g_object_set_data_full (G_OBJECT (tag), "table_schema", 
								g_strdup (fdbo->obj_schema), g_free);
					g_object_set_data_full (G_OBJECT (tag), "table_name", 
								g_strdup (fdbo->obj_name), g_free);
					g_object_set_data_full (G_OBJECT (tag), "table_short_name", 
								g_strdup (fdbo->obj_short_name), g_free);
					gtk_text_buffer_insert_with_tags (tbuffer, &current,
									  fdbo->obj_short_name, -1, tag, NULL);
					if (GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (fk)) {
						tag = gtk_text_buffer_create_tag (tbuffer, NULL, 
										  "foreground", "blue", 
										  "underline",
										  PANGO_UNDERLINE_SINGLE, 
										  NULL);
						g_object_set_data_full (G_OBJECT (tag), "fk_name", 
									g_strdup (fk->fk_name),
									g_free);
						gtk_text_buffer_insert (tbuffer, &current, "\n(", -1);
						gtk_text_buffer_insert_with_tags (tbuffer, &current,
										  _("Remove"), -1, tag, NULL);
						gtk_text_buffer_insert (tbuffer, &current, ")", -1);
					}					
				}
				else
					gtk_text_buffer_insert_with_tags_by_name (tbuffer,
										  &current, 
										  fdbo->obj_short_name,
										  -1,
										  "section", NULL);

				for (i = 0; i < fk->cols_nb; i++) {
					gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
					gtk_text_buffer_insert (tbuffer, &current,
								fk->fk_names_array [i], -1);
					gtk_text_buffer_insert (tbuffer, &current, " ", -1);
					gtk_text_buffer_insert_pixbuf (tbuffer, &current,
								       ui_get_pixbuf_icon (UI_ICON_REFERENCE));

					gtk_text_buffer_insert (tbuffer, &current, " ", -1);
					gtk_text_buffer_insert (tbuffer, &current,
								fk->ref_pk_names_array [i], -1);

					if (fdbo->obj_type == GDA_META_DB_TABLE) {
						GdaMetaTableColumn *fkcol, *refcol;
						fkcol = g_slist_nth_data (mtable->columns,
									  fk->fk_cols_array[i] - 1);
						refcol = g_slist_nth_data (GDA_META_TABLE (fdbo)->columns,
									   fk->ref_pk_cols_array[i] - 1);
						if (fkcol && refcol &&
						    (fkcol->gtype != refcol->gtype)) {
							/* incompatible FK types */
							gchar *text;
							text = g_strdup_printf (_("incompatible types: '%s' for the foreign key and '%s' for the referenced primary key"),
										fkcol->column_type,
										refcol->column_type);
							gtk_text_buffer_insert (tbuffer, &current, " ", -1);
							gtk_text_buffer_insert_with_tags_by_name (tbuffer,
												  &current, 
												  text,
												  -1,
												  "warning", NULL);
							g_free (text);
						}
					}
				}

				GdaMetaForeignKeyPolicy policy;
				policy = GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY (fk);
				if (policy != GDA_META_FOREIGN_KEY_UNKNOWN) {
					gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
					/* To translators: the UPDATE is an SQL operation type */
					gtk_text_buffer_insert (tbuffer, &current, _("Policy on UPDATE"), -1);
					gtk_text_buffer_insert (tbuffer, &current, ": ", -1);
					gtk_text_buffer_insert (tbuffer, &current,
								t_utils_fk_policy_to_string (policy),
								-1);
				}
				policy = GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY (fk);
				if (policy != GDA_META_FOREIGN_KEY_UNKNOWN) {
					gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
					/* To translators: the DELETE is an SQL operation type */
					gtk_text_buffer_insert (tbuffer, &current, _("Policy on DELETE"), -1);
					gtk_text_buffer_insert (tbuffer, &current, ": ", -1);
					gtk_text_buffer_insert (tbuffer, &current,
								t_utils_fk_policy_to_string (policy),
								-1);
				}
				
				gtk_text_buffer_insert (tbuffer, &current, "\n\n", -1);
			}

			/* Unique constraints */
			GdaDataModel *model;
			GError *error = NULL;
			g_value_set_string ((catalog_v = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
			model = gda_meta_store_extract (t_connection_get_meta_store (tcolumns->priv->tcnc),
							"SELECT tc.constraint_name, k.column_name FROM _key_column_usage k INNER JOIN _table_constraints tc ON (k.table_catalog=tc.table_catalog AND k.table_schema=tc.table_schema AND k.table_name=tc.table_name AND k.constraint_name=tc.constraint_name) WHERE tc.constraint_type='UNIQUE' AND k.table_catalog = ##catalog::string AND k.table_schema = ##schema::string AND k.table_name = ##tname::string ORDER by k.ordinal_position", &error,
							"catalog", catalog_v,
							"schema", schema_v,
							"tname", name_v, NULL);
			if (!model) {
				g_warning (_("Could not compute table's UNIQUE constraints for %s.%s.%s"),
					   g_value_get_string (catalog_v), 
					   g_value_get_string (schema_v), 
					   g_value_get_string (name_v));
				g_error_free (error);
			}
			else {
				gint nrows;
				
				nrows = gda_data_model_get_n_rows (model);
				if (nrows > 0) {
					gint i;
					GValue *current_value = NULL;
					const GValue *cvalue;
					for (i = 0; i < nrows; i++) {
						cvalue = gda_data_model_get_value_at (model, 0, i, NULL);
						if (!current_value || 
						    gda_value_compare (cvalue, current_value)) {
							if (current_value)
								gtk_text_buffer_insert (tbuffer, &current,
											"\n\n", -1);
							gtk_text_buffer_insert_with_tags_by_name (tbuffer,
												  &current, 
												  _("Unique constraint"), -1,
												  "section", NULL);
							gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
							current_value = gda_value_copy (cvalue);
						}
						else
							gtk_text_buffer_insert (tbuffer, &current, ", ", -1);
						cvalue = gda_data_model_get_value_at (model, 1, i, NULL);
						gtk_text_buffer_insert (tbuffer, &current, 
									g_value_get_string (cvalue), -1);
					}
					gtk_text_buffer_insert (tbuffer, &current, "\n\n", -1);
				}
				g_object_unref (model);
			}

			/* reverse FK constraints */
			GSList *rev_list;
			rev_list = build_reverse_depend_list (mstruct, mtable);
			if (rev_list) {
				gtk_text_buffer_insert_with_tags_by_name (tbuffer,
									  &current, 
									  _("Tables referencing this one"), -1,
									  "section", NULL);
				for (list = rev_list; list; list = list->next) {
					GdaMetaDbObject *ddbo;
					ddbo = GDA_META_DB_OBJECT (list->data);
					if (ddbo->obj_type == GDA_META_DB_TABLE) {
						gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
						GtkTextTag *tag;
						tag = gtk_text_buffer_create_tag (tbuffer, NULL, 
										  "foreground", "blue", 
										  "weight", PANGO_WEIGHT_NORMAL,
										  "underline", PANGO_UNDERLINE_SINGLE, 
										  NULL);
						g_object_set_data_full (G_OBJECT (tag), "table_schema", 
									g_strdup (ddbo->obj_schema), g_free);
						g_object_set_data_full (G_OBJECT (tag), "table_name", 
									g_strdup (ddbo->obj_name), g_free);
						g_object_set_data_full (G_OBJECT (tag), "table_short_name", 
									g_strdup (ddbo->obj_short_name), g_free);
						gtk_text_buffer_insert_with_tags (tbuffer, &current,
										  ddbo->obj_short_name,
										  -1, tag, NULL);
					}
				}
				g_slist_free (rev_list);
				gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
			}

#ifdef HAVE_LDAP
			if (t_connection_is_ldap (tcolumns->priv->tcnc)) {
				const gchar *base_dn, *filter, *attributes, *scope_str;
				GdaLdapSearchScope scope;
				tbuffer = tcolumns->priv->ldap_def;
				gtk_text_buffer_get_start_iter (tbuffer, &current);
				if (t_connection_describe_table  (tcolumns->priv->tcnc, dbo->obj_name,
									&base_dn, &filter,
									&attributes, &scope, NULL)) {
					gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
										  "BASE: ", -1,
										  "section", NULL);
					if (base_dn) {
						GtkTextTag *tag;
						tag = gtk_text_buffer_create_tag (tbuffer, NULL, 
										  "foreground", "blue", 
										  "weight", PANGO_WEIGHT_NORMAL,
										  "underline", PANGO_UNDERLINE_SINGLE,
										  NULL);
						g_object_set_data_full (G_OBJECT (tag), "dn",
									g_strdup (base_dn), g_free);
						
						gtk_text_buffer_insert_with_tags (tbuffer, &current, base_dn, -1,
										  tag, NULL);
					}
					
					gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
					gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
										  "FILTER: ", -1,
										  "section", NULL);
					if (filter)
						gtk_text_buffer_insert (tbuffer, &current, filter, -1);
					
					gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
					gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
										  "ATTRIBUTES: ", -1,
										  "section", NULL);
					if (attributes)
						gtk_text_buffer_insert (tbuffer, &current, attributes, -1);
					
					gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
					gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
										  "SCOPE: ", -1,
										  "section", NULL);
					
					switch (scope) {
					case GDA_LDAP_SEARCH_BASE:
						scope_str = "base";
						break;
					case GDA_LDAP_SEARCH_ONELEVEL:
						scope_str = "onelevel";
						break;
					case GDA_LDAP_SEARCH_SUBTREE:
						scope_str = "subtree";
						break;
					default:
						TO_IMPLEMENT;
						scope_str = _("Unknown");
						break;
					}
					gtk_text_buffer_insert (tbuffer, &current, scope_str, -1);
					
					tcolumns->priv->ldap_props_shown = TRUE;
					gtk_widget_show (tcolumns->priv->ldap_header);
					gtk_widget_show (tcolumns->priv->ldap_text);
				}
				else {
					tcolumns->priv->ldap_props_shown = FALSE;
					gtk_widget_hide (tcolumns->priv->ldap_header);
					gtk_widget_hide (tcolumns->priv->ldap_text);
				}
			}
#endif

		}

		if (schema_v)
			gda_value_free (schema_v);
		if (catalog_v)
			gda_value_free (catalog_v);
		gda_value_free (name_v);
	}
}

/*
 * builds a list of #GdaMetaDbObject where dbo->depend_list includes @mtable in @mstruct
 * Returns: (transfer container): a new list
 */
static GSList *
build_reverse_depend_list (GdaMetaStruct *mstruct, GdaMetaTable *mtable)
{
	GSList *retlist = NULL;
	GSList *list, *alldbo;
	alldbo = gda_meta_struct_get_all_db_objects (mstruct);
	for (list = alldbo; list; list = list->next) {
		if (g_slist_find (GDA_META_DB_OBJECT (list->data)->depend_list, mtable))
			retlist = g_slist_prepend (retlist, list->data);
	}
	g_slist_free (alldbo);
	return retlist;
}

/**
 * table_columns_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
table_columns_new (TableInfo *tinfo)
{
	TableColumns *tcolumns;
	GdaTreeManager *manager;

	enum {
		COLUMN_NAME,
		COLUMN_TYPE,
		COLUMN_NOTNULL,
		COLUMN_DEFAULT,
		COLUMN_ICON,
		COLUMN_DETAILS
	};

	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);

	tcolumns = TABLE_COLUMNS (g_object_new (TABLE_COLUMNS_TYPE, NULL));

	tcolumns->priv->tinfo = tinfo;
	tcolumns->priv->tcnc = g_object_ref (table_info_get_connection (tinfo));
	g_signal_connect (tcolumns->priv->tcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), tcolumns);
	
	/* main container */
	GtkWidget *paned;
	paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (tcolumns), paned, TRUE, TRUE, 0);
	gtk_widget_show (paned);

	/*
	 * Columns
	 */
	tcolumns->priv->columns_tree = gda_tree_new ();
	manager = mgr_columns_new (tcolumns->priv->tcnc, table_info_get_table_schema (tinfo), 
				   table_info_get_table_name (tinfo));
        gda_tree_add_manager (tcolumns->priv->columns_tree, manager);
        g_object_unref (manager);

	/* update the tree's contents */
        if (! gda_tree_update_all (tcolumns->priv->columns_tree, NULL)) {
                if (tcolumns->priv->idle_update_columns == 0)
                        tcolumns->priv->idle_update_columns = g_idle_add ((GSourceFunc) idle_update_columns, tcolumns);
        }

	/* tree model */
        GtkTreeModel *model;
        GtkWidget *treeview;
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        model = gdaui_tree_store_new (tcolumns->priv->columns_tree, 6,
                                      G_TYPE_STRING, MGR_COLUMNS_COL_NAME_ATT_NAME,
				      G_TYPE_STRING, MGR_COLUMNS_COL_TYPE_ATT_NAME,
				      G_TYPE_BOOLEAN, MGR_COLUMNS_COL_NOTNULL_ATT_NAME,
				      G_TYPE_STRING, MGR_COLUMNS_COL_DEFAULT_ATT_NAME,
                                      G_TYPE_OBJECT, "icon",
				      G_TYPE_STRING, MGR_COLUMNS_COL_DETAILS);
        treeview = ui_make_tree_view (model);
        g_object_unref (model);

        /* Colum: Name */
        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("Column Name"));

        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COLUMN_ICON);

        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, renderer, TRUE);
        gtk_tree_view_column_add_attribute (column, renderer, "markup", COLUMN_NAME);

        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	/* Colum: Type */
	renderer = gtk_cell_renderer_text_new ();
	/* To translators: "Type" is the data type of a table's column */
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
							   "text", COLUMN_TYPE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	/* Colum: Nullok */
	renderer = gtk_cell_renderer_toggle_new ();
	/* To translators: "Not NULL?" is a table's column's attribute. The NULL term should not be translated as it refers to the SQL NULL value */
	column = gtk_tree_view_column_new_with_attributes (_("Not NULL?"), renderer,
							   "active", COLUMN_NOTNULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	/* Colum: Default */
	renderer = gtk_cell_renderer_text_new ();
	/* To translators: "Default value" is a table's column's attribute */
	column = gtk_tree_view_column_new_with_attributes (_("Default value"), renderer,
							   "text", COLUMN_DEFAULT, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	/* Colum: Details */
	renderer = gtk_cell_renderer_text_new ();
	/* To translators: "Details" is a table's column's attribute */
	column = gtk_tree_view_column_new_with_attributes (_("Details"), renderer,
							   "text", COLUMN_DETAILS, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	/* scrolled window packing */
        GtkWidget *sw;
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (sw), treeview);
	gtk_paned_pack1 (GTK_PANED (paned), sw, TRUE, FALSE);
	
	/* Paned, part 2 */
	GtkWidget *vbox;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (paned), vbox, TRUE, TRUE);

#ifdef HAVE_LDAP
	if (t_connection_is_ldap (tcolumns->priv->tcnc)) {
		GtkWidget *label;
		gchar *str;
		
		str = g_strdup_printf ("<b>%s</b>", _("LDAP virtual table definition"));
		label = gdaui_bar_new (str);
		g_free (str);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		tcolumns->priv->ldap_header = label;

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
		tcolumns->priv->ldap_text = sw;

		GtkWidget *textview;
		textview = gtk_text_view_new ();
		gtk_container_add (GTK_CONTAINER (sw), textview);
		gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 5);
		gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 5);
		gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
		tcolumns->priv->ldap_def = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
		gtk_text_buffer_set_text (tcolumns->priv->ldap_def, "aa", -1);

		gtk_text_buffer_create_tag (tcolumns->priv->ldap_def, "section",
					    "weight", PANGO_WEIGHT_BOLD,
					    "foreground", "blue", NULL);
		
		gtk_text_buffer_create_tag (tcolumns->priv->ldap_def, "warning",
					    "foreground", "red", NULL);

		g_signal_connect (textview, "key-press-event", 
				  G_CALLBACK (key_press_event), tcolumns);
		g_signal_connect (textview, "event-after", 
				  G_CALLBACK (event_after), tcolumns);
		g_signal_connect (textview, "motion-notify-event", 
				  G_CALLBACK (motion_notify_event), tcolumns);
		g_signal_connect (textview, "visibility-notify-event", 
				  G_CALLBACK (visibility_notify_event), tcolumns);
	}
#endif

	/*
	 * Constraints
	 */
	GtkWidget *label;
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>", _("Constraints and integrity rules"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

	GtkWidget *textview;
	textview = gtk_text_view_new ();
        gtk_container_add (GTK_CONTAINER (sw), textview);
        gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 5);
        gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 5);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
        tcolumns->priv->constraints = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
        gtk_text_buffer_set_text (tcolumns->priv->constraints, "aa", -1);
        gtk_widget_show_all (vbox);

        gtk_text_buffer_create_tag (tcolumns->priv->constraints, "section",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);

        gtk_text_buffer_create_tag (tcolumns->priv->constraints, "warning",
                                    "foreground", "red", NULL);

	g_signal_connect (textview, "key-press-event", 
			  G_CALLBACK (key_press_event), tcolumns);
	g_signal_connect (textview, "event-after", 
			  G_CALLBACK (event_after), tcolumns);
	g_signal_connect (textview, "motion-notify-event", 
			  G_CALLBACK (motion_notify_event), tcolumns);
	g_signal_connect (textview, "visibility-notify-event", 
			  G_CALLBACK (visibility_notify_event), tcolumns);

	gtk_widget_show_all (vbox);

	/*
	 * initial update
	 */
	GdaMetaStruct *mstruct;
	mstruct = t_connection_get_meta_struct (tcolumns->priv->tcnc);
	if (mstruct)
		meta_changed_cb (tcolumns->priv->tcnc, mstruct, tcolumns);

	return (GtkWidget*) tcolumns;
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;

/* Looks at all tags covering the position (x, y) in the text view, 
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView *text_view, gint x, gint y, TableColumns *tcolumns)
{
	GSList *tags = NULL, *tagp = NULL;
	GtkTextIter iter;
	gboolean hovering = FALSE;
	
	gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
	
	tags = gtk_text_iter_get_tags (&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;

		if (g_object_get_data (G_OBJECT (tag), "table_name") ||
		    g_object_get_data (G_OBJECT (tag), "fk_name") ||
		    g_object_get_data (G_OBJECT (tag), "dn")) {
			hovering = TRUE;
			break;
		}
	}
	
	if (hovering != tcolumns->priv->hovering_over_link) {
		tcolumns->priv->hovering_over_link = hovering;
		
		if (tcolumns->priv->hovering_over_link) {
			if (! hand_cursor)
				hand_cursor = gdk_cursor_new_for_display (
                        gtk_widget_get_display (GTK_WIDGET(text_view)),
                        GDK_HAND2);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       hand_cursor);
		}
		else {
			if (!regular_cursor)
				regular_cursor = gdk_cursor_new_for_display (
                        gtk_widget_get_display (GTK_WIDGET(text_view)),
                        GDK_XTERM);
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
			 TableColumns *tcolumns)
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
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), bx, by, tcolumns);
	
	return FALSE;
}

/*
 * Update the cursor image if the pointer moved. 
 */
static gboolean
motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, TableColumns *tcolumns)
{
	gint x, y;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), x, y, tcolumns);

	return FALSE;
}

/* Looks at all tags covering the position of iter in the text view, 
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (G_GNUC_UNUSED GtkWidget *text_view, GtkTextIter *iter, TableColumns *tcolumns)
{
	GSList *tags = NULL, *tagp = NULL;
	
	tags = gtk_text_iter_get_tags (iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		const gchar *table_name;
		const gchar *table_schema;
		const gchar *table_short_name;
		const gchar *fk_name;
#ifdef HAVE_LDAP
		const gchar *dn;
#endif
		SchemaBrowserPerspective *bpers;
		
		table_schema = g_object_get_data (G_OBJECT (tag), "table_schema");
		table_name = g_object_get_data (G_OBJECT (tag), "table_name");
		table_short_name = g_object_get_data (G_OBJECT (tag), "table_short_name");
		fk_name = g_object_get_data (G_OBJECT (tag), "fk_name");
#ifdef HAVE_LDAP
		dn = g_object_get_data (G_OBJECT (tag), "dn");
#endif

		bpers = SCHEMA_BROWSER_PERSPECTIVE (ui_find_parent_widget (GTK_WIDGET (tcolumns),
						    TYPE_SCHEMA_BROWSER_PERSPECTIVE));
		if (table_name && table_schema && table_short_name && bpers) {
			schema_browser_perspective_display_table_info (bpers,
								       table_schema,
								       table_name,
								       table_short_name);
		}
		else if (fk_name) {
			GdaMetaStruct *mstruct;
			GdaMetaDbObject *dbo;
			GValue *v1, *v2;
			GtkWidget *parent;
			table_schema = table_info_get_table_schema (tcolumns->priv->tinfo);
			table_name = table_info_get_table_name (tcolumns->priv->tinfo);
			g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), table_schema);
			g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), table_name);
			mstruct = t_connection_get_meta_struct (tcolumns->priv->tcnc);
			dbo = gda_meta_struct_get_db_object (mstruct, NULL, v1, v2);
			gda_value_free (v1);
			gda_value_free (v2);
			parent = gtk_widget_get_toplevel (GTK_WIDGET (tcolumns));
			if (!dbo || (dbo->obj_type != GDA_META_DB_TABLE)) {
				ui_show_error ((GtkWindow*) parent,
						    _("Could not find table '%s.%s'"),
						    table_schema, table_name);
			}
			else {
				GdaMetaTableForeignKey *fk = NULL;
				GdaMetaTable *mtable;
				GSList *list;
				mtable = GDA_META_TABLE (dbo);
				for (list = mtable->fk_list; list; list = list->next) {
					GdaMetaTableForeignKey *tfk;
					tfk = (GdaMetaTableForeignKey*) list->data;
					if (tfk->fk_name && !strcmp (tfk->fk_name, fk_name)) {
						fk = tfk;
						break;
					}
				}
				if (fk) {
					GError *error = NULL;
					if (! fk_declare_undeclare (mstruct,
								    BROWSER_IS_WINDOW (parent) ? BROWSER_WINDOW (parent) : NULL,
								    fk, &error)) {
						ui_show_error ((GtkWindow *) parent, _("Failed to undeclare foreign key: %s"),
								    error && error->message ? error->message : _("No detail"));
						g_clear_error (&error);
					}
					else if (BROWSER_IS_WINDOW (parent))
						browser_window_show_notice (BROWSER_WINDOW (parent),
									    GTK_MESSAGE_INFO, "fkdeclare",
									    _("Successfully undeclared foreign key"));
					else
						ui_show_message ((GtkWindow *) parent, "%s",
								      _("Successfully undeclared foreign key"));
				}
				else
					ui_show_error ((GtkWindow*) parent,
							    _("Could not find declared foreign key '%s'"),
							    fk_name);
			}
		}
#ifdef HAVE_LDAP
		else if (dn) {
			BrowserWindow *bwin;
			BrowserPerspective *pers;

			bwin = (BrowserWindow*) gtk_widget_get_toplevel ((GtkWidget*) tcolumns);
			pers = browser_window_change_perspective (bwin, _("LDAP browser"));
			
			ldap_browser_perspective_display_ldap_entry (LDAP_BROWSER_PERSPECTIVE (pers), dn);
		}
#endif
        }

	if (tags) 
		g_slist_free (tags);
}


/*
 * Links can also be activated by clicking.
 */
static gboolean
event_after (GtkWidget *text_view, GdkEvent *ev, TableColumns *tcolumns)
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
	
	follow_if_link (text_view, &iter, tcolumns);
	
	return FALSE;
}

/* 
 * Links can be activated by pressing Enter.
 */
static gboolean
key_press_event (GtkWidget *text_view, GdkEventKey *event, TableColumns *tcolumns)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	
	switch (event->keyval) {
	case GDK_KEY_Return: 
	case GDK_KEY_KP_Enter:
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, 
						  gtk_text_buffer_get_insert (buffer));
		follow_if_link (text_view, &iter, tcolumns);
		break;
		
	default:
		break;
	}
	return FALSE;
}
