/* GNOME DB Common Library
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_log_h__)
#  define __gda_log_h__

#include <glib.h>
#include <gnome.h>

BEGIN_GNOME_DECLS

void         gda_log_enable     (void);
void         gda_log_disable    (void);
gboolean     gda_log_is_enabled (void);

void         gda_log_message    (const gchar *format, ...);
void         gda_log_error      (const gchar *format, ...);

END_GNOME_DECLS

#endif
