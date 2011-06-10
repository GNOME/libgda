/*
 * Copyright (C) 2000 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
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

#ifndef __GDA_LOG_H__
#define __GDA_LOG_H__

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

/**
 * SECTION:gda-log
 * @short_description: Log functions
 * @title: Logging
 * @stability: Stable
 * @see_also:
 *
 * Logging functions.
 */

/*
 * For application generating logs
 */
void     gda_log_enable (void);
void     gda_log_disable (void);
gboolean gda_log_is_enabled (void);

void     gda_log_message (const gchar * format, ...);
void     gda_log_error (const gchar * format, ...);

G_END_DECLS

#endif
