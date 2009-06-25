/* browser-canvas-db-relations.c
 *
 * Copyright (C) 2002 - 2007 Vivien Malerba
 * Copyright (C) 2002 Fernando Martins
 *
 * This program is free software; you can redistribute it and/or
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

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "browser-canvas-priv.h"
#include "browser-canvas-db-relations.h"
#include "browser-canvas-table.h"
#include "browser-canvas-column.h"
#include "browser-canvas-fkey.h"

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
	GSList           *all_items; /* list of all the BrowserCanvasItem objects */

	GdaMetaStruct    *mstruct;
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
			(GInstanceInitFunc) browser_canvas_db_relations_init
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
		if (canvas->priv->mstruct) {
			g_object_unref (canvas->priv->mstruct);
			canvas->priv->mstruct = NULL;
		}

		g_hash_table_destroy (canvas->priv->hash_tables);
		g_hash_table_destroy (canvas->priv->hash_fkeys);

		g_free (canvas->priv);
		canvas->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
browser_canvas_db_relations_set_property (GObject *object,
					guint param_id,
					const GValue *value,
					GParamSpec *pspec)
{
	BrowserCanvasDbRelations *canvas;

        canvas = BROWSER_CANVAS_DB_RELATIONS (object);
        if (canvas->priv) {
                switch (param_id) {
		case PROP_META_STRUCT:
			if (canvas->priv->mstruct)
				clean_canvas_items (BROWSER_CANVAS (canvas));

			canvas->priv->mstruct = g_value_get_object (value);
                        if (canvas->priv->mstruct) 
                                g_object_ref (canvas->priv->mstruct);
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
		}
	}
}

/**
 * browser_canvas_db_relations_new
 * @mstruct: a #GdaMetaStruct object, or %NULL
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
	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), NULL);
        return GTK_WIDGET (g_object_new (TYPE_BROWSER_CANVAS_DB_RELATIONS, "meta-struct", mstruct, NULL));
}



static void
clean_canvas_items (BrowserCanvas *canvas)
{
	BrowserCanvasDbRelations *dbrel = BROWSER_CANVAS_DB_RELATIONS (canvas);

	/* clean memory */
	g_hash_table_destroy (dbrel->priv->hash_tables);
	g_hash_table_destroy (dbrel->priv->hash_fkeys);
	dbrel->priv->hash_tables = g_hash_table_new (NULL, NULL);
	dbrel->priv->hash_fkeys = g_hash_table_new (NULL, NULL);
	if (dbrel->priv->all_items) {
		g_slist_free (dbrel->priv->all_items);
		dbrel->priv->all_items = NULL;
	}
}

static GtkWidget *canvas_entity_popup_func (BrowserCanvasTable *ce);

static void popup_func_delete_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce);
static void popup_func_add_depend_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce);
static GtkWidget *
canvas_entity_popup_func (BrowserCanvasTable *ce)
{
	GtkWidget *menu, *entry;

	menu = gtk_menu_new ();
	entry = gtk_menu_item_new_with_label (_("Remove"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_delete_cb), ce);
	gtk_menu_append (GTK_MENU (menu), entry);
	gtk_widget_show (entry);
	entry = gtk_menu_item_new_with_label (_("Add referenced tables"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_add_depend_cb), ce);
	gtk_menu_append (GTK_MENU (menu), entry);
	gtk_widget_show (entry);

	return menu;
}

static void
popup_func_delete_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce)
{
	GdaMetaTable *mtable;
	BrowserCanvasDbRelations *dbrel;

	dbrel = BROWSER_CANVAS_DB_RELATIONS (browser_canvas_item_get_canvas (BROWSER_CANVAS_ITEM (ce)));

	mtable = g_hash_table_lookup (dbrel->priv->hash_tables, ce);
	g_hash_table_remove (dbrel->priv->hash_tables, ce);
	g_hash_table_remove (dbrel->priv->hash_tables, mtable);
	goo_canvas_item_remove (GOO_CANVAS_ITEM (ce));
	dbrel->priv->all_items = g_slist_remove (dbrel->priv->all_items, ce);
}

static void
popup_func_add_depend_cb (GtkMenuItem *mitem, BrowserCanvasTable *ce)
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
	GooCanvasBounds bounds;
	goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ce), &bounds);
	bounds.y1 = bounds.y2 + 35.;
	bounds.x2 = bounds.x1 - 20.;

	for (list = mtable->fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = GDA_META_TABLE_FOREIGN_KEY (list->data);
		if (fk->depend_on->obj_type != GDA_META_DB_TABLE)
			continue;
		if (g_hash_table_lookup (dbrel->priv->hash_tables, fk->depend_on))
			continue;

		BrowserCanvasTable *new_item;
		GValue *v1, *v2, *v3;
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), fk->depend_on->obj_catalog);
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), fk->depend_on->obj_schema);
		g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), fk->depend_on->obj_name);
		new_item = browser_canvas_db_relations_add_table (dbrel, v1, v2, v3);
		gda_value_free (v1);
		gda_value_free (v2);
		gda_value_free (v3);
								       
		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (new_item), &bounds);
	}
}

static void popup_add_table_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *canvas);
static GtkWidget *
build_context_menu (BrowserCanvas *canvas)
{
	GtkWidget *menu, *mitem, *submenu, *submitem;
	GSList *dbolist, *list;
	BrowserCanvasDbRelations *dbrel = BROWSER_CANVAS_DB_RELATIONS (canvas);

	if (!dbrel->priv->mstruct)
		return NULL;

	menu = gtk_menu_new ();
	submitem = gtk_menu_item_new_with_label (_("Add table"));
	gtk_widget_show (submitem);
	gtk_menu_append (menu, submitem);
	submenu = NULL;

	/* build list of tables */
	dbolist = gda_meta_struct_get_all_db_objects (dbrel->priv->mstruct);
	for (list = dbolist; list; list = list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		GdaMetaTable *mtable;

		if (dbo->obj_type != GDA_META_DB_TABLE)
			continue;
		
		mtable = GDA_META_TABLE (dbo);
		if (mtable && g_hash_table_lookup (dbrel->priv->hash_tables, mtable))
			continue; /* skip that table as it is already present in the canvas */

		if (!submenu) {
			submenu = gtk_menu_new ();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (submitem), submenu);
			gtk_widget_show (submenu);
		}
		
		mitem = gtk_menu_item_new_with_label (dbo->obj_name);
		gtk_widget_show (mitem);
		gtk_menu_append (submenu, mitem);

		GValue *tcatalog, *tschema, *tname;
		g_value_set_string ((tcatalog = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
		g_value_set_string ((tschema = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
		g_value_set_string ((tname = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
		g_object_set_data_full (G_OBJECT (mitem), "tcat", tcatalog, (GDestroyNotify) gda_value_free);
		g_object_set_data_full (G_OBJECT (mitem), "tschema",  tschema, (GDestroyNotify) gda_value_free);
		g_object_set_data_full (G_OBJECT (mitem), "tname", tname, (GDestroyNotify) gda_value_free);
		g_signal_connect (G_OBJECT (mitem), "activate", G_CALLBACK (popup_add_table_cb), canvas);
	}
	g_slist_free (dbolist);

	/* sub menu is incensitive if there are no more tables left to add */
	gtk_widget_set_sensitive (submitem, submenu ? TRUE : FALSE);

	return menu;
}

static void
popup_add_table_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrel)
{
	GdaMetaTable *mtable;
	GValue *table_catalog;
	GValue *table_schema;
	GValue *table_name;

	table_catalog = g_object_get_data (G_OBJECT (mitem), "tcat");
	table_schema = g_object_get_data (G_OBJECT (mitem), "tschema");
	table_name = g_object_get_data (G_OBJECT (mitem), "tname");

	/*g_print ("Add %s.%s.%s\n", g_value_get_string (table_catalog), 
	  g_value_get_string (table_schema), g_value_get_string (table_name));*/
	mtable = (GdaMetaTable*) gda_meta_struct_complement (dbrel->priv->mstruct, GDA_META_DB_TABLE,
							     table_catalog, table_schema, table_name, NULL);
	if (mtable) {
		browser_canvas_db_relations_add_table (dbrel, table_catalog, table_schema, table_name);
	}
	else
		g_print ("Unknown...\n");
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
 * @table_catalog: the catalog in which the table is, or %NULL
 * @table_schema: the schema in which the table is, or %NULL
 * @table_name: the table's name
 *
 * Add a table to @canvas.
 *
 * Returns: the corresponding canvas item, or %NULL if the table was not found.
 */
BrowserCanvasTable *
browser_canvas_db_relations_add_table  (BrowserCanvasDbRelations *canvas, 
					const GValue *table_catalog, const GValue *table_schema,
					const GValue *table_name)
{
	g_return_val_if_fail (IS_BROWSER_CANVAS_DB_RELATIONS (canvas), NULL);
	g_return_val_if_fail (canvas->priv, NULL);

	GdaMetaTable *mtable;
	GooCanvas *goocanvas;

	if (!canvas->priv->mstruct)
		return NULL;

	goocanvas = BROWSER_CANVAS (canvas)->priv->goocanvas;
	mtable = (GdaMetaTable *) gda_meta_struct_complement (canvas->priv->mstruct, GDA_META_DB_TABLE,
							      table_catalog, table_schema, table_name, NULL);
	if (mtable) {
		gdouble x = 0, y = 0;
		GooCanvasItem *table_item;

		table_item = g_hash_table_lookup (canvas->priv->hash_tables, mtable);
		if (table_item)
			return BROWSER_CANVAS_TABLE (table_item);

		table_item = browser_canvas_table_new (goo_canvas_get_root_item (goocanvas), 
						       mtable, x, y, NULL);
		g_hash_table_insert (canvas->priv->hash_tables, mtable, table_item);
		g_hash_table_insert (canvas->priv->hash_tables, table_item, mtable);
		canvas->priv->all_items = g_slist_prepend (canvas->priv->all_items, table_item);
		g_object_set (G_OBJECT (table_item), 
			      "popup_menu_func", canvas_entity_popup_func, NULL);
		browser_canvas_declare_item (BROWSER_CANVAS (canvas),
					     BROWSER_CANVAS_ITEM (table_item));

		/* if there are some FK links, then also add them */
		GSList *list;
		for (list = mtable->fk_list; list; list = list->next) {
			GooCanvasItem *ref_table_item;
			GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
			ref_table_item = g_hash_table_lookup (canvas->priv->hash_tables, fk->depend_on);
			if (ref_table_item) {
				GooCanvasItem *fk_item;
				fk_item = browser_canvas_fkey_new (goo_canvas_get_root_item (goocanvas), fk, NULL);
				browser_canvas_declare_item (BROWSER_CANVAS (canvas),
							     BROWSER_CANVAS_ITEM (fk_item));

				g_hash_table_insert (canvas->priv->hash_fkeys, fk, fk_item);
			}
		}
		for (list = mtable->reverse_fk_list; list; list = list->next) {
			GooCanvasItem *ref_table_item;
			GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
			ref_table_item = g_hash_table_lookup (canvas->priv->hash_tables, fk->meta_table);
			if (ref_table_item) {
				GooCanvasItem *fk_item;
				fk_item = browser_canvas_fkey_new (goo_canvas_get_root_item (goocanvas), fk, NULL);
				browser_canvas_declare_item (BROWSER_CANVAS (canvas),
							     BROWSER_CANVAS_ITEM (fk_item));

				g_hash_table_insert (canvas->priv->hash_fkeys, fk, fk_item);
			}
		}

		return BROWSER_CANVAS_TABLE (table_item);
	}
	else
		return NULL;
}
 
