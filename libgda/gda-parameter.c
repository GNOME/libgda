/* gda-parameter.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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
#include "gda-parameter.h"
#include "gda-dict.h"
#include "gda-dict-type.h"
#include "gda-query.h"
#include "gda-entity.h"
#include "gda-entity-field.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"
#include "gda-data-model.h"
#include "gda-data-handler.h"
#include "gda-marshal.h"

/* 
 * Main static functions 
 */
static void gda_parameter_class_init (GdaParameterClass * class);
static void gda_parameter_init (GdaParameter * srv);
static void gda_parameter_dispose (GObject   * object);
static void gda_parameter_finalize (GObject   * object);

static void gda_parameter_set_property    (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gda_parameter_get_property    (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);

/* Referer interface */
static void        gda_parameter_referer_init        (GdaRefererIface *iface);
static void        gda_parameter_replace_refs        (GdaReferer *iface, GHashTable *replacements);

static void gda_parameter_add_user (GdaParameter *param, GdaObject *user);
static void gda_parameter_del_user (GdaParameter *param, GdaObject *user);

static void destroyed_user_cb (GdaObject *obj, GdaParameter *param);
static void destroyed_alias_of_cb (GdaObject *obj, GdaParameter *param);
static void destroyed_restrict_cb (GdaObject *obj, GdaParameter *param);

static void alias_of_changed_cb (GdaParameter *alias_of, GdaParameter *param);

static void gda_parameter_signal_changed (GdaObject *base, gboolean block_changed_signal);
#ifdef GDA_DEBUG
static void gda_parameter_dump (GdaParameter *parameter, guint offset);
#endif

static void gda_parameter_set_full_bind_param (GdaParameter *param, GdaParameter *alias_of);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
        RESTRICT_CHANGED,
        LAST_SIGNAL
};

static gint gda_parameter_signals[LAST_SIGNAL] = { 0 };


/* properties */
enum
{
	PROP_0,
	PROP_PLUGIN,
	PROP_QF_INTERNAL,
	PROP_USE_DEFAULT_VALUE,
	PROP_SIMPLE_BIND,
	PROP_FULL_BIND,
	PROP_RESTRICT_MODEL,
	PROP_RESTRICT_COLUMN,
	PROP_GDA_TYPE,
	PROP_DICT_TYPE /* TO ADD */
};


struct _GdaParameterPrivate
{
	GSList                *param_users; /* list of #GdaObject using that parameter */
	GType                  g_type;
	GdaParameter          *alias_of;     /* FULL bind to param */
	GdaParameter          *change_with;  /* SIMPLE bind to param */
	
	gboolean               invalid_forced;
	gboolean               valid;

	GValue                *value;
	GValue                *default_value; /* CAN be either NULL or of any type */
	gboolean               has_default_value; /* TRUE if param has a default value (even if unknown) */
	gboolean               default_forced;
	gboolean               not_null;      /* TRUE if 'value' must not be NULL when passed to destination fields */

	GdaDataModel          *restrict_model;
	gint                   restrict_col;

	gchar                 *plugin;        /* plugin to be used for user interaction */
};

/* module error */
GQuark gda_parameter_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_parameter_error");
        return quark;
}

GType
gda_parameter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaParameterClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_parameter_class_init,
			NULL,
			NULL,
			sizeof (GdaParameter),
			0,
			(GInstanceInitFunc) gda_parameter_init
		};

		static const GInterfaceInfo referer_info = {
			(GInterfaceInitFunc) gda_parameter_referer_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaParameter", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_REFERER, &referer_info);
	}
	return type;
}

static void
gda_parameter_referer_init (GdaRefererIface *iface)
{
	iface->activate = NULL;
	iface->deactivate = NULL;
	iface->is_active = NULL;
	iface->get_ref_objects = NULL;
	iface->replace_refs = gda_parameter_replace_refs;
}

static void
gda_parameter_class_init (GdaParameterClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_parameter_signals[RESTRICT_CHANGED] =
                g_signal_new ("restrict_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaParameterClass, restrict_changed),
                              NULL, NULL,
                              gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

        class->restrict_changed = NULL;

	/* virtual functions */
	GDA_OBJECT_CLASS (class)->signal_changed = gda_parameter_signal_changed;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_parameter_dump;
#endif

	object_class->dispose = gda_parameter_dispose;
	object_class->finalize = gda_parameter_finalize;

	/* Properties */
	object_class->set_property = gda_parameter_set_property;
	object_class->get_property = gda_parameter_get_property;
        g_object_class_install_property (object_class, PROP_GDA_TYPE,
                                         g_param_spec_ulong ("g_type", NULL, NULL,
							   0, G_MAXULONG, GDA_TYPE_NULL,
							   (G_PARAM_READABLE | 
							    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_PLUGIN,
					 g_param_spec_string ("entry_plugin", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_USE_DEFAULT_VALUE,
					 g_param_spec_boolean ("use_default_value", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SIMPLE_BIND,
					 g_param_spec_object ("simple_bind", NULL, NULL, 
                                                               GDA_TYPE_PARAMETER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_FULL_BIND,
					 g_param_spec_object ("full_bind", NULL, NULL, 
                                                               GDA_TYPE_PARAMETER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_RESTRICT_MODEL,
                                         g_param_spec_object ("restrict_model", NULL, NULL,
                                                               GDA_TYPE_DATA_MODEL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_RESTRICT_COLUMN,
                                         g_param_spec_int ("restrict_column", NULL, NULL,
							   0, G_MAXINT, 0,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	/* class attributes */
	/* class attributes */
	GDA_OBJECT_CLASS (class)->id_unique_enforced = FALSE;
}

static void
gda_parameter_init (GdaParameter *parameter)
{
	parameter->priv = g_new0 (GdaParameterPrivate, 1);
	parameter->priv->param_users = NULL;
	parameter->priv->g_type = G_TYPE_INVALID;
	parameter->priv->alias_of = NULL;
	parameter->priv->change_with = NULL;

	parameter->priv->invalid_forced = FALSE;
	parameter->priv->valid = TRUE;
	parameter->priv->default_forced = FALSE;
	parameter->priv->value = NULL;
	parameter->priv->default_value = NULL;
	parameter->priv->has_default_value = FALSE;

	parameter->priv->not_null = FALSE;
	parameter->priv->restrict_model = NULL;
	parameter->priv->restrict_col = 0;
	parameter->priv->plugin = NULL;
}

/**
 * gda_parameter_new
 * @type: the #GType requested
 *
 * Creates a new parameter of type @type
 *
 * Returns: a new #GdaParameter object
 */
GdaParameter *
gda_parameter_new (GType type)
{
	GObject   *obj;

	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);

        obj = g_object_new (GDA_TYPE_PARAMETER, "g_type", type, NULL);

        return (GdaParameter *) obj;
}

/**
 * gda_parameter_new_copy
 * @orig: a #GdaParameter object to copy
 *
 * Copy constructor.
 *
 * Returns: a new #GdaParameter object
 */
GdaParameter *
gda_parameter_new_copy (GdaParameter *orig)
{
	GObject   *obj;
	GdaParameter *param;
	GSList *list;
	GdaDict *dict;

	g_return_val_if_fail (orig && GDA_IS_PARAMETER (orig), NULL);
	g_return_val_if_fail (orig->priv, NULL);

	dict = gda_object_get_dict (GDA_OBJECT (orig));
	obj = g_object_new (GDA_TYPE_PARAMETER, "dict", dict,"g_type", orig->priv->g_type, NULL);
	param = GDA_PARAMETER (obj);

	gda_object_set_name (GDA_OBJECT (param), gda_object_get_name (GDA_OBJECT (orig)));
	gda_object_set_description (GDA_OBJECT (param), gda_object_get_description (GDA_OBJECT (orig)));

	list = orig->priv->param_users;
	while (list) {
		gda_parameter_add_user (param, GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	if (orig->priv->alias_of)
		gda_parameter_set_full_bind_param (param, orig->priv->alias_of);
	if (orig->priv->change_with)
		gda_parameter_bind_to_param (param, orig->priv->change_with);
	
	if (orig->priv->restrict_model) {
		/*g_print ("Restrict param %p\n", param);*/
		gda_parameter_restrict_values (param, orig->priv->restrict_model, orig->priv->restrict_col,
					       NULL);
	}

	/* direct settings */
	param->priv->invalid_forced = orig->priv->invalid_forced;
	param->priv->valid = orig->priv->valid;
	param->priv->default_forced = orig->priv->default_forced;	
	if (orig->priv->value)
		param->priv->value = gda_value_copy (orig->priv->value);
	if (orig->priv->default_value)
		param->priv->default_value = gda_value_copy (orig->priv->default_value);
	param->priv->has_default_value = orig->priv->has_default_value;
	param->priv->not_null = orig->priv->not_null;
	if (orig->priv->plugin)
		param->priv->plugin = g_strdup (orig->priv->plugin);

	return param;
}

/**
 * gda_parameter_new_string
 * @name: the name of the parameter to create
 * @str: the contents of the parameter to create
 *
 * Creates a new #GdaParameter object of type G_TYPE_STRING
 *
 * Returns: a new #GdaParameter object
 */
GdaParameter *
gda_parameter_new_string (const gchar *name, const gchar *str)
{
	GdaParameter *param;

	param = gda_parameter_new (G_TYPE_STRING);
	gda_object_set_name (GDA_OBJECT (param), name);
	gda_parameter_set_value_str (param, str);

	return param;
}

/**
 * gda_parameter_new_boolean
 * @name: the name of the parameter to create
 * @value: the value to give to the new parameter
 *
 * Creates a new #GdaParameter object of type G_TYPE_BOOLEAN
 *
 * Returns: a new #GdaParameter object
 */
GdaParameter *
gda_parameter_new_boolean (const gchar *name, gboolean value)
{
	GdaParameter *param;

	param = gda_parameter_new (G_TYPE_BOOLEAN);
	gda_object_set_name (GDA_OBJECT (param), name);
	gda_parameter_set_value_str (param, value ? "true" : "false");

	return param;
}

static void
gda_parameter_add_user (GdaParameter *param, GdaObject *user)
{
	if (!g_slist_find (param->priv->param_users, user)) {
		param->priv->param_users = g_slist_append (param->priv->param_users, user);
		gda_object_connect_destroy (user, G_CALLBACK (destroyed_user_cb), param);
		g_object_ref (G_OBJECT (user));
	}
}

static void
gda_parameter_del_user (GdaParameter *param, GdaObject *user)
{
	if (g_slist_find (param->priv->param_users, user)) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (user),
						      G_CALLBACK (destroyed_user_cb), param);
		param->priv->param_users = g_slist_remove (param->priv->param_users, user);
		g_object_unref (G_OBJECT (user));
	}
}

static void
destroyed_user_cb (GdaObject *obj, GdaParameter *param)
{
	gda_parameter_del_user (param, obj);
}

static void
gda_parameter_dispose (GObject *object)
{
	GdaParameter *parameter;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_PARAMETER (object));

	parameter = GDA_PARAMETER (object);
	if (parameter->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		gda_parameter_bind_to_param (parameter, NULL);
		gda_parameter_set_full_bind_param (parameter, NULL);

		if (parameter->priv->restrict_model)
			destroyed_restrict_cb ((GdaObject*) parameter->priv->restrict_model, parameter);

		while (parameter->priv->param_users)
			gda_parameter_del_user (parameter, GDA_OBJECT (parameter->priv->param_users->data));

		parameter->priv->g_type = G_TYPE_INVALID;

		if (parameter->priv->value) {
			gda_value_free (parameter->priv->value);
			parameter->priv->value = NULL;
		}

		if (parameter->priv->default_value) {
			gda_value_free (parameter->priv->default_value);
			parameter->priv->default_value = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_parameter_finalize (GObject   * object)
{
	GdaParameter *parameter;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_PARAMETER (object));

	parameter = GDA_PARAMETER (object);
	if (parameter->priv) {
		if (parameter->priv->plugin)
			g_free (parameter->priv->plugin);

		g_free (parameter->priv);
		parameter->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_parameter_set_property    (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	const gchar *ptr;
	GdaParameter *parameter;

	parameter = GDA_PARAMETER (object);
	if (parameter->priv) {
		switch (param_id) {
		case PROP_GDA_TYPE:
			parameter->priv->g_type = g_value_get_ulong (value);
			break;
		case PROP_PLUGIN:
			ptr = g_value_get_string (value);
			if (parameter->priv->plugin) {
				g_free (parameter->priv->plugin);
				parameter->priv->plugin = NULL;
			}
			if (ptr)
				parameter->priv->plugin = g_strdup (ptr);
			break;
		case PROP_USE_DEFAULT_VALUE:
			if (g_value_get_boolean (value)) {
				if (!parameter->priv->has_default_value)
					g_warning ("Can't force parameter to use default value if there "
						   "is no default value");
				else {
					parameter->priv->default_forced = TRUE;
					parameter->priv->invalid_forced = FALSE;
				}
			}
			else
				parameter->priv->default_forced = FALSE;

			break;
		case PROP_SIMPLE_BIND:
			gda_parameter_bind_to_param (parameter, GDA_PARAMETER (g_value_get_object (value)));
			break;
		case PROP_FULL_BIND:
			gda_parameter_set_full_bind_param (parameter, GDA_PARAMETER (g_value_get_object (value)));
			break;
		case PROP_RESTRICT_MODEL: {
			GdaDataModel* ptr = g_value_get_object (value);
			g_return_if_fail (gda_parameter_restrict_values (parameter, 
									 (GdaDataModel *)ptr, -1, NULL));
			break;
                }
		case PROP_RESTRICT_COLUMN:
			parameter->priv->restrict_col = g_value_get_int (value);
			break;
		}
	}
}

static void
gda_parameter_get_property    (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdaParameter *parameter;

	parameter = GDA_PARAMETER (object);
	if (parameter->priv) {
		switch (param_id) {
		case PROP_GDA_TYPE:
			g_value_set_ulong (value, parameter->priv->g_type);
		case PROP_PLUGIN:
			g_value_set_string (value, parameter->priv->plugin);
			break;
		case PROP_USE_DEFAULT_VALUE:
			g_value_set_boolean (value, parameter->priv->default_forced);
			break;
		case PROP_SIMPLE_BIND:
			g_value_set_object (value, G_OBJECT (parameter->priv->change_with));
			break;
		case PROP_FULL_BIND:
			g_value_set_object (value, G_OBJECT (parameter->priv->alias_of));
			break;
		case PROP_RESTRICT_MODEL:
			g_value_set_object (value, G_OBJECT (parameter->priv->restrict_model));
			break;
		case PROP_RESTRICT_COLUMN:
			g_value_set_int (value, parameter->priv->restrict_col);
			break;	
		}
	}
}


/**
 * gda_parameter_declare_param_user
 * @param: a #GdaParameter object
 * @user: the #GdaObject object using that parameter for
 *
 * Tells that @user is potentially using @param.
 */
void
gda_parameter_declare_param_user (GdaParameter *param, GdaObject *user)
{
	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);
	g_return_if_fail (GDA_IS_OBJECT (user));

	gda_parameter_add_user (param, user);	
}

/**
 * gda_parameter_get_param_users
 * @param: a #GdaParameter object
 * 
 * Get the #GdaEntityField objects which created @param (and which will use its value)
 *
 * Returns: the list of #GdaEntityField object; it must not be changed or free'd
 */
GSList *
gda_parameter_get_param_users (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (param->priv, NULL);

	return param->priv->param_users;
}

/**
 * gda_parameter_replace_param_users
 * @param: a #GdaParameter object
 * @replacements: the (objects to be replaced, replacing object) pairs
 *
 * For each declared parameter user in the @replacements keys, declare the value stored in
 * @replacements also as a user of @param.
 */
void
gda_parameter_replace_param_users (GdaParameter *param, GHashTable *replacements)
{
	GSList *list;
	gpointer ref;

	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

	/* Destination fields */
	list = param->priv->param_users;
	while (list) {
		ref = g_hash_table_lookup (replacements, list->data);
		if (ref) /* we don't actually replace the ref, but simply add one destination field */
			gda_parameter_declare_param_user (param, ref);

		list = g_slist_next (list);
	}
}

/**
 * gda_parameter_get_g_type
 * @param: a #GdaParameter object
 * 
 * Get the requested data type for @param.
 *
 * Returns: the data type
 */
GType
gda_parameter_get_g_type (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), G_TYPE_INVALID);
	g_return_val_if_fail (param->priv, G_TYPE_INVALID);

	return param->priv->g_type;
}


/**
 * gda_parameter_get_value
 * @param: a #GdaParameter object
 *
 * Get the value held into the parameter
 *
 * Returns: the value (a NULL value returns a GDA_TYPE_NULL GValue)
 */
const GValue *
gda_parameter_get_value (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (param->priv, NULL);

	if (!param->priv->alias_of) {
		if (!param->priv->value)
			param->priv->value = gda_value_new_null ();
		return param->priv->value;
	}
	else 
		return gda_parameter_get_value (param->priv->alias_of);
}

/**
 * gda_parameter_get_value_str
 * @param: a #GdaParameter object
 * 
 * Get a string representation of the value stored in @param. Calling
 * gda_parameter_set_value_str () with this value will restore @param's current
 * state.
 *
 * Returns: a new string, or %NULL if @param's value is NULL
 */
gchar *
gda_parameter_get_value_str (GdaParameter *param)
{
	const GValue *current_val;

	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (param->priv, NULL);

	current_val = gda_parameter_get_value (param);
	if (!current_val)
		return NULL;
	else {
		GdaDict *dict;
		GdaDataHandler *dh;

		dict = gda_object_get_dict (GDA_OBJECT (param));
		dh = gda_dict_get_handler (dict, param->priv->g_type);
		if (dh)
			return gda_data_handler_get_str_from_value (dh, current_val);
		else
			return NULL;
	}
}

/*
 * gda_parameter_set_value
 * @param: a #GdaParameter object
 * @value: a value to set the parameter to
 *
 * Sets the value within the parameter. If @param is an alias for another
 * parameter, then the value is also set for that other parameter.
 *
 * The action of any call to gda_parameter_declare_invalid() is cancelled
 * as soon as this method is called, even if @param's value does not change.
 * 
 * If the value is not different from the one already contained within @param,
 * then @param is not chaged and no signal is emitted.
 */
void
gda_parameter_set_value (GdaParameter *param, const GValue *value)
{
	gboolean changed = TRUE;
	const GValue *current_val;
#define DEBUG_PARAM
#undef DEBUG_PARAM

#ifdef DEBUG_PARAM
	gboolean was_valid = gda_parameter_is_valid (param);
#endif

	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

	param->priv->invalid_forced = FALSE;

	/* param will be changed? */
	current_val = gda_parameter_get_value (param);
	if (current_val == value)
		changed = FALSE;

	if (changed && gda_value_is_null ((GValue *)current_val) && 
	    ((value && gda_value_is_null ((GValue *)value)) || !value)) 
		changed = FALSE;
	
	if (changed && value && (G_VALUE_TYPE ((GValue *)value) == 
				 G_VALUE_TYPE ((GValue *)current_val)))
		changed = gda_value_compare ((GValue *)value, (GValue *)current_val);
		
	/* param's validity */
	param->priv->valid = TRUE;
	if (!value || gda_value_is_null ((GValue *)value))
		if (param->priv->not_null)
			param->priv->valid = FALSE;

	if (value &&
	    (G_VALUE_TYPE ((GValue *)value) != GDA_TYPE_NULL) &&
	    (G_VALUE_TYPE ((GValue *)value) != param->priv->g_type)) {
		param->priv->valid = FALSE;
	}

#ifdef DEBUG_PARAM
	g_print ("Changed param %p (%s): value %s --> %s \t(type %d -> %d) VALID: %d->%d CHANGED: %d\n", 
		 param, gda_object_get_name (param),
		 current_val ? gda_value_stringify ((GValue *)current_val) : "_NULL_",
		 value ? gda_value_stringify ((GValue *)(value)) : "_NULL_",
		 current_val ? G_VALUE_TYPE ((GValue *)current_val) : 0,
		 value ? G_VALUE_TYPE ((GValue *)value) : 0, 
		 was_valid, gda_parameter_is_valid (param), changed);
#endif

	/* end of procedure if the value has not been changed, after calculating the param's validity */
	if (!changed) {
		if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (param), "changed_pending"))) {
			gboolean changed_blocked;
			g_object_get (G_OBJECT (param), "changed_blocked", &changed_blocked, NULL);
			if (!changed_blocked) {
				g_object_set_data (G_OBJECT (param), "changed_pending", NULL);
				gda_object_signal_emit_changed (GDA_OBJECT (param));
			}
		}
		return;
	}

	/* the parameter does not have the default value forced since it has changed */
	param->priv->default_forced = FALSE;

	/* real setting of the value */
	if (param->priv->alias_of) {
#ifdef DEBUG_PARAM
		g_print ("Param %p is alias of param %p => propagating changes to param %p\n",
			 param, param->priv->alias_of, param->priv->alias_of);
#endif
		gda_parameter_set_value (param->priv->alias_of, value);
	}
	else {
		gboolean changed_blocked;
		if (param->priv->value) {
			gda_value_free (param->priv->value);
			param->priv->value = NULL;
		}

		if (value)
			param->priv->value = gda_value_copy ((GValue *)value);

		/* if the GdaObject has "changed_blocked" property TRUE, then store that the next time we need
		 * to emit the "changed" signal even if the stored value has not changed "
		 */
		g_object_get (G_OBJECT (param), "changed_blocked", &changed_blocked, NULL);
		if (changed_blocked)
			g_object_set_data (G_OBJECT (param), "changed_pending", GINT_TO_POINTER (TRUE));
		else
			gda_object_signal_emit_changed (GDA_OBJECT (param));
	}
}

/**
 * gda_parameter_set_value_str
 * @param: a #GdaParameter object
 * @value: a value to set the parameter to, as a string
 *
 * Same function as gda_parameter_set_value() except that the value
 * is provided as a string, and may return FALSE if the string did not
 * represent a correct value for the data type of the parameter.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_parameter_set_value_str (GdaParameter *param, const gchar *value)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), FALSE);
	g_return_val_if_fail (param->priv, FALSE);

	if (!value || !g_ascii_strcasecmp (value, "NULL")) {
		gda_parameter_set_value (param, NULL);	
		return TRUE;
	}
	else {
		GValue *gdaval = NULL;
		GdaDict *dict;
		GdaDataHandler *dh;

		dict = gda_object_get_dict (GDA_OBJECT (param));
		dh = gda_dict_get_handler (dict, param->priv->g_type);
		if (dh)
			gdaval = gda_data_handler_get_value_from_str (dh, value, param->priv->g_type);
		
		if (gdaval) {
			gda_parameter_set_value (param, gdaval);
			gda_value_free (gdaval);
			return TRUE;
		}
		else
			return FALSE;
	}
}

/**
 * gda_parameter_declare_invalid
 * @param: a #GdaParameter object
 *
 * Forces a parameter to be invalid; to set it valid again, a new value must be assigned
 * to it using gda_parameter_set_value().
 */
void
gda_parameter_declare_invalid (GdaParameter *param)
{
	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

#ifdef GDA_DEBUG_NO
	g_print ("Param %p (%s): declare invalid\n", param, gda_object_get_name (param));
#endif

	if (param->priv->invalid_forced)
		return;

	param->priv->invalid_forced = TRUE;
	param->priv->valid = FALSE;
	
	if (param->priv->value) {
		gda_value_free (param->priv->value);
		param->priv->value = NULL;
	}

	/* if we are an alias, then we forward the value setting to the master */
	if (param->priv->alias_of) 
		gda_parameter_declare_invalid (param->priv->alias_of);
	else 
		gda_object_signal_emit_changed (GDA_OBJECT (param));
}


/**
 * gda_parameter_is_valid
 * @param: a #GdaParameter object
 *
 * Get the validity of @param (that is, of the value held by @param)
 *
 * Returns: TRUE if @param's value can safely be used
 */
gboolean
gda_parameter_is_valid (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), FALSE);
	g_return_val_if_fail (param->priv, FALSE);

	if (param->priv->alias_of) {
		return gda_parameter_is_valid (param->priv->alias_of);
	}
	else {
		if (param->priv->invalid_forced) {
			return FALSE;
		}

		if (param->priv->default_forced) {
			return param->priv->default_value ? TRUE : FALSE;
		}
		else {
			return param->priv->valid;
		}
	}
}


/**
 * gda_parameter_get_default_value
 * @param: a #GdaParameter object
 *
 * Get the default value held into the parameter. WARNING: the default value does not need to be of 
 * the same type as the one required by @param.
 *
 * Returns: the default value
 */
const GValue *
gda_parameter_get_default_value (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (param->priv, NULL);

	if (param->priv->has_default_value)
		return param->priv->default_value;
	else
		return NULL;
}


/*
 * gda_parameter_set_default_value
 * @param: a #GdaParameter object
 * @value: a value to set the parameter's default value to
 *
 * Sets the default value within the parameter. WARNING: the default value does not need to be of 
 * the same type as the one required by @param.
 */
void
gda_parameter_set_default_value (GdaParameter *param, const GValue *value)
{
	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

	if (param->priv->default_value) {
		gda_value_free (param->priv->default_value);
		param->priv->default_value = NULL;
	}

	if (value) {
		param->priv->has_default_value = TRUE;
		param->priv->default_value = gda_value_copy ((GValue *)value);
	}
	
	/* don't emit the "changed" signal */
}

/**
 * gda_parameter_get_exists_default_value
 * @param: a #GdaParameter object
 *
 * Returns: TRUE if @param has a default value (which may be unspecified)
 */
gboolean
gda_parameter_get_exists_default_value (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), FALSE);
	g_return_val_if_fail (param->priv, FALSE);

	return param->priv->has_default_value;
}

/**
 * gda_parameter_set_exists_default_value
 *
 * Tells if @param has default unspecified value. This function is usefull
 * if one wants to inform that @param has a default value but does not know
 * what that default value actually is.
 */
void
gda_parameter_set_exists_default_value (GdaParameter *param, gboolean default_value_exists)
{
	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

	if (default_value_exists)
		param->priv->has_default_value = TRUE;
	else {
		gda_parameter_set_default_value (param, NULL);
		param->priv->has_default_value = FALSE;
	}
}


/**
 * gda_parameter_set_not_null
 * @param: a #GdaParameter object
 * @not_null:
 *
 * Sets if the parameter can have a NULL value. If @not_null is TRUE, then that won't be allowed
 */
void
gda_parameter_set_not_null (GdaParameter *param, gboolean not_null)
{
	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

	if (not_null != param->priv->not_null) {
		param->priv->not_null = not_null;

		/* updating the parameter's validity regarding the NULL value */
		if (!not_null && 
		    (!param->priv->value || gda_value_is_null (param->priv->value)))
			param->priv->valid = TRUE;

		if (not_null && 
		    (!param->priv->value || gda_value_is_null (param->priv->value)))
			param->priv->valid = FALSE;

		gda_object_signal_emit_changed (GDA_OBJECT (param));
	}
}

/**
 * gda_parameter_get_not_null
 * @param: a #GdaParameter object
 *
 * Get wether the parameter can be NULL or not
 *
 * Returns: TRUE if the parameter cannot be NULL
 */
gboolean
gda_parameter_get_not_null (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), FALSE);
	g_return_val_if_fail (param->priv, FALSE);

	return param->priv->not_null;
}

/**
 * gda_parameter_restrict_values
 * @param: a #GdaParameter object
 * @model: a #GdaDataModel object or NULL
 * @col: the reference column in @model
 * @error: location to store error, or %NULL
 *
 * Sets a limit on the possible values for the @param parameter: the value must be among the values
 * contained in the @col column of the @model data model.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_parameter_restrict_values (GdaParameter *param, GdaDataModel *model,
			       gint col, GError **error)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), FALSE);
	g_return_val_if_fail (param->priv, FALSE);

	/* No check is done on the validity of @col or even its existance */
	/* Note: for internal implementation if @col<0, then it's ignored */

	if (param->priv->restrict_model == model) {
		if (col >= 0)
			param->priv->restrict_col = col;
	}
	else {
		if (param->priv->restrict_model)
			destroyed_restrict_cb (GDA_OBJECT (param->priv->restrict_model), param);
		
		if (col >= 0)
			param->priv->restrict_col = col;
		
		if (model) {
			param->priv->restrict_model = model;
			g_object_ref (model);
			gda_object_connect_destroy (model, G_CALLBACK (destroyed_restrict_cb), param);
		}
	}

#ifdef GDA_DEBUG_signal
        g_print (">> 'RESTRICT_CHANGED' from %p\n", param);
#endif
	g_signal_emit (param, gda_parameter_signals[RESTRICT_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'RESTRICT_CHANGED' from %p\n", param);
#endif
	return TRUE;
}

static void
destroyed_restrict_cb (GdaObject *obj, GdaParameter *param)
{
	g_assert (param->priv->restrict_model == (GdaDataModel *)obj);
	g_signal_handlers_disconnect_by_func (obj,
					      G_CALLBACK (destroyed_restrict_cb), param);
	g_object_unref (obj);
	param->priv->restrict_model = NULL;
}

/**
 * gda_parameter_has_restrict_values
 * @param: a #GdaParameter
 * @model: a place to store a pointer to the model restricting the parameter, or %NULL
 * @col: a place to store the column in the model restricting the parameter, or %NULL
 *
 * Tells if @param has its values restricted by a #GdaDataModel, and optionnaly
 * allows to fetch the resteictions.
 *
 * Returns: TRUE if @param has its values restricted.
 */
gboolean
gda_parameter_has_restrict_values (GdaParameter *param, GdaDataModel **model, gint *col)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), FALSE);
	g_return_val_if_fail (param->priv, FALSE);
	
	if (model)
		*model = param->priv->restrict_model;
	if (col)
		*col = param->priv->restrict_col;

	return param->priv->restrict_model ? TRUE : FALSE;
}

/**
 * gda_parameter_bind_to_param
 * @param: a #GdaParameter
 * @bind_to: a #GdaParameter or %NULL
 *
 * Sets @param to change when @bind_to changes (and does not make @bind_to change when @param changes)
 */
void
gda_parameter_bind_to_param (GdaParameter *param, GdaParameter *bind_to)
{
	const GValue *cvalue;
	GValue *value1 = NULL, *value2 = NULL;

	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);
	g_return_if_fail (param != bind_to);

	if (param->priv->change_with == bind_to)
		return;

	/* get a copy of the current values of @param and @bind_to */
	if (bind_to) {
		g_return_if_fail (bind_to && GDA_IS_PARAMETER (bind_to));
		g_return_if_fail (bind_to->priv);
		g_return_if_fail (param->priv->g_type == bind_to->priv->g_type);
		cvalue = gda_parameter_get_value (bind_to);
		if (cvalue && !gda_value_is_null ((GValue*)cvalue))
			value2 = gda_value_copy ((GValue*)cvalue);
	}

	cvalue = gda_parameter_get_value (param);
	if (cvalue && !gda_value_is_null ((GValue*)cvalue))
		value1 = gda_value_copy ((GValue*)cvalue);

	/* get rid of the old alias */
	if (param->priv->change_with) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (param->priv->change_with),
						      G_CALLBACK (destroyed_alias_of_cb), param);
		g_signal_handlers_disconnect_by_func (G_OBJECT (param->priv->change_with),
						      G_CALLBACK (alias_of_changed_cb), param);
		param->priv->change_with = NULL;
	}

	/* setting the new alias or reseting the value if there is no new alias */
	if (bind_to) {
		gboolean equal = FALSE;

		param->priv->change_with = bind_to;
		gda_object_connect_destroy (param->priv->change_with, 
					       G_CALLBACK (destroyed_alias_of_cb), param);
		g_signal_connect (G_OBJECT (param->priv->change_with), "changed",
				  G_CALLBACK (alias_of_changed_cb), param);

		/* if alias_of has a different value than param, then we set param to the new value */
		if (value1 && value2 &&
		    (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2)))
			equal = !gda_value_compare (value1, value2);
		else {
			if (!value1 && !value2)
				equal = TRUE;
		}
		if (!equal)
			gda_parameter_set_value (param, value2);
	}
		
	if (value1) gda_value_free (value1);
	if (value2) gda_value_free (value2);
}

/*
 * gda_parameter_set_full_bind_param
 * @param: a #GdaParameter
 * @alias_of: a #GdaParameter or %NULL
 *
 * Sets @param to change when @alias_of changes and makes @alias_of change when @param changes.
 * The difference with gda_parameter_bind_to_param is that when @param changes, then @alias_of also
 * changes.
 */
static void
gda_parameter_set_full_bind_param (GdaParameter *param, GdaParameter *alias_of)
{
	const GValue *cvalue;
	GValue *value1 = NULL, *value2 = NULL;

	g_return_if_fail (GDA_IS_PARAMETER (param));
	g_return_if_fail (param->priv);

	if (param->priv->alias_of == alias_of)
		return;

	/* get a copy of the current values of @param and @alias_of */
	if (alias_of) {
		g_return_if_fail (alias_of && GDA_IS_PARAMETER (alias_of));
		g_return_if_fail (alias_of->priv);
		g_return_if_fail (param->priv->g_type == alias_of->priv->g_type);
		cvalue = gda_parameter_get_value (alias_of);
		if (cvalue && !gda_value_is_null ((GValue*)cvalue))
			value2 = gda_value_copy ((GValue*)cvalue);
	}

	cvalue = gda_parameter_get_value (param);
	if (cvalue && !gda_value_is_null ((GValue*)cvalue))
		value1 = gda_value_copy ((GValue*)cvalue);
		
	
	/* get rid of the old alias */
	if (param->priv->alias_of) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (param->priv->alias_of),
						      G_CALLBACK (destroyed_alias_of_cb), param);
		g_signal_handlers_disconnect_by_func (G_OBJECT (param->priv->alias_of),
						      G_CALLBACK (alias_of_changed_cb), param);
		param->priv->alias_of = NULL;
	}

	/* setting the new alias or reseting the value if there is no new alias */
	if (alias_of) {
		gboolean equal = FALSE;

		/* get rid of the internal param's value */
		if (param->priv->value) {
			gda_value_free (param->priv->value);
			param->priv->value = NULL;
		}

		param->priv->alias_of = alias_of;
		gda_object_connect_destroy (param->priv->alias_of, 
					       G_CALLBACK (destroyed_alias_of_cb), param);
		g_signal_connect (G_OBJECT (param->priv->alias_of), "changed",
				  G_CALLBACK (alias_of_changed_cb), param);

		/* if alias_of has a different value than param, then we emit a CHANGED signal */
		if (value1 && value2 &&
		    (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2)))
			equal = !gda_value_compare (value1, value2);
		else {
			if (!value1 && !value2)
				equal = TRUE;
		}

		if (!equal)
			gda_object_signal_emit_changed (GDA_OBJECT (param));
	}
	else {
		/* restore the value that was in the previous alias parameter, 
		 * if there was such a value, and don't emit a signal */
		g_assert (! param->priv->value);
		if (value1)
			param->priv->value = value1;
		value1 = NULL;
	}

	if (value1) gda_value_free (value1);
	if (value2) gda_value_free (value2);
}

static void
destroyed_alias_of_cb (GdaObject *obj, GdaParameter *param)
{
	if ((gpointer) obj == (gpointer) param->priv->alias_of)
		gda_parameter_set_full_bind_param (param, NULL);
	else
		gda_parameter_bind_to_param (param, NULL);
}

static void
alias_of_changed_cb (GdaParameter *alias_of, GdaParameter *param)
{
	if ((gpointer) alias_of == (gpointer) param->priv->alias_of) {
		/* callback used as full bind */
		gda_object_signal_emit_changed (GDA_OBJECT (param));
	}
	else {
		/* callback used as simple bind */
		gda_parameter_set_value (param, gda_parameter_get_value (alias_of));
	}
}

static void
gda_parameter_signal_changed (GdaObject *base, gboolean block_changed_signal)
{
	GdaParameter *param = GDA_PARAMETER (base);

	if (param->priv->alias_of) {
		if (block_changed_signal)
			gda_object_block_changed (GDA_OBJECT (param->priv->alias_of));
		else
			gda_object_unblock_changed (GDA_OBJECT (param->priv->alias_of));
	}
}

/**
 * gda_parameter_get_bind_param
 * @param: a #GdaParameter
 *
 * Get the parameter which makes @param change its value when the param's value is changed.
 *
 * Returns: the #GdaParameter or %NULL
 */
GdaParameter *
gda_parameter_get_bind_param (GdaParameter *param)
{
	g_return_val_if_fail (GDA_IS_PARAMETER (param), NULL);
	g_return_val_if_fail (param->priv, NULL);

	return param->priv->change_with;
}

#ifdef GDA_DEBUG
static void
gda_parameter_dump (GdaParameter *parameter, guint offset)
{
	gchar *str;

	g_return_if_fail (parameter);
	g_return_if_fail (GDA_IS_PARAMETER (parameter));

	/* string for the offset */
	str = g_new0 (gchar, offset+1);
	memset (str, ' ', offset);
	
	/* dump */
	if (parameter->priv) {
		GSList *list;
		gchar *strval;

		strval = gda_value_stringify ((GValue *) gda_parameter_get_value (parameter));
		g_print ("%s" D_COL_H1 "GdaParameter %p (%s), type=%s, %s, value=%s\n" D_COL_NOR, str, parameter,
			 gda_object_get_name (GDA_OBJECT (parameter)), 
			 gda_g_type_to_string (parameter->priv->g_type),
			 gda_parameter_is_valid (parameter) ? "VALID" : "INVALID",
			 strval);
		g_free (strval);
		
		list = parameter->priv->param_users;
		while (list) {
			gchar *xmlid = NULL;

			if (GDA_IS_XML_STORAGE (list->data))
				xmlid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));

			if (list == parameter->priv->param_users)
				g_print ("%sFor fields: ", str);
			else
				g_print (", ");
			if (xmlid) {
				g_print ("%p (%s)", list->data, xmlid);
				g_free (xmlid);
			}
			else
				g_print ("%p", list->data);

			list = g_slist_next (list);
		}
		if (parameter->priv->param_users)
			g_print ("\n");

		if (parameter->priv->restrict_model) {
				g_print ("%sParameter restricted by column %d of data model %p\n", str,
					 parameter->priv->restrict_col,
					 parameter->priv->restrict_model);
				gda_object_dump (GDA_OBJECT (parameter->priv->restrict_model), offset+5);
		}

		if (parameter->priv->alias_of) {
			g_print ("%sAlias of parameter %p (%s)\n", str, parameter->priv->alias_of,
				 gda_object_get_name (GDA_OBJECT (parameter->priv->alias_of)));
			gda_parameter_dump (parameter->priv->alias_of, offset + 5);
		}
	}
	else
		g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, str, parameter);

	g_free (str);
}
#endif


/* 
 * GdaReferer interface implementation
 */

static void
gda_parameter_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	GdaParameter *parameter;
	gpointer repl;

	g_return_if_fail (iface && GDA_IS_PARAMETER (iface));
	g_return_if_fail (GDA_PARAMETER (iface)->priv);

	parameter = GDA_PARAMETER (iface);
	
	gda_parameter_replace_param_users (parameter, replacements);
	
	if (parameter->priv->alias_of) {
		repl = g_hash_table_lookup (replacements, parameter->priv->alias_of);
		if (repl) 
			gda_parameter_set_full_bind_param (parameter, repl);
	}

	if (parameter->priv->change_with) {
		repl = g_hash_table_lookup (replacements, parameter->priv->change_with);
		if (repl)
			gda_parameter_bind_to_param (parameter, GDA_PARAMETER (repl));
	}
}
