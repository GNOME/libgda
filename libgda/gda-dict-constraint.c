/* gda-dict-constraint.c
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

#include "gda-dict-constraint.h"
#include "gda-referer.h"
#include "gda-object-ref.h"
#include "gda-dict-database.h"
#include "gda-dict-table.h"
#include "gda-xml-storage.h"
#include "gda-entity.h"
#include "gda-dict-field.h"
#include "gda-entity-field.h"
#include <libgda/gda-util.h>
#include <glib/gi18n-lib.h>
#include <string.h>

/* 
 * Main static functions 
 */
static void gda_dict_constraint_class_init (GdaDictConstraintClass * class);
static void gda_dict_constraint_init (GdaDictConstraint * srv);
static void gda_dict_constraint_dispose (GObject   * object);
static void gda_dict_constraint_finalize (GObject   * object);

static void gda_dict_constraint_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_dict_constraint_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);

/* XML storage interface */
static void        gda_dict_constraint_xml_storage_init (GdaXmlStorageIface *iface);
static gchar      *gda_dict_constraint_get_xml_id (GdaXmlStorage *iface);
static xmlNodePtr  gda_dict_constraint_save_to_xml (GdaXmlStorage *iface, GError **error);
static gboolean    gda_dict_constraint_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error);


/* GdaReferer interface */
static void        gda_dict_constraint_referer_init (GdaRefererIface *iface);
static gboolean    gda_dict_constraint_activate            (GdaReferer *iface);
static gboolean    gda_dict_constraint_is_active           (GdaReferer *iface);

#ifdef GDA_DEBUG
static void        gda_dict_constraint_dump                (GdaDictConstraint *cstr, guint offset);
#endif

/* When a DbTable or GdaDictField is destroyed */
static void destroyed_object_cb (GObject *obj, GdaDictConstraint *cstr);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum
{
	PROP_0,
	PROP_USER_CSTR
};


/* private structure */
struct _GdaDictConstraintPrivate
{
	GdaDictConstraintType      type;
	GdaDictTable              *table;
	gboolean                   user_defined; /* FALSE if the constraint is hard coded into the database */
	
	/* Only used if single field constraint */
	GdaDictField              *single_field;

	/* Only used if Primary key */
	GSList                    *multiple_fields; 

	/* Only used for foreign keys */
	GdaDictTable              *ref_table; 
	GSList                    *fk_pairs;  
	GdaDictConstraintFkAction  on_delete;
	GdaDictConstraintFkAction  on_update;

	/* Only used for IN and SETOF constraints */
	GdaDataModel              *cstr_data;
	gint                       cstr_data_row;
};


/* module error */
GQuark gda_dict_constraint_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_dict_constraint_error");
	return quark;
}


GType
gda_dict_constraint_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDictConstraintClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_dict_constraint_class_init,
			NULL,
			NULL,
			sizeof (GdaDictConstraint),
			0,
			(GInstanceInitFunc) gda_dict_constraint_init
		};

		static const GInterfaceInfo xml_storage_info = {
			(GInterfaceInitFunc) gda_dict_constraint_xml_storage_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_dict_constraint_referer_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDictConstraint", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_XML_STORAGE, &xml_storage_info);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void 
gda_dict_constraint_xml_storage_init (GdaXmlStorageIface *iface)
{
	iface->get_xml_id = gda_dict_constraint_get_xml_id;
	iface->save_to_xml = gda_dict_constraint_save_to_xml;
	iface->load_from_xml = gda_dict_constraint_load_from_xml;
}

static void
gda_dict_constraint_referer_init (GdaRefererIface *iface)
{
	iface->activate = gda_dict_constraint_activate;
	iface->deactivate = NULL;
	iface->is_active = gda_dict_constraint_is_active;
	iface->get_ref_objects = NULL;
	iface->replace_refs = NULL;
}

static void
gda_dict_constraint_class_init (GdaDictConstraintClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_dict_constraint_dispose;
	object_class->finalize = gda_dict_constraint_finalize;

	/* Properties */
	object_class->set_property = gda_dict_constraint_set_property;
	object_class->get_property = gda_dict_constraint_get_property;
	g_object_class_install_property (object_class, PROP_USER_CSTR,
					 g_param_spec_boolean ("user_constraint", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/* virtual functions */
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_dict_constraint_dump;
#endif
}

static void
gda_dict_constraint_init (GdaDictConstraint * gda_dict_constraint)
{
	gda_dict_constraint->priv = g_new0 (GdaDictConstraintPrivate, 1);
	gda_dict_constraint->priv->type = CONSTRAINT_PRIMARY_KEY;
	gda_dict_constraint->priv->table = NULL;
	gda_dict_constraint->priv->user_defined = FALSE;
	gda_dict_constraint->priv->single_field = NULL;
	gda_dict_constraint->priv->multiple_fields = NULL;
	gda_dict_constraint->priv->ref_table = NULL;
	gda_dict_constraint->priv->fk_pairs = NULL;
	gda_dict_constraint->priv->on_delete = CONSTRAINT_FK_ACTION_NO_ACTION;
	gda_dict_constraint->priv->on_update = CONSTRAINT_FK_ACTION_NO_ACTION;
}


/**
 * gda_dict_constraint_new
 * @table: the #GdaDictTable to which the constraint is attached
 * @type: the type of constraint
 *
 * Creates a new GdaDictConstraint object
 *
 * Returns: the new object
 */
GdaDictConstraint*
gda_dict_constraint_new (GdaDictTable *table, GdaDictConstraintType type)
{
	GObject   *obj;
	GdaDictConstraint *gda_dict_constraint;
	GdaDict *dict = NULL;

	g_return_val_if_fail (table && GDA_IS_DICT_TABLE (table), NULL);
	dict = gda_object_get_dict (GDA_OBJECT (table));
	
	obj = g_object_new (GDA_TYPE_DICT_CONSTRAINT, "dict", dict, NULL);

	gda_dict_constraint = GDA_DICT_CONSTRAINT (obj);

	gda_dict_constraint->priv->type = type;
	gda_dict_constraint->priv->table = table;

	gda_object_connect_destroy (table, G_CALLBACK (destroyed_object_cb), gda_dict_constraint);

	return gda_dict_constraint;
}

/**
 * gda_dict_constraint_new_with_db
 * @db: a #GdaDictDatabase object
 * 
 * Creates a new #GdaDictConstraint object without specifying anything about the
 * constraint except the database it is attached to. This is usefull only
 * when the object is going to be loaded from an XML node.
 *
 * Returns: the new uninitialized object
 */
GdaDictConstraint *
gda_dict_constraint_new_with_db (GdaDictDatabase *db)
{
	GObject   *obj;
	GdaDictConstraint *gda_dict_constraint;
	GdaDict *dict = NULL;

	/* here priv->table will be NULL and a "db" data is attached to the object */

	g_return_val_if_fail (db && GDA_IS_DICT_DATABASE (db), NULL);
	dict = gda_object_get_dict (GDA_OBJECT (db));
	
	obj = g_object_new (GDA_TYPE_DICT_CONSTRAINT, "dict", dict, NULL);

	gda_dict_constraint = GDA_DICT_CONSTRAINT (obj);
	
	g_object_set_data (obj, "db", db);

	gda_object_connect_destroy (db, G_CALLBACK (destroyed_object_cb), gda_dict_constraint);
	return gda_dict_constraint;
}

static void 
destroyed_object_cb (GObject *obj, GdaDictConstraint *cstr)
{
	gda_object_destroy (GDA_OBJECT (cstr));
}

static void
gda_dict_constraint_dispose (GObject *object)
{
	GdaDictConstraint *cstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_CONSTRAINT (object));

	cstr = GDA_DICT_CONSTRAINT (object);
	if (cstr->priv) {
		GSList *list;

		gda_object_destroy_check (GDA_OBJECT (object));

		switch (cstr->priv->type) {
		case CONSTRAINT_PRIMARY_KEY:
		case CONSTRAINT_UNIQUE:
			list = cstr->priv->multiple_fields;
			while (list) {
				g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
								      G_CALLBACK (destroyed_object_cb), cstr);
				list = g_slist_next (list);
			}
			g_slist_free (cstr->priv->multiple_fields);
			cstr->priv->multiple_fields = NULL;
			break;
		case CONSTRAINT_FOREIGN_KEY:
			if (cstr->priv->ref_table)
				g_signal_handlers_disconnect_by_func (G_OBJECT (cstr->priv->ref_table),
								      G_CALLBACK (destroyed_object_cb), cstr);
			cstr->priv->ref_table = NULL;

			list = cstr->priv->fk_pairs;
			while (list) {
				GdaDictConstraintFkeyPair *pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);

				g_signal_handlers_disconnect_by_func (G_OBJECT (pair->fkey),
								      G_CALLBACK (destroyed_object_cb), cstr);
				if (pair->ref_pkey)
					g_signal_handlers_disconnect_by_func (G_OBJECT (pair->ref_pkey),
									      G_CALLBACK (destroyed_object_cb), cstr);
				if (pair->ref_pkey_repl)
					g_object_unref (G_OBJECT (pair->ref_pkey_repl));
				g_free (list->data);
				list = g_slist_next (list);
			}
			g_slist_free (cstr->priv->fk_pairs);
			cstr->priv->fk_pairs = NULL;
			break;
		case CONSTRAINT_NOT_NULL:
			if (cstr->priv->single_field)
				g_signal_handlers_disconnect_by_func (G_OBJECT (cstr->priv->single_field),
								      G_CALLBACK (destroyed_object_cb), cstr);
			cstr->priv->single_field = NULL;
			break;
		case CONSTRAINT_CHECK_IN_LIST:
		case CONSTRAINT_CHECK_SETOF_LIST:
			if (cstr->priv->cstr_data) {
				g_object_unref (G_OBJECT (cstr->priv->cstr_data));
				cstr->priv->cstr_data = NULL;
			}
			break;
		case CONSTRAINT_CHECK_EXPR:
		default:
			TO_IMPLEMENT;
		}

		if (g_object_get_data (G_OBJECT (cstr), "db")) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (g_object_get_data (G_OBJECT (cstr), "db")),
							      G_CALLBACK (destroyed_object_cb), cstr);
			g_object_set_data (G_OBJECT (cstr), "db", NULL);
		}
		
		if (cstr->priv->table) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (cstr->priv->table),
							      G_CALLBACK (destroyed_object_cb), cstr);
			cstr->priv->table = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_dict_constraint_finalize (GObject   * object)
{
	GdaDictConstraint *gda_dict_constraint;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DICT_CONSTRAINT (object));

	gda_dict_constraint = GDA_DICT_CONSTRAINT (object);
	if (gda_dict_constraint->priv) {

		g_free (gda_dict_constraint->priv);
		gda_dict_constraint->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_dict_constraint_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdaDictConstraint *gda_dict_constraint;

	gda_dict_constraint = GDA_DICT_CONSTRAINT (object);
	if (gda_dict_constraint->priv) {
		switch (param_id) {
		case PROP_USER_CSTR:
			gda_dict_constraint->priv->user_defined = g_value_get_boolean (value);
			break;
		}
	}
}

static void
gda_dict_constraint_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdaDictConstraint *gda_dict_constraint;
	gda_dict_constraint = GDA_DICT_CONSTRAINT (object);
	
	if (gda_dict_constraint->priv) {
		switch (param_id) {
		case PROP_USER_CSTR:
			g_value_set_boolean (value, gda_dict_constraint->priv->user_defined);
			break;
		}	
	}
}


/**
 * gda_dict_constraint_get_constraint_type
 * @cstr: a #GdaDictConstraint object
 *
 * Get the type of constraint the @cstr object represents
 *
 * Returns: the constraint type
 */
GdaDictConstraintType
gda_dict_constraint_get_constraint_type (GdaDictConstraint *cstr)
{
	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), CONSTRAINT_UNKNOWN);
	g_return_val_if_fail (cstr->priv, CONSTRAINT_UNKNOWN);
	g_return_val_if_fail (cstr->priv->table, CONSTRAINT_UNKNOWN);

	return cstr->priv->type;
}

/**
 * gda_dict_constraint_equal
 * @cstr1: the first #GdaDictConstraint to compare
 * @cstr2: the second #GdaDictConstraint to compare
 *
 * Compares two #GdaDictConstraint objects to see if they are equal, without taking into account the
 * name of the constraints or weather they are user or system defined
 *
 * Returns: TRUE if the two constraints are equal and FALSE otherwise
 */
gboolean
gda_dict_constraint_equal (GdaDictConstraint *cstr1, GdaDictConstraint *cstr2)
{
	gboolean equal = TRUE;
	GSList *list1, *list2;

	g_return_val_if_fail (cstr1 && GDA_IS_DICT_CONSTRAINT (cstr1), FALSE);
	g_return_val_if_fail (cstr1->priv, FALSE);
	g_return_val_if_fail (cstr2 && GDA_IS_DICT_CONSTRAINT (cstr2), FALSE);
	g_return_val_if_fail (cstr2->priv, FALSE);
	g_return_val_if_fail (cstr1->priv->table, FALSE);
	g_return_val_if_fail (cstr2->priv->table, FALSE);

	if (cstr1->priv->type != cstr2->priv->type)
		return FALSE;

	if (cstr1->priv->table != cstr2->priv->table)
		return FALSE;
	
	gda_dict_constraint_activate (GDA_REFERER (cstr1));
	gda_dict_constraint_activate (GDA_REFERER (cstr2));

	switch (cstr1->priv->type) {
	case CONSTRAINT_PRIMARY_KEY:
	case CONSTRAINT_UNIQUE:
		list1 = cstr1->priv->multiple_fields;
		list2 = cstr2->priv->multiple_fields;
		while (list1 && list2 && equal) {
			if (list1->data != list2->data)
				equal = FALSE;
			list1 = g_slist_next (list1);
			list2 = g_slist_next (list2);
		}
		if (list1 || list2)
			equal = FALSE;
		break;
	case CONSTRAINT_FOREIGN_KEY:
		list1 = cstr1->priv->fk_pairs;
		list2 = cstr2->priv->fk_pairs;
		while (list1 && list2 && equal) {
			if (GDA_DICT_CONSTRAINT_FK_PAIR (list1->data)->fkey != 
			    GDA_DICT_CONSTRAINT_FK_PAIR (list2->data)->fkey)
				equal = FALSE;
			if (GDA_DICT_CONSTRAINT_FK_PAIR (list1->data)->ref_pkey != 
			    GDA_DICT_CONSTRAINT_FK_PAIR (list2->data)->ref_pkey)
				equal = FALSE;

			if (GDA_DICT_CONSTRAINT_FK_PAIR (list1->data)->ref_pkey_repl &&
			    GDA_DICT_CONSTRAINT_FK_PAIR (list2->data)->ref_pkey_repl) {
				GType type1, type2;
				GdaObjectRefType itype1, itype2;
				const gchar *name1, *name2;
				
				name1 = gda_object_ref_get_ref_name (GDA_DICT_CONSTRAINT_FK_PAIR (list1->data)->ref_pkey_repl,
								  &type1, &itype1);
				name2 = gda_object_ref_get_ref_name (GDA_DICT_CONSTRAINT_FK_PAIR (list2->data)->ref_pkey_repl,
								  &type2, &itype2);
				if ((type1 != type2) || (itype1 != itype2) ||
				    strcmp (name1, name2))
					equal = FALSE;
			}
			else {
				if (GDA_DICT_CONSTRAINT_FK_PAIR (list1->data)->ref_pkey_repl ||
				    GDA_DICT_CONSTRAINT_FK_PAIR (list2->data)->ref_pkey_repl)
					equal = FALSE;
			}

			list1 = g_slist_next (list1);
			list2 = g_slist_next (list2);
		}
		if (list1 || list2)
			equal = FALSE;
		break;
	case CONSTRAINT_NOT_NULL:
		if (cstr1->priv->single_field != cstr2->priv->single_field)
			equal = FALSE;
		break;
	case CONSTRAINT_CHECK_IN_LIST:
	case CONSTRAINT_CHECK_SETOF_LIST:
		/* create and use a gda_data_model_equal() method */
		TO_IMPLEMENT;
		break;
	case CONSTRAINT_CHECK_EXPR:
		TO_IMPLEMENT;
		break;
	default:
		equal = FALSE;
		break;
	}

	return equal;
}


/**
 * gda_dict_constraint_get_table
 * @cstr: a #GdaDictConstraint object
 *
 * Get the table to which the constraint is attached
 *
 * Returns: the #GdaDictTable
 */
GdaDictTable *
gda_dict_constraint_get_table (GdaDictConstraint *cstr)
{
	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), NULL);
	g_return_val_if_fail (cstr->priv, NULL);
	g_return_val_if_fail (cstr->priv->table, NULL);

	return cstr->priv->table;
}

/**
 * gda_dict_constraint_uses_field
 * @cstr: a #GdaDictConstraint object
 * @field: a #GdaDictField object
 *
 * Tests if @field is part of the @cstr constraint
 *
 * Returns: TRUE if @cstr uses @field
 */
gboolean
gda_dict_constraint_uses_field (GdaDictConstraint *cstr, GdaDictField *field)
{
	gboolean retval = FALSE;
	GSList *list;

	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), FALSE);
	g_return_val_if_fail (cstr->priv, FALSE);
	g_return_val_if_fail (field && GDA_IS_DICT_FIELD (field), FALSE);

	switch (gda_dict_constraint_get_constraint_type (cstr)) {
	case CONSTRAINT_PRIMARY_KEY:
	case CONSTRAINT_UNIQUE:
		if (g_slist_find (cstr->priv->multiple_fields, field))
			retval = TRUE;
		break;
	case CONSTRAINT_FOREIGN_KEY:
		list = cstr->priv->fk_pairs;
		while (list && !retval) {
			if (GDA_DICT_CONSTRAINT_FK_PAIR (list->data)->fkey == field)
				retval = TRUE;
			list = g_slist_next (list);
		}
		break;
	case CONSTRAINT_NOT_NULL:
		if (cstr->priv->single_field == field)
			retval = TRUE;
		break;
	case CONSTRAINT_CHECK_EXPR:
	default:
		TO_IMPLEMENT;
	}
	
	return retval;
}


static void gda_dict_constraint_multiple_set_fields (GdaDictConstraint *cstr, const GSList *fields);

/**
* gda_dict_constraint_pkey_set_fields
* @cstr: a #GdaDictConstraint object
* @fields: a list of #GdaDictField objects
*
* Sets the fields which make the primary key represented by @cstr. All the fields
* must belong to the same #GdaDictTable to which the constraint is attached
*/
void
gda_dict_constraint_pkey_set_fields (GdaDictConstraint *cstr, const GSList *fields)
{
	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	g_return_if_fail (cstr->priv);
	g_return_if_fail (cstr->priv->type == CONSTRAINT_PRIMARY_KEY);
	g_return_if_fail (cstr->priv->table);
	g_return_if_fail (fields);

	gda_dict_constraint_multiple_set_fields (cstr, fields);
}


static void
gda_dict_constraint_multiple_set_fields (GdaDictConstraint *cstr, const GSList *fields)
{
	const GSList *list;
	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));

	/* testing for errors */
	list = fields;
	while (list) {
		if (!list->data) {
			g_warning ("List contains a NULL value, not a field");
			return;
		}
		if (!GDA_IS_DICT_FIELD (list->data)) {
			g_warning ("List item %p is not a field\n", list->data);
			return;
		}
		if (gda_entity_field_get_entity (GDA_ENTITY_FIELD (list->data)) != GDA_ENTITY (cstr->priv->table)) {
			g_warning ("Field %p belongs to a table different from the constraint\n", list->data);
			return;
		}

		list = g_slist_next (list);
	}

	/* if a fields list is already here, get rid of it */
	if (cstr->priv->multiple_fields) {
		list = cstr->priv->multiple_fields;
		while (list) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
							      G_CALLBACK (destroyed_object_cb), cstr);
			list = g_slist_next (list);
		}
		g_slist_free (cstr->priv->multiple_fields);
		cstr->priv->multiple_fields = NULL;
	}

	/* make a copy of the new fields list */
	list = fields;
	while (list) {
		gda_object_connect_destroy (list->data, G_CALLBACK (destroyed_object_cb), cstr);
		cstr->priv->multiple_fields = g_slist_append (cstr->priv->multiple_fields, list->data);
		list = g_slist_next (list);
	}
}

/**
 * gda_dict_constraint_pkey_get_fields
 * @cstr: a #GdaDictConstraint object
 *
 * Get the list of fields composing the primary key constraint which @cstr represents. The
 * returned list is allocated and must be de-allocated by the caller.
 *
 * Returns: a new list of fields
 */
GSList *
gda_dict_constraint_pkey_get_fields (GdaDictConstraint *cstr)
{
	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), NULL);
	g_return_val_if_fail (cstr->priv, NULL);
	g_return_val_if_fail (cstr->priv->type == CONSTRAINT_PRIMARY_KEY, NULL);
	g_return_val_if_fail (cstr->priv->table, NULL);

	return g_slist_copy (cstr->priv->multiple_fields);
}


/** 
 * gda_dict_constraint_fkey_set_fields
 * @cstr: a #GdaDictConstraint object
 * @pairs: a list of #GdaDictField objects
 *
 * Sets the field pairs which make the foreign key represented by @cstr. All the field pairs
 * must list a field which belong to the same #GdaDictTable to which the constraint is attached
 * and a field which belongs to a #GdaDictTable which is different from the one just mentionned and which
 * is within the same database.
 * The pairs are of type #GdaDictConstraintFkeyPair.
 */
void
gda_dict_constraint_fkey_set_fields (GdaDictConstraint *cstr, const GSList *pairs)
{
	const GSList *list;
	GdaDictTable *ref_table = NULL;
	GSList *oldlist;

	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	g_return_if_fail (cstr->priv);
	g_return_if_fail (cstr->priv->type == CONSTRAINT_FOREIGN_KEY);
	g_return_if_fail (cstr->priv->table);

	/* testing for errors */
	list = pairs;
	while (list) {
		GdaDictConstraintFkeyPair *pair;

		if (!list->data) {
			g_warning ("List contains a NULL value, not a pair of fields");
			return;
		}

		pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);
		if (!GDA_IS_DICT_FIELD (pair->fkey)) {
			g_warning ("Pair item %p has fkey which is not a is not a field", list->data);
			return;
		}
		if (pair->ref_pkey_repl) { /* we have a GdaObjectRef object */
			if (!GDA_IS_OBJECT_REF (pair->ref_pkey_repl)) {
				g_warning ("Pair item %p has ref_pkey_repl which is not a is not a GdaObjectRef", list->data);
				return;	
			}
			if (gda_object_ref_get_ref_type (pair->ref_pkey_repl) !=
			    GDA_TYPE_DICT_FIELD) {
				g_warning ("Pair item %p has ref_pkey_repl which does not reference a field", list->data);
				return;	
			}
		}
		else { /* we have a GdaDictField object */
			if (!pair->ref_pkey || !GDA_IS_DICT_FIELD (pair->ref_pkey)) {
				g_warning ("Pair item %p has ref_pkey which is not a is not a field", list->data);
				return;
			}
			if (!ref_table)
				ref_table = GDA_DICT_TABLE (gda_entity_field_get_entity (GDA_ENTITY_FIELD (pair->ref_pkey)));
			else
				if (gda_entity_field_get_entity (GDA_ENTITY_FIELD (pair->ref_pkey)) != 
				    GDA_ENTITY (ref_table)) {
					g_warning ("Referenced table is not the same for all pairs");
					return;
				}
		}

		if (gda_entity_field_get_entity (GDA_ENTITY_FIELD (pair->fkey)) != 
					 GDA_ENTITY (cstr->priv->table)) {
			g_warning ("Field %p belongs to a table different from the constraint", 
				   pair->fkey);
			return;
		}

		list = g_slist_next (list);
	}

	/* if a fields list is already here, we treat in two parts: first (before the new list is analysed)
	 * we diocsonnect signals from objects of the old list, and then (after the new list has been analysed)
	 * we remove any reference to unused objects and clear the list */
	oldlist = cstr->priv->fk_pairs;
	list = cstr->priv->fk_pairs;
	while (list) {
		GdaDictConstraintFkeyPair *pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);
		
		g_signal_handlers_disconnect_by_func (G_OBJECT (pair->fkey),
						      G_CALLBACK (destroyed_object_cb), cstr);
		if (pair->ref_pkey)
			g_signal_handlers_disconnect_by_func (G_OBJECT (pair->ref_pkey),
							      G_CALLBACK (destroyed_object_cb), cstr);
		
		list = g_slist_next (list);
	}
	if (cstr->priv->ref_table) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (cstr->priv->ref_table),
						      G_CALLBACK (destroyed_object_cb), cstr);
		cstr->priv->ref_table = NULL;
	}

	/* make a copy of the new fields list */
	cstr->priv->fk_pairs = NULL;
	list = pairs;
	while (list) {
		GdaDictConstraintFkeyPair *pair;
		pair = g_new0 (GdaDictConstraintFkeyPair, 1);
		*pair = *(GDA_DICT_CONSTRAINT_FK_PAIR (list->data));

		gda_object_connect_destroy (pair->fkey, 
					 G_CALLBACK (destroyed_object_cb), cstr);
		
		if (!pair->ref_pkey_repl) 
			/* here we have tested that pair->ref_pkey is OK */
			gda_object_connect_destroy (pair->ref_pkey, 
						 G_CALLBACK (destroyed_object_cb), cstr);
		else 
			g_object_ref (G_OBJECT (pair->ref_pkey_repl));

		cstr->priv->fk_pairs = g_slist_append (cstr->priv->fk_pairs, pair);
		list = g_slist_next (list);
	}
	cstr->priv->ref_table = ref_table;
	if (ref_table)
		gda_object_connect_destroy (ref_table, 
					 G_CALLBACK (destroyed_object_cb), cstr);

	/* second part of getting rid of the ols list of fields */
	if (oldlist) {
		list = oldlist;
		while (list) {
			GdaDictConstraintFkeyPair *pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);

			if (pair->ref_pkey_repl)
				g_object_unref (G_OBJECT (pair->ref_pkey_repl));
				
			g_free (list->data);
			list = g_slist_next (list);
		}
		g_slist_free (oldlist);
	}


	gda_dict_constraint_activate (GDA_REFERER (cstr));
}


/**
 * gda_dict_constraint_fkey_get_ref_table
 * @cstr: a #GdaDictConstraint object
 *
 * Get the #GdaDictTable at the other end of the foreign key relation represented by this
 * constraint
 *
 * Returns: the #GdaDictTable
 */
GdaDictTable *
gda_dict_constraint_fkey_get_ref_table (GdaDictConstraint *cstr)
{
	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), NULL);
	g_return_val_if_fail (cstr->priv, NULL);
	g_return_val_if_fail (cstr->priv->type == CONSTRAINT_FOREIGN_KEY, NULL);
	g_return_val_if_fail (cstr->priv->table, NULL);
	
	gda_dict_constraint_activate (GDA_REFERER (cstr));

	return cstr->priv->ref_table;
}

/**
 * gda_dict_constraint_fkey_get_fields
 * @cstr: a #GdaDictConstraint object
 *
 * Get the list of field pairs composing the foreign key constraint which @cstr represents. In the returned
 * list, each pair item is allocated and it's up to the caller to free the list and each pair, and the
 * reference count for each pointer to GObjects in each pair is NOT INCREASED, which means the caller of this
 * function DOES NOT hold any reference on the mentionned GObjects (if he needs to, it has to call g_object_ref())
 *
 * Returns: a new list of #GdaDictConstraintFkeyPair pairs
 */
GSList *
gda_dict_constraint_fkey_get_fields (GdaDictConstraint *cstr)
{
	GSList *list, *retval = NULL;

	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), NULL);
	g_return_val_if_fail (cstr->priv, NULL);
	g_return_val_if_fail (cstr->priv->type == CONSTRAINT_FOREIGN_KEY, NULL);
	g_return_val_if_fail (cstr->priv->table, NULL);

	gda_dict_constraint_activate (GDA_REFERER (cstr));
	
	/* copy the list of pairs */
	list = cstr->priv->fk_pairs;
	while (list) {
		GdaDictConstraintFkeyPair *pair;
		pair = g_new0 (GdaDictConstraintFkeyPair, 1);
		*pair = * GDA_DICT_CONSTRAINT_FK_PAIR (list->data);
		retval = g_slist_append (retval, pair);
		list = g_slist_next (list);
	}

	return retval;
}

/**
 * gda_dict_constraint_fkey_set_actions
 * @cstr: a #GdaDictConstraint object
 * @on_update: the action undertaken when an UPDATE occurs
 * @on_delete: the action undertaken when a DELETE occurs
 *
 * Sets the actions undertaken by the DBMS when some actions occur on the referenced data
 */
void
gda_dict_constraint_fkey_set_actions (GdaDictConstraint *cstr, 
				   GdaDictConstraintFkAction on_update, GdaDictConstraintFkAction on_delete)
{
	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	g_return_if_fail (cstr->priv);
	g_return_if_fail (cstr->priv->type == CONSTRAINT_FOREIGN_KEY);
	g_return_if_fail (cstr->priv->table);

	cstr->priv->on_update = on_update;
	cstr->priv->on_delete = on_delete;
}

/**
 * gda_dict_constraint_fkey_get_actions
 * @cstr: a #GdaDictConstraint object
 * @on_update: an address to store the action undertaken when an UPDATE occurs
 * @on_delete: an address to store the action undertaken when a DELETE occurs
 *
 * Get the actions undertaken by the DBMS when some actions occur on the referenced data
 */
void
gda_dict_constraint_fkey_get_actions (GdaDictConstraint *cstr, 
				   GdaDictConstraintFkAction *on_update, GdaDictConstraintFkAction *on_delete)
{
	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	g_return_if_fail (cstr->priv);
	g_return_if_fail (cstr->priv->type == CONSTRAINT_FOREIGN_KEY);
	g_return_if_fail (cstr->priv->table);

	if (on_update)
		*on_update = cstr->priv->on_update;
	if (on_delete)
		*on_delete = cstr->priv->on_delete;
}


/**
 * gda_dict_constraint_unique_set_fields
 * @cstr: a #GdaDictConstraint object
 * @fields:
 *
 */
void
gda_dict_constraint_unique_set_fields (GdaDictConstraint *cstr, const GSList *fields)
{
	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	g_return_if_fail (cstr->priv);
	g_return_if_fail (cstr->priv->type == CONSTRAINT_UNIQUE);
	g_return_if_fail (cstr->priv->table);
	g_return_if_fail (fields);

	gda_dict_constraint_multiple_set_fields (cstr, fields);
}

/**
 * gda_dict_constraint_unique_get_fields
 * @cstr: a #GdaDictConstraint object
 *
 * Get the list of fields represented by this UNIQUE constraint. It's up to the caller to free the list.
 *
 * Returns: a new list of fields
 */
GSList *
gda_dict_constraint_unique_get_fields (GdaDictConstraint *cstr)
{
	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), NULL);
	g_return_val_if_fail (cstr->priv, NULL);
	g_return_val_if_fail (cstr->priv->type == CONSTRAINT_UNIQUE, NULL);
	g_return_val_if_fail (cstr->priv->table, NULL);

	return g_slist_copy (cstr->priv->multiple_fields);
}

/**
 * gda_dict_constraint_non_null_set_field
 * @cstr: a #GdaDictConstraint object
 * @field:
 *
 */
void
gda_dict_constraint_not_null_set_field (GdaDictConstraint *cstr, GdaDictField *field)
{
	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	g_return_if_fail (cstr->priv);
	g_return_if_fail (cstr->priv->type == CONSTRAINT_NOT_NULL);
	g_return_if_fail (cstr->priv->table);
	g_return_if_fail (field && GDA_IS_DICT_FIELD (field));
	g_return_if_fail (gda_entity_field_get_entity (GDA_ENTITY_FIELD (field)) == GDA_ENTITY (cstr->priv->table));


	/* if a field  is already here, get rid of it */
	if (cstr->priv->single_field) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (cstr->priv->single_field),
						      G_CALLBACK (destroyed_object_cb), cstr);
		cstr->priv->single_field = NULL;
	}

	gda_object_connect_destroy (field, G_CALLBACK (destroyed_object_cb), cstr);
	cstr->priv->single_field = field;
}

/**
 * gda_dict_constraint_non_null_get_field
 * @cstr: a #GdaDictConstraint object
 *
 */
GdaDictField *
gda_dict_constraint_not_null_get_field (GdaDictConstraint *cstr)
{
	g_return_val_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr), NULL);
	g_return_val_if_fail (cstr->priv, NULL);
	g_return_val_if_fail (cstr->priv->type == CONSTRAINT_NOT_NULL, NULL);
	g_return_val_if_fail (cstr->priv->table, NULL);

	return cstr->priv->single_field;
}



#ifdef GDA_DEBUG
static void
gda_dict_constraint_dump (GdaDictConstraint *cstr, guint offset)
{
	gchar *str;
	gint i;

	g_return_if_fail (cstr && GDA_IS_DICT_CONSTRAINT (cstr));
	
        /* string for the offset */
        str = g_new0 (gchar, offset+1);
        for (i=0; i<offset; i++)
                str[i] = ' ';
        str[offset] = 0;

        /* dump */
        if (cstr->priv && cstr->priv->table)
		g_print ("%s" D_COL_H1 "GdaDictConstraint" D_COL_NOR " %p type=%d name=%s\n",
			 str, cstr, cstr->priv->type, gda_object_get_description (GDA_OBJECT (cstr)));
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, cstr);
}
#endif








/* 
 * GdaXmlStorage interface implementation
 */
static gchar *
gda_dict_constraint_get_xml_id (GdaXmlStorage *iface)
{
	gchar *t_xml_id, *tmp, *xml_id;

	g_return_val_if_fail (iface && GDA_IS_DICT_CONSTRAINT (iface), NULL);
	g_return_val_if_fail (GDA_DICT_CONSTRAINT (iface)->priv, NULL);
	g_return_val_if_fail (GDA_DICT_CONSTRAINT (iface)->priv->table, NULL);

	t_xml_id = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (GDA_DICT_CONSTRAINT (iface)->priv->table));
	tmp = gda_utility_build_encoded_id ("FI", gda_object_get_name (GDA_OBJECT (iface)));
	xml_id = g_strdup_printf ("%s:%s", t_xml_id, tmp);
	g_free (t_xml_id);
	g_free (tmp);
	g_assert_not_reached ();
	
	return xml_id;
}

static const gchar *
constraint_type_to_str (GdaDictConstraintType type)
{
	switch (type) {
	case CONSTRAINT_PRIMARY_KEY:
		return "PKEY";
		break;
	case CONSTRAINT_FOREIGN_KEY:
		return "FKEY";
		break;
	case CONSTRAINT_UNIQUE:
		return "UNIQ";
		break;
	case CONSTRAINT_NOT_NULL:
		return "NNUL";
		break;
	case CONSTRAINT_CHECK_EXPR:
		return "CHECK";
		break;
	default:
		return "???";
		break;
	}
}

static GdaDictConstraintType
constraint_str_to_type (const gchar *str)
{
	g_return_val_if_fail (str, CONSTRAINT_UNKNOWN);

	switch (*str) {
	case 'P':
		return CONSTRAINT_PRIMARY_KEY;
		break;
	case 'F':
		return CONSTRAINT_FOREIGN_KEY;
		break;
	case 'U':
		return CONSTRAINT_UNIQUE;
		break;
	case 'N':
		return CONSTRAINT_NOT_NULL;
		break;
	case 'C':
		return CONSTRAINT_CHECK_EXPR;
		break;
	default:
		return CONSTRAINT_UNKNOWN;
		break;
	}
}


static const gchar *
constraint_action_to_str (GdaDictConstraintFkAction action)
{
	switch (action) {
	case CONSTRAINT_FK_ACTION_CASCADE:
		return "CAS";
		break;
	case CONSTRAINT_FK_ACTION_SET_NULL:
		return "NULL";
		break;
	case CONSTRAINT_FK_ACTION_SET_DEFAULT:
		return "DEF";
		break;
	case CONSTRAINT_FK_ACTION_SET_VALUE:
		return "VAL";
		break;
	case CONSTRAINT_FK_ACTION_NO_ACTION:
		return "RESTRICT";
		break;
	default:
		return "???";
		break;	
	}
}

static GdaDictConstraintFkAction
constraint_str_to_action (const gchar *str)
{
	g_return_val_if_fail (str, CONSTRAINT_FK_ACTION_NO_ACTION);
	switch (*str) {
	case 'C':
		return CONSTRAINT_FK_ACTION_CASCADE;
		break;
	case 'N':
		return CONSTRAINT_FK_ACTION_SET_NULL;
		break;
	case 'D':
		return CONSTRAINT_FK_ACTION_SET_DEFAULT;
		break;
	case 'V':
		return CONSTRAINT_FK_ACTION_SET_VALUE;
		break;
	case 'R':
		return CONSTRAINT_FK_ACTION_NO_ACTION;
		break;
	default:
		return CONSTRAINT_FK_ACTION_NO_ACTION;
		break;	
	}
}

static xmlNodePtr
gda_dict_constraint_save_to_xml (GdaXmlStorage *iface, GError **error)
{
	xmlNodePtr node = NULL;
	xmlNodePtr child;
	GdaDictConstraint *cstr;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (iface && GDA_IS_DICT_CONSTRAINT (iface), NULL);
	g_return_val_if_fail (GDA_DICT_CONSTRAINT (iface)->priv, NULL);
	g_return_val_if_fail (GDA_DICT_CONSTRAINT (iface)->priv->table, NULL);

	cstr = GDA_DICT_CONSTRAINT (iface);
	gda_dict_constraint_activate (GDA_REFERER (cstr));

	if (! gda_dict_constraint_is_active (GDA_REFERER (cstr))) {
		g_set_error (error,
			     GDA_DICT_CONSTRAINT_ERROR,
			     GDA_DICT_CONSTRAINT_XML_SAVE_ERROR,
			     _("Constraint cannot be activated!"));
		return NULL;
	}
	node = xmlNewNode (NULL, (xmlChar*)"gda_dict_constraint");
	
	xmlSetProp(node, (xmlChar*)"name", (xmlChar*)gda_object_get_name (GDA_OBJECT (cstr)));
	xmlSetProp(node, (xmlChar*)"user_defined", (xmlChar*)(cstr->priv->user_defined ? "t" : "f"));
	xmlSetProp(node, (xmlChar*)"type", (xmlChar*)constraint_type_to_str (cstr->priv->type));

	str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (cstr->priv->table));
	xmlSetProp(node, (xmlChar*)"table", (xmlChar*)str);
	g_free (str);

	switch (cstr->priv->type) {
	case CONSTRAINT_UNIQUE:
	case CONSTRAINT_PRIMARY_KEY:
		list = cstr->priv->multiple_fields;
		while (list) {
			child = xmlNewChild (node, NULL, (xmlChar*)"gda_dict_constraint_field", NULL);
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
			xmlSetProp(child, (xmlChar*)"field", (xmlChar*)str);
			g_free (str);
			list = g_slist_next (list);
		}
		break;
	case CONSTRAINT_FOREIGN_KEY:
		list = cstr->priv->fk_pairs;
		while (list) {
			child = xmlNewChild (node, NULL, (xmlChar*)"gda_dict_constraint_pair", NULL);
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (GDA_DICT_CONSTRAINT_FK_PAIR (list->data)->fkey));
			xmlSetProp(child, (xmlChar*)"field", (xmlChar*)str);
			g_free (str);
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (GDA_DICT_CONSTRAINT_FK_PAIR (list->data)->ref_pkey));
			xmlSetProp(child, (xmlChar*)"ref", (xmlChar*)str);
			g_free (str);
			list = g_slist_next (list);
		}

		xmlSetProp(node, (xmlChar*)"on_update", (xmlChar*)constraint_action_to_str (cstr->priv->on_update));
		xmlSetProp(node, (xmlChar*)"on_delete", (xmlChar*)constraint_action_to_str (cstr->priv->on_delete));
		break;
	case CONSTRAINT_NOT_NULL:
		child = xmlNewChild (node, NULL, (xmlChar*)"gda_dict_constraint_field", NULL);
		if (cstr->priv->single_field)
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (cstr->priv->single_field));
		xmlSetProp(child, (xmlChar*)"field", (xmlChar*)str);
		g_free (str);
		break;
	case CONSTRAINT_CHECK_EXPR:
		TO_IMPLEMENT;
		break;
	default:
		break;
	}

	return node;
}

static gboolean
gda_dict_constraint_load_from_xml (GdaXmlStorage *iface, xmlNodePtr node, GError **error)
{
	GdaDictConstraint *cstr;
	gchar *prop;
	gboolean type = FALSE, field_error = FALSE;
	GdaDictTable *table = NULL;
	GdaDictDatabase *db;
	gchar *ref_pb = NULL;
	xmlNodePtr subnode;
	GSList *contents, *list;

	g_return_val_if_fail (iface && GDA_IS_DICT_CONSTRAINT (iface), FALSE);
	g_return_val_if_fail (GDA_DICT_CONSTRAINT (iface)->priv, FALSE);
	g_return_val_if_fail (node, FALSE);
	g_return_val_if_fail (!GDA_DICT_CONSTRAINT (iface)->priv->table, FALSE);
	db = g_object_get_data (G_OBJECT (iface), "db");
	g_return_val_if_fail (db, FALSE);

	cstr = GDA_DICT_CONSTRAINT (iface);
	if (strcmp ((gchar*)node->name, "gda_dict_constraint")) {
		g_set_error (error,
			     GDA_DICT_CONSTRAINT_ERROR,
			     GDA_DICT_CONSTRAINT_XML_LOAD_ERROR,
			     _("XML Tag is not <gda_dict_constraint>"));
		return FALSE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"name");
	if (prop) {
		gda_object_set_name (GDA_OBJECT (cstr), prop);
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"table");
	if (prop) {
		table = gda_dict_database_get_table_by_xml_id (db, prop);
		if (table) {
			g_object_set_data (G_OBJECT (iface), "db", NULL);
			g_signal_handlers_disconnect_by_func (G_OBJECT (db),
							      G_CALLBACK (destroyed_object_cb), cstr);
			cstr->priv->table = table;
			gda_object_connect_destroy (table, 
						 G_CALLBACK (destroyed_object_cb), cstr);
			g_free (prop);
		}
		else 
			ref_pb = prop;
	}
	
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"user_defined");
	if (prop) {
		cstr->priv->user_defined = (*prop && (*prop == 't')) ? TRUE : FALSE;
		g_free (prop);
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"type");
	if (prop) {
		cstr->priv->type = constraint_str_to_type (prop);
		g_free (prop);
		type = TRUE;
	}
	
	prop = (gchar*)xmlGetProp(node, (xmlChar*)"on_update");
	if (prop) {
		cstr->priv->on_update = constraint_str_to_action (prop);
		g_free (prop);
		type = TRUE;
	}

	prop = (gchar*)xmlGetProp(node, (xmlChar*)"on_delete");
	if (prop) {
		cstr->priv->on_delete = constraint_str_to_action (prop);
		g_free (prop);
		type = TRUE;
	}

	/* contents of the constraint if applicable */
	contents = NULL;
	subnode = node->children;
	while (subnode) {
		if (!strcmp ((gchar*)subnode->name, "gda_dict_constraint_field")) {
			GdaDictField *field = NULL;
			prop = (gchar*)xmlGetProp(subnode, (xmlChar*)"field");
			if (prop) {
				field = gda_dict_database_get_field_by_xml_id (db, prop);
				g_free (prop);
			}
			if (field)
				contents = g_slist_append (contents, field);
			else {
				field_error = TRUE;
				ref_pb = (gchar*)xmlGetProp(subnode, (xmlChar*)"field");
			}
		}
		
		if (!strcmp((gchar*)subnode->name, "gda_dict_constraint_pair")) {
			GdaDictField *field = NULL, *ref = NULL;
			prop = (gchar*)xmlGetProp(subnode, (xmlChar*)"field");
			if (prop) {
				field = gda_dict_database_get_field_by_xml_id (db, prop);
				g_free (prop);
			}
			prop = (gchar*)xmlGetProp(subnode, (xmlChar*)"ref");
			if (prop) {
				ref = gda_dict_database_get_field_by_xml_id (db, prop);
				g_free (prop);
			}
			if (field && ref) {
				GdaDictConstraintFkeyPair *pair;

				pair = g_new0 (GdaDictConstraintFkeyPair, 1);
				pair->fkey = field;
				pair->ref_pkey = ref;
				pair->ref_pkey_repl = NULL;
				contents = g_slist_append (contents, pair);
			}
			else {
				field_error = TRUE;
				if (!field)
					ref_pb = (gchar*)xmlGetProp(subnode, (xmlChar*)"field");
				else
					ref_pb = (gchar*)xmlGetProp(subnode, (xmlChar*)"ref");
			}
		}
		subnode = subnode->next;
	}

	switch (cstr->priv->type) {
	case CONSTRAINT_PRIMARY_KEY:
		gda_dict_constraint_pkey_set_fields (cstr, contents);
		g_slist_free (contents);
		break;
        case CONSTRAINT_FOREIGN_KEY:
		gda_dict_constraint_fkey_set_fields (cstr, contents);
		list = contents;
		while (list) {
			g_free (list->data);
			list = g_slist_next (list);
		}
		g_slist_free (contents);
		break;
        case CONSTRAINT_UNIQUE:
		gda_dict_constraint_unique_set_fields (cstr, contents);
		g_slist_free (contents);
		break;
        case CONSTRAINT_NOT_NULL:
		if (contents) {
			gda_dict_constraint_not_null_set_field (cstr, contents->data);
			g_slist_free (contents);
		}
		else
			field_error = TRUE;
		break;
        case CONSTRAINT_CHECK_EXPR:
		TO_IMPLEMENT;
		break;
	default:
		break;
	}

	if (type && table && !field_error)
		return TRUE;
	else {
		if (!type)
			g_set_error (error,
				     GDA_DICT_CONSTRAINT_ERROR,
				     GDA_DICT_CONSTRAINT_XML_LOAD_ERROR,
				     _("Missing required attributes for <gda_dict_constraint>"));
		if (!table) {
			g_set_error (error,
				     GDA_DICT_CONSTRAINT_ERROR,
				     GDA_DICT_CONSTRAINT_XML_LOAD_ERROR,
				     _("Referenced table (%s) not found"), ref_pb);
			if (ref_pb)
				g_free (ref_pb);
		}
		if (field_error) {
			g_set_error (error,
				     GDA_DICT_CONSTRAINT_ERROR,
				     GDA_DICT_CONSTRAINT_XML_LOAD_ERROR,
				     _("Referenced field in constraint (%s) not found"), ref_pb);
			if (ref_pb)
				g_free (ref_pb);
		}
		return FALSE;
	}
}




/*
 * GdaReferer interface implementation
 */

/*
 * gda_dict_constraint_activate
 *
 * Tries to convert any GdaObjectRef object to the pointed GdaDictField instead
 */
static gboolean
gda_dict_constraint_activate (GdaReferer *iface)
{
	gboolean activated = TRUE;
	GdaDictConstraint *cstr;
	GdaDictTable *ref_table = NULL;
	
	g_return_val_if_fail (iface && GDA_IS_DICT_CONSTRAINT (iface), FALSE);
	cstr = GDA_DICT_CONSTRAINT (iface);
	g_return_val_if_fail (cstr->priv, FALSE);
	g_return_val_if_fail (cstr->priv->table, FALSE);

	if (gda_dict_constraint_is_active (GDA_REFERER (cstr)))
		return TRUE;

	if (cstr->priv->type == CONSTRAINT_FOREIGN_KEY) {
		GSList *list;
		GdaDictConstraintFkeyPair *pair;
		GdaObject *ref;

		list = cstr->priv->fk_pairs;
		while (list) {
			pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);
			if (!pair->ref_pkey) {
				g_assert (pair->ref_pkey_repl);
				ref = gda_object_ref_get_ref_object (pair->ref_pkey_repl);
				if (ref) {
					pair->ref_pkey = GDA_DICT_FIELD (ref);
					g_object_unref (G_OBJECT (pair->ref_pkey_repl));
					pair->ref_pkey_repl = NULL;
					gda_object_connect_destroy (pair->ref_pkey,
								 G_CALLBACK (destroyed_object_cb), cstr);

					if (!ref_table)
						ref_table = GDA_DICT_TABLE (gda_entity_field_get_entity (GDA_ENTITY_FIELD (pair->ref_pkey)));
					else
						if (gda_entity_field_get_entity (GDA_ENTITY_FIELD (pair->ref_pkey)) != 
						    GDA_ENTITY (ref_table)) {
							g_warning ("Referenced table is not the same for all pairs");
							return FALSE;
						}
				}
			}
			
			if (!pair->ref_pkey)
				activated = FALSE;
			
			list = g_slist_next (list);
		}

		if (ref_table != cstr->priv->ref_table) {
			if (cstr->priv->ref_table)
				g_signal_handlers_disconnect_by_func (G_OBJECT (cstr->priv->ref_table),
								      G_CALLBACK (destroyed_object_cb), cstr);
			cstr->priv->ref_table = ref_table;
			if (ref_table)
				gda_object_connect_destroy (ref_table,
							 G_CALLBACK (destroyed_object_cb), cstr);
		}
	}
	
	return activated;
}

/*
 * gda_dict_constraint_is_active
 *
 * Tells wether the GdaDictConstraint object has found all the
 * GdaDictField objects it needs
 */
static gboolean
gda_dict_constraint_is_active (GdaReferer *iface)
{
	gboolean activated = TRUE;
	GdaDictConstraint *cstr;
	
	g_return_val_if_fail (iface && GDA_IS_DICT_CONSTRAINT (iface), FALSE);
	cstr = GDA_DICT_CONSTRAINT (iface);
	g_return_val_if_fail (cstr->priv, FALSE);
	g_return_val_if_fail (cstr->priv->table, FALSE);

	if (cstr->priv->type == CONSTRAINT_FOREIGN_KEY) {
		GSList *list = cstr->priv->fk_pairs;
		while (list && activated) {
			GdaDictConstraintFkeyPair *pair = GDA_DICT_CONSTRAINT_FK_PAIR (list->data);
			if (pair->ref_pkey_repl)
				activated = FALSE;
			list = g_slist_next (list);
		}

		if (!cstr->priv->ref_table)
			activated = FALSE;
	}

	return activated;
}
