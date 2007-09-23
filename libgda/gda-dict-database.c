/* gda-dict-database.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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

#if 0 /* This object does not have any properties. */
static void gda_dict_database_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_database_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);
#endif

static void        gda_dict_database_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gda_dict_database_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gda_dict_database_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_dict_database_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

static void gda_dict_database_add_table (GdaDictDatabase *db, GdaDictTable *table, gint pos);

static void destroyed_table_cb (GdaDictTable *table, GdaDictDatabase *db);
static void destroyed_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *db);

static void updated_table_cb (GdaDictTable *table, GdaDictDatabase *db);
static void table_field_added_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *db);
static void table_field_updated_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *db);
static void table_field_removed_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *db);
static void updated_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *db);

#ifdef GDA_DEBUG
static void gda_dict_database_dump (GdaDictDatabase *db, gint offset);
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
	CONSTRAINT_ADDED,
	CONSTRAINT_REMOVED,
	CONSTRAINT_UPDATED,
	LAST_SIGNAL
};

static gint gda_dict_database_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* properties */
#if 0 /* This object does not have any properties. */
enum
{
	PROP_0,
	PROP
};
#endif


/* private structure */
struct _GdaDictDatabasePrivate
{
	/* Db structure */
	GSList                 *tables;
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

	if (G_UNLIKELY (type == 0)) {
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
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_TABLE);
	gda_dict_database_signals[TABLE_REMOVED] =
                g_signal_new ("table_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, table_removed),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_TABLE);
	gda_dict_database_signals[TABLE_UPDATED] =
                g_signal_new ("table_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, table_updated),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_TABLE);

        gda_dict_database_signals[FIELD_ADDED] =
                g_signal_new ("field_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, field_added),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_FIELD);
        gda_dict_database_signals[FIELD_REMOVED] =
                g_signal_new ("field_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, field_removed),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_FIELD);
        gda_dict_database_signals[FIELD_UPDATED] =
                g_signal_new ("field_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, field_updated),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_FIELD);

        gda_dict_database_signals[CONSTRAINT_ADDED] =
                g_signal_new ("constraint_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, constraint_added),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_CONSTRAINT);
        gda_dict_database_signals[CONSTRAINT_REMOVED] =
                g_signal_new ("constraint_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, constraint_removed),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_CONSTRAINT);
        gda_dict_database_signals[CONSTRAINT_UPDATED] =
                g_signal_new ("constraint_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDictDatabaseClass, constraint_updated),
                              NULL, NULL,
                              gda_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDA_TYPE_DICT_CONSTRAINT);

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
                              gda_marshal_VOID__STRING_UINT_UINT,
                              G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);
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
	class->constraint_added = NULL;
	class->constraint_removed = NULL;
	class->constraint_updated = NULL;
        class->data_update_started = NULL;
        class->data_update_finished = NULL;
        class->update_progress = NULL;

	object_class->dispose = gda_dict_database_dispose;
	object_class->finalize = gda_dict_database_finalize;

#if 0 /* This object does not have any properties.*/
	/* Properties */
	object_class->set_property = gda_dict_database_set_property;
	object_class->get_property = gda_dict_database_get_property;

        /* TODO: What kind of object is this meant to be?
           When we know, we should use g_param_spec_object() instead of g_param_spec_pointer().
           murrayc.
         */
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        #endif

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

#if 0 /* This object does not have any properties. */
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
#endif


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
	GdaDictDatabase *db;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_DICT_DATABASE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_DATABASE (iface)->priv, NULL);

	db = GDA_DICT_DATABASE (iface);

	/* main node */
        toptree = xmlNewNode (NULL, (xmlChar*)"gda_dict_database");
	
	/* Tables */
	tree = xmlNewChild (toptree, NULL, (xmlChar*)"gda_dict_tables", NULL);
	list = db->priv->tables;
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

	/* Constraints */
	tree = xmlNewChild (toptree, NULL, (xmlChar*)"gda_dict_constraints", NULL);
	list = db->priv->constraints;
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
	GdaDictDatabase *db;
	xmlNodePtr subnode;

	g_return_val_if_fail (iface && GDA_IS_DICT_DATABASE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_DATABASE (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	db = GDA_DICT_DATABASE (iface);

	if (db->priv->tables || db->priv->constraints) {
		g_set_error (error,
			     GDA_DICT_DATABASE_ERROR,
			     GDA_DICT_DATABASE_XML_LOAD_ERROR,
			     _("Database already contains data"));
		return FALSE;
	}
	if (strcmp ((gchar*)node->name, "gda_dict_database")) {
		g_set_error (error,
			     GDA_DICT_DATABASE_ERROR,
			     GDA_DICT_DATABASE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_database>"));
		return FALSE;
	}
	db->priv->xml_loading = TRUE;
	subnode = node->children;
	while (subnode) {
		gboolean done = FALSE;

		if (!strcmp ((gchar*)subnode->name, "gda_dict_tables")) {
			if (!gda_dict_database_load_from_xml_tables (iface, subnode, error)) {
				db->priv->xml_loading = FALSE;
				return FALSE;
			}

			done = TRUE;
		}

		if (!done && !strcmp ((gchar*)subnode->name, "gda_dict_constraints")) {
			if (!gda_dict_database_load_from_xml_constraints (iface, subnode, error)) {
				db->priv->xml_loading = FALSE;
				return FALSE;
			}

			done = TRUE;
		}

		subnode = subnode->next;
	}
	db->priv->xml_loading = FALSE;
	
	return TRUE;
}

static gboolean
gda_dict_database_load_from_xml_tables (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	gboolean allok = TRUE;
	xmlNodePtr subnode = node->children;

	while (subnode && allok) {
		if (! xmlNodeIsText (subnode)) {
			if (!strcmp ((gchar*)subnode->name, "gda_dict_table")) {
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

static void gda_dict_database_add_constraint_real (GdaDictDatabase *db, GdaDictConstraint *cstr, gboolean force_user_constraint);
static gboolean
gda_dict_database_load_from_xml_constraints (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	gboolean allok = TRUE;
	xmlNodePtr subnode = node->children;

	while (subnode && allok) {
		if (! xmlNodeIsText (subnode)) {
			if (!strcmp ((gchar*)subnode->name, "gda_dict_constraint")) {
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
gda_dict_database_add_table (GdaDictDatabase *db, GdaDictTable *table, gint pos)
{
	gchar *str;
	g_return_if_fail (table);
	g_return_if_fail (!g_slist_find (db->priv->tables, table));

	g_object_set (G_OBJECT (table), "database", db, NULL);
	db->priv->tables = g_slist_insert (db->priv->tables, table, pos);

	g_object_ref (G_OBJECT (table));
	gda_object_connect_destroy (table, G_CALLBACK (destroyed_table_cb), db);

	g_signal_connect (G_OBJECT (table), "changed",
			  G_CALLBACK (updated_table_cb), db);
	g_signal_connect (G_OBJECT (table), "field_added",
			  G_CALLBACK (table_field_added_cb), db);
	g_signal_connect (G_OBJECT (table), "field_updated",
			  G_CALLBACK (table_field_updated_cb), db);
	g_signal_connect (G_OBJECT (table), "field_removed",
			  G_CALLBACK (table_field_removed_cb), db);

	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (table));
	g_hash_table_insert (db->priv->tables_hash, str, table);

#ifdef GDA_DEBUG_signal
	g_print (">> 'TABLE_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (db), gda_dict_database_signals[TABLE_ADDED], 0, table);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TABLE_ADDED' from %s\n", __FUNCTION__);
#endif
}

/**
 * gda_dict_database_add_constraint
 * @db: a #GdaDictDatabase object
 * @cstr: a #GdaDictConstraint
 *
 * Add the @cstr constraint to the database. The @cstr constraint is a user-defined constraint
 * (which is not part of the database structure itself).
 */
void
gda_dict_database_add_constraint (GdaDictDatabase *db, GdaDictConstraint *cstr)
{
	gda_dict_database_add_constraint_real (db, cstr, TRUE);
}

static void
gda_dict_database_add_constraint_real (GdaDictDatabase *db, GdaDictConstraint *cstr, gboolean force_user_constraint)
{
	GdaDictConstraint *ptr = NULL;

	g_return_if_fail (db && GDA_IS_DICT_DATABASE (db));
	g_return_if_fail (db->priv);
	g_return_if_fail (cstr);

	/* Try to activate the constraints here */
	gda_referer_activate (GDA_REFERER (cstr));

	/* try to find if a similar constraint is there, if we are not loading from an XML file */
	if (!db->priv->xml_loading) {
		GSList *list = db->priv->constraints;
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
		db->priv->constraints = g_slist_append (db->priv->constraints, cstr);
		g_object_ref (G_OBJECT (cstr));
		gda_object_connect_destroy (cstr, G_CALLBACK (destroyed_constraint_cb), db);
		g_signal_connect (G_OBJECT (cstr), "changed",
				  G_CALLBACK (updated_constraint_cb), db);

		/* add the constraint to the 'constraints_hash' */
		table = gda_dict_constraint_get_table (cstr);
		list = g_hash_table_lookup (db->priv->constraints_hash, table);
		list = g_slist_append (list, cstr);
		g_hash_table_insert (db->priv->constraints_hash, table, list);

#ifdef GDA_DEBUG_signal
		g_print (">> 'CONSTRAINT_ADDED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (db), gda_dict_database_signals[CONSTRAINT_ADDED], 0, cstr);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'CONSTRAINT_ADDED' from %s\n", __FUNCTION__);
#endif
	}
}

#ifdef GDA_DEBUG
static void 
gda_dict_database_dump (GdaDictDatabase *db, gint offset)
{
	gchar *str;
	guint i;
	GSList *list;

	g_return_if_fail (db && GDA_IS_DICT_DATABASE (db));
	
	/* string for the offset */
	str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;
	
	/* dump */
	if (db->priv) 
		g_print ("%s" D_COL_H1 "GdaDictDatabase %p\n" D_COL_NOR, str, db);
	else
		g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, db);

	/* tables */
	list = db->priv->tables;
	if (list) {
		g_print ("%sTables:\n", str);
		while (list) {
			gda_object_dump (GDA_OBJECT (list->data), offset+5);
			list = g_slist_next (list);
		}
	}
	else
		g_print ("%sNo Table defined\n", str);

	/* constraints */
	list = db->priv->constraints;
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
 * @db: a #GdaDictDatabase object
 *
 * Fetch the GdaDict object to which the GdaDictDatabase belongs.
 *
 * Returns: the GdaDict object
 */
GdaDict *
gda_dict_database_get_dict (GdaDictDatabase *db)
{
	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);

	return gda_object_get_dict (GDA_OBJECT (db));
}



static gboolean database_tables_update_list (GdaDictDatabase * db, const gchar *table_name, GError **error);
static gboolean database_constraints_update_list (GdaDictDatabase * db, GError **error);
/**
 * gda_dict_database_update_dbms_data
 * @db: a #GdaDictDatabase object
 * @limit_to_type: limit the DBMS update to this type, or 0 for no limit
 * @limit_obj_name: limit the DBMS update to objects of this name, or %NULL for no limit
 * @error: location to store error, or %NULL
 *
 * Synchronises the database representation with the database structure which is stored in
 * the DBMS. For this operation to succeed, the connection to the DBMS server MUST be opened
 * (using the corresponding #GdaConnection object).
 *
 * Returns: TRUE if no error
 */
gboolean
gda_dict_database_update_dbms_data (GdaDictDatabase *db, GType limit_to_type, const gchar *limit_obj_name, GError **error)
{
	gboolean retval = TRUE;
	GdaConnection *cnc;
	GdaDict *dict;

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), FALSE);
	g_return_val_if_fail (db->priv, FALSE);
	
	if (db->priv->update_in_progress) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_META_DATA_UPDATE,
			     _("Update already started!"));
		return FALSE;
	}

	dict = gda_object_get_dict (GDA_OBJECT (db));
	cnc = gda_dict_get_connection (dict);
	if (!cnc) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("No connection associated to dictionary!"));
                return FALSE;
	}
	if (!gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_DATABASE_META_DATA_UPDATE,
			     _("Connection is not opened!"));
		return FALSE;
	}

	db->priv->update_in_progress = TRUE;
	db->priv->stop_update = FALSE;

#ifdef GDA_DEBUG_signal
	g_print (">> 'DATA_UPDATE_STARTED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (db), gda_dict_database_signals[DATA_UPDATE_STARTED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'DATA_UPDATE_STARTED' from %s\n", __FUNCTION__);
#endif

	if (!limit_to_type || (limit_to_type == GDA_TYPE_DICT_TABLE))
		retval = database_tables_update_list (db, limit_obj_name, error);

	if (retval && !db->priv->stop_update && 
	    (!limit_to_type || (limit_to_type == GDA_TYPE_DICT_CONSTRAINT)))
		retval = database_constraints_update_list (db, error);

#ifdef GDA_DEBUG_signal
	g_print (">> 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (db), gda_dict_database_signals[DATA_UPDATE_FINISHED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'DATA_UPDATE_FINISHED' from %s\n", __FUNCTION__);
#endif

	db->priv->update_in_progress = FALSE;
	if (db->priv->stop_update) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_META_DATA_UPDATE_USER_STOPPED,
			     _("Update stopped!"));
		return FALSE;
	}

	return retval;
}


/**
 * gda_dict_database_stop_update_dbms_data
 * @db: a #GdaDictDatabase object
 *
 * When the database updates its internal lists of DBMS objects, a call to this function will 
 * stop that update process. It has no effect when the database is not updating its DBMS data.
 */
void
gda_dict_database_stop_update_dbms_data (GdaDictDatabase *db)
{
	g_return_if_fail (db && GDA_IS_DICT_DATABASE (db));
	g_return_if_fail (db->priv);

	db->priv->stop_update = TRUE;
}

static GSList *
database_tables_n_views_update_treat_schema_result (GdaDictDatabase *db, GdaDataModel *rs, 
						    gboolean for_views, GError **error)
{
	GSList *updated_tables = NULL;
	guint now, total;
	gchar *str;
	GdaDictTable *table;
	GSList *constraints;
	GSList *list;

	if (!gda_utility_check_data_model (rs, 4, 
				       G_TYPE_STRING, 
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING)) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_TABLES_ERROR,
			     _("Schema for list of tables is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}

	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !db->priv->stop_update) {
		const GValue *value;
		gboolean newtable = FALSE;
		gint i = -1;

		value = gda_data_model_get_value_at (rs, 0, now);
		str = gda_value_stringify ((GValue *) value);
		table = gda_dict_database_get_table_by_name (db, str);
		if (!table) {
			gboolean found = FALSE;

			i = 0;
			/* table name */
			table = GDA_DICT_TABLE (gda_dict_table_new (gda_object_get_dict (GDA_OBJECT (db))));
			gda_object_set_name (GDA_OBJECT (table), str);
			newtable = TRUE;
			if (for_views)
				g_object_set (G_OBJECT (table), "is-view", TRUE, NULL);
			
			/* finding the right position for the table */
			list = db->priv->tables;
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
		if (value && !gda_value_is_null ((GValue *) value) && 
		    (* g_value_get_string((GValue *) value))) {
			str = gda_value_stringify ((GValue *) value);
			gda_object_set_description (GDA_OBJECT (table), str);
			g_free (str);
		}
		else 
			gda_object_set_description (GDA_OBJECT (table), NULL);

		/* owner */
		value = gda_data_model_get_value_at (rs, 1, now);
		if (value && !gda_value_is_null ((GValue *) value) && 
		    (* g_value_get_string((GValue *) value))) {
			str = gda_value_stringify ((GValue *) value);
			gda_object_set_owner (GDA_OBJECT (table), str);
			g_free (str);
		}
		else
			gda_object_set_owner (GDA_OBJECT (table), NULL);
				
		/* fields */
		g_object_set (G_OBJECT (table), "database", db, NULL);
		if (!gda_dict_table_update_dbms_data (table, error)) {
			g_object_unref (G_OBJECT (rs));
			return FALSE;
		}

		/* signal if the DT is new */
		if (newtable) {
			gda_dict_database_add_table (db, table, i);
			g_object_unref (G_OBJECT (table));
		}

		/* Taking care of the constraints coming attached to the GdaDictTable */
		constraints = g_object_get_data (G_OBJECT (table), "pending_constraints");
		if (constraints) {
			GSList *list = constraints;

			while (list) {
				gda_dict_database_add_constraint_real (db, GDA_DICT_CONSTRAINT (list->data), FALSE);
				g_object_set (G_OBJECT (list->data), "user_constraint", FALSE, NULL);
				g_object_unref (G_OBJECT (list->data));
				list = g_slist_next (list);
			}
			g_slist_free (constraints);
			g_object_set_data (G_OBJECT (table), "pending_constraints", NULL);
		}

		g_signal_emit_by_name (G_OBJECT (db), "update_progress", for_views ? "VIEWS" : "TABLES",
				       now, total);
		now++;
	}

	return updated_tables;
}

static gboolean
database_tables_update_list (GdaDictDatabase *db, const gchar *table_name, GError **error)
{
	GSList *list;
	GdaDataModel *rs;

	GSList *updated_tables, *updated_views;
	GdaConnection *cnc;
	GdaParameterList *options = NULL;
	
	cnc = gda_dict_get_connection (gda_object_get_dict (GDA_OBJECT (db)));
	if (!cnc) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("No connection associated to dictionary!"));
                return FALSE;
	}
	if (!gda_connection_is_opened (cnc)) {
                g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("Connection is not opened!"));
                return FALSE;
        }

	if (table_name) {
		options = gda_parameter_list_new (NULL);
		gda_parameter_list_add_param_from_string (options, "name", G_TYPE_STRING, table_name);
	}

	/* tables */
	rs = gda_connection_get_schema (cnc, GDA_CONNECTION_SCHEMA_TABLES, options, NULL);
	if (options)
		g_object_unref (options);
	if (!rs) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_TABLES_ERROR,
			     _("Can't get list of tables"));
		return FALSE;
	}
	updated_tables = database_tables_n_views_update_treat_schema_result (db, rs, FALSE, error);
	g_object_unref (G_OBJECT (rs));
	g_signal_emit_by_name (G_OBJECT (db), "update_progress", NULL, 0, 0);

	/* views */
	rs = gda_connection_get_schema (cnc, GDA_CONNECTION_SCHEMA_VIEWS, options, NULL);
	if (options)
		g_object_unref (options);
	if (!rs) {
		g_set_error (error, GDA_DICT_DATABASE_ERROR, GDA_DICT_DATABASE_TABLES_ERROR,
			     _("Can't get list of views"));
		g_slist_free (updated_tables);
		return FALSE;
	}
	updated_views = database_tables_n_views_update_treat_schema_result (db, rs, TRUE, error);
	g_object_unref (G_OBJECT (rs));
	g_signal_emit_by_name (G_OBJECT (db), "update_progress", NULL, 0, 0);

	updated_tables = g_slist_concat (updated_tables, updated_views);
	
	/* remove the tables not existing anymore */
	for (list = db->priv->tables; list; list = list->next) {
		if (table_name && strcmp (gda_object_get_name (GDA_OBJECT (list->data)), table_name))
			continue;

		if (!g_slist_find (updated_tables, list->data)) {
			gda_object_destroy (GDA_OBJECT (list->data));
			list = db->priv->tables;
		}
	}
	g_slist_free (updated_tables);
	
	g_signal_emit_by_name (G_OBJECT (db), "update_progress", NULL, 0, 0);

	/* activate all the constraints here */
	list = db->priv->constraints;
	while (list) {
		if (!gda_referer_activate (GDA_REFERER (list->data))) {
			gda_object_destroy (GDA_OBJECT (list->data));
			list = db->priv->constraints;
		}
		else
			list = g_slist_next (list);
	}
	
	return TRUE;
}


static void
destroyed_table_cb (GdaDictTable *table, GdaDictDatabase *db)
{
	gchar *str;
	g_return_if_fail (g_slist_find (db->priv->tables, table));

	db->priv->tables = g_slist_remove (db->priv->tables, table);

	g_signal_handlers_disconnect_by_func (G_OBJECT (table), 
					      G_CALLBACK (destroyed_table_cb), db);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table), 
					      G_CALLBACK (updated_table_cb), db);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table),
					      G_CALLBACK (table_field_added_cb), db);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table),
					      G_CALLBACK (table_field_updated_cb), db);
	g_signal_handlers_disconnect_by_func (G_OBJECT (table),
					      G_CALLBACK (table_field_removed_cb), db);

	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (table));
	g_hash_table_remove (db->priv->tables_hash, str);
	g_free (str);

#ifdef GDA_DEBUG_signal
	g_print (">> 'TABLE_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "table_removed", table);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TABLE_REMOVED' from %s\n", __FUNCTION__);
#endif

	g_object_unref (G_OBJECT (table));
}

static void
updated_table_cb (GdaDictTable *table, GdaDictDatabase *db)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'TABLE_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "table_updated", table);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TABLE_UPDATED' from %s\n", __FUNCTION__);
#endif	
}

static void
table_field_added_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *db)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "field_added", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif	
}

static void
table_field_updated_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *db)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "field_updated", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif	
}

static void
table_field_removed_cb (GdaDictTable *table, GdaDictField *field, GdaDictDatabase *db)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "field_removed", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif	
}

static gboolean
database_constraints_update_list (GdaDictDatabase *db, GError **error)
{
	TO_IMPLEMENT;
	/* To be implemented when Libgda has a dedicated schema to fetch constraints */

	return TRUE;
}


static void
destroyed_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *db)
{
	g_return_if_fail (g_slist_find (db->priv->constraints, cons));
	db->priv->constraints = g_slist_remove (db->priv->constraints, cons);

	g_signal_handlers_disconnect_by_func (G_OBJECT (cons), 
					      G_CALLBACK (destroyed_constraint_cb), db);
	g_signal_handlers_disconnect_by_func (G_OBJECT (cons), 
					      G_CALLBACK (updated_constraint_cb), db);
	
	if (db->priv->constraints_hash) {
		GdaDictTable *table;
		GSList *list;

		table = gda_dict_constraint_get_table (cons);
		list = g_hash_table_lookup (db->priv->constraints_hash, table);
		list = g_slist_remove (list, cons);
		if (list)
			g_hash_table_insert (db->priv->constraints_hash, table, list);
		else
			g_hash_table_remove (db->priv->constraints_hash, table);
	}

#ifdef GDA_DEBUG_signal
	g_print (">> 'CONSTRAINT_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "constraint_removed", cons);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CONSTRAINT_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_object_unref (G_OBJECT (cons));
}

static void
updated_constraint_cb (GdaDictConstraint *cons, GdaDictDatabase *db)
{
#ifdef GDA_DEBUG_signal
	g_print (">> 'CONSTRAINT_UPDATED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (db), "constraint_updated", cons);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'CONSTRAINT_UPDATED' from %s\n", __FUNCTION__);
#endif	
}


/**
 * gda_dict_database_get_tables
 * @db: a #GdaDictDatabase object
 *
 * Get a list of all the tables within @db
 *
 * Returns: a new list of all the #GdaDictTable objects
 */
GSList *
gda_dict_database_get_tables(GdaDictDatabase *db)
{
	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);

	return g_slist_copy (db->priv->tables);
}

/**
 * gda_dict_database_get_table_by_name
 * @db: a #GdaDictDatabase object
 * @name: the name of the requested table
 *
 * Get a reference to a GdaDictTable using its name.
 *
 * Returns: The GdaDictTable pointer or NULL if the requested table does not exist.
 */
GdaDictTable *
gda_dict_database_get_table_by_name (GdaDictDatabase *db, const gchar *name)
{
	GdaDictTable *table = NULL;
	GSList *tables;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);
	g_return_val_if_fail (name && *name, NULL);

	if (db->priv->lc_names)
		cmpstr = g_utf8_strdown (name, -1);
	else
		cmpstr = (gchar *) name;

	tables = db->priv->tables;
	while (!table && tables) {
		if (db->priv->lc_names) {
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

	if (db->priv->lc_names)
		g_free (cmpstr);
	
	return table;
}

/**
 * gda_dict_database_get_table_by_xml_id
 * @db: a #GdaDictDatabase object
 * @xml_id: the XML id of the requested table
 *
 * Get a reference to a GdaDictTable using its XML id.
 *
 * Returns: The GdaDictTable pointer or NULL if the requested table does not exist.
 */
GdaDictTable *
gda_dict_database_get_table_by_xml_id (GdaDictDatabase *db, const gchar *xml_id)
{
	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	return g_hash_table_lookup (db->priv->tables_hash, xml_id);
}

/**
 * gda_dict_database_get_field_by_name
 * @db: a #GdaDictDatabase object
 * @fullname: the name of the requested table field
 *
 * Get a reference to a GdaDictField specifying the full name (table_name.field_name)
 * of the requested field.
 *
 * Returns: The GdaDictField pointer or NULL if the requested field does not exist.
 */
GdaDictField *
gda_dict_database_get_field_by_name (GdaDictDatabase *db, const gchar *fullname)
{
	gchar *str, *tok, *ptr;
	GdaDictTable *ref_table;
	GdaDictField *ref_field = NULL;

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (fullname && *fullname, NULL);

	str = g_strdup (fullname);
	ptr = strtok_r (str, ".", &tok);
	ref_table = gda_dict_database_get_table_by_name (db, ptr);
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
 * @db: a #GdaDictDatabase object
 * @xml_id: the XML id of the requested table field
 *
 * Get a reference to a GdaDictField specifying its XML id
 *
 * Returns: The GdaDictField pointer or NULL if the requested field does not exist.
 */
GdaDictField *
gda_dict_database_get_field_by_xml_id (GdaDictDatabase *db, const gchar *xml_id)
{
	GdaDictField *field = NULL;
	GSList *tables;

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (xml_id && *xml_id, NULL);

	tables = db->priv->tables;
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
 * gda_dict_database_get_all_constraints
 * @db: a #GdaDictDatabase object
 *
 * Get a list of all the constraints applied to the database. Constraints are represented
 * as #GdaDictConstraint objects and represent any type of constraint.
 *
 * Returns: a new list of the constraints
 */
GSList *
gda_dict_database_get_all_constraints (GdaDictDatabase *db)
{
	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);

	return g_slist_copy (db->priv->constraints);
}

/**
 * gda_dict_database_get_table_constraints
 * @db: a #GdaDictDatabase object
 * @table: a #GdaDictTable, part of @db
 *
 * Get all the constraints applicable to @table
 *
 * Returns: a new GSList of #GdaDictConstraint objects
 */
GSList *
gda_dict_database_get_table_constraints (GdaDictDatabase *db, GdaDictTable *table)
{
	GSList *list;
	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);

	list = g_hash_table_lookup (db->priv->constraints_hash, table);
	if (list) 
		return g_slist_copy (list);
	else
		return NULL;
}

/**
 * gda_dict_database_get_all_fk_constraints
 * @db: a #GdaDictDatabase object
 *
 * Get a list of all the constraints applied to the database which represent a foreign constrains. 
 * Constraints are represented as #GdaDictConstraint objects.
 *
 * Returns: a new list of the constraints
 */
GSList *
gda_dict_database_get_all_fk_constraints (GdaDictDatabase *db)
{
	GSList *retval = NULL, *list;

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);

	list = db->priv->constraints;
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
 * @db: a #GdaDictDatabase object
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
gda_dict_database_get_tables_fk_constraints (GdaDictDatabase *db, GdaDictTable *table1, GdaDictTable *table2, 
				       gboolean table1_has_fk)
{
	GSList *retval = NULL, *list;
	GdaDictConstraint *fkptr;

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	g_return_val_if_fail (db->priv, NULL);
	if (table1)
		g_return_val_if_fail (GDA_IS_DICT_TABLE (table1), NULL);
	if (table2)
		g_return_val_if_fail (GDA_IS_DICT_TABLE (table2), NULL);
	if (!table1 && !table2)
		return NULL;

	if (table1_has_fk) {
		if (table1) {
			/* we want the FK constraints where @table1 is the FK table */
			list = g_hash_table_lookup (db->priv->constraints_hash, table1);
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
			list = db->priv->constraints;
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
		list = db->priv->constraints;
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
