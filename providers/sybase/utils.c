/* GDA sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *      Holger Thon <holger.thon@gnome-db.org>
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


#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <libgda/gda-log.h>

#include "gda-sybase.h"

void sybase_debug_msg(gchar *fmt, ...)
{
	va_list args;
	const size_t buf_size = 4096;
	char buf[buf_size + 1];
	
	va_start(args, fmt);
	vsnprintf(buf, buf_size, fmt, args);
	va_end(args);
	
	gda_log_message("Sybase: ", buf);
}
