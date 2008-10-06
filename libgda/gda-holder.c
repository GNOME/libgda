/* gda-holder.c
 *
 * Copyright (C) 2003 - 2008 Vivien Malerba
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

#include <glib/gi18n-lib.h>
#include <stdarg.h>
#include <string.h>
#include "gda-holder.h"
#include "gda-statement.h"
#include "gda-data-model.h"
#include "gda-data-handler.h"
#include "gda-marshal.h"
#include "gda-util.h"
#include <libgda.h>
#include <libgda/gda-attributes-manager.h>

/* 
 * Main static functions 
 */
static void gda_holder_class_init (GdaHolderClass * class);
static void gda_holder_init (GdaHolder *holder);
static void gda_holder_dispose (GObject *object);
static void gda_holder_finalize (GObject *object);

static void gda_holder_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gda_holder_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

static void full_bind_changed_cb (GdaHolder *alias_of, GdaHolder *holder);
static void gda_holder_set_full_bind (GdaHolder *holder, GdaHolder *alias_of);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;
GdaAttributesManager *gda_holder_attributes_manager;

/* signals */
enum
{
	CHANGED,
        SOURCE_CHANGED,
	VALIDATE_CHANGE,
	ATT_CHANGED,
        LAST_SIGNAL
};

static gint gda_holder_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };


/* properties */
enum
{
        PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_DESCR,
	PROP_SIMPLE_BIND,
	PROP_FULL_BIND,
	PROP_SOURCE_MODEL,
	PROP_SOURCE_COLUMN,
	PROP_GDA_TYPE,
	PROP_NOT_NULL
};


struct _GdaHolderPrivate
{
	gchar           *id;

	GType            g_type;
	GdaHolder       *full_bind;     /* FULL bind to holder */
	GdaHolder       *simple_bind;  /* SIMPLE bind to holder */
	
	gboolean         invalid_forced;
	gboolean         valid;
	gboolean         is_freeable;

	GValue           *value;
	GValue          *default_value; /* CAN be either NULL or of any type */
	gboolean         default_forced;
	gboolean         not_null;      /* TRUE if 'value' must not be NULL when passed to destination fields */

	GdaDataModel    *source_model;
	gint             source_col;
};

/* module error */
GQuark gda_holder_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_holder_error");
        return quark;
}

GType
gda_holder_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaHolderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_holder_class_init,
			NULL,
			NULL,
			sizeof (GdaHolder),
			0,
			(GInstanceInitFunc) gda_holder_init
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaHolder", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static gboolean
validate_change_accumulator (GSignalInvocationHint *ihint,
			   GValue *return_accu,
			   const GValue *handler_return,
			   gpointer data)
{
	GError *error;

	error = g_value_get_pointer (handler_return);
	g_value_set_pointer (return_accu, error);

	return error ? FALSE : TRUE; /* stop signal if 'thisvalue' is FALSE */
}

static GError *
m_validate_change (GdaHolder *holder, const GValue *new_value)
{
	return NULL;
}

static void
gda_holder_class_init (GdaHolderClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_holder_signals[SOURCE_CHANGED] =
                g_signal_new ("source-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaHolderClass, source_changed),
                              NULL, NULL,
                              gda_marshal_VOID__VOID, G_TYPE_NONE, 0);
	gda_holder_signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaHolderClass, changed),
                              NULL, NULL,
                              gda_marshal_VOID__VOID, G_TYPE_NONE, 0);
	gda_holder_signals[ATT_CHANGED] =
                g_signal_new ("attribute-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaHolderClass, att_changed),
                              NULL, NULL,
                              gda_marshal_VOID__STRING_POINTER, G_TYPE_NONE, 2, 
			      G_TYPE_STRING, G_TYPE_POINTER);

	/**
	 * GdaHolder::before-change:
	 * @holder: the object which received the signal
	 * @new_value: the proposed new value for @holder
	 * 
	 * Gets emitted when @holder is going to change its value. One can connect to
	 * this signal to control which values @holder can have (for example to implement some business rules)
	 *
	 * Return value: NULL if @holder is allowed to change its value to @new_value, or a #GError
	 * otherwise.
	 */
	gda_holder_signals[VALIDATE_CHANGE] =
		g_signal_new ("validate-change",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaHolderClass, validate_change),
                              validate_change_accumulator, NULL,
                              gda_marshal_POINTER__POINTER, G_TYPE_POINTER, 1, G_TYPE_POINTER);

        class->changed = NULL;
        class->source_changed = NULL;
        class->validate_change = m_validate_change;
	class->att_changed = NULL;

	/* virtual functions */
	object_class->dispose = gda_holder_dispose;
	object_class->finalize = gda_holder_finalize;

	/* Properties */
	object_class->set_property = gda_holder_set_property;
	object_class->get_property = gda_holder_get_property;
	g_object_class_install_property (object_class, PROP_ID,
					 g_param_spec_string ("id", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DESCR,
					 g_param_spec_string ("description", NULL, NULL, NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_GDA_TYPE,
                                         g_param_spec_ulong ("g-type", NULL, NULL,
							   0, G_MAXULONG, GDA_TYPE_NULL,
							   (G_PARAM_READABLE | 
							    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));
	g_object_class_install_property (object_class, PROP_NOT_NULL,
					 g_param_spec_boolean ("not-null", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SIMPLE_BIND,
					 g_param_spec_object ("simple-bind", NULL, NULL, 
                                                               GDA_TYPE_HOLDER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_FULL_BIND,
					 g_param_spec_object ("full-bind", NULL, NULL, 
                                                               GDA_TYPE_HOLDER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SOURCE_MODEL,
                                         g_param_spec_object ("source-model", NULL, NULL,
                                                               GDA_TYPE_DATA_MODEL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_SOURCE_COLUMN,
                                         g_param_spec_int ("source-column", NULL, NULL,
							   0, G_MAXINT, 0,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
	/* extra */
	gda_holder_attributes_manager = gda_attributes_manager_new (TRUE);
}

static void
gda_holder_init (GdaHolder *holder)
{
	holder->priv = g_new0 (GdaHolderPrivate, 1);

	holder->priv->id = NULL;

	holder->priv->g_type = G_TYPE_INVALID;
	holder->priv->full_bind = NULL;
	holder->priv->simple_bind = NULL;

	holder->priv->invalid_forced = FALSE;
	holder->priv->valid = TRUE;
	holder->priv->default_forced = FALSE;
	holder->priv->is_freeable = TRUE;
	holder->priv->value = NULL;
	holder->priv->default_value = NULL;

	holder->priv->not_null = FALSE;
	holder->priv->source_model = NULL;
	holder->priv->source_col = 0;
}

/**
 * gda_holder_new
 * @type: the #GType requested
 *
 * Creates a new holder of type @type
 *
 * Returns: a new #GdaHolder object
 */
GdaHolder *
gda_holder_new (GType type)
{
	GObject   *obj;

        obj = g_object_new (GDA_TYPE_HOLDER, "g-type", type, NULL);

        return (GdaHolder *) obj;
}

/**
 * gda_holder_copy
 * @orig: a #GdaHolder object to copy
 *
 * Copy constructor.
 * 
 * Note1: if @orig is set with a static value (see #gda_holder_take_static_value ()) 
 * its copy will have a fresh new allocated GValue, so that user should free it when done.
 *
 * Returns: a new #GdaHolder object
 */
GdaHolder *
gda_holder_copy (GdaHolder *orig)
{
	GObject *obj;
	GdaHolder *holder;
	gboolean allok = TRUE;

	g_return_val_if_fail (orig && GDA_IS_HOLDER (orig), NULL);
	g_return_val_if_fail (orig->priv, NULL);

	obj = g_object_new (GDA_TYPE_HOLDER, "g-type", orig->priv->g_type, NULL);
	holder = GDA_HOLDER (obj);

	if (orig->priv->id)
		holder->priv->id = g_strdup (orig->priv->id);

	if (orig->priv->full_bind)
		gda_holder_set_full_bind (holder, orig->priv->full_bind);
	if (orig->priv->simple_bind) 
		allok = gda_holder_set_bind (holder, orig->priv->simple_bind, NULL);
	
	if (allok && orig->priv->source_model) {
		/*g_print ("Source holder %p\n", holder);*/
		allok = gda_holder_set_source_model (holder, orig->priv->source_model, orig->priv->source_col,
						     NULL);
	}

	if (allok) {
		/* direct settings */
		holder->priv->invalid_forced = orig->priv->invalid_forced;
		holder->priv->valid = orig->priv->valid;
		holder->priv->is_freeable = TRUE;
		holder->priv->default_forced = orig->priv->default_forced;	
		if (orig->priv->value)
			holder->priv->value = gda_value_copy (orig->priv->value);
		if (orig->priv->default_value)
			holder->priv->default_value = gda_value_copy (orig->priv->default_value);
		holder->priv->not_null = orig->priv->not_null;
		gda_attributes_manager_copy (gda_holder_attributes_manager, (gpointer) orig, gda_holder_attributes_manager, (gpointer) holder);

		GValue *att_value;
		g_value_set_boolean ((att_value = gda_value_new (G_TYPE_BOOLEAN)), holder->priv->default_forced);
		gda_holder_set_attribute (holder, GDA_ATTRIBUTE_IS_DEFAULT, att_value);
		gda_value_free (att_value);


		return holder;
	}
	else {
		g_warning ("Internal error: could not copy GdaHolder (please report a bug).");
		g_object_unref (holder);
		return NULL;
	}
}

/**
 * gda_holder_new_inline
 * @type: a valid GLib type
 * @id: the id of the holder to create, or %NULL
 * @...: value to set
 *
 * Creates a new #GdaHolder object named @name, of type @type, and containing the value passed
 * as the last argument.
 *
 * Note that this function is a utility function and that anly a limited set of types are supported. Trying
 * to use an unsupported type will result in a warning, and the returned value holder holding a safe default
 * value.
 *
 * Returns: a new #GdaHolder object
 */
GdaHolder *
gda_holder_new_inline (GType type, const gchar *id, ...)
{
	GdaHolder *holder;

	static GStaticMutex serial_mutex = G_STATIC_MUTEX_INIT;
	static guint serial = 0;

	holder = gda_holder_new (type);
	if (holder) {
		GValue *value;
		va_list ap;
		GError *lerror = NULL;

		if (id)
			holder->priv->id = g_strdup (id);
		else {
			g_static_mutex_lock (&serial_mutex);
			holder->priv->id = g_strdup_printf ("%d", serial++);
			g_static_mutex_unlock (&serial_mutex);
		}

		va_start (ap, id);
		value = gda_value_new (type);
		if (type == G_TYPE_BOOLEAN) 
			g_value_set_boolean (value, va_arg (ap, int));
                else if (type == G_TYPE_STRING)
			g_value_set_string (value, va_arg (ap, gchar *));
                else if (type == G_TYPE_OBJECT)
			g_value_set_object (value, va_arg (ap, gpointer));
		else if (type == G_TYPE_INT)
			g_value_set_int (value, va_arg (ap, gint));
		else if (type == G_TYPE_UINT)
			g_value_set_uint (value, va_arg (ap, guint));
		else if (type == GDA_TYPE_BINARY)
			gda_value_set_binary (value, va_arg (ap, GdaBinary *));
		else if (type == G_TYPE_INT64)
			g_value_set_int64 (value, va_arg (ap, gint64));
		else if (type == G_TYPE_UINT64)
			g_value_set_uint64 (value, va_arg (ap, guint64));
		else if (type == GDA_TYPE_SHORT)
			gda_value_set_short (value, va_arg (ap, int));
		else if (type == GDA_TYPE_USHORT)
			gda_value_set_ushort (value, va_arg (ap, guint));
		else if (type == G_TYPE_CHAR)
			g_value_set_char (value, va_arg (ap, int));
		else if (type == G_TYPE_UCHAR)
			g_value_set_uchar (value, va_arg (ap, guint));
		else if (type == G_TYPE_FLOAT)
			g_value_set_float (value, va_arg (ap, double));
		else if (type == G_TYPE_DOUBLE)
			g_value_set_double (value, va_arg (ap, gdouble));
		else if (type == GDA_TYPE_NUMERIC)
			gda_value_set_numeric (value, va_arg (ap, GdaNumeric *));
		else if (type == G_TYPE_DATE)
			g_value_set_boxed (value, va_arg (ap, GDate *));
		else {
			g_warning ("%s() does not handle values of type %s, value will not be assigned.",
				   __FUNCTION__, g_type_name (type));
			g_object_unref (holder);
			holder = NULL;
		}
		va_end (ap);

		if (holder && !gda_holder_set_value (holder, value, &lerror)) {
			g_warning (_("Unable to set holder's value: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
			g_object_unref (holder);
			holder = NULL;
		}
		gda_value_free (value);
	}

	return holder;
}

static void
gda_holder_dispose (GObject *object)
{
	GdaHolder *holder;

	holder = GDA_HOLDER (object);
	if (holder->priv) {
		gda_holder_set_bind (holder, NULL, NULL);
		gda_holder_set_full_bind (holder, NULL);

		if (holder->priv->source_model) {
			g_object_unref (holder->priv->source_model);
			holder->priv->source_model = NULL;
		}

		holder->priv->g_type = G_TYPE_INVALID;

		if (holder->priv->value) {			
			if (holder->priv->is_freeable)
				gda_value_free (holder->priv->value);
			holder->priv->value = NULL;
		}

		if (holder->priv->default_value) {
			gda_value_free (holder->priv->default_value);
			holder->priv->default_value = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_holder_finalize (GObject   * object)
{
	GdaHolder *holder;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HOLDER (object));

	holder = GDA_HOLDER (object);
	if (holder->priv) {
		g_free (holder->priv->id);

		g_free (holder->priv);
		holder->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_holder_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdaHolder *holder;

	holder = GDA_HOLDER (object);
	if (holder->priv) {
		switch (param_id) {
		case PROP_ID:
			g_free (holder->priv->id);
			holder->priv->id = g_value_dup_string (value);
			break;
		case PROP_NAME:
			gda_holder_set_attribute (holder, GDA_ATTRIBUTE_NAME, value);
			break;
		case PROP_DESCR:
			gda_holder_set_attribute (holder, GDA_ATTRIBUTE_DESCRIPTION, value);
			break;
		case PROP_GDA_TYPE:
			if (holder->priv->g_type == GDA_TYPE_NULL)
				holder->priv->g_type = g_value_get_ulong (value);
			else
				g_warning (_("The 'g-type' property cannot be changed"));
			break;
		case PROP_NOT_NULL: {
			gboolean not_null = g_value_get_boolean (value);
			if (not_null != holder->priv->not_null) {
				holder->priv->not_null = not_null;
				
				/* updating the holder's validity regarding the NULL value */
				if (!not_null && 
				    (!holder->priv->value || gda_value_is_null (holder->priv->value)))
					holder->priv->valid = TRUE;
				
				if (not_null && 
				    (!holder->priv->value || gda_value_is_null (holder->priv->value)))
					holder->priv->valid = FALSE;
				
				g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
			}
			break;
		}
		case PROP_SIMPLE_BIND:
			if (!gda_holder_set_bind (holder, GDA_HOLDER (g_value_get_object (value)), NULL))
				g_warning ("Could not set the 'simple-bind' property");
			break;
		case PROP_FULL_BIND:
			gda_holder_set_full_bind (holder, GDA_HOLDER (g_value_get_object (value)));
			break;
		case PROP_SOURCE_MODEL: {
			GdaDataModel* ptr = g_value_get_object (value);
			g_return_if_fail (gda_holder_set_source_model (holder, 
								       (GdaDataModel *)ptr, -1, NULL));
			break;
                }
		case PROP_SOURCE_COLUMN:
			holder->priv->source_col = g_value_get_int (value);
			break;
		}
	}
}

static void
gda_holder_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdaHolder *holder;
	const GValue *cvalue;

	holder = GDA_HOLDER (object);
	if (holder->priv) {
		switch (param_id) {
		case PROP_ID:
			g_value_set_string (value, holder->priv->id);
			break;
		case PROP_NAME:
			cvalue = gda_holder_get_attribute (holder, GDA_ATTRIBUTE_NAME);
			if (cvalue)
				g_value_set_string (value, g_value_get_string (cvalue));
			else
				g_value_set_string (value, holder->priv->id);
			break;
		case PROP_DESCR:
			cvalue = gda_holder_get_attribute (holder, GDA_ATTRIBUTE_DESCRIPTION);
			if (cvalue)
				g_value_set_string (value, g_value_get_string (cvalue));
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_GDA_TYPE:
			g_value_set_ulong (value, holder->priv->g_type);
			break;
		case PROP_NOT_NULL:
			g_value_set_boolean (value, gda_holder_get_not_null (holder));
			break;
		case PROP_SIMPLE_BIND:
			g_value_set_object (value, G_OBJECT (holder->priv->simple_bind));
			break;
		case PROP_FULL_BIND:
			g_value_set_object (value, G_OBJECT (holder->priv->full_bind));
			break;
		case PROP_SOURCE_MODEL:
			g_value_set_object (value, G_OBJECT (holder->priv->source_model));
			break;
		case PROP_SOURCE_COLUMN:
			g_value_set_int (value, holder->priv->source_col);
			break;	
		}
	}
}

/**
 * gda_holder_get_g_type
 * @holder: a #GdaHolder object
 * 
 * Get @holder's type
 *
 * Returns: the data type
 */
GType
gda_holder_get_g_type (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), G_TYPE_INVALID);
	g_return_val_if_fail (holder->priv, G_TYPE_INVALID);

	return holder->priv->g_type;
}

/**
 * gda_holder_get_id
 * @holder: a #GdaHolder object
 *
 * Get the ID of @holder. The ID can be set using @holder's "id" property
 *
 * Returns: the ID (don't modify the string).
 */
const gchar *
gda_holder_get_id (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (holder->priv, NULL);

	return holder->priv->id;
}


/**
 * gda_holder_get_value
 * @holder: a #GdaHolder object
 *
 * Get the value held into the holder. If @holder is set to use its default value
 * and that default value is not of the same type as @holder, then %NULL is returned.
 *
 * If @holder is set to NULL, then the returned value is a #GDA_TYPE_NULL GValue.
 *
 * Returns: the value, or %NULL
 */
const GValue *
gda_holder_get_value (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (holder->priv, NULL);

	if (!holder->priv->full_bind) {
		/* return default value is possible */
		if (holder->priv->default_forced) {
			g_assert (holder->priv->default_value);
			if (G_VALUE_TYPE (holder->priv->default_value) == holder->priv->g_type) 
				return holder->priv->default_value;
			else
				return NULL;
		}

		if (!holder->priv->value)
			holder->priv->value = gda_value_new_null ();
		return holder->priv->value;
	}
	else 
		return gda_holder_get_value (holder->priv->full_bind);
}

/**
 * gda_holder_get_value_str
 * @holder: a #GdaHolder object
 * @dh: a #GdaDataHandler to use, or %NULL
 *
 * Same functionality as gda_holder_get_value() except that it returns the value as a string
 * (the conversion is done using @dh if not %NULL, or the default data handler otherwise).
 *
 * Returns: the value, or %NULL
 */
gchar *
gda_holder_get_value_str (GdaHolder *holder, GdaDataHandler *dh)
{
	const GValue *current_val;

	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (holder->priv, NULL);

	current_val = gda_holder_get_value (holder);
        if (!current_val || gda_value_is_null (current_val))
                return NULL;
        else {
                GdaDataHandler *dh;

                dh = gda_get_default_handler (holder->priv->g_type);
                if (dh)
                        return gda_data_handler_get_str_from_value (dh, current_val);
                else
                        return NULL;
        }
}

static gboolean real_gda_holder_set_value (GdaHolder *holder, GValue *value, gboolean do_copy, GError **error);

/**
 * gda_holder_set_value
 * @holder: a #GdaHolder object
 * @value: a value to set the holder to, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Sets the value within the holder. If @holder is an alias for another
 * holder, then the value is also set for that other holder.
 *
 * On success, the action of any call to gda_holder_force_invalid() is cancelled
 * as soon as this method is called (even if @holder's value does not actually change)
 * 
 * If the value is not different from the one already contained within @holder,
 * then @holder is not changed and no signal is emitted.
 *
 * Note1: the @value argument is treated the same way if it is %NULL or if it is a #GDA_TYPE_NULL value
 *
 * Note2: if @holder can't accept the @value value, then this method returns FALSE, and @holder will be left
 * in an invalid state.
 *
 * Note3: before the change is accepted by @holder, the "validate-change" signal will be emitted (the value
 * of which can prevent the change from happening) which can be connected to to have a greater control
 * of which values @holder can have, or implement some business rules.
 *
 * Returns: TRUE if value has been set
 */
gboolean
gda_holder_set_value (GdaHolder *holder, const GValue *value, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	return real_gda_holder_set_value (holder, (GValue*) value, TRUE, error);
}

/**
 * gda_holder_set_value_str
 * @holder: a #GdaHolder object
 * @dh: a #GdaDataHandler to use, or %NULL
 * @value: a value to set the holder to, as a string
 * @error: a place to store errors, or %NULL
 *
 * Same functionality as gda_holder_set_value() except that it uses a string representation
 * of the value to set, which will be converted into a GValue first (using default data handler if
 * @dh is %NULL).
 *
 * Note1: if @value is %NULL or is the "NULL" string, then @holder's value is set to %NULL.
 * Note2: if @holder can't accept the @value value, then this method returns FALSE, and @holder will be left
 * in an invalid state.
 *
 * Returns: TRUE if value has been set
 */
gboolean
gda_holder_set_value_str (GdaHolder *holder, GdaDataHandler *dh, const gchar *value, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);
	g_return_val_if_fail (!dh || GDA_IS_DATA_HANDLER (dh), FALSE);

	if (!value || !g_ascii_strcasecmp (value, "NULL")) 
                return gda_holder_set_value (holder, NULL, error);
    else {
		GValue *gdaval = NULL;

		if (!dh)
			dh = gda_get_default_handler (holder->priv->g_type);
		if (dh)
        	gdaval = gda_data_handler_get_value_from_str (dh, value, holder->priv->g_type);

		if (gdaval)
			return real_gda_holder_set_value (holder, gdaval, FALSE, error);
        else {
			g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_STRING_CONVERSION_ERROR,
				     _("Unable to convert string to '%s' type"), 
				     gda_g_type_to_string (holder->priv->g_type));
        	return FALSE;
		}
	}
}

/**
 * gda_holder_take_value
 * @holder: a #GdaHolder object
 * @value: a value to set the holder to
 * @error: a place to store errors, or %NULL
 *
 * Sets the value within the holder. If @holder is an alias for another
 * holder, then the value is also set for that other holder.
 *
 * On success, the action of any call to gda_holder_force_invalid() is cancelled
 * as soon as this method is called (even if @holder's value does not actually change).
 * 
 * If the value is not different from the one already contained within @holder,
 * then @holder is not chaged and no signal is emitted.
 *
 * Note1: if @holder can't accept the @value value, then this method returns FALSE, and @holder will be left
 * in an invalid state.
 *
 * Note2: before the change is accepted by @holder, the "validate-change" signal will be emitted (the value
 * of which can prevent the change from happening) which can be connected to to have a greater control
 * of which values @holder can have, or implement some business rules.
 *
 * Note3: if user previously set this holder with gda_holder_take_static_value () the GValue
 * stored internally will be forgiven and replaced by the @value. User should then
 * take care of the 'old' static GValue.
 *
 * Returns: TRUE if value has been set
 */
gboolean
gda_holder_take_value (GdaHolder *holder, GValue *value, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	return real_gda_holder_set_value (holder, (GValue*) value, FALSE, error);
}

static gboolean
real_gda_holder_set_value (GdaHolder *holder, GValue *value, gboolean do_copy, GError **error)
{
	gboolean changed = TRUE;
	gboolean newvalid;
	const GValue *current_val;
	gboolean newnull;
#define DEBUG_HOLDER
#undef DEBUG_HOLDER

#ifdef DEBUG_HOLDER
	gboolean was_valid = gda_holder_is_valid (holder);
#endif

	/* if the value has been set with gda_holder_take_static_value () you'll be able
	 * to change the value only with another call to real_gda_holder_set_value 
	 */
	if (!holder->priv->is_freeable) {
		g_warning (_("Can't use this method to set value because there is already a static value"));
		return FALSE;
	}
		
	/* holder will be changed? */
	newnull = !value || gda_value_is_null (value);
	current_val = gda_holder_get_value (holder);
	if (current_val == value)
		changed = FALSE;
	else if ((!current_val || gda_value_is_null ((GValue *)current_val)) && newnull)
		changed = FALSE;
	else if (value && current_val &&
		 (G_VALUE_TYPE (value) == G_VALUE_TYPE ((GValue *)current_val)))
		changed = gda_value_differ (value, (GValue *)current_val);
		
	/* holder's validity */
	newvalid = TRUE;
	if (newnull && holder->priv->not_null) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_NULL_ERROR,
			     _("Holder does not allow NULL values"));
		newvalid = FALSE;
		changed = TRUE;
	}
	else if (!newnull && (G_VALUE_TYPE (value) != holder->priv->g_type)) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
			     _("Wrong value type: expected type '%s' when value's type is '%s'"),
			     gda_g_type_to_string (holder->priv->g_type),
			     gda_g_type_to_string (G_VALUE_TYPE (value)));
		newvalid = FALSE;
		changed = TRUE;
	}

#ifdef DEBUG_HOLDER
	g_print ("Changed holder %p (%s): value %s --> %s \t(type %d -> %d) VALID: %d->%d CHANGED: %d\n", 
		 holder, holder->priv->id,
		 current_val ? gda_value_stringify ((GValue *)current_val) : "_NULL_",
		 value ? gda_value_stringify ((value)) : "_NULL_",
		 current_val ? G_VALUE_TYPE ((GValue *)current_val) : 0,
		 value ? G_VALUE_TYPE (value) : 0, 
		 was_valid, newvalid, changed);
#endif

	/* end of procedure if the value has not been changed, after calculating the holder's validity */
	if (!changed) {
		if (!do_copy && value)
			gda_value_free (value);
		holder->priv->invalid_forced = FALSE;
		holder->priv->valid = newvalid;
		return TRUE;
	}

	/* check if we are allowed to change value */
	GError *lerror = NULL;
	g_signal_emit (holder, gda_holder_signals[VALIDATE_CHANGE], 0, value, &lerror);
	if (lerror) {
		/* change refused by signal callback */
		g_propagate_error (error, lerror);
		if (!do_copy) 
			gda_value_free (value);
		return FALSE;
	}

	/* new valid status */
	holder->priv->invalid_forced = FALSE;
	holder->priv->valid = newvalid;
	/* we're setting a non-static value, so be sure to flag is as freeable */
	holder->priv->is_freeable = TRUE;

	/* check is the new value is the default one */
	holder->priv->default_forced = FALSE;
	if (holder->priv->default_value) {
		if ((G_VALUE_TYPE (holder->priv->default_value) == GDA_TYPE_NULL) && newnull)
			holder->priv->default_forced = TRUE;
		else if ((G_VALUE_TYPE (holder->priv->default_value) == holder->priv->g_type) &&
			 value && (G_VALUE_TYPE (value) == holder->priv->g_type))
			holder->priv->default_forced = !gda_value_compare (holder->priv->default_value, value);
	}
	GValue *att_value;
	g_value_set_boolean ((att_value = gda_value_new (G_TYPE_BOOLEAN)), holder->priv->default_forced);
	gda_holder_set_attribute (holder, GDA_ATTRIBUTE_IS_DEFAULT, att_value);
	gda_value_free (att_value);

	/* real setting of the value */
	if (holder->priv->full_bind) {
#ifdef DEBUG_HOLDER
		g_print ("Holder %p is alias of holder %p => propagating changes to holder %p\n",
			 holder, holder->priv->full_bind, holder->priv->full_bind);
#endif
		return real_gda_holder_set_value (holder->priv->full_bind, value, do_copy, error);
	}
	else {
		if (holder->priv->value) {
			gda_value_free (holder->priv->value);
			holder->priv->value = NULL;
		}

		if (value) {
			if (newvalid) {
				if (do_copy)
					holder->priv->value = gda_value_copy (value);
				else
					holder->priv->value = value;
			}
			else if (!do_copy) 
				gda_value_free (value);
		}

		g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	}

	return newvalid;
}

static GValue *
real_gda_holder_set_const_value (GdaHolder *holder, const GValue *value, 
								 gboolean *value_changed, GError **error)
{
	gboolean changed = TRUE;
	gboolean newvalid;
	const GValue *current_val;
	GValue *value_to_return = NULL;
	gboolean newnull;
#define DEBUG_HOLDER
#undef DEBUG_HOLDER

#ifdef DEBUG_HOLDER
	gboolean was_valid = gda_holder_is_valid (holder);
#endif

	/* holder will be changed? */
	newnull = !value || gda_value_is_null (value);
	current_val = gda_holder_get_value (holder);
	if (current_val == value)
		changed = FALSE;
	else if ((!current_val || gda_value_is_null (current_val)) && newnull) 
		changed = FALSE;
	else if (value && current_val &&
		 (G_VALUE_TYPE (value) == G_VALUE_TYPE (current_val))) 
		changed = gda_value_differ (value, current_val);
		
	/* holder's validity */
	newvalid = TRUE;
	if (newnull && holder->priv->not_null) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_NULL_ERROR,
			     _("Holder does not allow NULL values"));
		newvalid = FALSE;
		changed = TRUE;
	}
	else if (!newnull && (G_VALUE_TYPE (value) != holder->priv->g_type)) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
			     _("Wrong value type: expected type '%s' when value's type is '%s'"),
			     gda_g_type_to_string (holder->priv->g_type),
			     gda_g_type_to_string (G_VALUE_TYPE (value)));
		newvalid = FALSE;
		changed = TRUE;
	}

#ifdef DEBUG_HOLDER
	g_print ("Changed holder %p (%s): value %s --> %s \t(type %d -> %d) VALID: %d->%d CHANGED: %d\n", 
		 holder, holder->priv->id,
		 current_val ? gda_value_stringify ((GValue *)current_val) : "_NULL_",
		 value ? gda_value_stringify ((value)) : "_NULL_",
		 current_val ? G_VALUE_TYPE ((GValue *)current_val) : 0,
		 value ? G_VALUE_TYPE (value) : 0, 
		 was_valid, newvalid, changed);
#endif
	

	/* end of procedure if the value has not been changed, after calculating the holder's validity */
	if (!changed) {
		holder->priv->invalid_forced = FALSE;
		holder->priv->valid = newvalid;
#ifdef DEBUG_HOLDER		
		g_print ("Holder is not changed, returning %p\n", holder->priv->value);
#endif		

		/* set the changed status */
		*value_changed = FALSE;
		return holder->priv->value;
	}
	else {
		*value_changed = TRUE;
	}

	/* check if we are allowed to change value */
	GError *lerror = NULL;
	g_signal_emit (holder, gda_holder_signals[VALIDATE_CHANGE], 0, value, &lerror);
	if (lerror) {
		/* change refused by signal callback */
		g_propagate_error (error, lerror);
		return NULL;
	}

	/* new valid status */
	holder->priv->invalid_forced = FALSE;
	holder->priv->valid = newvalid;
	/* we're setting a static value, so be sure to flag is as unfreeable */
	holder->priv->is_freeable = FALSE;

	/* check is the new value is the default one */
	holder->priv->default_forced = FALSE;
	if (holder->priv->default_value) {
		if ((G_VALUE_TYPE (holder->priv->default_value) == GDA_TYPE_NULL) && newnull)
			holder->priv->default_forced = TRUE;
		else if ((G_VALUE_TYPE (holder->priv->default_value) == holder->priv->g_type) &&
			 value && (G_VALUE_TYPE (value) == holder->priv->g_type))
			holder->priv->default_forced = !gda_value_compare (holder->priv->default_value, value);
	}
	GValue *att_value;
	g_value_set_boolean ((att_value = gda_value_new (G_TYPE_BOOLEAN)), holder->priv->default_forced);
	gda_holder_set_attribute (holder, GDA_ATTRIBUTE_IS_DEFAULT, att_value);
	gda_value_free (att_value);

	/* real setting of the value */
	if (holder->priv->full_bind) {
#ifdef DEBUG_HOLDER
		g_print ("Holder %p is alias of holder %p => propagating changes to holder %p\n",
			 holder, holder->priv->full_bind, holder->priv->full_bind);
#endif
		return real_gda_holder_set_const_value (holder->priv->full_bind, value, 
												value_changed, error);
	}
	else {
		if (holder->priv->value) {
			value_to_return = holder->priv->value;
			holder->priv->value = NULL;
		}

		if (value) {
			if (newvalid) {
				holder->priv->value = (GValue*)value;
			}
		}

		g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	}

	return value_to_return;
}

/**
 * gda_holder_take_static_value
 * @holder: a #GdaHolder object
 * @value: a const value to set the holder to
 * @value_changed: a boolean set with TRUE if the value changes, FALSE elsewhere.
 * @error: a place to store errors, or %NULL
 *
 * Sets the const value within the holder. If @holder is an alias for another
 * holder, then the value is also set for that other holder.
 *
 * The value will not be freed, and user should take care of it, either for its
 * freeing or for its correct value at the moment of query.
 * 
 * If the value is not different from the one already contained within @holder,
 * then @holder is not chaged and no signal is emitted.
 *
 * Note1: if @holder can't accept the @value value, then this method returns NULL, and @holder will be left
 * in an invalid state.
 *
 * Note2: before the change is accepted by @holder, the "validate-change" signal will be emitted (the value
 * of which can prevent the change from happening) which can be connected to to have a greater control
 * of which values @holder can have, or implement some business rules.
 *
 * Returns: NULL if an error occurred or if the previous GValue was NULL itself. It returns
 * the static GValue user set previously, so that he can free it.
 *
 */
GValue *
gda_holder_take_static_value (GdaHolder *holder, const GValue *value, gboolean *value_changed,
							  GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	return real_gda_holder_set_const_value (holder, value, value_changed, error);
}

/**
 * gda_holder_force_invalid
 * @holder: a #GdaHolder object
 *
 * Forces a holder to be invalid; to set it valid again, a new value must be assigned
 * to it using gda_holder_set_value() or gda_holder_take_value().
 *
 * @holder's value is set to %NULL.
 */
void
gda_holder_force_invalid (GdaHolder *holder)
{
	g_return_if_fail (GDA_IS_HOLDER (holder));
	g_return_if_fail (holder->priv);

#ifdef GDA_DEBUG_NO
	g_print ("Holder %p (%s): declare invalid\n", holder, holder->priv->id);
#endif

	if (holder->priv->invalid_forced)
		return;

	holder->priv->invalid_forced = TRUE;
	holder->priv->valid = FALSE;
	
	if (holder->priv->value) {
		if (holder->priv->is_freeable)
			gda_value_free (holder->priv->value);
		holder->priv->value = NULL;
	}

	/* if we are an alias, then we forward the value setting to the master */
	if (holder->priv->full_bind) 
		gda_holder_force_invalid (holder->priv->full_bind);
	else 
		g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
}


/**
 * gda_holder_is_valid
 * @holder: a #GdaHolder object
 *
 * Get the validity of @holder (that is, of the value held by @holder)
 *
 * Returns: TRUE if @holder's value can safely be used
 */
gboolean
gda_holder_is_valid (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	if (holder->priv->full_bind)
		return gda_holder_is_valid (holder->priv->full_bind);
	else {
		if (holder->priv->invalid_forced) 
			return FALSE;

		if (holder->priv->default_forced) 
			return holder->priv->default_value ? TRUE : FALSE;
		else 
			return holder->priv->valid;
	}
}

/**
 * gda_holder_set_value_to_default
 * @holder: a #GdaHolder object
 *
 * Set @holder's value to its default value.
 *
 * Returns: TRUE if @holder has got a default value
 */
gboolean
gda_holder_set_value_to_default (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	if (holder->priv->default_forced)
		return TRUE;

	if (!holder->priv->default_value)
		return FALSE;
	else {
		holder->priv->default_forced = TRUE;
		holder->priv->invalid_forced = FALSE;
		if (holder->priv->value) {
			if (holder->priv->is_freeable)
				gda_value_free (holder->priv->value);
			holder->priv->value = NULL;
		}
	}

	GValue *att_value;
	g_value_set_boolean ((att_value = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
	gda_holder_set_attribute (holder, GDA_ATTRIBUTE_IS_DEFAULT, att_value);
	gda_value_free (att_value);
	g_signal_emit (holder, gda_holder_signals[CHANGED], 0);

	return TRUE;
}

/**
 * gda_holder_value_is_default
 * @holder: a #GdaHolder object
 *
 * Tells if @holder's current value is the default one.
 *
 * Returns: TRUE if @holder @holder's current value is the default one
 */
gboolean
gda_holder_value_is_default (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	return holder->priv->default_forced;
}


/**
 * gda_holder_get_default_value
 * @holder: a #GdaHolder object
 *
 * Get the default value held into the holder. WARNING: the default value does not need to be of 
 * the same type as the one required by @holder.
 *
 * Returns: the default value
 */
const GValue *
gda_holder_get_default_value (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (holder->priv, NULL);

	return holder->priv->default_value;
}


/**
 * gda_holder_set_default_value
 * @holder: a #GdaHolder object
 * @value: a value to set the holder's default value, or %NULL
 *
 * Sets the default value within the holder. If @value is %NULL then @holder won't have a
 * default value anymore. To set a default value to %NULL, then pass a #GValue created using
 * gda_value_new_null().
 *
 * NOTE: the default value does not need to be of the same type as the one required by @holder.
 */
void
gda_holder_set_default_value (GdaHolder *holder, const GValue *value)
{
	g_return_if_fail (GDA_IS_HOLDER (holder));
	g_return_if_fail (holder->priv);

	if (holder->priv->default_value) {
		if (holder->priv->default_forced) {
			gda_holder_take_value (holder, holder->priv->default_value, NULL);
			holder->priv->default_forced = FALSE;
			holder->priv->default_value = NULL;
		}
		else {
			gda_value_free (holder->priv->default_value);
			holder->priv->default_value = NULL;
		}
	}

	holder->priv->default_forced = FALSE;
	if (value) {
		const GValue *current = gda_holder_get_value (holder);

		/* check if default is equal to current value */
		if (gda_value_is_null (value) &&
		    (!current || gda_value_is_null (current)))
			holder->priv->default_forced = TRUE;
		else if ((G_VALUE_TYPE (value) == holder->priv->g_type) &&
			 current && !gda_value_compare (value, current))
			holder->priv->default_forced = TRUE;

		holder->priv->default_value = gda_value_copy ((GValue *)value);
	}
	
	GValue *att_value;
	g_value_set_boolean ((att_value = gda_value_new (G_TYPE_BOOLEAN)), holder->priv->default_forced);
	gda_holder_set_attribute (holder, GDA_ATTRIBUTE_IS_DEFAULT, att_value);
	gda_value_free (att_value);

	/* don't emit the "changed" signal */
}

/**
 * gda_holder_set_not_null
 * @holder: a #GdaHolder object
 * @not_null:
 *
 * Sets if the holder can have a NULL value. If @not_null is TRUE, then that won't be allowed
 */
void
gda_holder_set_not_null (GdaHolder *holder, gboolean not_null)
{
	g_return_if_fail (GDA_IS_HOLDER (holder));
	g_return_if_fail (holder->priv);

	g_object_set (G_OBJECT (holder), "not-null", not_null, NULL);
}

/**
 * gda_holder_get_not_null
 * @holder: a #GdaHolder object
 *
 * Get wether the holder can be NULL or not
 *
 * Returns: TRUE if the holder cannot be NULL
 */
gboolean
gda_holder_get_not_null (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	return holder->priv->not_null;
}

/**
 * gda_holder_set_source_model
 * @holder: a #GdaHolder object
 * @model: a #GdaDataModel object or NULL
 * @col: the reference column in @model
 * @error: location to store error, or %NULL
 *
 * Sets a limit on the possible values for the @holder holder: the value must be among the values
 * contained in the @col column of the @model data model.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_holder_set_source_model (GdaHolder *holder, GdaDataModel *model,
			     gint col, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);

	/* No check is done on the validity of @col or even its existance */
	/* Note: for internal implementation if @col<0, then it's ignored */

	if (holder->priv->source_model == model) {
		if (col >= 0)
			holder->priv->source_col = col;
	}
	else {
		if (holder->priv->source_model) {
			g_object_unref (holder->priv->source_model);
			holder->priv->source_model = NULL;
		}
		
		if (col >= 0)
			holder->priv->source_col = col;
		
		if (model) {
			holder->priv->source_model = model;
			g_object_ref (model);
		}
	}

#ifdef GDA_DEBUG_signal
        g_print (">> 'SOURCE_CHANGED' from %p\n", holder);
#endif
	g_signal_emit (holder, gda_holder_signals[SOURCE_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'SOURCE_CHANGED' from %p\n", holder);
#endif
	return TRUE;
}


/**
 * gda_holder_get_source_model
 * @holder: a #GdaHolder
 * @col: a place to store the column in the model sourceing the holder, or %NULL
 *
 * Tells if @holder has its values sourceed by a #GdaDataModel, and optionnaly
 * allows to fetch the resteictions.
 *
 * Returns: a pointer to the #GdaDataModel source for @holder
 */
GdaDataModel *
gda_holder_get_source_model (GdaHolder *holder, gint *col)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);
	
	if (col)
		*col = holder->priv->source_col;

	return holder->priv->source_model;
}

/**
 * gda_holder_set_bind
 * @holder: a #GdaHolder
 * @bind_to: a #GdaHolder or %NULL
 *
 * Sets @holder to change when @bind_to changes (and does not make @bind_to change when @holder changes).
 *
 * If @bind_to is %NULL, then @holder will not be bound anymore.
 */
gboolean
gda_holder_set_bind (GdaHolder *holder, GdaHolder *bind_to, GError **error)
{
	const GValue *cvalue;
	GValue *value1 = NULL;
	const GValue *value2 = NULL;

	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	g_return_val_if_fail (holder->priv, FALSE);
	g_return_val_if_fail (holder != bind_to, FALSE);

	if (holder->priv->simple_bind == bind_to)
		return TRUE;

	/* get a copy of the current values of @holder and @bind_to */
	if (bind_to) {
		g_return_val_if_fail (GDA_IS_HOLDER (bind_to), FALSE);
		g_return_val_if_fail (bind_to->priv, FALSE);
		if (holder->priv->g_type != bind_to->priv->g_type) {
			g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
				     _("Cannot bind holders if their type is not the same"));
			return FALSE;
		}
		value2 = gda_holder_get_value (bind_to);
	}

	cvalue = gda_holder_get_value (holder);
	if (cvalue)
		value1 = gda_value_copy ((GValue*)cvalue);

	/* get rid of the old alias */
	if (holder->priv->simple_bind) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder->priv->simple_bind),
						      G_CALLBACK (full_bind_changed_cb), holder);
		g_object_unref (holder->priv->simple_bind);
		holder->priv->simple_bind = NULL;
	}

	/* setting the new alias or reseting the value if there is no new alias */
	if (bind_to) {
		holder->priv->simple_bind = bind_to;
		g_object_ref (holder->priv->simple_bind);
		g_signal_connect (G_OBJECT (holder->priv->simple_bind), "changed",
				  G_CALLBACK (full_bind_changed_cb), holder);

		/* if bind_to has a different value than holder, then we set holder to the new value */
		if (value1)
			gda_value_free (value1);
		return gda_holder_set_value (holder, value2, error);
	}
	else
		return gda_holder_take_value (holder, value1, error);
}

/*
 * gda_holder_set_full_bind
 * @holder: a #GdaHolder
 * @alias_of: a #GdaHolder or %NULL
 *
 * Sets @holder to change when @alias_of changes and makes @alias_of change when @holder changes.
 * The difference with gda_holder_set_bind is that when @holder changes, then @alias_of also
 * changes.
 */
static void
gda_holder_set_full_bind (GdaHolder *holder, GdaHolder *alias_of)
{
	const GValue *cvalue;
	GValue *value1 = NULL, *value2 = NULL;

	g_return_if_fail (GDA_IS_HOLDER (holder));
	g_return_if_fail (holder->priv);

	if (holder->priv->full_bind == alias_of)
		return;

	/* get a copy of the current values of @holder and @alias_of */
	if (alias_of) {
		g_return_if_fail (GDA_IS_HOLDER (alias_of));
		g_return_if_fail (alias_of->priv);
		g_return_if_fail (holder->priv->g_type == alias_of->priv->g_type);
		cvalue = gda_holder_get_value (alias_of);
		if (cvalue && !gda_value_is_null ((GValue*)cvalue))
			value2 = gda_value_copy ((GValue*)cvalue);
	}

	cvalue = gda_holder_get_value (holder);
	if (cvalue && !gda_value_is_null ((GValue*)cvalue))
		value1 = gda_value_copy ((GValue*)cvalue);
		
	
	/* get rid of the old alias */
	if (holder->priv->full_bind) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (holder->priv->full_bind),
						      G_CALLBACK (full_bind_changed_cb), holder);
		g_object_unref (holder->priv->full_bind);
		holder->priv->full_bind = NULL;
	}

	/* setting the new alias or reseting the value if there is no new alias */
	if (alias_of) {
		gboolean equal = FALSE;

		/* get rid of the internal holder's value */
		if (holder->priv->value) {
			if (holder->priv->is_freeable)
				gda_value_free (holder->priv->value);
			holder->priv->value = NULL;
		}

		holder->priv->full_bind = alias_of;
		g_object_ref (alias_of);
		g_signal_connect (G_OBJECT (alias_of), "changed",
				  G_CALLBACK (full_bind_changed_cb), holder);

		/* if alias_of has a different value than holder, then we emit a CHANGED signal */
		if (value1 && value2 &&
		    (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2)))
			equal = !gda_value_compare (value1, value2);
		else {
			if (!value1 && !value2)
				equal = TRUE;
		}

		if (!equal)
			g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	}
	else {
		/* restore the value that was in the previous alias holder, 
		 * if there was such a value, and don't emit a signal */
		g_assert (! holder->priv->value);
		if (value1)
			holder->priv->value = value1;
		value1 = NULL;
	}

	if (value1) gda_value_free (value1);
	if (value2) gda_value_free (value2);
}

static void
full_bind_changed_cb (GdaHolder *alias_of, GdaHolder *holder)
{
	g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
}

/**
 * gda_holder_get_bind
 * @holder: a #GdaHolder
 *
 * Get the holder which makes @holder change its value when the holder's value is changed.
 *
 * Returns: the #GdaHolder or %NULL
 */
GdaHolder *
gda_holder_get_bind (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (holder->priv, NULL);

	return holder->priv->simple_bind;
}

/**
 * gda_holder_get_alphanum_id
 * @holder: a #GdaHolder object
 *
 * Get an "encoded" version of @holder's name. The "encoding" consists in replacing non
 * alphanumeric character with the string "__gdaXX" where XX is the hex. representation
 * of the non alphanumeric char.
 *
 * This method is just a wrapper around the gda_text_to_alphanum() function.
 *
 * Returns: a new string
 */
gchar *
gda_holder_get_alphanum_id (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	g_return_val_if_fail (holder->priv, NULL);
	return gda_text_to_alphanum (holder->priv->id);
}

/**
 * gda_holder_get_attribute
 * @holder: a #GdaHolder
 * @attribute: attribute name as a string
 *
 * Get the value associated to a named attribute.
 *
 * Attributes can have any name, but Libgda proposes some default names, see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * Returns: a read-only #GValue, or %NULL if not attribute named @attribute has been set for @holder
 */
const GValue *
gda_holder_get_attribute (GdaHolder *holder, const gchar *attribute)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	return gda_attributes_manager_get (gda_holder_attributes_manager, holder, attribute);
}

/**
 * gda_holder_set_attribute
 * @holder: a #GdaHolder
 * @attribute: attribute name as a static string
 * @value: a #GValue, or %NULL
 *
 * Set the value associated to a named attribute.
 *
 * Attributes can have any name, but Libgda proposes some default names, see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 * If there is already an attribute named @attribute set, then its value is replaced with the new @value, 
 * except if @value is %NULL, in which case the attribute is removed.
 *
 * Warning: @sttribute should be a static string (no copy of it is made), so the string should exist as long as the @holder
 * object exists.
 */
void
gda_holder_set_attribute (GdaHolder *holder, const gchar *attribute, const GValue *value)
{
	const GValue *cvalue;
	g_return_if_fail (GDA_IS_HOLDER (holder));

	cvalue = gda_attributes_manager_get (gda_holder_attributes_manager, holder, attribute);
	if (cvalue && !gda_value_differ (cvalue, value))
		return;

	gda_attributes_manager_set (gda_holder_attributes_manager, holder, attribute, value);
	g_signal_emit (holder, gda_holder_signals[ATT_CHANGED], 0, attribute, value);
}
