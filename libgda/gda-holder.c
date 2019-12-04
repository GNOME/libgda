/*
 * Copyright (C) 2008 Massimo Cora <maxcvs@email.it>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 Daniel Espinosa <despinosa@src.gnome.org>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-holder"

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
#include <libgda/gda-custom-marshal.h>

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

/* GdaLockable interface */
static void                 gda_holder_lockable_init (GdaLockableInterface *iface);
static void                 gda_holder_lock      (GdaLockable *lockable);
static gboolean             gda_holder_trylock   (GdaLockable *lockable);
static void                 gda_holder_unlock    (GdaLockable *lockable);


static void bound_holder_changed_cb (GdaHolder *alias_of, GdaHolder *holder);
static void full_bound_holder_changed_cb (GdaHolder *alias_of, GdaHolder *holder);
static void gda_holder_set_full_bind (GdaHolder *holder, GdaHolder *alias_of);

/* signals */
enum
{
	CHANGED,
	SOURCE_CHANGED,
	VALIDATE_CHANGE,
	TO_DEFAULT,
	LAST_SIGNAL
};

static gint gda_holder_signals[LAST_SIGNAL] = { 0, 0, 0 };


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
	PROP_NOT_NULL,
	PROP_VALIDATE_CHANGES,
	PROP_PLUGIN
};


typedef struct {
	gchar           *id;

	GType            g_type;
	GdaHolder       *full_bind;     /* FULL bind to holder */
	GdaHolder       *simple_bind;  /* SIMPLE bind to holder */
	gulong           simple_bind_type_changed_id;
	
	gboolean         invalid_forced;
	GError          *invalid_error;
	gboolean         valid;
	gboolean         is_freeable;

	GValue           *value;
	GValue          *default_value; /* CAN be either NULL or of any type */
	gboolean         default_forced;
	gboolean         not_null;      /* TRUE if 'value' must not be NULL when passed to destination fields */

	GdaDataModel    *source_model;
	gint             source_col;

	GRecMutex        mutex;

	gboolean         validate_changes;
	gchar           *name;
	gchar           *desc;
	gchar           *plugin;
} GdaHolderPrivate;
G_DEFINE_TYPE_WITH_CODE (GdaHolder, gda_holder, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaHolder)
                         G_IMPLEMENT_INTERFACE(GDA_TYPE_LOCKABLE, gda_holder_lockable_init))
/* module error */
GQuark gda_holder_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_holder_error");
        return quark;
}

static gboolean
validate_change_accumulator (G_GNUC_UNUSED GSignalInvocationHint *ihint,
			   GValue *return_accu,
			   const GValue *handler_return,
			   G_GNUC_UNUSED gpointer data)
{
	GError *error;

	error = g_value_get_boxed (handler_return);
	g_value_set_boxed (return_accu, error);

	return error ? FALSE : TRUE; /* stop signal if error has been set */
}

static GError *
m_validate_change (G_GNUC_UNUSED GdaHolder *holder, G_GNUC_UNUSED const GValue *new_value)
{
	return NULL;
}

static void
gda_holder_class_init (GdaHolderClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	/**
	 * GdaHolder::source-changed:
	 * @holder: the #GdaHolder
	 * 
	 * Gets emitted when the data model in which @holder's values should be has changed
	 */
	gda_holder_signals[SOURCE_CHANGED] =
                g_signal_new ("source-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaHolderClass, source_changed),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID, G_TYPE_NONE, 0);
	/**
	 * GdaHolder::changed:
	 * @holder: the #GdaHolder
	 * 
	 * Gets emitted when @holder's value has changed
	 */
	gda_holder_signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaHolderClass, changed),
                              NULL, NULL,
                              _gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	/**
	 * GdaHolder::validate-change:
	 * @holder: the object which received the signal
	 * @new_value: the proposed new value for @holder
	 * 
	 * Gets emitted when @holder is going to change its value. One can connect to
	 * this signal to control which values @holder can have (for example to implement some business rules)
	 *
	 * Returns: NULL if @holder is allowed to change its value to @new_value, or a #GError
	 * otherwise.
	 */
	gda_holder_signals[VALIDATE_CHANGE] =
		g_signal_new ("validate-change",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaHolderClass, validate_change),
                              validate_change_accumulator, NULL,
                              _gda_marshal_ERROR__VALUE, G_TYPE_ERROR, 1, G_TYPE_VALUE);
	/**
	 * GdaHolder::to-default:
	 * @holder: the object which received the signal
	 *
	 * Gets emitted when @holder is set to its default value
	 */
	gda_holder_signals[TO_DEFAULT] =
                g_signal_new ("to-default",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaHolderClass, to_default),
                              NULL, NULL,
                              NULL, G_TYPE_NONE, 0);

        class->changed = NULL;
        class->source_changed = NULL;
        class->validate_change = m_validate_change;
	class->to_default = NULL;

	/* virtual functions */
	object_class->dispose = gda_holder_dispose;
	object_class->finalize = gda_holder_finalize;

	/* Properties */
	object_class->set_property = gda_holder_set_property;
	object_class->get_property = gda_holder_get_property;
	g_object_class_install_property (object_class, PROP_ID,
					 g_param_spec_string ("id", NULL, "Holder's ID", NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL, "Holder's name", NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DESCR,
					 g_param_spec_string ("description", NULL, "Holder's description", NULL, 
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_GDA_TYPE,
					 g_param_spec_gtype ("g-type", NULL, "Holder's GType", G_TYPE_NONE, 
							     (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));
	g_object_class_install_property (object_class, PROP_NOT_NULL,
					 g_param_spec_boolean ("not-null", NULL, "Can the value holder be NULL?", FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SIMPLE_BIND,
					 g_param_spec_object ("simple-bind", NULL, 
							      "Make value holder follow other GdaHolder's changes", 
                                                               GDA_TYPE_HOLDER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_FULL_BIND,
					 g_param_spec_object ("full-bind", NULL,
							      "Make value holder follow other GdaHolder's changes "
							      "and the other way around", 
                                                               GDA_TYPE_HOLDER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SOURCE_MODEL,
                                         g_param_spec_object ("source-model", NULL, "Data model among which the holder's "
							      "value should be",
							      GDA_TYPE_DATA_MODEL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_SOURCE_COLUMN,
                                         g_param_spec_int ("source-column", NULL, "Column number to use in coordination "
							   "with the source-model property",
							   0, G_MAXINT, 0,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_PLUGIN,
					 g_param_spec_string ("plugin", NULL, "Holder's plugin", NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	/**
	 * GdaHolder:validate-changes:
	 *
	 * Defines if the "validate-change" signal gets emitted when
	 * the holder's value changes.
	 *
	 * Since: 5.2.0
	 */
	g_object_class_install_property (object_class, PROP_VALIDATE_CHANGES,
					 g_param_spec_boolean ("validate-changes", NULL, "Defines if the validate-change signal is emitted on value change", TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gda_holder_lockable_init (GdaLockableInterface *iface)
{
	iface->lock = gda_holder_lock;
	iface->trylock = gda_holder_trylock;
	iface->unlock = gda_holder_unlock;
}

static void
gda_holder_init (GdaHolder *holder)
{
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	priv->id = g_strdup ("id");

	priv->g_type = GDA_TYPE_NULL;
	priv->full_bind = NULL;
	priv->simple_bind = NULL;
	priv->simple_bind_type_changed_id = 0;

	priv->invalid_forced = FALSE;
	priv->invalid_error = NULL;
	priv->valid = TRUE;
	priv->default_forced = FALSE;
	priv->is_freeable = TRUE;
	priv->value = NULL;
	priv->default_value = NULL;

	priv->not_null = FALSE;
	priv->source_model = NULL;
	priv->source_col = 0;

	g_rec_mutex_init (& (priv->mutex));

	priv->validate_changes = TRUE;
	priv->name = NULL;
	priv->desc = NULL;
	priv->plugin = NULL;
}

/**
 * gda_holder_new:
 * @type: the #GType requested
 * @id: an identifiation
 *
 * Creates a new holder of type @type
 *
 * Returns: a new #GdaHolder object
 */
GdaHolder *
gda_holder_new (GType type, const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);
	return (GdaHolder*) g_object_new (GDA_TYPE_HOLDER, "g-type", type, "id", id, NULL);
}

/**
 * gda_holder_copy:
 * @orig: a #GdaHolder object to copy
 *
 * Copy constructor.
 * 
 * Note1: if @orig is set with a static value (see gda_holder_take_static_value()) 
 * its copy will have a fresh new allocated GValue, so that user should free it when done.
 *
 * Returns: (transfer full): a new #GdaHolder object
 */
GdaHolder *
gda_holder_copy (GdaHolder *orig)
{
	GObject *obj;
	GdaHolder *holder;
	gboolean allok = TRUE;

	g_return_val_if_fail (orig && GDA_IS_HOLDER (orig), NULL);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (orig);

	gda_holder_lock ((GdaLockable*) orig);
	obj = g_object_new (GDA_TYPE_HOLDER, "g-type", priv->g_type, NULL);
	holder = GDA_HOLDER (obj);
	GdaHolderPrivate *cpriv = gda_holder_get_instance_private (holder);

	if (priv->id) {
		g_free (cpriv->id);
    cpriv->id = g_strdup (priv->id);
  }
	if (priv->name)
		cpriv->name = g_strdup (priv->name);
	if (priv->desc)
		cpriv->desc = g_strdup (priv->desc);
	if (priv->plugin)
		cpriv->plugin = g_strdup (priv->plugin);

	if (priv->full_bind)
		gda_holder_set_full_bind (holder, priv->full_bind);
	if (priv->simple_bind)
		allok = gda_holder_set_bind (holder, priv->simple_bind, NULL);
	
	if (allok && priv->source_model) {
		/*g_print ("Source holder %p\n", holder);*/
		allok = gda_holder_set_source_model (holder, priv->source_model, priv->source_col,
						     NULL);
	}

	if (allok) {
		/* direct settings */
		cpriv->invalid_forced = priv->invalid_forced;
		if (priv->invalid_error)
			cpriv->invalid_error = g_error_copy (priv->invalid_error);
		cpriv->valid = priv->valid;
		cpriv->is_freeable = TRUE;
		cpriv->default_forced = priv->default_forced;
		if (priv->value)
			cpriv->value = gda_value_copy (priv->value);
		if (priv->default_value)
			cpriv->default_value = gda_value_copy (priv->default_value);
		cpriv->not_null = priv->not_null;

		gda_holder_unlock ((GdaLockable*) orig);
		return holder;
	}
	else {
		g_warning ("Internal error: could not copy GdaHolder (please report a bug).");
		g_object_unref (holder);
		gda_holder_unlock ((GdaLockable*) orig);
		return NULL;
	}
}

/**
 * gda_holder_new_inline:
 * @type: a valid GLib type
 * @id: (nullable): the id of the holder to create, or %NULL
 * @...: value to set
 *
 * Creates a new #GdaHolder object with an ID set to @id, of type @type, 
 * and containing the value passed as the last argument.
 *
 * Note that this function is a utility function and that only a limited set of types are supported. Trying
 * to use an unsupported type will result in a warning, and the returned value holder holding a safe default
 * value.
 *
 * Returns: a new #GdaHolder object
 */
GdaHolder *
gda_holder_new_inline (GType type, const gchar *id, ...)
{
	GdaHolder *holder;

	static guint serial = 0;
	const gchar *idm = NULL;

	g_print ("Creating inline: %s", id);

	if (id != NULL) {
		idm = id;
	} else {
		idm = g_strdup_printf ("%d", serial++);
	}

	holder = gda_holder_new (type, idm);
	if (holder) {
		GValue *value;
		va_list ap;
		GError *lerror = NULL;

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
			g_value_set_schar (value, va_arg (ap, int));
		else if (type == G_TYPE_UCHAR)
			g_value_set_uchar (value, va_arg (ap, guint));
		else if (type == G_TYPE_FLOAT)
			g_value_set_float (value, va_arg (ap, double));
		else if (type == G_TYPE_DOUBLE)
			g_value_set_double (value, va_arg (ap, gdouble));
		else if (type == G_TYPE_GTYPE)
			g_value_set_gtype (value, va_arg (ap, GType));
		else if (type == G_TYPE_LONG)
			g_value_set_long (value, va_arg (ap, glong));
		else if (type == G_TYPE_ULONG)
			g_value_set_ulong (value, va_arg (ap, gulong));
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	gda_holder_set_bind (holder, NULL, NULL);
	gda_holder_set_full_bind (holder, NULL);

  if (priv->id != NULL) {
    g_free (priv->id);
    priv->id = NULL;
  }

	if (priv->source_model) {
		g_object_unref (priv->source_model);
		priv->source_model = NULL;
	}

	priv->g_type = G_TYPE_INVALID;

	if (priv->value) {
		if (priv->is_freeable)
			gda_value_free (priv->value);
		priv->value = NULL;
	}

	if (priv->default_value) {
		gda_value_free (priv->default_value);
		priv->default_value = NULL;
	}

	if (priv->invalid_error) {
		g_error_free (priv->invalid_error);
		priv->invalid_error = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gda_holder_parent_class)->dispose (object);
}

static void
gda_holder_finalize (GObject   * object)
{
	GdaHolder *holder;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HOLDER (object));

	holder = GDA_HOLDER (object);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	if (priv->id != NULL) {
		g_free (priv->id);
		priv->id = NULL;
	}
	if (priv->name != NULL) {
		g_free (priv->name);
		priv->name = NULL;
	}
	if (priv->desc != NULL) {
		g_free (priv->desc);
		priv->desc = NULL;
	}
	if (priv->plugin != NULL) {
		g_free (priv->plugin);
		priv->plugin = NULL;
	}
	g_rec_mutex_clear (& (priv->mutex));

	/* parent class */
	G_OBJECT_CLASS (gda_holder_parent_class)->finalize (object);
}


static void 
gda_holder_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdaHolder *holder;

	holder = GDA_HOLDER (object);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	if (priv) {
		switch (param_id) {
		case PROP_ID:
			if (priv->id != NULL) {
				g_free (priv->id);
				priv->id = NULL;
			}
			priv->id = g_value_dup_string (value);
			break;
		case PROP_NAME:
			if (priv->name != NULL) {
				g_free (priv->name);
				priv->name = NULL;
			}
			priv->name = g_value_dup_string (value);
      g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
			break;
		case PROP_DESCR:
			if (priv->desc != NULL) {
				g_free (priv->desc);
				priv->desc = NULL;
			}
			priv->desc = g_value_dup_string (value);
      g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
			break;
		case PROP_PLUGIN:
			if (priv->plugin != NULL) {
				g_free (priv->plugin);
				priv->plugin = NULL;
			}
			priv->plugin = g_value_dup_string (value);
      g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
			break;
		case PROP_GDA_TYPE:
			if (priv->g_type == GDA_TYPE_NULL) {
				priv->g_type = g_value_get_gtype (value);
				g_object_notify ((GObject*) holder, "g-type");
			}
			else
				g_warning (_("The 'g-type' property cannot be changed"));
			break;
		case PROP_NOT_NULL: {
			gboolean not_null = g_value_get_boolean (value);
			if (not_null != priv->not_null) {
				priv->not_null = not_null;
				
				/* updating the holder's validity regarding the NULL value */
				if (!not_null && 
				    (!priv->value || GDA_VALUE_HOLDS_NULL (priv->value)))
					priv->valid = TRUE;
				
				if (not_null && 
				    (!priv->value || GDA_VALUE_HOLDS_NULL (priv->value)))
					priv->valid = FALSE;
				
				g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
			}
			break;
		}
		case PROP_SIMPLE_BIND:
			if (!gda_holder_set_bind (holder, (GdaHolder*) g_value_get_object (value), NULL))
				g_warning ("Could not set the 'simple-bind' property");
			break;
		case PROP_FULL_BIND:
			gda_holder_set_full_bind (holder, (GdaHolder*) g_value_get_object (value));
			break;
		case PROP_SOURCE_MODEL: {
			GdaDataModel* ptr = g_value_get_object (value);
			g_return_if_fail (gda_holder_set_source_model (holder, 
								       (GdaDataModel *)ptr, -1, NULL));
			break;
                }
		case PROP_SOURCE_COLUMN:
			priv->source_col = g_value_get_int (value);
			break;
		case PROP_VALIDATE_CHANGES:
			priv->validate_changes = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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

	holder = GDA_HOLDER (object);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	if (priv) {
		switch (param_id) {
		case PROP_ID:
			g_value_set_string (value, priv->id);
			break;
		case PROP_NAME:
			if (priv->name != NULL)
				g_value_set_string (value, priv->name);
			else
				g_value_set_string (value, priv->id);
			break;
		case PROP_DESCR:
			if (priv->desc != NULL)
				g_value_set_string (value, priv->desc);
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_PLUGIN:
			if (priv->plugin != NULL)
				g_value_set_string (value, priv->desc);
			else
				g_value_set_string (value, NULL);
			break;
		case PROP_GDA_TYPE:
			g_value_set_gtype (value, priv->g_type);
			break;
		case PROP_NOT_NULL:
			g_value_set_boolean (value, gda_holder_get_not_null (holder));
			break;
		case PROP_SIMPLE_BIND:
			g_value_set_object (value, (GObject*) priv->simple_bind);
			break;
		case PROP_FULL_BIND:
			g_value_set_object (value, (GObject*) priv->full_bind);
			break;
		case PROP_SOURCE_MODEL:
			g_value_set_object (value, (GObject*) priv->source_model);
			break;
		case PROP_SOURCE_COLUMN:
			g_value_set_int (value, priv->source_col);
			break;
		case PROP_VALIDATE_CHANGES:
			g_value_set_boolean (value, priv->validate_changes);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_holder_get_g_type:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	return priv->g_type;
}

/**
 * gda_holder_get_id:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	return priv->id;
}


/**
 * gda_holder_get_value:
 * @holder: a #GdaHolder object
 *
 * Get the value held into the holder. If @holder is set to use its default value
 * and that default value is not of the same type as @holder, then %NULL is returned.
 *
 * If @holder is set to NULL, then the returned value is a #GDA_TYPE_NULL GValue.
 *
 * If @holder is invalid, then the returned value is %NULL.
 *
 * Returns: (nullable) (transfer none): the value, or %NULL
 */
const GValue *
gda_holder_get_value (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	if (priv->full_bind)
		return gda_holder_get_value (priv->full_bind);
	else {
		if (priv->valid) {
			/* return default value if possible */
			if (priv->default_forced) {
				g_assert (priv->default_value);
				if (G_VALUE_TYPE (priv->default_value) == priv->g_type)
					return priv->default_value;
				else
					return NULL;
			}
			
			if (!priv->value)
				priv->value = gda_value_new_null ();
			return priv->value;
		}
		else
			return NULL;
	}
}

/**
 * gda_holder_get_value_str:
 * @holder: a #GdaHolder object
 * @dh: (nullable): a #GdaDataHandler to use, or %NULL
 *
 * Same functionality as gda_holder_get_value() except that it returns the value as a string
 * (the conversion is done using @dh if not %NULL, or the default data handler otherwise).
 *
 * Returns: (transfer full): the value, or %NULL
 */
gchar *
gda_holder_get_value_str (GdaHolder *holder, GdaDataHandler *dh)
{
	const GValue *current_val;
	GdaDataHandler *dhl = NULL;

	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	gda_holder_lock ((GdaLockable*) holder);
	current_val = gda_holder_get_value (holder);
        if (!current_val || GDA_VALUE_HOLDS_NULL (current_val)) {
		gda_holder_unlock ((GdaLockable*) holder);
                return NULL;
	}
        else {
		gchar *retval = NULL;
                if (!dh)
			dhl = gda_data_handler_get_default (priv->g_type);
		else
			dhl = g_object_ref (dh);
		if (dhl) {
                        retval = gda_data_handler_get_str_from_value (dhl, current_val);
			g_object_unref (dhl);
		}
		gda_holder_unlock ((GdaLockable*) holder);
		return retval;
        }
}

static gboolean real_gda_holder_set_value (GdaHolder *holder, GValue *value, gboolean do_copy, GError **error);

/**
 * gda_holder_set_value:
 * @holder: a #GdaHolder object
 * @value: (nullable): a value to set the holder to, or %NULL
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

	return real_gda_holder_set_value (holder, (GValue*) value, TRUE, error);
}

/**
 * gda_holder_set_value_str:
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
	g_return_val_if_fail (!dh || GDA_IS_DATA_HANDLER (dh), FALSE);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	if (!value || !g_ascii_strcasecmp (value, "NULL")) 
                return gda_holder_set_value (holder, NULL, error);
	else {
		GValue *gdaval = NULL;
		gboolean retval = FALSE;
		GdaDataHandler *ldh = NULL;

		gda_holder_lock ((GdaLockable*) holder);
		if (!dh)
			ldh = gda_data_handler_get_default (priv->g_type);
		else
			ldh = g_object_ref (dh);

		if (ldh) {
			gdaval = gda_data_handler_get_value_from_str (ldh, value, priv->g_type);
			g_object_unref (ldh);
		}
		
		if (gdaval)
			retval = real_gda_holder_set_value (holder, gdaval, FALSE, error);
		else
			g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_STRING_CONVERSION_ERROR,
				     _("Unable to convert string to '%s' type"), 
				     gda_g_type_to_string (priv->g_type));
		gda_holder_unlock ((GdaLockable*) holder);
		return retval;
	}
}

/**
 * gda_holder_take_value:
 * @holder: a #GdaHolder object
 * @value: (transfer full): a value to set the holder to
 * @error: a place to store errors, or %NULL
 *
 * Sets the value within the holder. If @holder is an alias for another
 * holder, then the value is also set for that other holder.
 *
 * On success, the action of any call to gda_holder_force_invalid() is cancelled
 * as soon as this method is called (even if @holder's value does not actually change).
 * 
 * If the value is not different from the one already contained within @holder,
 * then @holder is not changed and no signal is emitted.
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
 * Note4: in any case, the caller should not use @value anymore after this function returns because it may
 * have been freed. If necessary, use gda_holder_get_value() to get the real value.
 *
 * Returns: TRUE if value has been set
 */
gboolean
gda_holder_take_value (GdaHolder *holder, GValue *value, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);

	return real_gda_holder_set_value (holder, (GValue*) value, FALSE, error);
}

static gboolean
real_gda_holder_set_value (GdaHolder *holder, GValue *value, gboolean do_copy, GError **error)
{
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	gboolean changed = TRUE;
	gboolean newvalid;
	const GValue *current_val;
	gboolean newnull;
	gboolean was_valid;
	GValue *new_value = NULL;
#define DEBUG_HOLDER
#undef DEBUG_HOLDER

	gda_holder_lock ((GdaLockable*) holder);
	was_valid = gda_holder_is_valid (holder);

	/* if the value has been set with gda_holder_take_static_value () you'll be able
	 * to change the value only with another call to real_gda_holder_set_value 
	 */
	if (!priv->is_freeable) {
		gda_holder_unlock ((GdaLockable*) holder);
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_CHANGE_ERROR,
                 (_("Can't use this method to set value because there is already a static value")));
		return FALSE;
	}
	new_value = value;
	// If value's type is compatible to current holder's value's type transform it
	if (value && priv->g_type != G_VALUE_TYPE (value)
	   && g_value_type_transformable (G_VALUE_TYPE (value), priv->g_type))
	{
		new_value = gda_value_new (priv->g_type);
		g_value_transform (value, new_value);
	}
  else {
    if (do_copy && value != NULL) {
      new_value = gda_value_copy (value);
    }
	}
	/* holder will be changed? */
	newnull = !new_value || GDA_VALUE_HOLDS_NULL (new_value);
	current_val = gda_holder_get_value (holder);
	if (current_val == new_value)
		changed = FALSE;
	else if ((!current_val || GDA_VALUE_HOLDS_NULL ((GValue *)current_val)) && newnull)
		changed = FALSE;
	else if (new_value && current_val &&
		 (G_VALUE_TYPE (new_value) == G_VALUE_TYPE ((GValue *)current_val)))
		changed = gda_value_differ (new_value, (GValue *)current_val);
		
	/* holder's validity */
	newvalid = TRUE;
	if (newnull && priv->not_null) {
	  g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_NULL_ERROR,
			     _("(%s): Holder does not allow NULL values"),
			     priv->id);
		newvalid = FALSE;
		changed = TRUE;
	}
	else if (!newnull && (G_VALUE_TYPE (new_value) != priv->g_type)) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
			     _("(%s): Wrong Holder value type, expected type '%s' when value's type is '%s'"),
			     priv->id,
			     gda_g_type_to_string (priv->g_type),
			     gda_g_type_to_string (G_VALUE_TYPE (new_value)));
		newvalid = FALSE;
		changed = TRUE;
	}

	if (was_valid != newvalid)
		changed = TRUE;

#ifdef DEBUG_HOLDER
	g_print ("Holder to change %p (%s): value %s --> %s \t(type %d -> %d) VALID: %d->%d CHANGED: %d\n", 
		 holder, priv->id,
		 gda_value_stringify ((GValue *)current_val),
		 gda_value_stringify ((new_value)),
		 current_val ? G_VALUE_TYPE ((GValue *)current_val) : 0,
		 new_value ? G_VALUE_TYPE (new_value) : 0,
		 was_valid, newvalid, changed);
#endif

	/* end of procedure if the value has not been changed, after calculating the holder's validity */
	if (!changed) {
		if (new_value)
			gda_value_free (new_value);
		priv->invalid_forced = FALSE;
		if (priv->invalid_error) {
			g_error_free (priv->invalid_error);
			priv->invalid_error = NULL;
		}
		priv->valid = newvalid;
		gda_holder_unlock ((GdaLockable*) holder);
		return TRUE;
	}

	/* check if we are allowed to change value */
	if (priv->validate_changes) {
		GError *lerror = NULL;
		g_signal_emit (holder, gda_holder_signals[VALIDATE_CHANGE], 0, new_value, &lerror);
		if (lerror) {
			/* change refused by signal callback */
#ifdef DEBUG_HOLDER
			g_print ("Holder change refused %p (ERROR %s)\n", holder,
				 lerror->message);
#endif
			g_propagate_error (error, lerror);
			if (new_value)
				gda_value_free (new_value);
			gda_holder_unlock ((GdaLockable*) holder);
			return FALSE;
		}
	}

	/* new valid status */
	priv->invalid_forced = FALSE;
	if (priv->invalid_error) {
		g_error_free (priv->invalid_error);
		priv->invalid_error = NULL;
	}
	priv->valid = newvalid;
	/* we're setting a non-static value, so be sure to flag is as freeable */
	priv->is_freeable = TRUE;

	/* check is the new value is the default one */
	priv->default_forced = FALSE;
	if (priv->default_value) {
		if ((G_VALUE_TYPE (priv->default_value) == GDA_TYPE_NULL) && newnull)
			priv->default_forced = TRUE;
		else if ((G_VALUE_TYPE (priv->default_value) == priv->g_type) &&
			 new_value && (G_VALUE_TYPE (new_value) == priv->g_type))
			priv->default_forced = !gda_value_compare (priv->default_value, new_value);
	}

	/* real setting of the value */
	if (priv->full_bind) {
#ifdef DEBUG_HOLDER
		g_print ("Holder %p is alias of holder %p => propagating changes to holder %p\n",
			 holder, priv->full_bind, priv->full_bind);
#endif
		gda_holder_unlock ((GdaLockable*) holder);
    gboolean ret = real_gda_holder_set_value (priv->full_bind, new_value, do_copy, error);
    if (new_value)
      gda_value_free (new_value);
		return ret;
	}
	else {
		if (priv->value) {
			gda_value_free (priv->value);
			priv->value = NULL;
		}

		if (new_value) {
			if (newvalid) {
				priv->value = gda_value_copy (new_value);
			}
			gda_value_free (new_value);
		}

		gda_holder_unlock ((GdaLockable*) holder);
		g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	}

	return newvalid;
}

static GValue *
real_gda_holder_set_const_value (GdaHolder *holder, const GValue *value, 
								 gboolean *value_changed, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);

	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
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
	newnull = !value || GDA_VALUE_HOLDS_NULL (value);
	current_val = gda_holder_get_value (holder);
	if (current_val == value)
		changed = FALSE;
	else if ((!current_val || GDA_VALUE_HOLDS_NULL (current_val)) && newnull) 
		changed = FALSE;
	else if (value && current_val &&
		 (G_VALUE_TYPE (value) == G_VALUE_TYPE (current_val))) 
		changed = gda_value_differ (value, current_val);
		
	/* holder's validity */
	newvalid = TRUE;
	if (newnull && priv->not_null) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_NULL_ERROR,
			     _("(%s): Holder does not allow NULL values"),
			     priv->id);
		newvalid = FALSE;
		changed = TRUE;
	}
	else if (!newnull && (G_VALUE_TYPE (value) != priv->g_type)) {
		g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
			     _("(%s): Wrong value type: expected type '%s' when value's type is '%s'"),
			     priv->id,
			     gda_g_type_to_string (priv->g_type),
			     gda_g_type_to_string (G_VALUE_TYPE (value)));
		newvalid = FALSE;
		changed = TRUE;
	}

#ifdef DEBUG_HOLDER
	g_print ("Changed holder %p (%s): value %s --> %s \t(type %d -> %d) VALID: %d->%d CHANGED: %d\n", 
		 holder, priv->id,
		 current_val ? gda_value_stringify ((GValue *)current_val) : "_NULL_",
		 value ? gda_value_stringify ((value)) : "_NULL_",
		 current_val ? G_VALUE_TYPE ((GValue *)current_val) : 0,
		 value ? G_VALUE_TYPE (value) : 0, 
		 was_valid, newvalid, changed);
#endif
	

	/* end of procedure if the value has not been changed, after calculating the holder's validity */
	if (!changed) {
		priv->invalid_forced = FALSE;
		if (priv->invalid_error) {
			g_error_free (priv->invalid_error);
			priv->invalid_error = NULL;
		}
		priv->valid = newvalid;
#ifdef DEBUG_HOLDER		
		g_print ("Holder is not changed");
#endif		
		/* set the changed status */
		*value_changed = FALSE;
	}
	else {
		*value_changed = TRUE;
	}

	/* check if we are allowed to change value */
	if (priv->validate_changes) {
		GError *lerror = NULL;
		g_signal_emit (holder, gda_holder_signals[VALIDATE_CHANGE], 0, value, &lerror);
		if (lerror) {
			/* change refused by signal callback */
			g_propagate_error (error, lerror);
			return NULL;
		}
	}

	/* new valid status */
	priv->invalid_forced = FALSE;
	if (priv->invalid_error) {
		g_error_free (priv->invalid_error);
		priv->invalid_error = NULL;
	}
	priv->valid = newvalid;
	/* we're setting a static value, so be sure to flag is as unfreeable */
	priv->is_freeable = FALSE;

	/* check is the new value is the default one */
	priv->default_forced = FALSE;
	if (priv->default_value) {
		if ((G_VALUE_TYPE (priv->default_value) == GDA_TYPE_NULL) && newnull)
			priv->default_forced = TRUE;
		else if ((G_VALUE_TYPE (priv->default_value) == priv->g_type) &&
			 value && (G_VALUE_TYPE (value) == priv->g_type))
			priv->default_forced = !gda_value_compare (priv->default_value, value);
	}

	/* real setting of the value */
	if (priv->full_bind) {
#ifdef DEBUG_HOLDER
		g_print ("Holder %p is alias of holder %p => propagating changes to holder %p\n",
			 holder, priv->full_bind, priv->full_bind);
#endif
		return real_gda_holder_set_const_value (priv->full_bind, value,
												value_changed, error);
	}
	else {
		if (priv->value) {
			if (G_IS_VALUE (priv->value))
				value_to_return = priv->value;
			else
				value_to_return = NULL;
			priv->value = NULL;
		}

		if (value) {
			if (newvalid) {
				priv->value = (GValue*)value;
			}
		}

		g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	}

#ifdef DEBUG_HOLDER	
	g_print ("returning %p, wannabe was %p\n", value_to_return,
			 value);
#endif	
	
	return value_to_return;
}

/**
 * gda_holder_take_static_value:
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
 * then @holder is not changed and no signal is emitted.
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
 */
GValue *
gda_holder_take_static_value (GdaHolder *holder, const GValue *value, gboolean *value_changed,
			      GError **error)
{
	GValue *retvalue;
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);

	gda_holder_lock ((GdaLockable*) holder);
	retvalue = real_gda_holder_set_const_value (holder, value, value_changed, error);
	gda_holder_unlock ((GdaLockable*) holder);

	return retvalue;
}

/**
 * gda_holder_force_invalid:
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
	gda_holder_force_invalid_e (holder, NULL);
}

/**
 * gda_holder_force_invalid_e:
 * @holder: a #GdaHolder object
 * @error: (nullable) (transfer full): a #GError explaining why @holder is declared invalid, or %NULL
 *
 * Forces a holder to be invalid; to set it valid again, a new value must be assigned
 * to it using gda_holder_set_value() or gda_holder_take_value().
 *
 * @holder's value is set to %NULL.
 *
 * Since: 4.2.10
 */
void
gda_holder_force_invalid_e (GdaHolder *holder, GError *error)
{
	g_return_if_fail (GDA_IS_HOLDER (holder));
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

#ifdef GDA_DEBUG_NO
	g_print ("Holder %p (%s): declare invalid\n", holder, priv->id);
#endif

	gda_holder_lock ((GdaLockable*) holder);
	if (priv->invalid_error)
		g_error_free (priv->invalid_error);
	priv->invalid_error = error;

	if (priv->invalid_forced) {
		gda_holder_unlock ((GdaLockable*) holder);
		return;
	}

	priv->invalid_forced = TRUE;
	priv->valid = FALSE;
	if (priv->value) {
		if (priv->is_freeable)
			gda_value_free (priv->value);
		priv->value = NULL;
	}

	/* if we are an alias, then we forward the value setting to the master */
	if (priv->full_bind) {
		gda_holder_force_invalid (priv->full_bind);
		gda_holder_unlock ((GdaLockable*) holder);
	}
	else {
		gda_holder_unlock ((GdaLockable*) holder);
		g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	}
}

/**
 * gda_holder_is_valid:
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
	return gda_holder_is_valid_e (holder, NULL);
}

/**
 * gda_holder_is_valid_e:
 * @holder: a #GdaHolder object
 * @error: (nullable): a place to store invalid error, or %NULL
 *
 * Get the validity of @holder (that is, of the value held by @holder)
 *
 * Returns: TRUE if @holder's value can safely be used
 *
 * Since: 4.2.10
 */
gboolean
gda_holder_is_valid_e (GdaHolder *holder, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	gda_holder_lock ((GdaLockable*) holder);
	if (priv->full_bind)
		retval = gda_holder_is_valid_e (priv->full_bind, error);
	else {
		if (priv->invalid_forced)
			retval = FALSE;
		else {
			if (priv->default_forced)
				retval = priv->default_value ? TRUE : FALSE;
			else 
				retval = priv->valid;
		}
		if (!retval && priv->invalid_error)
			g_propagate_error (error,  g_error_copy (priv->invalid_error));
	}
	gda_holder_unlock ((GdaLockable*) holder);
	return retval;
}

/**
 * gda_holder_set_value_to_default:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	gda_holder_lock ((GdaLockable*) holder);
	if (priv->default_forced) {
		gda_holder_unlock ((GdaLockable*) holder);
		return TRUE;
	}

	if (!priv->default_value) {
		gda_holder_unlock ((GdaLockable*) holder);
		return FALSE;
	}
	else {
		priv->default_forced = TRUE;
		priv->invalid_forced = FALSE;
		if (priv->invalid_error) {
			g_error_free (priv->invalid_error);
			priv->invalid_error = NULL;
		}

		if (priv->value) {
			if (priv->is_freeable)
				gda_value_free (priv->value);
			priv->value = NULL;
		}
	}

	g_signal_emit (holder, gda_holder_signals[CHANGED], 0);
	g_signal_emit (holder, gda_holder_signals[TO_DEFAULT], 0);

	gda_holder_unlock ((GdaLockable*) holder);
	return TRUE;
}

/**
 * gda_holder_value_is_default:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	return priv->default_forced;
}


/**
 * gda_holder_get_default_value:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	return priv->default_value;
}


/**
 * gda_holder_set_default_value:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	gda_holder_lock ((GdaLockable*) holder);
	if (priv->default_value) {
		if (priv->default_forced) {
			gda_holder_take_value (holder, priv->default_value, NULL);
			priv->default_forced = FALSE;
			priv->default_value = NULL;
		}
		else {
			gda_value_free (priv->default_value);
			priv->default_value = NULL;
		}
	}

	priv->default_forced = FALSE;
	if (value) {
		const GValue *current = gda_holder_get_value (holder);

		/* check if default is equal to current value */
		if (GDA_VALUE_HOLDS_NULL (value) &&
		    (!current || GDA_VALUE_HOLDS_NULL (current))) {
			priv->default_forced = TRUE;
		} else if ((G_VALUE_TYPE (value) == priv->g_type) &&
			 current && !gda_value_compare (value, current)) {
			priv->default_forced = TRUE;
		}

		priv->default_value = gda_value_copy ((GValue *)value);
	}
	
	/* don't emit the "changed" signal */
	gda_holder_unlock ((GdaLockable*) holder);
}

/**
 * gda_holder_set_not_null:
 * @holder: a #GdaHolder object
 * @not_null: TRUE if @holder should not accept %NULL values
 *
 * Sets if the holder can have a NULL value. If @not_null is TRUE, then that won't be allowed
 */
void
gda_holder_set_not_null (GdaHolder *holder, gboolean not_null)
{
	g_return_if_fail (GDA_IS_HOLDER (holder));

	g_object_set (G_OBJECT (holder), "not-null", not_null, NULL);
}

/**
 * gda_holder_get_not_null:
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
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	return priv->not_null;
}

/**
 * gda_holder_set_source_model:
 * @holder: a #GdaHolder object
 * @model: a #GdaDataModel object or %NULL
 * @col: the reference column in @model
 * @error: location to store error, or %NULL
 *
 * Sets an hint that @holder's values should be restricted among the values
 * contained in the @col column of the @model data model. Note that this is just a hint,
 * meaning this policy is not enforced by @holder's implementation.
 *
 * If @model is %NULL, then the effect is to cancel ant previous call to gda_holder_set_source_model()
 * where @model was not %NULL.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_holder_set_source_model (GdaHolder *holder, GdaDataModel *model,
			     gint col, GError **error)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
  if (model != NULL) {
	  g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	  g_return_val_if_fail (col >= 0 && col < gda_data_model_get_n_columns (model), FALSE);
  }

	/* No check is done on the validity of @col or even its existance */
	/* Note: for internal implementation if @col<0, then it's ignored */

	gda_holder_lock ((GdaLockable*) holder);
	if (model && (col >= 0)) {
		GType htype, ctype;
		GdaColumn *gcol;
		htype = gda_holder_get_g_type (holder);
		gcol = gda_data_model_describe_column (model, col);
		if (gcol) {
			ctype = gda_column_get_g_type (gcol);
			if ((htype != GDA_TYPE_NULL) && (ctype != GDA_TYPE_NULL) &&
          (htype != ctype)) {
				g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
					     _("GdaHolder has a gda type (%s) incompatible with "
                                               "source column %d type (%s)"),
                                             gda_g_type_to_string (htype),
                                             col, gda_g_type_to_string (ctype));
				gda_holder_unlock ((GdaLockable*) holder);
				return FALSE;
			}
		}
	}

	if (col >= 0)
		priv->source_col = col;

	if (priv->source_model != model) {
		if (priv->source_model) {
			g_object_unref (priv->source_model);
			priv->source_model = NULL;
		}
		
		priv->source_model = model;
		if (model)
			g_object_ref (model);
		else
			priv->source_col = 0;
	}

#ifdef GDA_DEBUG_signal
        g_print (">> 'SOURCE_CHANGED' from %p\n", holder);
#endif
	g_signal_emit (holder, gda_holder_signals[SOURCE_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'SOURCE_CHANGED' from %p\n", holder);
#endif

	gda_holder_unlock ((GdaLockable*) holder);
	return TRUE;
}


/**
 * gda_holder_get_source_model:
 * @holder: a #GdaHolder
 * @col: a place to store the column in the model sourcing the holder, or %NULL
 *
 * If gda_holder_set_source_model() has been used to provide a hint that @holder's value
 * should be among the values contained in a column of a data model, then this method
 * returns which data model, and if @col is not %NULL, then it is set to the restricting column
 * as well.
 *
 * Otherwise, this method returns %NULL, and if @col is not %NULL, then it is set to 0.
 *
 * Returns: (transfer none): a pointer to a #GdaDataModel, or %NULL
 */
GdaDataModel *
gda_holder_get_source_model (GdaHolder *holder, gint *col)
{
	GdaDataModel *model;
	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	gda_holder_lock ((GdaLockable*) holder);
	if (col)
		*col = priv->source_col;
	model = priv->source_model;
	gda_holder_unlock ((GdaLockable*) holder);
	return model;
}

/*
 * This callback is called when @priv->simple_bind's GType was GDA_TYPE_NULL at the time
 * gda_holder_set_bind() was called, and it makes sure @holder's GType is the same as @priv->simple_bind's
 */
static void
bind_to_notify_cb (GdaHolder *bind_to, G_GNUC_UNUSED GParamSpec *pspec, GdaHolder *holder)
{
	g_return_if_fail (GDA_IS_HOLDER (bind_to));
	gda_holder_lock ((GdaLockable*) holder);
	gda_holder_lock ((GdaLockable*) bind_to);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	GdaHolderPrivate *bpriv = gda_holder_get_instance_private (bind_to);

	g_signal_handler_disconnect (priv->simple_bind,
				     priv->simple_bind_type_changed_id);
	priv->simple_bind_type_changed_id = 0;
	if (priv->g_type == GDA_TYPE_NULL) {
		priv->g_type = bpriv->g_type;
		g_object_notify ((GObject*) holder, "g-type");
	}
	else if (priv->g_type != bpriv->g_type) {
		/* break holder's binding because type differ */
		g_message (_("Cannot bind holders if their type is not the same, breaking existing bind where '%s' was bound to '%s'"),
			   gda_holder_get_id (holder), gda_holder_get_id (bind_to));
		gda_holder_set_bind (holder, NULL, NULL);
	}

	gda_holder_unlock ((GdaLockable*) holder);
	gda_holder_unlock ((GdaLockable*) bind_to);
}

/**
 * gda_holder_set_bind:
 * @holder: a #GdaHolder
 * @bind_to: a #GdaHolder or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Sets @holder to change when @bind_to changes (and does not make @bind_to change when @holder changes).
 * For the operation to succeed, the GType of @holder and @bind_to must be the same, with the exception that
 * any of them can have a %GDA_TYPE_NULL type (in this situation, the GType of the two #GdaHolder objects
 * involved is set to match the other when any of them sets its type to something different than GDA_TYPE_NULL).
 *
 * If @bind_to is %NULL, then @holder will not be bound anymore.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_holder_set_bind (GdaHolder *holder, GdaHolder *bind_to, GError **error)
{
	const GValue *cvalue;
	GValue *value1 = NULL;
	const GValue *value2 = NULL;
	GdaHolderPrivate *priv = NULL;
	GdaHolderPrivate *bpriv = NULL;

	g_return_val_if_fail (GDA_IS_HOLDER (holder), FALSE);
	priv = gda_holder_get_instance_private (holder);
	g_return_val_if_fail (holder != bind_to, FALSE);

	gda_holder_lock ((GdaLockable*) holder);
	if (priv->simple_bind == bind_to) {
		gda_holder_unlock ((GdaLockable*) holder);
		return TRUE;
	}

	/* get a copy of the current values of @holder and @bind_to */
	if (bind_to) {
		g_return_val_if_fail (GDA_IS_HOLDER (bind_to), FALSE);

		bpriv = gda_holder_get_instance_private (bind_to);

		if ((priv->g_type != GDA_TYPE_NULL) &&
		    (bpriv->g_type != GDA_TYPE_NULL) &&
		    (priv->g_type != bpriv->g_type)) {
			g_set_error (error, GDA_HOLDER_ERROR, GDA_HOLDER_VALUE_TYPE_ERROR,
				     "%s", _("Cannot bind holders if their type is not the same"));
			gda_holder_unlock ((GdaLockable*) holder);
			return FALSE;
		}
		value2 = gda_holder_get_value (bind_to);
	}

	cvalue = gda_holder_get_value (holder);
	if (cvalue)
		value1 = gda_value_copy ((GValue*)cvalue);

	/* get rid of the old alias */
	if (priv->simple_bind) {
		g_signal_handlers_disconnect_by_func (priv->simple_bind,
						      G_CALLBACK (bound_holder_changed_cb), holder);

		if (priv->simple_bind_type_changed_id) {
			g_signal_handler_disconnect (priv->simple_bind,
						     priv->simple_bind_type_changed_id);
			priv->simple_bind_type_changed_id = 0;
		}
		g_object_unref (priv->simple_bind);
		priv->simple_bind = NULL;
	}

	/* setting the new alias or reseting the value if there is no new alias */
	gboolean retval;
	if (bind_to) {
		priv->simple_bind = g_object_ref (bind_to);
		g_signal_connect (priv->simple_bind, "changed",
				  G_CALLBACK (bound_holder_changed_cb), holder);

		if (bpriv->g_type == GDA_TYPE_NULL)
			priv->simple_bind_type_changed_id = g_signal_connect (bind_to, "notify::g-type",
										      G_CALLBACK (bind_to_notify_cb),
										      holder);
		else if (priv->g_type == GDA_TYPE_NULL)
			g_object_set ((GObject*) holder, "g-type", bpriv->g_type , NULL);

		/* if bind_to has a different value than holder, then we set holder to the new value */
		if (value1)
			gda_value_free (value1);
		retval = gda_holder_set_value (holder, value2, error);
	}
	else
		retval = gda_holder_take_value (holder, value1, error);

	gda_holder_unlock ((GdaLockable*) holder);
	return retval;
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
	g_return_if_fail (GDA_IS_HOLDER (holder));
	const GValue *cvalue;
	GValue *value1 = NULL, *value2 = NULL;

	g_return_if_fail (GDA_IS_HOLDER (holder));
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	gda_holder_lock ((GdaLockable*) holder);
	if (priv->full_bind == alias_of) {
		gda_holder_unlock ((GdaLockable*) holder);
		return;
	}

	/* get a copy of the current values of @holder and @alias_of */
	if (alias_of) {
		g_return_if_fail (GDA_IS_HOLDER (alias_of));
		GdaHolderPrivate *apriv = gda_holder_get_instance_private (alias_of);
		g_return_if_fail (priv->g_type == apriv->g_type);
		cvalue = gda_holder_get_value (alias_of);
		if (cvalue && !GDA_VALUE_HOLDS_NULL ((GValue*)cvalue))
			value2 = gda_value_copy ((GValue*)cvalue);
	}

	cvalue = gda_holder_get_value (holder);
	if (cvalue && !GDA_VALUE_HOLDS_NULL ((GValue*)cvalue))
		value1 = gda_value_copy ((GValue*)cvalue);
	
	/* get rid of the old alias */
	if (priv->full_bind) {
		g_signal_handlers_disconnect_by_func (priv->full_bind,
						      G_CALLBACK (full_bound_holder_changed_cb), holder);
		g_object_unref (priv->full_bind);
		priv->full_bind = NULL;
	}

	/* setting the new alias or reseting the value if there is no new alias */
	if (alias_of) {
		gboolean equal = FALSE;

		/* get rid of the internal holder's value */
		if (priv->value) {
			if (priv->is_freeable)
				gda_value_free (priv->value);
			priv->value = NULL;
		}

		priv->full_bind = g_object_ref (alias_of);
		g_signal_connect (priv->full_bind, "changed",
				  G_CALLBACK (full_bound_holder_changed_cb), holder);

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
		g_assert (! priv->value);
		if (value1)
			priv->value = value1;
		value1 = NULL;
	}

	if (value1) gda_value_free (value1);
	if (value2) gda_value_free (value2);
	gda_holder_unlock ((GdaLockable*) holder);
}

static void
full_bound_holder_changed_cb (GdaHolder *alias_of, GdaHolder *holder)
{
	gda_holder_lock ((GdaLockable*) holder);
	gda_holder_lock ((GdaLockable*) alias_of);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	g_assert (alias_of == priv->full_bind);
	g_signal_emit (holder, gda_holder_signals [CHANGED], 0);

	gda_holder_unlock ((GdaLockable*) holder);
	gda_holder_unlock ((GdaLockable*) alias_of);
}

static void
bound_holder_changed_cb (GdaHolder *alias_of, GdaHolder *holder)
{
	gda_holder_lock ((GdaLockable*) holder);
	gda_holder_lock ((GdaLockable*) alias_of);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	g_assert (alias_of == priv->simple_bind);
	const GValue *cvalue;
	GError *lerror = NULL;
	cvalue = gda_holder_get_value (alias_of);
	if (! gda_holder_set_value (holder, cvalue, &lerror)) {
		if (lerror && ((lerror->domain != GDA_HOLDER_ERROR) || (lerror->code != GDA_HOLDER_VALUE_NULL_ERROR)))
			g_warning (_("Could not change GdaHolder to match value change in bound GdaHolder: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
	}
	gda_holder_unlock ((GdaLockable*) holder);
	gda_holder_unlock ((GdaLockable*) alias_of);
}

/**
 * gda_holder_get_bind:
 * @holder: a #GdaHolder
 *
 * Get the holder which makes @holder change its value when the holder's value is changed.
 *
 * Returns: (transfer none): the #GdaHolder or %NULL
 */
GdaHolder *
gda_holder_get_bind (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);

	return priv->simple_bind;
}

/**
 * gda_holder_get_alphanum_id:
 * @holder: a #GdaHolder object
 *
 * Get an "encoded" version of @holder's name. The "encoding" consists in replacing non
 * alphanumeric character with the string "__gdaXX" where XX is the hex. representation
 * of the non alphanumeric char.
 *
 * This method is just a wrapper around the gda_text_to_alphanum() function.
 *
 * Returns: (transfer full): a new string
 */
gchar *
gda_holder_get_alphanum_id (GdaHolder *holder)
{
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	return gda_text_to_alphanum (priv->id);
}

static void
gda_holder_lock (GdaLockable *lockable)
{
	GdaHolder *holder = (GdaHolder *) lockable;
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	g_rec_mutex_lock (& (priv->mutex));
}

static gboolean
gda_holder_trylock (GdaLockable *lockable)
{
	GdaHolder *holder = (GdaHolder *) lockable;
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	return g_rec_mutex_trylock (& (priv->mutex));
}

static void
gda_holder_unlock (GdaLockable *lockable)
{
	GdaHolder *holder = (GdaHolder *) lockable;
	GdaHolderPrivate *priv = gda_holder_get_instance_private (holder);
	g_rec_mutex_unlock (& (priv->mutex));
}
