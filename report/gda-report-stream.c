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

#include "config.h"
#include <gda-common.h>
#include <gda-report-stream.h>

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
  
/*
  gda_reportstream_signals[REPORTSTREAM_ERROR] = gtk_signal_new("error",
							    GTK_RUN_FIRST,
							    object_class->type,
							    GTK_SIGNAL_OFFSET(Gda_ReportStreamClass, error),
							    gtk_marshal_NONE__POINTER,
							    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  gda_reportstream_signals[CONNECTION_WARNING] = gtk_signal_new("warning",
							    GTK_RUN_LAST,
							    object_class->type,
							    GTK_SIGNAL_OFFSET(Gda_ReportStreamClass, warning),
							    gtk_marshal_NONE__POINTER,
							    GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
  gtk_object_class_add_signals(object_class, gda_reportstream_signals, LAST_SIGNAL);
*/
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

