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

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "browser-canvas-priv.h"
#include "browser-canvas-db-relations.h"
#include "browser-canvas-table.h"
#include "browser-canvas-column.h"
#include "browser-canvas-fkey.h"
#include "../objects-cloud.h"
#include "../fk-declare.h"
#include <libgda-ui/internal/popup-container.h>
#include "../browser-window.h"
#include "../ui-support.h"

static void browser_canvas_db_relations_class_init (BrowserCanvasDbRelationsClass *class);
static void browser_canvas_db_relations_init       (BrowserCanvasDbRelations *canvas);
static void browser_canvas_db_relations_dispose   (GObject *object);

static void browser_canvas_db_relations_set_property (GObject *object,
						      guint param_id,
						      const GValue *value,
						      GParamSpec *pspec);
static void browser_canvas_db_relations_get_property (GObject *object,
						      guint param_id,
						      GValue *value,
						      GParamSpec *pspec);

/* virtual functions */
static void       clean_canvas_items  (BrowserCanvas *canvas);
static GtkWidget *build_context_menu  (BrowserCanvas *canvas);
static GSList    *get_layout_items    (BrowserCanvas *canvas);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

enum
{
        PROP_0,
	PROP_META_STRUCT
};

struct _BrowserCanvasDbRelationsPrivate
{
	GHashTable       *hash_tables; /* key = GdaMetaTable, value = BrowserCanvasMetaTable (and the reverse) */
	GHashTable       *hash_fkeys; /* key = GdaMetaTableForeignKey, value = BrowserCanvasFkey */

	GdaMetaStruct    *mstruct;
	GooCanvasItem    *level_separator; /* all tables items will be above this item and FK lines below */

	GtkWidget        *add_dialog;
	ObjectsCloud     *cloud;
};

GType
browser_canvas_db_relations_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasDbRelationsClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_db_relations_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvasDbRelations),
			0,
			(GInstanceInitFunc) browser_canvas_db_relations_init,
			0
		};		

		type = g_type_register_static (TYPE_BROWSER_CANVAS, "BrowserCanvasDbRelations", &info, 0);
	}
	return type;
}

static void
browser_canvas_db_relations_init (BrowserCanvasDbRelations * canvas)
{
	canvas->priv = g_new0 (BrowserCanvasDbRelationsPrivate, 1);
	canvas->priv->hash_tables = g_hash_table_new (NULL, NULL);
	canvas->priv->hash_fkeys = g_hash_table_new (NULL, NULL);
	canvas->priv->mstruct = NULL;
}

static void
browser_canvas_db_relations_class_init (BrowserCanvasDbRelationsClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	/* BrowserCanvas virtual functions */
	BROWSER_CANVAS_CLASS (class)->clean_canvas_items = clean_canvas_items;
	BROWSER_CANVAS_CLASS (class)->build_context_menu = build_context_menu;
	BROWSER_CANVAS_CLASS (class)->get_layout_items = get_layout_items;
	object_class->dispose = browser_canvas_db_relations_dispose;

	/* properties */
	object_class->set_property = browser_canvas_db_relations_set_property;
        object_class->get_property = browser_canvas_db_relations_get_property;
	g_object_class_install_property (object_class, PROP_META_STRUCT,
                                         g_param_spec_object ("meta-struct", "GdaMetaStruct", NULL,
							      GDA_TYPE_META_STRUCT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
browser_canvas_db_relations_dispose (GObject *object)
{
	BrowserCanvasDbRelations *canvas;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_DB_RELATIONS (object));

	canvas = BROWSER_CANVAS_DB_RELATIONS (object);

	if (canvas->priv) {
		clean_canvas_items (BROWSER_CANVAS (canvas));
		if (canvas->priv->mstruct)
			g_object_unref (canvas->priv->mstruct);

		g_hash_table_destroy (canvas->priv->hash_tables);
		g_hash_table_destroy (canvas->priv->hash_fkeys);

		if (canvas->priv->add_dialog)
			gtk_widget_destroy (canvas->priv->add_dialog);

		g_free (canvas->priv);
		canvas->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

typedef struct {
	GdaMetaTable   *table;
	GooCanvasBounds bounds;
} PresentTable;

static void
browser_canvas_db_relations_set_property (GObject *object,
					  guint param_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	BrowserCanvasDbRelations *canvas;
	BrowserCanvas *b_canvas;

        canvas = BROWSER_CANVAS_DB_RELATIONS (object);
	b_canvas = BROWSER_CANVAS (object);
        if (canvas->priv) {
                switch (param_id) {
		case PROP_META_STRUCT: {
			GSList *present_tables = NULL;
			GdaMetaStruct *mstruct = g_value_get_object (value);
			GdaMetaStruct *old_mstruct;
			if (canvas->priv->mstruct == mstruct)
				break;
			if (mstruct)
				g_object_ref (mstruct);
			
			if (canvas->priv->mstruct) {
				GSList *list;
				for (list = b_canvas->priv->items; list; list = list->next) {
					BrowserCanvasItem *item;
					GdaMetaTable *mtable;
					item = BROWSER_CANVAS_ITEM (list->data);
					mtable = g_hash_table_lookup (canvas->priv->hash_tables,
								      item);
					if (! mtable)
						continue;
					
					PresentTable *pt;
					pt = g_new (PresentTable, 1);
					present_tables = g_slist_prepend (present_tables, pt);
					pt->table = mtable;
					goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (item),
								    &(pt->bounds));
				}
			}
			old_mstruct = canvas->priv->mstruct;
			clean_canvas_items (BROWSER_CANVAS (canvas));
			canvas->priv->mstruct = mstruct;
			if (present_tables) {
				GSList *list;
				for (list = present_tables; list; list = list->next) {
					PresentTable *pt = (PresentTable*) list->data;
					GdaMetaDbObject *dbo = (GdaMetaDbObject*) pt->table;
					GValue *v1 = NULL, *v2 = NULL, *v3 = NULL;
					BrowserCanvasTable *ctable;
					GooCanvasBounds bounds;
										
					if (dbo->obj_catalog)
						g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)),
								    dbo->obj_catalog);
					if (dbo->obj_schema)
						g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)),
								    dbo->obj_schema);
					if (dbo->obj_name)
						g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)),
								    dbo->obj_name);
					
					ctable = browser_canvas_db_relations_add_table (canvas, v1, v2,
											v3);
					if (v1) gda_value_free (v1);
					if (v3) gda_value_free (v2);
					if (v2) gda_value_free (v3);
					
					if (ctable) {
						goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ctable),
									    &bounds);
						browser_canvas_translate_item (BROWSER_CANVAS (canvas),
									       (BrowserCanvasItem*) ctable,
									       pt->bounds.x1 - bounds.x1,
									       pt->bounds.y1 - bounds.y1);
					}
					g_free (pt);
				}
				g_slist_free (present_tables);
				g_object_set (G_OBJECT (b_canvas->priv->goocanvas),
					      "automatic-bounds", TRUE, NULL);
			}
			if (old_mstruct)
				g_object_unref (old_mstruct);

			if (canvas->priv->cloud)
				objects_cloud_set_meta_struct (canvas->priv->cloud,
							       canvas->priv->mstruct);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
browser_canvas_db_relations_get_property (GObject *object,
					  guint param_id,
					  GValue *value,
					  GParamSpec *pspec)
{
	BrowserCanvasDbRelations *canvas;

        canvas = BROWSER_CANVAS_DB_RELATIONS (object);
        if (canvas->priv) {
                switch (param_id) {
		case PROP_META_STRUCT:
			g_value_set_object (value, canvas->priv->mstruct);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
cloud_object_selected_cb (G_GNUC_UNUSED ObjectsCloud *ocloud, G_GNUC_UNUSED ObjectsCloudObjType sel_type,
			  const gchar *sel_contents, BrowserCanvasDbRelations *dbrel)
{
	GdaMetaTable *mtable;
	GValue *table_schema;
	GValue *table_name;
	GdaQuarkList *ql;

	ql = gda_quark_list_new_from_string (sel_contents);
	g_value_set_string ((table_schema = gda_value_new (G_TYPE_STRING)),
			    gda_quark_list_find (ql, "OBJ_SCHEMA"));
	g_value_set_string ((table_name = gda_value_new (G_TYPE_STRING)),
			    gda_quark_list_find (ql, "OBJ_NAME"));
	gda_quark_list_free (ql);

#ifdef GDA_DEBUG_NO
	g_print ("Add %s.%s\n",
		 g_value_get_string (table_schema), g_value_get_string (table_name));
#endif
	mtable = (GdaMetaTable*) gda_meta_struct_complement (dbrel->priv->mstruct, GDA_META_DB_TABLE,
							     NULL, table_schema, table_name, NULL);
	if (mtable) {
		BrowserCanvasTable *ctable;
		GooCanvasBounds bounds;
		gdouble x, y;
		
		x = BROWSER_CANVAS (dbrel)->xmouse;
		y = BROWSER_CANVAS (dbrel)->ymouse;

		ctable = browser_canvas_db_relations_add_table (dbrel, NULL, table_schema, table_name);
		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ctable), &bounds);
		browser_canvas_item_translate (BROWSER_CANVAS_ITEM (ctable),
					       x - bounds.x1, y - bounds.y1);
	}
	gda_value_free (table_schema);
	gda_value_free (table_name);
}

/**
 * browser_canvas_db_relations_new
 * @mstruct: (nullable): a #GdaMetaStruct object, or %NULL
 *
 * Creates a new canvas widget to display the relations between the database's tables.
 *
 * After the #BrowserCanvasDbRelations has been created, it is possible to display tables
 * using browser_canvas_db_relations_add_table().
 *
 * Returns: a new #GtkWidget widget
 */
GtkWidget *
browser_canvas_db_relations_new (GdaMetaStruct *mstruct)
{
	BrowserCanvas *canvas;
	BrowserCanvasDbRelations *dbrels;
	GooCanvasItem *item;
	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), NULL);

	canvas = BROWSER_CANVAS (g_object_new (TYPE_BROWSER_CANVAS_DB_RELATIONS,
					       "meta-struct", mstruct, NULL));
	dbrels = BROWSER_CANVAS_DB_RELATIONS (canvas);
	item = goo_canvas_group_new (goo_canvas_get_root_item (canvas->priv->goocanvas), NULL);
	dbrels->priv->level_separator = item;

        return GTK_WIDGET (canvas);
}

static void
clean_canvas_items (BrowserCanvas *canvas)
{
	BrowserCanvasDbRelations *dbrel = BROWSER_CANVAS_DB_RELATIONS (canvas);
	GSList *clist, *list;

	/* remove canvas item */
	clist = g_slist_copy (canvas->priv->items);
	for (list = clist; list; list = list->next)
		goo_canvas_item_remove (GOO_CANVAS_ITEM (list->data));
	g_slist_free (clist);

	/* clean memory */
	g_hash_table_destroy (dbrel->priv->hash_tables);
	g_hash_table_destroy (dbrel->priv->hash_fkeys);
	dbrel->priv->hash_tables = g_hash_table_new (NULL, NULL);
	dbrel->priv->hash_fkeys = g_hash_table_new (NULL, NULL);
}

static GtkWidget *canvas_entity_popup_func (BrowserCanvasTable *ce);

static void popup_func_delete_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce);
static void popup_func_add_depend_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce);
static void popup_func_add_ref_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce);
static void popup_func_declare_fk_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce);

static GtkWidget *
canvas_entity_popup_func (BrowserCanvasTable *ce)
{
	GtkWidget *menu, *entry;

	menu = gtk_menu_new ();
	entry = gtk_menu_item_new_with_label (_("Remove from graph"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_delete_cb), ce);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
	gtk_widget_show (entry);
	entry = gtk_menu_item_new_with_label (_("Add referenced tables to graph"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_add_depend_cb), ce);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
	gtk_widget_show (entry);
	entry = gtk_menu_item_new_with_label (_("Add tables referencing this table to graph"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_add_ref_cb), ce);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
	gtk_widget_show (entry);

	entry = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
	gtk_widget_show (entry);

	entry = gtk_menu_item_new_with_label (_("Declare foreign key for this table"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_declare_fk_cb), ce);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
	gtk_widget_show (entry);

	return menu;
}

static void
popup_func_declare_fk_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvasTable *ce)
{
	GtkWidget *dlg, *parent;
	GdaMetaStruct *mstruct;
	GdaMetaTable *mtable;
	gint response;

	parent = (GtkWidget*) gtk_widget_get_toplevel ((GtkWidget*) goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (ce)));
	g_object_get (G_OBJECT (ce), "meta-struct", &mstruct, "table", &mtable, NULL);
	dlg = fk_declare_new ((GtkWindow *) parent, mstruct, mtable);
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
	g_object_unref (mstruct);
}

static void
popup_func_delete_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvasTable *ce)
{
	GdaMetaTable *mtable;
	BrowserCanvasDbRelations *dbrel;

	dbrel = BROWSER_CANVAS_DB_RELATIONS (browser_canvas_item_get_canvas (BROWSER_CANVAS_ITEM (ce)));

	mtable = g_hash_table_lookup (dbrel->priv->hash_tables, ce);
	g_hash_table_remove (dbrel->priv->hash_tables, ce);
	g_hash_table_remove (dbrel->priv->hash_tables, mtable);
	goo_canvas_item_remove (GOO_CANVAS_ITEM (ce));

	/* remove FK items */
	GSList *list;
	for (list = mtable->fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
		GooCanvasItem *fk_item;
		
		fk_item = g_hash_table_lookup (dbrel->priv->hash_fkeys, fk);
		if (fk_item) {
			goo_canvas_item_remove (fk_item);
			g_hash_table_remove (dbrel->priv->hash_fkeys, fk);
		}
	}
	
	for (list = mtable->reverse_fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
		GooCanvasItem *fk_item;
		
		fk_item = g_hash_table_lookup (dbrel->priv->hash_fkeys, fk);
		if (fk_item) {
			goo_canvas_item_remove (fk_item);
			g_hash_table_remove (dbrel->priv->hash_fkeys, fk);
		}
	}
}

static void
popup_func_add_depend_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvasTable *ce)
{
	BrowserCanvasDbRelations *dbrel;
	GdaMetaDbObject *dbo;

	dbrel = BROWSER_CANVAS_DB_RELATIONS (browser_canvas_item_get_canvas (BROWSER_CANVAS_ITEM (ce)));
	dbo = g_hash_table_lookup (dbrel->priv->hash_tables, ce);
	if (!dbo || (dbo->obj_type != GDA_META_DB_TABLE))
		return;

	if (!dbrel->priv->mstruct)
		return;
	
	GdaMetaTable *mtable = GDA_META_TABLE (dbo);
	GSList *list;

	for (list = mtable->fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = GDA_META_TABLE_FOREIGN_KEY (list->data);
		if (fk->depend_on->obj_type != GDA_META_DB_TABLE)
			continue;
		if (g_hash_table_lookup (dbrel->priv->hash_tables, fk->depend_on))
			continue;

		GValue *v1, *v2, *v3;
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), fk->depend_on->obj_catalog);
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), fk->depend_on->obj_schema);
		g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), fk->depend_on->obj_name);
		browser_canvas_db_relations_add_table (dbrel, v1, v2, v3);
		gda_value_free (v1);
		gda_value_free (v2);
		gda_value_free (v3);
	}
}

static void
popup_func_add_ref_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvasTable *ce)
{
	BrowserCanvasDbRelations *dbrel;
	GdaMetaDbObject *dbo;

	dbrel = BROWSER_CANVAS_DB_RELATIONS (browser_canvas_item_get_canvas (BROWSER_CANVAS_ITEM (ce)));
	dbo = g_hash_table_lookup (dbrel->priv->hash_tables, ce);
	if (!dbo || (dbo->obj_type != GDA_META_DB_TABLE))
		return;

	if (!dbrel->priv->mstruct)
		return;
	
	GSList *alldbo, *list;

	alldbo = gda_meta_struct_get_all_db_objects (dbrel->priv->mstruct);
	for (list = alldbo; list; list = list->next) {
		GdaMetaDbObject *fkdbo = GDA_META_DB_OBJECT (list->data);
		if (fkdbo->obj_type != GDA_META_DB_TABLE)
			continue;

		GSList *fklist;
		for (fklist = GDA_META_TABLE (fkdbo)->fk_list; fklist; fklist = fklist->next) {
			GdaMetaTableForeignKey *fk = GDA_META_TABLE_FOREIGN_KEY (fklist->data);
			if (fk->depend_on != dbo)
				continue;
			if (g_hash_table_lookup (dbrel->priv->hash_tables, fkdbo))
				continue;

			GValue *v1, *v2, *v3;
			g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), fkdbo->obj_catalog);
			g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), fkdbo->obj_schema);
			g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), fkdbo->obj_name);
			browser_canvas_db_relations_add_table (dbrel, v1, v2, v3);
			gda_value_free (v1);
			gda_value_free (v2);
			gda_value_free (v3);
		}
	}
	g_slist_free (alldbo);
}

static GSList *
complement_layout_items (BrowserCanvasDbRelations *dbrel, BrowserCanvasItem *current, GSList *elist)
{
	GSList *items = elist;
	GdaMetaTable *mtable;
	mtable = g_hash_table_lookup (dbrel->priv->hash_tables, current);
	if (!mtable)
		return items;

	GSList *list;
	for (list = mtable->fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
		BrowserCanvasItem *item;

		item = g_hash_table_lookup (dbrel->priv->hash_fkeys, fk);
		if (item && !g_slist_find (items, item)) {
			items = g_slist_prepend (items, item);
			items = complement_layout_items (dbrel, item, items);
		}
	}

	for (list = mtable->reverse_fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
		BrowserCanvasItem *item;

		item = g_hash_table_lookup (dbrel->priv->hash_fkeys, fk);
		if (item && !g_slist_find (items, item)) {
			items = g_slist_prepend (items, item);
			items = complement_layout_items (dbrel, item, items);
		}
	}

	return items;
}

static GSList *
get_layout_items (BrowserCanvas *canvas)
{
	GSList *items = NULL;
	BrowserCanvasDbRelations *dbrel = BROWSER_CANVAS_DB_RELATIONS (canvas);

	if (!canvas->priv->current_selected_item)
		return g_slist_copy (canvas->priv->items);
	
	GdaMetaTable *mtable;
	mtable = g_hash_table_lookup (dbrel->priv->hash_tables, canvas->priv->current_selected_item);
	if (!mtable)
		return g_slist_copy (canvas->priv->items);

	items = g_slist_prepend (NULL, canvas->priv->current_selected_item);
	items = complement_layout_items (dbrel, canvas->priv->current_selected_item, items);
	
	/* add non related items */
	GSList *list;
	for (list = canvas->priv->items; list; list = list->next) {
		if (!g_slist_find (items, list->data))
			items = g_slist_prepend (items, list->data);
	}

	return g_slist_reverse (items);
}
static gint dbo_sort_func (GdaMetaDbObject *dbo1, GdaMetaDbObject *dbo2);
static void popup_add_table_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *canvas);
static void table_menu_item_activated_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrel);
static void popup_add_all_tables_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrel);
static GtkWidget *
build_context_menu (BrowserCanvas *canvas)
{
	GtkWidget *menu, *submitem, *submenu, *mitem;
	BrowserCanvasDbRelations *dbrel = BROWSER_CANVAS_DB_RELATIONS (canvas);
	GSList *list, *all_dbo;

	if (!dbrel->priv->mstruct)
		return NULL;

	menu = gtk_menu_new ();
	/* entry to display a window with tables in it */
	submitem = gtk_menu_item_new_with_label (_("Add tables"));
	gtk_widget_show (submitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), submitem);
	g_signal_connect (G_OBJECT (submitem), "activate", G_CALLBACK (popup_add_table_cb), canvas);

	/* entry to display sub menus */
	submitem = gtk_menu_item_new_with_label (_("Add one table"));
	gtk_widget_show (submitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), submitem);
	submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (submitem), submenu);

	/* build sub menu */
	GHashTable *schemas = NULL; /* key = schema name, value = a #GtkMenu as parent */
	GSList *added_schemas = NULL;
	schemas = g_hash_table_new (g_str_hash, g_str_equal);
	all_dbo = gda_meta_struct_get_all_db_objects (dbrel->priv->mstruct);
	all_dbo = g_slist_sort (all_dbo, (GCompareFunc) dbo_sort_func);

	for (list = all_dbo; list; list = list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		if (dbo->obj_type != GDA_META_DB_TABLE)
			continue;
		if (g_hash_table_lookup (dbrel->priv->hash_tables, dbo))
			/* table already present on canvas */
			continue;
		
		if (strcmp (dbo->obj_short_name, dbo->obj_full_name)) {
			mitem = gtk_menu_item_new_with_label (dbo->obj_short_name);
			g_object_set_data (G_OBJECT (mitem), "dbtable", GDA_META_TABLE (dbo));
			gtk_menu_shell_prepend (GTK_MENU_SHELL (submenu), mitem);
			g_signal_connect (mitem, "activate",
					  G_CALLBACK (table_menu_item_activated_cb), dbrel);
		}

		GtkWidget *schema_menu;
		schema_menu = g_hash_table_lookup (schemas, dbo->obj_schema);
		if (!schema_menu) {
			mitem = gtk_menu_item_new_with_label (dbo->obj_schema);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mitem);

			schema_menu = gtk_menu_new ();
			g_object_set_data (G_OBJECT (schema_menu), "dbo", dbo);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), schema_menu);
			g_hash_table_insert (schemas, dbo->obj_schema, schema_menu);
			added_schemas = g_slist_prepend (added_schemas, schema_menu);
		}
		
		mitem = gtk_menu_item_new_with_label (dbo->obj_short_name);
		g_object_set_data (G_OBJECT (mitem), "dbtable", GDA_META_TABLE (dbo));
		gtk_menu_shell_prepend (GTK_MENU_SHELL (schema_menu), mitem);
		g_signal_connect (mitem, "activate",
				  G_CALLBACK (table_menu_item_activated_cb), dbrel);
	}
	g_slist_free (all_dbo);
	g_hash_table_destroy (schemas);

	/* entry to add ALL tables */
	mitem = gtk_separator_menu_item_new ();
	gtk_widget_show (mitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (submenu), mitem);

	mitem = gtk_menu_item_new_with_label (_("Add all tables"));
	gtk_widget_show (mitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (submenu), mitem);
	g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_add_all_tables_cb), dbrel);

	/* entry below each schema sub menu to add all tables in schema */
	for (list = added_schemas; list; list = list->next) {
		GdaMetaDbObject *dbo;
		dbo = g_object_get_data (G_OBJECT (list->data), "dbo");
		g_assert (dbo);

		mitem = gtk_separator_menu_item_new ();
		gtk_widget_show (mitem);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (list->data), mitem);

		mitem = gtk_menu_item_new_with_label (_("Add all tables in schema"));
		gtk_widget_show (mitem);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (list->data), mitem);
		g_object_set_data_full (G_OBJECT (mitem), "schema", g_strdup (dbo->obj_schema),
					g_free);
		g_signal_connect (G_OBJECT (mitem), "activate",
				  G_CALLBACK (popup_add_all_tables_cb), dbrel);
	}
	g_slist_free (added_schemas);

	gtk_widget_show_all (submenu);

	return menu;
}

static gint
dbo_sort_func (GdaMetaDbObject *dbo1, GdaMetaDbObject *dbo2)
{
	const gchar *n1, *n2;
	g_assert (dbo1);
	g_assert (dbo2);
	if (dbo1->obj_name[0] ==  '"')
		n1 = dbo1->obj_name + 1;
	else
		n1 = dbo1->obj_name;
	if (dbo2->obj_name[0] ==  '"')
		n2 = dbo2->obj_name + 1;
	else
		n2 = dbo2->obj_name;
	return strcmp (n2, n1);
}

static void
popup_add_all_tables_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrel)
{
	GSList *all_dbo, *list;
	const gchar *schema;
	schema = g_object_get_data (G_OBJECT (mitem), "schema");
	all_dbo = gda_meta_struct_get_all_db_objects (dbrel->priv->mstruct);
	for (list = all_dbo; list; list = list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		if (dbo->obj_type != GDA_META_DB_TABLE)
			continue;
		if (g_hash_table_lookup (dbrel->priv->hash_tables, dbo))
			/* table already present on canvas */
			continue;
		if (schema && strcmp (schema, dbo->obj_schema))
			continue;
		
		GValue *table_schema;
		GValue *table_name;
		BrowserCanvasTable *ctable;
		GooCanvasBounds bounds;
		gdouble x, y;
		g_value_set_string ((table_schema = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
		g_value_set_string ((table_name = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
		
		x = BROWSER_CANVAS (dbrel)->xmouse;
		y = BROWSER_CANVAS (dbrel)->ymouse;
		
		ctable = browser_canvas_db_relations_add_table (dbrel, NULL, table_schema, table_name);
		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ctable), &bounds);
		browser_canvas_item_translate (BROWSER_CANVAS_ITEM (ctable),
					       x - bounds.x1, y - bounds.y1);
		gda_value_free (table_schema);
		gda_value_free (table_name);
	}
	g_slist_free (all_dbo);
}

static void
table_menu_item_activated_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrel)
{
	GValue *table_schema;
	GValue *table_name;
	GdaMetaTable *mtable;
	GdaMetaDbObject *dbo;
	BrowserCanvasTable *ctable;
	GooCanvasBounds bounds;
	gdouble x, y;

	mtable = g_object_get_data (G_OBJECT (mitem), "dbtable");
	if (! mtable)
		return;
	
	dbo = GDA_META_DB_OBJECT (mtable);
	g_value_set_string ((table_schema = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
	g_value_set_string ((table_name = gda_value_new (G_TYPE_STRING)), dbo->obj_name);

	x = BROWSER_CANVAS (dbrel)->xmouse;
	y = BROWSER_CANVAS (dbrel)->ymouse;

	ctable = browser_canvas_db_relations_add_table (dbrel, NULL, table_schema, table_name);
	goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ctable), &bounds);
	browser_canvas_item_translate (BROWSER_CANVAS_ITEM (ctable),
				       x - bounds.x1, y - bounds.y1);
	gda_value_free (table_schema);
	gda_value_free (table_name);
}

static gboolean
add_dialog_delete_event (GtkWidget *dialog, G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	gtk_widget_hide (dialog);
	return TRUE;
}

static void
popup_add_table_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrels)
{
	if (! dbrels->priv->add_dialog) {
		GtkWidget *vbox, *cloud, *find;
		dbrels->priv->add_dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (dbrels->priv->add_dialog),
				      _("Select tables to add to diagram"));
		gtk_window_set_transient_for (GTK_WINDOW (dbrels->priv->add_dialog),
					      (GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) dbrels));
		g_signal_connect (dbrels->priv->add_dialog, "delete-event",
				  G_CALLBACK (add_dialog_delete_event), NULL);
		gtk_window_set_default_size (GTK_WINDOW (dbrels->priv->add_dialog), 430, 400);
		
		g_object_set_data (G_OBJECT (dbrels->priv->add_dialog), "__canvas", dbrels);

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_container_add (GTK_CONTAINER (dbrels->priv->add_dialog), vbox);
		
		cloud = objects_cloud_new (dbrels->priv->mstruct, OBJECTS_CLOUD_TYPE_TABLE);
		dbrels->priv->cloud = OBJECTS_CLOUD (cloud);
		gtk_widget_set_size_request (GTK_WIDGET (cloud), 200, 300);
		g_signal_connect (cloud, "selected",
				  G_CALLBACK (cloud_object_selected_cb), dbrels);
		gtk_box_pack_start (GTK_BOX (vbox), cloud, TRUE, TRUE, 0);
		
		find = objects_cloud_create_filter (OBJECTS_CLOUD (cloud));
		gtk_box_pack_start (GTK_BOX (vbox), find, FALSE, FALSE, 0);
		
		gtk_widget_show_all (vbox);
	}

	gtk_widget_show (dbrels->priv->add_dialog);
}

/**
 * browser_canvas_db_relations_get_table_item
 * @canvas:
 * @table:
 *
 * Returns:
 */
BrowserCanvasTable *
browser_canvas_db_relations_get_table_item  (BrowserCanvasDbRelations *canvas, GdaMetaTable *table)
{
	BrowserCanvasTable *table_item;
	g_return_val_if_fail (IS_BROWSER_CANVAS_DB_RELATIONS (canvas), NULL);
	g_return_val_if_fail (canvas->priv, NULL);

	table_item = g_hash_table_lookup (canvas->priv->hash_tables, table);
	return BROWSER_CANVAS_TABLE (table_item);
}

/**
 * browser_canvas_db_relations_add_table
 * @canvas: a #BrowserCanvasDbRelations canvas
 * @table_catalog: (nullable): the catalog in which the table is, or %NULL
 * @table_schema: (nullable): the schema in which the table is, or %NULL
 * @table_name: the table's name
 *
 * Add a table to @canvas.
 *
 * Returns: (transfer none): the corresponding canvas item, or %NULL if the table was not found.
 */
BrowserCanvasTable *
browser_canvas_db_relations_add_table  (BrowserCanvasDbRelations *canvas, 
					const GValue *table_catalog, const GValue *table_schema,
					const GValue *table_name)
{
	g_return_val_if_fail (IS_BROWSER_CANVAS_DB_RELATIONS (canvas), NULL);

	GdaMetaTable *mtable;
	GooCanvas *goocanvas;
	GError *lerror = NULL;

	if (!canvas->priv->mstruct)
		return NULL;

	goocanvas = BROWSER_CANVAS (canvas)->priv->goocanvas;
	mtable = (GdaMetaTable *) gda_meta_struct_complement (canvas->priv->mstruct, GDA_META_DB_TABLE,
							      table_catalog, table_schema, table_name, &lerror);
	if (mtable) {
		gdouble x = 0, y = 0;
		GooCanvasItem *table_item;

		table_item = g_hash_table_lookup (canvas->priv->hash_tables, mtable);
		if (table_item)
			return BROWSER_CANVAS_TABLE (table_item);

		table_item = browser_canvas_table_new (goo_canvas_get_root_item (goocanvas), 
						       canvas->priv->mstruct, mtable, x, y, NULL);
		g_hash_table_insert (canvas->priv->hash_tables, mtable, table_item);
		g_hash_table_insert (canvas->priv->hash_tables, table_item, mtable);
		g_object_set (G_OBJECT (table_item), 
			      "popup_menu_func", canvas_entity_popup_func, NULL);
		browser_canvas_declare_item (BROWSER_CANVAS (canvas),
					     BROWSER_CANVAS_ITEM (table_item));
		goo_canvas_item_raise (GOO_CANVAS_ITEM (table_item), canvas->priv->level_separator);

		/* if there are some FK links, then also add them */
		GSList *list;
		for (list = mtable->fk_list; list; list = list->next) {
			GooCanvasItem *ref_table_item;
			GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
			ref_table_item = g_hash_table_lookup (canvas->priv->hash_tables, fk->depend_on);
			if (ref_table_item) {
				GooCanvasItem *fk_item;
				fk_item = g_hash_table_lookup (canvas->priv->hash_fkeys, fk);
				if (!fk_item) {
					fk_item = browser_canvas_fkey_new (goo_canvas_get_root_item (goocanvas),
									   canvas->priv->mstruct, fk, NULL);
					browser_canvas_declare_item (BROWSER_CANVAS (canvas),
								     BROWSER_CANVAS_ITEM (fk_item));
					
					g_hash_table_insert (canvas->priv->hash_fkeys, fk, fk_item);
					goo_canvas_item_lower (GOO_CANVAS_ITEM (fk_item),
							       canvas->priv->level_separator);
				}
			}
		}
		for (list = mtable->reverse_fk_list; list; list = list->next) {
			GooCanvasItem *ref_table_item;
			GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
			ref_table_item = g_hash_table_lookup (canvas->priv->hash_tables, fk->meta_table);
			if (ref_table_item) {
				GooCanvasItem *fk_item;
				fk_item = g_hash_table_lookup (canvas->priv->hash_fkeys, fk);
				if (!fk_item) {
					fk_item = browser_canvas_fkey_new (goo_canvas_get_root_item (goocanvas),
									   canvas->priv->mstruct, fk, NULL);
					browser_canvas_declare_item (BROWSER_CANVAS (canvas),
								     BROWSER_CANVAS_ITEM (fk_item));
					
					g_hash_table_insert (canvas->priv->hash_fkeys, fk, fk_item);
					goo_canvas_item_lower (GOO_CANVAS_ITEM (fk_item),
							       canvas->priv->level_separator);
				}
			}
		}

		return BROWSER_CANVAS_TABLE (table_item);
	}
	else {
		g_print ("WARNING: %s\n", lerror && lerror->message ? lerror->message : "No detail");
		g_clear_error (&lerror);
		return NULL;
	}
}

/**
 * browser_canvas_db_relations_select_table
 */
void
browser_canvas_db_relations_select_table (BrowserCanvasDbRelations *canvas,
					  BrowserCanvasTable *table)
{
	g_return_if_fail (IS_BROWSER_CANVAS_DB_RELATIONS (canvas));
	g_return_if_fail (!table || IS_BROWSER_CANVAS_ITEM (table));

	browser_canvas_item_toggle_select (BROWSER_CANVAS (canvas), (BrowserCanvasItem*) table);
}

/**
 * browser_canvas_db_relations_items_to_data_manager
 */
gchar *
browser_canvas_db_relations_items_to_data_manager (BrowserCanvasDbRelations *canvas)
{
	gchar *retval = NULL;
	GSList *list;
	xmlDocPtr doc;
	xmlNodePtr topnode;

	g_return_val_if_fail (IS_BROWSER_CANVAS (canvas), NULL);
	
	/* create XML doc and root node */
	doc = xmlNewDoc (BAD_CAST "1.0");
	topnode = xmlNewDocNode (doc, NULL, BAD_CAST "data", NULL);
        xmlDocSetRootElement (doc, topnode);
	
	/* actually serialize all the items which can be serialized */
	for (list = BROWSER_CANVAS (canvas)->priv->items; list; list = list->next) {
                BrowserCanvasItem *item = BROWSER_CANVAS_ITEM (list->data);
		GdaMetaTable *mtable;

		mtable = g_hash_table_lookup (canvas->priv->hash_tables, item);
		if (mtable) {
			xmlNodePtr node;
			node = xmlNewChild (topnode, NULL, BAD_CAST "table", NULL);
			xmlSetProp (node, BAD_CAST "name",
				    BAD_CAST GDA_META_DB_OBJECT (mtable)->obj_short_name);

			GSList *fklist;
			for (fklist = mtable->fk_list; fklist; fklist = fklist->next) {
				GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) fklist->data;
				GooCanvasItem *fk_item;
				
				fk_item = g_hash_table_lookup (canvas->priv->hash_fkeys, fk);
				if (fk_item) {
					node = xmlNewChild (node, NULL, BAD_CAST "depend", NULL);
					xmlSetProp (node, BAD_CAST "foreign_key_table",
						    BAD_CAST fk->depend_on->obj_short_name);
				}
			}

		}
	}

	/* create buffer from XML tree */
	xmlChar *xstr = NULL;
	xmlDocDumpFormatMemory (doc, &xstr, NULL, 1);
	if (xstr) {
		retval = g_strdup ((gchar *) xstr);
		xmlFree (xstr);
	}
	xmlFreeDoc (doc);
	
	return retval;	
}
