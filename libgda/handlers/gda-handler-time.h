/*
 * Copyright (C) 2006 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDA_HANDLER_TIME__
#define __GDA_HANDLER_TIME__

#include <glib-object.h>
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
	GObject              object;
	GdaHandlerTimePriv  *priv;
};

/* struct for the object's class */
struct _GdaHandlerTimeClass
{
	GObjectClass         parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

/**
 * SECTION:gda-handler-time
 * @short_description: Default handler for time values
 * @title: GdaHanderTime
 * @stability: Stable
 * @see_also: #GdaDataHandler interface
 *
 * You should normally not need to use this API, refer to the #GdaDataHandler
 * interface documentation for more information.
 */

GType           gda_handler_time_get_type      (void) G_GNUC_CONST;
GdaDataHandler *gda_handler_time_new           (void);
GdaDataHandler *gda_handler_time_new_no_locale (void);
void            gda_handler_time_set_sql_spec  (GdaHandlerTime *dh, GDateDMY first, GDateDMY sec,
						GDateDMY third, gchar separator, gboolean twodigits_years);
void            gda_handler_time_set_str_spec  (GdaHandlerTime *dh, GDateDMY first, GDateDMY sec,
						GDateDMY third, gchar separator, gboolean twodigits_years);
gchar          *gda_handler_time_get_no_locale_str_from_value (GdaHandlerTime *dh, const GValue *value);

gchar          *gda_handler_time_get_format    (GdaHandlerTime *dh, GType type);
G_END_DECLS

#endif
