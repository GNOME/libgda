/* gda-handler-time.h
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

#ifndef __GDA_HANDLER_TIME__
#define __GDA_HANDLER_TIME__

#include <libgda/gda-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_HANDLER_TIME          (gda_handler_time_get_type())
#define GDA_HANDLER_TIME(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_handler_time_get_type(), GdaHandlerTime)
#define GDA_HANDLER_TIME_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_handler_time_get_type (), GdaHandlerTimeClass)
#define GDA_IS_HANDLER_TIME(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_handler_time_get_type ())


typedef struct _GdaHandlerTime      GdaHandlerTime;
typedef struct _GdaHandlerTimeClass GdaHandlerTimeClass;
typedef struct _GdaHandlerTimePriv  GdaHandlerTimePriv;


/* struct for the object's data */
struct _GdaHandlerTime
{
	GdaObject            object;

	GdaHandlerTimePriv  *priv;
};

/* struct for the object's class */
struct _GdaHandlerTimeClass
{
	GdaObjectClass         parent_class;
};


GType           gda_handler_time_get_type      (void);
GdaDataHandler *gda_handler_time_new           (void);
GdaDataHandler *gda_handler_time_new_no_locale (void);
void            gda_handler_time_set_sql_spec  (GdaHandlerTime *dh, GDateDMY first, GDateDMY sec,
						GDateDMY third, gchar separator, gboolean twodigits_years);

G_END_DECLS

#endif
