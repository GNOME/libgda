/* libgda library
 *
 * Copyright (C) 2000 Carlos Perelló Marín <carlos@hispalinux.es>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <gnome.h>
#include <gda.h>

enum
{
  REPORTSTREAM_ERROR,
  REPORTSTREAM_WARNING,
  LAST_SIGNAL
};

static gint
gda_reportstream_signals[LAST_SIGNAL] = {0, };


#ifdef HAVE_GOBJECT
static void gda_reportstream_finalize (GObject *object);
#endif

static void    gda_reportstream_class_init    (Gda_ReportStreamClass* klass);
static void    gda_reportstream_init          (Gda_ReportStream* object);
static void    gda_reportstream_real_error    (Gda_ReportStream* object, GList*);
static void    gda_reportstream_real_warning  (Gda_ReportStream* object, GList*);

static void
gda_reportstream_real_error (Gda_ReportStream* object, GList* errors)
{
  g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
  object->errors_head = g_list_concat(object->errors_head, errors);
}

static void
gda_reportstream_real_warning (Gda_ReportStream* object, GList* warnings)
{
  g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

#ifdef HAVE_GOBJECT
GType
gda_reportstream_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo info =
      {
       sizeof (Gda_ReportStreamClass),               /* class_size */
        NULL,                                        /* base_init */
        NULL,                                        /* base_finalize */
       (GClassInitFunc) gda_reportstream_class_init, /* class_init */
        NULL,                                        /* class_finalize */
        NULL,                                        /* class_data */
       sizeof (Gda_ReportStream),                    /* instance_size */
        0,                                           /* n_preallocs */
       (GInstanceInitFunc) gda_reportstream_init,    /* instance_init */
        NULL,                                        /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "Gda_ReportStream", &info);
    }
  return type;
}
#else
GtkType
gda_reportstream_get_type (void)
{
  static guint gda_reportstream_type = 0;

  if (!gda_reportstream_type)
    {
      GtkTypeInfo gda_reportstream_info =
      {
        "Gda_ReportStream",
        sizeof (Gda_ReportStream),
        sizeof (Gda_ReportStreamClass),
        (GtkClassInitFunc) gda_reportstream_class_init,
        (GtkObjectInitFunc) gda_reportstream_init,
        (GtkArgSetFunc)NULL,
        (GtkArgSetFunc)NULL,
      };
      gda_reportstream_type = gtk_type_unique(gtk_object_get_type(),
                                            &gda_reportstream_info);
    }
  return gda_reportstream_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_reportstream_class_init (Gda_ReportStreamClass *klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = &gda_reportstream_finalize;
  klass->parent = g_type_class_peek_parent (klass);
}
#else
static void
gda_reportstream_class_init (Gda_ReportStreamClass* klass)
{
  GtkObjectClass*   object_class = GTK_OBJECT_CLASS(klass);
  
  gda_reportstream_signals[REPORTSTREAM_ERROR] = gtk_signal_new("error",
							    GTK_RUN_FIRST,
							    object_class->type,
							    GTK_SIGNAL_OFFSET(Gda_ReportStreamClass, error),
							    gtk_marshal_NONE__POINTER,
							    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  gda_connection_signals[CONNECTION_WARNING] = gtk_signal_new("warning",
							    GTK_RUN_LAST,
							    object_class->type,
							    GTK_SIGNAL_OFFSET(Gda_ConnectionClass, warning),
							    gtk_marshal_NONE__POINTER,
							    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  gtk_object_class_add_signals(object_class, gda_reportstream_signals, LAST_SIGNAL);
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_reportstream_init (Gda_ReportStream* object, Gda_ReportStreamClass *klass)
#else
gda_reportstream_init (Gda_ReportStream *object)
#endif
{
	g_return_if_fail(GDA_REPORTSTREAM_IS_OBJECT(object));
}

gint
gda_reportstream_corba_exception (Gda_ReportStream* object, CORBA_Environment* ev)
{
  Gda_Error* error;
  
  g_return_val_if_fail(ev != 0, -1);
  
  switch (ev->_major)
    {
    case CORBA_NO_EXCEPTION:
      return 0;
    case CORBA_SYSTEM_EXCEPTION:
      {
        CORBA_SystemException* sysexc;

        sysexc = CORBA_exception_value(ev);
        error = gda_error_new();
        error->source = g_strdup("[CORBA System Exception]");
        switch(sysexc->minor)
          {
          case ex_CORBA_COMM_FAILURE:
            error->description = g_strdup_printf(_("%s: The server didn't respond."),
                                                 CORBA_exception_id(ev));
            break;
          default:
            error->description = g_strdup_printf(_("%s: An Error occured in the CORBA system."),
						 CORBA_exception_id(ev));
            break;
          }
        gda_reportstream_add_single_error(object, error);
        return -1;
      }
    case CORBA_USER_EXCEPTION:
      error = gda_error_new();
      error->source = g_strdup("[CORBA User Exception]");
	  gda_reportstream_add_single_error(object, error);
      return -1;
    default:
      g_error("Unknown CORBA exception for connection");
    }
  return 0;
}