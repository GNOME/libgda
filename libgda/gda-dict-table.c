/* gda-dict-table.c
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

#include "gda-dict-table.h"
#include "gda-dict-database.h"
#include "gda-dict-field.h"
#include "gda-dict-constraint.h"
#include "gda-xml-storage.h"
#include "gda-entity-field.h"
#include "gda-entity.h"
#include "gda-data-handler.h"
#include "gda-connection.h"
#include "gda-dict-type.h"
#include <string.h>
#include "gda-object-ref.h"
#include <libgda/gda-util.h>
#include "gda-parameter-list.h"
#include <glib/gi18n-lib.h>

/* 
 * Main static functions 
 */
static void gda_dict_table_class_init (GdaDictTableClass * class);
static void gda_dict_table_init (GdaDictTable * srv);
static void gda_dict_table_dispose (GObject   * object);
static void gda_dict_table_finalize (GObject   * object);

static void gda_dict_table_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_table_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* XML storage interface */
static void        gda_dict_table_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gda_dict_table_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gda_dict_table_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_dict_table_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);

/* Entity interface */
static void             gda_dict_table_entity_init         (GdaEntityIface *iface);
static gboolean         gda_dict_table_has_field           (GdaEntity *iface, GdaEntityField *field);
static GSList          *gda_dict_table_get_fields          (GdaEntity *iface);
static GdaEntityField    *gda_dict_table_get_field_by_name   (GdaEntity *iface, const gchar *name);
static GdaEntityField    *gda_dict_table_get_field_by_xml_id (GdaEntity *iface, const gchar *xml_id);
static GdaEntityField    *gda_dict_table_get_field_by_index  (GdaEntity *iface, gint index);
static gint             gda_dict_table_get_field_index     (GdaEntity *iface, GdaEntityField *field);
static void             gda_dict_table_add_field           (GdaEntity *iface, GdaEntityField *field);
static void             gda_dict_table_add_field_before    (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before);
static void             gda_dict_table_swap_fields         (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2);
static void             gda_dict_table_remove_field        (GdaEntity *iface, GdaEntityField *field);
static gboolean         gda_dict_table_is_writable         (GdaEntity *iface);


static void        gda_dict_table_set_database        (GdaDictTable *table, GdaDictDatabase *db);
static void        gda_dict_table_add_field_at_pos    (GdaDictTable *table, GdaDictField *field, gint pos);

/* When the Database is destroyed */
static void        destroyed_object_cb (GObject *obj, GdaDictTable *table);
static void        destroyed_field_cb  (GObject *obj, GdaDictTable *table);
static void        destroyed_parent_cb (GObject *obj, GdaDictTable *table);

static void        changed_field_cb    (GObject *obj, GdaDictTable *table);


#ifdef GDA_DEBUG
static void        gda_dict_table_dump    (GdaDictTable *table, guint offset);
#endif


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_DB,
	PROP_IS_VIEW
};


/* private structure */
struct _GdaDictTablePrivate
{
	GdaDictDatabase *db;
	GSList          *fields;
	gboolean         is_view;
	GSList          *parents;     /* list of other GdaDictTable objects which are parents */
	GHashTable      *fields_hash; /* to improve fields retreival performances */
	gboolean         is_updating; /* TRUE when DBMS update in process */
};

/* module error */
GQuark gda_dict_table_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_table_error");
	return quark;
}


GType
gda_dict_table_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDictTableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_table_class_init,
			NULL,
			NULL,
			sizeof (GdaDictTable),
			0,
			(GInstanceInitFunc) gda_dict_table_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_dict_table_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo entity_info = {
			(GInterfaceInitFunc) gda_dict_table_entity_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictTable", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_ENTITY, &entity_info);
	}
	return type;
}

static void 
gda_dict_table_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = gda_dict_table_get_xml_id;
	iface->save_to_xml = gda_dict_table_save_to_xml;
	iface->load_from_xml = gda_dict_table_load_from_xml;
}

static void
gda_dict_table_entity_init (GdaEntityIface *iface)
{
	iface->has_field = gda_dict_table_has_field;
	iface->get_fields = gda_dict_table_get_fields;
	iface->get_field_by_name = gda_dict_table_get_field_by_name;
	iface->get_field_by_xml_id = gda_dict_table_get_field_by_xml_id;
	iface->get_field_by_index = gda_dict_table_get_field_by_index;
	iface->get_field_index = gda_dict_table_get_field_index;
	iface->add_field = gda_dict_table_add_field;
	iface->add_field_before = gda_dict_table_add_field_before;
	iface->swap_fields = gda_dict_table_swap_fields;
	iface->remove_field = gda_dict_table_remove_field;
	iface->is_writable = gda_dict_table_is_writable;
}

static void
gda_dict_table_class_init (GdaDictTableClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_dict_table_dispose;
	object_class->finalize = gda_dict_table_finalize;

	/* Properties */
	object_class->set_property = gda_dict_table_set_property;
	object_class->get_property = gda_dict_table_get_property;
	g_object_class_install_property (object_class, PROP_DB,
					 g_param_spec_object ("database", NULL, NULL, 
                                                               GDA_TYPE_DICT_DATABASE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_IS_VIEW,
					 g_param_spec_boolean ("is-view", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_dict_table_dump;
#endif

}

static void
gda_dict_table_init (GdaDictTable * gda_dict_table)
{
	gda_dict_table->priv = g_new0 (GdaDictTablePrivate, 1);
	gda_dict_table->priv->db = NULL;
	gda_dict_table->priv->fields = NULL;
	gda_dict_table->priv->is_view = FALSE;
	gda_dict_table->priv->parents = NULL;
	gda_dict_table->priv->fields_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	gda_dict_table->priv->is_updating = FALSE;
}

/**
 * gda_dict_table_new
 * @dict: a #GdaDict object
 *
 * Creates a new GdaDictTable object
 *
 * Returns: the new object
 */
GObject*
gda_dict_table_new (GdaDict *dict)
{
	GObject   *obj;
	GdaDictTable *gda_dict_table;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_DICT_TABLE, "dict", ASSERT_DICT (dict), NULL);
	gda_dict_table = GDA_DICT_TABLE (obj);

	return obj;
}

static void 
destroyed_field_cb (GObject *obj, GdaDictTable *table)
{
	gchar *str;
	g_assert (g_slist_find (table->priv->fields, obj));

	table->priv->fields = g_slist_remove (table->priv->fields, obj);
	g_signal_handlers_disconnect_by_func (G_OBJECT (obj), 
					      G_CALLBACK (destroyed_field_cb), table);
	g_signal_handlers_disconnect_by_func (G_OBJECT (obj), 
					      G_CALLBACK (changed_field_cb), table);
	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (obj));
	g_hash_table_remove (table->priv->fields_hash, str);
	g_free (str);

#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (table), "field_removed", obj);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_REMOVED' from %s\n", __FUNCTION__);
#endif

	g_object_set (obj, "db_table", NULL, NULL);	
	g_object_unref (obj);
}

static void 
destroyed_parent_cb (GObject *obj, GdaDictTable *table)
{
	g_assert (g_slist_find (table->priv->parents, obj));
	g_signal_handlers_disconnect_by_func (G_OBJECT (obj), 
					      G_CALLBACK (destroyed_parent_cb), table);
	table->priv->parents = g_slist_remove (table->priv->parents, obj);
}

static void
gda_dict_table_dispose (GObject *object)
{
	GdaDictTable *gda_dict_table;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_TABLE (object));

	gda_dict_table = GDA_DICT_TABLE (object);
	if (gda_dict_table->priv) {
		GSList *list;

		gda_object_destroy_check (GDA_OBJECT (object));

		gda_dict_table_set_database (gda_dict_table, NULL);

		if (gda_dict_table->priv->fields_hash) {
			g_hash_table_destroy (gda_dict_table->priv->fields_hash);
			gda_dict_table->priv->fields_hash = NULL;
		}

		while (gda_dict_table->priv->fields)
			gda_object_destroy (GDA_OBJECT (gda_dict_table->priv->fields->data));

		list = gda_dict_table->priv->parents;
		while (list) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
							      G_CALLBACK (destroyed_parent_cb), gda_dict_table);
			list = g_slist_next (list);
		}
		if (gda_dict_table->priv->parents) {
			g_slist_free (gda_dict_table->priv->parents);
			gda_dict_table->priv->parents = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_table_finalize (GObject   * object)
{
	GdaDictTable *gda_dict_table;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_TABLE (object));

	gda_dict_table = GDA_DICT_TABLE (object);
	if (gda_dict_table->priv) {

		g_free (gda_dict_table->priv);
		gda_dict_table->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_table_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	GdaDictTable *gda_dict_table;

	gda_dict_table = GDA_DICT_TABLE (object);
	if (gda_dict_table->priv) {
		switch (param_id) {
		case PROP_DB: {
			GdaDictDatabase* ptr = g_value_get_object (value);
			gda_dict_table_set_database (gda_dict_table, GDA_DICT_DATABASE (ptr));
			break;
                }
		case PROP_IS_VIEW:
			gda_dict_table->priv->is_view = g_value_get_boolean (value);
			break;
		}
	}
}

static void
gda_dict_table_get_property (GObject *object,
			  guint param_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	GdaDictTable *gda_dict_table;
	gda_dict_table = GDA_DICT_TABLE (object);
	
	if (gda_dict_table->priv) {
		switch (param_id) {
		case PROP_DB:
			g_value_set_object (value, G_OBJECT (gda_dict_table->priv->db));
			break;
		case PROP_IS_VIEW:
			g_value_set_boolean (value, gda_dict_table->priv->is_view);
			break;
		}	
	}
}

static void
gda_dict_table_set_database (GdaDictTable *table, GdaDictDatabase *db)
{
	if (table->priv->db) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (table->priv->db),
						      G_CALLBACK (destroyed_object_cb), table);
		table->priv->db = NULL;
	}
	
	if (db && GDA_IS_DICT_DATABASE (db)) {
		table->priv->db = GDA_DICT_DATABASE (db);
		gda_object_connect_destroy (db, G_CALLBACK (destroyed_object_cb), table);
	}
}

static void
destroyed_object_cb (GObject *obj, GdaDictTable *table)
{
	gda_object_destroy (GDA_OBJECT (table));
}

/**
 * gda_dict_table_get_database
 * @table: a #GdaDictTable object
 *
 * Get the database to which the table belongs
 *
 * Returns: a #GdaDictDatabase pointer
 */
GdaDictDatabase
*gda_dict_table_get_database (GdaDictTable *table)
{
	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), NULL);
	g_return_val_if_fail (table->priv, NULL);

	return table->priv->db;
}

/**
 * gda_dict_table_is_view
 * @table: a #GdaDictTable object
 *
 * Does the object represent a view rather than a table?
 *
 * Returns: TRUE if it is a view
 */
gboolean
gda_dict_table_is_view (GdaDictTable *table)
{
	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), FALSE);
	g_return_val_if_fail (table->priv, FALSE);

	return table->priv->is_view;
}

/**
 * gda_dict_table_get_parents
 * @table: a #GdaDictTable object
 *
 * Get the parent tables of the table given as argument. This is significant only
 * for DBMS which support tables inheritance (like PostgreSQL for example).
 *
 * Returns: a constant list of #GdaDictTable objects
 */
const GSList *
gda_dict_table_get_parents (GdaDictTable *table)
{
	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), NULL);
	g_return_val_if_fail (table->priv, NULL);

	return table->priv->parents;
}

/**
 * gda_dict_table_get_constraints
 * @table: a #GdaDictTable object
 *
 * Get all the constraints which apply to the given table (each constraint
 * can represent a NOT NULL, a primary key or foreign key or a check constraint.
 *
 * Returns: a new list of #GdaDictConstraint objects
 */
GSList *
gda_dict_table_get_constraints (GdaDictTable *table)
{
	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), NULL);
	g_return_val_if_fail (table->priv, NULL);

	return gda_dict_database_get_table_constraints (table->priv->db, table);
}

/**
 * gda_dict_table_get_pk_constraint
 * @table: a #GdaDictTable object
 *
 * Get the primary key constraint of @table, if there is any. If several
 * #GdaDictConstraint represent a primary key constraint for @table, then
 * the first one in the list of constraints is returned.
 *
 * Returns: a #GdaDictConstraint object or %NULL.
 */
GdaDictConstraint *
gda_dict_table_get_pk_constraint (GdaDictTable *table)
{
	GdaDictConstraint *pkcons = NULL;
	GSList *db_constraints, *list;

	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (table)->priv, NULL);

	db_constraints = gda_dict_database_get_all_constraints (table->priv->db);
	list = db_constraints;
	while (list && !pkcons) {
		if ((gda_dict_constraint_get_table (GDA_DICT_CONSTRAINT (list->data)) == table) &&
		    (gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) == 
		     CONSTRAINT_PRIMARY_KEY))
			pkcons = GDA_DICT_CONSTRAINT (list->data);

		list = g_slist_next (list);
	}
	g_slist_free (db_constraints);

	return pkcons;
}

/**
 * gda_dict_table_update_dbms_data
 * @table: a #GdaDictTable object
 * @error: location to store error, or %NULL
 *
 * Synchronises the Table representation with the table structure which is stored in
 * the DBMS. For this operation to succeed, the connection to the DBMS server MUST be opened
 * (using the corresponding #GdaConnection object).
 *
 * Returns: TRUE if no error
 */
gboolean
gda_dict_table_update_dbms_data (GdaDictTable *table, GError **error)
{
	GdaDict *dict;
	GSList *fields;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *updated_fields = NULL;
	GdaConnection *cnc;
	GdaDictField *field;
	GdaParameterList *paramlist;
        GdaParameter *param;
	gint current_position = 0;
	GSList *constraints;
	GHashTable *fk_hash;
	gboolean has_extra_attributes = FALSE;
	
	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), FALSE);
	g_return_val_if_fail (GDA_DICT_TABLE (table)->priv, FALSE);
	table->priv->is_updating = TRUE;

	/* g_print ("################ TABLE %s\n", gda_object_get_name (GDA_OBJECT (table))); */
	dict = gda_object_get_dict (GDA_OBJECT (table));
	cnc = gda_dict_get_connection (dict);
	if (!cnc) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
                             _("No connection associated to dictionary!"));
		table->priv->is_updating = FALSE;
                return FALSE;
	}
	if (!gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_TABLE_META_DATA_UPDATE,
			     _("Connection is not opened!"));
		table->priv->is_updating = FALSE;
		return FALSE;
	}

	/* In this procedure, we are creating some new GdaDictConstraint objects to express
	 * the constraints the database has on this table. This list is "attached" to the GdaDictTable
	 * object by the "pending_constraints" keyword, and is later normally fetched by the GdaDictDatabase object.
	 * If we still have one at this point, we must emit a warning and free that list.
	 */
	if ((constraints = g_object_get_data (G_OBJECT (table), "pending_constraints"))) {
		GSList *list = constraints;

		g_warning ("GdaDictTable object %p has a non empty list of pending constraints", table);
		while (list) {
			g_object_unref (G_OBJECT (list->data));
			list = g_slist_next (list);
		}
		g_slist_free (constraints);
		constraints = NULL;
		g_object_set_data (G_OBJECT (table), "pending_constraints", NULL);
	}
	fk_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* parameters list */
	paramlist = gda_parameter_list_new (NULL);
	param = gda_parameter_new_string ("name", gda_object_get_name (GDA_OBJECT (table)));
        gda_parameter_list_add_param (paramlist, param);
	g_object_unref (param);
	rs = gda_connection_get_schema (cnc, GDA_CONNECTION_SCHEMA_FIELDS, paramlist, NULL);
	g_object_unref (paramlist);

	/* Result set analysis */
	if (!gda_utility_check_data_model (rs, 9, 
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_INT,
				       G_TYPE_INT,
				       G_TYPE_BOOLEAN,
				       G_TYPE_BOOLEAN,
				       G_TYPE_BOOLEAN,
				       G_TYPE_STRING, 
				       -1)) {
		g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_FIELDS_ERROR,
			     _("Schema for list of fields is wrong"));
		g_object_unref (G_OBJECT (rs));
		table->priv->is_updating = FALSE;
		return FALSE;
	}

	if (gda_data_model_get_n_columns (GDA_DATA_MODEL (rs)) == 10) {
		GdaColumn *att;
		att = gda_data_model_describe_column (GDA_DATA_MODEL (rs), 9);
		
		if (gda_column_get_g_type (att) != G_TYPE_STRING) {
			g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_FIELDS_ERROR,
				     _("Schema for list of fields is wrong"));
			g_object_unref (G_OBJECT (rs));
		}

		has_extra_attributes = TRUE;
	}

	/* Resultset parsing */
	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while (now < total) {
		const GValue *value;
		gboolean newfield = FALSE;
		GdaEntityField *mgf;

		value = gda_data_model_get_value_at (rs, 0, now);
		str = gda_value_stringify ((GValue *) value);
		mgf = gda_dict_table_get_field_by_name (GDA_ENTITY (table), str);
		if (!mgf) {
			/* field name */
			field = GDA_DICT_FIELD (gda_dict_field_new (gda_object_get_dict (GDA_OBJECT (table)), NULL));
			gda_object_set_name (GDA_OBJECT (field), str);
			newfield = TRUE;
		}
		else {
			field = GDA_DICT_FIELD (mgf);
			current_position = g_slist_index (table->priv->fields, field) + 1;
		}
		g_free (str);
		
		updated_fields = g_slist_append (updated_fields, field);

		/* FIXME: No description for fields in the schema. */
		/* FIXME: No owner for fields in the schema. */
		
		/* Data type */
		value = gda_data_model_get_value_at (rs, 1, now);
		if (value && !gda_value_is_null ((GValue *) value) && 
		    g_value_get_string ((GValue *) value) && (* g_value_get_string ((GValue *) value))) {
			GdaDictType *type;

			str = gda_value_stringify ((GValue *) value);
			type = gda_dict_get_dict_type_by_name (dict, str);
			if (type)
				gda_entity_field_set_dict_type (GDA_ENTITY_FIELD (field), type);
			else {
				/* declare a custom data type */
				gchar *descr;
				type = gda_dict_type_new (dict);
				gda_dict_type_set_sqlname (type, str);
				gda_dict_type_set_g_type (type, GDA_TYPE_BLOB);
				descr = g_strdup_printf (_("Custom data type, declared for the %s.%s field"),
							 gda_object_get_name (GDA_OBJECT (table)),
							 gda_object_get_name (GDA_OBJECT (field)));
				gda_object_set_description (GDA_OBJECT (type), descr);
				g_free (descr);
				gda_dict_declare_object (dict, (GdaObject *) type);
				gda_entity_field_set_dict_type (GDA_ENTITY_FIELD (field), type);
				g_object_unref (type);
			}
			g_free (str);
		}
		if (!gda_entity_field_get_dict_type (GDA_ENTITY_FIELD (field))) {
			if (value)
				str = gda_value_stringify ((GValue *) value);
			else
				str = g_strdup ("NULL");
			g_set_error (error, GDA_DICT_TABLE_ERROR, GDA_DICT_FIELDS_ERROR,
				     _("Can't find data type %s"), str);
			g_free (str);
			g_object_unref (G_OBJECT (rs));
			table->priv->is_updating = FALSE;
			return FALSE;
		}

		/* Size */
		value = gda_data_model_get_value_at (rs, 2, now);
		if (value && !gda_value_is_null ((GValue *) value)) 
			gda_dict_field_set_length (field, g_value_get_int ((GValue *) value));

		/* Scale */
		value = gda_data_model_get_value_at (rs, 3, now);
		if (value && !gda_value_is_null ((GValue *) value)) 
			gda_dict_field_set_scale (field, g_value_get_int ((GValue *) value));

		/* Default value */
		value = gda_data_model_get_value_at (rs, 8, now);
		if (value && !gda_value_is_null ((GValue *) value)) {
			gchar *defv = gda_value_stringify ((GValue *) value);
			if (defv && *defv)
				gda_dict_field_set_default_value (field, value);
			if (defv)
				g_free (defv);
		}
				
		/* signal if the field is new or updated */
		if (newfield) {
			g_object_set (G_OBJECT (field), "db_table", table, NULL);
			gda_dict_table_add_field_at_pos (table, field, current_position++);
			g_object_unref (G_OBJECT (field));
		}
		else {
			if (g_object_get_data (G_OBJECT (field), ".gda")) {
#ifdef GDA_DEBUG_signal
				g_print (">> 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif
				g_signal_emit_by_name (G_OBJECT (table), "field_updated", field);
#ifdef GDA_DEBUG_signal
				g_print ("<< 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif 
			}
		}
		g_object_set_data (G_OBJECT (field), ".gda", NULL);
		
		/* NOT NULL constraint */
		value = gda_data_model_get_value_at (rs, 4, now);
		if (value && !gda_value_is_null ((GValue *) value) && g_value_get_boolean ((GValue *) value)) {
			GdaDictConstraint *cstr = GDA_DICT_CONSTRAINT (gda_dict_constraint_new (table, CONSTRAINT_NOT_NULL));
			gda_dict_constraint_not_null_set_field (cstr, field);
			constraints = g_slist_append (constraints, cstr);
		}
		else {
			/* remove the constraint */
			GdaDictConstraint *cstr = NULL;
			GSList *list, *table_cons;
			
			/* find NOT NULL constraint */
			table_cons = gda_dict_table_get_constraints (table);
			list = table_cons;
			while (list && !cstr) {
				if ((gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data))==CONSTRAINT_NOT_NULL) &&
				    gda_dict_constraint_uses_field (GDA_DICT_CONSTRAINT (list->data), field))
					cstr = GDA_DICT_CONSTRAINT (list->data);
				list = g_slist_next (list);
			}
			g_slist_free (table_cons);

			if (cstr) 
				/* remove that constraint */
				gda_object_destroy (GDA_OBJECT (cstr));
		}

		/* Other constraints:
		 * For constraints other than the NOT NULL constraint, this is a temporary solution to
		 * get the constraints before we get a real constraints request schema in libgda.
		 *
		 * THE FOLLOWING ASSUMPTIONS ARE MADE:
		 * PRIMARY KEY: there is only ONE primary key per table even if it is a composed primary key
		 * UNIQUE: each field with the UNIQUE attribute is considered unique itself (=> no constraint
		 * for unique couple (or more) fields)
		 * FOREIGN KEY: we don't have the associated actions to ON UPDATE and ON DELETE actions
		 */

		/* PRIMARY KEY constraint */
		value = gda_data_model_get_value_at (rs, 5, now);
		if (value && !gda_value_is_null ((GValue *) value) && g_value_get_boolean ((GValue *) value)) {
			GdaDictConstraint *cstr = NULL;
			GSList *list = constraints, *nlist;
			
			/* find the primary key constraint if it already exists */
			while (list && !cstr) {
				if (gda_dict_constraint_get_constraint_type (GDA_DICT_CONSTRAINT (list->data)) ==
				    CONSTRAINT_PRIMARY_KEY)
					cstr = GDA_DICT_CONSTRAINT (list->data);
				list = g_slist_next (list);
			}

			if (!cstr) {
				cstr = GDA_DICT_CONSTRAINT (gda_dict_constraint_new (table, CONSTRAINT_PRIMARY_KEY));
				constraints = g_slist_append (constraints, cstr);
			}

			/* set the fields */
			nlist = gda_dict_constraint_pkey_get_fields (cstr);
			nlist = g_slist_append (nlist, field);
			gda_dict_constraint_pkey_set_fields (cstr, nlist);
			g_slist_free (nlist);
		}

		/* UNIQUE constraint */
		value = gda_data_model_get_value_at (rs, 6, now);
		if (value && !gda_value_is_null ((GValue *) value) && g_value_get_boolean ((GValue *) value)) {
			GdaDictConstraint *cstr;
			GSList *nlist;

			cstr = GDA_DICT_CONSTRAINT (gda_dict_constraint_new (table, CONSTRAINT_UNIQUE));
			constraints = g_slist_append (constraints, cstr);

			nlist = g_slist_append (NULL, field);
			gda_dict_constraint_unique_set_fields (cstr, nlist);
			g_slist_free (nlist);
		}

		/* FOREIGN KEY constraint */
		value = gda_data_model_get_value_at (rs, 7, now);
		if (value && !gda_value_is_null ((GValue *) value) && g_value_get_string ((GValue *) value) && 
		    (* g_value_get_string ((GValue *) value))) {
			gchar *ref_table, *str, *tok;
			GdaObjectRef *ref;
			GdaDictConstraint *cstr = NULL;
			GSList *list, *nlist;
			GdaDictConstraintFkeyPair *pair;

			/* ref table */
			str = g_strdup (g_value_get_string ((GValue *) value));
			ref_table = g_strdup (strtok_r (str, ".", &tok));
			g_free (str);
			
			ref = GDA_OBJECT_REF (gda_object_ref_new (gda_object_get_dict (GDA_OBJECT (table))));
			gda_object_ref_set_ref_name (ref, GDA_TYPE_DICT_FIELD, 
						  REFERENCE_BY_NAME, g_value_get_string ((GValue *) value));
			
			/* find the foreign key constraint if it already exists */
			cstr = g_hash_table_lookup (fk_hash, ref_table);
			if (!cstr) {
				cstr = GDA_DICT_CONSTRAINT (gda_dict_constraint_new (table, CONSTRAINT_FOREIGN_KEY));
				constraints = g_slist_append (constraints, cstr);
				g_hash_table_insert (fk_hash, ref_table, cstr);
			}
			else
				g_free (ref_table);
			
			nlist = gda_dict_constraint_fkey_get_fields (cstr);
			pair = g_new0 (GdaDictConstraintFkeyPair, 1);
			pair->fkey = field;
			pair->ref_pkey = NULL;
			pair->ref_pkey_repl = ref;
			nlist = g_slist_append (nlist, pair);
			gda_dict_constraint_fkey_set_fields (cstr, nlist);
			
			/* memory libreation */
			list = nlist;
			while (list) {
				g_free (list->data);
				list = g_slist_next (list);
			}
			g_object_unref (G_OBJECT (ref));
			g_slist_free (nlist);
		}

		/* EXTRA attributes if supported by the schema */
		if (has_extra_attributes) {
			value = gda_data_model_get_value_at (rs, 9, now);
			
			if (! gda_value_is_null ((GValue *) value))
				gda_dict_field_set_attributes (field, 
					 gda_utility_table_field_attrs_parse (g_value_get_string ((GValue *) value)));
		}
		
		now++;
	}
	    
	g_object_unref (G_OBJECT (rs));
	
	/* remove the fields not existing anymore */
	fields = table->priv->fields;
	while (fields) {
		if (!g_slist_find (updated_fields, fields->data)) {
			gda_object_destroy (GDA_OBJECT (fields->data));
			fields = table->priv->fields;
		}
		else
			fields = g_slist_next (fields);
	}
	g_slist_free (updated_fields);

	/* stick the constraints list to the GdaDictTable object */
	g_object_set_data (G_OBJECT (table), "pending_constraints", constraints);
	g_hash_table_destroy (fk_hash);
	
	table->priv->is_updating = FALSE;
	return TRUE;
}


/*
 * pos = -1 to append
 */
static void
gda_dict_table_add_field_at_pos (GdaDictTable *table, GdaDictField *field, gint pos)
{
	gchar *str;
	table->priv->fields = g_slist_insert (table->priv->fields, field, pos);
	
	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (field));
	g_hash_table_insert (table->priv->fields_hash, str, field);

	g_object_ref (G_OBJECT (field));
	gda_object_connect_destroy (field, 
				 G_CALLBACK (destroyed_field_cb), table);
	g_signal_connect (G_OBJECT (field), "changed",
			  G_CALLBACK (changed_field_cb), table);

#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (table), "field_added", field);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELD_ADDED' from %s\n", __FUNCTION__);
#endif	
}


static void
changed_field_cb (GObject *obj, GdaDictTable *table)
{
	if (table->priv->is_updating) 
		g_object_set_data (G_OBJECT (obj), ".gda", GINT_TO_POINTER (1));
	else {
#ifdef GDA_DEBUG_signal
		g_print (">> 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (table), "field_updated", obj);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'FIELD_UPDATED' from %s\n", __FUNCTION__);
#endif	
	}
}


#ifdef GDA_DEBUG
static void
gda_dict_table_dump (GdaDictTable *table, guint offset)
{
	gchar *str;
        guint i;
        GSList *list;
	
	g_return_if_fail (table && GDA_IS_DICT_TABLE (table));

        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (table->priv)
                g_print ("%s" D_COL_H1 "GdaDictTable" D_COL_NOR  " %s (%p)\n",
                         str, gda_object_get_name (GDA_OBJECT (table)), table);
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, table);

	/* fields */
        list = table->priv->fields;
        if (list) {
                g_print ("%sFields:\n", str);
                while (list) {
                        gda_object_dump (GDA_OBJECT (list->data), offset+5);
                        list = g_slist_next (list);
                }
        }
        else
                g_print ("%sContains no field\n", str);

}
#endif





/* 
 * GdaEntity interface implementation
 */
static gboolean
gda_dict_table_has_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, FALSE);

	return g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field) ? TRUE : FALSE;
}

static GSList *
gda_dict_table_get_fields (GdaEntity *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, NULL);

	return g_slist_copy (GDA_DICT_TABLE (iface)->priv->fields);
}

static GdaEntityField *
gda_dict_table_get_field_by_name (GdaEntity *iface, const gchar *name)
{
	GdaEntityField *field = NULL;
	GSList *list;
	gchar *lcname = g_utf8_strdown (name, -1);

	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, NULL);

	list = GDA_DICT_TABLE (iface)->priv->fields;
	while (list && !field) {
		if (!strcmp (gda_entity_field_get_name (GDA_ENTITY_FIELD (list->data)), lcname) ||
		    !strcmp (gda_entity_field_get_name (GDA_ENTITY_FIELD (list->data)), name))
			field = GDA_ENTITY_FIELD (list->data);
		list = g_slist_next (list);
	}
	g_free (lcname);

	return field;
}

static GdaEntityField *
gda_dict_table_get_field_by_xml_id (GdaEntity *iface, const gchar *xml_id)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, NULL);

	return g_hash_table_lookup (GDA_DICT_TABLE (iface)->priv->fields_hash, xml_id);
}

static GdaEntityField *
gda_dict_table_get_field_by_index (GdaEntity *iface, gint index)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, NULL);
	g_return_val_if_fail (index >= 0, NULL);
	g_return_val_if_fail (index < g_slist_length (GDA_DICT_TABLE (iface)->priv->fields), NULL);
	
	return GDA_ENTITY_FIELD (g_slist_nth_data (GDA_DICT_TABLE (iface)->priv->fields, index));
}

static gint
gda_dict_table_get_field_index (GdaEntity *iface, GdaEntityField *field)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), -1);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, -1);

	return g_slist_index (GDA_DICT_TABLE (iface)->priv->fields, field);
}

static void
gda_dict_table_add_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_if_fail (iface && GDA_IS_DICT_TABLE (iface));
	g_return_if_fail (GDA_DICT_TABLE (iface)->priv);
	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (!g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field));
	g_return_if_fail (gda_entity_field_get_entity (field) == iface);
	
	gda_dict_table_add_field_at_pos (GDA_DICT_TABLE (iface), GDA_DICT_FIELD (field), -1);
}

static void
gda_dict_table_add_field_before (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before)
{
	GdaDictTable *table;
	gint pos = -1;

	g_return_if_fail (iface && GDA_IS_DICT_TABLE (iface));
	g_return_if_fail (GDA_DICT_TABLE (iface)->priv);
	table = GDA_DICT_TABLE (iface);

	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (!g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field));
	g_return_if_fail (gda_entity_field_get_entity (field) == iface);
	if (field_before) {
		g_return_if_fail (field_before && GDA_IS_DICT_FIELD (field_before));
		g_return_if_fail (g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field_before));
		pos = g_slist_index (table->priv->fields, field_before);
	}

	gda_dict_table_add_field_at_pos (table, GDA_DICT_FIELD (field), pos);
}

static void
gda_dict_table_swap_fields (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2)
{
	GSList *ptr1, *ptr2;

	g_return_if_fail (iface && GDA_IS_DICT_TABLE (iface));
	g_return_if_fail (GDA_DICT_TABLE (iface)->priv);
	g_return_if_fail (field1 && GDA_IS_DICT_FIELD (field1));
	g_return_if_fail (field2 && GDA_IS_DICT_FIELD (field2));
	ptr1 = g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field1);
	ptr2 = g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field2);
	g_return_if_fail (ptr1);
	g_return_if_fail (ptr2);
	
	ptr1->data = field2;
	ptr2->data = field1;

#ifdef GDA_DEBUG_signal
	g_print (">> 'FIELDS_ORDER_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit_by_name (G_OBJECT (iface), "fields_order_changed");
#ifdef GDA_DEBUG_signal
	g_print ("<< 'FIELDS_ORDER_CHANGED' from %s\n", __FUNCTION__);
#endif

}

static void
gda_dict_table_remove_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_if_fail (iface && GDA_IS_DICT_TABLE (iface));
	g_return_if_fail (GDA_DICT_TABLE (iface)->priv);
	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (g_slist_find (GDA_DICT_TABLE (iface)->priv->fields, field));

	destroyed_field_cb (G_OBJECT (field), GDA_DICT_TABLE (iface));
}

static gboolean
gda_dict_table_is_writable (GdaEntity *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, FALSE);
	
	return GDA_DICT_TABLE (iface)->priv->is_view ? FALSE : TRUE;
}

/* 
 * GdaXmlStorage interface implementation
 */
static gchar *
gda_dict_table_get_xml_id (GdaXmlStorage *iface)
{
	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, NULL);

	return gda_utility_build_encoded_id ("TV", gda_object_get_name (GDA_OBJECT (iface)));
}

static xmlNodePtr
gda_dict_table_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	GdaDictTable *table;
	gchar *str;
	const gchar *cstr;
	GSList *list;
	gint i;

	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), NULL);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, NULL);

	table = GDA_DICT_TABLE (iface);

	node = xmlNewNode (NULL, (xmlChar*)"gda_dict_table");
	
	str = gda_dict_table_get_xml_id (iface);
	xmlSetProp(node, (xmlChar*)"id", (xmlChar*)str);
	g_free (str);
	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (table)));
	cstr = gda_object_get_owner (GDA_OBJECT (table));
	if (cstr && *cstr)
		xmlSetProp(node, (xmlChar*)"owner", (xmlChar*)cstr);
	xmlSetProp(node, (xmlChar*)"descr", (xmlChar*)gda_object_get_description (GDA_OBJECT (table)));

	xmlSetProp(node, (xmlChar*)"is_view", (xmlChar*)(table->priv->is_view ? "t" : "f"));

	/* parent tables */
	i = 0;
	list = table->priv->parents;
	while (list) {
		xmlNodePtr parent;
		gchar *str;

		parent = xmlNewChild (node, NULL, (xmlChar*)"gda_dict_parent_table", NULL);
		str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
		xmlSetProp(parent, (xmlChar*)"table", (xmlChar*)str);
		g_free (str);

		str = g_strdup_printf ("%d", i);
		xmlSetProp(parent, (xmlChar*)"order", (xmlChar*)str);
		g_free (str);

		list = g_slist_next (list);
	}
	
	/* fields */
	list = table->priv->fields;
	while (list) {
		xmlNodePtr field;
		
		field = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);

		if (field)
			xmlAddChild (node, field);
		else {
			xmlFreeNode (node);
			return NULL;
		}
		list = g_slist_next (list);
	}

	return node;
}

static gboolean
gda_dict_table_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDictTable *table;
	gchar *prop;
	gboolean name = FALSE;
	xmlNodePtr children;

	g_return_val_if_fail (iface && GDA_IS_DICT_TABLE (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_TABLE (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);

	table = GDA_DICT_TABLE (iface);
	if (strcmp ((gchar*)node->name, "gda_dict_table")) {
		g_set_error (error,
			     GDA_DICT_TABLE_ERROR,
			     GDA_DICT_TABLE_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_table>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
	if (prop) {
		name = TRUE;
		gda_object_set_name (GDA_OBJECT (table), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"descr");
	if (prop) {
		gda_object_set_description (GDA_OBJECT (table), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"owner");
	if (prop) {
		gda_object_set_owner (GDA_OBJECT (table), prop);
		g_free (prop);
	}

	table->priv->is_view = FALSE;
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"is_view");
	if (prop) {
		table->priv->is_view = (*prop == 't') ? TRUE : FALSE;
		g_free (prop);
	}

	children = node->children;
	while (children) {
		gboolean done = FALSE;

		/* parent table */
		if (!strcmp ((gchar*)children->name, "gda_dict__parent_table")) {
			TO_IMPLEMENT;
			done = TRUE;
		}
		/* fields */
		if (!done && !strcmp ((gchar*)children->name, "gda_dict_field")) {
			GdaDictField *field;
			field = GDA_DICT_FIELD (gda_dict_field_new (gda_object_get_dict (GDA_OBJECT (iface)), NULL));
			if (gda_xml_storage_load_from_xml (GDA_XML_STORAGE (field), children, error)) {
				g_object_set (G_OBJECT (field), "db_table", table, NULL);
				gda_dict_table_add_field (GDA_ENTITY (table), GDA_ENTITY_FIELD (field));
				g_object_unref (G_OBJECT (field));
			}
			else
				return FALSE;
		}
		
		children = children->next;
	}

	if (name)
		return TRUE;
	else {
		g_set_error (error,
			     GDA_DICT_TABLE_ERROR,
			     GDA_DICT_TABLE_XML_LOAD_ERROR,
			     _("Missing required attributes for <gda_dict_table>"));
		return FALSE;
	}
}

