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

#ifndef __gda_reportstream_h__
#define __gda_reportstream_h__ 1

/* Data Structures and Prototypes for the LibGDA Report Client
 * Library
 */
typedef struct _Gda_ReportStream       Gda_ReportStream;
typedef struct _Gda_ReportStreamClass  Gda_ReportStreamClass;


#define GDA_TYPE_REPORTSTREAM          (gda_reportstream_get_type())
#define GDA_REPORTSTREAM(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORTSTREAM, Gda_ReportStream)
#define GDA_REPORTSTREAM_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORTSTREAM, GdaReportStreamClass)
#define IS_GDA_REPORTSTREAM(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORTSTREAM)
#define IS_GDA_REPORTSTREAM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORTSTREAM))

struct _Gda_ReportStream
{
  GtkObject     object;
  CORBA_Object  reportstream;
  CORBA_ORB     orb;
/*  GList*        commands;
  GList*        recordsets;
  gchar*        provider;
  gchar*        default_db;
  gchar*        database;
  gchar*        user;
  gchar*        passwd;
  GList*        errors_head;
  gint          is_open;*/
};

struct _Gda_ReportStreamClass
{
  GtkObjectClass parent_class;
};

  
guint              gda_reportstream_get_type            (void);
Gda_ReportStream*  gda_reportstream_new                 (CORBA_ORB orb);
void               gda_reportstream_free                (Gda_ReportStream* cnc);

#endif