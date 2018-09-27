/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include "dummy-object.h"
#include <glib-object.h>

/*
 * Main static functions
 */
typedef struct {
  gint dummy;
} DummyObjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (DummyObject, dummy_object, G_TYPE_OBJECT)
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	SIG0,
        SIG1,
        SIG2,
	SIG3,
        LAST_SIGNAL
};

static gint dummy_object_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };


/* VOID:INT,STRING (gda-marshal.list:39) */
#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
static void
local_marshal_VOID__INT_STRING (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__INT_STRING) (gpointer     data1,
                                                 gint         arg_1,
                                                 gpointer     arg_2,
                                                 gpointer     data2);
  register GMarshalFunc_VOID__INT_STRING callback;
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
  callback = (GMarshalFunc_VOID__INT_STRING) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_int (param_values + 1),
            g_marshal_value_peek_string (param_values + 2),
            data2);
}

/* STRING:STRING,INT (gda-marshal.list:43) */
void
_gda_marshal_STRING__STRING_INT (GClosure     *closure,
                                 GValue       *return_value G_GNUC_UNUSED,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint G_GNUC_UNUSED,
                                 gpointer      marshal_data)
{
  typedef gchar* (*GMarshalFunc_STRING__STRING_INT) (gpointer     data1,
                                                     gpointer     arg_1,
                                                     gint         arg_2,
                                                     gpointer     data2);
  register GMarshalFunc_STRING__STRING_INT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gchar* v_return;

  g_return_if_fail (return_value != NULL);
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
  callback = (GMarshalFunc_STRING__STRING_INT) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_marshal_value_peek_string (param_values + 1),
                       g_marshal_value_peek_int (param_values + 2),
                       data2);

  g_value_take_string (return_value, v_return);
}

static gboolean
sig3_accumulator (G_GNUC_UNUSED GSignalInvocationHint *ihint,
		  GValue *return_accu,
		  const GValue *handler_return,
		  G_GNUC_UNUSED gpointer data)
{
        const gchar *rstr, *str;

        rstr = g_value_get_string (handler_return);
	str = g_value_get_string (return_accu);
	if (str) {
		if (rstr) {
			gchar *tmp;
			tmp = g_strdup_printf ("%s[]%s", rstr, str);
			g_value_take_string (return_accu, tmp);
		}
	}
	else
		g_value_set_string (return_accu, rstr);

        return TRUE; /* stop signal if error has been set */
}

gchar *
m_sig3 (DummyObject *object, gchar *str, gint i)
{
	return NULL;
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
	dummy_object_signals[SIG2] =
                g_signal_new ("sig2",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DummyObjectClass, sig2),
                              NULL, NULL,
                              local_marshal_VOID__INT_STRING, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	dummy_object_signals[SIG3] =
                g_signal_new ("sig3",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DummyObjectClass, sig3),
                              sig3_accumulator, NULL,
                              _gda_marshal_STRING__STRING_INT, G_TYPE_STRING, 2, G_TYPE_STRING, G_TYPE_INT);

        class->sig1 = NULL;
        class->sig0 = NULL;
        class->sig2 = NULL;
	class->sig3 = m_sig3;
}

static void
dummy_object_init (DummyObject *object)
{
  DummyObjectPrivate *priv = dummy_object_get_instance_private (object);
  priv->dummy = 0;
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
