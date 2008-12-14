/* GDA Common Library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_log_h__)
#  define __gda_log_h__

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

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
