/* libgda library
 *
 * Copyright (C) 2000 Carlos Perelló Marín <carlos@hispalinux.es>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <gnome.h>
#include <gda.h>

enum
{
  LAST_SIGNAL
};

static gint
gda_reportstream_signals[LAST_SIGNAL] = {0, };

static void    gda_reportstream_class_init    (Gda_ReportStreamClass* klass);
static void    gda_reportstream_init          (Gda_ReportStream* rs);

guint
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


static void
gda_reportstream_class_init (Gda_ReportStreamClass* klass)
{
  GtkObjectClass*   object_class;

  object_class = (GtkObjectClass*) klass;
  
  gtk_object_class_add_signals(object_class, gda_reportstream_signals, LAST_SIGNAL);
}

static void
gda_reportstream_init (Gda_ReportStream* rs)
{
	g_return_if_fail(GDA_REPORTSTREAM_IS_OBJECT(rs));
}