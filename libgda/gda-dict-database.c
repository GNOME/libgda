/* gda-dict-database.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-dict-database.h"
#include "gda-dict-table.h"
#include "gda-dict-field.h"
#include "gda-dict-constraint.h"
#include "gda-entity.h"
#include "gda-xml-storage.h"
#include <libgda/gda-util.h>
#include "gda-marshal.h"
#include "gda-connection.h"

/* 
 * Main static functions 
 */
static void gda_dict_database_class_init (GdaDictDatabaseClass * class);
static void gda_dict_database_init (GdaDictDatabase * srv);
static void gda_dict_database_dispose (GObject   * object);
static void gda_dict_database_finalize (GObject   * object);

static void gda_dict_database_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_database_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

static void        gda_dict_database_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gda_dict_database_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gda_dict_database_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_dict_database_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

static void gda_dict_database_add_table (GdaDictDatabase *mgdb, GdaDictTable *table, gint pos);
static void gda_dict_database_add_sequence (GdaDictDatabase *mgdb, GdaDictSequence *seq, gint pos);

static void destroyed_table_cb (GdaDictTable *table, GdaDictDatabase *mgdb);
static void destroyed_sequence_cb (GdaDictSequence *seq, GdaDictDatabase *mgdb);
static void destroyed_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *mgdb);

static void updated_table_cb (GdaDictTable *table, GdaDictDatabase *mgdb);
static void table_field_added_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *mgdb);
static void table_field_updated_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *mgdb);
static void table_field_removed_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *mgdb);
static void updated_sequence_cb (GdaDictSequence *seq, GdaDictDatabase *mgdb);
static void updated_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *mgdb);

#ifdef GDA_DEBUG
static void gda_dict_database_dump (GdaDictDatabase *mgdb, gint offset);
#endif

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	DATA_UPDATE_STARTED,
	DATA_UPDATE_FINISHED,
	UPDATE_PROGRESS,
	TABLE_ADDED,
	TABLE_REMOVED,
	TABLE_UPDATED,
	FIELD_ADDED,
	FIELD_REMOVED,
	FIELD_UPDATED,
	SEQUENCE_ADDED,
	SEQUENCE_REMOVED,
	SEQUENCE_UPDATED,
	CONSTRAINT_ADDED,
	CONSTRAINT_REMOVED,
	CONSTRAINT_UPDATED,
	FS_LINK_ADDED,
	FS_LINK_REMOVED,
	LAST_SIGNAL
};

static gint gda_dict_database_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP
};


/* private structure */
struct _GdaDictDatabasePrivate
{
	/* Db structure */
	GSList                 *tables;
	GSList                 *sequences;
	GSList                 *constraints;
	GHashTable             *constraints_hash; /* key=table, value=GSList of constraints on that table */
	GHashTable             *tables_hash;

	/* XML loading attributes */
	gboolean                xml_loading;

	/* DBMS update related information */
	gboolean                update_in_progress;
	gboolean                stop_update; /* TRUE if a DBMS data update must be stopped */

	/* the names are set to lower case before any comparison */
	gboolean                lc_names;
};


/* module error */
GQuark gda_dict_database_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_database_error");
	return quark;
}

/**
 * gda_dict_database_get_type
 *
 * Returns: the type id
 */
GType
gda_dict_database_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDictDatabaseClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_database_class_init,
			NULL,
			NULL,
			sizeof (GdaDictDatabase),
			0,
			(GInstanceInitFunc) gda_dict_database_init
		};
		

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_dict_database_xml_storage_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictDatabase", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
	}
	return type;
}

static void 
gda_dict_database_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = gda_dict_database_get_xml_id;
	iface->save_to_xml = gda_dict_database_save_to_xml;
	iface->load_from_xml = gda_dict_database_load_from_xml;
}

static void
gda_dict_database_class_init (GdaDictDatabaseClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

        gda_dict_database_signals[TABLE_ADDED] =
                g_signal_new ("table_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, table_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_dict_database_signals[TABLE_REMOVED] =
                g_signal_new ("table_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, table_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_dict_database_signals[TABLE_UPDATED] =
                g_signal_new ("table_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, table_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[FIELD_ADDED] =
                g_signal_new ("field_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, field_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[FIELD_REMOVED] =
                g_signal_new ("field_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, field_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[FIELD_UPDATED] =
                g_signal_new ("field_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, field_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[SEQUENCE_ADDED] =
                g_signal_new ("sequence_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, sequence_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[SEQUENCE_REMOVED] =
                g_signal_new ("sequence_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, sequence_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[SEQUENCE_UPDATED] =
                g_signal_new ("sequence_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, sequence_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[CONSTRAINT_ADDED] =
                g_signal_new ("constraint_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, constraint_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[CONSTRAINT_REMOVED] =
                g_signal_new ("constraint_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, constraint_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        gda_dict_database_signals[CONSTRAINT_UPDATED] =
                g_signal_new ("constraint_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, constraint_updated),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
	gda_dict_database_signals[FS_LINK_ADDED] =
                g_signal_new ("fs_link_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, fs_link_added),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER_POINTER,
                              G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	gda_dict_database_signals[FS_LINK_REMOVED] =
                g_signal_new ("fs_link_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, fs_link_removed),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER_POINTER,
                              G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	gda_dict_database_signals[DATA_UPDATE_STARTED] =
                g_signal_new ("data_update_started",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, data_update_started),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        gda_dict_database_signals[UPDATE_PROGRESS] =
                g_signal_new ("update_progress",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, update_progress),
                              NULL, NULL,
                              gda_marshal_VOID__POINTER_UINT_UINT,
                              G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT);
	gda_dict_database_signals[DATA_UPDATE_FINISHED] =
                g_signal_new ("data_update_finished",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, data_update_finished),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

	class->table_added = NULL;
	class->table_removed = NULL;
	class->table_updated = NULL;
	class->field_added = NULL;
	class->field_removed = NULL;
	class->field_updated = NULL;
	class->sequence_added = NULL;
	class->sequence_removed = NULL;
	class->sequence_updated = NULL;
	class->constraint_added = NULL;
	class->constraint_removed = NULL;
	class->constraint_updated = NULL;
	class->fs_link_added = NULL;
	class->fs_link_removed = NULL;
        class->data_update_started = NULL;
        class->data_update_finished = NULL;
        class->update_progress = NULL;

	object_class->dispose = gda_dict_database_dispose;
	object_class->finalize = gda_dict_database_finalize;

	/* Properties */
	object_class->set_property = gda_dict_database_set_property;
	object_class->get_property = gda_dict_database_get_property;
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_dict_database_dump;
#endif

}


static void
gda_dict_database_init (GdaDictDatabase * gda_dict_database)
{
	gda_dict_database->priv = g_new0 (GdaDictDatabasePrivate, 1);
	gda_dict_database->priv->tables = NULL;
        gda_dict_database->priv->sequences = NULL;
        gda_dict_database->priv->constraints = NULL;
	gda_dict_database->priv->constraints_hash = g_hash_table_new (NULL, NULL);

	gda_dict_database->priv->tables_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	gda_dict_database->priv->xml_loading = FALSE;

	gda_dict_database->priv->update_in_progress = FALSE;
	gda_dict_database->priv->stop_update = FALSE;

	gda_dict_database->priv->lc_names = TRUE;
}


/**
 * gda_dict_database_new
 * @dict: a #GdaDict object
 * 
 * Creates a new GdaDictDatabase object
 *
 * Returns: the new object
 */
GObject   *
gda_dict_database_new (GdaDict *dict)
{
	GObject   *obj;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_DICT_DATABASE, "dict", ASSERT_DICT (dict), NULL);

	return obj;
}


static void
constraints_hash_foreach (GdaDictTable *table, GSList *constraints, gpointer data)
{
	g_slist_free (constraints);
}

static void
gda_dict_database_dispose (GObject *object)
{
	GdaDictDatabase *gda_dict_database;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_DATABASE (object));

	gda_dict_database = GDA_DICT_DATABASE (object);
	if (gda_dict_database->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (gda_dict_database->priv->tables_hash) {
			g_hash_table_destroy (gda_dict_database->priv->tables_hash);
			gda_dict_database->priv->tables_hash = NULL;
		}

		if (gda_dict_database->priv->constraints_hash) {
			g_hash_table_foreach (gda_dict_database->priv->constraints_hash, (GHFunc) constraints_hash_foreach, NULL);
			g_hash_table_destroy (gda_dict_database->priv->constraints_hash);
			gda_dict_database->priv->constraints_hash = NULL;
		}

		/* getting rid of the constraints */
		while (gda_dict_database->priv->constraints) 
			gda_object_destroy (GDA_OBJECT (gda_dict_database->priv->constraints->data));

		/* getting rid of the sequences */
		while (gda_dict_database->priv->sequences) 
			gda_object_destroy (GDA_OBJECT (gda_dict_database->priv->sequences->data));
		
		/* getting rid of the tables */
		while (gda_dict_database->priv->tables) 
			gda_object_destroy (GDA_OBJECT (gda_dict_database->priv->tables->data));
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_database_finalize (GObject *object)
{
	GdaDictDatabase *gda_dict_database;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_DATABASE (object));

	gda_dict_database = GDA_DICT_DATABASE (object);
	if (gda_dict_database->priv) {
		g_free (gda_dict_database->priv);
		gda_dict_database->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_database_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	gpointer ptr;
	GdaDictDatabase *gda_dict_database;

	gda_dict_database = GDA_DICT_DATABASE (object);
	if (gda_dict_database->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			ptr = g_value_get_pointer (value);
			break;
		}
	}
}
static void
gda_dict_database_get_property (GObject *object,
			guint param_id,
			GValue *value,
			GParamSpec *pspec)
{
	GdaDictDatabase *gda_dict_database;
	gda_dict_database = GDA_DICT_DATABASE (object);
	
	if (gda_dict_database->priv) {
		switch (param_id) {
		case PROP:
			/* FIXME */
			g_value_set_pointer (value, NULL);
			break;
		}	
	}
}


/* GdaXmlStorage interface implementation */
static gchar *
gda_dict_database_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_DATABASE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_DATABASE (iface)->priv, NULL);

	return NULL;
}

static xmlNodePtr
gda_dict_database_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr toptree, tree;
	GdaDictDatabase *mgdb;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_DICT_DATABASE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_DATABASE (iface)->priv, NULL);

	mgdb = GDA_DICT_DATABASE (iface);

	/* main node */
        toptree = xmlNewNode (NULL, "gda_dict_database");
	
	/* Tables */
	tree = xmlNewChild (toptree, NULL, "gda_dict_tables", NULL);
	list = mgdb->priv->tables;
	while (list) {
		xmlNodePtr table;
		
		table = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
				
		if (table)
			xmlAddChild (tree, table);
		else {
			xmlFreeNode (tree);
			return NULL;
		}

		list = g_slist_next (list);
	}

	/* Sequences */
	if (mgdb->priv->sequences) {
		tree = xmlNewChild (toptree, NULL, "gda_dict_sequences", NULL);
		list = mgdb->priv->sequences;
		while (list) {
			xmlNodePtr table;
			
			table = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
			
			if (table)
				xmlAddChild (tree, table);
			else {
				xmlFreeNode (tree);
				return NULL;
			}
			
			list = g_slist_next (list);
		}
	}

	/* Constraints */
	tree = xmlNewChild (toptree, NULL, "gda_dict_constraints", NULL);
	list = mgdb->priv->constraints;
	while (list) {
		xmlNodePtr cstr;
		
		cstr = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), NULL);
				
		if (cstr)
			xmlAddChild (tree, cstr); /* otherwise, just ignore the error */
#ifdef GDA_DEBUG
		else
			g_print (D_COL_ERR "ERROR\n" D_COL_NOR);
#endif
		list = g_slist_next (list);
	}

	return toptree;
}

static gboolean gda_dict_database_load_from_xml_tables (GdaXmlStorage *iface, xmlNodePtr node, GError **error);
static gboolean gda_dict_database_load_from_xml_constraints (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

static gboolean
gda_dict_database_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDictDatabase *mgdb;
	xmlNodePtr subnode;

	g_return_val_if_fail (iface && GDA_IS_DICT_DATABASE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_DATABASE (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	mgdb = GDA_DICT_DATABASE (iface);

	if (mgdb->priv->tables || mgdb->priv->sequences || mgdb->priv->constraints) {
		g_set_error (error,
			     GDA_DICT_DATABASE_ERROR,
			     GDA_DICT_DATABASE_XML_LOAD_ERROR,
			     _("Database already contains data"));
		return FALSE;
	}
	if (strcmp (node->name, "gda_dict_database")) {
		g_set_error (error,
			     GDA_DICT_DATABASE_ERROR,
			     GDA_DICT_DATABASE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_database>"));
		return FALSE;
	}
	mgdb->priv->xml_loading = TRUE;
	subnode = node->children;
	while (subnode) {
		gboolean done = FALSE;

		if (!strcmp (subnode->name, "gda_dict_tables")) {
			if (!gda_dict_database_load_from_xml_tables (iface, subnode, error)) {
				mgdb->priv->xml_loading = FALSE;
				return FALSE;
			}

			done = TRUE;
		}

		if (!done && !strcmp (subnode->name, "gda_dict_sequences")) {
			TO_IMPLEMENT;
			done = TRUE;
		}

		if (!done && !strcmp (subnode->name, "gda_dict_constraints")) {
			if (!gda_dict_database_load_from_xml_constraints (iface, subnode, error)) {
				mgdb->priv->xml_loading = FALSE;
				return FALSE;
			}

			done = TRUE;
		}

		subnode = subnode->next;
	}
	mgdb->priv->xml_loading = FALSE;
	
	return TRUE;
}

static gboolean
gda_dict_database_load_from_xml_tables (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	gboolean allok = TRUE;
	xmlNodePtr subnode = node->children;

	while (subnode && allok) {
		if (! xmlNodeIsText (subnode)) {
			if (!strcmp (subnode->name, "gda_dict_table")) {
				GdaDictTable *table;
				
				table = GDA_DICT_TABLE (gda_dict_table_new (gda_object_get_dict (GDA_OBJECT (iface))));
				allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (table), subnode, error);
				if (allok)
					gda_dict_database_add_table (GDA_DICT_DATABASE (iface), table, -1);
				g_object_unref (G_OBJECT (table));
			}
			else {
				allok = FALSE;
				g_set_error (error,
					     GDA_DICT_DATABASE_ERROR,
					     GDA_DICT_DATABASE_XML_LOAD_ERROR,
					     _("XML Tag below <GDA_DICT_TABLES> is not <GDA_DICT_TABLE>"));
			}
		}			

		subnode = subnode->next;
	}	

	return allok;
}

static void gda_dict_database_add_constraint_real (GdaDictDatabase *mgdb, GdaDictConstraint *cstr, gboolean force_user_constraint);
static gboolean
gda_dict_database_load_from_xml_constraints (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	gboolean allok = TRUE;
	xmlNodePtr subnode = node->children;

	while (subnode && allok) {
		if (! xmlNodeIsText (subnode)) {
			if (!strcmp (subnode->name, "gda_dict_constraint")) {
				GdaDictConstraint *cstr;
				
				cstr = GDA_DICT_CONSTRAINT (gda_dict_constraint_new_with_db (GDA_DICT_DATABASE (iface)));
				allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (cstr), subnode, error);
				gda_dict_database_add_constraint_real (GDA_DICT_DATABASE (iface), cstr, FALSE);
				g_object_unref (G_OBJECT (cstr));
			}
			else {
				allok = FALSE;
				g_set_error (error,
					     GDA_DICT_DATABASE_ERROR,
					     GDA_DICT_DATABASE_XML_LOAD_ERROR,
					     _("XML Tag below <GDA_DICT_CONSTRAINTS> is not <GDA_DICT_CONSTRAINT>"));
			}
		}			

		subnode = subnode->next;
	}	

	return allok;
}

/*
 * pos = -1 to append the table to the list
 */
static void
gda_dict_database_add_table (GdaDictDatabase *mgdb, GdaDictTable *table, gint pos)
{
	gchar *str;
	g_return_if_fail (table);
	g_return_if_fail (!g_slist_find (mgdb->priv->tables, table));

	g_object_set (G_OBJECT (table), "database", mgdb, NULL);
	mgdb->priv->tables = g_slist_insert (mgdb->priv->tables, table, pos);

	g_object_ref (G_OBJECT (table));
	gda_object_connect_destroy (table, G_CALLBACK (destroyed_table_cb), mgdb);

	g_signal_connect (G_OBJECT (table), "changed",
			  G_CALLBACK (updated_table_cb), mgdb);
	g_signal_connect (G_OBJECT (table), "field_added",
			  G_CALLBACK (table_field_added_cb), mgdb);
	g_signal_connect (G_OBJECT (table), "field_updated",
			  G_CALLBACK (table_field_updated_cb), mgdb);
	g_signal_connect (G_OBJECT (table), "field_removed",
			  G_CALLBACK (table_field_removed_cb), mgdb);

	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (table));
	g_hash_table_insert (mgdb->priv->tables_hash, str, table);

#ifdef GDA_DEBUG_signal
	g_print (">> 'TABLE_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (mgdb), gda_dict_database_signals[TABLE_ADDED], 0, table);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TABLE_ADDED' from %s\n", __FUNCTION__);
#endif
}

/*
 * pos = -1 to append the table to the list
 */
static void
gda_dict_database_add_sequence (GdaDictDatabase *mgdb, GdaDictSequence *seq, gint pos)
{
	g_return_if_fail (seq);
	g_return_if_fail (!g_slist_find (mgdb->priv->sequences, seq));

	g_object_set (G_OBJECT (seq), "database", mgdb, NULL);
	mgdb->priv->sequences = g_slist_insert (mgdb->priv->sequences, seq, pos);

	g_object_ref (G_OBJECT (seq));
	gda_object_connect_destroy (seq, G_CALLBACK (destroyed_sequence_cb), mgdb);
	g_signal_connect (G_OBJECT (seq), "changed",
			  G_CALLBACK (updated_sequence_cb), mgdb);


#ifdef GDA_DEBUG_signal
	g_print (">> 'SEQUENCE_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (mgdb), gda_dict_database_signals[SEQUENCE_ADDED], 0, seq);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'SEQUENCE_ADDED' from %s\n", __FUNCTION__);
#endif
}

/**
 * gda_dict_database_add_constraint
 * @mgdb: a #GdaDictDatabase object
 * @cstr: a #GdaDictConstraint
 *
 * Add the @cstr constraint to the database. The @cstr constraint is a user-defined constraint
 * (which is not part of the database structure itself).
 */
void
gda_dict_database_add_constraint (GdaDictDatabase *mgdb, GdaDictConstraint *cstr)
{
	gda_dict_database_add_constraint_real (mgdb, cstr, TRUE);
}

static void
gda_dict_database_add_constraint_real (GdaDictDatabase *mgdb, GdaDictConstraint *cstr, gboolean force_user_constraint)
{
	GdaDictConstraint *ptr = NULL;

	g_return_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb));
	g_return_if_fail (mgdb->priv);
	g_return_if_fail (cstr);

	/* Try to activate the constraints here */
	gda_referer_activate (GDA_REFERER (cstr));

	/* try to find if a similar constraint is there, if we are not loading from an XML file */
	if (!mgdb->priv->xml_loading) {
		GSList *list = mgdb->priv->constraints;
		while (list && !ptr) {
			if (gda_dict_constraint_equal (cstr, GDA_DICT_CONSTRAINT (list->data)))
				ptr = GDA_DICT_CONSTRAINT (list->data);
			list = g_slist_next (list);
		}
	}

	if (ptr) {
		/* update the existing constraint with name and description */
		gda_object_set_name (GDA_OBJECT (ptr), gda_object_get_name (GDA_OBJECT (cstr)));
		gda_object_set_description (GDA_OBJECT (ptr), gda_object_get_description (GDA_OBJECT (cstr)));
		gda_object_set_owner (GDA_OBJECT (ptr), gda_object_get_owner (GDA_OBJECT (cstr)));
	}
	else {
		GSList *list;
		GdaDictTable *table;

		/* user defined constraint */
		if (force_user_constraint)
			g_object_set (G_OBJECT (cstr), "user_constraint", TRUE, NULL);

		/* add @cstr to the list of constraints */
		mgdb->priv->constraints = g_slist_append (mgdb->priv->constraints, cstr);
		g_object_ref (G_OBJECT (cstr));
		gda_object_connect_destroy (cstr, G_CALLBACK (destroyed_constraint_cb), mgdb);
		g_signal_connect (G_OBJECT (cstr), "changed",
				  G_CALLBACK (updated_constraint_cb), mgdb);

		/* add the constraint to the 'constraints_hash' */
		table = gda_dict_constraint_get_table (cstr);
		list = g_hash_table_lookup (mgdb->priv->constraints_hash, table);
		list = g_slist_append (list, cstr);
		g_hash_table_insert (mgdb->priv->constraints_hash, table, list);

#ifdef GDA_DEBUG_signal
		g_print (">> 'CONSTRAINT_ADDED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (mgdb), gda_dict_database_signals[CONSTRAINT_ADDED], 0, cstr);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'CONSTRAINT_ADDED' from %s\n", __FUNCTION__);
#endif
	}
}

#ifdef GDA_DEBUG
static void 
gda_dict_database_dump (GdaDictDatabase *mgdb, gint offset)
{
	gchar *str;
	guint i;
	GSList *list;

	g_return_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb));
	
	/* string for the offset */
	str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;
	
	/* dump */
	if (mgdb->priv) 
		g_print ("%s" D_COL_H1 "GdaDictDatabase %p\n" D_COL_NOR, str, mgdb);
	else
		g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, mgdb);

	/* tables */
	list = mgdb->priv->tables;
	if (list) {
		g_print ("%sTables:\n", str);
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}
	}
	else
		g_print ("%sNo Table defined\n", str);

	/* sequences */
	list = mgdb->priv->sequences;
	if (list) {
		g_print ("%sSequences:\n", str);
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}
	}
	else
		g_print ("%sNo Sequence defined\n", str);


	/* constraints */
	list = mgdb->priv->constraints;
	if (list) {
		g_print ("%sConstraints:\n", str);
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}
	}
	else
		g_print ("%sNo Constraint defined\n", str);


	g_free (str);

}
#endif


/**
 * gda_dict_database_get_dict
 * @mgdb: a #GdaDictDatabase object
 *
 * Fetch the GdaDict object to which the GdaDictDatabase belongs.
 *
 * Returns: the GdaDict object
 */
GdaDict *
gda_dict_database_get_dict (GdaDictDatabase *mgdb)
{
	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);

	return gda_object_get_dict (GDA_OBJECT (mgdb));
}



static gboolean database_tables_update_list (GdaDictDatabase * mgdb, GError **error);
static gboolean database_sequences_update_list (GdaDictDatabase * mgdb, GError **error);
static gboolean database_constraints_update_list (GdaDictDatabase * mgdb, GError **error);
/**
 * gda_dict_database_update_dbms_data
 * @mgdb: a #GdaDictDatabase object
 * @error: location to store error, or %NULL
 *
 * Synchronises the Database representation with the database structure which is stored in
 * the DBMS. For this operation to succeed, the connection to the DBMS server MUST be opened
 * (using the corresponding #GdaConnection object).
 *
 * Returns: TRUE if no error
 */
gboolean
gda_dict_database_update_dbms_data (GdaDictDatabase *mgdb, GError **error)
{
	gboolean retval = TRUE;
	GdaConnection *cnc;
	GdaDict *dict;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), FALSE);
	g_return_val_if_fail (mgdb->priv, FALSE);
	
	if (mgdb->priv->update_in_progress) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_META_DATA_UPDATE,
			     _("Update already started!"));
		return FALSE;
	}

	dict = gda_object_get_dict (GDA_OBJECT (mgdb));
	cnc = gda_dict_get_connection (dict);
	if (!cnc) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("No connection associated to dictionary!"));
                return FALSE;
	}
	if (!gda_connection_is_open (cnc)) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_DATABASE_META_DATA_UPDATE,
			     _("Connection is not opened!"));
		return FALSE;
	}

	mgdb->priv->update_in_progress = TRUE;
	mgdb->priv->stop_update = FALSE;

#ifdef GDA_DEBUG_signal
	g_print (">> 'DATA_UPDATE_STARTED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (mgdb), gda_dict_database_signals[DATA_UPDATE_STARTED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'DATA_UPDATE_STARTED' from %s\n", __FUNCTION__);
#endif

	retval = database_tables_update_list (mgdb, error);
	if (retval && !mgdb->priv->stop_update) 
		retval = database_sequences_update_list (mgdb, error);
	if (retval && !mgdb->priv->stop_update) 
		retval = database_constraints_update_list (mgdb, error);

#ifdef GDA_DEBUG_signal
	g_print (">> 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (mgdb), gda_dict_database_signals[DATA_UPDATE_FINISHED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif

	mgdb->priv->update_in_progress = FALSE;
	if (mgdb->priv->stop_update) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_META_DATA_UPDATE_USER_STOPPED,
			     _("Update stopped!"));
		return FALSE;
	}

	return retval;
}


/**
 * gda_dict_database_stop_update_dbms_data
 * @mgdb: a #GdaDictDatabase object
 *
 * When the database updates its internal lists of DBMS objects, a call to this function will 
 * stop that update process. It has no effect when the database is not updating its DBMS data.
 */
void
gda_dict_database_stop_update_dbms_data (GdaDictDatabase *mgdb)
{
	g_return_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb));
	g_return_if_fail (mgdb->priv);

	mgdb->priv->stop_update = TRUE;
}

static gboolean
database_tables_update_list (GdaDictDatabase *mgdb, GError **error)
{
	GSList *list;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *updated_tables = NULL;
	GdaConnection *cnc;
	GdaDictTable *table;
	GSList *constraints;

	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (mgdb)));
	if (!cnc) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("No connection associated to dictionary!"));
                return FALSE;
	}
	if (!gda_connection_is_open (cnc)) {
                g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("Connection is not opened!"));
                return FALSE;
        }

	rs = gda_connection_get_schema (cnc, GDA_CONNECTION_SCHEMA_TABLES, NULL, NULL);
	if (!rs) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_TABLES_ERROR,
			     _("Can't get list of tables"));
		return FALSE;
	}

	if (!utility_check_data_model (rs, 4, 
				       GDA_VALUE_TYPE_STRING, 
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING,
				       GDA_VALUE_TYPE_STRING)) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_TABLES_ERROR,
			     _("Schema for list of tables is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}

	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !mgdb->priv->stop_update) {
		const GdaValue *value;
		gboolean newtable = FALSE;
		gint i = -1;

		value = gda_data_model_get_value_at (rs, 0, now);
		str = gda_value_stringify (value);
		table = gda_dict_database_get_table_by_name (mgdb, str);
		if (!table) {
			gboolean found = FALSE;

			i = 0;
			/* table name */
			table = GDA_DICT_TABLE (gda_dict_table_new (gda_object_get_dict (GDA_OBJECT (mgdb))));
			gda_object_set_name (GDA_OBJECT (table), str);
			newtable = TRUE;
			
			/* finding the right position for the table */
			list = mgdb->priv->tables;
			while (list && !found) {
				if (strcmp (str, gda_object_get_name (GDA_OBJECT (list->data))) < 0)
					found = TRUE;
				else
					i++;
				list = g_slist_next (list);
			}
		}
		g_free (str);
		
		updated_tables = g_slist_append (updated_tables, table);

		/* description */
		value = gda_data_model_get_value_at (rs, 2, now);
		if (value && !gda_value_is_null (value) && (* gda_value_get_string(value))) {
			str = gda_value_stringify (value);
			gda_object_set_description (GDA_OBJECT (table), str);
			g_free (str);
		}
		else 
			gda_object_set_description (GDA_OBJECT (table), NULL);

		/* owner */
		value = gda_data_model_get_value_at (rs, 1, now);
		if (value && !gda_value_is_null (value) && (* gda_value_get_string(value))) {
			str = gda_value_stringify (value);
			gda_object_set_owner (GDA_OBJECT (table), str);
			g_free (str);
		}
		else
			gda_object_set_owner (GDA_OBJECT (table), NULL);
				
		/* fields */
		g_object_set (G_OBJECT (table), "database", mgdb, NULL);
		if (!gda_dict_table_update_dbms_data (table, error)) {
			g_object_unref (G_OBJECT (rs));
			return FALSE;
		}

		/* signal if the DT is new */
		if (newtable) {
			gda_dict_database_add_table (mgdb, table, i);
			g_object_unref (G_OBJECT (table));
		}

		/* Taking care of the constraints coming attached to the GdaDictTable */
		constraints = g_object_get_data (G_OBJECT (table), "pending_constraints");
		if (constraints) {
			GSList *list = constraints;

			while (list) {
				gda_dict_database_add_constraint_real (mgdb, GDA_DICT_CONSTRAINT (list->data), FALSE);
				g_object_set (G_OBJECT (list->data), "user_constraint", FALSE, NULL);
				g_object_unref (G_OBJECT (list->data));
				list = g_slist_next (list);
			}
			g_slist_free (constraints);
			g_object_set_data (G_OBJECT (table), "pending_constraints", NULL);
		}

		g_signal_emit_by_name (G_OBJECT (mgdb), "update_progress", "TABLES",
				       now, total);
		now++;
	}
	
	g_object_unref (G_OBJECT (rs));
	
	/* remove the tables not existing anymore */
	list = mgdb->priv->tables;
	while (list) {
		if (!g_slist_find (updated_tables, list->data)) {
			gda_object_destroy (GDA_OBJECT (list->data));
			list = mgdb->priv->tables;
		}
		else
			list = g_slist_next (list);
	}
	g_slist_free (updated_tables);
	
	g_signal_emit_by_name (G_OBJECT (mgdb), "update_progress", NULL, 0, 0);

	/* activate all the constraints here */
	list = mgdb->priv->constraints;
	while (list) {
		if (!gda_referer_activate (GDA_REFERER (list->data))) {
			gda_object_destroy (GDA_OBJECT (list->data));
			list = mgdb->priv->constraints;
		}
		else
			list = g_slist_next (list);
	}
	
	return TRUE;
}


static void
destroyed_table_cb (GdaDictTable *table, GdaDictDatabase *mgdb)
{
	gchar *str;
	g_return_if_fail (g_slist_find (mgdb->priv->tables, table));

	mgdb->priv->tables = g_slist_remove (mgdb->priv->tables, table);

	g_signal_handlers_disconnect_by_func (G_OBJECT (table), 
					      G_CALLBACK (destroyed_table_cb), mgdb);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table), 
					      G_CALLBACK (updated_table_cb), mgdb);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table),
					      G_CALLBACK (table_field_added_cb), mgdb);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table),
					      G_CALLBACK (table_field_updated_cb), mgdb);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table),
					      G_CALLBACK (table_field_removed_cb), mgdb);

	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (table));
	g_hash_table_remove (mgdb->priv->tables_hash, str);
	g_free (str);

#ifdef GDA_DEBUG_signal
	g_print (">> 'TABLE_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "table_removed", table);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TABLE_REMOVED' from %s\n", __FUNCTION__);
#endif

	g_object_unref (G_OBJECT (table));
}

static void
updated_table_cb (GdaDictTable *table, GdaDictDatabase *mgdb)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'TABLE_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "table_updated", table);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TABLE_UPDATED' from %s\n", __FUNCTION__);
#endif	
}

static void
table_field_added_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *mgdb)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "field_added", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif	
}

static void
table_field_updated_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *mgdb)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "field_updated", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif	
}

static void
table_field_removed_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *mgdb)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "field_removed", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif	
}



static gboolean
database_sequences_update_list (GdaDictDatabase *mgdb, GError **error)
{
	TO_IMPLEMENT;
	
	return TRUE;
}

static void
destroyed_sequence_cb (GdaDictSequence *seq, GdaDictDatabase *mgdb)
{
	g_return_if_fail (g_slist_find (mgdb->priv->sequences, seq));
	mgdb->priv->sequences = g_slist_remove (mgdb->priv->sequences, seq);

	g_signal_handlers_disconnect_by_func (G_OBJECT (seq), 
					      G_CALLBACK (destroyed_sequence_cb), mgdb);
	g_signal_handlers_disconnect_by_func (G_OBJECT (seq), 
					      G_CALLBACK (updated_sequence_cb), mgdb);

#ifdef GDA_DEBUG_signal
	g_print (">> 'SEQUENCE_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "sequence_removed", seq);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'SEQUENCE_REMOVED' from %s\n", __FUNCTION__);
#endif

	g_object_unref (G_OBJECT (seq));
}


static void
updated_sequence_cb (GdaDictSequence *seq, GdaDictDatabase *mgdb)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'SEQUENCE_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "sequence_updated", seq);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'SEQUENCE_UPDATED' from %s\n", __FUNCTION__);
#endif	
}


static gboolean
database_constraints_update_list (GdaDictDatabase *mgdb, GError **error)
{
	TO_IMPLEMENT;
	/* To be implemented when Libgda has a dedicated schema to fetch constraints */

	return TRUE;
}


static void
destroyed_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *mgdb)
{
	g_return_if_fail (g_slist_find (mgdb->priv->constraints, cons));
	mgdb->priv->constraints = g_slist_remove (mgdb->priv->constraints, cons);

	g_signal_handlers_disconnect_by_func (G_OBJECT (cons), 
					      G_CALLBACK (destroyed_constraint_cb), mgdb);
	g_signal_handlers_disconnect_by_func (G_OBJECT (cons), 
					      G_CALLBACK (updated_constraint_cb), mgdb);
	
	if (mgdb->priv->constraints_hash) {
		GdaDictTable *table;
		GSList *list;

		table = gda_dict_constraint_get_table (cons);
		list = g_hash_table_lookup (mgdb->priv->constraints_hash, table);
		list = g_slist_remove (list, cons);
		if (list)
			g_hash_table_insert (mgdb->priv->constraints_hash, table, list);
		else
			g_hash_table_remove (mgdb->priv->constraints_hash, table);
	}

#ifdef GDA_DEBUG_signal
	g_print (">> 'CONSTRAINT_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "constraint_removed", cons);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CONSTRAINT_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_object_unref (G_OBJECT (cons));
}

static void
updated_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *mgdb)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'CONSTRAINT_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (mgdb), "constraint_updated", cons);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CONSTRAINT_UPDATED' from %s\n", __FUNCTION__);
#endif	
}


/**
 * gda_dict_database_get_tables
 * @mgdb: a #GdaDictDatabase object
 *
 * Get a list of all the tables within @mgdb
 *
 * Returns: a new list of all the #GdaDictTable objects
 */
GSList *
gda_dict_database_get_tables(GdaDictDatabase *mgdb)
{
	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);

	return g_slist_copy (mgdb->priv->tables);
}

/**
 * gda_dict_database_get_table_by_name
 * @mgdb: a #GdaDictDatabase object
 * @name: the name of the requested table
 *
 * Get a reference to a GdaDictTable using its name.
 *
 * Returns: The GdaDictTable pointer or NULL if the requested table does not exist.
 */
GdaDictTable *
gda_dict_database_get_table_by_name (GdaDictDatabase *mgdb, const gchar *name)
{
	GdaDictTable *table = NULL;
	GSList *tables;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);
	g_return_val_if_fail (name && *name, NULL);

	if (mgdb->priv->lc_names)
		cmpstr = g_utf8_strdown (name, -1);
	else
		cmpstr = name;

	tables = mgdb->priv->tables;
	while (!table && tables) {
		if (mgdb->priv->lc_names) {
			cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (tables->data)), -1);
			if (!strcmp (cmpstr2, cmpstr))
				table = GDA_DICT_TABLE (tables->data);
			g_free (cmpstr2);
		}
		else
			if (!strcmp (gda_object_get_name (GDA_OBJECT (tables->data)), cmpstr))
				table = GDA_DICT_TABLE (tables->data);

		tables = g_slist_next (tables);
	}

	if (mgdb->priv->lc_names)
		g_free (cmpstr);
	
	return table;
}

/**
 * gda_dict_database_get_table_by_xml_id
 * @mgdb: a #GdaDictDatabase object
 * @xml_id: the XML id of the requested table
 *
 * Get a reference to a GdaDictTable using its XML id.
 *
 * Returns: The GdaDictTable pointer or NULL if the requested table does not exist.
 */
GdaDictTable *
gda_dict_database_get_table_by_xml_id (GdaDictDatabase *mgdb, const gchar *xml_id)
{
	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	return g_hash_table_lookup (mgdb->priv->tables_hash, xml_id);
}

/**
 * gda_dict_database_get_field_by_name
 * @mgdb: a #GdaDictDatabase object
 * @fullname: the name of the requested table field
 *
 * Get a reference to a GdaDictField specifying the full name (table_name.field_name)
 * of the requested field.
 *
 * Returns: The GdaDictField pointer or NULL if the requested field does not exist.
 */
GdaDictField *
gda_dict_database_get_field_by_name (GdaDictDatabase *mgdb, const gchar *fullname)
{
	gchar *str, *tok, *ptr;
	GdaDictTable *ref_table;
	GdaDictField *ref_field = NULL;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (fullname && *fullname, NULL);

	str = g_strdup (fullname);
	ptr = strtok_r (str, ".", &tok);
	ref_table = gda_dict_database_get_table_by_name (mgdb, ptr);
	if (ref_table) {
		gpointer data;
		ptr = strtok_r (NULL, ".", &tok);
		if (ptr) {
			data = gda_entity_get_field_by_name (GDA_ENTITY (ref_table), ptr);
			if (data)
				ref_field = GDA_DICT_FIELD (data);
		}
	}
	g_free (str);

	return ref_field;
}


/**
 * gda_dict_database_get_field_by_xml_id
 * @mgdb: a #GdaDictDatabase object
 * @xml_id: the XML id of the requested table field
 *
 * Get a reference to a GdaDictField specifying its XML id
 *
 * Returns: The GdaDictField pointer or NULL if the requested field does not exist.
 */
GdaDictField *
gda_dict_database_get_field_by_xml_id (GdaDictDatabase *mgdb, const gchar *xml_id)
{
	GdaDictField *field = NULL;
	GSList *tables;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	tables = mgdb->priv->tables;
	while (tables && !field) {
		gpointer data;
		data = gda_entity_get_field_by_xml_id (GDA_ENTITY (tables->data), xml_id);
		if (data)
			field = GDA_DICT_FIELD (data);
		tables = g_slist_next (tables);
	}

	return field;
}

/**
 * gda_dict_database_get_sequence_by_name
 * @mgdb: a #GdaDictDatabase object
 * @name: the name of the requested sequence
 *
 * Get a reference to a GdaDictSequence specifying its name
 *
 * Returns: The GdaDictSequence pointer or NULL if the requested sequence does not exist.
 */
GdaDictSequence *
gda_dict_database_get_sequence_by_name (GdaDictDatabase *mgdb, const gchar *name)
{
	GdaDictSequence *seq = NULL;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (name && *name, NULL);

	TO_IMPLEMENT;
	return seq;
}

/**
 * gda_dict_database_get_sequence_by_xml_id
 * @mgdb: a #GdaDictDatabase object
 * @xml_id: the XML id of the requested sequence
 *
 * Get a reference to a GdaDictSequence specifying its XML id.
 *
 * Returns: The GdaDictSequence pointer or NULL if the requested sequence does not exist.
 */
GdaDictSequence *
gda_dict_database_get_sequence_by_xml_id (GdaDictDatabase *mgdb, const gchar *xml_id)
{
	GdaDictSequence *seq = NULL;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	TO_IMPLEMENT;
	return seq;
}

/** 
 * gda_dict_database_get_sequence_to_field
 * @mgdb: a #GdaDictDatabase object
 * @field: a #GdaDictField object
 *
 * Get a reference to a GdaDictSequence which is "linked" to the GdaDictField given as parameter.
 * This "link" means that each new value of the field will be given by the returned sequence
 *
 * Returns: The GdaDictSequence pointer or NULL if there is no sequence for this job.
 */
GdaDictSequence *
gda_dict_database_get_sequence_to_field  (GdaDictDatabase *mgdb, GdaDictField *field)
{
	GdaDictSequence *seq = NULL;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (field && GDA_IS_ENTITY_FIELD (field), NULL);

	TO_IMPLEMENT;
	return seq;
}


/**
 * gda_dict_database_link_sequence
 * @mgdb: a #GdaDictDatabase object
 * @seq:
 * @field:
 *
 * Tells the database that each new value of the field given as argument should be
 * obtained from the specified sequence (this is usefull when the field is a simple
 * primary key for example).
 */
void
gda_dict_database_link_sequence (GdaDictDatabase *mgdb, GdaDictSequence *seq, GdaDictField *field)
{
	g_return_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb));
	/*g_return_if_fail (seq && IS_GNOME_DB_SEQUENCE (seq));*/
	g_return_if_fail (field && GDA_IS_ENTITY_FIELD (field));

	TO_IMPLEMENT;	
}

/**
 * gda_dict_database_unlink_sequence
 * @mgdb: a #GdaDictDatabase object
 * @seq:
 * @field:
 *
 * Tells the database that each new value of the field given as argument should not be
 * obtained from the specified sequence (this is usefull when the field is a simple
 * primary key for example). This is the opposite of the gda_dict_database_link_sequence() method.
 */
void
gda_dict_database_unlink_sequence (GdaDictDatabase *mgdb, GdaDictSequence *seq, GdaDictField *field)
{
	g_return_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb));
	/*g_return_if_fail (seq && IS_GNOME_DB_SEQUENCE (seq));*/
	g_return_if_fail (field && GDA_IS_ENTITY_FIELD (field));

	TO_IMPLEMENT;	
}

/**
 * gda_dict_database_get_all_constraints
 * @mgdb: a #GdaDictDatabase object
 *
 * Get a list of all the constraints applied to the database. Constraints are represented
 * as #GdaDictConstraint objects and represent any type of constraint.
 *
 * Returns: a new list of the constraints
 */
GSList *
gda_dict_database_get_all_constraints (GdaDictDatabase *mgdb)
{
	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);

	return g_slist_copy (mgdb->priv->constraints);
}

/**
 * gda_dict_database_get_table_constraints
 * @mgdb: a #GdaDictDatabase object
 * @table: a #GdaDictTable, part of @mgdb
 *
 * Get all the constraints applicable to @table
 *
 * Returns: a new GSList of #GdaDictConstraint objects
 */
GSList *
gda_dict_database_get_table_constraints (GdaDictDatabase *mgdb, GdaDictTable *table)
{
	GSList *list;
	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);

	list = g_hash_table_lookup (mgdb->priv->constraints_hash, table);
	if (list) 
		return g_slist_copy (list);
	else
		return NULL;
}

/**
 * gda_dict_database_get_all_fk_constraints
 * @mgdb: a #GdaDictDatabase object
 *
 * Get a list of all the constraints applied to the database which represent a foreign constrains. 
 * Constraints are represented as #GdaDictConstraint objects.
 *
 * Returns: a new list of the constraints
 */
GSList *
gda_dict_database_get_all_fk_constraints (GdaDictDatabase *mgdb)
{
	GSList *retval = NULL, *list;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);

	list = mgdb->priv->constraints;
	while (list) {
		if (gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) == 
		    CONSTRAINT_FOREIGN_KEY)
			retval = g_slist_append (retval, list->data);

		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_dict_database_get_tables_fk_constraints
 * @mgdb: a #GdaDictDatabase object
 * @table1: a #GdaDictTable, or %NULL
 * @table2: a #GdaDictTable, or %NULL
 * @table1_has_fk: TRUE if the returned constraints are the one for which @table1 contains the foreign key
 *
 * Get a list of all the constraints applied to the database which represent a foreign key constrains, between
 * @table1 and @table2. If @table1 or @table2 are %NULL, then the returned foreign key constraints are the ones
 * between any table and @table1 or @table2.
 *
 * Constraints are represented as #GdaDictConstraint objects.
 *
 * Returns: a new list of the constraints
 */
GSList *
gda_dict_database_get_tables_fk_constraints (GdaDictDatabase *mgdb, GdaDictTable *table1, GdaDictTable *table2, 
				       gboolean table1_has_fk)
{
	GSList *retval = NULL, *list;
	GdaDictConstraint *fkptr;

	g_return_val_if_fail (mgdb && GDA_IS_DICT_DATABASE (mgdb), NULL);
	g_return_val_if_fail (mgdb->priv, NULL);
	if (table1)
		g_return_val_if_fail (GDA_IS_DICT_TABLE (table1), NULL);
	if (table2)
		g_return_val_if_fail (GDA_IS_DICT_TABLE (table2), NULL);
	if (!table1 && !table2)
		return NULL;

	if (table1_has_fk) {
		if (table1) {
			/* we want the FK constraints where @table1 is the FK table */
			list = g_hash_table_lookup (mgdb->priv->constraints_hash, table1);
			while (list) {
				fkptr = GDA_DICT_CONSTRAINT (list->data);
				if ((gda_dict_constraint_get_constraint_type (fkptr) == CONSTRAINT_FOREIGN_KEY) &&
				    (!table2 || (gda_dict_constraint_fkey_get_ref_table (fkptr) ==  (GdaDictTable *) table2)))
					retval = g_slist_append (retval, fkptr);
				list = g_slist_next (list);
			}
		}
		else {
			/* we want all the FK constraints where @table2 is the ref_pk table */
			list = mgdb->priv->constraints;
			while (list) {
				fkptr = GDA_DICT_CONSTRAINT (list->data);
				if ((gda_dict_constraint_get_constraint_type (fkptr) == CONSTRAINT_FOREIGN_KEY) &&
				    (gda_dict_constraint_fkey_get_ref_table (fkptr) ==  (GdaDictTable *) table2))
					retval = g_slist_append (retval, fkptr);
				list = g_slist_next (list);
			}
		}
	}
	else {
		list = mgdb->priv->constraints;
		while (list) {
			fkptr = GDA_DICT_CONSTRAINT (list->data);
			if (gda_dict_constraint_get_constraint_type (fkptr) == CONSTRAINT_FOREIGN_KEY) {
				GdaDictTable *fk_table = gda_dict_constraint_get_table (fkptr);
				GdaDictTable *ref_pk_table = gda_dict_constraint_fkey_get_ref_table (fkptr);
								
				if (((!table1 || (fk_table == table1)) && (!table2 || (ref_pk_table == table2))) ||
				    ((!table1 || (ref_pk_table == table1)) && (!table2 || (fk_table == table2))) )
					retval = g_slist_append (retval, fkptr);
			}
				
			list = g_slist_next (list);
		}
	}

	return retval;
}
