/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "mgr-columns.h"
#include "../ui-support.h"

struct _MgrColumnsPriv {
	TConnection  *tcnc;
	gchar *schema;
	gchar *table_name;
};

static void mgr_columns_class_init (MgrColumnsClass *klass);
static void mgr_columns_init       (MgrColumns *tmgr1, MgrColumnsClass *klass);
static void mgr_columns_dispose    (GObject *object);
static void mgr_columns_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec);
static void mgr_columns_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);

/* virtual methods */
static GSList *mgr_columns_update_children (GdaTreeManager *manager, GdaTreeNode *node, 
					    const GSList *children_nodes, gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/* properties */
enum {
        PROP_0,
	PROP_BROWSER_CNC
};

/*
 * MgrColumns class implementation
 * @klass:
 */
static void
mgr_columns_class_init (MgrColumnsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = mgr_columns_update_children;

	/* Properties */
        object_class->set_property = mgr_columns_set_property;
        object_class->get_property = mgr_columns_get_property;

	g_object_class_install_property (object_class, PROP_BROWSER_CNC,
                                         g_param_spec_object ("browser-connection", NULL, "Connection to use",
                                                              T_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	object_class->dispose = mgr_columns_dispose;
}

static void
mgr_columns_init (MgrColumns *mgr, G_GNUC_UNUSED MgrColumnsClass *klass)
{
	mgr->priv = g_new0 (MgrColumnsPriv, 1);
}

static void
mgr_columns_dispose (GObject *object)
{
	MgrColumns *mgr = (MgrColumns *) object;

	if (mgr->priv) {
		if (mgr->priv->tcnc)
			g_object_unref (mgr->priv->tcnc);
		g_free (mgr->priv->schema);
		g_free (mgr->priv->table_name);

		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/* module error */
GQuark mgr_columns_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("mgr_columns_error");
        return quark;
}

/**
 * mgr_columns_get_type
 *
 * Returns: the GType
 */
GType
mgr_columns_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (MgrColumnsClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) mgr_columns_class_init,
                        NULL,
                        NULL,
                        sizeof (MgrColumns),
                        0,
                        (GInstanceInitFunc) mgr_columns_init,
			0
                };

                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "MgrColumns", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

static void
mgr_columns_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
        MgrColumns *mgr;

        mgr = MGR_COLUMNS (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_BROWSER_CNC:
			mgr->priv->tcnc = (TConnection*) g_value_get_object (value);
			if (mgr->priv->tcnc)
				g_object_ref (mgr->priv->tcnc);
			
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
mgr_columns_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
        MgrColumns *mgr;

        mgr = MGR_COLUMNS (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_BROWSER_CNC:
			g_value_set_object (value, mgr->priv->tcnc);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

/**
 * mgr_columns_new
 * @tcnc: a #TConnection object
 * @schema: the schema the table is in
 * @name: the table's name
 *
 * Creates a new #GdaTreeManager object which will add one tree node for column found in the
 * table @schema.@table
 *
 * Returns: a new #GdaTreeManager object
 */
GdaTreeManager*
mgr_columns_new (TConnection *tcnc, const gchar *schema, const gchar *table)
{
	MgrColumns *mgr;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (schema, NULL);
	g_return_val_if_fail (table, NULL);

	mgr = (MgrColumns*) g_object_new (MGR_COLUMNS_TYPE,
					  "browser-connection", tcnc, NULL);
	mgr->priv->schema = g_strdup (schema);
	mgr->priv->table_name = g_strdup (table);

	return (GdaTreeManager*) mgr;
}


/*
 * Build a hash where key = column's name as a string, and value = GdaTreeNode
 */
static GHashTable *
hash_for_existing_nodes (const GSList *nodes)
{
	GHashTable *hash;
	const GSList *list;

	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (list = nodes; list; list = list->next) {
		const GValue *cvalue;
		cvalue = gda_tree_node_get_node_attribute ((GdaTreeNode*) list->data, MGR_COLUMNS_COL_NAME_ATT_NAME);
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING)) {
			const gchar *str = g_value_get_string (cvalue);
			if (str)
				g_hash_table_insert (hash, (gpointer) str, list->data);
		}
	}
	return hash;
}

static gboolean
column_is_fk_part (GdaMetaDbObject *dbo, gint index)
{
	GSList *fklist;
	
	for (fklist = GDA_META_TABLE (dbo)->fk_list; fklist; fklist = fklist->next) {
		GdaMetaTableForeignKey *fk;
		gint i;
		fk = (GdaMetaTableForeignKey *) fklist->data;
		for (i = 0; i < fk->cols_nb; i++) {
			if (fk->fk_cols_array [i] == index + 1)
				return TRUE;
		}
	}
	return FALSE;
}

static GSList *
mgr_columns_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
			     gboolean *out_error, GError **error)
{
	MgrColumns *mgr = MGR_COLUMNS (manager);
	GSList *nodes_list = NULL;
	GHashTable *ehash = NULL;

	if (out_error)
		*out_error = FALSE;

	if (children_nodes)
		ehash = hash_for_existing_nodes (children_nodes);

	GdaMetaStruct *mstruct;
	mstruct = t_connection_get_meta_struct (mgr->priv->tcnc);
	if (!mstruct) {
		g_set_error (error, MGR_COLUMNS_ERROR, MGR_COLUMNS_NO_META_STRUCT,
                             "%s", _("Not ready"));
                if (out_error)
                        *out_error = TRUE;
		return NULL;
	}

	GdaMetaDbObject *dbo;
	GValue *schema_v = NULL, *name_v;
	if (mgr->priv->schema)
		g_value_set_string ((schema_v = gda_value_new (G_TYPE_STRING)), mgr->priv->schema);
	g_value_set_string ((name_v = gda_value_new (G_TYPE_STRING)), mgr->priv->table_name);
	dbo = gda_meta_struct_get_db_object (mstruct, NULL, schema_v, name_v);
	if (schema_v)
		gda_value_free (schema_v);
	gda_value_free (name_v);

	if (!dbo) {
		g_set_error (error, MGR_COLUMNS_ERROR, MGR_COLUMNS_TABLE_NOT_FOUND,
                             "%s", _("Table not found"));
                if (out_error)
                        *out_error = TRUE;
		return NULL;
	}
	if ((dbo->obj_type != GDA_META_DB_TABLE) && (dbo->obj_type != GDA_META_DB_VIEW)) {
		g_set_error (error, MGR_COLUMNS_ERROR, MGR_COLUMNS_WRONG_OBJ_TYPE,
                             "%s", _("Requested object is not a table or view"));
                if (out_error)
                        *out_error = TRUE;
		return NULL;
	}
	
	GdaMetaTable *dbotable;
	GSList *columns;
	gint index;
	dbotable = GDA_META_TABLE (dbo);
	
	for (columns = dbotable->columns, index = 0;
	     columns;
	     columns = columns->next, index ++) {
		GdaMetaTableColumn *col;
		col = GDA_META_TABLE_COLUMN (columns->data);
		GdaTreeNode* snode = NULL;
		GValue *av;

		if (ehash)
			snode = g_hash_table_lookup (ehash, col->column_name);
		if (snode) {
			/* use the same node */
			g_object_ref (G_OBJECT (snode));
		}
		else {
			/* column's name */
			snode = gda_tree_manager_create_node (manager, node, NULL);
			if (col->pkey)
				g_value_take_string ((av = gda_value_new (G_TYPE_STRING)),
						     g_strdup_printf ("<b>%s</b>", col->column_name));
			else
				g_value_set_string ((av = gda_value_new (G_TYPE_STRING)), col->column_name);
			gda_tree_node_set_node_attribute (snode, MGR_COLUMNS_COL_NAME_ATT_NAME, av, NULL);
			gda_value_free (av);
		}
			
		/* column's type */
		g_value_set_string ((av = gda_value_new (G_TYPE_STRING)), col->column_type);
		gda_tree_node_set_node_attribute (snode, MGR_COLUMNS_COL_TYPE_ATT_NAME, av, NULL);
		gda_value_free (av);
		
		/* NOT NULL */
		g_value_set_boolean ((av = gda_value_new (G_TYPE_BOOLEAN)), !col->nullok);
		gda_tree_node_set_node_attribute (snode, MGR_COLUMNS_COL_NOTNULL_ATT_NAME, av, NULL);
		gda_value_free (av);
		
		/* default value */
		g_value_set_string ((av = gda_value_new (G_TYPE_STRING)), col->default_value);
		gda_tree_node_set_node_attribute (snode, MGR_COLUMNS_COL_DEFAULT_ATT_NAME, av, NULL);
		gda_value_free (av);
		
		/* icon */
		UiIconType type = UI_ICON_COLUMN;
		GdkPixbuf *pixbuf;
		gboolean is_fk = column_is_fk_part (dbo, index);
		if (col->pkey)
			type = UI_ICON_COLUMN_PK;
		else if (!col->nullok) {
			if (is_fk)
				type = UI_ICON_COLUMN_FK_NN;
			else
				type = UI_ICON_COLUMN_NN;
		}
		else if (is_fk)
			type = UI_ICON_COLUMN_FK;
		
		pixbuf = ui_get_pixbuf_icon (type);
		av = gda_value_new (G_TYPE_OBJECT);
		g_value_set_object (av, pixbuf);
		gda_tree_node_set_node_attribute (snode, "icon", av, NULL);
		gda_value_free (av);

		/* details */
		GString *details = NULL;
		if (col->pkey)
			details = g_string_new (_("Primary key"));
		if (is_fk) {
			if (details)
				g_string_append (details, ", ");
			else
				details = g_string_new ("");
			g_string_append (details, _("Foreign key"));
		}
		if (col->auto_incement) {
			if (details)
				g_string_append (details, ", ");
			else
				details = g_string_new ("");
			g_string_append (details, _("Auto incremented"));
		}
		if (details) {
			g_value_set_string ((av = gda_value_new (G_TYPE_STRING)), details->str);
			gda_tree_node_set_node_attribute (snode, MGR_COLUMNS_COL_DETAILS, av, NULL);
			gda_value_free (av);
			g_string_free (details, TRUE);
		}

		nodes_list = g_slist_prepend (nodes_list, snode);		
	}

	if (ehash)
		g_hash_table_destroy (ehash);

	return g_slist_reverse (nodes_list);
}
