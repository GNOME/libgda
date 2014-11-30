/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgda/gda-tree.h>
#include "table-info.h"
#include "table-relations.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../gdaui-bar.h"
#include "schema-browser-perspective.h"
#include "../canvas/browser-canvas-db-relations.h"

struct _TableRelationsPrivate {
	TConnection *tcnc;
	TableInfo *tinfo;
	GtkWidget *canvas;
	gboolean all_schemas;
};

static void table_relations_class_init (TableRelationsClass *klass);
static void table_relations_init       (TableRelations *trels, TableRelationsClass *klass);
static void table_relations_dispose   (GObject *object);

static void meta_changed_cb (TConnection *tcnc, GdaMetaStruct *mstruct, TableRelations *trels);

static GObjectClass *parent_class = NULL;


/*
 * TableRelations class implementation
 */

static void
table_relations_class_init (TableRelationsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = table_relations_dispose;
}


static void
table_relations_init (TableRelations *trels, G_GNUC_UNUSED TableRelationsClass *klass)
{
	trels->priv = g_new0 (TableRelationsPrivate, 1);
	trels->priv->all_schemas = FALSE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (trels), GTK_ORIENTATION_VERTICAL);
}

static void
table_relations_dispose (GObject *object)
{
	TableRelations *trels = (TableRelations *) object;

	/* free memory */
	if (trels->priv) {
		if (trels->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (trels->priv->tcnc,
							      G_CALLBACK (meta_changed_cb), trels);
			g_object_unref (trels->priv->tcnc);
		}
		g_free (trels->priv);
		trels->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
table_relations_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo relations = {
			sizeof (TableRelationsClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) table_relations_class_init,
			NULL,
			NULL,
			sizeof (TableRelations),
			0,
			(GInstanceInitFunc) table_relations_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "TableRelations", &relations, 0);
	}
	return type;
}

static void
meta_changed_cb (G_GNUC_UNUSED TConnection *tcnc, GdaMetaStruct *mstruct, TableRelations *trels)
{
	GdaMetaDbObject *dbo;
	GValue *tname, *tschema;
	BrowserCanvasTable *ctable;

	g_object_set (G_OBJECT (trels->priv->canvas), "meta-struct", mstruct, NULL);

	g_value_set_string ((tschema = gda_value_new (G_TYPE_STRING)),
			    table_info_get_table_schema (trels->priv->tinfo));
	g_value_set_string ((tname = gda_value_new (G_TYPE_STRING)),
			    table_info_get_table_name (trels->priv->tinfo));
        ctable = browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (trels->priv->canvas), NULL,
							tschema, tname);
	browser_canvas_db_relations_select_table (BROWSER_CANVAS_DB_RELATIONS (trels->priv->canvas),
						  ctable);

	dbo = gda_meta_struct_get_db_object (mstruct, NULL, tschema, tname);

	if (dbo && (dbo->obj_type == GDA_META_DB_TABLE)) {
		GdaMetaTable *table;
		table = GDA_META_TABLE (dbo);
		
		GSList *list;
		for (list = table->reverse_fk_list; list; list = list->next) {
			GdaMetaTableForeignKey *fkey = (GdaMetaTableForeignKey*) list->data;
			if (! trels->priv->all_schemas &&
			    ! strcmp (fkey->meta_table->obj_short_name, fkey->meta_table->obj_full_name))
				continue;
			g_value_set_string (tname, fkey->meta_table->obj_name);
			g_value_set_string (tschema, fkey->meta_table->obj_schema);
			browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (trels->priv->canvas),
							       NULL,
							       tschema, tname);
		}

		for (list = table->fk_list; list; list = list->next) {
			GdaMetaTableForeignKey *fkey = (GdaMetaTableForeignKey*) list->data;

			if (! trels->priv->all_schemas &&
			    (fkey->depend_on->obj_type != GDA_META_DB_UNKNOWN) &&
			    ! strcmp (fkey->depend_on->obj_short_name, fkey->depend_on->obj_full_name))
				continue;

			g_value_set_string (tname, fkey->depend_on->obj_name);
			g_value_set_string (tschema, fkey->depend_on->obj_schema);
			browser_canvas_db_relations_add_table (BROWSER_CANVAS_DB_RELATIONS (trels->priv->canvas),
							       NULL,
							       tschema, tname);
		}
	}
	gda_value_free (tschema);
	gda_value_free (tname);

	browser_canvas_perform_auto_layout (BROWSER_CANVAS (trels->priv->canvas), TRUE,
					    BROWSER_CANVAS_LAYOUT_DEFAULT);
}

/**
 * table_relations_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
table_relations_new (TableInfo *tinfo)
{
	TableRelations *trels;

	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);

	trels = TABLE_RELATIONS (g_object_new (TABLE_RELATIONS_TYPE, NULL));

	trels->priv->tinfo = tinfo;
	trels->priv->tcnc = g_object_ref (table_info_get_connection (tinfo));
	g_signal_connect (trels->priv->tcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), trels);
	
	/*
	 * Relations
	 */
	trels->priv->canvas = browser_canvas_db_relations_new (NULL);
	gtk_box_pack_start (GTK_BOX (trels), trels->priv->canvas, TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (trels));

	/*
	 * initial update
	 */
	GdaMetaStruct *mstruct;
	mstruct = t_connection_get_meta_struct (trels->priv->tcnc);
	if (mstruct)
		meta_changed_cb (trels->priv->tcnc, mstruct, trels);

	return (GtkWidget*) trels;
}

