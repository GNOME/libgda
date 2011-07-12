/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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

#include "dummy-object.h"
#include <glib-object.h>

/* 
 * Main static functions 
 */
static void dummy_object_class_init (DummyObjectClass * class);
static void dummy_object_init (DummyObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	SIG0,
        SIG1,
        SIG2,
        LAST_SIGNAL
};

static gint dummy_object_signals[LAST_SIGNAL] = { 0, 0, 0 };

GType
dummy_object_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (DummyObjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) dummy_object_class_init,
			NULL,
			NULL,
			sizeof (DummyObject),
			0,
			(GInstanceInitFunc) dummy_object_init,
			NULL
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "DummyObject", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/* VOID:STRING,INT (gda-marshal.list:39) */
#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
static void
local_marshal_VOID__STRING_INT (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_INT) (gpointer     data1,
                                                 gpointer     arg_1,
                                                 gint         arg_2,
                                                 gpointer     data2);
  register GMarshalFunc_VOID__STRING_INT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING_INT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_string (param_values + 1),
            g_marshal_value_peek_int (param_values + 2),
            data2);
}

static void
dummy_object_class_init (DummyObjectClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	dummy_object_signals[SIG0] =
                g_signal_new ("sig0",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DummyObjectClass, sig0),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	dummy_object_signals[SIG1] =
                g_signal_new ("sig1",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DummyObjectClass, sig1),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
	dummy_object_signals[SIG1] =
                g_signal_new ("sig2",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DummyObjectClass, sig1),
                              NULL, NULL,
                              local_marshal_VOID__STRING_INT, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);

        class->sig1 = NULL;
        class->sig0 = NULL;
        class->sig2 = NULL;
}

static void
dummy_object_init (DummyObject *object)
{
}

/**
 * dummy_object_new
 *
 * Creates a new object of type @type
 *
 * Returns: a new #DummyObject object
 */
DummyObject *
dummy_object_new (void)
{
        return (DummyObject *) g_object_new (DUMMY_TYPE_OBJECT, NULL);
}
