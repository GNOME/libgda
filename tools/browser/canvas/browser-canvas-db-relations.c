/* browser-canvas-db-relations.c
 *
 * Copyright (C) 2002 - 2009 Vivien Malerba
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
#include "../common/objects-cloud.h"
#include "../common/popup-container.h"

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

	GtkWidget        *popup_container;
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

		gtk_widget_destroy (canvas->priv->popup_container);

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
		case PROP_META_STRUCT: {
			GdaMetaStruct *mstruct = g_value_get_object (value);
			if (mstruct)
				g_object_ref (mstruct);

			if (canvas->priv->mstruct) {
				clean_canvas_items (BROWSER_CANVAS (canvas));
				g_object_unref (canvas->priv->mstruct);
				canvas->priv->mstruct = NULL;
			}
			canvas->priv->mstruct = mstruct;
			if (canvas->priv->cloud)
				objects_cloud_set_meta_struct (canvas->priv->cloud, canvas->priv->mstruct);
			break;
		}
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

static void
popup_position (PopupContainer *container, gint *out_x, gint *out_y)
{
	GtkWidget *canvas;
        canvas = g_object_get_data (G_OBJECT (container), "__canvas");

        gint x, y;
        GtkRequisition req;

        gtk_widget_size_request (canvas, &req);

#if GTK_CHECK_VERSION(2,18,0)
	GtkAllocation alloc;
	gdk_window_get_origin (gtk_widget_get_window (canvas), &x, &y);
	gtk_widget_get_allocation (canvas, &alloc);
        x += alloc.x;
        y += alloc.y;
#else
        gdk_window_get_origin (canvas->window, &x, &y);

        x += canvas->allocation.x;
        y += canvas->allocation.y;
#endif

        if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;
        *out_x = x;
        *out_y = y;
}

static void
cloud_object_selected_cb (ObjectsCloud *ocloud, ObjectsCloudObjType sel_type, 
			  const gchar *sel_contents, BrowserCanvasDbRelations *dbrel)
{
	g_print ("-> %s\n", sel_contents);

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

	g_print ("Add %s.%s\n",
		 g_value_get_string (table_schema), g_value_get_string (table_name));
	mtable = (GdaMetaTable*) gda_meta_struct_complement (dbrel->priv->mstruct, GDA_META_DB_TABLE,
							     NULL, table_schema, table_name, NULL);
	if (mtable) {
		BrowserCanvasTable *ctable;
		GooCanvasBounds bounds;
		gdouble x, y;
		
		x = BROWSER_CANVAS (dbrel)->xmouse;
		y = BROWSER_CANVAS (dbrel)->ymouse;
		//goo_canvas_convert_from_pixels (BROWSER_CANVAS (dbrel)->priv->goocanvas, &x, &y);
		ctable = browser_canvas_db_relations_add_table (dbrel, NULL, table_schema, table_name);
		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (ctable), &bounds);
		browser_canvas_item_translate (BROWSER_CANVAS_ITEM (ctable),
					       x - bounds.x1, y - bounds.y1);
	}
	else
		g_print ("Unknown...\n");
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
	BrowserCanvas *canvas;
	BrowserCanvasDbRelations *dbrels;
	GooCanvasItem *item;
	GtkWidget *vbox, *cloud, *find;
	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), NULL);

	canvas = BROWSER_CANVAS (g_object_new (TYPE_BROWSER_CANVAS_DB_RELATIONS, "meta-struct", mstruct, NULL));
	dbrels = BROWSER_CANVAS_DB_RELATIONS (canvas);
	item = goo_canvas_group_new (goo_canvas_get_root_item (canvas->priv->goocanvas), NULL);
	dbrels->priv->level_separator = item;

	dbrels->priv->popup_container = popup_container_new_with_func (popup_position);
	g_object_set_data (G_OBJECT (dbrels->priv->popup_container), "__canvas", canvas);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (dbrels->priv->popup_container), vbox);

	cloud = objects_cloud_new (mstruct, OBJECTS_CLOUD_TYPE_TABLE);
	dbrels->priv->cloud = OBJECTS_CLOUD (cloud);
	gtk_widget_set_size_request (GTK_WIDGET (cloud), 200, 300);
	g_signal_connect (cloud, "selected",
			  G_CALLBACK (cloud_object_selected_cb), dbrels);
	gtk_box_pack_start (GTK_BOX (vbox), cloud, TRUE, TRUE, 0);

	find = objects_cloud_create_filter (OBJECTS_CLOUD (cloud));
	gtk_box_pack_start (GTK_BOX (vbox), find, FALSE, FALSE, 0);
	
	gtk_widget_show_all (vbox);

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
static GtkWidget *
canvas_entity_popup_func (BrowserCanvasTable *ce)
{
	GtkWidget *menu, *entry;

	menu = gtk_menu_new ();
	entry = gtk_menu_item_new_with_label (_("Remove"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_delete_cb), ce);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
	gtk_widget_show (entry);
	entry = gtk_menu_item_new_with_label (_("Add referenced tables"));
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (popup_func_add_depend_cb), ce);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
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

static void popup_add_table_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *canvas);
static GtkWidget *
build_context_menu (BrowserCanvas *canvas)
{
	GtkWidget *menu, *submitem;
	BrowserCanvasDbRelations *dbrel = BROWSER_CANVAS_DB_RELATIONS (canvas);

	if (!dbrel->priv->mstruct)
		return NULL;

	menu = gtk_menu_new ();
	submitem = gtk_menu_item_new_with_label (_("Add table"));
	gtk_widget_show (submitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), submitem);
	g_signal_connect (G_OBJECT (submitem), "activate", G_CALLBACK (popup_add_table_cb), canvas);

	return menu;
}

static void
popup_add_table_cb (GtkMenuItem *mitem, BrowserCanvasDbRelations *dbrel)
{
	gtk_widget_show (dbrel->priv->popup_container);
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
	else
		return NULL;
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
				if (fk_item)
					xmlNewChild (node, NULL, BAD_CAST "link_with",
						     BAD_CAST fk->depend_on->obj_short_name);
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
