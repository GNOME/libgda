/*
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_HANDLER_TEXT__
#define __GDA_HANDLER_TEXT__

#include <glib-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

/**
 * SECTION:gda-handler-text
 * @short_description: Default handler for large string values
 * @title: GdaHanderText
 * @stability: Stable
 * @see_also: #GdaDataHandler interface
 *
 * You should normally not need to use this API, refer to the #GdaDataHandler
 * interface documentation for more information.
 */


#define GDA_TYPE_HANDLER_TEXT (gda_handler_text_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaHandlerText, gda_handler_text, GDA, HANDLER_TEXT, GObject)

struct _GdaHandlerTextClass
{
  GObjectClass parent_class;
};

GdaDataHandler   *gda_handler_text_new                  (void);
GdaDataHandler   *gda_handler_text_new_with_connection  (GdaConnection *cnc);

G_END_DECLS

#endif
