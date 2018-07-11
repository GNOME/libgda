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

#include <gtk/gtk.h>
#include <math.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "browser-canvas.h"
#include "browser-canvas-fkey.h"
#include "browser-canvas-table.h"
#include "browser-canvas-text.h"
#include "browser-canvas-utility.h"
#include "browser-canvas-db-relations.h"
#include "../ui-support.h"
#include "../browser-window.h"
#include "../fk-declare.h"
#include "../common/t-utils.h"

static void browser_canvas_fkey_class_init (BrowserCanvasFkeyClass * class);
static void browser_canvas_fkey_init       (BrowserCanvasFkey * cc);
static void browser_canvas_fkey_dispose    (GObject *object);
static void browser_canvas_fkey_finalize   (GObject *object);

static void browser_canvas_fkey_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void browser_canvas_fkey_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

static void browser_canvas_fkey_get_edge_nodes (BrowserCanvasItem *citem, 
					      BrowserCanvasItem **from, BrowserCanvasItem **to);

static void clean_items (BrowserCanvasFkey *cc);
static void create_items (BrowserCanvasFkey *cc);
static void update_items (BrowserCanvasFkey *cc);

enum
{
	PROP_0,
	PROP_META_STRUCT,
	PROP_FK_CONSTRAINT
};

struct _BrowserCanvasFkeyPrivate
{
	GdaMetaStruct          *mstruct;
	GdaMetaTableForeignKey *fk;
	BrowserCanvasTable     *fk_table_item;
	BrowserCanvasTable     *ref_pk_table_item;
	GSList                 *shapes; /* list of BrowserCanvasCanvasShape structures */
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;
static GooCanvasLineDash *dash = NULL, *no_dash = NULL;

GType
browser_canvas_fkey_get_type (void)
{
	static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserCanvasFkeyClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_canvas_fkey_class_init,
			NULL,
			NULL,
			sizeof (BrowserCanvasFkey),
			0,
			(GInstanceInitFunc) browser_canvas_fkey_init,
			0
		};		

		type = g_type_register_static (TYPE_BROWSER_CANVAS_ITEM, "BrowserCanvasFkey", &info, 0);
	}

	return type;
}	

static void
browser_canvas_fkey_class_init (BrowserCanvasFkeyClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	BROWSER_CANVAS_ITEM_CLASS (class)->get_edge_nodes = browser_canvas_fkey_get_edge_nodes;

	object_class->dispose = browser_canvas_fkey_dispose;
	object_class->finalize = browser_canvas_fkey_finalize;

	/* Properties */
	object_class->set_property = browser_canvas_fkey_set_property;
	object_class->get_property = browser_canvas_fkey_get_property;

	g_object_class_install_property
                (object_class, PROP_META_STRUCT,
                 g_param_spec_object ("meta-struct", NULL, NULL, 
				      GDA_TYPE_META_STRUCT,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_FK_CONSTRAINT,
					 g_param_spec_pointer ("fk_constraint", "FK constraint", 
							       NULL, 
							       G_PARAM_WRITABLE));

	dash = goo_canvas_line_dash_new (2, 5., 1.5);
	no_dash = goo_canvas_line_dash_new (0);
}

static void
browser_canvas_fkey_init (BrowserCanvasFkey *cc)
{
	cc->priv = g_new0 (BrowserCanvasFkeyPrivate, 1);
	cc->priv->mstruct = NULL;
	cc->priv->fk = NULL;
	cc->priv->fk_table_item = NULL;
	cc->priv->ref_pk_table_item = NULL;
	cc->priv->shapes = NULL;
}

static void
fk_table_item_weak_ref_lost (BrowserCanvasFkey *cc, G_GNUC_UNUSED BrowserCanvasTable *old_table_item)
{
	cc->priv->fk_table_item = NULL;
}

static void
ref_pk_table_item_weak_ref_lost (BrowserCanvasFkey *cc, G_GNUC_UNUSED BrowserCanvasTable *old_table_item)
{
	cc->priv->ref_pk_table_item = NULL;
}


static void
browser_canvas_fkey_dispose (GObject *object)
{
	BrowserCanvasFkey *cc;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_FKEY (object));

	cc = BROWSER_CANVAS_FKEY (object);

	clean_items (cc);
	if (cc->priv->mstruct) {
		g_object_unref (cc->priv->mstruct);
		cc->priv->mstruct = NULL;
	}
	cc->priv->fk = NULL;
	if (cc->priv->fk_table_item) {
		g_object_weak_unref (G_OBJECT (cc->priv->fk_table_item),
				     (GWeakNotify) fk_table_item_weak_ref_lost, cc);
		cc->priv->fk_table_item = NULL;
	}
	if (cc->priv->ref_pk_table_item) {
		g_object_weak_unref (G_OBJECT (cc->priv->ref_pk_table_item),
				     (GWeakNotify) ref_pk_table_item_weak_ref_lost, cc);
		cc->priv->ref_pk_table_item = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}


static void
browser_canvas_fkey_finalize (GObject *object)
{
	BrowserCanvasFkey *cc;
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BROWSER_CANVAS_FKEY (object));

	cc = BROWSER_CANVAS_FKEY (object);
	if (cc->priv) {
		g_slist_free (cc->priv->shapes);
		g_free (cc->priv);
		cc->priv = NULL;
	}

	/* for the parent class */
	parent_class->finalize (object);
}

static void 
browser_canvas_fkey_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	BrowserCanvasFkey *cc;

	cc = BROWSER_CANVAS_FKEY (object);

	switch (param_id) {
	case PROP_META_STRUCT:
		cc->priv->mstruct = g_value_dup_object (value);
		break;
	case PROP_FK_CONSTRAINT:
		if (cc->priv->fk != g_value_get_pointer (value)) {
			cc->priv->fk = g_value_get_pointer (value);
			clean_items (cc);
			create_items (cc);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void 
browser_canvas_fkey_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	BrowserCanvasFkey *cc;

	cc = BROWSER_CANVAS_FKEY (object);

	switch (param_id) {
	case PROP_META_STRUCT:
		g_value_set_object (value, cc->priv->mstruct);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
browser_canvas_fkey_get_edge_nodes (BrowserCanvasItem *citem, 
					  BrowserCanvasItem **from, BrowserCanvasItem **to)
{
	BrowserCanvasFkey *cc;

	cc = BROWSER_CANVAS_FKEY (citem);

	if (from)
		*from = (BrowserCanvasItem*) cc->priv->fk_table_item;
	if (to)
		*to = (BrowserCanvasItem*) cc->priv->ref_pk_table_item;
}

static gboolean single_item_enter_notify_event_cb (GooCanvasItem *ci, GooCanvasItem *target_item,
						   GdkEventCrossing *event, BrowserCanvasFkey *cc);
static gboolean single_item_leave_notify_event_cb (GooCanvasItem *ci, GooCanvasItem *target_item,
						   GdkEventCrossing *event, BrowserCanvasFkey *cc);
static gboolean single_item_button_press_event_cb (GooCanvasItem *ci, GooCanvasItem *target_item,
						   GdkEventButton *event, BrowserCanvasFkey *cc);
static void table_item_moved_cb (GooCanvasItem *table, BrowserCanvasFkey *cc);

/* 
 * destroy any existing GooCanvasItem objects 
 */
static void 
clean_items (BrowserCanvasFkey *cc)
{
	if (cc->priv->fk_table_item) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (cc->priv->fk_table_item),
						      G_CALLBACK (table_item_moved_cb), cc);
		g_object_weak_unref (G_OBJECT (cc->priv->fk_table_item),
				     (GWeakNotify) fk_table_item_weak_ref_lost, cc);
		cc->priv->fk_table_item = NULL;
	}

	if (cc->priv->ref_pk_table_item) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (cc->priv->ref_pk_table_item),
						      G_CALLBACK (table_item_moved_cb), cc);
		g_object_weak_unref (G_OBJECT (cc->priv->ref_pk_table_item),
				     (GWeakNotify) ref_pk_table_item_weak_ref_lost, cc);
		cc->priv->ref_pk_table_item = NULL;
	}
	
	/* remove all the GooCanvasItem objects */
	browser_canvas_canvas_shapes_remove_all (cc->priv->shapes);
	cc->priv->shapes = NULL;
}

/*
 * create new GooCanvasItem objects
 */
static void 
create_items (BrowserCanvasFkey *cc)
{
	GSList *list, *canvas_shapes;
	BrowserCanvasTable *table_item;
	BrowserCanvas *canvas =  g_object_get_data (G_OBJECT (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (cc))),
						    "browsercanvas");

	g_assert (cc->priv->fk);

	/* Analyse FK constraint */
	table_item = browser_canvas_db_relations_get_table_item (BROWSER_CANVAS_DB_RELATIONS (canvas),
								 GDA_META_TABLE (cc->priv->fk->meta_table));
	cc->priv->fk_table_item = table_item;
	g_return_if_fail (table_item);
	g_object_weak_ref (G_OBJECT (table_item), (GWeakNotify) fk_table_item_weak_ref_lost, cc);

	g_signal_connect (G_OBJECT (table_item), "moving",
			  G_CALLBACK (table_item_moved_cb), cc);
	g_signal_connect (G_OBJECT (table_item), "moved",
			  G_CALLBACK (table_item_moved_cb), cc);

	table_item = browser_canvas_db_relations_get_table_item (BROWSER_CANVAS_DB_RELATIONS (canvas),
								 GDA_META_TABLE (cc->priv->fk->depend_on));
	cc->priv->ref_pk_table_item = table_item;
	g_return_if_fail (table_item);

	g_object_weak_ref (G_OBJECT (table_item), (GWeakNotify) ref_pk_table_item_weak_ref_lost, cc);
	g_signal_connect (G_OBJECT (table_item), "moving",
			  G_CALLBACK (table_item_moved_cb), cc);
	g_signal_connect (G_OBJECT (table_item), "moved",
			  G_CALLBACK (table_item_moved_cb), cc);

	/* actual line(s) */
	g_assert (!cc->priv->shapes);
	canvas_shapes = browser_canvas_util_compute_anchor_shapes (GOO_CANVAS_ITEM (cc), NULL,
								   cc->priv->fk_table_item, 
								   cc->priv->ref_pk_table_item, 
								   /*MAX (cc->priv->fk->cols_nb, 1)*/ 1,
								   0, TRUE);

	cc->priv->shapes = browser_canvas_canvas_shapes_remove_obsolete_shapes (canvas_shapes);
	for (list = canvas_shapes; list; list = list->next) {
		GooCanvasItem *item = BROWSER_CANVAS_CANVAS_SHAPE (list->data)->item;
		gchar *color = "black";
		g_object_set (G_OBJECT (item), 
			      "stroke-color", color,
			      "line-dash", GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (cc->priv->fk) ? dash : no_dash,
			      NULL);
		
		if (G_OBJECT_TYPE (item) == GOO_TYPE_CANVAS_POLYLINE) {
			g_object_set (G_OBJECT (item), 
				      "start-arrow", TRUE,
				      "arrow-tip-length", 4.,
				      "arrow-length", 5.,
				      "arrow-width", 4.,
				      NULL);
		}
		else if (G_OBJECT_TYPE (item) == GOO_TYPE_CANVAS_ELLIPSE)
			g_object_set (G_OBJECT (item), 
				      "fill-color", color,
				      NULL);

		g_object_set_data (G_OBJECT (item), "fkcons", cc->priv->fk);
		g_signal_connect (G_OBJECT (item), "enter-notify-event", 
				  G_CALLBACK (single_item_enter_notify_event_cb), cc);
		g_signal_connect (G_OBJECT (item), "leave-notify-event", 
				  G_CALLBACK (single_item_leave_notify_event_cb), cc);
		g_signal_connect (G_OBJECT (item), "button-press-event",
				  G_CALLBACK (single_item_button_press_event_cb), cc);
		
	}
}

/*
 * update GooCanvasItem objects
 */
static void 
update_items (BrowserCanvasFkey *cc)
{
	cc->priv->shapes = browser_canvas_util_compute_anchor_shapes (GOO_CANVAS_ITEM (cc), cc->priv->shapes,
								      cc->priv->fk_table_item, 
								      cc->priv->ref_pk_table_item, 
								      /*MAX (cc->priv->fk->cols_nb, 1)*/ 1,
								      0, TRUE);
	cc->priv->shapes = browser_canvas_canvas_shapes_remove_obsolete_shapes (cc->priv->shapes);
}

/*
 * item is for a single FK constraint
 */
static gboolean
single_item_enter_notify_event_cb (GooCanvasItem *ci, G_GNUC_UNUSED GooCanvasItem *target_item,
				   G_GNUC_UNUSED GdkEventCrossing *event, BrowserCanvasFkey *cc)
{
	gint i;

	for (i = 0; i < cc->priv->fk->cols_nb; i++) {
		GdaMetaTableColumn *tcol;
		BrowserCanvasColumn *column;

		/* fk column */
		tcol = g_slist_nth_data (GDA_META_TABLE (cc->priv->fk->meta_table)->columns, 
					 cc->priv->fk->fk_cols_array[i] - 1);

		column = browser_canvas_table_get_column_item (cc->priv->fk_table_item, tcol);
		browser_canvas_text_set_highlight (BROWSER_CANVAS_TEXT (column), TRUE);
		
		/* ref pk column */
		tcol = g_slist_nth_data (GDA_META_TABLE (cc->priv->fk->depend_on)->columns, 
					 cc->priv->fk->ref_pk_cols_array[i] - 1);

		column = browser_canvas_table_get_column_item (cc->priv->ref_pk_table_item, tcol);
		browser_canvas_text_set_highlight (BROWSER_CANVAS_TEXT (column), TRUE);

		gchar *str;
		str = g_strdup_printf ("%s '%s'\n%s: %s\n%s: %s",
				       GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (cc->priv->fk) ? 
				       _("Declared foreign key") : _("Foreign key"),
				       cc->priv->fk->fk_name,
				       _("Policy on UPDATE"),
				       t_utils_fk_policy_to_string (GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY (cc->priv->fk)),
				       _("Policy on DELETE"),
				       t_utils_fk_policy_to_string (GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY (cc->priv->fk)));
		gtk_widget_set_tooltip_text (GTK_WIDGET (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (ci))),
					     str);
		g_free (str);
	}

	return FALSE;
}

static gboolean
single_item_leave_notify_event_cb (G_GNUC_UNUSED GooCanvasItem *ci, G_GNUC_UNUSED GooCanvasItem *target_item,
				   G_GNUC_UNUSED GdkEventCrossing *event, BrowserCanvasFkey *cc)
{
	gint i;

	for (i = 0; i < cc->priv->fk->cols_nb; i++) {
		GdaMetaTableColumn *tcol;
		BrowserCanvasColumn *column;

		/* fk column */
		tcol = g_slist_nth_data (GDA_META_TABLE (cc->priv->fk->meta_table)->columns, 
					 cc->priv->fk->fk_cols_array[i] - 1);

		column = browser_canvas_table_get_column_item (cc->priv->fk_table_item, tcol);
		browser_canvas_text_set_highlight (BROWSER_CANVAS_TEXT (column), FALSE);
		
		/* ref pk column */
		tcol = g_slist_nth_data (GDA_META_TABLE (cc->priv->fk->depend_on)->columns, 
					 cc->priv->fk->ref_pk_cols_array[i] - 1);

		column = browser_canvas_table_get_column_item (cc->priv->ref_pk_table_item, tcol);
		browser_canvas_text_set_highlight (BROWSER_CANVAS_TEXT (column), FALSE);
	}

	return FALSE;
}

static void
delete_declared_fk_cb (G_GNUC_UNUSED GtkMenuItem *mitem, BrowserCanvasFkey *cc)
{
	GError *error = NULL;
	GtkWidget *parent;
	parent = (GtkWidget*) gtk_widget_get_toplevel ((GtkWidget*) goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (cc)));
	if (! fk_declare_undeclare (cc->priv->mstruct,
				    BROWSER_IS_WINDOW (parent) ? BROWSER_WINDOW (parent) : NULL,
				    cc->priv->fk, &error)) {
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

static gboolean
single_item_button_press_event_cb (G_GNUC_UNUSED GooCanvasItem *ci, G_GNUC_UNUSED GooCanvasItem *target_item,
				   G_GNUC_UNUSED GdkEventButton *event, BrowserCanvasFkey *cc)
{
	GdaMetaTableForeignKey *fk = g_object_get_data (G_OBJECT (ci), "fkcons");
	if (GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (fk)) {
		GtkWidget *menu, *entry;
		
		menu = gtk_menu_new ();
		entry = gtk_menu_item_new_with_label (_("Remove this declared foreign key"));
		g_object_set_data (G_OBJECT (entry), "fkcons", fk);
		g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (delete_declared_fk_cb), cc);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), entry);
		gtk_widget_show (entry);
		gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
		return TRUE;
	}
	else
		return FALSE;
}

static void
table_item_moved_cb (G_GNUC_UNUSED GooCanvasItem *table, BrowserCanvasFkey *cc)
{
	update_items (cc);
}

/**
 * browser_canvas_fkey_new
 * @parent: the parent item, or NULL. 
 * @fkcons: the #GdaMetaTableForeignKey to represent
 * @...: optional pairs of property names and values, and a terminating NULL.
 *
 * Creates a new canvas item to represent the @fkcons FK constraint
 *
 * Returns: a new #GooCanvasItem object
 */
GooCanvasItem *
browser_canvas_fkey_new (GooCanvasItem *parent, GdaMetaStruct *mstruct, GdaMetaTableForeignKey *fkcons, ...)
{
	GooCanvasItem *item;
	const char *first_property;
	va_list var_args;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);

	item = g_object_new (TYPE_BROWSER_CANVAS_FKEY, "meta-struct", mstruct, NULL);

	if (parent) {
		goo_canvas_item_add_child (parent, item, -1);
		g_object_unref (item);
	}

	g_object_set (item, "fk_constraint", fkcons, NULL);

	va_start (var_args, fkcons);
	first_property = va_arg (var_args, char*);
	if (first_property)
		g_object_set_valist ((GObject*) item, first_property, var_args);
	va_end (var_args);

	return item;
}
